#pragma once

#include "SPF_Camera_API.h"
#include "effects/Effect.hpp"

#include <optional>

namespace motioncab {

// Writes the effects' summed HeadOffset into the interior camera without
// owning the pose ("differential write"): each frame it subtracts the
// offset it applied last frame from the live pose, recovering the player's
// own seat/head pose and FOV, then adds the new offset on top. Free-look
// and the player's SPF seat settings keep working underneath.
//
// The engine keeps the last offset written even while the camera isn't
// the interior one, so applied() stays valid across view switches and
// must not be zeroed there. Remove() takes it back out for good.
class CameraRig {
public:
  // The live seat position and head rotation, read before the effects
  // update so the recenter check and the write use the same frame's pose.
  struct Pose {
    float seat_x, seat_y, seat_z;
    float yaw_rad, pitch_rad;
  };

  // Empty if the camera isn't resolved yet (getters fail): the frame must
  // then be skipped entirely, since a bogus reading would ratchet the seat
  // through a bad applied offset.
  std::optional<Pose> ReadPose(SPF_Camera_API *camera) const;

  // True if the native "recenter camera" hotkey just snapped the head
  // rotation to its default, which drops our rotation offset from the
  // live pose without any event to hook. The applied yaw/pitch are then
  // forgotten; the caller should reset the effects like on cabin entry.
  bool DetectNativeRecenter(SPF_Camera_API *camera, const Pose &pose);

  // True if something else rewrote the seat position since our last write,
  // e.g. changing a setting in the game's F4 seat menu, which sets the
  // player's seat as an absolute value and so drops our offset from it.
  // The applied position is then forgotten for the axes that changed, so
  // the new value is taken as the player's own seat instead of having an
  // offset that's no longer there subtracted from it.
  bool DetectExternalSeatWrite(const Pose &pose);

  // Same for the FOV: a truck switch's new camera comes with the player's
  // own FOV, and the game's F4 slider sets it as an absolute value, both
  // without our offset. The applied FOV is then forgotten. Call before
  // ManualZoomEffect writes this frame's FOV.
  bool DetectExternalFovWrite(SPF_Camera_API *camera);

  // Records a FOV write made outside Apply (ManualZoomEffect), so
  // DetectExternalFovWrite doesn't take it for someone else's.
  void NoteFovWrite(float fov_deg) {
    last_written_fov_ = fov_deg;
    has_last_written_fov_ = true;
  }

  // The player's own pose underneath: `pose` minus the applied offset.
  Pose Base(const Pose &pose) const;

  // Makes the player's own head rotation under `pose` `yaw_rad`/
  // `pitch_rad`, so the next Apply writes that plus the new offset. For
  // turning the player's view itself rather than adding to it (Cabin Walk
  // sitting down), with the mouse blocked meanwhile.
  void SetBaseRotation(Pose &pose, float yaw_rad, float pitch_rad) const;

  // Replaces last frame's offset with `offset` in the live pose.
  void Apply(SPF_Camera_API *camera, const Pose &pose,
             const HeadOffset &offset);

  // Takes the applied offset back out of the live pose (plugin unload). A
  // reloaded plugin starts from a zero applied offset and could never
  // subtract it, so e.g. unloading mid Blindspot Viewer peek would leave
  // the seat leaned forward for good.
  void Remove(SPF_Camera_API *camera);

  // Takes the applied FOV offset back out of the live FOV (game paused).
  // The game saves the FOV per truck as it is when the truck is left, which
  // only happens through a menu: an offset still in it (e.g. mid Blindspot
  // Viewer peek) would come back as the player's own FOV in that truck.
  // False if the write didn't take yet: call again on the next frame.
  bool RemoveFov(SPF_Camera_API *camera);

  // The game rebuilt the interior camera (another truck): the new one holds
  // the player's own pose, without our offset, so there's nothing to
  // subtract any more. Roll is kept: SPF remembers the roll it was given
  // and puts it back on the new camera.
  void ForgetAppliedPose();

  // The offset currently in the live pose.
  const HeadOffset &applied() const { return applied_; }

private:
  // Each writes one channel from its live value, then records `offset` as
  // applied for it. Roll and FOV read their own live value, and keep their
  // applied value if that read fails.
  void WriteSeat(SPF_Camera_API *camera, float x, float y, float z,
                 const HeadOffset &offset);
  void WriteHeadRot(SPF_Camera_API *camera, float yaw_rad, float pitch_rad,
                    const HeadOffset &offset);
  void WriteRoll(SPF_Camera_API *camera, const HeadOffset &offset);
  void WriteFov(SPF_Camera_API *camera, const HeadOffset &offset);

  HeadOffset applied_{};
  // Head rotation we wrote last frame (degrees), to tell the native
  // recenter's snap apart from free-look passing through the default.
  float last_written_yaw_deg_ = 0.0f, last_written_pitch_deg_ = 0.0f;
  bool has_last_written_rot_ = false;
  // Seat position we wrote last frame, to spot another writer's changes.
  float last_written_seat_x_ = 0.0f, last_written_seat_y_ = 0.0f,
        last_written_seat_z_ = 0.0f;
  bool has_last_written_seat_ = false;
  // FOV we (or ManualZoomEffect) wrote last, to spot another writer's.
  float last_written_fov_ = 0.0f;
  bool has_last_written_fov_ = false;
};

} // namespace motioncab
