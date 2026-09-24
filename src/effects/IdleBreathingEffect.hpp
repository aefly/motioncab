#pragma once

#include "Effect.hpp"

#include "SPF_Config_API.h"
#include "ui/SettingsDefaults.hpp"

#include <random>

namespace motioncab {

// Gives the camera a subtle, rhythmic rise and fall at idle, as if the
// driver is breathing: a pitch nod leads, with a smaller phase-lagged
// vertical translation underneath (a pure up/down bob reads as mechanical).
// The waveform is asymmetric (quicker inhale, slower exhale) and jitters
// cycle-to-cycle so it doesn't repeat. Fades out with speed, since it reads
// as noise once road/handling motion dominates.
class IdleBreathingEffect final : public Effect {
public:
  IdleBreathingEffect(SPF_Config_API *config_api,
                      SPF_Config_Handle *config_handle);

  const char *Name() const override { return "idle_breathing"; }
  bool IsEnabled() const override { return enabled_; }
  void SetEnabled(bool enabled) override { enabled_ = enabled; }

  void LoadConfig() override;
  void Reset() override;
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;

private:
  SPF_Config_API *config_api_;
  SPF_Config_Handle *config_handle_;

  bool enabled_ = true;
  float vertical_amplitude_ = defaults::kIdleBreathingVerticalAmount; // meters
  float pitch_amplitude_deg_ = defaults::kIdleBreathingHeadNodAmount; // degrees
  float breathing_rate_bpm_ =
      defaults::kIdleBreathingRate; // breaths per minute
  float fade_start_kmh_ =
      defaults::kIdleBreathingFadeStart; // full strength at/below this speed
  float fade_end_kmh_ =
      defaults::kIdleBreathingFadeEnd; // fully faded out at/above this speed

  float phase_ = 0.0f;            // radians
  float cycle_rate_scale_ = 1.0f; // randomized once per breath cycle
  float cycle_amp_scale_ = 1.0f;  // randomized once per breath cycle
  std::minstd_rand rng_;
};

} // namespace motioncab
