#pragma once

#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "math/Oscillator.hpp"
#include "math/SpringDamper.hpp"

namespace motioncab {

// The driver riding an air-suspended seat, plus a grade follow that raises
// the head downhill and lowers it uphill.
//
// The game already moves the cabin with the chassis: this is the body moving
// relative to the cabin. The chassis's vertical acceleration, taken at the
// driver's head (a bump under the front axle or the driver's side is felt
// more), drives a damped seat spring, so the head lags behind a jolt, then
// rebounds and rings out. Its roll acceleration drives a small head roll the
// same way. Both are high-passed, so the head always settles back to the
// player's own seat height without anything to calibrate.
//
// Braking and accelerating dive is taken out of the grade, read from the
// front/rear deflection difference compared to the last stop.
class SuspensionEffect final : public ConfigurableEffect {
public:
  SuspensionEffect(SPF_Config_API *config_api, SPF_Config_Handle *config_handle)
      : ConfigurableEffect(config_api, config_handle, "road", "suspension") {}

  void Reset() override;
  bool NeedsDriverSeat() const override { return true; }
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;
  void OnTruckConstantsChanged(const SPF_TruckConstants &constants) override;
  void OnTrailersChanged(const SPF_Trailer *trailers, uint32_t count) override;

private:
  void LoadSettings() override;

  float vertical_strength_ =
      settings::Default("settings.road.suspension.vertical_strength");
  // Seconds for the seat to settle after a bump: sets its stiffness.
  float reactivity_ = settings::Default("settings.road.suspension.reactivity");
  float seat_omega_ = 0.0f; // rad/s
  float grade_strength_ =
      settings::Default("settings.road.suspension.grade_strength");

  uint32_t wheel_count_ = 0;

  // The driver's head from the truck's origin, where the chassis motion is
  // felt.
  float head_x_ = 0.0f, head_z_ = 0.0f;

  // Lift axles and unsimulated wheels don't carry the truck, so they're left
  // out of the front/rear balance.
  bool is_front_[SPF_TELEMETRY_WHEEL_MAX_COUNT] = {};
  bool is_simulated_[SPF_TELEMETRY_WHEEL_MAX_COUNT] = {};
  bool is_liftable_[SPF_TELEMETRY_WHEEL_MAX_COUNT] = {};
  // Meters between the front and rear wheels' average, 0 without a clean
  // front/rear split (no dive correction then).
  float axle_separation_ = 0.0f;

  bool has_prev_trailer_state_ = false;
  bool prev_trailer_connected_ = false;

  float stationary_elapsed_ = 0.0f;
  bool has_delta_baseline_ = false;
  // Front minus rear deflection at the last stop.
  math::SpringDamper1D delta_baseline_;

  math::SpringDamper1D accel_y_filter_;
  math::SpringDamper1D accel_roll_filter_;
  math::SpringDamper1D accel_y_baseline_; // the high-pass
  math::SpringDamper1D accel_roll_baseline_;
  math::Oscillator1D seat_y_; // the head relative to the cabin
  math::Oscillator1D seat_roll_;
  math::SpringDamper1D grade_y_;
};

} // namespace motioncab
