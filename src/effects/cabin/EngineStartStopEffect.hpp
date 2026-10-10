#pragma once

#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"

namespace motioncab {

// A short, decaying shudder as the engine starts or stops. The start goes by
// the RPM (see kRpmStartThreshold), the earliest signal there is, since
// nothing tells when the engine cranks. For the stop, engine_enabled turns
// off right as the RPM starts dropping.
//
// Off for an electric truck, through IsEnabled() rather than the player's
// toggle, so it comes back with a diesel.
class EngineStartStopEffect final : public ConfigurableEffect {
public:
  EngineStartStopEffect(SPF_Config_API *config_api,
                        SPF_Config_Handle *config_handle)
      : ConfigurableEffect(config_api, config_handle, "cabin",
                           "engine_start_stop") {}

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
      settings::Default("settings.cabin.engine_start_stop.intensity");
  float duration_ =
      settings::Default("settings.cabin.engine_start_stop.duration");

  bool is_electric_ = false;
  char prev_truck_id_[SPF_TELEMETRY_ID_MAX_SIZE] = {};
  bool has_prev_state_ = false;
  bool prev_engine_enabled_ = false;
  float prev_rpm_ = 0.0f;

  float shake_timer_ = 0.0f;
  float shake_total_duration_ = 0.0f;
  float shake_amplitude_scale_ = 0.0f;
  float phase_ = 0.0f;
};

} // namespace motioncab
