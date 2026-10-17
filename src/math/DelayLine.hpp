#pragma once

#include <array>

namespace motioncab::math {

// Replays a value as it was a given number of seconds ago: push it each
// frame, read it back late. Reads interpolate between the stored frames, so
// the delay is the same at any frame rate.
class DelayLine {
public:
  void Reset() {
    clock_ = 0.0;
    head_ = 0;
    count_ = 0;
  }

  void Push(float value, float dt) {
    clock_ += dt;
    samples_[head_] = {clock_, value};
    head_ = (head_ + 1) % kCapacity;
    if (count_ < kCapacity)
      ++count_;
  }

  float Read(float delay_seconds) const {
    if (count_ == 0)
      return 0.0f;

    const double target = clock_ - delay_seconds;
    int newer_index = (head_ - 1 + kCapacity) % kCapacity;
    Sample newer = samples_[newer_index];
    for (int i = 1; i < count_; ++i) {
      const int older_index = (newer_index - 1 + kCapacity) % kCapacity;
      const Sample older = samples_[older_index];
      if (older.time_s <= target) {
        const double span = newer.time_s - older.time_s;
        const float t = span > 1e-6
                            ? static_cast<float>((target - older.time_s) / span)
                            : 0.0f;
        return older.value + (newer.value - older.value) * t;
      }
      newer = older;
      newer_index = older_index;
    }
    // Older than anything buffered: the oldest will do.
    return newer.value;
  }

private:
  // Plenty for a few tenths of a second at any frame rate.
  static constexpr int kCapacity = 300;

  // A float clock loses sub-millisecond precision after a couple of hours,
  // and the delay then turns short and jittery.
  struct Sample {
    double time_s = 0.0;
    float value = 0.0f;
  };

  double clock_ = 0.0;
  std::array<Sample, kCapacity> samples_{};
  int head_ = 0;
  int count_ = 0;
};

} // namespace motioncab::math
