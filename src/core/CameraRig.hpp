#pragma once

#include "SPF_Camera_API.h"
#include "effects/Effect.hpp"

#include <optional>

namespace motioncab {

// Adds our offset on top of the player's own pose instead of owning it: each
// frame takes last frame's offset back out of the live pose and puts the new
// one in, so free-look and the player's seat settings keep working.
//
// The engine keeps the last offset written even outside the interior view,
// so applied() must not be zeroed on a view switch.
class CameraRig {
public:
  struct Pose {
    float seat_x, seat_y, seat_z;
    float yaw_rad, pitch_rad;
  };

  // Empty while the camera isn't resolved: skip the whole frame then, or a
  // bogus reading would ratchet the seat away.
  std::optional<Pose> ReadPose(SPF_Camera_API *camera) const;

  // The game's recenter hotkey wipes our rotation offset without any event
  // to hook. When it's caught, the applied yaw/pitch are forgotten and the
  // caller should reset the effects.
  bool DetectNativeRecenter(SPF_Camera_API *camera, const Pose &pose);

  // The game's F4 seat menu (or a new camera) sets the seat outright,
  // dropping our offset. The axes that changed are taken as the player's own
  // seat, with nothing left to subtract.
  bool DetectExternalSeatWrite(const Pose &pose);

  // Same for the FOV (F4 slider, a truck switch's new camera). Call before
  // ManualZoomEffect writes this frame's FOV.
  bool DetectExternalFovWrite(SPF_Camera_API *camera);

  // For ManualZoomEffect's own writes, so they aren't taken for another
  // writer's.
  void NoteFovWrite(float fov_deg) {
    last_written_fov_ = fov_deg;
    has_last_written_fov_ = true;
  }

  // The player's own pose, under our offset.
  Pose Base(const Pose &pose) const;

  // Turns the player's own view rather than adding to it (Cabin Walk sitting
  // down, with the mouse blocked meanwhile).
  void SetBaseRotation(Pose &pose, float yaw_rad, float pitch_rad) const;

  void Apply(SPF_Camera_API *camera, const Pose &pose,
             const HeadOffset &offset);

  // For unloading: a reloaded plugin couldn't subtract an offset it never
  // knew about, so unloading mid Blindspot Viewer peek would leave the seat
  // leaned forward for good.
  void Remove(SPF_Camera_API *camera);

  // For the pause menu: the game saves the FOV per truck as it is when the
  // truck is left (always through a menu), so an offset still in it would
  // come back as the player's own FOV. False if the write didn't take yet:
  // try again next frame.
  bool RemoveFov(SPF_Camera_API *camera);

  // For a truck switch: the new camera comes without our offset. Roll is
  // kept, since SPF puts it back on the new camera.
  void ForgetAppliedPose();

  // For a world reload, which rebuilds SPF's camera too, roll included.
  void ForgetApplied();

  const HeadOffset &applied() const { return applied_; }

private:
  // Roll and FOV read their own live value, and keep their applied value if
  // that read fails.
  void WriteSeat(SPF_Camera_API *camera, float x, float y, float z,
                 const HeadOffset &offset);
  void WriteHeadRot(SPF_Camera_API *camera, float yaw_rad, float pitch_rad,
                    const HeadOffset &offset);
  void WriteRoll(SPF_Camera_API *camera, const HeadOffset &offset);
  void WriteFov(SPF_Camera_API *camera, const HeadOffset &offset);

  HeadOffset applied_{};
  // What we last wrote, to spot another writer's changes.
  float last_written_yaw_deg_ = 0.0f, last_written_pitch_deg_ = 0.0f;
  bool has_last_written_rot_ = false;
  float last_written_seat_x_ = 0.0f, last_written_seat_y_ = 0.0f,
        last_written_seat_z_ = 0.0f;
  bool has_last_written_seat_ = false;
  float last_written_fov_ = 0.0f;
  bool has_last_written_fov_ = false;
};

} // namespace motioncab
