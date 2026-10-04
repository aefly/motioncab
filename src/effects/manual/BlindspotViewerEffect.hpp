#pragma once

#include "core/Keybinds.hpp"
#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "math/SpringDamper.hpp"

#include "SPF_KeyBinds_API.h"

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
  bool NeedsDriverSeat() const override { return true; }
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;

private:
  void LoadSettings() override;

  SPF_KeyBinds_API *keybinds_api_;
  SPF_KeyBinds_Handle *keybinds_handle_;

  // meters, cabin-local
  float pos_x_ = settings::Default("settings.manual.blindspot_viewer.pos_x");
  float pos_y_ = settings::Default("settings.manual.blindspot_viewer.pos_y");
  float pos_z_ = settings::Default("settings.manual.blindspot_viewer.pos_z");
  float yaw_deg_ =
      settings::Default("settings.manual.blindspot_viewer.yaw_deg");
  float pitch_deg_ =
      settings::Default("settings.manual.blindspot_viewer.pitch_deg");
  float roll_deg_ =
      settings::Default("settings.manual.blindspot_viewer.roll_deg");
  float fov_offset_deg_ =
      settings::Default("settings.manual.blindspot_viewer.fov_offset_deg");
  // seconds, motion duration
  float smoothing_time_ =
      settings::Default("settings.manual.blindspot_viewer.smoothing_time");
  // false = press/hold, true = toggle
  bool toggle_mode_ =
      settings::DefaultBool("settings.manual.blindspot_viewer.toggle_mode");

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
