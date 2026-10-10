#pragma once

#include "SPF_Config_API.h"
#include "SPF_TelemetryData.h"

#include <optional>
#include <span>
#include <string>
#include <utility>

namespace motioncab {

// Where Cabin Walk can take the player in one truck model's cabin. There's
// no cabin geometry to read from the game, so it's calibrated by hand in
// game.
//
// Positions are in the camera's seat frame: meters, x right, y up, z back.
// Angles are degrees, yaw positive to the left, pitch positive up. A
// right-hand drive truck's camera mirrors the seat position but not the yaw
// (seen in game: moving to -x there walks out the driver's door), so the
// centerline is at +x on every truck, and the positions of a left-hand
// drive layout fit a right-hand drive truck as they are.
struct CabinLayout {
  // Where the standing head can go.
  float floor_x_min, floor_x_max, floor_z_min, floor_z_max;
  float stand_y;      // the standing head's height
  float crouch_depth; // under stand_y
  // Offsets from the player's own seat mirrored across the centerline, so
  // the passenger seat follows it as it's adjusted.
  float passenger_dx, passenger_dy, passenger_dz;
  float passenger_yaw, passenger_pitch;
  // On the bunk's front edge, halfway along the mattress.
  float bunk_sit_x, bunk_sit_y, bunk_sit_z, bunk_sit_yaw, bunk_sit_pitch;
  // A folded-up passenger seat, a day cab. Not in CabinLayoutFields, which
  // are the sliders' floats.
  bool has_passenger = true;
  bool has_bunk = true;
};

// The frame's origin is the driver's head. SPF_CabinWalk's passenger seat at
// x = 0.95 puts the centerline about half that far on the trucks it was
// tuned on.
inline constexpr float kTypicalCenterlineX = 0.475f;

// From the default head position, which telemetry gives from the centerline.
// A head closer to it than this is missing data: a driver sits well
// off-center.
inline constexpr float kMinHeadOffsetX = 0.1f;
float CabinCenterlineX(const SPF_TruckConstants &constants);

// For a truck nobody calibrated: the walkway runs down the middle and the
// bunk across the back. Heights and depths are SPF_CabinWalk's defaults,
// since telemetry says nothing about them.
CabinLayout DefaultCabinLayout(float centerline_x = kTypicalCenterlineX);

struct CabinLayoutField {
  const char *name; // the storage key, and "ui.cabin_walk.layout.<name>"
  float CabinLayout::*member;
  float min, max;
  bool angle; // degrees, else meters
};

std::span<const CabinLayoutField> CabinLayoutFields();

// Swaps a min/max pair entered backwards.
void NormalizeCabinLayout(CabinLayout &layout);

// Fits a left-hand drive layout to a right-hand drive truck and back.
CabinLayout WithMirroredYaws(const CabinLayout &layout);

// "<brand_id>_<id>", reduced to [A-Za-z0-9_] to hold as one config key
// segment. Empty while the truck isn't known.
std::string TruckLayoutKey(const SPF_TruckConstants &constants);

// The player's own layouts get a file of their own, since SPF only gives a
// plugin one settings.json. It's only created on the first save.
class CabinLayoutStore {
public:
  CabinLayoutStore(SPF_Config_API *config, std::string path)
      : config_(config), path_(std::move(path)) {}

  std::optional<CabinLayout> LoadPreset(const std::string &truck_key);
  std::optional<CabinLayout> LoadCustom(const std::string &truck_key);
  void Save(const std::string &truck_key, const CabinLayout &layout);
  void Forget(const std::string &truck_key);

private:
  // Null while the file doesn't exist, unless `create`.
  SPF_Config_Handle *Handle(bool create);
  std::optional<CabinLayout> Read(SPF_Config_Handle *h,
                                  const std::string &truck_key) const;

  SPF_Config_API *config_;
  std::string path_;
  SPF_Config_Handle *handle_ = nullptr;
};

} // namespace motioncab
