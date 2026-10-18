#pragma once

#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "math/SpringDamper.hpp"

namespace motioncab {

// The engine's firing pulses as they reach the driver's head through the
// mounts, the cab and the seat. An inline six fires three times a turn (30 Hz
// at idle, 90 Hz at 1800 RPM), far beyond what a frame can show, so the
// motion is a fine jitter at a fixed share of the frame rate and the engine
// only sets how strong it is: a mount resonance near idle (the truck shakes
// most there and smooths out at cruise), plus a hum following the torque,
// unevenly from one engine cycle to the next.
//
// Off for an electric truck, through IsEnabled() rather than the player's
// toggle, so it comes back with a diesel.
class EngineVibrationEffect final : public ConfigurableEffect {
public:
  EngineVibrationEffect(SPF_Config_API *config_api,
                        SPF_Config_Handle *config_handle);

  bool IsEnabled() const override {
    return ConfigurableEffect::IsEnabled() && !is_electric_;
  }

  void Reset() override;
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;
  void OnTruckConstantsChanged(const SPF_TruckConstants &constants) override;

private:
  void LoadSettings() override;

  float intensity_ =
      settings::Default("settings.cabin.engine_vibration.intensity");

  bool is_electric_ = false;
  float rpm_limit_ = 2500.0f;

  double jitter_phase_ = 0.0;
  double roughness_phase_ = 0.0;
  math::SpringDamper1D load_;
  math::SpringDamper1D frame_; // seconds
};

} // namespace motioncab
