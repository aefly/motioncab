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

// A micro-adjustment is led by one rotation, the other axes only follow a
// little, so it reads as one gesture. Most are small, now and then one goes
// farther.
constexpr double kMicroLeadOdds[] = {0.5, 0.35, 0.15}; // yaw, pitch, roll
constexpr float kMicroSpread = 0.45f;
constexpr float kMicroFollowSpread = 0.15f;
// The neck bends to the side it turns to: a yaw left brings a little roll
// left. In shares of each axis' kMicroRange.
constexpr float kMicroYawRoll = 0.25f;

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
constexpr PostureShape kManeuveringPostures[] = {
    {0.0f, 0.3f, -0.6f, 0.0f, -0.2f, 0.0f}, // leaning in, watching close
    {0.0f, 0.6f, 0.0f, 0.0f, 0.2f, 0.0f},   // sitting up tall
    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f},   // head slightly tilted
};
constexpr PostureShape kTownPostures[] = {
    {0.0f, -0.2f, -1.0f, 0.0f, -0.3f, 0.0f}, // forward, toward the road
    {0.0f, 0.6f, 0.0f, 0.0f, 0.2f, 0.0f},    // sitting up tall
    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f},    // head slightly tilted
    {0.5f, -0.2f, 0.0f, 0.0f, 0.0f, 0.3f},   // leaning a little on one side
};
constexpr PostureShape kHighwayPostures[] = {
    {0.0f, -0.4f, 0.3f, 0.0f, 0.0f, 0.0f},   // settled back in the seat
    {0.0f, -0.2f, -0.5f, 0.0f, -0.2f, 0.0f}, // forward, toward the road
    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.4f},    // head slightly tilted
};
// Sitting up to see down the mirrors, the head kept straight.
constexpr PostureShape kReversingPostures[] = {
    {0.0f, 1.0f, 0.0f, 0.0f, 0.3f, 0.0f}, // sitting up, neck stretched
    {0.0f, 0.6f, 0.2f, 0.0f, 0.5f, 0.0f}, // chin up
    {0.0f, 0.8f, 0.0f, 0.0f, 0.0f, 0.3f}, // sitting up, head tilted
};

// Each is a share of the base values above.
struct ModeProfile {
  std::span<const PostureShape> shapes;
  float neutral_odds;
  float scale;          // of kPostureRange
  float slowness;       // of kPostureSeconds
  float hold;           // of the posture interval setting
  float change_seconds; // to the mode's first posture, whatever the distance
  float micro;          // of kMicroRange
  float micro_interval; // of kMicroInterval
  float tremor;         // of kTremorAmplitude
  float tremor_pace;    // of kTremorRate
};
// In Mode's order.
constexpr ModeProfile kModes[] = {
    // Stopped a while, relaxed: wider, slower, rarely back to neutral.
    {kIdlePostures, 0.2f, 1.5f, 1.3f, 1.0f, 3.0f, 1.0f, 1.0f, 1.0f, 0.8f},
    // Slow, maneuvering: upright and busy, shorter holds.
    {kManeuveringPostures, 0.4f, 0.7f, 0.9f, 0.7f, 2.0f, 1.1f, 0.6f, 1.0f,
     1.1f},
    // In town, focused on the road: small postures, often back to neutral.
    {kTownPostures, 1.0f / 3.0f, 0.6f, 1.0f, 1.0f, 3.0f, 1.0f, 1.0f, 1.0f,
     1.0f},
    // On the highway, settled: smaller, slower, held longer, a steadier head.
    {kHighwayPostures, 0.4f, 0.45f, 1.2f, 1.4f, 3.0f, 0.7f, 1.4f, 0.8f, 0.9f},
    // Backing up, alert: quick, short holds.
    {kReversingPostures, 0.25f, 0.7f, 0.8f, 0.5f, 1.5f, 1.2f, 0.5f, 1.0f, 1.2f},
};

const ModeProfile &Profile(int mode) { return kModes[mode]; }

// Whether the shape leans or turns one way (X, yaw or roll), so that it has a
// side.
bool IsSided(const float *shape) {
  return shape[0] != 0.0f || shape[3] != 0.0f || shape[5] != 0.0f;
}

constexpr float kIdleSpeedKmh = 2.0f;
constexpr float kIdleDelaySeconds = 5.0f;
// Each with a lower way out, so a speed around the line doesn't flip the
// mode back and forth.
constexpr float kTownEnterKmh = 30.0f;
constexpr float kTownExitKmh = 25.0f;
constexpr float kHighwayEnterKmh = 70.0f;
constexpr float kHighwayExitKmh = 60.0f;
// How long the tremor and the micro-adjustments take to change with the mode.
constexpr float kModeBlendSeconds = 2.0f;

// Follows how hard the turn is taken, the lateral acceleration, rather than
// the wheel: a tight turn at walking pace takes full lock but barely leans
// anyone, a sweeping bend on the highway does. Full tilt at kFullTiltAccel,
// along the square root, so a gentle bend already tilts a little. The dead
// zone keeps road bumps out.
constexpr float kSteeringTiltDeg = 3.0f;
constexpr float kSteeringTiltSeconds = 0.4f;
constexpr float kTiltDeadZone = 0.2f;  // m/s²
constexpr float kFullTiltAccel = 3.0f; // m/s²

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

// A minimum-jerk move (SmootherStep) with a starting speed: this curve adds
// that speed at the start and fades it out with no jolt, ending still.
float CarryOver(float s) {
  const float s3 = s * s * s;
  return s - 6.0f * s3 + 8.0f * s3 * s - 3.0f * s3 * s * s;
}
float CarryOverSlope(float s) {
  const float s2 = s * s;
  return 1.0f - 18.0f * s2 + 32.0f * s2 * s - 15.0f * s2 * s2;
}
float SmootherStepSlope(float s) {
  const float rest = 1.0f - s;
  return 30.0f * s * s * rest * rest;
}
} // namespace

float NaturalHeadMovementEffect::Layer::Value(int axis) const {
  const float s = std::min(elapsed / duration, 1.0f);
  return math::Lerp(from[axis], to[axis], math::SmootherStep(s)) +
         velocity[axis] * duration * CarryOver(s);
}

float NaturalHeadMovementEffect::Layer::Velocity(int axis) const {
  const float s = std::min(elapsed / duration, 1.0f);
  return (to[axis] - from[axis]) * SmootherStepSlope(s) / duration +
         velocity[axis] * CarryOverSlope(s);
}

void NaturalHeadMovementEffect::Layer::MoveTo(const Pose &target,
                                              float min_seconds,
                                              float max_seconds) {
  float distance = 0.0f;
  for (int a = 0; a < kAxes; ++a) {
    const float speed = Velocity(a);
    from[a] = Value(a);
    velocity[a] = speed;
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
  velocity.fill(0.0f);
  elapsed = duration;
}

NaturalHeadMovementEffect::NaturalHeadMovementEffect(
    SPF_Config_API *config_api, SPF_Config_Handle *config_handle)
    : ConfigurableEffect(config_api, config_handle, "cabin",
                         "natural_head_movement"),
      rng_(std::random_device{}()) {
  steering_roll_.SetTimeConstant(kSteeringTiltSeconds);
  tremor_gain_.SetTimeConstant(kModeBlendSeconds);
  tremor_pace_.SetTimeConstant(kModeBlendSeconds);
  micro_gain_.SetTimeConstant(kModeBlendSeconds);
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
  posture_shape_ = nullptr;
  posture_side_ = 0.0f;
  stopped_for_ = 0.0f;
  mode_ = Mode::kTown;
  const ModeProfile &town = Profile(static_cast<int>(mode_));
  tremor_gain_.Reset(town.tremor);
  tremor_pace_.Reset(town.tremor_pace);
  micro_gain_.Reset(town.micro);
  steering_roll_.Reset();
}

float NaturalHeadMovementEffect::Jitter(float mean) {
  std::uniform_real_distribution<float> jitter(0.5f, 1.5f);
  return mean * jitter(rng_);
}

NaturalHeadMovementEffect::Mode
NaturalHeadMovementEffect::Assess(const SPF_TruckData &truck, float dt) {
  const float speed = telemetry::SpeedKmh(truck);
  stopped_for_ = speed < kIdleSpeedKmh ? stopped_for_ + dt : 0.0f;
  // Stopped in reverse too: about to back up.
  if (truck.gear < 0)
    return Mode::kReversing;
  if (stopped_for_ >= kIdleDelaySeconds)
    return Mode::kStopped;
  const bool highway = mode_ == Mode::kHighway;
  if (speed > (highway ? kHighwayExitKmh : kHighwayEnterKmh))
    return Mode::kHighway;
  const bool town = mode_ == Mode::kTown || highway;
  return speed > (town ? kTownExitKmh : kTownEnterKmh) ? Mode::kTown
                                                       : Mode::kManeuvering;
}

void NaturalHeadMovementEffect::DrawMicro() {
  std::normal_distribution<float> follow(0.0f, kMicroFollowSpread);
  std::normal_distribution<float> lead(0.0f, kMicroSpread);
  std::discrete_distribution<int> pick(std::begin(kMicroLeadOdds),
                                       std::end(kMicroLeadOdds));
  Pose target;
  for (float &axis : target)
    axis = follow(rng_);
  target[kYaw + pick(rng_)] = lead(rng_);
  // Roll positive right, yaw positive left.
  target[kRoll] -= kMicroYawRoll * target[kYaw];
  for (float &axis : target)
    axis = std::clamp(axis, -1.0f, 1.0f);
  micro_.MoveTo(target, kShortMoveShare * kMicroSeconds, kMicroSeconds);
  micro_.timer =
      Jitter(kMicroInterval * Profile(static_cast<int>(mode_)).micro_interval);
}

void NaturalHeadMovementEffect::DrawPosture() {
  const ModeProfile &mode = Profile(static_cast<int>(mode_));
  Pose target{};
  std::uniform_real_distribution<float> unit(0.0f, 1.0f);
  const PostureShape *shape = nullptr;
  float side = unit(rng_) < 0.5f ? -1.0f : 1.0f;
  if (unit(rng_) >= mode.neutral_odds) {
    // Not the one held: the head would only wobble in place.
    std::uniform_int_distribution<size_t> pick(0, mode.shapes.size() - 1);
    do {
      shape = &mode.shapes[pick(rng_)];
    } while (mode.shapes.size() > 1 && *shape == posture_shape_);
    // A lean to the other side goes back to neutral first.
    if (posture_shape_ && IsSided(*shape) && IsSided(posture_shape_) &&
        side != posture_side_)
      shape = nullptr;
  }
  posture_shape_ = shape ? *shape : nullptr;
  posture_side_ = side;
  if (shape) {
    std::uniform_real_distribution<float> strength(0.5f, 1.0f);
    std::uniform_real_distribution<float> vary(-kPostureVariation,
                                               kPostureVariation);
    const float amount = strength(rng_) * mode.scale;
    for (int a = 0; a < kAxes; ++a) {
      const bool lateral = a == kX || a == kYaw || a == kRoll;
      target[a] = (lateral ? side : 1.0f) * (*shape)[a] * amount + vary(rng_);
    }
  }
  const float duration = kPostureSeconds * mode.slowness;
  posture_.MoveTo(target, kShortMoveShare * duration, duration);
  posture_.timer = Jitter(posture_interval_ * mode.hold);
}

HeadOffset
NaturalHeadMovementEffect::Update(float dt, const SPF_TruckData &truck,
                                  const SPF_Controls & /*controls*/) {
  // Driving off or shifting into reverse, the driver's posture changes at
  // once.
  const Mode mode = Assess(truck, dt);
  if (mode != mode_) {
    mode_ = mode;
    DrawPosture();
    posture_.duration = Profile(static_cast<int>(mode)).change_seconds;
  }
  const ModeProfile &profile = Profile(static_cast<int>(mode_));
  const float tremor_gain = tremor_gain_.Update(profile.tremor, dt);
  const float tremor_pace = tremor_pace_.Update(profile.tremor_pace, dt);
  const float micro_gain = micro_gain_.Update(profile.micro, dt);

  for (size_t o = 0; o < tremor_phase_.size(); ++o)
    tremor_phase_[o] = math::WrapNoisePhase(tremor_phase_[o] +
                                            dt * kTremorRate[o] * tremor_pace);

  micro_.Advance(dt);
  posture_.Advance(dt);
  micro_.timer -= dt;
  if (micro_.timer <= 0.0f)
    DrawMicro();
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
        tremor * kTremorNorm * kTremorAmplitude[a] * tremor_gain *
            tremor_intensity_ +
        micro_.Value(a) * kMicroRange[a] * micro_gain * micro_intensity_;
    const float posture = posture_.Value(a) * kPostureRange[a];
    axis[a] = drift + posture * posture_intensity_;
  }

  // Turning left accelerates the truck toward -X. Roll is positive right.
  const float lateral = -truck.local_linear_acceleration.x;
  const float turn = std::clamp((std::fabs(lateral) - kTiltDeadZone) /
                                    (kFullTiltAccel - kTiltDeadZone),
                                0.0f, 1.0f);
  const float tilt = std::copysign(std::sqrt(turn), lateral);
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
