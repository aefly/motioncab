#pragma once

#include "SPF_Config_API.h"
#include "SPF_TelemetryData.h"

#include <optional>
#include <span>
#include <string>
#include <utility>

namespace motioncab {

// Where Cabin Walk can take the player in one truck's cabin. The game has
// no cabin geometry to read, so it's calibrated in-game per truck model
// (the Quick Settings' Advanced part) and falls back to a generic layout
// until then.
//
// Every position is in the interior camera's seat-position frame, as
// Cam_GetInteriorSeatPos reads it: meters, x right, y up, z backward.
// Angles are degrees, as the head rotation: yaw positive to the left,
// pitch positive up. A right-hand drive truck's interior camera mirrors
// the seat position left to right (seen in game: moving to -x there walks
// out of the driver's door), but not the yaw. So in that frame the cabin's
// centerline is at +x on every truck, angles keep their meaning, and a
// left-hand drive layout fits a right-hand drive truck of the same model
// as it is.
struct CabinLayout {
  // The area the standing head can walk over.
  float floor_x_min, floor_x_max, floor_z_min, floor_z_max;
  float stand_y;      // standing head height
  float crouch_depth; // how far below it crouching takes the head
  // The passenger seat: its position mirrors the player's own seat across
  // the centerline (see CabinWalkEffect::SpotAt), following the seat as it's
  // adjusted, moved by these offsets (meters: right, up, back), and its
  // look.
  float passenger_dx, passenger_dy, passenger_dz;
  float passenger_yaw, passenger_pitch;
  // Sitting on the bunk's front edge, halfway along the mattress.
  float bunk_sit_x, bunk_sit_y, bunk_sit_z, bunk_sit_yaw, bunk_sit_pitch;
  // False where the passenger seat can't be sat in (folded up, or none), or
  // there's no bunk (a day cab): Cabin Walk never takes the player there.
  // Not CabinLayoutFields (those are the sliders' floats); stored under
  // their own names.
  bool has_passenger = true;
  bool has_bunk = true;
};

// The cabin's centerline, as an x in CabinLayout's frame: the driver's head
// (the frame's origin) sits this far from it, always at a positive x (see
// CabinLayout). SPF_CabinWalk's passenger seat at x = 0.95 puts it at about
// half that on the trucks it was tuned on.
inline constexpr float kTypicalCenterlineX = 0.475f;

// The truck's centerline from its default head position, which telemetry
// gives relative to the cabin's pivot, on the centerline. A head closer
// than kMinHeadOffsetX to it is taken as missing data (a driver sits well
// off-center), and the typical centerline is used instead.
inline constexpr float kMinHeadOffsetX = 0.1f;
float CabinCenterlineX(const SPF_TruckConstants &constants);

// A layout for a truck nobody calibrated, built around its centerline: the
// walkway runs down the middle, and the bunk sits across the back. Heights
// and depths are SPF_CabinWalk's defaults, since telemetry says nothing
// about them.
CabinLayout DefaultCabinLayout(float centerline_x = kTypicalCenterlineX);

// One CabinLayout field: its name (storage key and "ui.cabin_walk.layout.
// <name>" label), its slider's range and its unit.
struct CabinLayoutField {
  const char *name;
  float CabinLayout::*member;
  float min, max;
  bool angle; // degrees, else meters
};

std::span<const CabinLayoutField> CabinLayoutFields();

// Swaps any min/max pair entered backwards, so the floor stays a valid box.
void NormalizeCabinLayout(CabinLayout &layout);

// `layout` with its spots' looks mirrored: a left-hand drive layout's looks,
// for a right-hand drive truck (its positions already fit, see above).
CabinLayout WithMirroredYaws(const CabinLayout &layout);

// "<brand_id>_<id>" of the truck model, reduced to [A-Za-z0-9_] so it
// holds as one config key segment; empty if the truck isn't known yet.
std::string TruckLayoutKey(const SPF_TruckConstants &constants);

// The calibrated layouts, one per truck model: the player's own, in their
// own JSON file (SPF only gives a plugin one settings.json), created on the
// first save, and the presets compiled into the plugin (CabinPresets.hpp).
class CabinLayoutStore {
public:
  CabinLayoutStore(SPF_Config_API *config, std::string path)
      : config_(config), path_(std::move(path)) {}

  // Only the preset shipped with the plugin for `truck_key`, if any.
  std::optional<CabinLayout> LoadPreset(const std::string &truck_key);
  // Only the player's own layout for `truck_key`, if any.
  std::optional<CabinLayout> LoadCustom(const std::string &truck_key);
  // As the player's layout.
  void Save(const std::string &truck_key, const CabinLayout &layout);
  // Drops the player's layout, back to the preset if any: its entry is
  // emptied (Cfg_RemoveKey doesn't work on a custom context file).
  void Forget(const std::string &truck_key);

private:
  // The player's file, opened on first use; null while it doesn't exist,
  // unless `create` (opening a custom context creates a missing file).
  SPF_Config_Handle *Handle(bool create);
  std::optional<CabinLayout> Read(SPF_Config_Handle *h,
                                  const std::string &truck_key) const;

  SPF_Config_API *config_;
  std::string path_;
  SPF_Config_Handle *handle_ = nullptr;
};

} // namespace motioncab
