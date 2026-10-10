#pragma once

#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"

namespace motioncab {

// A fine buzz whose frequency and size climb with the RPM.
//
// Off for an electric truck, through IsEnabled() rather than the player's
// toggle, so it comes back with a diesel.
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
  float rpm_limit_ = 2500.0f;

  float phase_ = 0.0f; // radians
};

} // namespace motioncab
