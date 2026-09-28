#pragma once

#include "core/SettingsSchema.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "effects/road/Surface.hpp"
#include "math/SpringDamper.hpp"

#include <random>

namespace motioncab {

// Adds continuous, textured chatter to the camera based on the ground
// material under the wheels (gravel, cobblestone, dirt vs. asphalt),
// texture rather than shape. Distinct from SuspensionEffect, which reacts
// to actual suspension travel instead.
//
// Off-road (dirt, grass, soft ground) it also rocks the head from side to
// side (roll), a slower motion than the chatter, since that ground is
// uneven and not just rough. Asphalt, even coarse, only chatters. See
// Surface.hpp for how the ground is classified.
class RoadIrregularityEffect final : public ConfigurableEffect {
public:
  RoadIrregularityEffect(SPF_Config_API *config_api,
                         SPF_Config_Handle *config_handle);

  void Reset() override;
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;
  void OnTruckConstantsChanged(const SPF_TruckConstants &constants) override;
  void OnCommonDataChanged(const SPF_CommonData &data) override;

private:
  void LoadSettings() override;

  // scales both the chatter and the rocking
  float intensity_ =
      settings::Default("settings.road.road_irregularity.intensity");
  // seconds, noise low-pass time constant
  float reactivity_ =
      settings::Default("settings.road.road_irregularity.reactivity");

  uint32_t wheel_count_ = 0;
  road::SurfaceMap surfaces_;

  std::minstd_rand rng_;
  math::SpringDamper1D noise_x_;
  math::SpringDamper1D noise_y_;

  uint32_t roll_seed_;
  float roll_phase_ = 0.0f; // noise-domain time, not reset
  // Fades the rocking in/out when the ground changes.
  math::SpringDamper1D unevenness_;
};

} // namespace motioncab
