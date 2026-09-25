#include "RoadIrregularityEffect.hpp"

#include "math/Noise.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

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

bool EqualsCaseInsensitive(const char *a, const char *b) {
  std::string sa(a), sb(b);
  for (auto &c : sa)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  for (auto &c : sb)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return sa == sb;
}

struct SurfaceEntry {
  const char *name;
  float roughness;  // fine chatter
  float unevenness; // side-to-side rocking
};

// Exact names confirmed empirically from SPF_CommonData.substances[] at
// runtime (ETS2)
constexpr SurfaceEntry kSurfaceTable[] = {
    {"road", 0.0f, 0.0f},        {"road_smooth", 0.0f, 0.0f},
    {"road_coarse", 0.5f, 0.0f}, {"road_dirt", 0.7f, 0.6f},
    {"road_snow", 0.5f, 0.2f},   {"dirt", 0.9f, 1.0f},
    {"gravel", 1.0f, 0.5f},      {"grass", 0.5f, 0.9f},
    {"snow", 0.4f, 0.6f},        {"soft", 0.3f, 0.8f},
    {"concrete", 0.15f, 0.0f},   {"metal", 0.2f, 0.0f},
    {"wood", 0.2f, 0.0f},        {"rumble_stripe", 1.0f, 0.0f},
    {"ice", 0.0f, 0.0f},         {"static", 0.0f, 0.0f},
    {"invis", 0.0f, 0.0f},       {"rubber", 0.0f, 0.0f},
    {"plastic", 0.0f, 0.0f},     {"glass", 0.0f, 0.0f},
};
} // namespace

RoadIrregularityEffect::RoadIrregularityEffect(SPF_Config_API *config_api,
                                               SPF_Config_Handle *config_handle)
    : config_api_(config_api), config_handle_(config_handle),
      rng_(std::random_device{}()), roll_seed_(std::random_device{}()) {
  unevenness_.SetTimeConstant(kUnevennessSmoothing);
}

RoadIrregularityEffect::SurfaceTraits
RoadIrregularityEffect::ClassifySurface(const char *substance_name) {
  if (!substance_name || !substance_name[0])
    return {};

  for (const auto &entry : kSurfaceTable) {
    if (EqualsCaseInsensitive(substance_name, entry.name))
      return {entry.roughness, entry.unevenness};
  }
  return {}; // unrecognized name (other map/mod): default to smooth
}

void RoadIrregularityEffect::LoadConfig() {
  if (!config_api_ || !config_handle_)
    return;

  enabled_ = config_api_->Cfg_GetBool(
      config_handle_, "settings.road.road_irregularity.enabled", enabled_);
  intensity_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.road.road_irregularity.intensity", intensity_));
  reactivity_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.road.road_irregularity.reactivity",
      reactivity_));

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
  wheel_count_ = constants.wheel_count;
  if (wheel_count_ > SPF_TELEMETRY_WHEEL_MAX_COUNT)
    wheel_count_ = 0;
}

void RoadIrregularityEffect::OnCommonDataChanged(const SPF_CommonData &data) {
  substance_count_ = data.substance_count;
  if (substance_count_ > SPF_TELEMETRY_SUBSTANCE_MAX_COUNT)
    substance_count_ = SPF_TELEMETRY_SUBSTANCE_MAX_COUNT;
  for (uint32_t i = 0; i < substance_count_; ++i) {
    substance_traits_[i] = ClassifySurface(data.substances[i]);
  }
}

HeadOffset RoadIrregularityEffect::Update(float dt, const SPF_TruckData &truck,
                                          const SPF_Controls & /*controls*/) {
  if (wheel_count_ == 0 || substance_count_ == 0)
    return {};

  float roughness_sum = 0.0f;
  float unevenness_sum = 0.0f;
  for (uint32_t i = 0; i < wheel_count_; ++i) {
    const uint32_t substance_index = truck.wheels[i].substance;
    if (substance_index < substance_count_) {
      roughness_sum += substance_traits_[substance_index].roughness;
      unevenness_sum += substance_traits_[substance_index].unevenness;
    }
  }
  const float roughness = roughness_sum / static_cast<float>(wheel_count_);
  const float unevenness =
      unevenness_.Update(unevenness_sum / static_cast<float>(wheel_count_), dt);
  const float speed_kmh = std::fabs(truck.speed) * 3.6f;

  HeadOffset offset;

  // Off-road rocking: gradient noise is already smooth, so it needs no
  // low-pass filter. Its phase only advances with speed, so a stopped
  // truck holds still instead of swaying on its own.
  const float rate_t = std::min(speed_kmh / kRollRateRefKmh, 1.0f);
  const float rate = kRollRateMin + (kRollRateMax - kRollRateMin) * rate_t;
  const float roll_speed_factor = std::min(speed_kmh / kRollSpeedRampKmh, 1.0f);
  roll_phase_ += rate * roll_speed_factor * dt;
  const float roll_noise =
      (1.0f - kRollDetailWeight) *
          math::GradientNoise1D(roll_phase_, roll_seed_) +
      kRollDetailWeight *
          math::GradientNoise1D(roll_phase_ * kRollDetailRatio,
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
