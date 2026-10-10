#include "IdleBreathingEffect.hpp"

#include "effects/Telemetry.hpp"
#include "math/Units.hpp"

#include <cmath>
#include <utility>

namespace motioncab {

namespace {
// The head nods slightly ahead of the chest's rise rather than in lockstep.
constexpr float kPitchLeadPhase = 0.35f;

float FadeFactor(float speed_kmh, float fade_start_kmh, float fade_end_kmh) {
  if (speed_kmh <= fade_start_kmh)
    return 1.0f;
  if (speed_kmh >= fade_end_kmh)
    return 0.0f;
  return 1.0f - (speed_kmh - fade_start_kmh) / (fade_end_kmh - fade_start_kmh);
}

// A quicker inhale than exhale, so it doesn't look like a metronome. Peaks
// around +-1.1.
float BreathWave(float phase) {
  return std::sin(phase) + 0.25f * std::sin(2.0f * phase - 1.2f);
}
} // namespace

IdleBreathingEffect::IdleBreathingEffect(SPF_Config_API *config_api,
                                         SPF_Config_Handle *config_handle)
    : ConfigurableEffect(config_api, config_handle, "cabin", "idle_breathing"),
      rng_(std::random_device{}()) {}

void IdleBreathingEffect::LoadSettings() {
  vertical_amplitude_ = Float("vertical_amplitude", vertical_amplitude_);
  pitch_amplitude_deg_ = Float("pitch_amplitude_deg", pitch_amplitude_deg_);
  breathing_rate_bpm_ = Float("breathing_rate_bpm", breathing_rate_bpm_);
  fade_start_kmh_ = Float("fade_start_kmh", fade_start_kmh_);
  fade_end_kmh_ = Float("fade_end_kmh", fade_end_kmh_);
  // Nothing stops the end slider from going below the start one.
  if (fade_end_kmh_ < fade_start_kmh_)
    std::swap(fade_start_kmh_, fade_end_kmh_);
}

void IdleBreathingEffect::Reset() {
  phase_ = 0.0f;
  cycle_rate_scale_ = 1.0f;
  cycle_amp_scale_ = 1.0f;
}

HeadOffset IdleBreathingEffect::Update(float dt, const SPF_TruckData &truck,
                                       const SPF_Controls & /*controls*/) {
  const float speed_kmh = telemetry::SpeedKmh(truck);
  const float fade = FadeFactor(speed_kmh, fade_start_kmh_, fade_end_kmh_);

  const float breaths_per_second =
      (breathing_rate_bpm_ / 60.0f) * cycle_rate_scale_;
  phase_ += math::kTwoPi * breaths_per_second * dt;
  if (phase_ >= math::kTwoPi) {
    phase_ = math::WrapPhase(phase_);
    // So no two breaths are the same.
    std::uniform_real_distribution<float> rate_jitter(0.92f, 1.08f);
    std::uniform_real_distribution<float> amp_jitter(0.85f, 1.15f);
    cycle_rate_scale_ = rate_jitter(rng_);
    cycle_amp_scale_ = amp_jitter(rng_);
  }

  const float amp = fade * cycle_amp_scale_;

  HeadOffset offset;
  offset.pos_y = vertical_amplitude_ * BreathWave(phase_) * amp;
  offset.pitch =
      pitch_amplitude_deg_ * BreathWave(phase_ + kPitchLeadPhase) * amp;
  return offset;
}

} // namespace motioncab
