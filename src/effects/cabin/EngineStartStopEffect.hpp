#pragma once

#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"

namespace motioncab {

// Shakes the camera with a short, decaying mechanical shudder the moment
// the engine starts or stops, a one-shot jolt rather than
// EngineVibrationEffect's continuous RPM buzz.
//   - Start: triggered by engine_rpm crossing near-zero, not
//     engine_enabled, since the latter lags the real catch by 1+ second. Still
//     no telemetry for the cranking phase before that, so this is the
//     earliest signal available.
//   - Stop: engine_enabled flips false right as RPM decay begins, so it's
//     already the earliest usable signal there.
//
// Disabled for electric trucks (see telemetry::IsElectric). Gated via
// IsEnabled() rather than mutating the user's enabled toggle, so
// switching back to diesel just works again.
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
  // seconds, length of the start-up shudder
  float duration_ =
      settings::Default("settings.cabin.engine_start_stop.duration");

  bool is_electric_ = false;
  // Truck model id, to tell a genuine truck swap apart from other events
  // that also fire OnTruckConstantsChanged (trailer (dis)connect, etc.):
  // only a real swap should discard prev_rpm_/has_prev_state_.
  char prev_truck_id_[SPF_TELEMETRY_ID_MAX_SIZE] = {};
  bool has_prev_state_ = false;
  bool prev_engine_enabled_ = false;
  float prev_rpm_ = 0.0f;

  float shake_timer_ = 0.0f;
  float shake_total_duration_ = 0.0f;
  float shake_amplitude_scale_ = 0.0f; // 1.0 for start, smaller for stop
  float phase_ = 0.0f;
};

} // namespace motioncab
