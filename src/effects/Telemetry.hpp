#pragma once

#include "SPF_TelemetryData.h"
#include "math/Units.hpp"

#include <cmath>
#include <cstdint>

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

// RPM crossing this means the engine has started. engine_enabled lags the
// real catch by a second or more, so the shudder came well after the fact.
inline constexpr float kRpmStartThreshold = 5.0f;

} // namespace motioncab::telemetry
