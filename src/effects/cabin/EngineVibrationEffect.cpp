#include "EngineVibrationEffect.hpp"

#include "effects/Telemetry.hpp"
#include "math/Noise.hpp"
#include "math/Units.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace motioncab {

namespace {
constexpr float kFiringsPerRevolution = 3.0f; // inline six, four-stroke

// Below a truck diesel's torque band (~1000 RPM), so an upshift doesn't
// count as lugging.
constexpr float kLugFadeStartRpm = 650.0f;
constexpr float kLugFadeEndRpm = 900.0f;
constexpr float kLugBoost = 0.8f;

// The mounts and cab resonate a little under the idle firing frequency, so
// idle shakes most and the shake fades as the RPM climbs away from it.
constexpr float kResonanceHz = 26.0f;
constexpr float kResonanceDamping = 0.35f;
constexpr float kShakeWeight = 0.08f;

// The firing pulses grow with the torque: pulling hard hums, coasting
// barely does. Higher RPM pushes it a bit further.
constexpr float kHumWeight = 1.0f;
constexpr float kHumNoLoad = 0.15f;
constexpr float kHumLowRpm = 0.3f;
constexpr float kLoadSeconds = 0.25f;

// No two engine cycles fire quite alike: the strength wanders about once a
// cycle (every other revolution), which makes the idle lope. Kept faint: any
// more and the idle seemed to come and go.
constexpr float kRoughness = 0.15f;
constexpr float kRoughnessCellsPerCycle = 0.8f;
// At least five frames per cell, or the loping turns into flicker.
constexpr float kMaxRoughnessCellsPerFrame = 0.2f;

// At amplitude 1. Pitch and roll lead: the firing torque rocks the engine
// about the crankshaft, and the cab with it.
constexpr float kPitchDeg = 0.06f;
constexpr float kRollDeg = 0.05f;
constexpr float kYawDeg = 0.015f;
constexpr float kY = 0.0003f; // meters

// A rate in cycles per second read as a slow wobble, and nearing 0.5 cycles
// per frame (every frame flipping) the eye blurs it into nothing. A sine
// rather than noise: noise's strength varies from one cell to the next, so a
// steady idle shook hard for a moment, then went nearly still. Each axis runs
// at its own rate and its phase wanders, so the axes never fall into step.
constexpr std::array<float, 4> kJitterCyclesPerFrame = {0.28f, 0.26f, 0.31f,
                                                        0.24f};
constexpr float kWanderCycles = 0.4f;
constexpr float kWanderCellsPerSecond = 1.5f;

constexpr float kRunningRpm = 300.0f;
constexpr float kFrameSeconds = 0.5f;
constexpr float kNominalFrame = 1.0f / 60.0f;

constexpr uint32_t kWanderSeed = 0x45564231u;
constexpr uint32_t kAxisSeedStep = 0x9E3779B9u;
constexpr uint32_t kRoughnessSeed = 0x45565448u;

float Ramp(float x, float from, float to) {
  return math::SmoothStep(std::clamp((x - from) / (to - from), 0.0f, 1.0f));
}

// A driven oscillator's response to the firing frequency, 1 at its peak.
// Damped past 1/sqrt(2), it has no peak and only falls from 1.
float MountResponse(float firing_hz, float resonance_hz, float damping) {
  const float r = firing_hz / resonance_hz;
  const float stiffness = 1.0f - r * r;
  const float friction = 2.0f * damping * r;
  const float peak = damping < std::numbers::sqrt2_v<float> / 2.0f
                         ? 2.0f * damping * std::sqrt(1.0f - damping * damping)
                         : 1.0f;
  return peak / std::sqrt(stiffness * stiffness + friction * friction);
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

  // Capped by the raw throttle: letting off drops the RPM through the band
  // while the eased load is still up. In neutral nothing holds the engine
  // back, and the idle sits under the band.
  const float lug =
      truck.gear == 0
          ? 0.0f
          : std::min(load, throttle) *
                (1.0f - Ramp(rpm, kLugFadeStartRpm, kLugFadeEndRpm));
  const float firing_hz = rpm / 60.0f * kFiringsPerRevolution;
  const float shake =
      kShakeWeight * MountResponse(firing_hz, kResonanceHz, kResonanceDamping);
  const float rpm_share = std::clamp(rpm / rpm_limit_, 0.0f, 1.0f);
  const float hum = kHumWeight * math::Lerp(kHumNoLoad, 1.0f, load) *
                    math::Lerp(kHumLowRpm, 1.0f, rpm_share);

  const float cycles_per_second = rpm / 120.0f;
  const float roughness_rate =
      std::min(cycles_per_second * kRoughnessCellsPerCycle,
               kMaxRoughnessCellsPerFrame / frame);
  roughness_phase_ =
      math::WrapNoisePhase(roughness_phase_ + roughness_rate * dt);
  const float roughness =
      1.0f +
      kRoughness * math::GradientNoise1D(static_cast<float>(roughness_phase_),
                                         kRoughnessSeed);

  const float running = Ramp(rpm, telemetry::kRpmStartThreshold, kRunningRpm);
  const float amplitude = (shake + hum) * (1.0f + kLugBoost * lug) * roughness *
                          intensity_ * running;

  wander_phase_ =
      math::WrapNoisePhase(wander_phase_ + kWanderCellsPerSecond * dt);
  for (size_t axis = 0; axis < jitter_phases_.size(); ++axis)
    jitter_phases_[axis] = std::fmod(
        jitter_phases_[axis] + kJitterCyclesPerFrame[axis] / frame * dt, 1.0);
  const auto jitter = [&](uint32_t axis) {
    const float wander =
        kWanderCycles *
        math::GradientNoise1D(static_cast<float>(wander_phase_),
                              kWanderSeed + axis * kAxisSeedStep);
    return std::sin(math::kTwoPi *
                    (static_cast<float>(jitter_phases_[axis]) + wander));
  };

  HeadOffset offset;
  offset.pitch = amplitude * kPitchDeg * jitter(0);
  offset.roll = amplitude * kRollDeg * jitter(1);
  offset.yaw = amplitude * kYawDeg * jitter(2);
  offset.pos_y = amplitude * kY * jitter(3);
  return offset;
}

} // namespace motioncab
