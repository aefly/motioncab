#pragma once

#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "effects/road/Surface.hpp"
#include "math/SpringDamper.hpp"

#include <random>

namespace motioncab {

// The ground's texture rather than its shape (SuspensionEffect has that): a
// chatter on rough surfaces like gravel. Uneven ground (dirt, grass) also
// rocks the head from side to side, more slowly. Asphalt, even coarse, only
// chatters.
class RoadIrregularityEffect final : public ConfigurableEffect {
public:
  RoadIrregularityEffect(SPF_Config_API *config_api,
                         SPF_Config_Handle *config_handle);

  void Reset() override;
  bool NeedsDriverSeat() const override { return true; }
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;
  void OnTruckConstantsChanged(const SPF_TruckConstants &constants) override;
  void OnCommonDataChanged(const SPF_CommonData &data) override;

private:
  void LoadSettings() override;

  float intensity_ =
      settings::Default("settings.road.road_irregularity.intensity");
  float reactivity_ =
      settings::Default("settings.road.road_irregularity.reactivity");

  uint32_t wheel_count_ = 0;
  SurfaceMap surfaces_;

  std::minstd_rand rng_;
  math::SpringDamper1D noise_x_;
  math::SpringDamper1D noise_y_;

  uint32_t roll_seed_;
  // The detail octave keeps its own phase: a multiple of a wrapped phase
  // would jump at each wrap.
  double roll_phase_ = 0.0;
  double roll_detail_phase_ = 0.0;
  math::SpringDamper1D unevenness_;
};

} // namespace motioncab
