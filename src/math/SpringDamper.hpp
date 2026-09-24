#pragma once

namespace motioncab::math {

// Critically-damped spring smoother (semi-implicit, frame-rate independent).
// `time_constant` is roughly the time to close ~95% of the gap to a new target.
class SpringDamper1D {
public:
  void SetTimeConstant(float seconds) {
    time_constant_ = seconds > 1e-4f ? seconds : 1e-4f;
  }

  void Reset(float value = 0.0f) {
    value_ = value;
    velocity_ = 0.0f;
  }

  float Update(float target, float dt) {
    if (dt <= 0.0f)
      return value_;

    const float omega = 2.0f / time_constant_;
    const float x = omega * dt;
    // Fast rational approximation of exp(-x), stable for any dt.
    const float exp_approx =
        1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

    const float change = value_ - target;
    const float temp = (velocity_ + omega * change) * dt;
    velocity_ = (velocity_ - omega * temp) * exp_approx;
    value_ = target + (change + temp) * exp_approx;
    return value_;
  }

  float value() const { return value_; }

private:
  float time_constant_ = 0.15f;
  float value_ = 0.0f;
  float velocity_ = 0.0f;
};

} // namespace motioncab::math
