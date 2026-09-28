#pragma once

#include "core/SettingsSchema.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "effects/road/Surface.hpp"
#include "math/SpringDamper.hpp"

#include <array>
#include <cstdint>

namespace motioncab {

// Speed-driven body sway: the driver never sits perfectly still in a moving
// cab. Procedural, built from continuous noise (math/Noise.hpp):
//  - three frequency bands (slow sway, cab/seat bounce, fine vibration) with
//    very different weights per axis: vertical is dominated by the faster
//    bounce, lateral by the slow sway;
//  - the head mostly moves rather than turns, the eyes staying on the road:
//    little yaw, roll following lateral motion, and pitch nodding against
//    the vertical bounce, plus a little independent noise;
//  - a very slow amplitude envelope makes some stretches rougher than others.
// Both amplitude and noise speed grow with truck speed. The amplitude also
// follows the ground: calmer on smooth asphalt, rougher on gravel or dirt
// (see Surface.hpp). Corners are left to BodyDynamicsEffect and
// HeadMotionEffect.
// Independent of SuspensionEffect (real wheel travel) and
// RoadIrregularityEffect (surface texture): this one is the slow body sway.
class SpeedShakeEffect final : public ConfigurableEffect {
public:
  SpeedShakeEffect(SPF_Config_API *config_api,
                   SPF_Config_Handle *config_handle);

  void Reset() override;
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;
  void OnTruckConstantsChanged(const SPF_TruckConstants &constants) override;
  void OnCommonDataChanged(const SPF_CommonData &data) override;

private:
  void LoadSettings() override;

  static constexpr int kBands = 3;
  // Independent noise channels: x, y, yaw, roll (own part), pitch (own part).
  static constexpr int kChannels = 5;

  float intensity_ = settings::Default("settings.road.speed_shake.intensity");
  // seconds; slows the noise down (higher) or speeds it up (lower), without
  // changing its size
  float smoothing_ =
      settings::Default("settings.road.speed_shake.smoothing_time");
  float rate_multiplier_ = 1.0f; // from smoothing_ in LoadSettings()
  // multiplier on yaw/pitch/roll only
  float rotation_ = settings::Default("settings.road.speed_shake.rotation");
  // multiplier on the up/down bounce only
  float vertical_ = settings::Default("settings.road.speed_shake.vertical");
  // 0..1, depth of the slow amplitude envelope
  float roughness_ = settings::Default("settings.road.speed_shake.roughness");

  uint32_t wheel_count_ = 0;
  road::SurfaceMap surfaces_;

  uint32_t seed_;
  // Noise-domain time per band, not reset either, so each cabin entry picks
  // up a new stretch of noise instead of replaying the same one.
  std::array<float, kBands> phase_{};
  float envelope_phase_ = 0.0f;       // not reset: keeps the roughness varying
  math::SpringDamper1D entry_fade_;   // 0 -> 1 after Reset(), no pop
  math::SpringDamper1D surface_gain_; // amplitude factor from the ground
};

} // namespace motioncab
