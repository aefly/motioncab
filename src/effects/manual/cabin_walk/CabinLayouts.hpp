#pragma once

#include "SPF_Config_API.h"
#include "SPF_TelemetryData.h"

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace motioncab {

inline constexpr int kMaxAisleSteps = 6;

// A change of floor height along a bus's aisle, at a depth.
struct CabinStep {
  float z;      // its edge's depth
  float height; // up past it going back, negative going down
};

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
  // A bus's floor has steps along its aisle (the walkway, running back),
  // counting wherever they are. Off on every truck, so their presets leave
  // these out.
  bool has_aisle = false;
  // A float, to sit among the sliders' values. Steps past it are unused.
  float aisle_step_count = 0.0f;
  std::array<CabinStep, kMaxAisleSteps> aisle_steps{};
};

int AisleStepCount(const CabinLayout &layout);
// The steps in use, in their order along the aisle: the player sets them in
// any order.
std::array<CabinStep, kMaxAisleSteps>
AisleStepsFrontToBack(const CabinLayout &layout, int &count);
// The floor's height at depth `z` over the walkway's front: the steps in
// front of it.
float AisleRiseAt(const CabinLayout &layout, float z);

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
  enum class Unit { kMeters, kDegrees, kCount };

  const char *name; // the storage key, and "ui.cabin_walk.layout.<name>"
  // A function rather than a member pointer, which can't reach into the
  // steps' array.
  float &(*ref)(CabinLayout &);
  float min, max;
  Unit unit;

  float &Of(CabinLayout &layout) const { return ref(layout); }
  float Of(const CabinLayout &layout) const {
    return ref(const_cast<CabinLayout &>(layout));
  }
};

std::span<const CabinLayoutField> CabinLayoutFields();

// Null for a name no field has. A step's are "aisle_step_<n>_z" and
// "aisle_step_<n>_height", n from 1.
const CabinLayoutField *FindCabinLayoutField(std::string_view name);

// Swaps a min/max pair entered backwards.
void NormalizeCabinLayout(CabinLayout &layout);

// Fits a left-hand drive layout to a right-hand drive truck and back.
CabinLayout WithMirroredYaws(const CabinLayout &layout);

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
