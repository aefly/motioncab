#include "BodyDynamicsEffect.hpp"

#include <algorithm>

namespace motioncab {

namespace {
// Base response gains; the user-facing strength settings scale these.
constexpr float kLeanGain = 0.4f; // degrees of roll per (m/s^2) lateral
constexpr float kNodGain = 0.3f;  // degrees of pitch per (m/s^2) longitudinal
// Cap so a collision or physics spike can't throw the camera around.
constexpr float kMaxLeanDeg = 4.0f;
constexpr float kMaxNodDeg = 3.0f;
// Roll direction: -1 tilts the head toward the outside of the corner
// (assuming positive roll is a tilt to the right). Flip if it feels inverted.
constexpr float kLeanSign = -1.0f;
} // namespace

void BodyDynamicsEffect::LoadConfig() {
  if (!config_api_ || !config_handle_)
    return;

  enabled_ = config_api_->Cfg_GetBool(
      config_handle_, "settings.driving.body_dynamics.enabled", enabled_);
  lean_strength_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.driving.body_dynamics.lean_strength",
      lean_strength_));
  nod_strength_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.driving.body_dynamics.nod_strength",
      nod_strength_));
  smoothing_time_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.driving.body_dynamics.smoothing_time",
      smoothing_time_));

  roll_.SetTimeConstant(smoothing_time_);
  pitch_.SetTimeConstant(smoothing_time_);
}

void BodyDynamicsEffect::Reset() {
  roll_.Reset();
  pitch_.Reset();
}

HeadOffset BodyDynamicsEffect::Update(float dt, const SPF_TruckData &truck,
                                      const SPF_Controls & /*controls*/) {
  const SPF_FVector &accel = truck.local_linear_acceleration;

  // Lateral acceleration points toward the inside of the corner; the head
  // leans the other way. Braking (positive Z) drops the head forward, i.e.
  // negative pitch (the interior default pitch is slightly negative = down).
  const float target_roll =
      std::clamp(kLeanSign * accel.x * kLeanGain * lean_strength_, -kMaxLeanDeg,
                 kMaxLeanDeg);
  const float target_pitch =
      std::clamp(-accel.z * kNodGain * nod_strength_, -kMaxNodDeg, kMaxNodDeg);

  HeadOffset offset;
  offset.roll = roll_.Update(target_roll, dt);
  offset.pitch = pitch_.Update(target_pitch, dt);
  return offset;
}

} // namespace motioncab
