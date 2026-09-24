#include "SuspensionEffect.hpp"

namespace motioncab {

namespace {
constexpr float kCalibrationSeconds = 2.0f;      // capture window at startup
constexpr float kCalibrationTimeConstant = 0.5f; // settles within the window
// Raised from 0.4: real deflection swings are small (a few mm to ~1.5cm
// even on rough terrain, confirmed via runtime logging), so the old gain
// left little visible travel even at max Vertical Strength.
constexpr float kVerticalGain =
    1.0f; // meters of head offset per meter of deflection
constexpr float kGradeGain =
    1.0f; // meters of head offset per radian of chassis pitch
// A front/rear split with less than this much separation isn't a
// meaningful wheelbase to divide by (degenerate/single-axle layout).
constexpr float kMinAxleSeparation = 0.5f;
} // namespace

void SuspensionEffect::LoadConfig() {
  if (!config_api_ || !config_handle_)
    return;

  enabled_ = config_api_->Cfg_GetBool(
      config_handle_, "settings.road.suspension.enabled", enabled_);
  vertical_strength_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.road.suspension.vertical_strength",
      vertical_strength_));
  reactivity_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.road.suspension.reactivity", reactivity_));
  grade_strength_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.road.suspension.grade_strength",
      grade_strength_));

  bump_y_.SetTimeConstant(reactivity_);
  grade_y_.SetTimeConstant(reactivity_);
  baseline_.SetTimeConstant(kCalibrationTimeConstant);
  delta_baseline_.SetTimeConstant(kCalibrationTimeConstant);
  pitch_baseline_.SetTimeConstant(kCalibrationTimeConstant);
}

void SuspensionEffect::Reset() {
  bump_y_.Reset();
  grade_y_.Reset();
  // The frozen baseline intentionally is NOT recalibrated here: it tracks
  // the truck's actual static load.
}

void SuspensionEffect::RecalibrateBaseline() {
  calibration_elapsed_ = 0.0f;
  baseline_locked_ = false;
  baseline_.Reset();
  delta_baseline_.Reset();
  pitch_baseline_.Reset();
}

void SuspensionEffect::OnTruckConstantsChanged(
    const SPF_TruckConstants &constants) {
  wheel_count_ = constants.wheel_count;
  if (wheel_count_ > SPF_TELEMETRY_WHEEL_MAX_COUNT)
    wheel_count_ = 0;

  axle_separation_ = 0.0f;
  if (wheel_count_ > 0) {
    float min_z = constants.wheels[0].position.z;
    float max_z = min_z;
    for (uint32_t i = 1; i < wheel_count_; ++i) {
      const float z = constants.wheels[i].position.z;
      if (z < min_z)
        min_z = z;
      if (z > max_z)
        max_z = z;
    }
    const float mid_z = (min_z + max_z) * 0.5f;

    float front_z_sum = 0.0f, rear_z_sum = 0.0f;
    uint32_t front_count = 0, rear_count = 0;
    for (uint32_t i = 0; i < wheel_count_; ++i) {
      const float z = constants.wheels[i].position.z;
      is_front_[i] = z < mid_z; // SCS vehicle space: Z = backward
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
  }

  // A truck swap invalidates the frozen static load baseline.
  RecalibrateBaseline();
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

  // Hitching/unhitching changes the truck's static sag just as much as a
  // truck swap does.
  prev_trailer_connected_ = connected_now;
  RecalibrateBaseline();
}

HeadOffset SuspensionEffect::Update(float dt, const SPF_TruckData &truck,
                                    const SPF_Controls & /*controls*/) {
  if (wheel_count_ == 0)
    return {};

  float sum_deflection = 0.0f;
  float sum_front = 0.0f, sum_rear = 0.0f;
  uint32_t front_count = 0, rear_count = 0;
  for (uint32_t i = 0; i < wheel_count_; ++i) {
    const float d = truck.wheels[i].suspension_deflection;
    sum_deflection += d;
    if (is_front_[i]) {
      sum_front += d;
      ++front_count;
    } else {
      sum_rear += d;
      ++rear_count;
    }
  }
  const float avg_deflection =
      sum_deflection / static_cast<float>(wheel_count_);
  const bool have_axle_split =
      axle_separation_ > 0.0f && front_count > 0 && rear_count > 0;
  const float front_rear_delta =
      have_axle_split ? (sum_front / static_cast<float>(front_count)) -
                            (sum_rear / static_cast<float>(rear_count))
                      : 0.0f;

  // orientation.pitch is a unit-circle fraction (<-0.25,0.25> = <-90,90>
  // degrees, positive = nose up), not radians. Convert before gaining.
  constexpr float kTwoPi = 6.28318530718f;
  const float raw_pitch_radians =
      static_cast<float>(truck.world_placement.orientation.pitch) * kTwoPi;

  float baseline;
  float delta_baseline;
  float pitch_baseline;
  if (baseline_locked_) {
    baseline = baseline_.value();
    delta_baseline = delta_baseline_.value();
    pitch_baseline = pitch_baseline_.value();
  } else {
    baseline = baseline_.Update(avg_deflection, dt);
    delta_baseline = delta_baseline_.Update(front_rear_delta, dt);
    pitch_baseline = pitch_baseline_.Update(raw_pitch_radians, dt);
    calibration_elapsed_ += dt;
    if (calibration_elapsed_ >= kCalibrationSeconds)
      baseline_locked_ = true;
  }
  const float vertical_bump = avg_deflection - baseline;

  HeadOffset offset;
  offset.pos_y =
      bump_y_.Update(vertical_bump * kVerticalGain * vertical_strength_, dt);

  if (grade_strength_ != 0.0f) {
    // The chassis's own resting pitch (ride height/geometry) is never
    // exactly 0. Measure grade relative to it, not to true horizontal.
    float pitch_radians = raw_pitch_radians - pitch_baseline;

    if (have_axle_split) {
      // Cancel braking/accelerating dive (asymmetric front/rear
      // deflection); a real grade compresses both ends evenly.
      const float squat_indicator = front_rear_delta - delta_baseline;
      pitch_radians += squat_indicator / axle_separation_;
    }

    // Downhill (nose down, negative) should raise the head; uphill (nose
    // up, positive) should lower it, hence the negation.
    offset.pos_y +=
        grade_y_.Update(-pitch_radians * kGradeGain * grade_strength_, dt);
  }
  return offset;
}

} // namespace motioncab
