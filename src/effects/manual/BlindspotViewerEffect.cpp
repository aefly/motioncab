#include "BlindspotViewerEffect.hpp"

#include "core/Keybinds.hpp"
#include "math/Noise.hpp"

#include <algorithm>

namespace motioncab {

namespace {

// How gradually the motion gets going, as a share of smoothing_time.
constexpr float kIntentFraction = 0.25f;
// A spring closes ~95% of the gap in about 2.37 time constants, so the body
// settles in about smoothing_time.
constexpr float kBodyFraction = 0.42f;
constexpr float kInertiaGain = 0.015f; // meters per m/s^2
constexpr float kInertiaMax = 0.06f;   // meters
constexpr float kInertiaTime = 0.3f;
// Holding the lean, the body drifts slowly and the head makes small
// corrections.
constexpr float kSwayPosM = 0.004f;
constexpr float kSwayPosHz = 0.23f;
constexpr float kSwayRotDeg = 0.25f;
constexpr float kSwayRotHz = 0.4f;

} // namespace

void BlindspotViewerEffect::LoadSettings() {
  pos_x_ = Float("pos_x", pos_x_);
  pos_y_ = Float("pos_y", pos_y_);
  pos_z_ = Float("pos_z", pos_z_);
  yaw_deg_ = Float("yaw_deg", yaw_deg_);
  pitch_deg_ = Float("pitch_deg", pitch_deg_);
  roll_deg_ = Float("roll_deg", roll_deg_);
  fov_offset_deg_ = Float("fov_offset_deg", fov_offset_deg_);
  smoothing_time_ = Float("smoothing_time", smoothing_time_);
  toggle_mode_ = Bool("toggle_mode", toggle_mode_);

  intent_.SetTimeConstant(smoothing_time_ * kIntentFraction);
  body_.SetTimeConstant(smoothing_time_ * kBodyFraction);
  inertia_x_.SetTimeConstant(kInertiaTime);
  inertia_z_.SetTimeConstant(kInertiaTime);
}

void BlindspotViewerEffect::Reset() {
  peeking_ = false;
  press_edge_.Reset();
  intent_.Reset();
  body_.Reset();
  inertia_x_.Reset();
  inertia_z_.Reset();
}

HeadOffset BlindspotViewerEffect::Update(float dt, const SPF_TruckData &truck,
                                         const SPF_Controls & /*controls*/) {
  if (!keybinds_api_ || !keybinds_handle_)
    return {};

  const bool pressed = keybinds::IsHeld(keybinds_api_, keybinds_handle_,
                                        keybinds::kBlindspotPeek);
  const bool press_edge = press_edge_.Update(pressed);
  if (!toggle_mode_)
    peeking_ = false; // no stale toggle if the mode is switched back
  else if (press_edge)
    peeking_ = !peeking_;

  const bool want_peek = toggle_mode_ ? peeking_ : pressed;

  // Two critically damped springs in a row: a slow start and a soft stop
  // like a real body's, with no overshoot to bounce back from.
  const float intent = intent_.Update(want_peek ? 1.0f : 0.0f, dt);
  const float lean = std::clamp(body_.Update(intent, dt), 0.0f, 1.0f);

  // Off the backrest, braking and accelerating throw the body around more.
  const SPF_FVector &accel = truck.local_linear_acceleration;
  auto inertia = [&](math::SpringDamper1D &spring, float a) {
    const float target =
        std::clamp(-a * kInertiaGain * lean, -kInertiaMax, kInertiaMax);
    return spring.Update(target, dt);
  };
  const float inertia_x = inertia(inertia_x_, accel.x);
  const float inertia_z = inertia(inertia_z_, accel.z);

  // The sway is invisible when seated, so its clock restarts there rather
  // than growing all session and losing float precision.
  sway_time_ = lean > 0.0f ? sway_time_ + dt : 0.0f;
  auto sway = [&](float hz, uint32_t seed) {
    return math::GradientNoise1D(sway_time_ * hz, seed) * lean;
  };

  HeadOffset offset;
  offset.pos_x = pos_x_ * lean + inertia_x + sway(kSwayPosHz, 11) * kSwayPosM;
  offset.pos_y = pos_y_ * lean;
  offset.pos_z = pos_z_ * lean + inertia_z + sway(kSwayPosHz, 23) * kSwayPosM;
  offset.yaw = yaw_deg_ * lean + sway(kSwayRotHz, 37) * kSwayRotDeg;
  offset.pitch = pitch_deg_ * lean + sway(kSwayRotHz, 41) * kSwayRotDeg;
  offset.roll = roll_deg_ * lean;
  offset.fov = fov_offset_deg_ * lean;
  return offset;
}

} // namespace motioncab
