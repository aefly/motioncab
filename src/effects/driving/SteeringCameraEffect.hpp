#pragma once

#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "math/SpringDamper.hpp"

#include <array>

namespace motioncab {

// Turns the camera's yaw to follow the steering wheel, reproducing the
// game's native "Steering Camera Rotation" but spring-eased instead of
// snapping. The native option should stay disabled in-game since MotionCab
// owns this motion instead.
//
// Reads SPF_Controls.effectiveInput.steering, not
// SPF_TruckData.effective_steering, which stays at 0.0 at runtime.
//
// Two separate timing knobs: `delay_seconds_` is a reaction delay before
// the camera starts moving at all (time-based delay line on the raw
// signal, feeding the spring); `smoothing_time_` is the spring's own time
// constant, how eased the motion is once it starts.
//
// The rotation amount is set per side: in a left-hand-drive cab the right
// mirror is further away, so a right turn usually wants a wider pan. The
// camera turns faster toward the wider side once out of `center_zone_pct_`:
// lane corrections within it turn at the narrower side's rate either way,
// so they don't look lopsided (see SteeringYaw()).
// Smoothing stays shared on purpose: changing the spring's time constant
// mid-turn makes it snap up the lag built so far, which feels like a
// sudden head jerk.
//
// With `disable_in_reverse_` on (the default), the camera eases back to
// center while a reverse gear is selected (SPF_TruckData.gear < 0), and
// eases back onto the wheel over a second and a half once out of it.
class SteeringCameraEffect final : public ConfigurableEffect {
public:
  SteeringCameraEffect(SPF_Config_API *config_api,
                       SPF_Config_Handle *config_handle)
      : ConfigurableEffect(config_api, config_handle, "driving",
                           "steering_camera") {}

  void Reset() override;
  bool NeedsDriverSeat() const override { return true; }
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;

private:
  void LoadSettings() override;

  // Time is kept in double: a float clock loses sub-millisecond precision
  // after a couple of hours in the cabin, and the delay line then
  // interpolates between misplaced samples (a shortened, jittery delay).
  struct Sample {
    double time_s = 0.0;
    float steering = 0.0f;
  };

  static constexpr int kDelayBufferCapacity =
      300; // ample headroom for any supported delay/frame rate

  void PushSample(double time_s, float steering);
  float GetDelayedSteering(double target_time_s) const;

  // Max yaw at full steering lock, in degrees.
  float rotation_left_deg_ =
      settings::Default("settings.driving.steering_camera.rotation_left_deg");
  float rotation_right_deg_ =
      settings::Default("settings.driving.steering_camera.rotation_right_deg");
  // Steering (percent of full lock) over which the wider side eases in.
  float center_zone_pct_ =
      settings::Default("settings.driving.steering_camera.center_zone_pct");
  // seconds, spring time constant
  float smoothing_time_ =
      settings::Default("settings.driving.steering_camera.smoothing_time");
  // seconds, reaction delay before motion starts
  float delay_seconds_ =
      settings::Default("settings.driving.steering_camera.delay_seconds");
  bool disable_in_reverse_ = settings::DefaultBool(
      "settings.driving.steering_camera.disable_in_reverse");

  math::SpringDamper1D yaw_;
  bool needs_resync_ = false;
  // How much the camera follows the wheel, 0 in reverse to 1 (eased).
  float follow_ = 1.0f;

  double elapsed_time_s_ = 0.0;
  std::array<Sample, kDelayBufferCapacity> delay_buffer_{};
  int delay_head_ = 0;
  int delay_count_ = 0;
};

} // namespace motioncab
