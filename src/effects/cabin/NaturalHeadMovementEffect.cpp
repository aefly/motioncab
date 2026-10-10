#include "NaturalHeadMovementEffect.hpp"

#include "effects/Telemetry.hpp"
#include "math/Noise.hpp"
#include "math/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

namespace motioncab {

namespace {
// The per-axis values are X, Y, Z in meters, then yaw, pitch, roll in
// degrees, at intensity 1.

constexpr double kTremorRate[] = {0.15, 0.4}; // noise cells per second
constexpr float kOctaveWeight[] = {1.0f, 0.35f};
constexpr float kTremorNorm = 1.0f / (kOctaveWeight[0] + kOctaveWeight[1]);
constexpr float kTremorAmplitude[] = {0.002f, 0.0015f, 0.002f,
                                      0.35f,  0.25f,   0.2f};

// The *Seconds are for a move across the whole range.
constexpr float kMicroRange[] = {0.003f, 0.002f, 0.004f, 1.0f, 0.7f, 0.6f};
constexpr float kMicroSeconds = 3.0f;
constexpr float kMicroInterval = 15.0f;

constexpr float kPostureRange[] = {0.012f, 0.012f, 0.025f, 5.0f, 3.0f, 3.0f};
constexpr float kPostureSeconds = 2.0f;
constexpr float kPostureVariation = 0.1f;

// How long a move with no distance to go takes, as a share of a full one.
constexpr float kShortMoveShare = 0.4f;

// Body shapes rather than random axes, so a posture looks like something a
// driver would do. Each is on one side: X, yaw and roll flip for the other.
using PostureShape = float[6];
constexpr PostureShape kIdlePostures[] = {
    {0.0f, -1.0f, 0.3f, 0.0f, -0.2f, 0.0f},  // slouched
    {0.8f, -0.4f, 0.0f, -0.1f, 0.0f, 0.6f},  // leaning on the door
    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},    // head tilted
    {0.0f, 0.2f, 0.0f, 0.0f, 0.6f, 1.0f},    // stretching the neck
    {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.2f},    // looking out the window
    {0.0f, -0.2f, -0.3f, 0.0f, -1.0f, 0.0f}, // head down, at the lap
};
constexpr PostureShape kDrivingPostures[] = {
    {0.0f, -0.2f, -1.0f, 0.0f, -0.3f, 0.0f}, // forward, toward the road
    {0.0f, 0.6f, 0.0f, 0.0f, 0.2f, 0.0f},    // sitting up tall
    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f},    // head slightly tilted
    {0.5f, -0.2f, 0.0f, 0.0f, 0.0f, 0.3f},   // leaning a little on one side
};

struct PostureMode {
  std::span<const PostureShape> shapes;
  float neutral_odds;
  float scale;    // of kPostureRange
  float slowness; // of kPostureSeconds
};
// Stopped a while, relaxed: wider, slower, rarely back to neutral.
constexpr PostureMode kIdleMode{kIdlePostures, 0.2f, 1.5f, 1.3f};
// Driving, focused on the road: small postures, often back to neutral.
constexpr PostureMode kDrivingMode{kDrivingPostures, 1.0f / 3.0f, 0.6f, 1.0f};

// Switching modes moves to the new one's first posture over a set time,
// whatever the distance.
constexpr float kIdleSpeedKmh = 2.0f;
constexpr float kIdleDelaySeconds = 5.0f;
constexpr float kRelaxSeconds = 3.0f;
constexpr float kFocusSeconds = 3.0f;

// At full lock. It follows the square root of the steering, so a lane
// correction already tilts a little without full lock tilting too much.
constexpr float kSteeringTiltDeg = 3.0f;
constexpr float kSteeringTiltSeconds = 0.4f;

// Where the neck is from the eyes, in meters.
constexpr float kNeckBelow = 0.10f;
constexpr float kNeckBehind = 0.08f;

// Each axis gets its own seed, so they don't move together.
constexpr uint32_t kTremorSeed = 0x4E484D31u;
constexpr uint32_t kAxisSeedStep = 0x9E3779B9u;
constexpr uint32_t kOctaveSeedStep = 0x85EBCA6Bu;

// Where the eyes go when the head turns on the neck, in the head's frame.
// Rolled first, then pitched, then yawed.
struct Vec3 {
  float x, y, z;
};
Vec3 NeckPivot(float yaw, float pitch, float roll) {
  Vec3 e{0.0f, kNeckBelow, -kNeckBehind};
  e = {e.y * std::sin(roll), e.y * std::cos(roll), e.z};
  e = {e.x, e.y * std::cos(pitch) - e.z * std::sin(pitch),
       e.y * std::sin(pitch) + e.z * std::cos(pitch)};
  e = {e.x * std::cos(yaw) + e.z * std::sin(yaw), e.y,
       -e.x * std::sin(yaw) + e.z * std::cos(yaw)};
  return {e.x, e.y - kNeckBelow, e.z + kNeckBehind};
}
} // namespace

float NaturalHeadMovementEffect::Layer::Value(int axis) const {
  return math::Lerp(from[axis], to[axis],
                    math::SmootherStep(elapsed / duration));
}

void NaturalHeadMovementEffect::Layer::MoveTo(const Pose &target,
                                              float min_seconds,
                                              float max_seconds) {
  float distance = 0.0f;
  for (int a = 0; a < kAxes; ++a) {
    from[a] = Value(a);
    distance = std::max(distance, std::fabs(target[a] - from[a]));
  }
  to = target;
  elapsed = 0.0f;
  duration = math::Lerp(min_seconds, max_seconds, std::min(distance, 1.0f));
}

void NaturalHeadMovementEffect::Layer::Advance(float dt) {
  elapsed = std::min(elapsed + dt, duration);
}

void NaturalHeadMovementEffect::Layer::Reset() {
  from.fill(0.0f);
  to.fill(0.0f);
  elapsed = duration;
}

NaturalHeadMovementEffect::NaturalHeadMovementEffect(
    SPF_Config_API *config_api, SPF_Config_Handle *config_handle)
    : ConfigurableEffect(config_api, config_handle, "cabin",
                         "natural_head_movement"),
      rng_(std::random_device{}()) {
  steering_roll_.SetTimeConstant(kSteeringTiltSeconds);
  Reset();
}

void NaturalHeadMovementEffect::LoadSettings() {
  tremor_intensity_ = Float("tremor_intensity", tremor_intensity_);
  micro_intensity_ = Float("micro_intensity", micro_intensity_);
  posture_intensity_ = Float("posture_intensity", posture_intensity_);
  posture_interval_ = Float("posture_interval", posture_interval_);
  steering_tilt_ = Float("steering_tilt", steering_tilt_);
}

void NaturalHeadMovementEffect::OnTruckConstantsChanged(
    const SPF_TruckConstants &constants) {
  right_hand_drive_ = telemetry::IsRightHandDrive(constants);
}

void NaturalHeadMovementEffect::Reset() {
  // Not the tremor's phases: the head picks up somewhere new.
  micro_.Reset();
  micro_.timer = Jitter(kMicroInterval);
  posture_.Reset();
  posture_.timer = Jitter(posture_interval_);
  stopped_for_ = 0.0f;
  idle_ = false;
  steering_roll_.Reset();
}

float NaturalHeadMovementEffect::Jitter(float mean) {
  std::uniform_real_distribution<float> jitter(0.5f, 1.5f);
  return mean * jitter(rng_);
}

void NaturalHeadMovementEffect::DrawMicro() {
  std::uniform_real_distribution<float> side(-1.0f, 1.0f);
  Pose target;
  for (float &axis : target)
    axis = side(rng_);
  micro_.MoveTo(target, kShortMoveShare * kMicroSeconds, kMicroSeconds);
}

void NaturalHeadMovementEffect::DrawPosture() {
  const PostureMode &mode = idle_ ? kIdleMode : kDrivingMode;
  Pose target{};
  std::uniform_real_distribution<float> unit(0.0f, 1.0f);
  if (unit(rng_) >= mode.neutral_odds) {
    std::uniform_int_distribution<size_t> pick(0, mode.shapes.size() - 1);
    const PostureShape &shape = mode.shapes[pick(rng_)];
    const float side = unit(rng_) < 0.5f ? -1.0f : 1.0f;
    std::uniform_real_distribution<float> strength(0.5f, 1.0f);
    std::uniform_real_distribution<float> vary(-kPostureVariation,
                                               kPostureVariation);
    const float amount = strength(rng_) * mode.scale;
    for (int a = 0; a < kAxes; ++a) {
      const bool lateral = a == kX || a == kYaw || a == kRoll;
      target[a] = (lateral ? side : 1.0f) * shape[a] * amount + vary(rng_);
    }
  }
  const float duration = kPostureSeconds * mode.slowness;
  posture_.MoveTo(target, kShortMoveShare * duration, duration);
  posture_.timer = Jitter(posture_interval_);
}

HeadOffset NaturalHeadMovementEffect::Update(float dt,
                                             const SPF_TruckData &truck,
                                             const SPF_Controls &controls) {
  for (size_t o = 0; o < tremor_phase_.size(); ++o)
    tremor_phase_[o] =
        math::WrapNoisePhase(tremor_phase_[o] + dt * kTremorRate[o]);

  // Driving off, the driver sits up and looks at the road at once.
  stopped_for_ =
      telemetry::SpeedKmh(truck) < kIdleSpeedKmh ? stopped_for_ + dt : 0.0f;
  const bool idle = stopped_for_ >= kIdleDelaySeconds;
  if (idle != idle_) {
    idle_ = idle;
    DrawPosture();
    posture_.duration = idle ? kRelaxSeconds : kFocusSeconds;
  }

  micro_.Advance(dt);
  posture_.Advance(dt);
  micro_.timer -= dt;
  if (micro_.timer <= 0.0f) {
    DrawMicro();
    micro_.timer = Jitter(kMicroInterval);
  }
  posture_.timer -= dt;
  if (posture_.timer <= 0.0f)
    DrawPosture();

  float axis[kAxes];
  for (int a = 0; a < kAxes; ++a) {
    float tremor = 0.0f;
    for (size_t o = 0; o < tremor_phase_.size(); ++o) {
      const uint32_t seed = kTremorSeed +
                            static_cast<uint32_t>(a) * kAxisSeedStep +
                            static_cast<uint32_t>(o) * kOctaveSeedStep;
      tremor +=
          kOctaveWeight[o] *
          math::GradientNoise1D(static_cast<float>(tremor_phase_[o]), seed);
    }
    const float drift =
        tremor * kTremorNorm * kTremorAmplitude[a] * tremor_intensity_ +
        micro_.Value(a) * kMicroRange[a] * micro_intensity_;
    const float posture = posture_.Value(a) * kPostureRange[a];
    axis[a] = drift + posture * posture_intensity_;
  }

  // Steering is positive to the left, roll positive to the right.
  const float steering =
      std::clamp(controls.effectiveInput.steering, -1.0f, 1.0f);
  const float tilt = std::copysign(std::sqrt(std::fabs(steering)), steering);
  axis[kRoll] +=
      steering_roll_.Update(-kSteeringTiltDeg * tilt * steering_tilt_, dt);

  // A right-hand drive truck's camera mirrors X and roll, not yaw (seen in
  // game).
  const float camera_x = right_hand_drive_ ? -1.0f : 1.0f;
  const Vec3 neck =
      NeckPivot(axis[kYaw] * math::kDegToRad, axis[kPitch] * math::kDegToRad,
                axis[kRoll] * math::kDegToRad);

  HeadOffset offset;
  offset.pos_x = camera_x * (axis[kX] + neck.x);
  offset.pos_y = axis[kY] + neck.y;
  offset.pos_z = axis[kZ] + neck.z;
  offset.yaw = axis[kYaw];
  offset.pitch = axis[kPitch];
  offset.roll = camera_x * axis[kRoll];
  return offset;
}

} // namespace motioncab
