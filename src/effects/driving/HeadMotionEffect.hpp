#pragma once

#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "math/SpringDamper.hpp"

namespace motioncab {

// The head lagging behind the cabin's motion through inertia.
//
// No vertical sway: on a change of grade it added up with SuspensionEffect's,
// which now has that to itself.
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

  float sway_strength_ =
      settings::Default("settings.driving.head_motion.sway_strength");
  float tilt_strength_ =
      settings::Default("settings.driving.head_motion.tilt_strength");
  float smoothing_time_ =
      settings::Default("settings.driving.head_motion.smoothing_time");

  math::SpringDamper1D sway_x_;
  math::SpringDamper1D sway_z_;
  math::SpringDamper1D tilt_yaw_;
  math::SpringDamper1D tilt_pitch_;
};

} // namespace motioncab
