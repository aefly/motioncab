#include "ManualLookEffect.hpp"

#include "core/Keybinds.hpp"

namespace motioncab {

namespace {

// Stage times, as fractions of smoothing_time: the first eases the head
// into the motion instead of throwing it at full acceleration on the
// press (or on a quick switch to another look mid-motion), the second
// moves it. Together they take about as long as the single spring did.
constexpr float kIntentFraction = 0.3f;
constexpr float kHeadFraction = 0.85f;

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
  glance_left_pitch_deg_ =
      Float("glance_left_pitch_deg", glance_left_pitch_deg_);
  glance_right_pitch_deg_ =
      Float("glance_right_pitch_deg", glance_right_pitch_deg_);
  glance_left_fov_deg_ = Float("glance_left_fov_deg", glance_left_fov_deg_);
  glance_right_fov_deg_ = Float("glance_right_fov_deg", glance_right_fov_deg_);
  smoothing_time_ = Float("smoothing_time", smoothing_time_);
  toggle_mode_ = Bool("toggle_mode", toggle_mode_);

  yaw_intent_.SetTimeConstant(smoothing_time_ * kIntentFraction);
  yaw_.SetTimeConstant(smoothing_time_ * kHeadFraction);
  pitch_intent_.SetTimeConstant(smoothing_time_ * kIntentFraction);
  pitch_.SetTimeConstant(smoothing_time_ * kHeadFraction);
  fov_intent_.SetTimeConstant(smoothing_time_ * kIntentFraction);
  fov_.SetTimeConstant(smoothing_time_ * kHeadFraction);
}

void ManualLookEffect::Reset() {
  yaw_intent_.Reset();
  yaw_.Reset();
  pitch_intent_.Reset();
  pitch_.Reset();
  fov_intent_.Reset();
  fov_.Reset();
  pressed_ = Look::kCenter;
  toggled_ = Look::kCenter;
  toggled_before_press_ = Look::kCenter;
  engaged_ = false;
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

  // Left = positive yaw, right = negative, and positive pitch looks up.
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

  // Where Mirror Check has the head (see class comment).
  const float mirror_yaw = mirror_check_ ? mirror_check_->yaw() : 0.0f;
  const float mirror_pitch = mirror_check_ ? mirror_check_->pitch() : 0.0f;
  const float mirror_fov = mirror_check_ ? mirror_check_->fov() : 0.0f;
  if (engaged_ != (look != Look::kCenter)) {
    engaged_ = !engaged_;
    const float sign = engaged_ ? 1.0f : -1.0f;
    yaw_intent_.Shift(sign * mirror_yaw);
    yaw_.Shift(sign * mirror_yaw);
    pitch_intent_.Shift(sign * mirror_pitch);
    pitch_.Shift(sign * mirror_pitch);
    fov_intent_.Shift(sign * mirror_fov);
    fov_.Shift(sign * mirror_fov);
  }
  const float base_yaw = engaged_ ? mirror_yaw : 0.0f;
  const float base_pitch = engaged_ ? mirror_pitch : 0.0f;
  const float base_fov = engaged_ ? mirror_fov : 0.0f;

  HeadOffset offset;
  offset.yaw =
      yaw_.Update(yaw_intent_.Update(target_yaw_deg, dt), dt) - base_yaw;
  offset.pitch = pitch_.Update(pitch_intent_.Update(target_pitch_deg, dt), dt) -
                 base_pitch;
  offset.fov =
      fov_.Update(fov_intent_.Update(target_fov_deg, dt), dt) - base_fov;
  return offset;
}

} // namespace motioncab
