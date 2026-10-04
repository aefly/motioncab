#pragma once

#include "effects/manual/cabin_walk/CabinLayouts.hpp"

#include <span>
#include <string_view>

namespace motioncab {

// A layout shipped with the plugin, under its truck model's key
// (TruckLayoutKey).
struct CabinPreset {
  std::string_view key;
  CabinLayout layout;
};

// Every shipped layout (ETS2's and ATS's: no key is shared between them).
std::span<const CabinPreset> CabinPresets();

// The shipped layout for `key`, null if there's none.
const CabinLayout *FindCabinPreset(std::string_view key);

} // namespace motioncab
