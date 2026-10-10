#include "ManualZoomEffect.hpp"

#include "core/Keybinds.hpp"
#include "math/Units.hpp"

#include <algorithm>
#include <cmath>

namespace motioncab {

ManualZoomEffect::ManualZoomEffect(SPF_Config_API *config_api,
                                   SPF_Config_Handle *config_handle,
                                   SPF_KeyBinds_API *keybinds_api,
                                   SPF_KeyBinds_Handle *keybinds_handle,
                                   CameraRig *rig)
    : config_(config_api, config_handle, "manual", "manual_zoom"),
      keybinds_api_(keybinds_api), keybinds_handle_(keybinds_handle),
      rig_(rig) {}

void ManualZoomEffect::LoadConfig() {
  if (!config_.IsAvailable())
    return;

  const bool was_enabled = enabled_;
  enabled_ = config_.Bool("enabled", enabled_);
  if (!enabled_ && was_enabled) {
    RestoreFov(); // Update() won't run again to do it
    restore_pending_ = false;
    fov_overridden_ = false;
    RestoreDynamicFov();
  }
  if (enabled_ && !was_enabled) {
    // The FOV may have changed while disabled.
    has_synced_ = false;
  }
  zoom_fov_deg_ = config_.Float("zoom_fov_deg", zoom_fov_deg_);
  smoothing_time_ = config_.Float("smoothing_time", smoothing_time_);

  fov_spring_.SetTimeConstant(smoothing_time_);
}

void ManualZoomEffect::Reset() {
  RestoreFov();
  // With a restore pending, the live FOV is still our zoomed one, not
  // something to sync from.
  if (!restore_pending_)
    has_synced_ = false;
  RestoreDynamicFov();
}

// Without this, a Reset() while zoomed (pause, view switch, alt-tab) would
// sync from the zoomed FOV and keep it as the player's own.
void ManualZoomEffect::RestoreFov() {
  if (!fov_overridden_)
    return;
  if (has_synced_ && camera_api_)
    SetFov(base_fov_deg_);
  // The write can be ignored (the camera not resolved yet right after
  // re-entry), so Update() keeps at it until the live FOV matches.
  restore_pending_ = has_synced_;
  fov_overridden_ = restore_pending_;
}

// The game widens the FOV with speed (~4 degrees at speed with the default
// factor, measured in game). Cutting that factor to 1 at once would pop the
// FOV, so it's eased along with the zoom.
void ManualZoomEffect::UpdateDynamicFov(float dt, bool zoom_held) {
  if (!camera_api_ || !camera_api_->Cam_GetInteriorSpeedFovChangeFactor ||
      !camera_api_->Cam_SetInteriorSpeedFovChangeFactor)
    return;

  if (!dynamic_fov_suppressed_) {
    if (!zoom_held)
      return;
    float factor = 1.0f;
    // Already 1 means the player has dynamic FOV off.
    if (!camera_api_->Cam_GetInteriorSpeedFovChangeFactor(&factor) ||
        std::fabs(factor - 1.0f) <= 1e-3f)
      return;
    saved_speed_fov_factor_ = factor;
    dynamic_blend_ = 1.0f;
    dynamic_fov_suppressed_ = true;
  }

  // Matches the FOV spring's response so both motions overlap.
  const float duration = std::max(0.2f, smoothing_time_ * 1.5f);
  const float step = dt / duration;
  dynamic_blend_ =
      std::clamp(dynamic_blend_ + (zoom_held ? -step : step), 0.0f, 1.0f);
  const float eased = math::SmoothStep(dynamic_blend_);
  camera_api_->Cam_SetInteriorSpeedFovChangeFactor(
      1.0f + (saved_speed_fov_factor_ - 1.0f) * eased);
  if (!zoom_held && dynamic_blend_ >= 1.0f)
    dynamic_fov_suppressed_ = false;
}

void ManualZoomEffect::RestoreDynamicFov() {
  if (!dynamic_fov_suppressed_)
    return;
  if (camera_api_ && camera_api_->Cam_SetInteriorSpeedFovChangeFactor)
    camera_api_->Cam_SetInteriorSpeedFovChangeFactor(saved_speed_fov_factor_);
  dynamic_fov_suppressed_ = false;
  dynamic_blend_ = 1.0f;
}

bool ManualZoomEffect::GetFov(float *out_fov) {
  if (!camera_api_ || !camera_api_->Cam_GetInteriorFov(out_fov))
    return false;
  if (rig_)
    *out_fov -= rig_->applied().fov;
  return true;
}

void ManualZoomEffect::SetFov(float fov) {
  if (!camera_api_)
    return;
  const float live_fov = fov + (rig_ ? rig_->applied().fov : 0.0f);
  camera_api_->Cam_SetInteriorFov(live_fov);
  if (rig_)
    rig_->NoteFovWrite(live_fov);
}

void ManualZoomEffect::Update(float dt, SPF_Camera_API *camera_api) {
  if (!camera_api || !keybinds_api_ || !keybinds_handle_)
    return;
  camera_api_ = camera_api;

  if (restore_pending_) {
    SetFov(base_fov_deg_);
    float live = 0.0f;
    if (!GetFov(&live) || std::fabs(live - base_fov_deg_) > 0.05f)
      return;
    restore_pending_ = false;
    fov_overridden_ = false;
    fov_spring_.Reset(base_fov_deg_);
    has_synced_ = true;
  }

  if (!has_synced_) {
    // The getter can fail for a frame or two after cabin entry: retry
    // rather than keep 0 as the player's FOV.
    if (GetFov(&base_fov_deg_)) {
      fov_spring_.Reset(base_fov_deg_);
      has_synced_ = true;
    }
  }

  const bool zoom_held =
      keybinds::IsHeld(keybinds_api_, keybinds_handle_, keybinds::kZoom);

  // Not the live FOV: on a quick repeat press the spring hasn't settled
  // yet, and each press would ratchet the FOV to return to inward.
  const float target = zoom_held ? zoom_fov_deg_ : base_fov_deg_;
  const float smoothed = fov_spring_.Update(target, dt);

  // Once settled, follow the live FOV instead of writing it, or we'd fight
  // any other change to it (the F4 slider).
  constexpr float kSettledEpsilonDeg = 0.05f;
  if (!zoom_held && std::fabs(smoothed - base_fov_deg_) < kSettledEpsilonDeg) {
    if (fov_overridden_) {
      // Land exactly on the player's FOV before letting go: syncing from
      // the last write, a little short of it, would shift it on every zoom.
      SetFov(base_fov_deg_);
      fov_spring_.Reset(base_fov_deg_);
    } else if (GetFov(&base_fov_deg_)) {
      fov_spring_.Reset(base_fov_deg_);
    }
    UpdateDynamicFov(dt, false);
    fov_overridden_ = false;
    return;
  }

  UpdateDynamicFov(dt, zoom_held);
  SetFov(smoothed);
  fov_overridden_ = true;
}

} // namespace motioncab
