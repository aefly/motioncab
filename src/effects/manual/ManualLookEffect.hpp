#pragma once

#include "core/Keybinds.hpp"
#include "core/SettingsSchema.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "math/SpringDamper.hpp"

#include "SPF_KeyBinds_API.h"

namespace motioncab {

// Lets the player glance left or right on demand, spring-eased into place
// and back. Polls the raw key state every frame via Kbind_GetActionValue
// rather than reacting to the one-shot Kbind_Register callback, since
// press/hold mode needs to know the key is still down.
//
// `toggle_mode_` selects between two behaviors: press/hold (default, look
// while held, recenter on release) and toggle (press to look, press again
// or the other side to recenter/switch). Either way the spring handles the
// easing.
class ManualLookEffect final : public ConfigurableEffect {
public:
  ManualLookEffect(SPF_Config_API *config_api, SPF_Config_Handle *config_handle,
                   SPF_KeyBinds_API *keybinds_api,
                   SPF_KeyBinds_Handle *keybinds_handle)
      : ConfigurableEffect(config_api, config_handle, "manual", "manual_look"),
        keybinds_api_(keybinds_api), keybinds_handle_(keybinds_handle) {}

  void Reset() override;
  bool NeedsDriverSeat() const override { return true; }
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;

private:
  void LoadSettings() override;

  SPF_KeyBinds_API *keybinds_api_;
  SPF_KeyBinds_Handle *keybinds_handle_;

  // yaw when looking left/right, in degrees
  float look_angle_deg_ =
      settings::Default("settings.manual.manual_look.look_angle_deg");
  // seconds, spring time constant
  float smoothing_time_ =
      settings::Default("settings.manual.manual_look.smoothing_time");
  // false = press/hold, true = toggle
  bool toggle_mode_ =
      settings::DefaultBool("settings.manual.manual_look.toggle_mode");

  math::SpringDamper1D yaw_;

  // Toggle-mode state: -1 = looking right, 0 = center, 1 = looking left.
  int toggle_direction_ = 0;
  keybinds::PressEdge left_edge_;
  keybinds::PressEdge right_edge_;
};

} // namespace motioncab
