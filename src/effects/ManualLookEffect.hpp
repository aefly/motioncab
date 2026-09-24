#pragma once

#include "Effect.hpp"
#include "math/SpringDamper.hpp"

#include "SPF_Config_API.h"
#include "SPF_KeyBinds_API.h"
#include "ui/SettingsDefaults.hpp"

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
class ManualLookEffect final : public Effect {
public:
  ManualLookEffect(SPF_Config_API *config_api, SPF_Config_Handle *config_handle,
                   SPF_KeyBinds_API *keybinds_api,
                   SPF_KeyBinds_Handle *keybinds_handle)
      : config_api_(config_api), config_handle_(config_handle),
        keybinds_api_(keybinds_api), keybinds_handle_(keybinds_handle) {}

  const char *Name() const override { return "manual_look"; }
  bool IsEnabled() const override { return enabled_; }
  void SetEnabled(bool enabled) override { enabled_ = enabled; }

  void LoadConfig() override;
  void Reset() override;
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;

private:
  SPF_Config_API *config_api_;
  SPF_Config_Handle *config_handle_;
  SPF_KeyBinds_API *keybinds_api_;
  SPF_KeyBinds_Handle *keybinds_handle_;

  bool enabled_ = true;
  float look_angle_deg_ =
      defaults::kManualLookLookAngle; // yaw when looking left/right, in
                                      // degrees
  float smoothing_time_ =
      defaults::kManualLookSmoothing; // seconds, spring time constant
  bool toggle_mode_ =
      defaults::kManualLookToggleMode; // false = press/hold, true = toggle

  math::SpringDamper1D yaw_;

  // Toggle-mode state: -1 = looking right, 0 = center, 1 = looking left.
  int toggle_direction_ = 0;
  bool prev_left_pressed_ = false;
  bool prev_right_pressed_ = false;
};

} // namespace motioncab
