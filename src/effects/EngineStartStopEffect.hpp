#pragma once

#include "Effect.hpp"

#include "SPF_Config_API.h"
#include "ui/SettingsDefaults.hpp"

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
// Disabled for electric trucks via SPF_TruckConstants.adblue_capacity <= 0
// (no direct "is electric" flag; fuel_capacity doesn't work, since BEVs
// still report a nonzero value). Gated via IsEnabled() rather than
// mutating the user's enabled_ toggle, so switching back to diesel just
// works again.
class EngineStartStopEffect final : public Effect {
public:
  EngineStartStopEffect(SPF_Config_API *config_api,
                        SPF_Config_Handle *config_handle)
      : config_api_(config_api), config_handle_(config_handle) {}

  const char *Name() const override { return "engine_start_stop"; }
  bool IsEnabled() const override { return enabled_ && !is_electric_; }
  void SetEnabled(bool enabled) override { enabled_ = enabled; }

  // engine_rpm crossing this (near-zero) is the "engine has started" signal
  // shared with EngineVibrationEffect. engine_enabled lags the real catch
  // by 1+ second, which read as buzz/shudder firing well after the fact.
  static constexpr float kRpmStartThreshold = 5.0f;

  void LoadConfig() override;
  void Reset() override;
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;
  void OnTruckConstantsChanged(const SPF_TruckConstants &constants) override;

private:
  SPF_Config_API *config_api_;
  SPF_Config_Handle *config_handle_;

  bool enabled_ = true;
  float intensity_ = defaults::kEngineStartStopIntensity;
  float duration_ = defaults::kEngineStartStopDuration; // seconds, length of
                                                        // the start-up shudder

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
