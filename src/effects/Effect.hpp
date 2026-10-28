#pragma once

#include "SPF_TelemetryData.h"

#include <cmath>
#include <string_view>

namespace motioncab {

// Added on top of the player's own pose, summed over every effect.
struct HeadOffset {
  float pos_x = 0.0f, pos_y = 0.0f, pos_z = 0.0f; // meters, cabin-local
  // The angles are all in degrees, like most of the camera API: only
  // Cam_SetInteriorHeadRot takes radians, and CameraRig converts for it.
  float yaw = 0.0f, pitch = 0.0f;
  float roll = 0.0f;
  float fov = 0.0f;
};

// A NaN or infinity must never reach the camera: the write is differential,
// so it would stay in the pose and poison every later frame.
inline bool IsFinite(const HeadOffset &o) {
  return std::isfinite(o.pos_x) && std::isfinite(o.pos_y) &&
         std::isfinite(o.pos_z) && std::isfinite(o.yaw) &&
         std::isfinite(o.pitch) && std::isfinite(o.roll) &&
         std::isfinite(o.fov);
}

class Effect {
public:
  virtual ~Effect() = default;

  virtual bool IsEnabled() const = 0;
  virtual void SetEnabled(bool enabled) = 0;

  virtual void LoadConfig() = 0;

  // Back to rest, e.g. on entering the cabin view.
  virtual void Reset() = 0;

  virtual HeadOffset Update(float dt, const SPF_TruckData &truck,
                            const SPF_Controls &controls) = 0;

  // For the effects that only make sense in the driver's seat (driving
  // motion, mirror checks, ...): they fade out while Cabin Walk has the
  // player elsewhere.
  virtual bool NeedsDriverSeat() const { return false; }

  // "settings.<group>.<effect>", as conflicts::kPauses names it.
  virtual std::string_view Id() const = 0;

  virtual void
  OnTruckConstantsChanged(const SPF_TruckConstants & /*constants*/) {}

  // Carries the substance name table, for telling road surfaces apart.
  virtual void OnCommonDataChanged(const SPF_CommonData & /*data*/) {}

  // Every frame.
  virtual void OnTrailersChanged(const SPF_Trailer * /*trailers*/,
                                 uint32_t /*count*/) {}
};

} // namespace motioncab
