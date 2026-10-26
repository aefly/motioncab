#pragma once

#include "core/Keybinds.hpp"
#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "math/SpringDamper.hpp"

#include "SPF_KeyBinds_API.h"

namespace motioncab {

// Two looks per side, each on its own key: a wide one for cross traffic and
// a mirror glance. The turn signal glances at its mirror too, the same look
// as the key's: every mirror value is set per side, since the passenger
// mirror is farther away.
//
// A key wins over the signal, so a blinker mid-look doesn't move the head.
// By default the signal only looks standing still: while driving, the head
// turning away from the road reads as a distraction, not a deliberate check.
class SideLookEffect final : public ConfigurableEffect {
public:
  enum class Look { kCenter, kLeft, kRight, kGlanceLeft, kGlanceRight };

  // Two springs in a row, as shares of smoothing_time: the first eases the
  // head into the move instead of throwing it at full acceleration on the
  // press (or a quick switch to another look). Together they take about as
  // long as the single spring did.
  static constexpr float kIntentFraction = 0.3f;
  static constexpr float kHeadFraction = 0.85f;

  SideLookEffect(SPF_Config_API *config_api, SPF_Config_Handle *config_handle,
                 SPF_KeyBinds_API *keybinds_api,
                 SPF_KeyBinds_Handle *keybinds_handle)
      : ConfigurableEffect(config_api, config_handle, "manual", "manual_look"),
        keybinds_api_(keybinds_api), keybinds_handle_(keybinds_handle) {}

  void Reset() override;
  bool NeedsDriverSeat() const override { return true; }
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;

private:
  void LoadSettings() override;

  SPF_KeyBinds_API *keybinds_api_;
  SPF_KeyBinds_Handle *keybinds_handle_;

  // A combination wins over its keys alone.
  Look HeldLook() const;
  bool HasChord(Look look) const;
  // The mirror glance the turn signal asks for, if any.
  Look SignalLook(const SPF_TruckData &truck);

  float look_left_deg_ =
      settings::Default("settings.manual.manual_look.look_left_deg");
  float look_right_deg_ =
      settings::Default("settings.manual.manual_look.look_right_deg");
  float glance_left_deg_ =
      settings::Default("settings.manual.manual_look.glance_left_deg");
  float glance_right_deg_ =
      settings::Default("settings.manual.manual_look.glance_right_deg");
  // Positive up: the mirrors aren't always both as high.
  float glance_left_pitch_deg_ =
      settings::Default("settings.manual.manual_look.glance_left_pitch_deg");
  float glance_right_pitch_deg_ =
      settings::Default("settings.manual.manual_look.glance_right_pitch_deg");
  // Negative zooms in.
  float glance_left_fov_deg_ =
      settings::Default("settings.manual.manual_look.glance_left_fov_deg");
  float glance_right_fov_deg_ =
      settings::Default("settings.manual.manual_look.glance_right_fov_deg");
  float smoothing_time_ =
      settings::Default("settings.manual.manual_look.smoothing_time");
  bool toggle_mode_ =
      settings::DefaultBool("settings.manual.manual_look.toggle_mode");
  bool mirror_on_signal_ =
      settings::DefaultBool("settings.manual.manual_look.mirror_on_signal");
  bool require_stationary_ =
      settings::DefaultBool("settings.manual.manual_look.require_stationary");
  // A blinker switched on while moving stays ignored once stopped.
  bool ignore_after_moving_signal_ = settings::DefaultBool(
      "settings.manual.manual_look.ignore_after_moving_signal");

  math::SpringDamper1D yaw_intent_;
  math::SpringDamper1D yaw_;
  math::SpringDamper1D pitch_intent_;
  math::SpringDamper1D pitch_;
  math::SpringDamper1D fov_intent_;
  math::SpringDamper1D fov_;

  // Kept until every key is released, unless a combination comes on top.
  Look pressed_ = Look::kCenter;
  Look toggled_ = Look::kCenter;
  Look toggled_before_press_ = Look::kCenter;
  keybinds::PressEdge forward_edge_;
  // The look-forward key ends a signal's glance until the blinker is off.
  bool signal_dismissed_ = false;

  bool prev_lblinker_ = false;
  bool prev_rblinker_ = false;
  bool lblinker_moving_at_activation_ = false;
  bool rblinker_moving_at_activation_ = false;
};

} // namespace motioncab
