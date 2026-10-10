#pragma once

#include "SPF_Camera_API.h"

#include <array>
#include <vector>

namespace motioncab {

// Swaps the camera's rotation limits and defaults for Cabin Walk's while the
// player is up, then puts the truck's own back.
//
// The azimuth overrides (the head leaning toward a mirror or out of the
// window at given look angles) only make sense from the driver's seat, and
// would shove a standing head around, so they're emptied meanwhile. The
// game's sound keeps its own copy of them, taken when the camera is
// activated, and mixes the cabin as heard from outside in the zones flagged
// "outside": the camera is re-activated for the copy to follow.
class InteriorCameraOverride {
public:
  // Degrees, left and up positive.
  struct Limits {
    float left, right, up, down;
  };

  // False, changing nothing, if the camera isn't resolved.
  bool Engage(SPF_Camera_API *camera);
  // What the azimuth overrides were adding when Engage() emptied them, for
  // the caller to start from without a jump.
  const std::array<float, 3> &engage_head_offset() const {
    return engage_head_offset_;
  }
  // From the truck's own overrides, saved by Engage().
  std::array<float, 3> HeadOffsetAt(float yaw_deg) const;
  // So a headrest or wall right by the eyes isn't clipped away.
  float away_near_plane() const { return near_plane_ * kAwayNearPlaneShare; }
  // Every frame.
  void KeepNearPlane(SPF_Camera_API *camera);

  void SetLimits(SPF_Camera_API *camera, const Limits &limits);
  // Where the game's recenter key snaps the head, in degrees.
  void SetDefaults(SPF_Camera_API *camera, float yaw_deg, float pitch_deg);

  void Restore(SPF_Camera_API *camera);

  // For a camera that no longer exists (truck change, world reload).
  void Forget();

  // A right-hand drive camera turns the head to the opposite of the default
  // yaw when re-activated (seen in game), so a refresh writes it mirrored.
  void SetRightHandDrive(bool right_hand_drive) {
    right_hand_drive_ = right_hand_drive;
  }

  bool engaged() const { return engaged_; }
  // Re-activating the camera recenters the head a frame or so later, onto
  // defaults pointed at the current look. The caller holds the look
  // meanwhile, then calls EndRefresh() to put the truck's defaults back.
  int refreshes() const { return refreshes_; }
  void EndRefresh(SPF_Camera_API *camera);
  // Only valid while engaged.
  const Limits &original_limits() const { return limits_; }
  float original_default_yaw() const { return default_yaw_deg_; }
  float original_default_pitch() const { return default_pitch_deg_; }

private:
  // Seen in game: a view switch and back ends the outside mix.
  void RefreshSoundZones(SPF_Camera_API *camera);
  // In radians, like the head rotation.
  void RecenterOnto(SPF_Camera_API *camera, float yaw_rad, float pitch_rad);

  struct Azimuth {
    bool valid;
    float start, end;
    bool outside;
    float start_head[3], end_head[3];
  };

  static constexpr float kAwayNearPlaneShare = 0.25f;

  bool engaged_ = false;
  bool right_hand_drive_ = false;
  float near_plane_ = 0.0f;
  bool has_near_plane_ = false;
  int refreshes_ = 0;
  std::array<float, 3> engage_head_offset_{};
  Limits limits_{};
  float default_yaw_deg_ = 0.0f, default_pitch_deg_ = 0.0f;
  std::vector<Azimuth> azimuths_;
};

} // namespace motioncab
