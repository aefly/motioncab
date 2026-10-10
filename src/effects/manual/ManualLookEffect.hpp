#pragma once

#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "effects/manual/MirrorCheckEffect.hpp"
#include "math/SpringDamper.hpp"

#include "SPF_KeyBinds_API.h"

namespace motioncab {

// Two looks per side, each on its own key: a wide one for cross traffic and
// a mirror glance.
//
// A look's values are where the head ends up, Mirror Check included: while
// a look is on, the springs hold the whole offset and Mirror Check's is
// taken back out, so a blinker mid-look doesn't move the head. The springs
// are shifted by Mirror Check's offset as a look starts or ends, so the
// head doesn't jump.
class ManualLookEffect final : public ConfigurableEffect {
public:
  enum class Look { kCenter, kLeft, kRight, kGlanceLeft, kGlanceRight };

  // mirror_check must be registered first, to be read the same frame.
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

  // A combination wins over its keys alone.
  Look HeldLook() const;
  bool HasChord(Look look) const;

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

  // Mirror Check included while engaged_.
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
  bool engaged_ = false;
};

} // namespace motioncab
