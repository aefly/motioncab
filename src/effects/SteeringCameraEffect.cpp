#include "SteeringCameraEffect.hpp"

namespace motioncab {

void SteeringCameraEffect::LoadConfig() {
  if (!config_api_ || !config_handle_)
    return;

  enabled_ = config_api_->Cfg_GetBool(
      config_handle_, "settings.driving.steering_camera.enabled", enabled_);
  rotation_factor_deg_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.driving.steering_camera.rotation_factor_deg",
      rotation_factor_deg_));
  smoothing_time_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.driving.steering_camera.smoothing_time",
      smoothing_time_));
  delay_seconds_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.driving.steering_camera.delay_seconds",
      delay_seconds_));

  yaw_.SetTimeConstant(smoothing_time_);
}

void SteeringCameraEffect::Reset() {
  // Snapping yaw_ to 0 here (rather than deferring to the next Update())
  // meant the camera briefly centered on cabin re-entry even with the
  // wheel turned, then visibly rotated back into place once the spring
  // caught up. needs_resync_ instead re-anchors the spring to whatever the
  // wheel's current angle already is, on the very first frame back.
  needs_resync_ = true;
  elapsed_time_s_ = 0.0f;
  delay_head_ = 0;
  delay_count_ = 0;
}

void SteeringCameraEffect::PushSample(float time_s, float steering) {
  delay_buffer_[delay_head_] = {time_s, steering};
  delay_head_ = (delay_head_ + 1) % kDelayBufferCapacity;
  if (delay_count_ < kDelayBufferCapacity)
    ++delay_count_;
}

float SteeringCameraEffect::GetDelayedSteering(float target_time_s) const {
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
      const float span = newer.time_s - older.time_s;
      const float t =
          span > 1e-6f ? (target_time_s - older.time_s) / span : 0.0f;
      return older.steering + (newer.steering - older.steering) * t;
    }

    newer = older;
    newer_index = older_index;
  }

  // target_time predates every buffered sample: best effort, use the oldest.
  return newer.steering;
}

HeadOffset SteeringCameraEffect::Update(float dt,
                                        const SPF_TruckData & /*truck*/,
                                        const SPF_Controls &controls) {
  elapsed_time_s_ += dt;
  PushSample(elapsed_time_s_, controls.effectiveInput.steering);

  const float delayed_steering =
      GetDelayedSteering(elapsed_time_s_ - delay_seconds_);
  const float target_yaw_deg = delayed_steering * rotation_factor_deg_;

  if (needs_resync_) {
    yaw_.Reset(target_yaw_deg);
    needs_resync_ = false;
  }

  HeadOffset offset;
  offset.yaw = yaw_.Update(target_yaw_deg, dt);
  return offset;
}

} // namespace motioncab
