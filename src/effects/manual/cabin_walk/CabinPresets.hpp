#pragma once

#include "effects/manual/cabin_walk/CabinLayouts.hpp"

#include <span>
#include <string_view>

namespace motioncab {

struct CabinPreset {
  std::string_view key;
  CabinLayout layout;
};

// ETS2's and ATS's together: no key is shared between them.
std::span<const CabinPreset> CabinPresets();

const CabinLayout *FindCabinPreset(std::string_view key);

} // namespace motioncab
