#pragma once

#include "SPF_TelemetryData.h"
#include "math/Units.hpp"

#include <cmath>
#include <cstdint>

// Telemetry readings shared by several effects, so each heuristic lives in
// one place.
namespace motioncab::telemetry {

// Absolute truck speed in km/h, whichever way it's moving.
inline float SpeedKmh(const SPF_TruckData &truck) {
  return std::fabs(truck.speed) * math::kMsToKmh;
}

// constants.wheel_count, or 0 if it's out of range for the per-wheel arrays
// (the effects then treat the layout as unknown).
inline uint32_t WheelCount(const SPF_TruckConstants &constants) {
  return constants.wheel_count <= SPF_TELEMETRY_WHEEL_MAX_COUNT
             ? constants.wheel_count
             : 0;
}

// No direct "is electric" flag: an empty AdBlue tank is the tell
// (fuel_capacity doesn't work, since BEVs still report a nonzero value).
inline bool IsElectric(const SPF_TruckConstants &constants) {
  return constants.adblue_capacity <= 0.01f;
}

// engine_rpm crossing this (near-zero) is the "engine has started" signal.
// engine_enabled lags the real catch by 1+ second, which read as buzz or
// shudder firing well after the fact.
inline constexpr float kRpmStartThreshold = 5.0f;

} // namespace motioncab::telemetry
