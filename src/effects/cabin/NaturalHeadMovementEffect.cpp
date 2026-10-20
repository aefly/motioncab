#include "NaturalHeadMovementEffect.hpp"

#include "effects/Telemetry.hpp"
#include "math/Noise.hpp"
#include "math/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>

namespace motioncab {

namespace {
// The per-axis values are X, Y, Z in meters, then yaw, pitch, roll in
// degrees, at intensity 1.

constexpr double kSwayRate[] = {0.15, 0.4}; // noise cells per second
constexpr float kOctaveWeight[] = {1.0f, 0.35f};
constexpr float kSwayNorm = 1.0f / (kOctaveWeight[0] + kOctaveWeight[1]);
constexpr float kSwayAmplitude[] = {0.002f, 0.0015f, 0.002f,
                                    0.35f,  0.25f,   0.2f};

constexpr float kMicroRange[] = {0.003f, 0.002f, 0.004f, 1.0f, 0.7f, 0.6f};
// The smallest can be quick now that they wait for the postures: at 0.6,
// beside a slow posture change, they twitched.
constexpr float kMicroShortShare = 0.3f;
// A micro-adjustment waits for a posture change to settle, then this long
// (jittered): drawn on its own, it sometimes ran against the posture and the
// head hesitated midway. Not straight after, or the two read as a sequence.
constexpr float kMicroAfterPosture = 1.5f;

// Leaning toward the windshield takes the head well forward.
constexpr float kPostureRange[] = {0.012f, 0.012f, 0.035f, 5.0f, 3.0f, 3.0f};
constexpr float kPostureVariation = 0.1f;
// A posture's target is in shares of kPostureRange at this intensity, the
// setting's default, so a stronger posture is wider without taking longer.
constexpr float kPostureUnit =
    settings::Default("settings.cabin.natural_head_movement.posture_intensity");

// How long a move with no distance to go takes, as a share of a full one.
// In between, it grows with the square root of the distance, as a person's
// does: a short move is brief, a long one mostly faster.
constexpr float kShortMoveShare = 0.25f;
// So no two moves of the same size take the same time.
constexpr float kPaceVariation = 0.2f;

// The head leads, the body follows: the rotations take this share of the
// move, the translations start this share in and run on past its end, to
// this share of it. Kept overlapping, so the head and the body read as one
// change: further apart, the body's lean came as a second move.
constexpr float kHeadShare = 0.8f;
constexpr float kBodyDelayShare = 0.1f;
constexpr float kBodyEndShare = 1.2f;
// A rotation goes this far past its target, at this share of its part, then
// settles back: stopping dead on it looked mechanical.
constexpr float kOvershoot = 0.1f;
constexpr float kOvershootAt = 0.7f;
// A head speeds up quicker than it slows down: its top speed comes at this
// share of the move, or a little before, not halfway as with a plain
// minimum-jerk curve.
constexpr float kPeakAt = 0.4f;
constexpr float kWarp = (0.5f - kPeakAt) / (kPeakAt * (1.0f - kPeakAt));

// Moves come at random, like a Poisson process, rather than every interval
// give or take: a steady rhythm shows. Never sooner or later than these
// shares of the mean.
constexpr float kIntervalFloor = 0.25f;
constexpr float kIntervalCap = 3.0f;

// Even held, a posture is never frozen: the body drifts slowly around it.
constexpr double kDriftRate = 0.05; // noise cells per second, ~20 s
constexpr float kDriftAmplitude[] = {0.0026f, 0.00195f, 0.0052f,
                                     0.52f,   0.39f,    0.325f};
constexpr uint32_t kDriftSeed = 0x4E484D44u;

// A micro-adjustment is led by one rotation, the other axes only follow a
// little, so it reads as one gesture. The lead's size is Laplace-distributed,
// with this standard deviation: mostly tiny, now and then one goes farther.
constexpr double kMicroLeadOdds[] = {0.5, 0.35, 0.15}; // yaw, pitch, roll
constexpr float kMicroSpread = 0.45f;
constexpr float kMicroFollowSpread = 0.15f;
// The neck bends to the side it turns to: a yaw left brings a little roll
// left. In shares of each axis' kMicroRange.
constexpr float kMicroYawRoll = 0.25f;

// Body shapes rather than random axes, so a posture looks like something a
// driver would do. Each is on one side: X, yaw and roll flip for the other.
using PostureShape = float[6];
// The tilts stay well under the full roll: with the idle scale on top, a
// whole one, plus a micro-adjustment the same way, laid the head over.
constexpr PostureShape kIdlePostures[] = {
    {0.0f, -1.0f, 0.3f, 0.0f, -0.2f, 0.0f},  // slouched
    {0.8f, -0.4f, 0.0f, -0.1f, 0.0f, 0.6f},  // leaning on the door
    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.6f},    // head tilted
    {0.0f, 0.2f, 0.0f, 0.0f, 0.6f, 0.5f},    // stretching the neck
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
    {0.4f, -0.3f, 0.0f, 0.0f, 0.0f, 0.3f},   // leaning on the armrest
    {0.0f, 0.0f, 0.0f, 0.4f, 0.0f, 0.1f},    // glancing at the scenery
};
// What a mode plays, at the settings' defaults. The rates and durations are
// in seconds, the intensities in shares of the base amplitudes.
struct ModeValues {
  float sway;
  float sway_pace; // share of kSwayRate
  float micro;
  float micro_interval; // on average
  float micro_seconds;  // a move across the whole range
  float posture;
  float posture_interval; // on average
  float posture_seconds;  // a move across the whole range
  float change_seconds;   // the least the mode's first posture takes
  float neutral_odds;     // that a posture is the neutral one
  float drift;            // share of kDriftAmplitude
  float roll;             // every layer's roll, the turn's tilt aside
  std::span<const PostureShape> shapes;
};

// In Mode's order, tuned in game.
constexpr ModeValues kModes[] = {
    // Stopped a while, relaxed: wide, slow postures, rarely back to neutral,
    // the body still.
    {5.0f, 0.8f, 1.0f, 15.0f, 5.0f, 2.2f, 25.0f, 6.5f, 3.0f, 0.2f, 0.5f, 1.0f,
     kIdlePostures},
    // Slow, maneuvering: upright and focused, the body steady.
    {6.0f, 1.0f, 0.9f, 10.0f, 5.0f, 1.9f, 25.0f, 5.0f, 2.0f, 0.4f, 0.3f, 1.0f,
     kManeuveringPostures},
    // In town, focused on the road, often back to neutral.
    {8.0f, 1.0f, 1.0f, 10.0f, 5.0f, 1.85f, 25.0f, 8.0f, 3.0f, 0.4f, 1.0f, 1.2f,
     kTownPostures},
    // On the highway, settled: rarely back to neutral.
    {8.0f, 1.0f, 1.0f, 8.0f, 5.0f, 2.0f, 25.0f, 6.0f, 3.0f, 0.25f, 1.0f, 1.2f,
     kHighwayPostures},
};

const ModeValues &ModeOf(int mode) { return kModes[mode]; }

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
// Reversing, the driver watches the mirrors: a head moving on its own only
// gets in the way, so the effect fades out until the truck leaves reverse.
constexpr float kReverseFadeSeconds = 1.0f;
// How long the sway, micro-adjustments, drift and roll take to change with the
// mode.
constexpr float kModeBlendSeconds = 2.0f;
// A change of mode draws a posture at once, unless one came this recently:
// it waits for the rest of this then. Backing into a dock goes through
// maneuvering and stopped every few seconds, and the head changed posture at
// each.
constexpr float kModePostureCooldown = 10.0f;

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
constexpr uint32_t kSwaySeed = 0x4E484D31u;
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
  if (s <= 0.0f || s >= 1.0f)
    return 0.0f;
  const float rest = 1.0f - s;
  return 30.0f * s * s * rest * rest;
}

// Out to 1 + kOvershoot, then back to 1, both legs minimum-jerk: the head
// comes to rest just past the target and eases back onto it, with no jolt.
float Overshoot(float s) {
  if (s < kOvershootAt)
    return (1.0f + kOvershoot) * math::SmootherStep(s / kOvershootAt);
  return 1.0f + kOvershoot -
         kOvershoot *
             math::SmootherStep((s - kOvershootAt) / (1.0f - kOvershootAt));
}
float OvershootSlope(float s) {
  if (s < kOvershootAt)
    return (1.0f + kOvershoot) * SmootherStepSlope(s / kOvershootAt) /
           kOvershootAt;
  return -kOvershoot *
         SmootherStepSlope((s - kOvershootAt) / (1.0f - kOvershootAt)) /
         (1.0f - kOvershootAt);
}

// Remaps a move's progress so that SmootherStep peaks at kPeakAt. Its ends
// keep a finite slope, so the move still starts and stops at rest.
float Warp(float s) { return s + kWarp * s * (1.0f - s); }
float WarpSlope(float s) { return 1.0f + kWarp * (1.0f - 2.0f * s); }

// Yaw, pitch and roll come after X, Y and Z in Axis.
bool IsRotation(int axis) { return axis >= 3; }
} // namespace

float NaturalHeadMovementEffect::Layer::Start(int axis) const {
  return IsRotation(axis) ? 0.0f : kBodyDelayShare * duration;
}

float NaturalHeadMovementEffect::Layer::End(int axis) const {
  return (IsRotation(axis) ? kHeadShare : kBodyEndShare) * duration;
}

float NaturalHeadMovementEffect::Layer::Value(int axis) const {
  const float start = Start(axis), end = End(axis);
  const float s =
      Warp(std::clamp((elapsed - start) / (end - start), 0.0f, 1.0f));
  const float eased = IsRotation(axis) ? Overshoot(s) : math::SmootherStep(s);
  // The previous move's speed fades out from the start, delay or not.
  return math::Lerp(from[axis], to[axis], eased) +
         velocity[axis] * end * CarryOver(std::min(elapsed / end, 1.0f));
}

float NaturalHeadMovementEffect::Layer::Velocity(int axis) const {
  const float start = Start(axis), end = End(axis);
  const float progress =
      std::clamp((elapsed - start) / (end - start), 0.0f, 1.0f);
  const float s = Warp(progress);
  const float slope =
      (IsRotation(axis) ? OvershootSlope(s) : SmootherStepSlope(s)) *
      WarpSlope(progress);
  return (to[axis] - from[axis]) * slope / (end - start) +
         velocity[axis] * CarryOverSlope(std::min(elapsed / end, 1.0f));
}

float NaturalHeadMovementEffect::Layer::Remaining() const {
  return kBodyEndShare * duration - elapsed;
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
  duration =
      math::Lerp(min_seconds, max_seconds, std::sqrt(std::min(distance, 1.0f)));
}

void NaturalHeadMovementEffect::Layer::Advance(float dt) {
  elapsed = std::min(elapsed + dt, kBodyEndShare * duration);
}

void NaturalHeadMovementEffect::Layer::Reset() {
  from.fill(0.0f);
  to.fill(0.0f);
  velocity.fill(0.0f);
  elapsed = kBodyEndShare * duration;
}

NaturalHeadMovementEffect::NaturalHeadMovementEffect(
    SPF_Config_API *config_api, SPF_Config_Handle *config_handle)
    : ConfigurableEffect(config_api, config_handle, "cabin",
                         "natural_head_movement"),
      rng_(std::random_device{}()) {
  steering_roll_.SetTimeConstant(kSteeringTiltSeconds);
  sway_gain_.SetTimeConstant(kModeBlendSeconds);
  sway_pace_.SetTimeConstant(kModeBlendSeconds);
  micro_gain_.SetTimeConstant(kModeBlendSeconds);
  drift_gain_.SetTimeConstant(kModeBlendSeconds);
  roll_gain_.SetTimeConstant(kModeBlendSeconds);
  Reset();
}

void NaturalHeadMovementEffect::LoadSettings() {
  // Default() only takes a literal, at compile time.
  constexpr float kSway =
      settings::Default("settings.cabin.natural_head_movement.sway_intensity");
  constexpr float kMicro =
      settings::Default("settings.cabin.natural_head_movement.micro_intensity");
  constexpr float kMicroInterval =
      settings::Default("settings.cabin.natural_head_movement.micro_interval");
  constexpr float kPosture = settings::Default(
      "settings.cabin.natural_head_movement.posture_intensity");
  constexpr float kInterval = settings::Default(
      "settings.cabin.natural_head_movement.posture_interval");
  constexpr float kRoll =
      settings::Default("settings.cabin.natural_head_movement.roll_scale");
  sway_share_ = Float("sway_intensity", kSway) / kSway;
  micro_share_ = Float("micro_intensity", kMicro) / kMicro;
  micro_interval_share_ =
      Float("micro_interval", kMicroInterval) / kMicroInterval;
  posture_share_ = Float("posture_intensity", kPosture) / kPosture;
  interval_share_ = Float("posture_interval", kInterval) / kInterval;
  roll_share_ = Float("roll_scale", kRoll) / kRoll;
  steering_tilt_ = Float("steering_tilt", steering_tilt_);
}

void NaturalHeadMovementEffect::OnTruckConstantsChanged(
    const SPF_TruckConstants &constants) {
  right_hand_drive_ = telemetry::IsRightHandDrive(constants);
}

void NaturalHeadMovementEffect::Reset() {
  // Not the sway's phases: the head picks up somewhere new.
  micro_.Reset();
  const ModeValues &town = ModeOf(static_cast<int>(Mode::kTown));
  micro_.timer = Interval(town.micro_interval * micro_interval_share_);
  micro_waited_ = false;
  posture_.Reset();
  posture_.timer = Interval(town.posture_interval * interval_share_);
  posture_shape_ = nullptr;
  posture_side_ = 0.0f;
  stopped_for_ = 0.0f;
  since_posture_ = kModePostureCooldown;
  mode_ = Mode::kTown;
  mode_known_ = false;
  steering_roll_.Reset();
}

float NaturalHeadMovementEffect::Jitter(float mean) {
  std::uniform_real_distribution<float> jitter(0.5f, 1.5f);
  return mean * jitter(rng_);
}

float NaturalHeadMovementEffect::Interval(float mean) {
  // Exponential above the floor, so the mean stays `mean` (the cap only trims
  // the rare long tail).
  std::exponential_distribution<float> wait(1.0f);
  const float share = kIntervalFloor + (1.0f - kIntervalFloor) * wait(rng_);
  return mean * std::min(share, kIntervalCap);
}

float NaturalHeadMovementEffect::Pace() {
  std::uniform_real_distribution<float> pace(1.0f - kPaceVariation,
                                             1.0f + kPaceVariation);
  return pace(rng_);
}

NaturalHeadMovementEffect::Mode
NaturalHeadMovementEffect::Assess(const SPF_TruckData &truck, float dt) {
  const float speed = telemetry::SpeedKmh(truck);
  stopped_for_ = speed < kIdleSpeedKmh ? stopped_for_ + dt : 0.0f;
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
  // Laplace: an exponential size, either way, with the same spread.
  std::exponential_distribution<float> lead(std::numbers::sqrt2_v<float> /
                                            kMicroSpread);
  std::uniform_int_distribution<int> sign(0, 1);
  std::discrete_distribution<int> pick(std::begin(kMicroLeadOdds),
                                       std::end(kMicroLeadOdds));
  Pose target;
  for (float &axis : target)
    axis = follow(rng_);
  target[kYaw + pick(rng_)] = sign(rng_) ? lead(rng_) : -lead(rng_);
  // Roll positive right, yaw positive left.
  target[kRoll] -= kMicroYawRoll * target[kYaw];
  for (float &axis : target)
    axis = std::clamp(axis, -1.0f, 1.0f);
  const ModeValues &mode = ModeOf(static_cast<int>(mode_));
  const float pace = Pace();
  micro_.MoveTo(target, kMicroShortShare * mode.micro_seconds * pace,
                mode.micro_seconds * pace);
  micro_.timer = Interval(mode.micro_interval * micro_interval_share_);
  micro_waited_ = false;
}

void NaturalHeadMovementEffect::DrawPosture(float seconds) {
  const ModeValues &mode = ModeOf(static_cast<int>(mode_));
  const std::span<const PostureShape> shapes = mode.shapes;
  since_posture_ = 0.0f;
  Pose target{};
  std::uniform_real_distribution<float> unit(0.0f, 1.0f);
  const PostureShape *shape = nullptr;
  float side = unit(rng_) < 0.5f ? -1.0f : 1.0f;
  if (unit(rng_) >= mode.neutral_odds) {
    // Not the one held: the head would only wobble in place.
    std::uniform_int_distribution<size_t> pick(0, shapes.size() - 1);
    do {
      shape = &shapes[pick(rng_)];
    } while (shapes.size() > 1 && *shape == posture_shape_);
    // A lean to the other side goes back to neutral first.
    if (posture_shape_ && IsSided(*shape) && IsSided(posture_shape_) &&
        side != posture_side_)
      shape = nullptr;
  }
  posture_.timer = Interval(mode.posture_interval * interval_share_);
  // Already neutral, there's nothing to move, and a move would only hold the
  // micro-adjustments back.
  if (!shape && !posture_shape_)
    return;
  posture_shape_ = shape ? *shape : nullptr;
  posture_side_ = side;
  if (shape) {
    std::uniform_real_distribution<float> strength(0.5f, 1.0f);
    std::uniform_real_distribution<float> vary(-kPostureVariation,
                                               kPostureVariation);
    const float amount = strength(rng_) * mode.posture / kPostureUnit;
    for (int a = 0; a < kAxes; ++a) {
      const bool lateral = a == kX || a == kYaw || a == kRoll;
      target[a] = (lateral ? side : 1.0f) * (*shape)[a] * amount + vary(rng_);
    }
  }
  const float duration = mode.posture_seconds * Pace();
  // A change of mode sets off at once, but a long way back still takes its
  // time: in a fixed 1.5 s, leaving a look out the window snapped the head.
  if (seconds > 0.0f)
    posture_.MoveTo(target, seconds, std::max(seconds, duration));
  else
    posture_.MoveTo(target, kShortMoveShare * duration, duration);
}

HeadOffset
NaturalHeadMovementEffect::Update(float dt, const SPF_TruckData &truck,
                                  const SPF_Controls & /*controls*/) {
  // Driving off or stopping, the driver's posture changes at once.
  const Mode mode = Assess(truck, dt);
  const float presence_target = truck.gear < 0 ? 0.0f : 1.0f;
  if (!mode_known_) {
    mode_known_ = true;
    mode_ = mode;
    presence_ = presence_target;
    const ModeValues &start = ModeOf(static_cast<int>(mode_));
    sway_gain_.Reset(start.sway);
    sway_pace_.Reset(start.sway_pace);
    micro_gain_.Reset(start.micro);
    drift_gain_.Reset(start.drift);
    roll_gain_.Reset(start.roll);
  } else if (mode != mode_) {
    mode_ = mode;
    if (since_posture_ >= kModePostureCooldown)
      DrawPosture(ModeOf(static_cast<int>(mode)).change_seconds);
    else
      posture_.timer =
          std::min(posture_.timer, kModePostureCooldown - since_posture_);
  }
  since_posture_ += dt;
  const float step = dt / kReverseFadeSeconds;
  presence_ = std::clamp(presence_target, presence_ - step, presence_ + step);
  const ModeValues &profile = ModeOf(static_cast<int>(mode_));
  const float sway_gain = sway_gain_.Update(profile.sway, dt) * sway_share_;
  const float sway_pace = sway_pace_.Update(profile.sway_pace, dt);
  const float micro_gain = micro_gain_.Update(profile.micro, dt) * micro_share_;
  const float drift_gain = drift_gain_.Update(profile.drift, dt);
  const float roll_gain = roll_gain_.Update(profile.roll, dt) * roll_share_;

  for (size_t o = 0; o < sway_phase_.size(); ++o)
    sway_phase_[o] =
        math::WrapNoisePhase(sway_phase_[o] + dt * kSwayRate[o] * sway_pace);
  drift_phase_ = math::WrapNoisePhase(drift_phase_ + dt * kDriftRate);

  micro_.Advance(dt);
  posture_.Advance(dt);
  posture_.timer -= dt;
  if (posture_.timer <= 0.0f)
    DrawPosture();
  // One already under way when the posture sets off finishes on its own.
  micro_.timer -= dt;
  if (micro_.timer <= 0.0f) {
    if (!micro_waited_ && posture_.Remaining() > 0.0f) {
      micro_.timer = posture_.Remaining() + Jitter(kMicroAfterPosture);
      micro_waited_ = true;
    } else
      DrawMicro();
  }

  float axis[kAxes];
  for (int a = 0; a < kAxes; ++a) {
    float sway = 0.0f;
    for (size_t o = 0; o < sway_phase_.size(); ++o) {
      const uint32_t seed = kSwaySeed +
                            static_cast<uint32_t>(a) * kAxisSeedStep +
                            static_cast<uint32_t>(o) * kOctaveSeedStep;
      sway += kOctaveWeight[o] *
              math::GradientNoise1D(static_cast<float>(sway_phase_[o]), seed);
    }
    const float drift = sway * kSwayNorm * kSwayAmplitude[a] * sway_gain +
                        micro_.Value(a) * kMicroRange[a] * micro_gain;
    const float settle =
        math::GradientNoise1D(static_cast<float>(drift_phase_),
                              kDriftSeed +
                                  static_cast<uint32_t>(a) * kAxisSeedStep) *
        kDriftAmplitude[a] * drift_gain;
    const float posture = posture_.Value(a) * kPostureRange[a];
    axis[a] = drift + posture * kPostureUnit * posture_share_ + settle;
  }
  axis[kRoll] *= roll_gain;

  // Turning left accelerates the truck toward -X. Roll is positive right.
  const float lateral = -truck.local_linear_acceleration.x;
  const float turn = std::clamp((std::fabs(lateral) - kTiltDeadZone) /
                                    (kFullTiltAccel - kTiltDeadZone),
                                0.0f, 1.0f);
  const float tilt = std::copysign(std::sqrt(turn), lateral);
  axis[kRoll] +=
      steering_roll_.Update(-kSteeringTiltDeg * tilt * steering_tilt_, dt);

  // A right-hand drive truck's camera mirrors X, not roll or yaw.
  const float camera_x = right_hand_drive_ ? -1.0f : 1.0f;
  const Vec3 neck =
      NeckPivot(axis[kYaw] * math::kDegToRad, axis[kPitch] * math::kDegToRad,
                axis[kRoll] * math::kDegToRad);

  const float weight = math::SmoothStep(presence_);
  HeadOffset offset;
  offset.pos_x = weight * camera_x * (axis[kX] + neck.x);
  offset.pos_y = weight * (axis[kY] + neck.y);
  offset.pos_z = weight * (axis[kZ] + neck.z);
  offset.yaw = weight * axis[kYaw];
  offset.pitch = weight * axis[kPitch];
  offset.roll = weight * axis[kRoll];
  return offset;
}

} // namespace motioncab
