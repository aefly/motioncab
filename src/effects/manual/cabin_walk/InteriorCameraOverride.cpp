#include "InteriorCameraOverride.hpp"

#include "math/Units.hpp"

#include <algorithm>

namespace motioncab {

bool InteriorCameraOverride::Engage(SPF_Camera_API *camera) {
  if (engaged_)
    return true;
  if (!camera->Cam_GetInteriorRotationLimits(&limits_.left, &limits_.right,
                                             &limits_.up, &limits_.down) ||
      !camera->Cam_GetInteriorRotationDefaults(&default_yaw_deg_,
                                               &default_pitch_deg_))
    return false;

  float look_yaw_rad = 0.0f, look_pitch_rad = 0.0f;
  camera->Cam_GetInteriorHeadRot(&look_yaw_rad, &look_pitch_rad);

  const size_t count = camera->Cam_GetInteriorAzimuthOverridesCount();
  azimuths_.assign(count, Azimuth{});
  for (size_t i = 0; i < count; ++i) {
    Azimuth &a = azimuths_[i];
    a.valid = camera->Cam_GetInteriorAzimuthOverrideStartAzimuth(i, &a.start) &&
              camera->Cam_GetInteriorAzimuthOverrideEndAzimuth(i, &a.end) &&
              camera->Cam_GetInteriorAzimuthOverrideOutside(i, &a.outside) &&
              camera->Cam_GetInteriorAzimuthOverrideStartHeadOffset(
                  i, &a.start_head[0], &a.start_head[1], &a.start_head[2]) &&
              camera->Cam_GetInteriorAzimuthOverrideEndHeadOffset(
                  i, &a.end_head[0], &a.end_head[1], &a.end_head[2]);
    if (!a.valid)
      continue;
    // An empty range with no offset never kicks in (SPF_CabinWalk's way,
    // seen in game).
    camera->Cam_SetInteriorAzimuthOverrideStartAzimuth(i, 0.0f);
    camera->Cam_SetInteriorAzimuthOverrideEndAzimuth(i, 0.0f);
    camera->Cam_SetInteriorAzimuthOverrideOutside(i, false);
    camera->Cam_SetInteriorAzimuthOverrideStartHeadOffset(i, 0.0f, 0.0f, 0.0f);
    camera->Cam_SetInteriorAzimuthOverrideEndHeadOffset(i, 0.0f, 0.0f, 0.0f);
  }
  has_near_plane_ = camera->Cam_GetInteriorNearPlane(&near_plane_);
  engaged_ = true;
  // The zones are the left-hand drive cabin's, which a right-hand drive
  // camera reads at the mirrored yaw. Their offsets are in the seat frame,
  // which the camera already mirrors.
  const float look_yaw_deg = look_yaw_rad / math::kDegToRad;
  engage_head_offset_ =
      HeadOffsetAt(right_hand_drive_ ? -look_yaw_deg : look_yaw_deg);
  RefreshSoundZones(camera);
  return true;
}

std::array<float, 3> InteriorCameraOverride::HeadOffsetAt(float yaw_deg) const {
  std::array<float, 3> offset{};
  for (const Azimuth &a : azimuths_) {
    if (!a.valid)
      continue;
    // A zone may run past 180 degrees.
    const float lo = std::min(a.start, a.end), hi = std::max(a.start, a.end);
    if (hi <= lo)
      continue;
    for (const float angle : {yaw_deg, yaw_deg + 360.0f, yaw_deg - 360.0f}) {
      if (angle < lo || angle > hi)
        continue;
      const float t = (angle - a.start) / (a.end - a.start);
      for (int k = 0; k < 3; ++k)
        offset[k] = a.start_head[k] + (a.end_head[k] - a.start_head[k]) * t;
      return offset;
    }
  }
  return offset;
}

void InteriorCameraOverride::SetLimits(SPF_Camera_API *camera,
                                       const Limits &limits) {
  if (engaged_)
    camera->Cam_SetInteriorRotationLimits(limits.left, limits.right, limits.up,
                                          limits.down);
}

void InteriorCameraOverride::SetDefaults(SPF_Camera_API *camera, float yaw_deg,
                                         float pitch_deg) {
  if (engaged_)
    camera->Cam_SetInteriorRotationDefaults(yaw_deg, pitch_deg);
}

void InteriorCameraOverride::Restore(SPF_Camera_API *camera) {
  if (!engaged_)
    return;
  camera->Cam_SetInteriorRotationLimits(limits_.left, limits_.right, limits_.up,
                                        limits_.down);
  camera->Cam_SetInteriorRotationDefaults(default_yaw_deg_, default_pitch_deg_);
  const size_t count = camera->Cam_GetInteriorAzimuthOverridesCount();
  for (size_t i = 0; i < count && i < azimuths_.size(); ++i) {
    const Azimuth &a = azimuths_[i];
    if (!a.valid)
      continue;
    camera->Cam_SetInteriorAzimuthOverrideStartAzimuth(i, a.start);
    camera->Cam_SetInteriorAzimuthOverrideEndAzimuth(i, a.end);
    camera->Cam_SetInteriorAzimuthOverrideOutside(i, a.outside);
    camera->Cam_SetInteriorAzimuthOverrideStartHeadOffset(
        i, a.start_head[0], a.start_head[1], a.start_head[2]);
    camera->Cam_SetInteriorAzimuthOverrideEndHeadOffset(
        i, a.end_head[0], a.end_head[1], a.end_head[2]);
  }
  if (has_near_plane_)
    camera->Cam_SetInteriorNearPlane(near_plane_);
  Forget();
  RefreshSoundZones(camera);
}

void InteriorCameraOverride::RefreshSoundZones(SPF_Camera_API *camera) {
  SPF_CameraType type;
  if (!camera->Cam_GetCurrentCamera(&type) || type != SPF_CAMERA_INTERIOR)
    return;
  // Re-activating recenters the head onto the rotation defaults, so they're
  // pointed at the current look first.
  float yaw_rad = 0.0f, pitch_rad = 0.0f;
  if (camera->Cam_GetInteriorHeadRot(&yaw_rad, &pitch_rad))
    RecenterOnto(camera, yaw_rad, pitch_rad);
  camera->Cam_SwitchTo(SPF_CAMERA_INTERIOR);
  ++refreshes_;
}

void InteriorCameraOverride::RecenterOnto(SPF_Camera_API *camera, float yaw_rad,
                                          float pitch_rad) {
  const float yaw_deg = yaw_rad / math::kDegToRad;
  camera->Cam_SetInteriorRotationDefaults(
      right_hand_drive_ ? -yaw_deg : yaw_deg, pitch_rad / math::kDegToRad);
}

void InteriorCameraOverride::EndRefresh(SPF_Camera_API *camera) {
  // Both refreshes happen in the driver's seat, whose defaults are the
  // truck's own, still known after Restore.
  camera->Cam_SetInteriorRotationDefaults(default_yaw_deg_, default_pitch_deg_);
}

void InteriorCameraOverride::KeepNearPlane(SPF_Camera_API *camera) {
  // Re-activating the camera may put its own back.
  if (engaged_ && has_near_plane_)
    camera->Cam_SetInteriorNearPlane(away_near_plane());
}

void InteriorCameraOverride::Forget() {
  engaged_ = false;
  has_near_plane_ = false;
  azimuths_.clear();
}

} // namespace motioncab
