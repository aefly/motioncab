#pragma once

#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "math/DelayLine.hpp"
#include "math/SpringDamper.hpp"

namespace motioncab {

// The game's own steering camera rotation, eased instead of snapping (the
// game's option should stay off). SPF_TruckData.effective_steering never
// updates, so it reads SPF_Controls.
//
// The amount is set per side: in a left-hand drive cab the right mirror is
// farther away, so a right turn usually wants a wider pan. Lane corrections
// within the center zone turn at the narrower side's rate either way, so
// they don't look lopsided. The smoothing stays shared on purpose: changing
// the spring's time constant mid-turn snaps up the lag built so far, a
// sudden head jerk.
class SteeringCameraEffect final : public ConfigurableEffect {
public:
  SteeringCameraEffect(SPF_Config_API *config_api,
                       SPF_Config_Handle *config_handle)
      : ConfigurableEffect(config_api, config_handle, "driving",
                           "steering_camera") {}

  void Reset() override;
  bool NeedsDriverSeat() const override { return true; }
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;

private:
  void LoadSettings() override;

  // At full lock.
  float rotation_left_deg_ =
      settings::Default("settings.driving.steering_camera.rotation_left_deg");
  float rotation_right_deg_ =
      settings::Default("settings.driving.steering_camera.rotation_right_deg");
  // Percent of full lock over which the wider side eases in.
  float center_zone_pct_ =
      settings::Default("settings.driving.steering_camera.center_zone_pct");
  float smoothing_time_ =
      settings::Default("settings.driving.steering_camera.smoothing_time");
  // Before the camera starts moving at all, on top of the smoothing.
  float delay_seconds_ =
      settings::Default("settings.driving.steering_camera.delay_seconds");
  bool disable_in_reverse_ = settings::DefaultBool(
      "settings.driving.steering_camera.disable_in_reverse");
  bool disable_when_stopped_ = settings::DefaultBool(
      "settings.driving.steering_camera.disable_when_stopped");

  math::SpringDamper1D yaw_;
  bool needs_resync_ = false;
  // 0 in reverse, or stopped with disable_when_stopped.
  float follow_ = 1.0f;
  bool moving_forward_ = false;

  math::DelayLine steering_delay_;
};

} // namespace motioncab
