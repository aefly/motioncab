#include "SuspensionEffect.hpp"

#include "effects/Telemetry.hpp"
#include "math/Units.hpp"

#include <algorithm>
#include <cmath>

namespace motioncab {

namespace {
// Leaves a small rebound after a bump. Any lighter and the road's constant
// small jolts kept the seat bouncing at its own frequency on flat road. (The
// stiffness comes from Reactivity: ~1.3 Hz at the default, typical of an air
// seat.)
constexpr float kSeatDamping = 0.6f;
// At speed the raw accelerations carry a constant fast jitter that a real
// cushion swallows, and that RoadIrregularityEffect already plays.
// Undulations and bumps are slower and get through.
constexpr float kAccelFilterTimeConstant = 0.08f;
// What's left below this (m/s^2) is the flat road's noise floor. Half gets
// through at this level, 90% at three times it.
constexpr float kAccelNoiseFloor = 0.05f;

// A dead zone without a kink.
float FadeNoiseFloor(float a, float floor) {
  const float a2 = a * a;
  return a * a2 / (a2 + floor * floor);
}

float SoftLimit(float a, float limit) { return limit * std::tanh(a / limit); }
// Meters per m/s^2, the same whatever the Reactivity. The game's
// accelerations are small (logged: ~0.2 m/s^2 on a rolling road, a few on a
// curb), so it has to be generous for undulations to show at all.
constexpr float kSeatTravelPerAccel = 0.02f;
constexpr float kMaxSeatTravel = 0.04f; // meters, either way
// Big jolts (logged: 5 to 9 m/s^2 over a curb at ~30 km/h) get squashed
// toward this, like a seat firming up near the end of its travel, so they
// read as a bump rather than throwing the head around.
constexpr float kAccelSoftLimit = 1.5f; // m/s^2

// Angular acceleration comes in rotations/s^2, whatever SPF_TelemetryData.h
// says: SPF passes the SCS units through.
constexpr float kRotationsToRadians = math::kTwoPi;

// The neck and torso are stiffer and better damped than the seat.
constexpr float kRollOmega = 12.0f; // rad/s, ~1.9 Hz
constexpr float kRollDamping = 0.4f;
// Tuned as 2 degrees per rotation/s^2, before the unit conversion.
constexpr float kRollDegPerAngularAccel = 2.0f / math::kTwoPi;
constexpr float kMaxRollDeg = 2.0f;
// The body stays upright while the cabin rocks.
constexpr float kRollSign = 1.0f;

// A safety net bringing the head back to the player's seat: the game leaves
// gravity out, and a truck can't sustain a vertical acceleration for long
// anyway. Slow enough to let a dip or crest a few seconds long through.
constexpr float kAccelBaselineTimeConstant = 8.0f;
// Collisions and physics glitches spike far beyond any road input.
constexpr float kMaxAccel = 30.0f;        // m/s^2
constexpr float kMaxAngularAccel = 20.0f; // rad/s^2

// The dive baseline waits for the truck to settle at a stop, so the braking
// dive has rebounded first.
constexpr float kStationarySpeedKmh = 0.5f;
constexpr float kSettleSeconds = 1.0f;
constexpr float kDeltaBaselineTimeConstant = 1.0f;

constexpr float kGradeGain = 1.0f; // meters per radian of pitch
// The road's pitch changes in steps at each segment joint, which a fast
// follow turned into jolts at speed.
constexpr float kGradeTimeConstant = 1.0f;
// Too short a wheelbase to divide by.
constexpr float kMinAxleSeparation = 0.5f;
} // namespace

void SuspensionEffect::LoadSettings() {
  vertical_strength_ = Float("vertical_strength", vertical_strength_);
  reactivity_ = Float("reactivity", reactivity_);
  grade_strength_ = Float("grade_strength", grade_strength_);

  // A value saved under the old, wider range would make the seat buzz.
  seat_omega_ =
      2.0f / std::max(reactivity_,
                      settings::Min("settings.road.suspension.reactivity"));
  seat_y_.Configure(seat_omega_, kSeatDamping);
  seat_roll_.Configure(kRollOmega, kRollDamping);
  grade_y_.SetTimeConstant(kGradeTimeConstant);
  accel_y_filter_.SetTimeConstant(kAccelFilterTimeConstant);
  accel_roll_filter_.SetTimeConstant(kAccelFilterTimeConstant);
  accel_y_baseline_.SetTimeConstant(kAccelBaselineTimeConstant);
  accel_roll_baseline_.SetTimeConstant(kAccelBaselineTimeConstant);
  delta_baseline_.SetTimeConstant(kDeltaBaselineTimeConstant);
}

void SuspensionEffect::Reset() {
  seat_y_.Reset();
  seat_roll_.Reset();
  grade_y_.Reset();
  // From 0 rather than the next reading, which may land mid-bump and leave
  // the slow baseline holding the head off its seat for seconds. Both settle
  // at 0 anyway.
  accel_y_filter_.Reset();
  accel_roll_filter_.Reset();
  accel_y_baseline_.Reset();
  accel_roll_baseline_.Reset();
  stationary_elapsed_ = 0.0f;
  // Not the dive baseline: it's the truck's resting geometry, which hasn't
  // changed.
}

void SuspensionEffect::OnTruckConstantsChanged(
    const SPF_TruckConstants &constants) {
  wheel_count_ = telemetry::WheelCount(constants);

  head_x_ = constants.cabin_position.x + constants.head_position.x;
  head_z_ = constants.cabin_position.z + constants.head_position.z;

  axle_separation_ = 0.0f;
  float min_z = 0.0f, max_z = 0.0f;
  bool has_simulated = false;
  for (uint32_t i = 0; i < wheel_count_; ++i) {
    is_simulated_[i] = constants.wheels[i].simulated;
    is_liftable_[i] = constants.wheels[i].liftable;
    if (!is_simulated_[i])
      continue;
    const float z = constants.wheels[i].position.z;
    min_z = has_simulated ? std::min(min_z, z) : z;
    max_z = has_simulated ? std::max(max_z, z) : z;
    has_simulated = true;
  }
  const float mid_z = (min_z + max_z) * 0.5f;

  float front_z_sum = 0.0f, rear_z_sum = 0.0f;
  uint32_t front_count = 0, rear_count = 0;
  for (uint32_t i = 0; i < wheel_count_; ++i) {
    const float z = constants.wheels[i].position.z;
    is_front_[i] = z < mid_z; // Z points backward
    if (!is_simulated_[i])
      continue;
    if (is_front_[i]) {
      front_z_sum += z;
      ++front_count;
    } else {
      rear_z_sum += z;
      ++rear_count;
    }
  }
  if (front_count > 0 && rear_count > 0) {
    const float separation =
        (rear_z_sum / rear_count) - (front_z_sum / front_count);
    if (separation >= kMinAxleSeparation)
      axle_separation_ = separation;
  }

  has_delta_baseline_ = false;
}

void SuspensionEffect::OnTrailersChanged(const SPF_Trailer *trailers,
                                         uint32_t count) {
  const bool connected_now = count > 0 && trailers[0].data.connected;

  if (!has_prev_trailer_state_) {
    prev_trailer_connected_ = connected_now;
    has_prev_trailer_state_ = true;
    return;
  }

  if (connected_now == prev_trailer_connected_)
    return;

  // Hitching or unhitching shifts the front/rear balance.
  prev_trailer_connected_ = connected_now;
  has_delta_baseline_ = false;
}

HeadOffset SuspensionEffect::Update(float dt, const SPF_TruckData &truck,
                                    const SPF_Controls & /*controls*/) {
  if (wheel_count_ == 0)
    return {};

  // While the world loads every wheel reads exactly 0, which no truck at rest
  // ever does: nothing to react to or seed the baselines from.
  bool has_live_data = false;
  for (uint32_t i = 0; i < wheel_count_; ++i) {
    if (truck.wheels[i].suspension_deflection != 0.0f) {
      has_live_data = true;
      break;
    }
  }
  if (!has_live_data)
    return {};

  HeadOffset offset;

  // --- Seat: chassis acceleration at the driver's head ---
  // Rigid body: a_head = a + alpha x r, whose vertical part is
  // alpha.z * r.x - alpha.x * r.z.
  const SPF_FVector &alpha = truck.local_angular_acceleration;
  const float alpha_x = std::clamp(alpha.x * kRotationsToRadians,
                                   -kMaxAngularAccel, kMaxAngularAccel);
  const float alpha_z = std::clamp(alpha.z * kRotationsToRadians,
                                   -kMaxAngularAccel, kMaxAngularAccel);
  const float accel_y = std::clamp(truck.local_linear_acceleration.y +
                                       alpha_z * head_x_ - alpha_x * head_z_,
                                   -kMaxAccel, kMaxAccel);

  const float accel_y_lp = accel_y_filter_.Update(accel_y, dt);
  const float alpha_z_lp = accel_roll_filter_.Update(alpha_z, dt);
  const float accel_y_hp =
      accel_y_lp - accel_y_baseline_.Update(accel_y_lp, dt);
  const float alpha_z_hp =
      alpha_z_lp - accel_roll_baseline_.Update(alpha_z_lp, dt);

  // A jolt upward drops the head relative to the cabin. Scaled by omega^2 so
  // the size doesn't depend on the spring's stiffness.
  const float seat_y = seat_y_.Update(
      -SoftLimit(FadeNoiseFloor(accel_y_hp, kAccelNoiseFloor), kAccelSoftLimit),
      dt);
  offset.pos_y = std::clamp(seat_y * seat_omega_ * seat_omega_ *
                                kSeatTravelPerAccel * vertical_strength_,
                            -kMaxSeatTravel, kMaxSeatTravel);

  const float seat_roll = seat_roll_.Update(kRollSign * alpha_z_hp, dt);
  offset.roll = std::clamp(seat_roll * kRollOmega * kRollOmega *
                               kRollDegPerAngularAccel * vertical_strength_,
                           -kMaxRollDeg, kMaxRollDeg);

  // --- Grade-follow ---
  // Wheels that don't carry the truck would skew the balance.
  float sum_front = 0.0f, sum_rear = 0.0f;
  uint32_t front_count = 0, rear_count = 0;
  for (uint32_t i = 0; i < wheel_count_; ++i) {
    const SPF_WheelData &wheel = truck.wheels[i];
    if (!is_simulated_[i] || !wheel.on_ground ||
        (is_liftable_[i] && truck.lift_axle))
      continue;
    if (is_front_[i]) {
      sum_front += wheel.suspension_deflection;
      ++front_count;
    } else {
      sum_rear += wheel.suspension_deflection;
      ++rear_count;
    }
  }
  const bool have_axle_split =
      axle_separation_ > 0.0f && front_count > 0 && rear_count > 0;
  const float front_rear_delta =
      have_axle_split ? (sum_front / static_cast<float>(front_count)) -
                            (sum_rear / static_cast<float>(rear_count))
                      : 0.0f;

  if (telemetry::SpeedKmh(truck) < kStationarySpeedKmh)
    stationary_elapsed_ += dt;
  else
    stationary_elapsed_ = 0.0f;
  // Seeded on the move too, so there's something sane until the first stop.
  if (!has_delta_baseline_ && have_axle_split) {
    delta_baseline_.Reset(front_rear_delta);
    has_delta_baseline_ = true;
  }
  const float delta_baseline =
      have_axle_split && stationary_elapsed_ >= kSettleSeconds
          ? delta_baseline_.Update(front_rear_delta, dt)
          : delta_baseline_.value();

  // Still followed at 0 strength, so turning it off fades the offset out
  // rather than dropping it.
  float grade_target = 0.0f;
  if (grade_strength_ != 0.0f) {
    // orientation.pitch is a fraction of a turn, positive nose up. Taken
    // against true horizontal so flat road always brings the head back to
    // the player's seat: a captured resting pitch couldn't tell the
    // chassis's own pitch from the slope the truck stopped on.
    float pitch_radians =
        static_cast<float>(truck.world_placement.orientation.pitch) *
        math::kTwoPi;

    if (have_axle_split) {
      // A grade compresses both ends evenly, a dive doesn't.
      pitch_radians += (front_rear_delta - delta_baseline) / axle_separation_;
    }

    grade_target = -pitch_radians * kGradeGain * grade_strength_;
  }
  offset.pos_y += grade_y_.Update(grade_target, dt);
  return offset;
}

} // namespace motioncab
