#include "RoadIrregularityEffect.hpp"

#include "effects/TelemetryUtil.hpp"
#include "math/Noise.hpp"

#include <algorithm>
#include <cmath>

namespace motioncab {

namespace {
constexpr float kBaseAmplitude = 0.0012f; // meters, at full roughness+speed
constexpr float kLateralGainRatio = 0.6f;
constexpr float kSpeedRampKmh = 30.0f; // ramps in fully by this speed
// Frame time the noise gain was tuned at (60 fps), and the shortest frame
// time compensated for so a very high fps can't blow the gain up.
constexpr float kReferenceDt = 1.0f / 60.0f;
constexpr float kMinCompensatedDt = 1.0f / 1000.0f;

// Off-road rocking (roll), degrees at full unevenness and intensity 1.0.
constexpr float kRollAmplitudeDeg = 1.2f;
// Unlike the chatter, the rocking is already there at walking pace.
constexpr float kRollSpeedRampKmh = 15.0f;
// Noise cells per second (roughly Hz): bumps pass faster with speed.
constexpr float kRollRateMin = 0.35f; // barely moving
constexpr float kRollRateMax = 1.6f;  // at kRollRateRefKmh and above
constexpr float kRollRateRefKmh = 50.0f;
// A second, faster octave so it isn't a smooth sine-like sway.
constexpr float kRollDetailRatio = 2.3f;   // rate relative to the base
constexpr float kRollDetailWeight = 0.35f; // share of the amplitude
constexpr uint32_t kRollDetailSeedOffset = 0x51DE;
// How fast the rocking fades in/out when the ground changes.
constexpr float kUnevennessSmoothing = 0.4f; // seconds
} // namespace

RoadIrregularityEffect::RoadIrregularityEffect(SPF_Config_API *config_api,
                                               SPF_Config_Handle *config_handle)
    : ConfigurableEffect(config_api, config_handle, "road",
                         "road_irregularity"),
      rng_(std::random_device{}()), roll_seed_(std::random_device{}()) {
  unevenness_.SetTimeConstant(kUnevennessSmoothing);
}

void RoadIrregularityEffect::LoadSettings() {
  intensity_ = Float("intensity", intensity_);
  reactivity_ = Float("reactivity", reactivity_);

  noise_x_.SetTimeConstant(reactivity_);
  noise_y_.SetTimeConstant(reactivity_);
}

void RoadIrregularityEffect::Reset() {
  noise_x_.Reset();
  noise_y_.Reset();
  unevenness_.Reset();
}

void RoadIrregularityEffect::OnTruckConstantsChanged(
    const SPF_TruckConstants &constants) {
  wheel_count_ = telemetry::WheelCount(constants);
}

void RoadIrregularityEffect::OnCommonDataChanged(const SPF_CommonData &data) {
  surfaces_.SetSubstances(data);
}

HeadOffset RoadIrregularityEffect::Update(float dt, const SPF_TruckData &truck,
                                          const SPF_Controls & /*controls*/) {
  if (wheel_count_ == 0 || surfaces_.empty())
    return {};

  const road::SurfaceTraits surface = surfaces_.Average(truck, wheel_count_);
  const float roughness = surface.roughness;
  const float unevenness = unevenness_.Update(surface.unevenness, dt);
  const float speed_kmh = telemetry::SpeedKmh(truck);

  HeadOffset offset;

  // Off-road rocking: gradient noise is already smooth, so it needs no
  // low-pass filter. Its phase only advances with speed, so a stopped
  // truck holds still instead of swaying on its own.
  const float rate_t = std::min(speed_kmh / kRollRateRefKmh, 1.0f);
  const float rate = kRollRateMin + (kRollRateMax - kRollRateMin) * rate_t;
  const float roll_speed_factor = std::min(speed_kmh / kRollSpeedRampKmh, 1.0f);
  const double roll_step = rate * roll_speed_factor * dt;
  roll_phase_ = math::WrapNoisePhase(roll_phase_ + roll_step);
  roll_detail_phase_ =
      math::WrapNoisePhase(roll_detail_phase_ + roll_step * kRollDetailRatio);
  const float roll_noise =
      (1.0f - kRollDetailWeight) *
          math::GradientNoise1D(static_cast<float>(roll_phase_), roll_seed_) +
      kRollDetailWeight *
          math::GradientNoise1D(static_cast<float>(roll_detail_phase_),
                                roll_seed_ + kRollDetailSeedOffset);
  offset.roll = roll_noise * kRollAmplitudeDeg * intensity_ * unevenness *
                roll_speed_factor;

  if (roughness <= 0.0f) {
    // Let the filters decay smoothly toward silence on smooth ground
    // instead of snapping to zero mid-transition.
    offset.pos_y = noise_y_.Update(0.0f, dt);
    offset.pos_x = noise_x_.Update(0.0f, dt) * kLateralGainRatio;
    return offset;
  }

  const float speed_factor =
      speed_kmh < kSpeedRampKmh ? speed_kmh / kSpeedRampKmh : 1.0f;
  const float amplitude =
      kBaseAmplitude * intensity_ * roughness * speed_factor;

  std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
  // One white-noise sample per frame through a fixed low-pass filter: the
  // output variance grows with dt, so the shake would be rougher at low fps
  // and calmer at high fps. Scaling by sqrt(kReferenceDt / dt) keeps the
  // strength the same at any frame rate (unchanged at 60 fps).
  const float fps_gain =
      std::sqrt(kReferenceDt / std::max(dt, kMinCompensatedDt));
  const float raw_x = noise(rng_) * fps_gain;
  const float raw_y = noise(rng_) * fps_gain;

  offset.pos_y = noise_y_.Update(raw_y * amplitude, dt);
  offset.pos_x = noise_x_.Update(raw_x * amplitude, dt) * kLateralGainRatio;
  return offset;
}

} // namespace motioncab
