#pragma once

#include "effects/ConfigurableEffect.hpp"
#include "math/SpringDamper.hpp"

#include "ui/SettingsDefaults.hpp"

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

  float lean_strength_ = defaults::kBodyDynamicsLeanStrength;
  float nod_strength_ = defaults::kBodyDynamicsNodStrength;
  float smoothing_time_ =
      defaults::kBodyDynamicsSmoothing; // seconds, spring time constant

  math::SpringDamper1D roll_;
  math::SpringDamper1D pitch_;
};

} // namespace motioncab
