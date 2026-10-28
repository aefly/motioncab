#include "CameraRig.hpp"

#include "core/PluginContext.hpp"
#include "math/Units.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

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
  // No event for it, so it's a guess: the game snaps or glides the rotation
  // to the default (both seen in game), so look for a run of frames moved by
  // someone else, each closer to the default, that stops on it.
  float default_yaw_deg, default_pitch_deg;
  if (!has_last_written_rot_ || !camera->Cam_GetInteriorRotationDefaults(
                                    &default_yaw_deg, &default_pitch_deg)) {
    recenter_run_ = {};
    return false;
  }

  const float live_yaw_deg = pose.yaw_rad / kDegToRad;
  const float live_pitch_deg = pose.pitch_rad / kDegToRad;
  auto distance = [&](float yaw_deg, float pitch_deg) {
    return std::hypot(yaw_deg - default_yaw_deg, pitch_deg - default_pitch_deg);
  };

  // Well above float round-trip noise.
  constexpr float kMovedDeg = 0.05f;
  if (std::fabs(live_yaw_deg - last_written_yaw_deg_) > kMovedDeg ||
      std::fabs(live_pitch_deg - last_written_pitch_deg_) > kMovedDeg) {
    if (!recenter_run_.active)
      recenter_run_ = {.active = true, .approaching = true};
    recenter_run_.approaching =
        recenter_run_.approaching &&
        distance(live_yaw_deg, live_pitch_deg) <
            distance(last_written_yaw_deg_, last_written_pitch_deg_);
    recenter_run_.max_offset_deg =
        std::max({recenter_run_.max_offset_deg, std::fabs(applied_.yaw),
                  std::fabs(applied_.pitch)});
    return false;
  }

  const RecenterRun run = std::exchange(recenter_run_, {});
  // The landing isn't exact when the effects move during the game's glide.
  constexpr float kMeaningfulOffsetDeg = 0.5f;
  constexpr float kLandedDeg = 0.5f;
  if (!run.active || !run.approaching ||
      run.max_offset_deg <= kMeaningfulOffsetDeg ||
      distance(live_yaw_deg, live_pitch_deg) > kLandedDeg)
    return false;

  applied_.yaw = 0.0f;
  applied_.pitch = 0.0f;
  return true;
}

bool CameraRig::DetectExternalSeatWrite(const Pose &pose) {
  if (!has_last_written_seat_)
    return false;

  // Well below any real change, well above float round-trip noise.
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

bool CameraRig::DetectExternalFovWrite(SPF_Camera_API *camera) {
  float fov_deg = 0.0f;
  if (!has_last_written_fov_ || !camera->Cam_GetInteriorFov(&fov_deg))
    return false;
  constexpr float kEpsilonDeg = 0.01f;
  if (std::fabs(fov_deg - last_written_fov_) <= kEpsilonDeg)
    return false;
  applied_.fov = 0.0f;
  last_written_fov_ = fov_deg;
  return true;
}

void CameraRig::UpdateRecenterGlide(SPF_Camera_API *camera, float dt,
                                    Pose &pose) {
  if (!recenter_glide_.active)
    return;
  float default_yaw_deg, default_pitch_deg;
  if (!camera->Cam_GetInteriorRotationDefaults(&default_yaw_deg,
                                               &default_pitch_deg)) {
    recenter_glide_ = {};
    return;
  }

  RecenterGlide &glide = recenter_glide_;
  const Pose base = Base(pose);
  const float default_yaw_rad = default_yaw_deg * kDegToRad;
  const float default_pitch_rad = default_pitch_deg * kDegToRad;
  if (!glide.started) {
    glide.started = true;
    glide_yaw_intent_.Reset(base.yaw_rad - default_yaw_rad);
    glide_yaw_.Reset(base.yaw_rad - default_yaw_rad);
    glide_pitch_intent_.Reset(base.pitch_rad - default_pitch_rad);
    glide_pitch_.Reset(base.pitch_rad - default_pitch_rad);
  } else {
    constexpr float kMovedRad = 0.05f * kDegToRad;
    if (std::fabs(base.yaw_rad - glide.set_yaw_rad) > kMovedRad ||
        std::fabs(base.pitch_rad - glide.set_pitch_rad) > kMovedRad) {
      recenter_glide_ = {};
      return;
    }
  }

  const float yaw_rad =
      glide_yaw_.Update(glide_yaw_intent_.Update(0.0f, dt), dt);
  const float pitch_rad =
      glide_pitch_.Update(glide_pitch_intent_.Update(0.0f, dt), dt);
  constexpr float kSettledRad = 0.01f * kDegToRad;
  const bool settled = std::fabs(yaw_rad) < kSettledRad &&
                       std::fabs(pitch_rad) < kSettledRad &&
                       std::fabs(glide_yaw_intent_.value()) < kSettledRad &&
                       std::fabs(glide_pitch_intent_.value()) < kSettledRad;
  glide.set_yaw_rad = default_yaw_rad + (settled ? 0.0f : yaw_rad);
  glide.set_pitch_rad = default_pitch_rad + (settled ? 0.0f : pitch_rad);
  SetBaseRotation(pose, glide.set_yaw_rad, glide.set_pitch_rad);
  if (settled)
    recenter_glide_ = {};
}

void CameraRig::SetRecenterSmoothing(float intent_seconds, float head_seconds) {
  glide_yaw_intent_.SetTimeConstant(intent_seconds);
  glide_pitch_intent_.SetTimeConstant(intent_seconds);
  glide_yaw_.SetTimeConstant(head_seconds);
  glide_pitch_.SetTimeConstant(head_seconds);
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
  ForgetApplied();
  applied_.roll = roll;
}

void CameraRig::ForgetApplied() {
  applied_ = {};
  has_last_written_rot_ = false;
  has_last_written_seat_ = false;
  has_last_written_fov_ = false;
}

void CameraRig::Apply(SPF_Camera_API *camera, const Pose &pose,
                      const HeadOffset &offset) {
  // Last line of defense, for whatever the per-effect check in EffectManager
  // doesn't see (Cabin Walk, a bad live pose): keep last frame's offset.
  if (!IsFinite(offset) || !std::isfinite(pose.seat_x) ||
      !std::isfinite(pose.seat_y) || !std::isfinite(pose.seat_z) ||
      !std::isfinite(pose.yaw_rad) || !std::isfinite(pose.pitch_rad)) {
    Context().LogThrottledFmt(SPF_LOG_ERROR, "camera_rig_non_finite", 10000,
                              "Skipped a camera write with a non-finite value");
    return;
  }
  WriteSeat(camera, pose.seat_x, pose.seat_y, pose.seat_z, offset);
  WriteHeadRot(camera, pose.yaw_rad, pose.pitch_rad, offset);
  WriteRoll(camera, offset);
  WriteFov(camera, offset);
}

void CameraRig::Remove(SPF_Camera_API *camera) {
  // Channel by channel, so one failing getter doesn't leave the others'
  // offset behind.
  const HeadOffset none{};
  float x, y, z;
  if (camera->Cam_GetInteriorSeatPos(&x, &y, &z)) {
    // The F4 menu may have rewritten the seat while we weren't looking
    // (unloading from another view): subtracting an offset that's no longer
    // there would move the player's seat for good.
    DetectExternalSeatWrite({x, y, z, 0.0f, 0.0f});
    WriteSeat(camera, x, y, z, none);
  }
  float yaw_rad, pitch_rad;
  if (camera->Cam_GetInteriorHeadRot(&yaw_rad, &pitch_rad))
    WriteHeadRot(camera, yaw_rad, pitch_rad, none);
  WriteRoll(camera, none);
  DetectExternalFovWrite(camera);
  WriteFov(camera, none);
  applied_ = {};
}

bool CameraRig::RemoveFov(SPF_Camera_API *camera) {
  DetectExternalFovWrite(camera);
  if (applied_.fov == 0.0f)
    return true;
  float fov_deg = 0.0f;
  if (!camera->Cam_GetInteriorFov(&fov_deg))
    return false;
  const float base_fov_deg = fov_deg - applied_.fov;
  camera->Cam_SetInteriorFov(base_fov_deg);
  // Only forgotten once the write has taken, or an offset still in the FOV
  // would become the player's own.
  float check_deg = 0.0f;
  if (!camera->Cam_GetInteriorFov(&check_deg) ||
      std::fabs(check_deg - base_fov_deg) > 0.01f)
    return false;
  NoteFovWrite(base_fov_deg);
  applied_.fov = 0.0f;
  return true;
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
  // SPF adds the offset to the game's own roll (a head tracker's), so there's
  // nothing to subtract. Only an exact 0 hands the roll back to the game's
  // code, hence no rounding residue.
  constexpr float kRollEpsilonDeg = 1e-3f;
  const float target_deg =
      std::fabs(offset.roll) < kRollEpsilonDeg ? 0.0f : offset.roll;
  camera->Cam_SetInteriorRollOffset(target_deg);
  applied_.roll = target_deg;
}

void CameraRig::WriteFov(SPF_Camera_API *camera, const HeadOffset &offset) {
  // A zooming ManualZoomEffect wrote this frame's FOV with our offset already
  // in it, so subtracting it still gives the right base. With no offset
  // either way, the FOV is left alone for the F4 slider.
  if (offset.fov == 0.0f && applied_.fov == 0.0f)
    return;
  float fov_deg = 0.0f;
  if (!camera->Cam_GetInteriorFov(&fov_deg))
    return;
  const float new_fov_deg = fov_deg - applied_.fov + offset.fov;
  camera->Cam_SetInteriorFov(new_fov_deg);
  NoteFovWrite(new_fov_deg);
  applied_.fov = offset.fov;
}

} // namespace motioncab
