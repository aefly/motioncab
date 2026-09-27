#pragma once

#include "core/Keybinds.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "math/SpringDamper.hpp"

#include "SPF_KeyBinds_API.h"
#include "ui/SettingsDefaults.hpp"

namespace motioncab {

// Based on SPF_FrontalBlindspotViewer by Track'n'Truck Devs
// (https://github.com/TrackAndTruckDevs/SPF_FrontalBlindspotViewer),
//
// Leans the driver forward to see past the the windshield pillar.
//
// One motion moves every axis together, leaning in and sitting back alike
// (staging them, e.g. lean then look up, read as robotic in-game):
// - It starts slowly and stops softly like a real body (two critically
//   damped springs in series give a bell-shaped speed profile without any
//   overshoot), and can reverse mid-way smoothly.
// - While leaning, the body is off the backrest: it sways a little on its
//   own and gets pushed around more by braking and acceleration.
class BlindspotViewerEffect final : public ConfigurableEffect {
public:
  BlindspotViewerEffect(SPF_Config_API *config_api,
                        SPF_Config_Handle *config_handle,
                        SPF_KeyBinds_API *keybinds_api,
                        SPF_KeyBinds_Handle *keybinds_handle)
      : ConfigurableEffect(config_api, config_handle, "manual",
                           "blindspot_viewer"),
        keybinds_api_(keybinds_api), keybinds_handle_(keybinds_handle) {}

  void Reset() override;
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;

private:
  void LoadSettings() override;

  SPF_KeyBinds_API *keybinds_api_;
  SPF_KeyBinds_Handle *keybinds_handle_;

  float pos_x_ = defaults::kBlindspotViewerPosX; // meters, cabin-local
  float pos_y_ = defaults::kBlindspotViewerPosY;
  float pos_z_ = defaults::kBlindspotViewerPosZ;
  float yaw_deg_ = defaults::kBlindspotViewerYaw;
  float pitch_deg_ = defaults::kBlindspotViewerPitch;
  float roll_deg_ = defaults::kBlindspotViewerRoll;
  float fov_offset_deg_ = defaults::kBlindspotViewerFovOffset;
  float smoothing_time_ =
      defaults::kBlindspotViewerSmoothing; // seconds, motion duration
  bool toggle_mode_ =
      defaults::kBlindspotViewerToggleMode; // false = press/hold, true = toggle

  bool peeking_ = false; // toggle-mode target
  keybinds::PressEdge press_edge_;

  // 0 = seated, 1 = fully peeking.
  math::SpringDamper1D intent_; // first stage, softens the start
  math::SpringDamper1D body_;
  math::SpringDamper1D inertia_x_; // extra sway while off the backrest
  math::SpringDamper1D inertia_z_;
  float sway_time_ = 0.0f; // drives the postural sway noise
};

} // namespace motioncab
