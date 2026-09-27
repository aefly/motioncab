#include "EngineVibrationEffect.hpp"

#include "effects/TelemetryUtil.hpp"
#include "math/Units.hpp"

#include <cmath>

namespace motioncab {

namespace {
constexpr float kBaseAmplitude = 0.0003f; // meters; a fine buzz
constexpr float kLateralGainRatio =
    0.25f;                             // lateral component relative to vertical
constexpr float kHarmonicOrder = 1.0f; // vibration cycles per engine revolution
constexpr float kMinAmplitudeFraction = 0.1f;
} // namespace

void EngineVibrationEffect::LoadSettings() {
  intensity_ = Float("intensity", intensity_);
}

void EngineVibrationEffect::Reset() { phase_ = 0.0f; }

void EngineVibrationEffect::OnTruckConstantsChanged(
    const SPF_TruckConstants &constants) {
  is_electric_ = telemetry::IsElectric(constants);
  if (constants.rpm_limit > 1.0f)
    rpm_limit_ = constants.rpm_limit;
}

HeadOffset EngineVibrationEffect::Update(float dt, const SPF_TruckData &truck,
                                         const SPF_Controls & /*controls*/) {
  // Same start signal as EngineStartStopEffect (see kRpmStartThreshold).
  if (truck.engine_rpm <= telemetry::kRpmStartThreshold)
    return {};

  const float rpm_fraction = truck.engine_rpm / rpm_limit_;
  const float amplitude_fraction =
      kMinAmplitudeFraction + (1.0f - kMinAmplitudeFraction) * rpm_fraction;
  const float amplitude = kBaseAmplitude * intensity_ * amplitude_fraction;

  const float frequency_hz = (truck.engine_rpm / 60.0f) * kHarmonicOrder;
  phase_ = math::WrapPhase(phase_ + math::kTwoPi * frequency_hz * dt);

  HeadOffset offset;
  offset.pos_y = amplitude * std::sin(phase_);
  offset.pos_x =
      amplitude * kLateralGainRatio * std::sin(phase_ + math::kPi / 2.0f);
  return offset;
}

} // namespace motioncab
