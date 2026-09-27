#pragma once

#include "core/SettingsSchema.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "math/SpringDamper.hpp"

#include <array>
#include <cstdint>

namespace motioncab {

// Speed-driven body sway: the driver never sits perfectly still in a moving
// cab. Procedural, built from continuous noise (math/Noise.hpp):
//  - three frequency bands (slow sway, cab/seat bounce, fine vibration) with
//    very different weights per axis: vertical is dominated by the faster
//    bounce, lateral by the slow sway;
//  - roll follows lateral motion and pitch follows vertical motion, plus a
//    little independent noise, so the axes move together like a real body;
//  - a very slow amplitude envelope makes some stretches rougher than others.
// Both amplitude and noise speed grow with truck speed.
// Independent of SuspensionEffect (real wheel travel) and
// RoadIrregularityEffect (surface material): this one only needs speed.
class SpeedShakeEffect final : public ConfigurableEffect {
public:
  SpeedShakeEffect(SPF_Config_API *config_api,
                   SPF_Config_Handle *config_handle);

  void Reset() override;
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;

private:
  void LoadSettings() override;

  static constexpr int kBands = 3;
  // Independent noise channels: x, y, yaw, roll (own part), pitch (own part).
  static constexpr int kChannels = 5;

  void ApplySmoothing();

  float intensity_ = settings::Default("settings.road.speed_shake.intensity");
  // seconds, slow-band low-pass time constant
  float smoothing_ =
      settings::Default("settings.road.speed_shake.smoothing_time");
  // multiplier on yaw/pitch/roll only
  float rotation_ = settings::Default("settings.road.speed_shake.rotation");
  // multiplier on the up/down bounce only
  float vertical_ = settings::Default("settings.road.speed_shake.vertical");
  // 0..1, depth of the slow amplitude envelope
  float roughness_ = settings::Default("settings.road.speed_shake.roughness");

  uint32_t seed_;
  std::array<float, kBands> phase_{}; // noise-domain time per band
  float envelope_phase_ = 0.0f;       // not reset: keeps the roughness varying
  std::array<std::array<math::SpringDamper1D, kChannels>, kBands> filter_;
};

} // namespace motioncab
