#pragma once

#include "Effect.hpp"
#include "math/SpringDamper.hpp"

#include "SPF_Config_API.h"
#include "ui/SettingsDefaults.hpp"

namespace motioncab {

// Simulates inertial head sway and tilt: the driver's head lags behind the
// cabin's own motion under acceleration, braking and cornering, the way a
// real passenger's head would.
//
// No vertical (Y) sway: it double-pushed pos_y alongside SuspensionEffect's
// grade-follow on grade transitions, so that's left entirely to Suspension.
// Assumes SCS's local-space convention: X = right, Y = up, Z = backward.
class HeadMotionEffect final : public Effect {
public:
  HeadMotionEffect(SPF_Config_API *config_api, SPF_Config_Handle *config_handle)
      : config_api_(config_api), config_handle_(config_handle) {}

  const char *Name() const override { return "head_motion"; }
  bool IsEnabled() const override { return enabled_; }
  void SetEnabled(bool enabled) override { enabled_ = enabled; }

  void LoadConfig() override;
  void Reset() override;
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;

private:
  SPF_Config_API *config_api_;
  SPF_Config_Handle *config_handle_;

  bool enabled_ = true;
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
