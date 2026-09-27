#pragma once

#include "core/SettingsSchema.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "math/SpringDamper.hpp"

namespace motioncab {

// Tilts and rolls the head the way a driver's whole body reacts to
// cornering and braking, complementing HeadMotionEffect (which only
// translates the head and follows cabin angular velocity):
//  - Lean: in a corner the head tilts (roll) toward the outside, pushed by
//    lateral acceleration.
//  - Nod: braking pitches the head forward, accelerating pitches it back.
// Assumes SCS's local-space convention: X = right, Z = backward, so braking
// is positive Z acceleration.
class BodyDynamicsEffect final : public ConfigurableEffect {
public:
  BodyDynamicsEffect(SPF_Config_API *config_api,
                     SPF_Config_Handle *config_handle)
      : ConfigurableEffect(config_api, config_handle, "driving",
                           "body_dynamics") {}

  void Reset() override;
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;

private:
  void LoadSettings() override;

  float lean_strength_ =
      settings::Default("settings.driving.body_dynamics.lean_strength");
  float nod_strength_ =
      settings::Default("settings.driving.body_dynamics.nod_strength");
  // seconds, spring time constant
  float smoothing_time_ =
      settings::Default("settings.driving.body_dynamics.smoothing_time");

  math::SpringDamper1D roll_;
  math::SpringDamper1D pitch_;
};

} // namespace motioncab
