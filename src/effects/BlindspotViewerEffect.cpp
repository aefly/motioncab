#include "BlindspotViewerEffect.hpp"

#include "math/Noise.hpp"

#include <algorithm>

namespace motioncab {

namespace {

// First stage's time, as a fraction of smoothing_time: how gradually the
// motion gets going.
constexpr float kIntentFraction = 0.25f;
// SpringDamper1D closes ~95% of the gap in about 2.37 time constants, so
// this makes the body's own stage settle in about smoothing_time.
constexpr float kBodyFraction = 0.42f;
// Extra sway from braking/acceleration while off the backrest.
constexpr float kInertiaGain = 0.015f; // meters per (m/s^2)
constexpr float kInertiaMax = 0.06f;   // meters
constexpr float kInertiaTime = 0.3f;   // seconds
// Postural sway while holding the lean: slow drift of the body and small
// corrections of the head.
constexpr float kSwayPosM = 0.004f;
constexpr float kSwayPosHz = 0.23f;
constexpr float kSwayRotDeg = 0.25f;
constexpr float kSwayRotHz = 0.4f;

} // namespace

void BlindspotViewerEffect::LoadConfig() {
  if (!config_api_ || !config_handle_)
    return;

  enabled_ = config_api_->Cfg_GetBool(
      config_handle_, "settings.manual.blindspot_viewer.enabled", enabled_);
  auto get_float = [&](const char *key, float fallback) {
    return static_cast<float>(
        config_api_->Cfg_GetFloat(config_handle_, key, fallback));
  };
  pos_x_ = get_float("settings.manual.blindspot_viewer.pos_x", pos_x_);
  pos_y_ = get_float("settings.manual.blindspot_viewer.pos_y", pos_y_);
  pos_z_ = get_float("settings.manual.blindspot_viewer.pos_z", pos_z_);
  yaw_deg_ = get_float("settings.manual.blindspot_viewer.yaw_deg", yaw_deg_);
  pitch_deg_ =
      get_float("settings.manual.blindspot_viewer.pitch_deg", pitch_deg_);
  roll_deg_ = get_float("settings.manual.blindspot_viewer.roll_deg", roll_deg_);
  fov_offset_deg_ = get_float("settings.manual.blindspot_viewer.fov_offset_deg",
                              fov_offset_deg_);
  smoothing_time_ = get_float("settings.manual.blindspot_viewer.smoothing_time",
                              smoothing_time_);
  toggle_mode_ = config_api_->Cfg_GetBool(
      config_handle_, "settings.manual.blindspot_viewer.toggle_mode",
      toggle_mode_);

  intent_.SetTimeConstant(smoothing_time_ * kIntentFraction);
  body_.SetTimeConstant(smoothing_time_ * kBodyFraction);
  inertia_x_.SetTimeConstant(kInertiaTime);
  inertia_z_.SetTimeConstant(kInertiaTime);
}

void BlindspotViewerEffect::Reset() {
  peeking_ = false;
  prev_pressed_ = false;
  intent_.Reset();
  body_.Reset();
  inertia_x_.Reset();
  inertia_z_.Reset();
}

HeadOffset BlindspotViewerEffect::Update(float dt, const SPF_TruckData &truck,
                                         const SPF_Controls & /*controls*/) {
  if (!keybinds_api_ || !keybinds_handle_)
    return {};

  // Immediate physical state, as ManualLookEffect does: MotionCab
  // implements its own toggle, ignoring the binding's own behavior.
  const bool pressed = keybinds_api_->Kbind_GetActionValue(
                           keybinds_handle_, "BlindspotViewer.peek") > 0.5f;
  if (!toggle_mode_)
    peeking_ = false; // no stale toggle state if the mode is switched back
  else if (pressed && !prev_pressed_)
    peeking_ = !peeking_;
  prev_pressed_ = pressed;

  const bool want_peek = toggle_mode_ ? peeking_ : pressed;

  // A soft first stage feeding the body's own, both critically damped so
  // it never overshoots the seat (or the peek) and bounces back.
  const float intent = intent_.Update(want_peek ? 1.0f : 0.0f, dt);
  const float lean = std::clamp(body_.Update(intent, dt), 0.0f, 1.0f);

  // Off the backrest, braking throws the body forward and accelerating
  // back more than when seated (HeadMotionEffect's sign convention).
  const SPF_FVector &accel = truck.local_linear_acceleration;
  auto inertia = [&](math::SpringDamper1D &spring, float a) {
    const float target =
        std::clamp(-a * kInertiaGain * lean, -kInertiaMax, kInertiaMax);
    return spring.Update(target, dt);
  };
  const float inertia_x = inertia(inertia_x_, accel.x);
  const float inertia_z = inertia(inertia_z_, accel.z);

  // Sway is scaled by lean, so restarting its clock while seated is
  // invisible and keeps it from growing (and losing float precision)
  // over a long session.
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
