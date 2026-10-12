#include "EngineVibrationEffect.hpp"

#include "effects/Telemetry.hpp"
#include "math/Noise.hpp"
#include "math/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace motioncab {

namespace {
// Roll leads: the crankshaft's torque rocks the cabin about its length.
constexpr float kThrobRollDeg = 0.01f;
constexpr float kThrobPitchDeg = 0.004f;
constexpr float kThrobY = 0.00004f; // meters
// So the head doesn't trace a clean ellipse.
constexpr float kThrobPitchLag = 0.9f; // radians
constexpr float kThrobYLag = 2.1f;
constexpr float kThrobUnevenness = 0.3f;
constexpr double kUnevennessRate = 0.6; // noise cells per engine cycle

constexpr float kIdleFadeStartRpm = 750.0f;
constexpr float kIdleFadeEndRpm = 1200.0f;

constexpr float kBuzzPitchDeg = 0.0085f;
constexpr float kBuzzRollDeg = 0.0065f;
constexpr float kBuzzYawDeg = 0.0025f;
constexpr float kBuzzY = 0.00005f; // meters
constexpr float kBuzzNoLoadShare = 0.3f;
constexpr float kBuzzIdleShare = 0.4f;
constexpr float kBuzzCellsPerRevolution = 0.5f;
constexpr float kMinBuzzRate = 6.0f; // cells per second

// Below a truck diesel's torque band (~1000 RPM), so an upshift doesn't
// count as lugging.
constexpr float kLugFadeStartRpm = 650.0f;
constexpr float kLugFadeEndRpm = 900.0f;
constexpr float kLugBoost = 0.5f;

constexpr float kRunningRpm = 300.0f;

// At least five frames per cycle, or the motion strobes.
constexpr float kMaxCyclesPerFrame = 0.2f;

constexpr float kLoadSeconds = 0.25f;
constexpr float kFrameSeconds = 0.5f;
constexpr float kNominalFrame = 1.0f / 60.0f;

constexpr uint32_t kBuzzSeed = 0x45564231u;
constexpr uint32_t kAxisSeedStep = 0x9E3779B9u;
constexpr uint32_t kUnevennessSeed = 0x45565448u;

float Ramp(float x, float from, float to) {
  return math::SmoothStep(std::clamp((x - from) / (to - from), 0.0f, 1.0f));
}
} // namespace

EngineVibrationEffect::EngineVibrationEffect(SPF_Config_API *config_api,
                                             SPF_Config_Handle *config_handle)
    : ConfigurableEffect(config_api, config_handle, "cabin",
                         "engine_vibration") {
  load_.SetTimeConstant(kLoadSeconds);
  frame_.SetTimeConstant(kFrameSeconds);
  Reset();
}

void EngineVibrationEffect::LoadSettings() {
  intensity_ = Float("intensity", intensity_);
}

void EngineVibrationEffect::Reset() {
  load_.Reset();
  frame_.Reset(kNominalFrame);
}

void EngineVibrationEffect::OnTruckConstantsChanged(
    const SPF_TruckConstants &constants) {
  is_electric_ = telemetry::IsElectric(constants);
  if (constants.rpm_limit > 1.0f)
    rpm_limit_ = constants.rpm_limit;
}

HeadOffset EngineVibrationEffect::Update(float dt, const SPF_TruckData &truck,
                                         const SPF_Controls & /*controls*/) {
  const float frame = frame_.Update(dt, dt);
  const float throttle = std::clamp(truck.effective_throttle, 0.0f, 1.0f);
  const float load = load_.Update(throttle, dt);
  const float rpm = truck.engine_rpm;
  if (rpm <= telemetry::kRpmStartThreshold)
    return {};

  const float max_rate = kMaxCyclesPerFrame / frame;
  const float running = Ramp(rpm, telemetry::kRpmStartThreshold, kRunningRpm);
  // Capped by the raw throttle: letting off drops the RPM through the band
  // while the eased load is still up.
  const float lug = std::min(load, throttle) *
                    (1.0f - Ramp(rpm, kLugFadeStartRpm, kLugFadeEndRpm));
  const float scale = intensity_ * running;

  const float cycle_rate = std::min(rpm / 120.0f, max_rate);
  throb_phase_ = math::WrapPhase(throb_phase_ + math::kTwoPi * cycle_rate * dt);
  throb_cycles_ =
      math::WrapNoisePhase(throb_cycles_ + cycle_rate * dt * kUnevennessRate);
  const float unevenness =
      1.0f + kThrobUnevenness *
                 math::GradientNoise1D(static_cast<float>(throb_cycles_),
                                       kUnevennessSeed);
  const float idle = 1.0f - Ramp(rpm, kIdleFadeStartRpm, kIdleFadeEndRpm);
  const float throb = (idle + kLugBoost * lug) * unevenness * scale;

  const float buzz_rate =
      std::clamp(rpm / 60.0f * kBuzzCellsPerRevolution,
                 std::min(kMinBuzzRate, max_rate), max_rate);
  buzz_phase_ = math::WrapNoisePhase(buzz_phase_ + buzz_rate * dt);
  const float rpm_share = std::clamp(rpm / rpm_limit_, 0.0f, 1.0f);
  const float buzz = math::Lerp(kBuzzNoLoadShare, 1.0f, load) *
                     math::Lerp(kBuzzIdleShare, 1.0f, rpm_share) *
                     (1.0f + kLugBoost * lug) * scale;
  const auto noise = [&](uint32_t axis) {
    return math::GradientNoise1D(static_cast<float>(buzz_phase_),
                                 kBuzzSeed + axis * kAxisSeedStep);
  };

  HeadOffset offset;
  offset.roll = throb * kThrobRollDeg * std::sin(throb_phase_) +
                buzz * kBuzzRollDeg * noise(0);
  offset.pitch =
      throb * kThrobPitchDeg * std::sin(throb_phase_ - kThrobPitchLag) +
      buzz * kBuzzPitchDeg * noise(1);
  offset.yaw = buzz * kBuzzYawDeg * noise(2);
  offset.pos_y = throb * kThrobY * std::sin(throb_phase_ - kThrobYLag) +
                 buzz * kBuzzY * noise(3);
  return offset;
}

} // namespace motioncab
