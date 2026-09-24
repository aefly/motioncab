#include "MirrorCheckEffect.hpp"

#include <cmath>

namespace motioncab {

namespace {
// Below this, the truck is considered "stationary" for require_stationary_
// purposes, a small margin over exact 0 to absorb residual physics creep.
constexpr float kStationarySpeedThreshold = 0.3f; // meters/second
} // namespace

void MirrorCheckEffect::LoadConfig() {
  if (!config_api_ || !config_handle_)
    return;

  enabled_ = config_api_->Cfg_GetBool(
      config_handle_, "settings.manual.mirror_check.enabled", enabled_);
  look_angle_deg_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.manual.mirror_check.look_angle_deg",
      look_angle_deg_));
  pitch_offset_deg_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.manual.mirror_check.pitch_offset_deg",
      pitch_offset_deg_));
  smoothing_time_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.manual.mirror_check.smoothing_time",
      smoothing_time_));
  require_stationary_ = config_api_->Cfg_GetBool(
      config_handle_, "settings.manual.mirror_check.require_stationary",
      require_stationary_);
  ignore_after_moving_signal_ = config_api_->Cfg_GetBool(
      config_handle_, "settings.manual.mirror_check.ignore_after_moving_signal",
      ignore_after_moving_signal_);

  yaw_.SetTimeConstant(smoothing_time_);
  pitch_.SetTimeConstant(smoothing_time_);
}

void MirrorCheckEffect::Reset() {
  yaw_.Reset();
  pitch_.Reset();
  // prev_*blinker_ and *_moving_at_activation_ are deliberately kept: Reset
  // runs on every cabin re-entry, and clearing them would make a blinker
  // switched on while driving look like a fresh activation while stopped.
}

HeadOffset MirrorCheckEffect::Update(float dt, const SPF_TruckData &truck,
                                     const SPF_Controls & /*controls*/) {
  const bool is_moving = std::fabs(truck.speed) > kStationarySpeedThreshold;

  // Latch whether the truck was moving at the exact moment each blinker
  // turned on, so a signal started while driving keeps ignoring the
  // mirror check even if the truck subsequently stops (e.g. waiting in
  // traffic mid-lane-change) instead of being re-evaluated every frame.
  if (truck.lblinker && !prev_lblinker_)
    lblinker_moving_at_activation_ = is_moving;
  if (truck.rblinker && !prev_rblinker_)
    rblinker_moving_at_activation_ = is_moving;
  prev_lblinker_ = truck.lblinker;
  prev_rblinker_ = truck.rblinker;

  float target_yaw_deg = 0.0f;
  float target_pitch_deg = 0.0f;

  const bool lblinker_allowed =
      !require_stationary_ ||
      !(ignore_after_moving_signal_ ? lblinker_moving_at_activation_
                                    : is_moving);
  const bool rblinker_allowed =
      !require_stationary_ ||
      !(ignore_after_moving_signal_ ? rblinker_moving_at_activation_
                                    : is_moving);

  // Empirically, lblinker/rblinker map to the opposite yaw sign from the
  // naive assumption (confirmed in-game: right blinker was looking left).
  if (lblinker_allowed && truck.lblinker) {
    target_yaw_deg = look_angle_deg_;
    target_pitch_deg = pitch_offset_deg_;
  } else if (rblinker_allowed && truck.rblinker) {
    target_yaw_deg = -look_angle_deg_;
    target_pitch_deg = pitch_offset_deg_;
  }

  HeadOffset offset;
  offset.yaw = yaw_.Update(target_yaw_deg, dt);
  offset.pitch = pitch_.Update(target_pitch_deg, dt);
  return offset;
}

} // namespace motioncab
