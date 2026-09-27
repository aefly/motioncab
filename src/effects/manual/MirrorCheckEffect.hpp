#pragma once

#include "core/SettingsSchema.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "math/SpringDamper.hpp"

namespace motioncab {

// Looks toward the corresponding side mirror while a turn signal is on,
// recentering when it's off, a driver's blind-spot check before a turn.
//
// Level-triggered, not one-shot: target is simply "mirror" while
// lblinker/rblinker is true, "center" otherwise; the spring does the
// easing, no hold/return state machine needed.
//
// require_stationary_ (default on) restricts triggering to standing still,
// since signaling while driving reads as distraction, not a deliberate check.
// With it on, ignore_after_moving_signal_ (default on) latches whether the
// truck was moving when the blinker turned on: if so, it keeps ignoring
// that blinker even if the truck later stops mid-signal, since it isn't
// re-evaluated continuously.
class MirrorCheckEffect final : public ConfigurableEffect {
public:
  MirrorCheckEffect(SPF_Config_API *config_api,
                    SPF_Config_Handle *config_handle)
      : ConfigurableEffect(config_api, config_handle, "manual",
                           "mirror_check") {}

  void Reset() override;
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;

private:
  void LoadSettings() override;

  // yaw toward the mirror, in degrees
  float look_angle_deg_ =
      settings::Default("settings.manual.mirror_check.look_angle_deg");
  // mirrors sit a little low
  float pitch_offset_deg_ =
      settings::Default("settings.manual.mirror_check.pitch_offset_deg");
  // seconds, spring time constant
  float smoothing_time_ =
      settings::Default("settings.manual.mirror_check.smoothing_time");
  // ignore blinkers while the truck is moving
  bool require_stationary_ =
      settings::DefaultBool("settings.manual.mirror_check.require_stationary");
  // see class comment
  bool ignore_after_moving_signal_ = settings::DefaultBool(
      "settings.manual.mirror_check.ignore_after_moving_signal");

  bool prev_lblinker_ = false;
  bool prev_rblinker_ = false;
  bool lblinker_moving_at_activation_ = false;
  bool rblinker_moving_at_activation_ = false;

  math::SpringDamper1D yaw_;
  math::SpringDamper1D pitch_;
};

} // namespace motioncab
