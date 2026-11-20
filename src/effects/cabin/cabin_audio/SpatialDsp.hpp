#pragma once

#include <atomic>

namespace motioncab {

class FmodLibrary;

// How each speaker's sound reaches each ear: what a SpatialDsp plays. The
// default is the bus as the game plays it: speaker 0 is its left channel in
// the left ear, speaker 1 its right channel in the right ear.
struct SpatialMix {
  static constexpr int kSpeakers = 2;
  static constexpr float kOpenHz = 22000.0f; // no lowpass

  // Each speaker's share of the bus's left and right channels.
  float feed[kSpeakers][2] = {{1.0f, 0.0f}, {0.0f, 1.0f}};
  // Each speaker's level at each ear (0 left, 1 right), and the cutoff of
  // the lowpass on the way there (the head's shadow, the ear's back).
  float gain[kSpeakers][2] = {{1.0f, 0.0f}, {0.0f, 1.0f}};
  float cutoff_hz[kSpeakers][2] = {{kOpenHz, kOpenHz}, {kOpenHz, kOpenHz}};
};

// Levels in a straight line, cutoffs in log space, as hearing goes.
SpatialMix Lerp(const SpatialMix &from, const SpatialMix &to, float t);

// A DSP of our own in the game's FMOD (FMOD_DSP_DESCRIPTION with a read
// callback): every output channel pair needs its own filter per ear, which
// FMOD's mix matrix and built-in DSPs can't give. It mixes the bus down to
// left and right, feeds the speakers, and writes the two ears to the front
// pair (every other channel silent).
//
// The mixer thread reads what the game thread sets through atomics: a block
// with some values old and some new is harmless.
class SpatialDsp {
public:
  bool Create(const FmodLibrary &fmod, void *core);
  // Disconnected first, so it releases even if a group forgot to remove it.
  void Release(const FmodLibrary &fmod);
  void *handle() const { return handle_; }
  // Its sound system is gone, and the DSP with it.
  void Forget() { handle_ = nullptr; }

  void Set(const SpatialMix &mix);
  // The loudest input sample since the last call, 0 if nothing played (FMOD
  // stops reading an idle group): whether the bus is sounding.
  float TakePeak() { return peak_.exchange(0.0f, std::memory_order_relaxed); }

  // The mixer thread's side.
  void Process(const float *in, float *out, unsigned length, int in_channels,
               int out_channels);

private:
  void *handle_ = nullptr;
  float sample_rate_ = 48000.0f;

  std::atomic<float> feed_[SpatialMix::kSpeakers][2];
  std::atomic<float> gain_[SpatialMix::kSpeakers][2];
  // One-pole lowpass coefficients, 1 for none.
  std::atomic<float> coefficient_[SpatialMix::kSpeakers][2];
  std::atomic<float> peak_{0.0f};

  // Mixer thread only.
  float current_gain_[SpatialMix::kSpeakers][2] = {};
  float state_[SpatialMix::kSpeakers][2] = {};
  bool started_ = false;
};

} // namespace motioncab
