#pragma once

#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "math/SpringDamper.hpp"

namespace motioncab {

// Looks toward the corresponding side mirror while a turn signal is on,
// recentering when it's off, a driver's blind-spot check before a turn.
// Every angle and the FOV change are set per side, since the passenger
// mirror is further away than the driver's.
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
  bool NeedsDriverSeat() const override { return true; }
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;

  // The offset as of the last Update(), in degrees, for ManualLookEffect
  // to take out of its looks.
  float yaw() const { return yaw_.value(); }
  float pitch() const { return pitch_.value(); }
  float fov() const { return fov_.value(); }

private:
  void LoadSettings() override;

  // yaw toward each mirror, in degrees
  float left_angle_deg_ =
      settings::Default("settings.manual.mirror_check.left_angle_deg");
  float right_angle_deg_ =
      settings::Default("settings.manual.mirror_check.right_angle_deg");
  // in degrees, positive up
  float left_pitch_deg_ =
      settings::Default("settings.manual.mirror_check.left_pitch_deg");
  float right_pitch_deg_ =
      settings::Default("settings.manual.mirror_check.right_pitch_deg");
  // FOV change, in degrees, negative zooms in
  float left_fov_deg_ =
      settings::Default("settings.manual.mirror_check.left_fov_deg");
  float right_fov_deg_ =
      settings::Default("settings.manual.mirror_check.right_fov_deg");
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
  math::SpringDamper1D fov_;
};

} // namespace motioncab
