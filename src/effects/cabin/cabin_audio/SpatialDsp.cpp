#include "SpatialDsp.hpp"

#include "core/PluginContext.hpp"
#include "effects/cabin/cabin_audio/FmodLibrary.hpp"
#include "math/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace motioncab {

namespace {

// FMOD_PLUGIN_SDK_VERSION, the same through FMOD 2.x.
constexpr unsigned kPluginSdkVersion = 110;
constexpr float kCenterShare = 0.70710678f; // a center channel, in each ear

// The start of FMOD_DSP_STATE, the same in every 2.x.
struct DspState {
  void *instance;
  void *plugindata;
};

using CreateFn = int (*)(DspState *state);
using ReadFn = int (*)(DspState *state, float *in, float *out, unsigned length,
                       int in_channels, int *out_channels);

// FMOD_DSP_DESCRIPTION (FMOD 2.x), the callbacks we don't use left null.
struct DspDescription {
  unsigned plugin_sdk_version;
  char name[32];
  unsigned version;
  int num_input_buffers;
  int num_output_buffers;
  CreateFn create;
  void *release;
  void *reset;
  ReadFn read;
  void *process;
  void *set_position;
  int num_parameters;
  void **parameter_descs;
  void *set_parameter_float;
  void *set_parameter_int;
  void *set_parameter_bool;
  void *set_parameter_data;
  void *get_parameter_float;
  void *get_parameter_int;
  void *get_parameter_bool;
  void *get_parameter_data;
  void *should_i_process;
  void *user_data;
  void *sys_register;
  void *sys_deregister;
  void *sys_mix;
};

// The create callback runs inside System::createDSP, on the calling thread:
// that's how the instance learns which SpatialDsp it plays.
thread_local SpatialDsp *creating = nullptr;

int OnCreate(DspState *state) {
  state->plugindata = creating;
  return FmodLibrary::kOk;
}

int OnRead(DspState *state, float *in, float *out, unsigned length,
           int in_channels, int *out_channels) {
  auto *dsp = static_cast<SpatialDsp *>(state->plugindata);
  const int outs = out_channels ? *out_channels : in_channels;
  if (dsp)
    dsp->Process(in, out, length, in_channels, outs);
  else
    std::memcpy(out, in, sizeof(float) * length * in_channels);
  return FmodLibrary::kOk;
}

const DspDescription kDescription = [] {
  DspDescription description{};
  description.plugin_sdk_version = kPluginSdkVersion;
  std::strncpy(description.name, "MotionCab Spatial",
               sizeof(description.name) - 1);
  description.version = 1;
  description.num_input_buffers = 1;
  description.num_output_buffers = 1;
  description.create = &OnCreate;
  description.read = &OnRead;
  return description;
}();

// Which ear a channel belongs to (-1 left, 1 right, 0 both), by FMOD's
// speaker order for that many channels: front left and right, then center
// and LFE (one for 5.0, two from 5.1 up), then left/right pairs.
int SideOf(int channel, int channels) {
  if (channels == 1)
    return 0;
  if (channels == 3 || channels == 5) {
    if (channel == 2)
      return 0;
    if (channel > 2)
      --channel;
  } else if (channels >= 6) {
    if (channel == 2 || channel == 3)
      return 0;
    if (channel > 3)
      channel -= 2;
  }
  return channel % 2 == 0 ? -1 : 1;
}

float LogLerp(float from, float to, float t) {
  return std::exp(math::Lerp(std::log(from), std::log(to), t));
}

} // namespace

SpatialMix Lerp(const SpatialMix &from, const SpatialMix &to, float t) {
  SpatialMix mix;
  for (int s = 0; s < SpatialMix::kSpeakers; ++s) {
    for (int i = 0; i < 2; ++i) {
      mix.feed[s][i] = math::Lerp(from.feed[s][i], to.feed[s][i], t);
      mix.gain[s][i] = math::Lerp(from.gain[s][i], to.gain[s][i], t);
      mix.cutoff_hz[s][i] =
          LogLerp(from.cutoff_hz[s][i], to.cutoff_hz[s][i], t);
    }
  }
  return mix;
}

bool SpatialDsp::Create(const FmodLibrary &fmod, void *core) {
  if (handle_)
    return true;
  int sample_rate = 0;
  if (fmod.System_GetSoftwareFormat(core, &sample_rate, nullptr, nullptr) ==
          FmodLibrary::kOk &&
      sample_rate > 0)
    sample_rate_ = static_cast<float>(sample_rate);
  Set(SpatialMix{});
  started_ = false;
  creating = this;
  void *dsp = nullptr;
  const int result = fmod.System_CreateDSP(core, &kDescription, &dsp);
  creating = nullptr;
  if (result != FmodLibrary::kOk || !dsp) {
    // Tried again on every frame the bus plays.
    Context().LogThrottledFmt(
        SPF_LOG_WARN, "cabin_audio_spatial_create", 10000,
        "(CabinAudio) Couldn't create the spatial DSP (%d)", result);
    return false;
  }
  handle_ = dsp;
  return true;
}

void SpatialDsp::Release(const FmodLibrary &fmod) {
  if (!handle_)
    return;
  fmod.DSP_DisconnectAll(handle_, 1, 1);
  fmod.DSP_Release(handle_);
  handle_ = nullptr;
}

void SpatialDsp::Set(const SpatialMix &mix) {
  const float open = SpatialMix::kOpenHz;
  for (int s = 0; s < SpatialMix::kSpeakers; ++s) {
    for (int e = 0; e < 2; ++e) {
      feed_[s][e].store(mix.feed[s][e], std::memory_order_relaxed);
      gain_[s][e].store(mix.gain[s][e], std::memory_order_relaxed);
      // A one-pole lowpass, eased out to none over the top octave: at 22 kHz
      // its coefficient isn't quite 1, and a jump there would click.
      const float cutoff = std::clamp(mix.cutoff_hz[s][e], 20.0f, open);
      const float coefficient =
          1.0f - std::exp(-math::kTwoPi * cutoff / sample_rate_);
      const float filtering =
          math::SmoothStep(std::clamp(std::log2(open / cutoff), 0.0f, 1.0f));
      coefficient_[s][e].store(math::Lerp(1.0f, coefficient, filtering),
                               std::memory_order_relaxed);
    }
  }
}

void SpatialDsp::Process(const float *in, float *out, unsigned length,
                         int in_channels, int out_channels) {
  constexpr int kSpeakers = SpatialMix::kSpeakers;
  float feed[kSpeakers][2], gain[kSpeakers][2], step[kSpeakers][2],
      coefficient[kSpeakers][2];
  for (int s = 0; s < kSpeakers; ++s) {
    for (int e = 0; e < 2; ++e) {
      feed[s][e] = feed_[s][e].load(std::memory_order_relaxed);
      gain[s][e] = gain_[s][e].load(std::memory_order_relaxed);
      coefficient[s][e] = coefficient_[s][e].load(std::memory_order_relaxed);
      if (!started_)
        current_gain_[s][e] = gain[s][e];
      // Ramped over the block, so a change never clicks.
      step[s][e] = length ? (gain[s][e] - current_gain_[s][e]) / length : 0.0f;
    }
  }
  started_ = true;

  int sides[32];
  const int ins = std::clamp(in_channels, 0, 32);
  for (int c = 0; c < ins; ++c)
    sides[c] = SideOf(c, in_channels);
  const float center = in_channels == 1 ? 1.0f : kCenterShare;

  float peak = 0.0f;
  for (unsigned i = 0; i < length; ++i) {
    const float *frame = in + static_cast<size_t>(i) * in_channels;
    float left = 0.0f, right = 0.0f;
    for (int c = 0; c < ins; ++c) {
      peak = std::max(peak, std::fabs(frame[c]));
      if (sides[c] < 0) {
        left += frame[c];
      } else if (sides[c] > 0) {
        right += frame[c];
      } else {
        left += center * frame[c];
        right += center * frame[c];
      }
    }
    float ears[2] = {0.0f, 0.0f};
    for (int s = 0; s < kSpeakers; ++s) {
      const float x = feed[s][0] * left + feed[s][1] * right;
      for (int e = 0; e < 2; ++e) {
        state_[s][e] += coefficient[s][e] * (x - state_[s][e]);
        current_gain_[s][e] += step[s][e];
        ears[e] += current_gain_[s][e] * state_[s][e];
      }
    }
    float *target = out + static_cast<size_t>(i) * out_channels;
    if (out_channels == 1) {
      target[0] = 0.5f * (ears[0] + ears[1]);
      continue;
    }
    target[0] = ears[0];
    target[1] = ears[1];
    for (int c = 2; c < out_channels; ++c)
      target[c] = 0.0f;
  }
  // Kept until the game thread takes it; a race only loses one block's.
  if (peak > peak_.load(std::memory_order_relaxed))
    peak_.store(peak, std::memory_order_relaxed);
  for (int s = 0; s < kSpeakers; ++s) {
    for (int e = 0; e < 2; ++e) {
      current_gain_[s][e] = gain[s][e];
      // Silence would leave the filters on denormals.
      if (std::fabs(state_[s][e]) < 1e-15f)
        state_[s][e] = 0.0f;
    }
  }
}

} // namespace motioncab
