#pragma once

#include "core/Settings.hpp"

#include <string_view>

// Which effects pause which. Both the EffectManager and the Quick Settings
// window read this, so a conflict is declared here rather than by turning a
// setting off. Effects go by their settings prefix.
namespace motioncab::conflicts {

struct Pause {
  const char *effect; // pauses...
  const char *paused; // ...this one while enabled
};

inline constexpr Pause kPauses[] = {
    {"settings.cabin.natural_head_movement", "settings.road.speed_shake"},
    {"settings.cabin.natural_head_movement", "settings.cabin.idle_breathing"},
};

constexpr bool Pauses(std::string_view effect, std::string_view paused) {
  for (const Pause &p : kPauses) {
    if (effect == p.effect && paused == p.paused)
      return true;
  }
  return false;
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
  }
  return true;
}
static_assert(PausesAreValid(),
              "kPauses names an effect without an \"enabled\" setting");

} // namespace motioncab::conflicts
