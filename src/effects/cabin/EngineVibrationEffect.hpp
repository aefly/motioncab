#pragma once

#include "core/SettingsSchema.hpp"
#include "effects/ConfigurableEffect.hpp"

namespace motioncab {

// Adds a continuous, fine buzz to the camera driven by engine RPM: both
// frequency and amplitude rise as the RPM climbs, like the firing pulses of
// a multi-cylinder engine. Distinct from IdleBreathingEffect's slow organic
// sway: this is a mechanical, high-frequency micro-shake.
//
// Disabled for electric trucks (see telemetry::IsElectric). Gated via
// IsEnabled() rather than mutating the user's enabled toggle, so
// switching back to diesel just works again.
class EngineVibrationEffect final : public ConfigurableEffect {
public:
  EngineVibrationEffect(SPF_Config_API *config_api,
                        SPF_Config_Handle *config_handle)
      : ConfigurableEffect(config_api, config_handle, "cabin",
                           "engine_vibration") {}

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
  float rpm_limit_ =
      2500.0f; // from SPF_TruckConstants; used to normalize amplitude

  float phase_ = 0.0f; // radians
};

} // namespace motioncab
