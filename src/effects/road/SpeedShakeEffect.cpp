#include "SpeedShakeEffect.hpp"

#include "effects/Telemetry.hpp"
#include "math/Noise.hpp"
#include "math/Units.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace motioncab {

namespace {
constexpr float kSpeedRefKmh = 90.0f; // full strength from here
constexpr float kSpeedDeadKmh = 3.0f;
constexpr float kSpeedCurve = 1.376f; // 20% at 30 km/h

// RMS at full speed and intensity on neutral ground. Peaks reach about 4x.
constexpr float kSwayPos = 0.0011f; // meters
constexpr float kSwayAngle = 0.11f; // degrees, before the per-axis ratios
// Measured offline: the noise peaks near +/-1 but averages much lower, and
// mixing bands lowers it further. Without it, the amplitudes above were
// nowhere near the real ones.
constexpr float kMixRms = 0.21f;

// Cab sway, cab/seat bounce and fine vibration, in noise cells per second
// (roughly Hz). A slower sway band made the head drift around like a
// handheld camera; much quicker and the head moved too fast.
constexpr float kBandRate[] = {0.7f, 1.8f, 4.5f};
constexpr float kRateSpeedMin = 0.85f; // barely moving
constexpr float kRateSpeedMax = 1.15f; // at kSpeedRefKmh

// Smoothing sets the noise speed, as sqrt(default / smoothing): x1.7 at
// 0.1 s, x0.55 at 1 s.
constexpr float kDefaultSmoothing =
    settings::Default("settings.road.speed_shake.smoothing_time");
constexpr float kMinSmoothing =
    settings::Min("settings.road.speed_shake.smoothing_time");

enum Channel { kX, kY, kYaw, kRoll, kPitch };
// Each row sums to 1. The fine vibration is all but left out: a real head
// doesn't buzz, and RoadIrregularityEffect has the texture.
constexpr float kBandWeight[5][3] = {
    {0.55f, 0.45f, 0.00f}, // x
    {0.30f, 0.65f, 0.05f}, // y
    {0.75f, 0.25f, 0.00f}, // yaw
    {0.50f, 0.50f, 0.00f}, // roll's own part
    {0.35f, 0.60f, 0.05f}, // pitch's own part
};

// A slow noise varies the pace of the bands, so the head settles a moment,
// then shifts more briskly. Calm never means frozen: a head held dead still
// for seconds looked broken.
constexpr float kActivityRate = 0.2f; // ~5 s cycle
constexpr float kActivityGain = 0.9f; // how sharply calm and brisk split
constexpr float kStillPace = 0.45f;
constexpr float kMovePace = 1.35f;
constexpr uint32_t kActivitySeedOffset = 0xAC71;

// The rest comes from their own noise.
constexpr float kRollFromLateral = 0.7f;
constexpr float kPitchFromVertical = 0.6f;
constexpr float kVerticalRatio = 0.6f;
constexpr float kPitchRatio = 0.7f;
// A yaw as large as the roll read as a handheld camera panning.
constexpr float kYawRatio = 0.3f;
// The head, heavier in front of the neck, lags when the seat pushes it up.
constexpr float kPitchSign = -1.0f;

// Some stretches of road rougher than others (~15 s cycle).
constexpr float kEnvelopeRate = 0.07f;
constexpr float kEnvelopeDepthMax = 1.0f;
constexpr float kEnvelopeMin = 0.2f;
constexpr float kEnvelopeMax = 1.6f;
constexpr uint32_t kEnvelopeSeedOffset = 0x5EED;

constexpr float kEntryFadeSeconds = 0.5f;

// Calmer on smooth asphalt, stronger off-road. Unknown ground stays neutral.
constexpr float kSmoothGroundGain = 0.85f;
constexpr float kRoughGroundGain = 1.5f;
constexpr float kSurfaceSmoothing = 0.5f;
} // namespace

SpeedShakeEffect::SpeedShakeEffect(SPF_Config_API *config_api,
                                   SPF_Config_Handle *config_handle)
    : ConfigurableEffect(config_api, config_handle, "road", "speed_shake"),
      seed_(std::random_device{}()) {
  entry_fade_.SetTimeConstant(kEntryFadeSeconds);
  surface_gain_.SetTimeConstant(kSurfaceSmoothing);
  Reset();
}

void SpeedShakeEffect::LoadSettings() {
  intensity_ = Float("intensity", intensity_);
  smoothing_ = Float("smoothing_time", smoothing_);
  rate_multiplier_ =
      std::sqrt(kDefaultSmoothing / std::max(smoothing_, kMinSmoothing));

  rotation_ = Float("rotation", rotation_);
  vertical_ = Float("vertical", vertical_);
  roughness_ = Float("roughness", roughness_);
}

void SpeedShakeEffect::Reset() {
  entry_fade_.Reset(0.0f);
  surface_gain_.Reset(1.0f);
}

void SpeedShakeEffect::OnTruckConstantsChanged(
    const SPF_TruckConstants &constants) {
  wheel_count_ = telemetry::WheelCount(constants);
}

void SpeedShakeEffect::OnCommonDataChanged(const SPF_CommonData &data) {
  surfaces_.SetSubstances(data);
}

HeadOffset SpeedShakeEffect::Update(float dt, const SPF_TruckData &truck,
                                    const SPF_Controls & /*controls*/) {
  const float speed_kmh = telemetry::SpeedKmh(truck);
  const float ramp = std::clamp(
      (speed_kmh - kSpeedDeadKmh) / (kSpeedRefKmh - kSpeedDeadKmh), 0.0f, 1.0f);
  // Already felt in town (20% at 30 km/h, 43% at 50), full toward highway
  // speed. Squared, it was barely there below 50 km/h.
  const float speed_amount = std::pow(ramp, kSpeedCurve);

  envelope_phase_ = math::WrapNoisePhase(envelope_phase_ + dt * kEnvelopeRate);
  const float envelope_noise = math::GradientNoise1D(
      static_cast<float>(envelope_phase_), seed_ + kEnvelopeSeedOffset);
  const float envelope =
      std::clamp(1.0f + roughness_ * kEnvelopeDepthMax * envelope_noise,
                 kEnvelopeMin, kEnvelopeMax);

  float ground_target = 1.0f;
  if (wheel_count_ > 0 && !surfaces_.empty()) {
    const SurfaceTraits surface = surfaces_.Average(truck, wheel_count_);
    const float t = std::max(surface.roughness, surface.unevenness);
    ground_target =
        kSmoothGroundGain + (kRoughGroundGain - kSmoothGroundGain) * t;
  }
  const float ground = surface_gain_.Update(ground_target, dt);

  const float amount = speed_amount * intensity_ * envelope * ground *
                       entry_fade_.Update(1.0f, dt);

  activity_phase_ = math::WrapNoisePhase(activity_phase_ + dt * kActivityRate);
  const float activity = math::SmoothStep(std::clamp(
      0.5f + kActivityGain *
                 math::GradientNoise1D(static_cast<float>(activity_phase_),
                                       seed_ + kActivitySeedOffset),
      0.0f, 1.0f));
  const float pace = kStillPace + (kMovePace - kStillPace) * activity;

  const float rate_scale =
      (kRateSpeedMin + (kRateSpeedMax - kRateSpeedMin) * ramp) *
      rate_multiplier_ * pace;

  // Each channel comes out at about unit RMS.
  std::array<float, kChannels> mixed{};
  for (int b = 0; b < kBands; ++b) {
    phase_[b] =
        math::WrapNoisePhase(phase_[b] + dt * kBandRate[b] * rate_scale);
    for (int c = 0; c < kChannels; ++c) {
      const float noise = math::GradientNoise1D(
          static_cast<float>(phase_[b]),
          seed_ + static_cast<uint32_t>(b * kChannels + c) * 7919U);
      mixed[c] += kBandWeight[c][b] * noise / kMixRms;
    }
  }

  const float lateral = mixed[kX];
  const float vertical = mixed[kY];
  const float roll_n =
      kRollFromLateral * lateral + (1.0f - kRollFromLateral) * mixed[kRoll];
  const float pitch_n = kPitchSign * kPitchFromVertical * vertical +
                        (1.0f - kPitchFromVertical) * mixed[kPitch];

  HeadOffset offset;
  offset.pos_x = lateral * kSwayPos * amount;
  offset.pos_y = vertical * kVerticalRatio * vertical_ * kSwayPos * amount;
  const float angle_amount = kSwayAngle * rotation_ * amount;
  offset.yaw = mixed[kYaw] * kYawRatio * angle_amount;
  offset.pitch = pitch_n * kPitchRatio * angle_amount;
  offset.roll = roll_n * angle_amount;
  return offset;
}

} // namespace motioncab
