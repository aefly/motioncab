#pragma once

#include "SPF_Config_API.h"
#include "SPF_Sound_API.h"
#include "SPF_TelemetryData.h"
#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "effects/Effect.hpp"
#include "effects/cabin/cabin_audio/FmodLibrary.hpp"
#include "effects/cabin/cabin_audio/SpatialDsp.hpp"

#include <chrono>

namespace motioncab {

// 3D spatial audio for the game's radio and GPS voice, played from the cabin's
// speakers. SPF can't pan a bus, so our DSPs go straight onto the game's
// FMOD buses (FmodLibrary). No HeadOffset: it's not in the EffectManager.
// Temporary: it goes through the game's FMOD itself until SPF has an API
// for a bus's DSPs, which I still need to write.
class CabinAudioEffect {
public:
  CabinAudioEffect(SPF_Config_API *config_api,
                   SPF_Config_Handle *config_handle);

  bool IsEnabled() const { return enabled_; }
  void LoadConfig();
  void OnTruckConstantsChanged(const SPF_TruckConstants &constants);

  // Each interior frame. The head's yaw is its final one (radians, positive
  // left); `offset` is the head's offset from the player's own seat.
  void Update(float head_yaw_rad, const HeadOffset &offset);
  // Each frame in another view: the radio heard from outside the cabin, the
  // GPS voice the game's (still ducking the radio).
  void UpdateOutside();
  // Each paused frame (any native menu, F4 included, and the menu a save
  // loads into) and each frame with no truck: everything fades back to the
  // game's own sound, then our DSPs come out until it runs again.
  void UpdatePaused();
  // On world unload, while FMOD is still there: our DSPs go too, and every
  // FMOD handle is forgotten.
  void Shutdown();
  // On plugin unload: Shutdown, and the game's FMOD import put back.
  void Unload();

  // Plays one of the game's GPS lines on the next unpaused frame, to tune the
  // voice parked.
  void RequestTestVoice() { test_voice_requested_ = true; }
  void PlayTestVoice(SPF_Sound_API *sound);
  bool test_voice_missing() const { return test_voice_missing_; }

private:
  // The small speaker (FMOD's Multiband Equalizer) is on the GPS only.
  enum DspSlot { kReverb, kEq, kSmallSpeaker, kDspCount };
  struct Dsp {
    void *handle = nullptr;
    int params[3] = {-1, -1, -1};
  };
  // A bus of the game's, and its channel group once our DSPs are in it.
  struct Link {
    const char *name; // the last part of its path
    bool warn;        // a missing bus or group is worth a log line
    void *bus = nullptr;
    void *group = nullptr;
    float retry_seconds = 0.0f;
    float missing_seconds = 0.0f;
    bool bus_logged = false;
    bool group_logged = false;
    SpatialDsp spatial;
    Dsp dsps[kDspCount];
    bool dsps_tried = false;
    // What ApplySpeakers last wrote, so it writes only on a change.
    float applied[6] = {};
    bool speakers_applied = false;
  };
  struct Head {
    float x, y, z;
    float look_x, look_z, right_x, right_z;
  };
  struct Hearing {
    float gain[2];
    float cutoff_hz[2];
  };

  float Tick();
  bool Prepare(float dt);
  // Cheap when there's nothing to take out.
  void Release();
  void *CurrentGroup(Link &link, float dt);
  bool Attach(Link &link, void *group);
  void Detach(Link &link);
  void CreateDsps(Link &link);
  // `amount` scales the EQ and reverb: the fade, and outside the cabin.
  void ApplySpeakers(Link &link, float amount);
  void ApplySmallSpeaker(float amount);

  Head MakeHead(float yaw_rad, float x, float y, float z) const;
  Hearing Hear(float x, float y, float z, const Head &head) const;
  SpatialMix CabinRadioMix(const Head &head) const;
  SpatialMix GpsMix(const Head &head) const;
  SpatialMix OutsideMix() const;
  void PlayRadio(float dt, bool outside);
  // `head` null outside the cabin: the voice eased back to the game's own,
  // only listened to for the ducking.
  void PlayGps(float dt, const Head *head);
  void Forget();

  EffectConfig config_;
  bool enabled_ = settings::DefaultBool("settings.cabin.cabin_audio.enabled");
  float width_ = settings::Default("settings.cabin.cabin_audio.width");
  float head_shadow_ =
      settings::Default("settings.cabin.cabin_audio.head_shadow");
  float muffle_ = settings::Default("settings.cabin.cabin_audio.muffle");
  float bass_db_ = settings::Default("settings.cabin.cabin_audio.bass");
  float mid_db_ = settings::Default("settings.cabin.cabin_audio.mid");
  float treble_db_ = settings::Default("settings.cabin.cabin_audio.treble");
  float blend_ = settings::Default("settings.cabin.cabin_audio.blend");
  float reverb_ = settings::Default("settings.cabin.cabin_audio.reverb");
  float room_ms_ = settings::Default("settings.cabin.cabin_audio.room_size");
  float radio_volume_db_ =
      settings::Default("settings.cabin.cabin_audio.radio_volume");
  bool gps_ = settings::DefaultBool("settings.cabin.cabin_audio.gps");
  float gps_low_cut_hz_ =
      settings::Default("settings.cabin.cabin_audio.gps_low_cut");
  float gps_high_cut_hz_ =
      settings::Default("settings.cabin.cabin_audio.gps_high_cut");
  float gps_tone_hz_ = settings::Default("settings.cabin.cabin_audio.gps_tone");
  float gps_tone_db_ =
      settings::Default("settings.cabin.cabin_audio.gps_tone_boost");
  float gps_volume_db_ =
      settings::Default("settings.cabin.cabin_audio.gps_volume");
  float duck_ = settings::Default("settings.cabin.cabin_audio.duck");
  float outside_ = settings::Default("settings.cabin.cabin_audio.outside");
  bool right_hand_drive_ = false;
  float centerline_x_;
  // The on/off fade (0..1), and its eased weight on everything we change.
  float fade_ = 0.0f;
  float weight_ = 0.0f;
  // 0 while paused, eased like the toggle: also part of the weight.
  float presence_ = 0.0f;
  float outside_weight_ = 0.0f;
  // Where the last unpaused frame was, held while paused.
  bool outside_last_ = false;
  Head head_last_{};
  // 1 while the GPS voice is placed in the cabin (in the cabin and on),
  // eased: its mix, EQ, reverb and small speaker all follow it.
  float gps_placed_ = 0.0f;
  // The GPS voice heard lately (seconds left), and the radio's ducking under
  // it (0..1, eased).
  float voice_hold_ = 0.0f;
  float ducking_ = 0.0f;
  // Bands A, B, C: filter, frequency, Q, gain.
  int small_speaker_params_[3][4] = {};
  float small_speaker_values_[5] = {};
  bool small_speaker_applied_ = false;
  bool test_voice_requested_ = false;
  bool test_voice_missing_ = false;
  unsigned test_voice_next_ = 0;

  std::chrono::steady_clock::time_point last_tick_{};
  bool ticked_ = false;

  FmodLibrary fmod_;
  void *studio_ = nullptr;
  void *core_ = nullptr;
  Link radio_{"radio", true};
  Link gps_link_{"navigation", false};
  float no_studio_seconds_ = 0.0f;
  bool studio_logged_ = false;

  // Smoothed toward what the head asks: the cabin's mix lasts while outside.
  SpatialMix cabin_mix_, gps_mix_;
};

} // namespace motioncab
