#include "SpeedShakeEffect.hpp"

#include "effects/TelemetryUtil.hpp"
#include "math/Noise.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace motioncab {

namespace {
constexpr float kSpeedRefKmh = 90.0f; // sway is at full strength here
constexpr float kSpeedDeadKmh = 3.0f; // below this the truck is "stopped"

// RMS amplitudes at full speed, intensity 1.0, on neutral ground; peaks
// reach about 4x these.
constexpr float kSwayPos = 0.0015f; // meters, lateral
constexpr float kSwayAngle = 0.15f; // degrees, before the per-axis ratios
// RMS of a channel's band mix (measured offline): gradient noise peaks near
// +/-1 but sits much lower on average, and mixing bands lowers it further.
// Dividing by it makes the amplitudes above the real ones; without it, the
// shake was well under a millimeter and a tenth of a degree.
constexpr float kMixRms = 0.2f;

// Band base rates in noise cells per second (roughly Hz), scaled up a little
// with speed. Bands: slow body sway, cab/seat bounce, fine vibration.
constexpr float kBandRate[] = {0.5f, 2.0f, 5.0f};
constexpr float kRateSpeedMin = 0.7f; // rate multiplier when barely moving
constexpr float kRateSpeedMax = 1.3f; // ... and at reference speed

// Smoothing sets the noise speed, as sqrt(default / smoothing): x1.7 at
// 0.1 s, x0.55 at 1 s. Gradient noise keeps its size at any speed, unlike
// the low-pass filters this replaced, which also shrank the shake.
constexpr float kDefaultSmoothing =
    settings::Default("settings.road.speed_shake.smoothing_time");
constexpr float kMinSmoothing =
    settings::Min("settings.road.speed_shake.smoothing_time");

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
// The eyes stay on the road, so the head barely turns: a yaw as large as
// the roll read as a handheld camera panning.
constexpr float kYawRatio = 0.3f;
// Pitch nods against the bounce: the head, heavier in front of the neck,
// lags when the seat pushes it up. +1 would follow the bounce instead.
constexpr float kPitchSign = -1.0f;

// Slow amplitude envelope: some stretches rougher than others (~15 s cycle).
constexpr float kEnvelopeRate = 0.07f;
// Roughness (0..1) scales how far the envelope swings around 1.0 (0 = flat).
constexpr float kEnvelopeDepthMax = 1.0f; // envelope noise gain at roughness 1
constexpr float kEnvelopeMin = 0.2f;
constexpr float kEnvelopeMax = 1.6f;
constexpr uint32_t kEnvelopeSeedOffset = 0x5EED;

// Fades the shake in on cabin entry, since the noise doesn't restart at 0.
constexpr float kEntryFadeSeconds = 0.5f;

// Amplitude factor from the ground (the rougher of its two traits): calmer
// on smooth asphalt, stronger off-road. Unknown ground stays neutral.
constexpr float kSmoothGroundGain = 0.85f;
constexpr float kRoughGroundGain = 1.5f;
constexpr float kSurfaceSmoothing = 0.5f; // seconds, eases ground changes
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
  // Squared: the sway builds up with the road's energy, so town speeds stay
  // calm (29% at 50 km/h) and it comes in fully toward highway speed.
  const float speed_amount = ramp * ramp;

  envelope_phase_ = math::WrapNoisePhase(envelope_phase_ + dt * kEnvelopeRate);
  const float envelope_noise = math::GradientNoise1D(
      static_cast<float>(envelope_phase_), seed_ + kEnvelopeSeedOffset);
  const float envelope =
      std::clamp(1.0f + roughness_ * kEnvelopeDepthMax * envelope_noise,
                 kEnvelopeMin, kEnvelopeMax);

  float ground_target = 1.0f;
  if (wheel_count_ > 0 && !surfaces_.empty()) {
    const road::SurfaceTraits surface = surfaces_.Average(truck, wheel_count_);
    const float t = std::max(surface.roughness, surface.unevenness);
    ground_target =
        kSmoothGroundGain + (kRoughGroundGain - kSmoothGroundGain) * t;
  }
  const float ground = surface_gain_.Update(ground_target, dt);

  const float amount = speed_amount * intensity_ * envelope * ground *
                       entry_fade_.Update(1.0f, dt);

  const float rate_scale =
      (kRateSpeedMin + (kRateSpeedMax - kRateSpeedMin) * ramp) *
      rate_multiplier_;

  // Mix the three bands of each channel into one value of about unit RMS.
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
