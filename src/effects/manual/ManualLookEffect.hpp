#pragma once

#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "effects/manual/MirrorCheckEffect.hpp"
#include "math/SpringDamper.hpp"

#include "SPF_KeyBinds_API.h"

namespace motioncab {

// Looks left or right on demand, spring-eased. Two looks per side, each on
// its own key: a wide one (cross traffic) and a mirror glance (tilted and
// zoomed as set), every value per side. Polls the key state every frame,
// since hold mode needs to know the key is still down. Hold (a wide look
// wins over a glance) or toggle (`toggle_mode_`).
//
// A look's values are where the head ends up, Mirror Check included: while
// a look is on, the springs hold the whole offset and Mirror Check's is
// taken back out, so a blinker mid-look doesn't move the head. The springs
// are shifted by Mirror Check's offset on the switch, keeping the head
// still.
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
  // in degrees, positive up: the mirrors aren't always both as high
  float glance_left_pitch_deg_ =
      settings::Default("settings.manual.manual_look.glance_left_pitch_deg");
  float glance_right_pitch_deg_ =
      settings::Default("settings.manual.manual_look.glance_right_pitch_deg");
  // FOV change of the mirror glances, in degrees, negative zooms in
  float glance_left_fov_deg_ =
      settings::Default("settings.manual.manual_look.glance_left_fov_deg");
  float glance_right_fov_deg_ =
      settings::Default("settings.manual.manual_look.glance_right_fov_deg");
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
  math::SpringDamper1D fov_intent_;
  math::SpringDamper1D fov_;

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
