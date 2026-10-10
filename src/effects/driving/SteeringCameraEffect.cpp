#include "SteeringCameraEffect.hpp"

#include "math/Units.hpp"

#include <algorithm>
#include <cmath>

namespace motioncab {

namespace {

// The wheel is often at full lock after a maneuver: aiming straight back at
// it made the spring swing the camera across in one go.
constexpr float kReverseFadeSeconds = 1.5f;

// Integral of SmoothStep(x / zone) from 0 to x.
float SmoothStepIntegral(float x, float zone) {
  if (x >= zone)
    return x - 0.5f * zone;
  const float u = x / zone;
  return zone * u * u * u * (1.0f - 0.5f * u);
}

// The narrower side is a straight line. On the wider one the rate (degrees
// per unit of steering) eases up from the narrower side's over `zone`,
// overshooting a little so full lock still lands on its amount. It's the
// rate that's eased, not the amount: blending the amount made the rate
// overshoot just past center, and the camera lurched.
float SteeringYaw(float steering, float left, float right, float zone) {
  const float narrower = std::min(left, right);
  const float side = steering >= 0.0f ? left : right;
  const float wide_rate = narrower + (side - narrower) / (1.0f - 0.5f * zone);
  const float x = std::abs(steering);
  const float yaw =
      narrower * x + (wide_rate - narrower) * SmoothStepIntegral(x, zone);
  return steering >= 0.0f ? yaw : -yaw;
}

} // namespace

void SteeringCameraEffect::LoadSettings() {
  rotation_left_deg_ = Float("rotation_left_deg", rotation_left_deg_);
  rotation_right_deg_ = Float("rotation_right_deg", rotation_right_deg_);
  // At 200% SteeringYaw() would divide by zero.
  center_zone_pct_ =
      std::clamp(Float("center_zone_pct", center_zone_pct_), 0.0f, 100.0f);
  smoothing_time_ = Float("smoothing_time", smoothing_time_);
  delay_seconds_ = Float("delay_seconds", delay_seconds_);
  disable_in_reverse_ = Bool("disable_in_reverse", disable_in_reverse_);

  yaw_.SetTimeConstant(smoothing_time_);
}

void SteeringCameraEffect::Reset() {
  // Not reset to 0: with the wheel turned, the camera would center on cabin
  // re-entry and then swing back. The next Update() puts the spring right
  // where the wheel is instead.
  needs_resync_ = true;
  elapsed_time_s_ = 0.0;
  delay_head_ = 0;
  delay_count_ = 0;
}

void SteeringCameraEffect::PushSample(double time_s, float steering) {
  delay_buffer_[delay_head_] = {time_s, steering};
  delay_head_ = (delay_head_ + 1) % kDelayBufferCapacity;
  if (delay_count_ < kDelayBufferCapacity)
    ++delay_count_;
}

float SteeringCameraEffect::GetDelayedSteering(double target_time_s) const {
  if (delay_count_ == 0)
    return 0.0f;

  int newer_index =
      (delay_head_ - 1 + kDelayBufferCapacity) % kDelayBufferCapacity;
  Sample newer = delay_buffer_[newer_index];

  for (int i = 1; i < delay_count_; ++i) {
    const int older_index =
        (newer_index - 1 + kDelayBufferCapacity) % kDelayBufferCapacity;
    const Sample older = delay_buffer_[older_index];

    if (older.time_s <= target_time_s) {
      const double span = newer.time_s - older.time_s;
      const float t =
          span > 1e-6
              ? static_cast<float>((target_time_s - older.time_s) / span)
              : 0.0f;
      return older.steering + (newer.steering - older.steering) * t;
    }

    newer = older;
    newer_index = older_index;
  }

  // Older than anything buffered: the oldest will do.
  return newer.steering;
}

HeadOffset SteeringCameraEffect::Update(float dt, const SPF_TruckData &truck,
                                        const SPF_Controls &controls) {
  elapsed_time_s_ += dt;
  PushSample(elapsed_time_s_, controls.effectiveInput.steering);

  const float delayed_steering =
      GetDelayedSteering(elapsed_time_s_ - delay_seconds_);
  // Reversing, the driver looks at the mirrors rather than down the road, so
  // following the wheel only gets in the way.
  const bool reversing = disable_in_reverse_ && truck.gear < 0;
  const float follow_target = reversing ? 0.0f : 1.0f;
  if (needs_resync_) {
    follow_ = follow_target;
  } else {
    const float step = dt / kReverseFadeSeconds;
    follow_ = std::clamp(follow_target, follow_ - step, follow_ + step);
  }
  // Positive steering is left, whatever SPF_ControlInput's comment says: it
  // turns the camera the right way in game.
  const float target_yaw_deg =
      math::SmoothStep(follow_) *
      SteeringYaw(delayed_steering, rotation_left_deg_, rotation_right_deg_,
                  center_zone_pct_ / 100.0f);

  if (needs_resync_) {
    yaw_.Reset(target_yaw_deg);
    needs_resync_ = false;
  }

  HeadOffset offset;
  offset.yaw = yaw_.Update(target_yaw_deg, dt);
  return offset;
}

} // namespace motioncab
