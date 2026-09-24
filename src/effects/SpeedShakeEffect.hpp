#pragma once

#include "Effect.hpp"
#include "math/SpringDamper.hpp"

#include "SPF_Config_API.h"

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
class SpeedShakeEffect final : public Effect {
public:
  SpeedShakeEffect(SPF_Config_API *config_api,
                   SPF_Config_Handle *config_handle);

  const char *Name() const override { return "speed_shake"; }
  bool IsEnabled() const override { return enabled_; }
  void SetEnabled(bool enabled) override { enabled_ = enabled; }

  void LoadConfig() override;
  void Reset() override;
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;

private:
  static constexpr int kBands = 3;
  // Independent noise channels: x, y, yaw, roll (own part), pitch (own part).
  static constexpr int kChannels = 5;

  void ApplySmoothing();

  SPF_Config_API *config_api_;
  SPF_Config_Handle *config_handle_;

  bool enabled_ = true;
  float intensity_ = 1.0f;
  float smoothing_ = 0.30f; // seconds, slow-band low-pass time constant
  float rotation_ = 1.0f;   // multiplier on yaw/pitch/roll only
  float vertical_ = 1.0f;   // multiplier on the up/down bounce only
  float roughness_ = 0.5f;  // 0..1, depth of the slow amplitude envelope

  uint32_t seed_;
  std::array<float, kBands> phase_{}; // noise-domain time per band
  float envelope_phase_ = 0.0f;       // not reset: keeps the roughness varying
  std::array<std::array<math::SpringDamper1D, kChannels>, kBands> filter_;
};

} // namespace motioncab
