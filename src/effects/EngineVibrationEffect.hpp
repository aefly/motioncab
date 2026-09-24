#pragma once

#include "Effect.hpp"

#include "SPF_Config_API.h"

namespace motioncab {

// Adds a continuous, fine buzz to the camera driven by engine RPM: both
// frequency and amplitude rise as the RPM climbs, like the firing pulses of
// a multi-cylinder engine. Distinct from IdleBreathingEffect's slow organic
// sway: this is a mechanical, high-frequency micro-shake.
//
// Disabled for electric trucks via SPF_TruckConstants.adblue_capacity <= 0
// (no direct "is electric" flag; fuel_capacity doesn't work, since BEVs
// still report a nonzero value). Gated via IsEnabled() rather than
// mutating the user's enabled_ toggle, so switching back to diesel just
// works again.
class EngineVibrationEffect final : public Effect {
public:
  EngineVibrationEffect(SPF_Config_API *config_api,
                        SPF_Config_Handle *config_handle)
      : config_api_(config_api), config_handle_(config_handle) {}

  const char *Name() const override { return "engine_vibration"; }
  bool IsEnabled() const override { return enabled_ && !is_electric_; }
  void SetEnabled(bool enabled) override { enabled_ = enabled; }

  void LoadConfig() override;
  void Reset() override;
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;
  void OnTruckConstantsChanged(const SPF_TruckConstants &constants) override;

private:
  SPF_Config_API *config_api_;
  SPF_Config_Handle *config_handle_;

  bool enabled_ = true;
  float intensity_ = 1.0f;

  bool is_electric_ = false;
  float rpm_limit_ =
      2500.0f; // from SPF_TruckConstants; used to normalize amplitude

  float phase_ = 0.0f; // radians
};

} // namespace motioncab
