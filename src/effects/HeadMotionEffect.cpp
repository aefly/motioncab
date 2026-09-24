#include "HeadMotionEffect.hpp"

namespace motioncab {

namespace {
// Base response gains; the user-facing "strength" settings scale these.
constexpr float kTranslationGain = 0.03f; // meters per (m/s^2)
constexpr float kRotationGain = 8.6f;     // degrees per (rad/s)
} // namespace

void HeadMotionEffect::LoadConfig() {
  if (!config_api_ || !config_handle_)
    return;

  enabled_ = config_api_->Cfg_GetBool(
      config_handle_, "settings.driving.head_motion.enabled", enabled_);
  sway_strength_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.driving.head_motion.sway_strength",
      sway_strength_));
  tilt_strength_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.driving.head_motion.tilt_strength",
      tilt_strength_));
  smoothing_time_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.driving.head_motion.smoothing_time",
      smoothing_time_));

  sway_x_.SetTimeConstant(smoothing_time_);
  sway_z_.SetTimeConstant(smoothing_time_);
  tilt_yaw_.SetTimeConstant(smoothing_time_);
  tilt_pitch_.SetTimeConstant(smoothing_time_);
}

void HeadMotionEffect::Reset() {
  sway_x_.Reset();
  sway_z_.Reset();
  tilt_yaw_.Reset();
  tilt_pitch_.Reset();
}

HeadOffset HeadMotionEffect::Update(float dt, const SPF_TruckData &truck,
                                    const SPF_Controls & /*controls*/) {
  const SPF_FVector &accel = truck.local_linear_acceleration;
  const SPF_FVector &cabin_omega = truck.cabin_angular_velocity;

  // Inertia pulls the head opposite to the applied acceleration.
  const float target_x = -accel.x * kTranslationGain * sway_strength_;
  const float target_z = -accel.z * kTranslationGain * sway_strength_;

  // Cabin roll/pitch rate produces a small counter-tilt of the head.
  const float target_yaw = -cabin_omega.y * kRotationGain * tilt_strength_;
  const float target_pitch = -cabin_omega.x * kRotationGain * tilt_strength_;

  HeadOffset offset;
  offset.pos_x = sway_x_.Update(target_x, dt);
  offset.pos_z = sway_z_.Update(target_z, dt);
  offset.yaw = tilt_yaw_.Update(target_yaw, dt);
  offset.pitch = tilt_pitch_.Update(target_pitch, dt);
  return offset;
}

} // namespace motioncab
