#pragma once

#include "Effect.hpp"
#include "math/SpringDamper.hpp"

#include "SPF_Config_API.h"
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
class SteeringCameraEffect final : public Effect {
public:
  SteeringCameraEffect(SPF_Config_API *config_api,
                       SPF_Config_Handle *config_handle)
      : config_api_(config_api), config_handle_(config_handle) {}

  const char *Name() const override { return "steering_camera"; }
  bool IsEnabled() const override { return enabled_; }
  void SetEnabled(bool enabled) override { enabled_ = enabled; }

  void LoadConfig() override;
  void Reset() override;
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;

private:
  struct Sample {
    float time_s = 0.0f;
    float steering = 0.0f;
  };

  static constexpr int kDelayBufferCapacity =
      300; // ample headroom for any supported delay/frame rate

  void PushSample(float time_s, float steering);
  float GetDelayedSteering(float target_time_s) const;

  SPF_Config_API *config_api_;
  SPF_Config_Handle *config_handle_;

  bool enabled_ = true;
  float rotation_factor_deg_ =
      defaults::kSteeringCameraRotationAmount; // max yaw at full steering
                                               // lock, in degrees
  float smoothing_time_ =
      defaults::kSteeringCameraSmoothing; // seconds, spring time constant
  float delay_seconds_ =
      defaults::kSteeringCameraReactionDelay; // seconds, reaction delay
                                              // before motion starts

  math::SpringDamper1D yaw_;
  bool needs_resync_ = false;

  float elapsed_time_s_ = 0.0f;
  std::array<Sample, kDelayBufferCapacity> delay_buffer_{};
  int delay_head_ = 0;
  int delay_count_ = 0;
};

} // namespace motioncab
