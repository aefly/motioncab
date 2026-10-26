#include "SideLookEffect.hpp"

#include "core/Keybinds.hpp"

#include <cmath>

namespace motioncab {

namespace {

// A little over 0, since a stopped truck still creeps a bit.
constexpr float kStationarySpeedThreshold = 0.3f; // m/s

const keybinds::Action *ActionOf(SideLookEffect::Look look) {
  using enum SideLookEffect::Look;
  switch (look) {
  case kLeft:
    return &keybinds::kLookLeft;
  case kRight:
    return &keybinds::kLookRight;
  case kGlanceLeft:
    return &keybinds::kGlanceLeft;
  case kGlanceRight:
    return &keybinds::kGlanceRight;
  case kCenter:
    break;
  }
  return nullptr;
}

} // namespace

void SideLookEffect::LoadSettings() {
  look_left_deg_ = Float("look_left_deg", look_left_deg_);
  look_right_deg_ = Float("look_right_deg", look_right_deg_);
  glance_left_deg_ = Float("glance_left_deg", glance_left_deg_);
  glance_right_deg_ = Float("glance_right_deg", glance_right_deg_);
  glance_left_pitch_deg_ =
      Float("glance_left_pitch_deg", glance_left_pitch_deg_);
  glance_right_pitch_deg_ =
      Float("glance_right_pitch_deg", glance_right_pitch_deg_);
  glance_left_fov_deg_ = Float("glance_left_fov_deg", glance_left_fov_deg_);
  glance_right_fov_deg_ = Float("glance_right_fov_deg", glance_right_fov_deg_);
  smoothing_time_ = Float("smoothing_time", smoothing_time_);
  toggle_mode_ = Bool("toggle_mode", toggle_mode_);
  mirror_on_signal_ = Bool("mirror_on_signal", mirror_on_signal_);
  require_stationary_ = Bool("require_stationary", require_stationary_);
  ignore_after_moving_signal_ =
      Bool("ignore_after_moving_signal", ignore_after_moving_signal_);

  yaw_intent_.SetTimeConstant(smoothing_time_ * kIntentFraction);
  yaw_.SetTimeConstant(smoothing_time_ * kHeadFraction);
  pitch_intent_.SetTimeConstant(smoothing_time_ * kIntentFraction);
  pitch_.SetTimeConstant(smoothing_time_ * kHeadFraction);
  fov_intent_.SetTimeConstant(smoothing_time_ * kIntentFraction);
  fov_.SetTimeConstant(smoothing_time_ * kHeadFraction);
}

void SideLookEffect::Reset() {
  yaw_intent_.Reset();
  yaw_.Reset();
  pitch_intent_.Reset();
  pitch_.Reset();
  fov_intent_.Reset();
  fov_.Reset();
  pressed_ = Look::kCenter;
  toggled_ = Look::kCenter;
  toggled_before_press_ = Look::kCenter;
  forward_edge_.Reset();
  // The blinker state is kept: this runs on every cabin re-entry, and a
  // blinker switched on while driving would then look freshly switched on
  // while stopped.
}

bool SideLookEffect::HasChord(Look look) const {
  const keybinds::Action *action = ActionOf(look);
  return action && keybinds::HasChord(keybinds_api_, keybinds_handle_, *action);
}

SideLookEffect::Look SideLookEffect::HeldLook() const {
  // When no combination decides, a wide look wins.
  static constexpr Look kLooks[] = {Look::kLeft, Look::kRight,
                                    Look::kGlanceLeft, Look::kGlanceRight};
  Look first_held = Look::kCenter;
  for (const Look look : kLooks) {
    if (!keybinds::IsHeld(keybinds_api_, keybinds_handle_, *ActionOf(look)))
      continue;
    if (HasChord(look))
      return look;
    if (first_held == Look::kCenter)
      first_held = look;
  }
  return first_held;
}

SideLookEffect::Look SideLookEffect::SignalLook(const SPF_TruckData &truck) {
  const bool is_moving = std::fabs(truck.speed) > kStationarySpeedThreshold;

  // A signal started while driving stays ignored even if the truck then
  // stops, e.g. waiting in traffic mid lane change.
  if (truck.lblinker && !prev_lblinker_)
    lblinker_moving_at_activation_ = is_moving;
  if (truck.rblinker && !prev_rblinker_)
    rblinker_moving_at_activation_ = is_moving;
  prev_lblinker_ = truck.lblinker;
  prev_rblinker_ = truck.rblinker;

  if (!mirror_on_signal_)
    return Look::kCenter;
  const auto allowed = [&](bool moving_at_activation) {
    return !require_stationary_ ||
           !(ignore_after_moving_signal_ ? moving_at_activation : is_moving);
  };
  if (truck.lblinker && allowed(lblinker_moving_at_activation_))
    return Look::kGlanceLeft;
  if (truck.rblinker && allowed(rblinker_moving_at_activation_))
    return Look::kGlanceRight;
  return Look::kCenter;
}

HeadOffset SideLookEffect::Update(float dt, const SPF_TruckData &truck,
                                  const SPF_Controls & /*controls*/) {
  const Look signal = SignalLook(truck);
  if (!keybinds_api_ || !keybinds_handle_)
    return {};

  if (forward_edge_.Update(keybinds::IsHeld(keybinds_api_, keybinds_handle_,
                                            keybinds::kLookForward))) {
    toggled_ = Look::kCenter;
    toggled_before_press_ = Look::kCenter;
    signal_dismissed_ = signal != Look::kCenter;
  }
  if (signal == Look::kCenter)
    signal_dismissed_ = false;

  const Look held = HeldLook();
  if (held == Look::kCenter) {
    pressed_ = Look::kCenter;
  } else if (held != pressed_ && !(HasChord(pressed_) && !HasChord(held))) {
    // A combination completed on top of its own key replaces that key's
    // look, toggle included; anything else is a new press.
    const bool completes_chord =
        pressed_ != Look::kCenter && HasChord(held) && !HasChord(pressed_);
    if (!completes_chord)
      toggled_before_press_ = toggled_;
    pressed_ = held;
    toggled_ = (toggled_before_press_ == held) ? Look::kCenter : held;
  }
  const Look key_look = toggle_mode_ ? toggled_ : pressed_;
  const Look look = key_look != Look::kCenter ? key_look
                    : signal_dismissed_       ? Look::kCenter
                                              : signal;

  float target_yaw_deg = 0.0f;
  float target_pitch_deg = 0.0f;
  float target_fov_deg = 0.0f;
  switch (look) {
  case Look::kCenter:
    break;
  case Look::kLeft:
    target_yaw_deg = look_left_deg_;
    break;
  case Look::kRight:
    target_yaw_deg = -look_right_deg_;
    break;
  case Look::kGlanceLeft:
    target_yaw_deg = glance_left_deg_;
    target_pitch_deg = glance_left_pitch_deg_;
    target_fov_deg = glance_left_fov_deg_;
    break;
  case Look::kGlanceRight:
    target_yaw_deg = -glance_right_deg_;
    target_pitch_deg = glance_right_pitch_deg_;
    target_fov_deg = glance_right_fov_deg_;
    break;
  }

  HeadOffset offset;
  offset.yaw = yaw_.Update(yaw_intent_.Update(target_yaw_deg, dt), dt);
  offset.pitch = pitch_.Update(pitch_intent_.Update(target_pitch_deg, dt), dt);
  offset.fov = fov_.Update(fov_intent_.Update(target_fov_deg, dt), dt);
  return offset;
}

} // namespace motioncab
