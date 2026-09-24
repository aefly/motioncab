#pragma once

#include "Effect.hpp"
#include "math/SpringDamper.hpp"

#include "SPF_Config_API.h"
#include "ui/SettingsDefaults.hpp"

namespace motioncab {

// Looks toward the corresponding side mirror while a turn signal is on,
// recentering when it's off, a driver's blind-spot check before a turn.
//
// Level-triggered, not one-shot: target is simply "mirror" while
// lblinker/rblinker is true, "center" otherwise; the spring does the
// easing, no hold/return state machine needed.
//
// require_stationary_ (default on) restricts triggering to standing still,
// since signaling while driving reads as distraction, not a deliberate check.
// With it on, ignore_after_moving_signal_ (default on) latches whether the
// truck was moving when the blinker turned on: if so, it keeps ignoring
// that blinker even if the truck later stops mid-signal, since it isn't
// re-evaluated continuously.
class MirrorCheckEffect final : public Effect {
public:
  MirrorCheckEffect(SPF_Config_API *config_api,
                    SPF_Config_Handle *config_handle)
      : config_api_(config_api), config_handle_(config_handle) {}

  const char *Name() const override { return "mirror_check"; }
  bool IsEnabled() const override { return enabled_; }
  void SetEnabled(bool enabled) override { enabled_ = enabled; }

  void LoadConfig() override;
  void Reset() override;
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;

private:
  SPF_Config_API *config_api_;
  SPF_Config_Handle *config_handle_;

  bool enabled_ = true;
  float look_angle_deg_ =
      defaults::kMirrorCheckLookAngle; // yaw toward the mirror, in degrees
  float pitch_offset_deg_ =
      defaults::kMirrorCheckPitchOffset; // mirrors sit a little low
  float smoothing_time_ =
      defaults::kMirrorCheckSmoothing; // seconds, spring time constant
  bool require_stationary_ =
      defaults::kMirrorCheckRequireStationary; // ignore blinkers while the
                                               // truck is moving
  bool ignore_after_moving_signal_ =
      defaults::kMirrorCheckIgnoreAfterMovingSignal; // see class comment

  bool prev_lblinker_ = false;
  bool prev_rblinker_ = false;
  bool lblinker_moving_at_activation_ = false;
  bool rblinker_moving_at_activation_ = false;

  math::SpringDamper1D yaw_;
  math::SpringDamper1D pitch_;
};

} // namespace motioncab
