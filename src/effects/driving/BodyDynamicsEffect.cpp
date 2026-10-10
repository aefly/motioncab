#include "BodyDynamicsEffect.hpp"

#include <algorithm>

namespace motioncab {

namespace {
// The strength settings scale these.
constexpr float kLeanGain = 0.4f; // degrees of roll per m/s^2
constexpr float kNodGain = 0.3f;  // degrees of pitch per m/s^2
// So a crash or a physics spike can't throw the camera around.
constexpr float kMaxLeanDeg = 4.0f;
constexpr float kMaxNodDeg = 3.0f;
// Tilts the head toward the outside of the corner.
constexpr float kLeanSign = -1.0f;
} // namespace

void BodyDynamicsEffect::LoadSettings() {
  lean_strength_ = Float("lean_strength", lean_strength_);
  nod_strength_ = Float("nod_strength", nod_strength_);
  smoothing_time_ = Float("smoothing_time", smoothing_time_);

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

  // Braking is positive Z and drops the head forward, which is negative
  // pitch.
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
