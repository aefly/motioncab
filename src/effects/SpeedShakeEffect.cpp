#include "SpeedShakeEffect.hpp"

#include "math/Noise.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace motioncab {

namespace {
constexpr float kSpeedRefKmh = 90.0f; // sway is at full strength here
constexpr float kSpeedDeadKmh = 3.0f; // below this the truck is "stopped"

// Amplitudes at full speed, intensity 1.0.
constexpr float kSwayPos = 0.0035f; // meters
constexpr float kSwayAngle = 0.35f; // degrees

// Band base rates in noise cells per second (roughly Hz), scaled up a little
// with speed. Bands: slow body sway, cab/seat bounce, fine vibration.
constexpr float kBandRate[] = {0.5f, 2.0f, 5.0f};
constexpr float kRateSpeedMin = 0.7f; // rate multiplier when barely moving
constexpr float kRateSpeedMax = 1.3f; // ... and at reference speed
// Each band's low-pass time constant relative to the Smoothing setting:
// faster bands must be filtered much less or they would be smoothed away.
constexpr float kBandSmoothingScale[] = {1.0f, 0.25f, 0.08f};

// Per-channel mix of the three bands (each row sums to 1).
enum Channel { kX, kY, kYaw, kRoll, kPitch };
constexpr float kBandWeight[5][3] = {
    {0.70f, 0.22f, 0.08f}, // x: mostly slow lateral sway
    {0.30f, 0.55f, 0.15f}, // y: dominated by the faster vertical bounce
    {0.85f, 0.15f, 0.00f}, // yaw: slow only
    {0.60f, 0.30f, 0.10f}, // roll (independent part)
    {0.40f, 0.50f, 0.10f}, // pitch (independent part)
};

// How much of roll/pitch comes from lateral/vertical motion vs own noise.
constexpr float kRollFromLateral = 0.7f;
constexpr float kPitchFromVertical = 0.6f;
// Vertical head motion is smaller than lateral; rotation smaller than sway.
constexpr float kVerticalRatio = 0.6f;
constexpr float kPitchRatio = 0.7f;

// Slow amplitude envelope: some stretches rougher than others (~15 s cycle).
constexpr float kEnvelopeRate = 0.07f;
// Roughness (0..1) scales how far the envelope swings around 1.0 (0 = flat).
constexpr float kEnvelopeDepthMax = 1.0f; // envelope noise gain at roughness 1
constexpr float kEnvelopeMin = 0.2f;
constexpr float kEnvelopeMax = 1.6f;
constexpr uint32_t kEnvelopeSeedOffset = 0x5EED;
} // namespace

SpeedShakeEffect::SpeedShakeEffect(SPF_Config_API *config_api,
                                   SPF_Config_Handle *config_handle)
    : config_api_(config_api), config_handle_(config_handle),
      seed_(std::random_device{}()) {
  ApplySmoothing();
}

void SpeedShakeEffect::ApplySmoothing() {
  for (int b = 0; b < kBands; ++b)
    for (auto &f : filter_[b])
      f.SetTimeConstant(smoothing_ * kBandSmoothingScale[b]);
}

void SpeedShakeEffect::LoadConfig() {
  if (!config_api_ || !config_handle_)
    return;

  enabled_ = config_api_->Cfg_GetBool(
      config_handle_, "settings.road.speed_shake.enabled", enabled_);
  intensity_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.road.speed_shake.intensity", intensity_));
  smoothing_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.road.speed_shake.smoothing_time", smoothing_));

  rotation_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.road.speed_shake.rotation", rotation_));
  vertical_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.road.speed_shake.vertical", vertical_));
  roughness_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.road.speed_shake.roughness", roughness_));

  ApplySmoothing();
}

void SpeedShakeEffect::Reset() {
  for (auto &band : filter_)
    for (auto &f : band)
      f.Reset();
  phase_.fill(0.0f);
}

HeadOffset SpeedShakeEffect::Update(float dt, const SPF_TruckData &truck,
                                    const SPF_Controls & /*controls*/) {
  const float speed_kmh = std::fabs(truck.speed) * 3.6f;
  const float ramp = std::clamp(
      (speed_kmh - kSpeedDeadKmh) / (kSpeedRefKmh - kSpeedDeadKmh), 0.0f, 1.0f);

  envelope_phase_ += dt * kEnvelopeRate;
  const float envelope =
      std::clamp(1.0f + roughness_ * kEnvelopeDepthMax *
                            math::GradientNoise1D(envelope_phase_,
                                                  seed_ + kEnvelopeSeedOffset),
                 kEnvelopeMin, kEnvelopeMax);
  const float amount = ramp * intensity_ * envelope;

  const float rate_scale =
      kRateSpeedMin + (kRateSpeedMax - kRateSpeedMin) * ramp;

  // Mix the three bands of each channel into one value in about [-1, 1].
  std::array<float, kChannels> mixed{};
  for (int b = 0; b < kBands; ++b) {
    phase_[b] += dt * kBandRate[b] * rate_scale;
    for (int c = 0; c < kChannels; ++c) {
      const float raw = math::GradientNoise1D(
          phase_[b], seed_ + static_cast<uint32_t>(b * kChannels + c) * 7919U);
      mixed[c] += kBandWeight[c][b] * filter_[b][c].Update(raw, dt);
    }
  }

  const float lateral = mixed[kX];
  const float vertical = mixed[kY];
  const float roll_n =
      kRollFromLateral * lateral + (1.0f - kRollFromLateral) * mixed[kRoll];
  const float pitch_n = kPitchFromVertical * vertical +
                        (1.0f - kPitchFromVertical) * mixed[kPitch];

  HeadOffset offset;
  offset.pos_x = lateral * kSwayPos * amount;
  offset.pos_y = vertical * kVerticalRatio * vertical_ * kSwayPos * amount;
  const float angle_amount = kSwayAngle * rotation_ * amount;
  offset.yaw = mixed[kYaw] * angle_amount;
  offset.pitch = pitch_n * kPitchRatio * angle_amount;
  offset.roll = roll_n * angle_amount;
  return offset;
}

} // namespace motioncab
