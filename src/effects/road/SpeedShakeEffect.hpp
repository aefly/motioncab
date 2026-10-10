#pragma once

#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "effects/road/Surface.hpp"
#include "math/SpringDamper.hpp"

#include <array>
#include <cstdint>

namespace motioncab {

// The slow body sway of a driver who never sits quite still in a moving cab,
// built from noise. The head mostly moves rather than turns, since the eyes
// stay on the road, and it settles a moment, then shifts, rather than
// swaying steadily. It grows with speed and with rougher ground.
//
// SuspensionEffect has the real wheel travel and RoadIrregularityEffect the
// surface texture; corners belong to BodyDynamicsEffect and HeadMotionEffect.
class SpeedShakeEffect final : public ConfigurableEffect {
public:
  SpeedShakeEffect(SPF_Config_API *config_api,
                   SPF_Config_Handle *config_handle);

  void Reset() override;
  bool NeedsDriverSeat() const override { return true; }
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;
  void OnTruckConstantsChanged(const SPF_TruckConstants &constants) override;
  void OnCommonDataChanged(const SPF_CommonData &data) override;

private:
  void LoadSettings() override;

  static constexpr int kBands = 3;
  static constexpr int kChannels = 5;

  float intensity_ = settings::Default("settings.road.speed_shake.intensity");
  // Changes how fast the noise runs, not its size.
  float smoothing_ =
      settings::Default("settings.road.speed_shake.smoothing_time");
  float rate_multiplier_ = 1.0f;
  float rotation_ = settings::Default("settings.road.speed_shake.rotation");
  float vertical_ = settings::Default("settings.road.speed_shake.vertical");
  // How much some stretches of road get rougher than others.
  float roughness_ = settings::Default("settings.road.speed_shake.roughness");

  uint32_t wheel_count_ = 0;
  SurfaceMap surfaces_;

  uint32_t seed_;
  // Never reset, so each cabin entry picks up a new stretch of noise rather
  // than replaying the same one.
  std::array<double, kBands> phase_{};
  double envelope_phase_ = 0.0;
  double activity_phase_ = 0.0;
  // The phases don't restart at 0, so the shake fades in after a Reset().
  math::SpringDamper1D entry_fade_;
  math::SpringDamper1D surface_gain_;
};

} // namespace motioncab
