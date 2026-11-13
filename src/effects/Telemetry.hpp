#pragma once

#include "SPF_TelemetryData.h"
#include "math/Units.hpp"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>

// Readings shared by several effects, so each heuristic lives in one place.
namespace motioncab::telemetry {

// Whichever way the truck is moving.
inline float SpeedKmh(const SPF_TruckData &truck) {
  return std::fabs(truck.speed) * math::kMsToKmh;
}

// 0, an unknown layout, if it doesn't fit the per-wheel arrays.
inline uint32_t WheelCount(const SPF_TruckConstants &constants) {
  return constants.wheel_count <= SPF_TELEMETRY_WHEEL_MAX_COUNT
             ? constants.wheel_count
             : 0;
}

// There's no flag for it. An electric truck has no AdBlue tank, while it
// still reports a fuel capacity.
inline bool IsElectric(const SPF_TruckConstants &constants) {
  return constants.adblue_capacity <= 0.01f;
}

// The head position is relative to the cabin's centerline.
inline bool IsRightHandDrive(const SPF_TruckConstants &constants) {
  return constants.head_position.x > 0.0f;
}

// The truck model, "<brand_id>_<id>", reduced to [A-Za-z0-9_] to hold as one
// config key segment. Empty while the truck isn't known.
inline std::string TruckKey(const SPF_TruckConstants &constants) {
  auto clean = [](const char *s) {
    std::string out;
    for (; *s; ++s)
      out += std::isalnum(static_cast<unsigned char>(*s)) ? *s : '_';
    return out;
  };
  const std::string brand = clean(constants.brand_id);
  const std::string model = clean(constants.id);
  if (brand.empty() && model.empty())
    return {};
  return brand + "_" + model;
}

// With the Simple Automatic gearbox, gear only goes negative in short bursts
// while the truck pulls away backward, and drops back to 0 in between and at a
// stop. The dashboard keeps showing R until the player drives forward again.
inline bool IsReversing(const SPF_TruckData &truck) {
  return truck.gear < 0 || truck.displayed_gear < 0;
}

// RPM crossing this means the engine has started. engine_enabled lags the
// real catch by a second or more, so the shudder came well after the fact.
inline constexpr float kRpmStartThreshold = 5.0f;

} // namespace motioncab::telemetry
