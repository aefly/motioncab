#pragma once

#include <algorithm>
#include <cmath>

namespace motioncab::math {

// Damped mass-spring driven by an external acceleration:
//   x'' + 2 * zeta * omega * x' + omega^2 * x = drive
// Unlike SpringDamper1D (critically damped, follows a target), it can
// overshoot and ring, which is what a sprung seat does after a bump. At
// rest under a constant drive it settles at drive / omega^2.
class Oscillator1D {
public:
  // `omega` in rad/s (natural frequency), `zeta` the damping ratio.
  void Configure(float omega, float zeta) {
    omega_ = omega > 1e-3f ? omega : 1e-3f;
    zeta_ = zeta > 0.0f ? zeta : 0.0f;
  }

  void Reset() {
    x_ = 0.0f;
    v_ = 0.0f;
  }

  float Update(float drive, float dt) {
    if (dt <= 0.0f)
      return x_;
    // A frame hitch shouldn't be integrated as one huge step.
    dt = std::min(dt, kMaxFrameSeconds);
    // Semi-implicit Euler stays stable only while omega * step is well
    // below 2, so a stiff spring at a low frame rate is substepped.
    const int steps =
        std::max(1, static_cast<int>(std::ceil(dt / kMaxStepSeconds)));
    const float h = dt / static_cast<float>(steps);
    for (int i = 0; i < steps; ++i) {
      const float a = drive - 2.0f * zeta_ * omega_ * v_ - omega_ * omega_ * x_;
      v_ += a * h;
      x_ += v_ * h;
    }
    return x_;
  }

  float value() const { return x_; }

private:
  static constexpr float kMaxFrameSeconds = 0.1f;
  static constexpr float kMaxStepSeconds = 1.0f / 240.0f;

  float omega_ = 8.0f;
  float zeta_ = 0.3f;
  float x_ = 0.0f;
  float v_ = 0.0f;
};

} // namespace motioncab::math
