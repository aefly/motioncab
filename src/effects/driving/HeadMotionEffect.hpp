#pragma once

#include "effects/ConfigurableEffect.hpp"
#include "math/SpringDamper.hpp"

#include "ui/SettingsDefaults.hpp"

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
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;

private:
  void LoadSettings() override;

  float sway_strength_ =
      defaults::kHeadMotionSwayStrength; // translational response to
                                         // linear acceleration
  float tilt_strength_ =
      defaults::kHeadMotionTiltStrength; // rotational response to
                                         // angular velocity
  float smoothing_time_ =
      defaults::kHeadMotionSmoothing; // seconds, spring time constant

  math::SpringDamper1D sway_x_;
  math::SpringDamper1D sway_z_;
  math::SpringDamper1D tilt_yaw_;
  math::SpringDamper1D tilt_pitch_;
};

} // namespace motioncab
