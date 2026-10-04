#pragma once

#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "math/SpringDamper.hpp"

namespace motioncab {

// Simulates inertial head sway and tilt: the driver's head lags behind the
// cabin's own motion under acceleration, braking and cornering, the way a
// real passenger's head would.
//
// No vertical (Y) sway: it double-pushed pos_y alongside SuspensionEffect's
// grade-follow on grade transitions, so that's left entirely to Suspension.
// Assumes SCS's local-space convention: X = right, Y = up, Z = backward.
class HeadMotionEffect final : public ConfigurableEffect {
public:
  HeadMotionEffect(SPF_Config_API *config_api, SPF_Config_Handle *config_handle)
      : ConfigurableEffect(config_api, config_handle, "driving",
                           "head_motion") {}

  void Reset() override;
  bool NeedsDriverSeat() const override { return true; }
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;

private:
  void LoadSettings() override;

  // translational response to linear acceleration
  float sway_strength_ =
      settings::Default("settings.driving.head_motion.sway_strength");
  // rotational response to angular velocity
  float tilt_strength_ =
      settings::Default("settings.driving.head_motion.tilt_strength");
  // seconds, spring time constant
  float smoothing_time_ =
      settings::Default("settings.driving.head_motion.smoothing_time");

  math::SpringDamper1D sway_x_;
  math::SpringDamper1D sway_z_;
  math::SpringDamper1D tilt_yaw_;
  math::SpringDamper1D tilt_pitch_;
};

} // namespace motioncab
