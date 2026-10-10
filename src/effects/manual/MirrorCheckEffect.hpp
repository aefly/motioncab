#pragma once

#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "math/SpringDamper.hpp"

namespace motioncab {

// Looks at the mirror on the side of the turn signal while it's on. Every
// value is set per side, since the passenger mirror is farther away.
//
// By default it only does so standing still: while driving, the head turning
// away from the road reads as a distraction, not a deliberate check.
class MirrorCheckEffect final : public ConfigurableEffect {
public:
  MirrorCheckEffect(SPF_Config_API *config_api,
                    SPF_Config_Handle *config_handle)
      : ConfigurableEffect(config_api, config_handle, "manual",
                           "mirror_check") {}

  void Reset() override;
  bool NeedsDriverSeat() const override { return true; }
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;

  // For ManualLookEffect to take out of its looks.
  float yaw() const { return yaw_.value(); }
  float pitch() const { return pitch_.value(); }
  float fov() const { return fov_.value(); }

private:
  void LoadSettings() override;

  float left_angle_deg_ =
      settings::Default("settings.manual.mirror_check.left_angle_deg");
  float right_angle_deg_ =
      settings::Default("settings.manual.mirror_check.right_angle_deg");
  // Positive up.
  float left_pitch_deg_ =
      settings::Default("settings.manual.mirror_check.left_pitch_deg");
  float right_pitch_deg_ =
      settings::Default("settings.manual.mirror_check.right_pitch_deg");
  // Negative zooms in.
  float left_fov_deg_ =
      settings::Default("settings.manual.mirror_check.left_fov_deg");
  float right_fov_deg_ =
      settings::Default("settings.manual.mirror_check.right_fov_deg");
  float smoothing_time_ =
      settings::Default("settings.manual.mirror_check.smoothing_time");
  bool require_stationary_ =
      settings::DefaultBool("settings.manual.mirror_check.require_stationary");
  // A blinker switched on while moving stays ignored once stopped.
  bool ignore_after_moving_signal_ = settings::DefaultBool(
      "settings.manual.mirror_check.ignore_after_moving_signal");

  bool prev_lblinker_ = false;
  bool prev_rblinker_ = false;
  bool lblinker_moving_at_activation_ = false;
  bool rblinker_moving_at_activation_ = false;

  math::SpringDamper1D yaw_;
  math::SpringDamper1D pitch_;
  math::SpringDamper1D fov_;
};

} // namespace motioncab
