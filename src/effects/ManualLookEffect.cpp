#include "ManualLookEffect.hpp"

namespace motioncab {

void ManualLookEffect::LoadConfig() {
  if (!config_api_ || !config_handle_)
    return;

  enabled_ = config_api_->Cfg_GetBool(
      config_handle_, "settings.manual.manual_look.enabled", enabled_);
  look_angle_deg_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.manual.manual_look.look_angle_deg",
      look_angle_deg_));
  smoothing_time_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.manual.manual_look.smoothing_time",
      smoothing_time_));
  toggle_mode_ = config_api_->Cfg_GetBool(
      config_handle_, "settings.manual.manual_look.toggle_mode", toggle_mode_);

  yaw_.SetTimeConstant(smoothing_time_);
}

void ManualLookEffect::Reset() {
  yaw_.Reset();
  toggle_direction_ = 0;
  prev_left_pressed_ = false;
  prev_right_pressed_ = false;
}

HeadOffset ManualLookEffect::Update(float dt, const SPF_TruckData & /*truck*/,
                                    const SPF_Controls & /*controls*/) {
  if (!keybinds_api_ || !keybinds_handle_)
    return {};

  // Immediate physical state, ignoring any hold/toggle behavior configured
  // for the binding itself; MotionCab implements its own toggle logic.
  const bool left_pressed =
      keybinds_api_->Kbind_GetActionValue(keybinds_handle_,
                                          "ManualLook.look_left") > 0.5f;
  const bool right_pressed =
      keybinds_api_->Kbind_GetActionValue(keybinds_handle_,
                                          "ManualLook.look_right") > 0.5f;
  const bool left_edge = left_pressed && !prev_left_pressed_;
  const bool right_edge = right_pressed && !prev_right_pressed_;
  prev_left_pressed_ = left_pressed;
  prev_right_pressed_ = right_pressed;

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
