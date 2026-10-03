#pragma once

#include "core/SettingsSchema.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "math/Oscillator.hpp"
#include "math/SpringDamper.hpp"

namespace motioncab {

// The driver riding an air-suspended seat, plus an optional grade-follow
// (raises head downhill, lowers uphill).
//
// The game already moves the cabin (and so the camera) with the chassis;
// this adds the driver's body motion relative to the cabin. The chassis's
// vertical acceleration, taken at the driver's head position (so a bump
// under the front axle is felt more than one under the rear, and a bump
// under one side rocks the driver's side), drives a damped seat spring:
// the head lags behind a jolt, then rebounds and rings out. The chassis's
// roll acceleration likewise drives a small head roll, the body staying
// upright while the cabin rocks. Both inputs are high-passed, so the head
// always settles back to the player's own seat height, with nothing to
// calibrate.
//
// Grade is measured against true horizontal, minus braking/accelerating
// dive, read from the front/rear deflection asymmetry relative to its
// value at the last stop (lifted axles and airborne wheels left out).
class SuspensionEffect final : public ConfigurableEffect {
public:
  SuspensionEffect(SPF_Config_API *config_api, SPF_Config_Handle *config_handle)
      : ConfigurableEffect(config_api, config_handle, "road", "suspension") {}

  void Reset() override;
  // Road motion is for driving: standing in the parked truck, it'd be idle
  // noise.
  bool NeedsDriverSeat() const override { return true; }
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;
  void OnTruckConstantsChanged(const SPF_TruckConstants &constants) override;
  void OnTrailersChanged(const SPF_Trailer *trailers, uint32_t count) override;

private:
  void LoadSettings() override;

  float vertical_strength_ =
      settings::Default("settings.road.suspension.vertical_strength");
  // seconds; how quickly the seat settles after a bump (sets its stiffness)
  float reactivity_ = settings::Default("settings.road.suspension.reactivity");
  float seat_omega_ = 0.0f; // rad/s, from reactivity_ in LoadSettings()
  // 0 = disabled; see class comment
  float grade_strength_ =
      settings::Default("settings.road.suspension.grade_strength");

  uint32_t wheel_count_ =
      0; // from SPF_TruckConstants; array slots beyond this are unused

  // Driver's head relative to the truck's origin (SCS vehicle space:
  // X = right, Y = up, Z = backward), where the chassis motion is felt.
  float head_x_ = 0.0f, head_z_ = 0.0f;

  // Front/rear classification for squat correction, from wheel position.z
  // (smaller z = further front), and what's needed to leave out wheels
  // that don't carry the truck (unsimulated, or on a raised lift axle).
  bool is_front_[SPF_TELEMETRY_WHEEL_MAX_COUNT] = {};
  bool is_simulated_[SPF_TELEMETRY_WHEEL_MAX_COUNT] = {};
  bool is_liftable_[SPF_TELEMETRY_WHEEL_MAX_COUNT] = {};
  float axle_separation_ =
      0.0f; // meters, avg rear z - avg front z; 0 = correction unavailable
            // (no clean front/rear split, e.g. single-axle layout)

  bool has_prev_trailer_state_ = false;
  bool prev_trailer_connected_ = false;

  float stationary_elapsed_ = 0.0f; // seconds stopped, see delta_baseline_
  bool has_delta_baseline_ = false;
  math::SpringDamper1D delta_baseline_; // front-minus-rear deflection at
                                        // the last stop

  math::SpringDamper1D accel_y_filter_;      // short smoothing of raw input
  math::SpringDamper1D accel_roll_filter_;   // same, for roll
  math::SpringDamper1D accel_y_baseline_;    // slow average, the high-pass
  math::SpringDamper1D accel_roll_baseline_; // same, for roll
  math::Oscillator1D seat_y_;                // head relative to the cabin
  math::Oscillator1D seat_roll_;             // same, roll
  math::SpringDamper1D grade_y_; // reactive grade-follow vertical offset
};

} // namespace motioncab
