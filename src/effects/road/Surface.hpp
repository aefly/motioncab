#pragma once

#include "SPF_TelemetryData.h"
#include "core/Strings.hpp"

#include <array>
#include <cstdint>

// The ground under the wheels, shared by the road effects. Telemetry only
// names each surface, so the names are rated by hand. An unknown one (other
// map, mod) counts as smooth rather than buzzing for no visible reason.
namespace motioncab {

struct SurfaceTraits {
  float roughness = 0.0f;  // 0..1, fine chatter
  float unevenness = 0.0f; // 0..1, side-to-side rocking
};

namespace detail {
struct SurfaceEntry {
  const char *name;
  SurfaceTraits traits;
};

// The names ETS2 reports at runtime.
inline constexpr SurfaceEntry kSurfaceTable[] = {
    {"road", {0.0f, 0.0f}},        {"road_smooth", {0.0f, 0.0f}},
    {"road_coarse", {0.5f, 0.0f}}, {"road_dirt", {0.7f, 0.6f}},
    {"road_snow", {0.5f, 0.2f}},   {"dirt", {0.9f, 1.0f}},
    {"gravel", {1.0f, 0.5f}},      {"grass", {0.5f, 0.9f}},
    {"snow", {0.4f, 0.6f}},        {"soft", {0.3f, 0.8f}},
    {"concrete", {0.15f, 0.0f}},   {"metal", {0.2f, 0.0f}},
    {"wood", {0.2f, 0.0f}},        {"rumble_stripe", {1.0f, 0.0f}},
    {"ice", {0.0f, 0.0f}},         {"static", {0.0f, 0.0f}},
    {"invis", {0.0f, 0.0f}},       {"rubber", {0.0f, 0.0f}},
    {"plastic", {0.0f, 0.0f}},     {"glass", {0.0f, 0.0f}},
};
} // namespace detail

inline SurfaceTraits ClassifySurface(const char *substance_name) {
  if (!substance_name || !substance_name[0])
    return {};
  for (const auto &entry : detail::kSurfaceTable) {
    if (strings::EqualsIgnoreCase(substance_name, entry.name))
      return entry.traits;
  }
  return {};
}

// Classifies the substance list once when it changes, rather than every
// frame.
class SurfaceMap {
public:
  void SetSubstances(const SPF_CommonData &data) {
    count_ = data.substance_count < SPF_TELEMETRY_SUBSTANCE_MAX_COUNT
                 ? data.substance_count
                 : SPF_TELEMETRY_SUBSTANCE_MAX_COUNT;
    for (uint32_t i = 0; i < count_; ++i)
      traits_[i] = ClassifySurface(data.substances[i]);
  }

  bool empty() const { return count_ == 0; }

  // A wheel on an unknown substance index counts as smooth.
  SurfaceTraits Average(const SPF_TruckData &truck,
                        uint32_t wheel_count) const {
    if (wheel_count == 0)
      return {};
    SurfaceTraits sum;
    for (uint32_t i = 0; i < wheel_count; ++i) {
      const uint32_t index = truck.wheels[i].substance;
      if (index < count_) {
        sum.roughness += traits_[index].roughness;
        sum.unevenness += traits_[index].unevenness;
      }
    }
    const float n = static_cast<float>(wheel_count);
    return {sum.roughness / n, sum.unevenness / n};
  }

private:
  std::array<SurfaceTraits, SPF_TELEMETRY_SUBSTANCE_MAX_COUNT> traits_{};
  uint32_t count_ = 0;
};

} // namespace motioncab
