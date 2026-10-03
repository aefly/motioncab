#include "CameraRig.hpp"

#include "math/Units.hpp"

#include <cmath>

namespace motioncab {

using math::kDegToRad;

std::optional<CameraRig::Pose>
CameraRig::ReadPose(SPF_Camera_API *camera) const {
  Pose pose{};
  if (!camera->Cam_GetInteriorSeatPos(&pose.seat_x, &pose.seat_y,
                                      &pose.seat_z) ||
      !camera->Cam_GetInteriorHeadRot(&pose.yaw_rad, &pose.pitch_rad))
    return std::nullopt;
  return pose;
}

bool CameraRig::DetectNativeRecenter(SPF_Camera_API *camera, const Pose &pose) {
  // Inferred heuristically: our contribution is non-trivial, and rotation
  // jumps away from what we wrote to land exactly on default. Landing on
  // default alone isn't enough: free-look sweeping through the default
  // while an effect is active (e.g. steering camera) passes within epsilon
  // of it, and a false positive re-bases the pose, snapping the player's
  // own look back to center.
  float default_yaw_deg, default_pitch_deg;
  if (!has_last_written_rot_ || !camera->Cam_GetInteriorRotationDefaults(
                                    &default_yaw_deg, &default_pitch_deg))
    return false;

  constexpr float kEpsilonDeg = 0.01f;
  constexpr float kMeaningfulOffsetDeg = 0.5f;
  auto was_reset = [&](float live_deg, float default_deg, float written_deg,
                       float offset_deg) {
    return std::fabs(offset_deg) > kMeaningfulOffsetDeg &&
           std::fabs(live_deg - written_deg) > kMeaningfulOffsetDeg &&
           std::fabs(live_deg - default_deg) < kEpsilonDeg;
  };
  const bool yaw_was_reset =
      was_reset(pose.yaw_rad / kDegToRad, default_yaw_deg,
                last_written_yaw_deg_, applied_.yaw);
  const bool pitch_was_reset =
      was_reset(pose.pitch_rad / kDegToRad, default_pitch_deg,
                last_written_pitch_deg_, applied_.pitch);
  if (!yaw_was_reset && !pitch_was_reset)
    return false;

  applied_.yaw = 0.0f;
  applied_.pitch = 0.0f;
  return true;
}

bool CameraRig::DetectExternalSeatWrite(const Pose &pose) {
  if (!has_last_written_seat_)
    return false;

  // Far below any change a player or the game makes, far above float
  // round-trip noise through the setter/getter.
  constexpr float kEpsilonMeters = 1e-4f;
  auto rewritten = [&](float live, float written) {
    return std::fabs(live - written) > kEpsilonMeters;
  };
  bool any = false;
  if (rewritten(pose.seat_x, last_written_seat_x_)) {
    applied_.pos_x = 0.0f;
    any = true;
  }
  if (rewritten(pose.seat_y, last_written_seat_y_)) {
    applied_.pos_y = 0.0f;
    any = true;
  }
  if (rewritten(pose.seat_z, last_written_seat_z_)) {
    applied_.pos_z = 0.0f;
    any = true;
  }
  return any;
}

CameraRig::Pose CameraRig::Base(const Pose &pose) const {
  return {pose.seat_x - applied_.pos_x, pose.seat_y - applied_.pos_y,
          pose.seat_z - applied_.pos_z, pose.yaw_rad - applied_.yaw * kDegToRad,
          pose.pitch_rad - applied_.pitch * kDegToRad};
}

void CameraRig::SetBaseRotation(Pose &pose, float yaw_rad,
                                float pitch_rad) const {
  pose.yaw_rad = yaw_rad + applied_.yaw * kDegToRad;
  pose.pitch_rad = pitch_rad + applied_.pitch * kDegToRad;
}

void CameraRig::ForgetAppliedPose() {
  const float roll = applied_.roll;
  applied_ = {};
  applied_.roll = roll;
  has_last_written_rot_ = false;
  has_last_written_seat_ = false;
}

void CameraRig::Apply(SPF_Camera_API *camera, const Pose &pose,
                      const HeadOffset &offset) {
  WriteSeat(camera, pose.seat_x, pose.seat_y, pose.seat_z, offset);
  WriteHeadRot(camera, pose.yaw_rad, pose.pitch_rad, offset);
  WriteRoll(camera, offset);
  WriteFov(camera, offset);
}

void CameraRig::Remove(SPF_Camera_API *camera) {
  // Channel by channel, so one failing getter doesn't keep the others'
  // offset in the pose.
  const HeadOffset none{};
  float x, y, z;
  if (camera->Cam_GetInteriorSeatPos(&x, &y, &z)) {
    // The seat may have been rewritten since our last write without a
    // cabin frame to notice it (F4 seat menu, then unload from another
    // view): subtracting an offset that's no longer there would shift the
    // player's own seat for good.
    DetectExternalSeatWrite({x, y, z, 0.0f, 0.0f});
    WriteSeat(camera, x, y, z, none);
  }
  float yaw_rad, pitch_rad;
  if (camera->Cam_GetInteriorHeadRot(&yaw_rad, &pitch_rad))
    WriteHeadRot(camera, yaw_rad, pitch_rad, none);
  WriteRoll(camera, none);
  WriteFov(camera, none);
  applied_ = {};
}

void CameraRig::WriteSeat(SPF_Camera_API *camera, float x, float y, float z,
                          const HeadOffset &offset) {
  last_written_seat_x_ = x - applied_.pos_x + offset.pos_x;
  last_written_seat_y_ = y - applied_.pos_y + offset.pos_y;
  last_written_seat_z_ = z - applied_.pos_z + offset.pos_z;
  has_last_written_seat_ = true;
  camera->Cam_SetInteriorSeatPos(last_written_seat_x_, last_written_seat_y_,
                                 last_written_seat_z_);
  applied_.pos_x = offset.pos_x;
  applied_.pos_y = offset.pos_y;
  applied_.pos_z = offset.pos_z;
}

void CameraRig::WriteHeadRot(SPF_Camera_API *camera, float yaw_rad,
                             float pitch_rad, const HeadOffset &offset) {
  // HeadOffset.yaw/pitch are degrees (see Effect.hpp); Cam_SetInteriorHeadRot
  // is documented (and its own usage example confirms) to take radians.
  const float base_yaw_rad = yaw_rad - applied_.yaw * kDegToRad;
  const float base_pitch_rad = pitch_rad - applied_.pitch * kDegToRad;
  const float new_yaw_rad = base_yaw_rad + offset.yaw * kDegToRad;
  const float new_pitch_rad = base_pitch_rad + offset.pitch * kDegToRad;
  camera->Cam_SetInteriorHeadRot(new_yaw_rad, new_pitch_rad);
  last_written_yaw_deg_ = new_yaw_rad / kDegToRad;
  last_written_pitch_deg_ = new_pitch_rad / kDegToRad;
  has_last_written_rot_ = true;
  applied_.yaw = offset.yaw;
  applied_.pitch = offset.pitch;
}

void CameraRig::WriteRoll(SPF_Camera_API *camera, const HeadOffset &offset) {
  float roll_deg = 0.0f;
  if (!camera->Cam_GetInteriorRoll(&roll_deg))
    return;
  camera->Cam_SetInteriorRoll(roll_deg - applied_.roll + offset.roll);
  applied_.roll = offset.roll;
}

void CameraRig::WriteFov(SPF_Camera_API *camera, const HeadOffset &offset) {
  // ManualZoomEffect wrote this frame's FOV (if zooming) with the applied
  // FOV offset already added, so subtracting it still recovers the right
  // base. Skipped when neither frame has an offset, to leave the FOV alone
  // for the game's F4 slider.
  if (offset.fov == 0.0f && applied_.fov == 0.0f)
    return;
  float fov_deg = 0.0f;
  if (!camera->Cam_GetInteriorFov(&fov_deg))
    return;
  camera->Cam_SetInteriorFov(fov_deg - applied_.fov + offset.fov);
  applied_.fov = offset.fov;
}

} // namespace motioncab
