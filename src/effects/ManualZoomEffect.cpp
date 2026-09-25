#include "ManualZoomEffect.hpp"

#include <algorithm>
#include <cmath>

namespace motioncab {

ManualZoomEffect::ManualZoomEffect(SPF_Config_API *config_api,
                                   SPF_Config_Handle *config_handle,
                                   SPF_KeyBinds_API *keybinds_api,
                                   SPF_KeyBinds_Handle *keybinds_handle)
    : config_api_(config_api), config_handle_(config_handle),
      keybinds_api_(keybinds_api), keybinds_handle_(keybinds_handle) {}

void ManualZoomEffect::LoadConfig() {
  if (!config_api_ || !config_handle_)
    return;

  const bool was_enabled = enabled_;
  enabled_ = config_api_->Cfg_GetBool(
      config_handle_, "settings.manual.manual_zoom.enabled", enabled_);
  if (!enabled_ && was_enabled) {
    RestoreFov(); // Update() won't run again to do it
    restore_pending_ = false;
    fov_overridden_ = false;
    RestoreDynamicFov();
  }
  if (enabled_ && !was_enabled) {
    // Update() doesn't run at all while disabled, so base_fov_deg_ can go
    // stale if the FOV changes some other way in the meantime. Force a
    // resync on the next Update() rather than only on cabin re-entry.
    has_synced_ = false;
  }
  zoom_fov_deg_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.manual.manual_zoom.zoom_fov_deg",
      zoom_fov_deg_));
  smoothing_time_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.manual.manual_zoom.smoothing_time",
      smoothing_time_));

  fov_spring_.SetTimeConstant(smoothing_time_);
}

void ManualZoomEffect::Reset() {
  RestoreFov();
  // While a restore is still pending the sync must keep the player's base
  // FOV: the live value is still our zoomed one.
  if (!restore_pending_)
    has_synced_ = false;
  RestoreDynamicFov();
}

// Puts the player's own FOV back if we are still overriding it. Without
// this, a Reset() while zoomed (pause, camera switch, alt-tab) would
// re-sync from the live, zoomed FOV and bake it in as the new base.
void ManualZoomEffect::RestoreFov() {
  if (!fov_overridden_)
    return;
  if (has_synced_ && camera_api_)
    SetFov(base_fov_deg_);
  // The write can be ignored (e.g. interior camera not resolved yet right
  // after re-entry), so Update() keeps retrying until the live FOV matches.
  restore_pending_ = has_synced_;
  fov_overridden_ = restore_pending_;
}

// The game's real FOV is ours plus a speed-dependent term scaled by the
// speed-FOV factor (measured in-game: ~4 deg extra at speed with the
// default factor 1.2). Cutting that factor to 1 instantly would pop the
// term out in a single frame, so it's eased in step with the FOV spring
// instead: down while zooming in, back up while zooming out.
void ManualZoomEffect::UpdateDynamicFov(float dt, bool zoom_held) {
  if (!camera_api_ || !camera_api_->Cam_GetInteriorSpeedFovChangeFactor ||
      !camera_api_->Cam_SetInteriorSpeedFovChangeFactor)
    return;

  if (!dynamic_fov_suppressed_) {
    if (!zoom_held)
      return;
    float factor = 1.0f;
    // Nothing to ease if it's already 1 (dynamic FOV off).
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
  const float t = dynamic_blend_;
  const float eased = t * t * (3.0f - 2.0f * t); // smoothstep
  camera_api_->Cam_SetInteriorSpeedFovChangeFactor(
      1.0f + (saved_speed_fov_factor_ - 1.0f) * eased);
  if (!zoom_held && dynamic_blend_ >= 1.0f)
    dynamic_fov_suppressed_ = false; // fully handed back to the game
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
  return camera_api_ && camera_api_->Cam_GetInteriorFov(out_fov);
}

void ManualZoomEffect::SetFov(float fov) {
  if (camera_api_)
    camera_api_->Cam_SetInteriorFov(fov);
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
    // Start from whatever FOV the player already has (native default or a
    // prior zoom level) instead of snapping to a fixed value. The getter
    // can fail for a frame or two right after cabin entry, so only latch
    // has_synced_ on success; a failed frame just retries instead of
    // locking in the 0.0f default.
    if (GetFov(&base_fov_deg_)) {
      fov_spring_.Reset(base_fov_deg_);
      has_synced_ = true;
    }
  }

  const bool zoom_held = keybinds_api_->Kbind_GetActionValue(
                             keybinds_handle_, "ManualZoom.zoom") > 0.5f;

  // base_fov_deg_ is kept in sync with the native FOV below whenever not
  // zoomed, so it's already correct by press time. Reading the live FOV
  // here instead would grab a not-yet-settled spring value on a repeat
  // press, ratcheting the "return to" FOV inward with each spam.

  const float target = zoom_held ? zoom_fov_deg_ : base_fov_deg_;
  const float smoothed = fov_spring_.Update(target, dt);

  // Once idle and settled, stop writing FOV and track the live value
  // instead. Otherwise this keeps re-asserting the stale base_fov_deg_,
  // fighting any FOV change made elsewhere (e.g. the game's own F4 slider).
  constexpr float kSettledEpsilonDeg = 0.05f;
  if (!zoom_held && std::fabs(smoothed - base_fov_deg_) < kSettledEpsilonDeg) {
    if (GetFov(&base_fov_deg_))
      fov_spring_.Reset(base_fov_deg_);
    UpdateDynamicFov(dt, false);
    fov_overridden_ = false;
    return;
  }

  UpdateDynamicFov(dt, zoom_held);
  SetFov(smoothed);
  fov_overridden_ = true;
}

} // namespace motioncab
