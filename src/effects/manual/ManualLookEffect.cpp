#include "ManualLookEffect.hpp"

#include "core/Keybinds.hpp"

namespace motioncab {

namespace {

const keybinds::Action *ActionOf(ManualLookEffect::Look look) {
  using enum ManualLookEffect::Look;
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

void ManualLookEffect::LoadSettings() {
  look_left_deg_ = Float("look_left_deg", look_left_deg_);
  look_right_deg_ = Float("look_right_deg", look_right_deg_);
  glance_left_deg_ = Float("glance_left_deg", glance_left_deg_);
  glance_right_deg_ = Float("glance_right_deg", glance_right_deg_);
  glance_pitch_deg_ = Float("glance_pitch_deg", glance_pitch_deg_);
  smoothing_time_ = Float("smoothing_time", smoothing_time_);
  toggle_mode_ = Bool("toggle_mode", toggle_mode_);

  yaw_.SetTimeConstant(smoothing_time_);
  pitch_.SetTimeConstant(smoothing_time_);
}

void ManualLookEffect::Reset() {
  yaw_.Reset();
  pitch_.Reset();
  pressed_ = Look::kCenter;
  toggled_ = Look::kCenter;
  toggled_before_press_ = Look::kCenter;
}

bool ManualLookEffect::HasChord(Look look) const {
  const keybinds::Action *action = ActionOf(look);
  return action && keybinds::HasChord(keybinds_api_, keybinds_handle_, *action);
}

ManualLookEffect::Look ManualLookEffect::HeldLook() const {
  // In priority order when no combination decides: a wide look first.
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

HeadOffset ManualLookEffect::Update(float dt, const SPF_TruckData & /*truck*/,
                                    const SPF_Controls & /*controls*/) {
  if (!keybinds_api_ || !keybinds_handle_)
    return {};

  const Look held = HeldLook();
  if (held == Look::kCenter) {
    pressed_ = Look::kCenter;
  } else if (held != pressed_ && !(HasChord(pressed_) && !HasChord(held))) {
    // A combination completed on top of its own key held alone replaces
    // that key's look, toggle included; anything else is a new press.
    const bool completes_chord =
        pressed_ != Look::kCenter && HasChord(held) && !HasChord(pressed_);
    if (!completes_chord)
      toggled_before_press_ = toggled_;
    pressed_ = held;
    toggled_ = (toggled_before_press_ == held) ? Look::kCenter : held;
  }
  const Look look = toggle_mode_ ? toggled_ : pressed_;

  // Left = positive yaw, right = negative, and positive pitch looks down,
  // matching the conventions empirically confirmed for MirrorCheckEffect.
  float target_yaw_deg = 0.0f;
  float target_pitch_deg = 0.0f;
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
    target_pitch_deg = glance_pitch_deg_;
    break;
  case Look::kGlanceRight:
    target_yaw_deg = -glance_right_deg_;
    target_pitch_deg = glance_pitch_deg_;
    break;
  }

  HeadOffset offset;
  offset.yaw = yaw_.Update(target_yaw_deg, dt);
  offset.pitch = pitch_.Update(target_pitch_deg, dt);
  return offset;
}

} // namespace motioncab
