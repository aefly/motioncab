#pragma once

#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"

#include <random>

namespace motioncab {

// A pitch nod with a smaller rise and fall under it, since a pure up/down
// bob looks mechanical. It fades out with speed, where it would only add
// noise to the road's motion.
class IdleBreathingEffect final : public ConfigurableEffect {
public:
  IdleBreathingEffect(SPF_Config_API *config_api,
                      SPF_Config_Handle *config_handle);

  void Reset() override;
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;

private:
  void LoadSettings() override;

  // meters
  float vertical_amplitude_ =
      settings::Default("settings.cabin.idle_breathing.vertical_amplitude");
  float pitch_amplitude_deg_ =
      settings::Default("settings.cabin.idle_breathing.pitch_amplitude_deg");
  float breathing_rate_bpm_ =
      settings::Default("settings.cabin.idle_breathing.breathing_rate_bpm");
  float fade_start_kmh_ =
      settings::Default("settings.cabin.idle_breathing.fade_start_kmh");
  float fade_end_kmh_ =
      settings::Default("settings.cabin.idle_breathing.fade_end_kmh");

  float phase_ = 0.0f; // radians
  // Redrawn every breath.
  float cycle_rate_scale_ = 1.0f;
  float cycle_amp_scale_ = 1.0f;
  std::minstd_rand rng_;
};

} // namespace motioncab
