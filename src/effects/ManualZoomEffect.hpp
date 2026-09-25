#pragma once

#include "SPF_Camera_API.h"
#include "SPF_Config_API.h"
#include "SPF_KeyBinds_API.h"
#include "math/SpringDamper.hpp"
#include "ui/SettingsDefaults.hpp"

namespace motioncab {

// Smooth manual zoom: hold to zoom in, release to ease back to the FOV
// from before the hold. Uses a dedicated keybind rather than the native
// zoom key, since layering a spring on Cam_GetInteriorFov/SetInteriorFov
// while also reacting to the native key fought its internal animation
// (doubled animation, camera stuck mid-zoom). Not a HeadOffset effect
// (drives FOV, not head pose), so it isn't part of EffectManager.
class ManualZoomEffect {
public:
  ManualZoomEffect(SPF_Config_API *config_api, SPF_Config_Handle *config_handle,
                   SPF_KeyBinds_API *keybinds_api,
                   SPF_KeyBinds_Handle *keybinds_handle);

  bool IsEnabled() const { return enabled_; }
  void SetEnabled(bool enabled) { enabled_ = enabled; }

  void LoadConfig();
  void Reset();
  void Update(float dt, SPF_Camera_API *camera_api);

  // Gives the game's speed-based dynamic FOV back if we switched it off for
  // a zoom. Also called on plugin unload so it can't stay disabled.
  void RestoreDynamicFov();

  // Gives the player's FOV back if we are still overriding it mid-zoom.
  void RestoreFov();

private:
  SPF_Config_API *config_api_;
  SPF_Config_Handle *config_handle_;
  SPF_KeyBinds_API *keybinds_api_;
  SPF_KeyBinds_Handle *keybinds_handle_;

  bool enabled_ = true;
  float zoom_fov_deg_ =
      defaults::kManualZoomZoomLevel; // FOV while the zoom key is held
  float smoothing_time_ =
      defaults::kManualZoomSmoothing; // seconds, spring time constant

  math::SpringDamper1D fov_spring_;
  float base_fov_deg_ = 0.0f; // FOV to return to when the key is released
  bool has_synced_ = false;
  bool restore_pending_ = false; // retrying to hand the FOV back
  bool fov_overridden_ = false;  // we wrote a FOV that isn't the player's own

  // While the truck moves, the game's dynamic (speed-based) FOV term is added
  // to ours and cutting/restoring it abruptly shows as a jerk. We ease its
  // speed factor down while zooming in and back up while zooming out.
  void UpdateDynamicFov(float dt, bool zoom_held);
  bool dynamic_fov_suppressed_ = false;  // we are currently managing the factor
  float saved_speed_fov_factor_ = 1.0f;  // player's value, restored after zoom
  float dynamic_blend_ = 1.0f;           // 1 = player's factor, 0 = factor 1
  SPF_Camera_API *camera_api_ = nullptr; // last seen, for restoring

  // Cam_GetInteriorFov/Cam_SetInteriorFov on camera_api_, null-checked.
  bool GetFov(float *out_fov);
  void SetFov(float fov);
};

} // namespace motioncab
