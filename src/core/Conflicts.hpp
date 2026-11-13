#pragma once

#include "core/Settings.hpp"

#include <algorithm>
#include <string_view>

// Which effects pause which. Both the EffectManager and the Quick Settings
// window read this, so a conflict is declared here rather than by turning a
// setting off. Effects go by their settings prefix.
namespace motioncab::conflicts {

struct Pause {
  const char *effect; // pauses...
  const char *paused; // ...this one while enabled
  // Only while the pausing effect is in use (Effect::InUse), rather than
  // for as long as it's enabled.
  bool while_in_use = false;
  // A bool setting of the pausing effect that turns the pause on. Required
  // for a pause while in use: the window lists only the other pauses in the
  // pausing effect's section.
  const char *option = nullptr;
};

inline constexpr Pause kPauses[] = {
    {"settings.cabin.natural_head_movement", "settings.cabin.idle_breathing"},
    {"settings.cabin.natural_head_movement", "settings.road.speed_shake"},
    // The view following the wheel would pull a look off what it's aimed at.
    {"settings.manual.manual_look", "settings.driving.steering_camera", true,
     "settings.manual.manual_look.pause_steering_camera"},
};

constexpr const Pause *Find(std::string_view effect, std::string_view paused) {
  for (const Pause &p : kPauses) {
    if (effect == p.effect && paused == p.paused)
      return &p;
  }
  return nullptr;
}

// Catches a typo in an effect name, or an effect pausing itself.
consteval bool PausesAreValid() {
  const auto has_toggle = [](std::string_view prefix) {
    for (const settings::Setting &s : settings::kAll) {
      if (settings::InEffect(s, prefix) && s.type == settings::Type::kBool &&
          settings::SplitKey(s.key).name == "enabled")
        return true;
    }
    return false;
  };
  for (const Pause &p : kPauses) {
    if (!has_toggle(p.effect) || !has_toggle(p.paused) ||
        std::string_view(p.effect) == p.paused)
      return false;
    if (p.while_in_use && !p.option)
      return false;
    if (p.option &&
        !std::ranges::any_of(settings::kAll, [&](const settings::Setting &s) {
          return s.key == std::string_view(p.option) &&
                 s.type == settings::Type::kBool &&
                 settings::InEffect(s, p.effect);
        }))
      return false;
  }
  return true;
}
static_assert(PausesAreValid(),
              "kPauses names an effect without an \"enabled\" setting, or "
              "a missing option");

} // namespace motioncab::conflicts
