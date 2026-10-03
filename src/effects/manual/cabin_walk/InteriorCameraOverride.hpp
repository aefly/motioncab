#pragma once

#include "SPF_Camera_API.h"

#include <array>
#include <vector>

namespace motioncab {

// Swaps the interior camera's rotation limits and defaults for our own while
// the player is out of the driver's seat (Cabin Walk), and puts the
// truck's own values back afterwards.
//
// Engage() also empties the azimuth overrides, the truck's head offsets
// for given look angles (leaning toward a mirror, or out of the window):
// they only make sense from the driver's seat, and would shove a standing
// head around. The game's sound keeps its own copy of them, taken when the
// interior camera is activated, and mixes the cabin as heard from outside
// (FMOD's cabin_out) in the zones flagged "outside": so both Engage() and
// Restore() re-activate the camera for the copy to follow.
class InteriorCameraOverride {
public:
  // Degrees, as Cam_Get/SetInteriorRotationLimits take them: left and up
  // positive, right and down negative.
  struct Limits {
    float left, right, up, down;
  };

  // Saves the truck's values, the first time since the last Restore() or
  // Forget(), and empties the azimuth overrides. False if the camera isn't
  // resolved; nothing is changed then.
  bool Engage(SPF_Camera_API *camera);
  // The head offset the azimuth overrides were adding at the look angle of
  // the last Engage() (leaning toward a mirror), in the seat's frame: gone
  // since, so the caller starts from it to avoid a jump.
  const std::array<float, 3> &engage_head_offset() const {
    return engage_head_offset_;
  }
  // The head offset the truck's own azimuth overrides give at `yaw_deg`
  // (leaning toward a mirror), in the seat's frame; zero where none
  // applies. Only while engaged: the camera's own are emptied meanwhile.
  std::array<float, 3> HeadOffsetAt(float yaw_deg) const;
  // The interior camera's near clipping distance while engaged: a fraction
  // of the truck's own, so a headrest or wall right by the eyes isn't
  // clipped away.
  float away_near_plane() const { return near_plane_ * kAwayNearPlaneShare; }
  // Keeps the engaged near plane on the camera. Every frame while engaged.
  void KeepNearPlane(SPF_Camera_API *camera);

  // Only while engaged.
  void SetLimits(SPF_Camera_API *camera, const Limits &limits);
  // Degrees, the head rotation the native recenter key snaps to.
  void SetDefaults(SPF_Camera_API *camera, float yaw_deg, float pitch_deg);

  // Puts the truck's values back, if engaged.
  void Restore(SPF_Camera_API *camera);

  // Drops the saved values without writing them: after a truck change or a
  // world reload, when they belong to a camera that no longer exists.
  void Forget();

  // A right-hand drive camera turns the head to the opposite of the default
  // yaw when re-activated (seen in game), so a refresh writes it mirrored.
  void SetRightHandDrive(bool right_hand_drive) {
    right_hand_drive_ = right_hand_drive;
  }

  bool engaged() const { return engaged_; }
  // Counts the camera's re-activations. Each one points the rotation
  // defaults at the current look for the recenter it causes a frame or so
  // later; the caller holds the look meanwhile, then calls EndRefresh() to
  // put the truck's defaults back.
  int refreshes() const { return refreshes_; }
  void EndRefresh(SPF_Camera_API *camera);
  // The truck's own values; valid only while engaged.
  const Limits &original_limits() const { return limits_; }
  float original_default_yaw() const { return default_yaw_deg_; }
  float original_default_pitch() const { return default_pitch_deg_; }

private:
  // Re-activates the interior camera, if it's the current one, so the
  // game's sound takes a new copy of the azimuth overrides (seen in game:
  // a view switch and back ends the outside mix).
  void RefreshSoundZones(SPF_Camera_API *camera);
  // Sets the rotation defaults so that the camera's next activation
  // recenters the head onto this look (radians, as the head rotation),
  // mirrored for a right-hand drive camera (see SetRightHandDrive).
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
