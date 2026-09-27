#pragma once

#include "effects/ConfigurableEffect.hpp"
#include "math/SpringDamper.hpp"

#include "ui/SettingsDefaults.hpp"

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
// mirror is further away, so a right turn usually wants a wider pan. Near
// center the wider side fades down to the narrower one, so small
// corrections behave the same either way and the narrower side always gets
// exactly its own value (see kSideBlendFullSteering). Smoothing stays shared
// on purpose: changing the spring's time constant mid-turn makes it snap
// up the lag built so far, which feels like a sudden head jerk.
//
// With `disable_in_reverse_` on (the default), the camera eases back to
// center while a reverse gear is selected (SPF_TruckData.gear < 0).
class SteeringCameraEffect final : public ConfigurableEffect {
public:
  SteeringCameraEffect(SPF_Config_API *config_api,
                       SPF_Config_Handle *config_handle)
      : ConfigurableEffect(config_api, config_handle, "driving",
                           "steering_camera") {}

  void Reset() override;
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;

private:
  void LoadSettings() override;

  struct Sample {
    float time_s = 0.0f;
    float steering = 0.0f;
  };

  static constexpr int kDelayBufferCapacity =
      300; // ample headroom for any supported delay/frame rate

  void PushSample(float time_s, float steering);
  float GetDelayedSteering(float target_time_s) const;

  // Max yaw at full steering lock, in degrees.
  float rotation_left_deg_ = defaults::kSteeringCameraRotationLeft;
  float rotation_right_deg_ = defaults::kSteeringCameraRotationRight;
  float smoothing_time_ =
      defaults::kSteeringCameraSmoothing; // seconds, spring time constant
  float delay_seconds_ =
      defaults::kSteeringCameraReactionDelay; // seconds, reaction delay
                                              // before motion starts
  bool disable_in_reverse_ = defaults::kSteeringCameraDisableInReverse;

  math::SpringDamper1D yaw_;
  bool needs_resync_ = false;

  float elapsed_time_s_ = 0.0f;
  std::array<Sample, kDelayBufferCapacity> delay_buffer_{};
  int delay_head_ = 0;
  int delay_count_ = 0;
};

} // namespace motioncab
