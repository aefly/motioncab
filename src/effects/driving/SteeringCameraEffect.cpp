#include "SteeringCameraEffect.hpp"

#include "math/Units.hpp"

#include <algorithm>
#include <cmath>

namespace motioncab {

namespace {

// Steering input (fraction of full lock) from which the left/right rotation
// amounts fully apply. Closer to center, the wider side fades down to the
// narrower one, so small lane corrections don't pick up the wider setting.
constexpr float kSideBlendFullSteering = 0.2f;

// 0 at center, easing up to 1 at kSideBlendFullSteering (smoothstep). A
// smooth ramp rather than a hard threshold, which would make the camera
// jump when the wheel crosses it.
float SideWeight(float steering) {
  return math::SmoothStep(
      std::min(std::abs(steering) / kSideBlendFullSteering, 1.0f));
}

// `left` for positive steering, `right` for negative. The narrower side
// always gets exactly its own value; the wider one starts from the
// narrower value at center and ramps up to its own.
float SideValue(float steering, float left, float right) {
  const float narrower = std::min(left, right);
  const float side = steering >= 0.0f ? left : right;
  return narrower + (side - narrower) * SideWeight(steering);
}

} // namespace

void SteeringCameraEffect::LoadSettings() {
  rotation_left_deg_ = Float("rotation_left_deg", rotation_left_deg_);
  rotation_right_deg_ = Float("rotation_right_deg", rotation_right_deg_);
  smoothing_time_ = Float("smoothing_time", smoothing_time_);
  delay_seconds_ = Float("delay_seconds", delay_seconds_);
  disable_in_reverse_ = Bool("disable_in_reverse", disable_in_reverse_);

  yaw_.SetTimeConstant(smoothing_time_);
}

void SteeringCameraEffect::Reset() {
  // Snapping yaw_ to 0 here (rather than deferring to the next Update())
  // meant the camera briefly centered on cabin re-entry even with the
  // wheel turned, then visibly rotated back into place once the spring
  // caught up. needs_resync_ instead re-anchors the spring to whatever the
  // wheel's current angle already is, on the very first frame back.
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

  // target_time predates every buffered sample: best effort, use the oldest.
  return newer.steering;
}

HeadOffset SteeringCameraEffect::Update(float dt, const SPF_TruckData &truck,
                                        const SPF_Controls &controls) {
  elapsed_time_s_ += dt;
  PushSample(elapsed_time_s_, controls.effectiveInput.steering);

  const float delayed_steering =
      GetDelayedSteering(elapsed_time_s_ - delay_seconds_);
  // In reverse the driver looks at the mirrors, not down the road, so
  // following the wheel only gets in the way: aim back at center and let
  // the spring ease there instead of snapping.
  const bool reversing = disable_in_reverse_ && truck.gear < 0;
  // Positive steering and positive yaw both mean left (counterclockwise),
  // whatever SPF_ControlInput's comment says: steering times a single
  // factor has always turned the camera toward the wheel in-game.
  const float target_yaw_deg =
      reversing
          ? 0.0f
          : delayed_steering * SideValue(delayed_steering, rotation_left_deg_,
                                         rotation_right_deg_);

  if (needs_resync_) {
    yaw_.Reset(target_yaw_deg);
    needs_resync_ = false;
  }

  HeadOffset offset;
  offset.yaw = yaw_.Update(target_yaw_deg, dt);
  return offset;
}

} // namespace motioncab
