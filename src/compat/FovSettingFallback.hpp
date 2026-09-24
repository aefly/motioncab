#pragma once

#include "SPF_Camera_API.h"

// TEMPORARY workaround. Delete this module (and its uses in
// ManualZoomEffect) once an SPF Framework release newer than 1.2.3 ships
// https://github.com/TrackAndTruckDevs/SPF-Framework/pull/11, and bump
// Info_SetMinFrameworkVersion to that release.
//
// Game 1.61.1.1s moved the code SPF 1.2.3 anchors its interior FOV offsets
// on, so Cam_GetInteriorFov/Cam_SetInteriorFov (and the FOV setting API)
// silently stop working and Manual Zoom does nothing. This redoes that PR's
// signature scan through SPF_Hooks_API and reads/writes the game's FOV
// setting directly. It's only used when Cam_GetInteriorFov fails while the
// interior camera itself is resolved, so a fixed SPF takes over on its own.
//
// SPF 1.2.3 doesn't find the zoom base (default) FOV either, so it's
// estimated as the live FOV minus the setting, re-sampled while the truck
// is stationary, where the game's speed-dependent FOV term is zero.
namespace motioncab::compat::fov_fallback {

// Interior FOV as base + setting, like Cam_GetInteriorFov. The base
// estimate is only refreshed when allow_rebase is true: pass false while
// holding an overridden FOV that will be restored later, so the restore
// lands exactly on the player's original setting.
bool GetFov(SPF_Camera_API *camera_api, bool allow_rebase, float *out_fov);

// Writes the FOV setting so the game composes `fov`, like
// Cam_SetInteriorFov. Needs a prior successful GetFov().
bool SetFov(float fov);

} // namespace motioncab::compat::fov_fallback
