#include "ManualLookEffect.hpp"

#include "core/Keybinds.hpp"

namespace motioncab {

void ManualLookEffect::LoadSettings() {
  look_angle_deg_ = Float("look_angle_deg", look_angle_deg_);
  smoothing_time_ = Float("smoothing_time", smoothing_time_);
  toggle_mode_ = Bool("toggle_mode", toggle_mode_);

  yaw_.SetTimeConstant(smoothing_time_);
}

void ManualLookEffect::Reset() {
  yaw_.Reset();
  toggle_direction_ = 0;
  left_edge_.Reset();
  right_edge_.Reset();
}

HeadOffset ManualLookEffect::Update(float dt, const SPF_TruckData & /*truck*/,
                                    const SPF_Controls & /*controls*/) {
  if (!keybinds_api_ || !keybinds_handle_)
    return {};

  const bool left_pressed =
      keybinds::IsHeld(keybinds_api_, keybinds_handle_, keybinds::kLookLeft);
  const bool right_pressed =
      keybinds::IsHeld(keybinds_api_, keybinds_handle_, keybinds::kLookRight);
  const bool left_edge = left_edge_.Update(left_pressed);
  const bool right_edge = right_edge_.Update(right_pressed);

  float target_yaw_deg = 0.0f;

  // Left = positive yaw, right = negative, matching the convention
  // empirically confirmed for MirrorCheckEffect.
  if (toggle_mode_) {
    if (left_edge)
      toggle_direction_ = (toggle_direction_ == 1) ? 0 : 1;
    else if (right_edge)
      toggle_direction_ = (toggle_direction_ == -1) ? 0 : -1;

    if (toggle_direction_ == 1)
      target_yaw_deg = look_angle_deg_;
    else if (toggle_direction_ == -1)
      target_yaw_deg = -look_angle_deg_;
  } else {
    if (left_pressed)
      target_yaw_deg = look_angle_deg_;
    else if (right_pressed)
      target_yaw_deg = -look_angle_deg_;
  }

  HeadOffset offset;
  offset.yaw = yaw_.Update(target_yaw_deg, dt);
  return offset;
}

} // namespace motioncab
