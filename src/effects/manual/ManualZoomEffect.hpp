#pragma once

#include "SPF_Camera_API.h"
#include "SPF_KeyBinds_API.h"
#include "core/CameraRig.hpp"
#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "math/SpringDamper.hpp"

namespace motioncab {

// Hold to zoom in, release to ease back. It has its own key: reacting to the
// game's zoom key fought the game's own animation (doubled, or stuck
// mid-zoom). It drives the FOV itself rather than through a HeadOffset, so
// it isn't in the EffectManager.
//
// It works under the effects' FOV offset: otherwise a zoom started mid
// Blindspot Viewer peek would take the peek's FOV as the player's own and
// return to it.
class ManualZoomEffect {
public:
  ManualZoomEffect(SPF_Config_API *config_api, SPF_Config_Handle *config_handle,
                   SPF_KeyBinds_API *keybinds_api,
                   SPF_KeyBinds_Handle *keybinds_handle, CameraRig *rig);

  bool IsEnabled() const { return enabled_; }
  void SetEnabled(bool enabled) { enabled_ = enabled; }

  void LoadConfig();
  void Reset();
  void Update(float dt, SPF_Camera_API *camera_api);

  void RestoreDynamicFov();
  void RestoreFov();

private:
  EffectConfig config_;
  SPF_KeyBinds_API *keybinds_api_;
  SPF_KeyBinds_Handle *keybinds_handle_;
  CameraRig *rig_;

  bool enabled_ = true;
  float zoom_fov_deg_ =
      settings::Default("settings.manual.manual_zoom.zoom_fov_deg");
  float smoothing_time_ =
      settings::Default("settings.manual.manual_zoom.smoothing_time");

  math::SpringDamper1D fov_spring_;
  float base_fov_deg_ = 0.0f; // the player's own, to return to
  bool has_synced_ = false;
  bool restore_pending_ = false;
  bool fov_overridden_ = false; // the live FOV isn't the player's own

  void UpdateDynamicFov(float dt, bool zoom_held);
  bool dynamic_fov_suppressed_ = false;
  float saved_speed_fov_factor_ = 1.0f;  // the player's
  float dynamic_blend_ = 1.0f;           // 1 is the player's factor, 0 is 1
  SPF_Camera_API *camera_api_ = nullptr; // last seen, for restoring

  // Under the rig's applied FOV offset.
  bool GetFov(float *out_fov);
  void SetFov(float fov);
};

} // namespace motioncab
