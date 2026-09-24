#include "EngineVibrationEffect.hpp"

#include "EngineStartStopEffect.hpp"

#include <cmath>
#include <numbers>

namespace motioncab {

namespace {
constexpr float kBaseAmplitude = 0.0003f; // meters; a fine buzz
constexpr float kLateralGainRatio =
    0.25f;                             // lateral component relative to vertical
constexpr float kHarmonicOrder = 1.0f; // vibration cycles per engine revolution
constexpr float kMinAmplitudeFraction = 0.1f;
} // namespace

void EngineVibrationEffect::LoadConfig() {
  if (!config_api_ || !config_handle_)
    return;

  enabled_ = config_api_->Cfg_GetBool(
      config_handle_, "settings.cabin.engine_vibration.enabled", enabled_);
  intensity_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.cabin.engine_vibration.intensity", intensity_));
}

void EngineVibrationEffect::Reset() { phase_ = 0.0f; }

void EngineVibrationEffect::OnTruckConstantsChanged(
    const SPF_TruckConstants &constants) {
  is_electric_ = constants.adblue_capacity <= 0.01f;
  if (constants.rpm_limit > 1.0f)
    rpm_limit_ = constants.rpm_limit;
}

HeadOffset EngineVibrationEffect::Update(float dt, const SPF_TruckData &truck,
                                         const SPF_Controls & /*controls*/) {
  // engine_enabled can lag the real start by 1+ second;
  // engine_rpm alone is the true signal, matching EngineStartStopEffect's
  // own threshold (shared constant, not a separately-tuned duplicate).
  if (truck.engine_rpm <= EngineStartStopEffect::kRpmStartThreshold)
    return {};

  const float rpm_fraction = truck.engine_rpm / rpm_limit_;
  const float amplitude_fraction =
      kMinAmplitudeFraction + (1.0f - kMinAmplitudeFraction) * rpm_fraction;
  const float amplitude = kBaseAmplitude * intensity_ * amplitude_fraction;

  const float frequency_hz = (truck.engine_rpm / 60.0f) * kHarmonicOrder;
  phase_ += 2.0f * std::numbers::pi_v<float> * frequency_hz * dt;
  // fmod instead of a single subtraction: a large dt spike (load hitch,
  // alt-tab, a Quick Job cancel reload) can advance phase_ by several full
  // turns in one frame, and subtracting only one turn would leave it
  // large enough for std::sin to lose precision for the next few frames.
  phase_ = std::fmod(phase_, 2.0f * std::numbers::pi_v<float>);

  HeadOffset offset;
  offset.pos_y = amplitude * std::sin(phase_);
  offset.pos_x = amplitude * kLateralGainRatio *
                 std::sin(phase_ + std::numbers::pi_v<float> / 2.0f);
  return offset;
}

} // namespace motioncab
