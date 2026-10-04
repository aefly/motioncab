#pragma once

#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "effects/manual/MirrorCheckEffect.hpp"
#include "math/SpringDamper.hpp"

#include "SPF_KeyBinds_API.h"

namespace motioncab {

// Lets the player look left or right on demand, spring-eased into place
// and back. Two looks per side, each on its own key: a wide one (cross
// traffic at a junction) and a mirror glance (a smaller turn, tilted down,
// e.g. merging onto a highway). Every angle is set per side, since the
// passenger mirror is further away than the driver's. Polls the raw key
// state every frame via Kbind_GetActionValue rather than reacting to the
// one-shot Kbind_Register callback, since press/hold mode needs to know the
// key is still down.
//
// `toggle_mode_` selects between two behaviors: press/hold (default, look
// while held, recenter on release; between plain keys, a wide look wins
// over a glance) and toggle (press to look, press again to recenter, or
// another look's key to switch). Either way the springs handle the easing.
//
// A look's angles are where the head ends up, not added to Mirror Check's
// (a look at cross traffic with a blinker on turns as far as set). While a
// look is on, the springs hold the head's angle, mirror check included, and
// its offset is taken back out every frame, so a blinker switched mid-look
// doesn't move the head. Released, they hold the look's own offset again
// and ease it back to 0, leaving the mirror check to move at its own pace.
// The switch shifts their values by the mirror check's offset, which keeps
// the head still.
class ManualLookEffect final : public ConfigurableEffect {
public:
  enum class Look { kCenter, kLeft, kRight, kGlanceLeft, kGlanceRight };

  // mirror_check must be updated before this effect, to be read the same
  // frame (registered first).
  ManualLookEffect(SPF_Config_API *config_api, SPF_Config_Handle *config_handle,
                   SPF_KeyBinds_API *keybinds_api,
                   SPF_KeyBinds_Handle *keybinds_handle,
                   const MirrorCheckEffect *mirror_check)
      : ConfigurableEffect(config_api, config_handle, "manual", "manual_look"),
        keybinds_api_(keybinds_api), keybinds_handle_(keybinds_handle),
        mirror_check_(mirror_check) {}

  void Reset() override;
  bool NeedsDriverSeat() const override { return true; }
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;

private:
  void LoadSettings() override;

  SPF_KeyBinds_API *keybinds_api_;
  SPF_KeyBinds_Handle *keybinds_handle_;
  const MirrorCheckEffect *mirror_check_;

  // The held look, a combination's first, kCenter if none.
  Look HeldLook() const;
  bool HasChord(Look look) const;

  // yaw of the wide looks, in degrees
  float look_left_deg_ =
      settings::Default("settings.manual.manual_look.look_left_deg");
  float look_right_deg_ =
      settings::Default("settings.manual.manual_look.look_right_deg");
  // yaw of the mirror glances, in degrees
  float glance_left_deg_ =
      settings::Default("settings.manual.manual_look.glance_left_deg");
  float glance_right_deg_ =
      settings::Default("settings.manual.manual_look.glance_right_deg");
  // the mirrors sit a little low, not always both as low
  float glance_left_pitch_deg_ =
      settings::Default("settings.manual.manual_look.glance_left_pitch_deg");
  float glance_right_pitch_deg_ =
      settings::Default("settings.manual.manual_look.glance_right_pitch_deg");
  // seconds, spring time constant
  float smoothing_time_ =
      settings::Default("settings.manual.manual_look.smoothing_time");
  // false = press/hold, true = toggle
  bool toggle_mode_ =
      settings::DefaultBool("settings.manual.manual_look.toggle_mode");

  // Two stages each, the first softening the start (see LoadSettings()).
  // The head's angle, mirror check included, while engaged_.
  math::SpringDamper1D yaw_intent_;
  math::SpringDamper1D yaw_;
  math::SpringDamper1D pitch_intent_;
  math::SpringDamper1D pitch_;

  // The look of the keys held right now, kept until they're all released
  // unless a combination look comes on top.
  Look pressed_ = Look::kCenter;
  // Toggle-mode state: the look held until pressed again, and what it was
  // when the keys held now were pressed.
  Look toggled_ = Look::kCenter;
  Look toggled_before_press_ = Look::kCenter;
  // A look is on: the springs hold the head's angle (see class comment).
  bool engaged_ = false;
};

} // namespace motioncab
