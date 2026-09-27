#include "SuspensionEffect.hpp"

#include "effects/TelemetryUtil.hpp"
#include "math/Units.hpp"

#include <algorithm>
#include <cmath>

namespace motioncab {

namespace {
// Seat spring. Its stiffness comes from Reactivity (omega = 2 / reactivity,
// ~1.3 Hz at the 0.25 s default, typical of an air-suspended truck seat);
// its damping leaves a small rebound after a bump without ringing on: a
// lighter one kept the seat bouncing at its own frequency on flat road at
// speed, excited by the road's constant small jolts.
constexpr float kSeatDamping = 0.6f;
// Smooths the raw accelerations first: at speed they carry a constant
// fast jitter (road texture, seams) that a real seat cushion swallows,
// and which RoadIrregularityEffect already renders. Undulations and bumps
// are slower and pass.
constexpr float kAccelFilterTimeConstant = 0.08f;
// Below about this (m/s^2), what's left is the flat road's noise floor,
// faded out smoothly (half of it passes at this level, 90% at 3x).
constexpr float kAccelNoiseFloor = 0.05f;

// Smooth dead zone: ~0 for |a| << floor, ~a for |a| >> floor, no kink.
float FadeNoiseFloor(float a, float floor) {
  const float a2 = a * a;
  return a * a2 / (a2 + floor * floor);
}

// Smooth limit: ~a for |a| << limit, approaching +/-limit beyond it.
float SoftLimit(float a, float limit) { return limit * std::tanh(a / limit); }
// Meters of head travel per m/s^2 of sustained vertical acceleration,
// normalized so Reactivity changes the timing but not the size. The game's
// accelerations are small (logged: ~0.2 m/s^2 on a rolling road, a few m/s^2
// on a curb), so this has to be generous for undulations to show at all.
constexpr float kSeatTravelPerAccel = 0.02f;
constexpr float kMaxSeatTravel = 0.04f; // meters, either way
// Big jolts (logged: 5 to 9 m/s^2 over a curb or speed bump at ~30 km/h)
// are compressed toward this, like a seat firming up near the end of its
// travel, so they read as a bump rather than throwing the head around.
// Small ones stay nearly linear (90% passes at half this level).
constexpr float kAccelSoftLimit = 1.5f; // m/s^2

// SPF passes the SCS telemetry through unconverted (see orientation.pitch
// below), and the SCS SDK gives angular acceleration in rotations/s^2,
// whatever SPF_TelemetryData.h says. Converted to rad/s^2 on read.
constexpr float kRotationsToRadians = math::kTwoPi;

// Head roll: the neck and torso are stiffer and better damped than the seat.
constexpr float kRollOmega = 12.0f; // rad/s, ~1.9 Hz
constexpr float kRollDamping = 0.4f;
// Degrees of head roll per rad/s^2 of sustained chassis roll acceleration
// (2 degrees per rotation/s^2, as tuned before the unit conversion).
constexpr float kRollDegPerAngularAccel = 2.0f / math::kTwoPi;
constexpr float kMaxRollDeg = 2.0f;
// Roll direction, the body staying upright while the cabin rocks. Positive
// chassis roll lifts the right side (SCS vehicle space: X = right, Z =
// backward), so the truck leans left. Flip if the head tilt feels inverted.
constexpr float kRollSign = 1.0f;

// The high-pass on both accelerations, a safety net so the head always
// settles back to the player's own seat: the game leaves gravity out, and a
// truck can't sustain a vertical acceleration for long anyway. Slow enough
// to let a dip or crest a few seconds long through.
constexpr float kAccelBaselineTimeConstant = 8.0f;
// Collisions and physics glitches spike far beyond any road input.
constexpr float kMaxAccel = 30.0f;        // m/s^2
constexpr float kMaxAngularAccel = 20.0f; // rad/s^2

// The dive baseline is only re-captured once the truck has settled at a
// stop, so the stopping dive has rebounded first.
constexpr float kStationarySpeedKmh = 0.5f;
constexpr float kSettleSeconds = 1.0f;
constexpr float kDeltaBaselineTimeConstant = 1.0f;

constexpr float kGradeGain =
    1.0f; // meters of head offset per radian of chassis pitch
// Road pitch changes in steps at each road segment joint, which a fast
// follow turned into jolts at speed. A grade is a slow thing to follow.
constexpr float kGradeTimeConstant = 1.0f;
// A front/rear split with less than this much separation isn't a
// meaningful wheelbase to divide by (degenerate/single-axle layout).
constexpr float kMinAxleSeparation = 0.5f;
} // namespace

void SuspensionEffect::LoadSettings() {
  vertical_strength_ = Float("vertical_strength", vertical_strength_);
  reactivity_ = Float("reactivity", reactivity_);
  grade_strength_ = Float("grade_strength", grade_strength_);

  // Floored to the slider's minimum: a value saved under the old, wider
  // range would make the seat buzz.
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
  // Restarted from 0 rather than from the next reading: that reading may
  // land mid-bump, and the slow baseline would then hold the head off its
  // seat for seconds. A truck can't sustain a vertical acceleration (the
  // game leaves gravity out), so 0 is where both settle anyway.
  accel_y_filter_.Reset();
  accel_roll_filter_.Reset();
  accel_y_baseline_.Reset();
  accel_roll_baseline_.Reset();
  stationary_elapsed_ = 0.0f;
  // The dive baseline intentionally is NOT reset here: it tracks the
  // truck's actual resting geometry.
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
    is_front_[i] = z < mid_z; // SCS vehicle space: Z = backward
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

  // A truck swap invalidates the resting front/rear balance.
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

  // Hitching/unhitching shifts the front/rear balance just as much as a
  // truck swap does.
  prev_trailer_connected_ = connected_now;
  has_delta_baseline_ = false;
}

HeadOffset SuspensionEffect::Update(float dt, const SPF_TruckData &truck,
                                    const SPF_Controls & /*controls*/) {
  if (wheel_count_ == 0)
    return {};

  // While the game world is still loading, telemetry reports every wheel
  // at exactly 0, which no real truck ever does at rest. Don't react to
  // that, nor seed the baselines from it.
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
  // Rigid body: a_head = a + alpha x r, whose vertical component is
  // alpha.z * r.x - alpha.x * r.z. Pitching lifts the front (where the
  // driver sits) and rolling lifts the driver's side.
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

  // The body lags behind the cabin: a jolt upward drops the head relative
  // to it, hence the negated drive. Scaled by omega^2 so the size doesn't
  // depend on the spring's stiffness (see kSeatTravelPerAccel).
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
  // Wheels that don't carry the truck would skew the front/rear balance:
  // unsimulated ones, a raised lift axle, and any wheel in the air.
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
  // Seeded from the first reading, even on the move, so there's a sane
  // baseline until the first stop refines it.
  if (!has_delta_baseline_ && have_axle_split) {
    delta_baseline_.Reset(front_rear_delta);
    has_delta_baseline_ = true;
  }
  const float delta_baseline =
      have_axle_split && stationary_elapsed_ >= kSettleSeconds
          ? delta_baseline_.Update(front_rear_delta, dt)
          : delta_baseline_.value();

  // Still followed at 0 strength, easing to 0, so turning Grade Follow off
  // fades the offset out instead of dropping it, and turning it back on
  // starts from 0 rather than a stale value.
  float grade_target = 0.0f;
  if (grade_strength_ != 0.0f) {
    // orientation.pitch is a unit-circle fraction (<-0.25,0.25> = <-90,90>
    // degrees, positive = nose up), not radians. Convert before gaining.
    // Measured against true horizontal, so flat road always brings the
    // head back to the player's own seat height. A captured "resting
    // pitch" can't tell the chassis's own (tiny) pitch apart from the
    // slope the truck happened to rest on.
    float pitch_radians =
        static_cast<float>(truck.world_placement.orientation.pitch) *
        math::kTwoPi;

    if (have_axle_split) {
      // Cancel braking/accelerating dive (asymmetric front/rear
      // deflection); a real grade compresses both ends evenly.
      pitch_radians += (front_rear_delta - delta_baseline) / axle_separation_;
    }

    // Downhill (nose down, negative) should raise the head; uphill (nose
    // up, positive) should lower it, hence the negation.
    grade_target = -pitch_radians * kGradeGain * grade_strength_;
  }
  offset.pos_y += grade_y_.Update(grade_target, dt);
  return offset;
}

} // namespace motioncab
