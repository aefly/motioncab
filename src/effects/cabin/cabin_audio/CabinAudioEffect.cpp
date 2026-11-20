#include "CabinAudioEffect.hpp"

#include "core/PluginContext.hpp"
#include "effects/Telemetry.hpp"
#include "effects/manual/cabin_walk/CabinLayouts.hpp"
#include "math/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>

namespace motioncab {

namespace {

// By name: FMOD's DSP type enum isn't stable across versions.
constexpr const char *kDspNames[] = {"FMOD Reverb", "FMOD 3-EQ",
                                     "FMOD Multiband Equalizer"};
// FMOD_CHANNELCONTROL_DSP_TAIL: the input end of the group's chain, before
// the game's fader.
constexpr int kDspTail = -3;

// The door speakers, from the player's own head position: low in the doors,
// a little ahead. The driver's door is at -x on either drive (the camera's
// frame), the passenger's mirrored across the centerline.
constexpr float kDoorOutX = 0.5f, kDoorY = -0.55f, kDoorZ = -0.3f;
// Straight ahead of the wheel: at the screen, toward the centerline, it
// sounded off to one side.
constexpr float kGpsX = 0.0f, kGpsY = -0.25f, kGpsZ = -0.7f;
// FMOD_DSP_MULTIBAND_EQ_FILTER_TYPE values.
constexpr int kFilterLowpass24 = 2, kFilterHighpass24 = 5, kFilterPeaking = 9;
constexpr float kNoLowCutHz = 20.0f;
// A little resonance at each end, as a small cone and its box have.
constexpr float kSmallSpeakerEdgeQ = 1.2f;
constexpr float kSmallSpeakerBumpQ = 1.0f;

constexpr float kOpenHz = SpatialMix::kOpenHz;
constexpr float kMinCutoffHz = 300.0f;
// A speaker straight to one side, at Width and Head shadow 100%: how much
// the far ear loses, and its cutoff.
constexpr float kMaxEarDrop = 0.65f; // about -9 dB
constexpr float kShadowCutoffHz = 1800.0f;
constexpr float kBackCutoffHz = 2500.0f; // behind, muffle at 100%
constexpr float kBackAttenuation = 0.5f; // behind, muffle at 100%
// Distance never makes it louder than at the wheel, nor fully silent.
constexpr float kMinDistanceGain = 0.2f;

// Outside the cabin, at 100%: through its walls, quieter, dull, and the two
// sides run together.
constexpr float kOutsideGain = 0.35f;
constexpr float kOutsideCutoffHz = 500.0f;
constexpr float kOutsideFarEar = 0.7f;
constexpr float kOutsideBlend = 0.3f;

// The radio under the GPS voice, as a navigation app does it: down quickly
// as the voice starts, back up slowly once it has been quiet a moment (a
// pause between two words doesn't bring the music back).
constexpr float kDuckedGain = 0.15f;      // at Ducking 100%, about -16 dB
constexpr float kVoiceThreshold = 0.003f; // about -50 dB
constexpr float kVoiceHoldSeconds = 0.6f;
constexpr float kDuckAttackSeconds = 0.08f;
constexpr float kDuckReleaseSeconds = 0.5f;

constexpr float kSpeakerLowCrossoverHz = 200.0f;
constexpr float kSpeakerHighCrossoverHz = 4000.0f;
// The reverb's wet level at 50%: 6 dB more or less per doubling.
constexpr float kReverbHalfWetDb = -12.0f;
constexpr float kMinDb = -80.0f; // FMOD's floor for its gains

struct ReverbParameter {
  const char *name;
  float value;
};
// A small, damped room: short and dense, its highs dying first.
constexpr ReverbParameter kCabinReverb[] = {
    {"Early Delay", 4.0f},     {"Late Delay", 8.0f},
    {"HF Reference", 4000.0f}, {"HF Decay Ratio", 30.0f},
    {"Diffusion", 80.0f},      {"Density", 100.0f},
    {"High Cut", 5000.0f},     {"Early/Late Mix", 75.0f},
    {"Dry Level", 0.0f},
};

constexpr float kSmoothingSeconds = 0.1f;
// Switching views is instant; a hard cut in the sound would click.
constexpr float kOutsideSeconds = 0.15f;
constexpr float kToggleFadeSeconds = 0.5f;
// Into and out of a menu: quicker, the menu is already there.
constexpr float kPauseFadeSeconds = 0.3f;
constexpr float kMaxTickSeconds = 0.1f;
constexpr float kBusRetrySeconds = 1.0f;
// Long enough for the game to load its banks.
constexpr float kLogBusesAfterSeconds = 5.0f;
constexpr float kLogNoStudioAfterSeconds = 5.0f;

float LogLerp(float from, float to, float t) {
  return std::exp(math::Lerp(std::log(from), std::log(to), t));
}

float Percent(float value) { return std::clamp(value / 100.0f, 0.0f, 1.0f); }

float Ease(float dt, float seconds) { return 1.0f - std::exp(-dt / seconds); }

} // namespace

CabinAudioEffect::CabinAudioEffect(SPF_Config_API *config_api,
                                   SPF_Config_Handle *config_handle)
    : config_(config_api, config_handle, "cabin", "cabin_audio"),
      centerline_x_(kTypicalCenterlineX) {}

void CabinAudioEffect::LoadConfig() {
  if (!config_.IsAvailable())
    return;
  enabled_ = config_.Bool("enabled", enabled_);
  width_ = config_.Float("width", width_);
  head_shadow_ = config_.Float("head_shadow", head_shadow_);
  muffle_ = config_.Float("muffle", muffle_);
  // Up to 0: a value saved when it could boost the bass no longer does.
  bass_db_ = std::min(0.0f, config_.Float("bass", bass_db_));
  mid_db_ = config_.Float("mid", mid_db_);
  treble_db_ = config_.Float("treble", treble_db_);
  blend_ = config_.Float("blend", blend_);
  reverb_ = config_.Float("reverb", reverb_);
  room_ms_ = config_.Float("room_size", room_ms_);
  radio_volume_db_ = config_.Float("radio_volume", radio_volume_db_);
  gps_ = config_.Bool("gps", gps_);
  gps_low_cut_hz_ = config_.Float("gps_low_cut", gps_low_cut_hz_);
  gps_high_cut_hz_ = config_.Float("gps_high_cut", gps_high_cut_hz_);
  gps_tone_hz_ = config_.Float("gps_tone", gps_tone_hz_);
  gps_tone_db_ = config_.Float("gps_tone_boost", gps_tone_db_);
  gps_volume_db_ = config_.Float("gps_volume", gps_volume_db_);
  duck_ = config_.Float("duck", duck_);
  outside_ = config_.Float("outside", outside_);
}

void CabinAudioEffect::OnTruckConstantsChanged(
    const SPF_TruckConstants &constants) {
  right_hand_drive_ = telemetry::IsRightHandDrive(constants);
  centerline_x_ = CabinCenterlineX(constants);
}

float CabinAudioEffect::Tick() {
  const auto now = std::chrono::steady_clock::now();
  const float dt =
      ticked_ ? std::chrono::duration<float>(now - last_tick_).count() : 0.0f;
  last_tick_ = now;
  ticked_ = true;
  return std::clamp(dt, 0.0f, kMaxTickSeconds);
}

bool CabinAudioEffect::Prepare(float dt) {
  fade_ = enabled_ ? std::min(1.0f, fade_ + dt / kToggleFadeSeconds)
                   : std::max(0.0f, fade_ - dt / kToggleFadeSeconds);
  if ((!enabled_ && fade_ <= 0.0f) || presence_ <= 0.0f) {
    Release();
    return false;
  }
  weight_ = math::SmoothStep(fade_) * math::SmoothStep(presence_);
  if (!fmod_.Load())
    return false;
  void *studio = fmod_.StudioSystem();
  if (studio != studio_) {
    // A new sound system: nothing of the old one is ours any more.
    Forget();
    studio_ = studio;
    core_ = fmod_.CoreSystem(studio);
  }
  if (!studio_ || !core_) {
    no_studio_seconds_ += dt;
    if (!studio_logged_ && no_studio_seconds_ > kLogNoStudioAfterSeconds) {
      studio_logged_ = true;
      Context().LogFmt(SPF_LOG_WARN,
                       "(CabinAudio) No Studio system yet (%u updates hooked, "
                       "core %s)",
                       fmod_.UpdateCalls(), core_ ? "found" : "missing");
    }
    return false;
  }
  return true;
}

void *CabinAudioEffect::CurrentGroup(Link &link, float dt) {
  if (!link.bus || !fmod_.Studio_Bus_IsValid(link.bus)) {
    link.bus = nullptr;
    link.group = nullptr;
    link.retry_seconds -= dt;
    if (link.retry_seconds > 0.0f)
      return nullptr;
    link.retry_seconds = kBusRetrySeconds;
    link.bus = fmod_.FindBus(studio_, link.name);
    if (!link.bus) {
      link.missing_seconds += kBusRetrySeconds;
      if (!link.bus_logged && link.missing_seconds > kLogBusesAfterSeconds) {
        link.bus_logged = true;
        Context().LogFmt(SPF_LOG_WARN, "(CabinAudio) No \"%s\" bus", link.name);
        fmod_.LogBuses(studio_);
      }
      return nullptr;
    }
  }
  // Asked again every frame: the game can let the group go once nothing
  // plays, and a freed one must never be touched.
  void *group = nullptr;
  const int result = fmod_.Studio_Bus_GetChannelGroup(link.bus, &group);
  if (result != FmodLibrary::kOk || !group) {
    if (link.warn && !link.group_logged) {
      link.group_logged = true;
      Context().LogFmt(SPF_LOG_WARN,
                       "(CabinAudio) The \"%s\" bus has no channel group (%d)",
                       link.name, result);
    }
    link.group = nullptr;
    return nullptr;
  }
  return group;
}

bool CabinAudioEffect::Attach(Link &link, void *group) {
  link.group = nullptr;
  CreateDsps(link);
  if (!link.spatial.Create(fmod_, core_))
    return false;
  // Each one at the tail, the input end: the last added runs first, so the
  // sound goes through the speakers' placing, the EQ, then the reverb (heard
  // all around).
  for (Dsp &dsp : link.dsps) {
    if (!dsp.handle)
      continue;
    fmod_.DSP_SetBypass(dsp.handle, 1);
    if (fmod_.ChannelGroup_AddDSP(group, kDspTail, dsp.handle) !=
        FmodLibrary::kOk) {
      Context().Log(SPF_LOG_WARN, "(CabinAudio) Couldn't add a DSP");
      fmod_.DSP_Release(dsp.handle);
      dsp = {};
    }
  }
  if (fmod_.ChannelGroup_AddDSP(group, kDspTail, link.spatial.handle()) !=
      FmodLibrary::kOk) {
    Context().LogFmt(SPF_LOG_WARN,
                     "(CabinAudio) Couldn't add the spatial DSP to \"%s\"",
                     link.name);
    for (const Dsp &dsp : link.dsps)
      if (dsp.handle)
        fmod_.ChannelGroup_RemoveDSP(group, dsp.handle);
    return false;
  }
  // The game lets a group go when nothing plays (the GPS between two
  // messages): the mix carries on from where it was.
  link.group = group;
  link.speakers_applied = false;
  if (&link == &gps_link_)
    small_speaker_applied_ = false;
  return true;
}

void CabinAudioEffect::Detach(Link &link) {
  if (!link.group)
    return;
  // Only if it's still the bus's group: a freed one has nothing to take out.
  void *current = nullptr;
  if (fmod_.StudioSystem() == studio_ && link.bus &&
      fmod_.Studio_Bus_IsValid(link.bus) &&
      fmod_.Studio_Bus_GetChannelGroup(link.bus, &current) ==
          FmodLibrary::kOk &&
      current == link.group) {
    for (const Dsp &dsp : link.dsps)
      if (dsp.handle)
        fmod_.ChannelGroup_RemoveDSP(link.group, dsp.handle);
    fmod_.ChannelGroup_RemoveDSP(link.group, link.spatial.handle());
  }
  link.group = nullptr;
}

void CabinAudioEffect::CreateDsps(Link &link) {
  if (link.dsps_tried)
    return;
  link.dsps_tried = true;
  Dsp *dsps = link.dsps;
  for (int slot = 0; slot < kDspCount; ++slot)
    if (slot != kSmallSpeaker || &link == &gps_link_)
      dsps[slot].handle = fmod_.CreateDspByName(core_, kDspNames[slot]);
  auto find = [&](DspSlot slot, int index, const char *name) {
    dsps[slot].params[index] = fmod_.FindParameter(dsps[slot].handle, name);
  };
  find(kReverb, 0, "Wet Level");
  find(kReverb, 1, "Decay Time");
  find(kEq, 0, "Low Gain");
  find(kEq, 1, "Mid Gain");
  find(kEq, 2, "High Gain");
  // A DSP missing a parameter it needs is left out rather than half driven.
  for (int slot = 0; slot < kDspCount; ++slot) {
    const int needed = slot == kEq ? 3 : slot == kReverb ? 2 : 0;
    for (int i = 0; i < needed; ++i) {
      if (dsps[slot].handle && dsps[slot].params[i] < 0) {
        fmod_.DSP_Release(dsps[slot].handle);
        dsps[slot] = {};
      }
    }
  }
  if (void *reverb = dsps[kReverb].handle)
    for (const ReverbParameter &parameter : kCabinReverb)
      if (const int index = fmod_.FindParameter(reverb, parameter.name);
          index >= 0)
        fmod_.DSP_SetParameterFloat(reverb, index, parameter.value);
  if (void *speaker = dsps[kSmallSpeaker].handle) {
    // Their shapes never change, only the frequencies and the bump's gain.
    static constexpr int kFilters[3] = {kFilterHighpass24, kFilterPeaking,
                                        kFilterLowpass24};
    static constexpr const char *kFields[4] = {"Filter", "Frequency", "Q",
                                               "Gain"};
    bool complete = true;
    char name[16];
    for (int band = 0; band < 3; ++band) {
      for (int field = 0; field < 4; ++field) {
        std::snprintf(name, sizeof(name), "%c) %s", 'A' + band, kFields[field]);
        small_speaker_params_[band][field] = fmod_.FindParameter(speaker, name);
        complete &= small_speaker_params_[band][field] >= 0;
      }
    }
    if (!complete) {
      fmod_.DSP_Release(speaker);
      dsps[kSmallSpeaker] = {};
    } else {
      for (int band = 0; band < 3; ++band) {
        const int *params = small_speaker_params_[band];
        fmod_.DSP_SetParameterInt(speaker, params[0], kFilters[band]);
        fmod_.DSP_SetParameterFloat(speaker, params[2],
                                    band == 1 ? kSmallSpeakerBumpQ
                                              : kSmallSpeakerEdgeQ);
      }
    }
  }
  if (void *eq = dsps[kEq].handle) {
    if (const int index = fmod_.FindParameter(eq, "Low Crossover"); index >= 0)
      fmod_.DSP_SetParameterFloat(eq, index, kSpeakerLowCrossoverHz);
    if (const int index = fmod_.FindParameter(eq, "High Crossover"); index >= 0)
      fmod_.DSP_SetParameterFloat(eq, index, kSpeakerHighCrossoverHz);
  }
}

void CabinAudioEffect::ApplySpeakers(Link &link, float amount) {
  const float values[] = {bass_db_, mid_db_,  treble_db_,
                          reverb_,  room_ms_, amount};
  if (link.speakers_applied && std::equal(std::begin(values), std::end(values),
                                          std::begin(link.applied)))
    return;
  link.speakers_applied = true;
  std::copy(std::begin(values), std::end(values), std::begin(link.applied));
  if (const Dsp &eq = link.dsps[kEq]; eq.handle) {
    fmod_.DSP_SetParameterFloat(eq.handle, eq.params[0], bass_db_ * amount);
    fmod_.DSP_SetParameterFloat(eq.handle, eq.params[1], mid_db_ * amount);
    fmod_.DSP_SetParameterFloat(eq.handle, eq.params[2], treble_db_ * amount);
    fmod_.DSP_SetBypass(eq.handle, amount == 0.0f ||
                                       (bass_db_ == 0.0f && mid_db_ == 0.0f &&
                                        treble_db_ == 0.0f));
  }
  if (const Dsp &reverb = link.dsps[kReverb]; reverb.handle) {
    const float level = Percent(reverb_) * amount;
    const bool off = level < 0.01f;
    // In decibels from the floor, so low amounts stay faint.
    const float wet =
        off ? kMinDb
            : std::max(kMinDb,
                       kReverbHalfWetDb + 20.0f * std::log10(2.0f * level));
    fmod_.DSP_SetParameterFloat(reverb.handle, reverb.params[0], wet);
    fmod_.DSP_SetParameterFloat(reverb.handle, reverb.params[1], room_ms_);
    fmod_.DSP_SetBypass(reverb.handle, off);
  }
}

void CabinAudioEffect::ApplySmallSpeaker(float amount) {
  const Dsp &eq = gps_link_.dsps[kSmallSpeaker];
  const float values[] = {gps_low_cut_hz_, gps_high_cut_hz_, gps_tone_hz_,
                          gps_tone_db_, amount};
  if (!eq.handle || (small_speaker_applied_ &&
                     std::equal(std::begin(values), std::end(values),
                                std::begin(small_speaker_values_))))
    return;
  small_speaker_applied_ = true;
  std::copy(std::begin(values), std::end(values),
            std::begin(small_speaker_values_));
  if (amount < 0.01f) {
    fmod_.DSP_SetBypass(eq.handle, 1);
    return;
  }
  const float frequencies[3] = {
      LogLerp(kNoLowCutHz, std::max(kNoLowCutHz, gps_low_cut_hz_), amount),
      gps_tone_hz_,
      LogLerp(kOpenHz, std::min(kOpenHz, gps_high_cut_hz_), amount),
  };
  for (int band = 0; band < 3; ++band)
    fmod_.DSP_SetParameterFloat(eq.handle, small_speaker_params_[band][1],
                                frequencies[band]);
  fmod_.DSP_SetParameterFloat(eq.handle, small_speaker_params_[1][3],
                              gps_tone_db_ * amount);
  fmod_.DSP_SetBypass(eq.handle, 0);
}

void CabinAudioEffect::PlayTestVoice(SPF_Sound_API *sound) {
  if (!test_voice_requested_)
    return;
  test_voice_requested_ = false;
  if (!sound || !sound->SND_IsReady())
    return;
  // The voice's lines, the same in every voice's bank
  // (sound/navigation/<voice>.bank, the one picked in the options loaded).
  static constexpr const char *kLines[] = {
      "event:/prepare_turn_left",   "event:/roundabout_2",
      "event:/and_then_keep_right", "event:/prepare_exit_right",
      "event:/recomputing",         "event:/u_turn",
  };
  // Not kept: SPF renumbers the events as banks come and go.
  const char *line = kLines[test_voice_next_++ % std::size(kLines)];
  const int event = sound->SND_FindEventIndexByPath(line);
  test_voice_missing_ = event < 0;
  void *instance = event >= 0 ? sound->SND_CreateEventInstance(event) : nullptr;
  if (!instance) {
    Context().LogFmt(SPF_LOG_WARN, "(CabinAudio) Couldn't play %s", line);
    return;
  }
  sound->SND_StartEvent(instance);
  // FMOD frees it once it has finished playing.
  sound->SND_ReleaseEvent(instance);
}

CabinAudioEffect::Head CabinAudioEffect::MakeHead(float yaw_rad, float x,
                                                  float y, float z) const {
  // The head's look and right, in the camera's frame (-z ahead). A
  // right-hand drive camera mirrors x but not the yaw.
  const float mirror = right_hand_drive_ ? -1.0f : 1.0f;
  const float sin_yaw = std::sin(yaw_rad), cos_yaw = std::cos(yaw_rad);
  return {x, y, z, -mirror * sin_yaw, -cos_yaw, mirror * cos_yaw, -sin_yaw};
}

CabinAudioEffect::Hearing CabinAudioEffect::Hear(float x, float y, float z,
                                                 const Head &head) const {
  const float dx = x - head.x, dy = y - head.y, dz = z - head.z;
  const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
  const float flat = std::hypot(dx, dz);
  // Against the whole distance: a speaker well below is less to one side.
  const float side = distance > 1e-3f
                         ? (dx * head.right_x + dz * head.right_z) / distance
                         : 0.0f;
  const float ahead =
      flat > 1e-3f ? (dx * head.look_x + dz * head.look_z) / flat : 1.0f;
  const float seated = std::sqrt(x * x + y * y + z * z);
  const float distance_gain =
      distance > seated ? std::max(kMinDistanceGain, seated / distance) : 1.0f;
  const float back =
      std::clamp((1.0f - ahead) * 0.5f, 0.0f, 1.0f) * Percent(muffle_);
  const float level = distance_gain * (1.0f - back * kBackAttenuation);
  const float back_cutoff = LogLerp(kOpenHz, kBackCutoffHz, back);
  Hearing hearing{};
  for (int ear = 0; ear < 2; ++ear) {
    const float shade = std::clamp(ear == 0 ? side : -side, 0.0f, 1.0f);
    hearing.gain[ear] = level * (1.0f - Percent(width_) * kMaxEarDrop * shade);
    hearing.cutoff_hz[ear] = std::max(
        kMinCutoffHz, back_cutoff * std::pow(kShadowCutoffHz / kOpenHz,
                                             Percent(head_shadow_) * shade));
  }
  return hearing;
}

SpatialMix CabinAudioEffect::CabinRadioMix(const Head &head) const {
  // The left channel plays in the left door: the driver's on a left-hand
  // drive truck, the passenger's on a right-hand drive one.
  const float driver_x = -kDoorOutX;
  const float passenger_x = 2.0f * centerline_x_ + kDoorOutX;
  const float doors[2] = {right_hand_drive_ ? passenger_x : driver_x,
                          right_hand_drive_ ? driver_x : passenger_x};
  // Balanced for the driver looking ahead, like a car's balance knob: the
  // far door's speaker turned up until both ears hear the same.
  const Head seated = MakeHead(0.0f, 0.0f, 0.0f, 0.0f);
  const Hearing left = Hear(doors[0], kDoorY, kDoorZ, seated);
  const Hearing right = Hear(doors[1], kDoorY, kDoorZ, seated);
  const float left_lead = left.gain[0] - left.gain[1];
  const float right_lead = right.gain[1] - right.gain[0];
  float trim[2] = {1.0f, 1.0f};
  if (left_lead > 1e-3f && right_lead > 1e-3f) {
    trim[1] = left_lead / right_lead;
    const float loudest = std::max(trim[0], trim[1]);
    trim[0] /= loudest;
    trim[1] /= loudest;
  }

  const float blend = Percent(blend_) * 0.5f; // at 100%, mono
  SpatialMix mix;
  for (int s = 0; s < SpatialMix::kSpeakers; ++s) {
    const Hearing hearing = Hear(doors[s], kDoorY, kDoorZ, head);
    mix.feed[s][s] = 1.0f - blend;
    mix.feed[s][1 - s] = blend;
    for (int ear = 0; ear < 2; ++ear) {
      mix.gain[s][ear] = trim[s] * hearing.gain[ear];
      mix.cutoff_hz[s][ear] = hearing.cutoff_hz[ear];
    }
  }
  return mix;
}

SpatialMix CabinAudioEffect::GpsMix(const Head &head) const {
  const Hearing hearing = Hear(kGpsX, kGpsY, kGpsZ, head);
  const float volume = std::pow(10.0f, gps_volume_db_ / 20.0f);
  // One speaker, the voice's two channels summed.
  SpatialMix mix;
  mix.feed[0][0] = mix.feed[0][1] = 0.5f;
  mix.feed[1][0] = mix.feed[1][1] = 0.0f;
  for (int ear = 0; ear < 2; ++ear) {
    mix.gain[0][ear] = volume * hearing.gain[ear];
    mix.cutoff_hz[0][ear] = hearing.cutoff_hz[ear];
    mix.gain[1][ear] = 0.0f;
  }
  return mix;
}

SpatialMix CabinAudioEffect::OutsideMix() const {
  const float amount = Percent(outside_);
  const float blend = kOutsideBlend * amount;
  const float level = math::Lerp(1.0f, kOutsideGain, amount);
  const float cutoff = LogLerp(kOpenHz, kOutsideCutoffHz, amount);
  SpatialMix mix;
  for (int s = 0; s < SpatialMix::kSpeakers; ++s) {
    mix.feed[s][s] = 1.0f - blend;
    mix.feed[s][1 - s] = blend;
    mix.gain[s][s] = level;
    mix.gain[s][1 - s] = level * kOutsideFarEar * amount;
    mix.cutoff_hz[s][0] = mix.cutoff_hz[s][1] = cutoff;
  }
  return mix;
}

void CabinAudioEffect::PlayRadio(float dt, bool outside) {
  void *group = CurrentGroup(radio_, dt);
  if (!group || (group != radio_.group && !Attach(radio_, group)))
    return;
  outside_weight_ = math::Lerp(outside_weight_, outside ? 1.0f : 0.0f,
                               Ease(dt, kOutsideSeconds));
  const float away = math::SmoothStep(outside_weight_);
  SpatialMix mix =
      Lerp(SpatialMix{}, Lerp(cabin_mix_, OutsideMix(), away), weight_);
  const float duck = math::Lerp(
      1.0f, kDuckedGain, Percent(duck_) * math::SmoothStep(ducking_) * weight_);
  const float volume =
      math::Lerp(1.0f, std::pow(10.0f, radio_volume_db_ / 20.0f), weight_);
  for (auto &speaker : mix.gain)
    for (float &gain : speaker)
      gain *= duck * volume;
  radio_.spatial.Set(mix);
  // Outside, the cabin's sound goes as far as the radio is muffled.
  ApplySpeakers(radio_, weight_ * math::Lerp(1.0f, Percent(outside_), away));
}

void CabinAudioEffect::Update(float head_yaw_rad, const HeadOffset &offset) {
  const float dt = Tick();
  presence_ = std::min(1.0f, presence_ + dt / kPauseFadeSeconds);
  if (!Prepare(dt))
    return;
  const Head head =
      MakeHead(head_yaw_rad, offset.pos_x, offset.pos_y, offset.pos_z);
  outside_last_ = false;
  head_last_ = head;
  const float ease = Ease(dt, kSmoothingSeconds);
  cabin_mix_ = Lerp(cabin_mix_, CabinRadioMix(head), ease);
  PlayGps(dt, &head);
  PlayRadio(dt, false);
}

void CabinAudioEffect::PlayGps(float dt, const Head *head) {
  voice_hold_ = std::max(0.0f, voice_hold_ - dt);
  // Listened to even when left as it is, as long as it ducks the radio.
  if (!gps_ && duck_ <= 0.0f && gps_placed_ < 1e-3f) {
    Detach(gps_link_);
    gps_placed_ = 0.0f;
  } else if (void *group = CurrentGroup(gps_link_, dt);
             group && (group == gps_link_.group || Attach(gps_link_, group))) {
    // Out of the cabin it eases back to the game's own, as the radio goes
    // outside, rather than its EQ and reverb cutting out.
    gps_placed_ = math::Lerp(gps_placed_, gps_ && head ? 1.0f : 0.0f,
                             Ease(dt, kOutsideSeconds));
    if (head)
      gps_mix_ = Lerp(gps_mix_, GpsMix(*head), Ease(dt, kSmoothingSeconds));
    const float amount = weight_ * math::SmoothStep(gps_placed_);
    gps_link_.spatial.Set(Lerp(SpatialMix{}, gps_mix_, amount));
    // The same cabin speakers as the radio, through a small one of its own.
    ApplySpeakers(gps_link_, amount);
    ApplySmallSpeaker(amount);
    if (gps_link_.spatial.TakePeak() > kVoiceThreshold)
      voice_hold_ = kVoiceHoldSeconds;
  }
  const float target = voice_hold_ > 0.0f ? 1.0f : 0.0f;
  ducking_ = math::Lerp(
      ducking_, target,
      Ease(dt, target > ducking_ ? kDuckAttackSeconds : kDuckReleaseSeconds));
}

void CabinAudioEffect::UpdateOutside() {
  const float dt = Tick();
  presence_ = std::min(1.0f, presence_ + dt / kPauseFadeSeconds);
  outside_last_ = true;
  if (!Prepare(dt))
    return;
  PlayGps(dt, nullptr);
  PlayRadio(dt, true);
}

void CabinAudioEffect::UpdatePaused() {
  const float dt = Tick();
  test_voice_requested_ = false;
  presence_ = std::max(0.0f, presence_ - dt / kPauseFadeSeconds);
  if (!Prepare(dt))
    return;
  // As it was when the menu came up, fading out.
  PlayGps(dt, outside_last_ ? nullptr : &head_last_);
  PlayRadio(dt, outside_last_);
}

void CabinAudioEffect::Release() {
  // The radio comes back at full volume: a voice cut off by a menu won't
  // finish.
  ducking_ = 0.0f;
  voice_hold_ = 0.0f;
  Detach(radio_);
  Detach(gps_link_);
}

void CabinAudioEffect::Shutdown() {
  Release();
  // The next world fades in from the game's own sound.
  presence_ = 0.0f;
  if (core_ && fmod_.StudioSystem() == studio_) {
    for (Link *link : {&radio_, &gps_link_}) {
      for (const Dsp &dsp : link->dsps)
        if (dsp.handle)
          fmod_.DSP_Release(dsp.handle);
      link->spatial.Release(fmod_);
    }
  }
  Forget();
}

void CabinAudioEffect::Unload() {
  Shutdown();
  fmod_.Unload();
}

void CabinAudioEffect::Forget() {
  studio_ = nullptr;
  core_ = nullptr;
  for (Link *link : {&radio_, &gps_link_}) {
    link->bus = nullptr;
    link->group = nullptr;
    link->retry_seconds = 0.0f;
    link->missing_seconds = 0.0f;
    for (Dsp &dsp : link->dsps)
      dsp = {};
    link->dsps_tried = false;
    link->spatial.Forget();
  }
}

} // namespace motioncab
