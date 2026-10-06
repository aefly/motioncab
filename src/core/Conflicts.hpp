#pragma once

#include "core/Settings.hpp"

#include <string_view>

// Which effects pause which, declared once: the EffectManager fades a
// paused effect out while an effect pausing it is enabled (its settings
// left alone), and the Quick Settings window says so in both sections.
// Effects go by their settings prefix, "settings.<group>.<effect>".
namespace motioncab::conflicts {

struct Pause {
  const char *effect; // pauses...
  const char *paused; // ...this one while enabled
};

inline constexpr Pause kPauses[] = {
    //  {"if this effect is enabled (active)", "then pause this one"},
    {"settings.cabin.natural_head_movement", "settings.road.speed_shake"},
    {"settings.cabin.natural_head_movement", "settings.cabin.idle_breathing"},
};

// True if `effect` pauses `paused`.
constexpr bool Pauses(std::string_view effect, std::string_view paused) {
  for (const Pause &p : kPauses) {
    if (effect == p.effect && paused == p.paused)
      return true;
  }
  return false;
}

// Every effect named in kPauses has its "enabled" toggle in settings::kAll,
// and none pauses itself.
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
  }
  return true;
}
static_assert(PausesAreValid(),
              "kPauses names an effect without an \"enabled\" setting");

} // namespace motioncab::conflicts
