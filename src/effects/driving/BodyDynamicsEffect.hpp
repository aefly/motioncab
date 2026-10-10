#pragma once

#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "math/SpringDamper.hpp"

namespace motioncab {

// The whole body reacting to cornering and braking: the head leans toward
// the outside of a corner and nods forward under braking. HeadMotionEffect
// covers the head's own sway.
class BodyDynamicsEffect final : public ConfigurableEffect {
public:
  BodyDynamicsEffect(SPF_Config_API *config_api,
                     SPF_Config_Handle *config_handle)
      : ConfigurableEffect(config_api, config_handle, "driving",
                           "body_dynamics") {}

  void Reset() override;
  bool NeedsDriverSeat() const override { return true; }
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;

private:
  void LoadSettings() override;

  float lean_strength_ =
      settings::Default("settings.driving.body_dynamics.lean_strength");
  float nod_strength_ =
      settings::Default("settings.driving.body_dynamics.nod_strength");
  float smoothing_time_ =
      settings::Default("settings.driving.body_dynamics.smoothing_time");

  math::SpringDamper1D roll_;
  math::SpringDamper1D pitch_;
};

} // namespace motioncab
