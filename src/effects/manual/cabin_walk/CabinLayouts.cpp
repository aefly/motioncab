#include "CabinLayouts.hpp"

#include "effects/manual/cabin_walk/CabinPresets.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

namespace motioncab {

namespace {

using L = CabinLayout;
using Unit = CabinLayoutField::Unit;

template <float L::*M> float &Member(L &l) { return l.*M; }
template <int I> float &StepZ(L &l) { return l.aisle_steps[I].z; }
template <int I> float &StepHeight(L &l) { return l.aisle_steps[I].height; }

constexpr CabinLayoutField kFields[] = {
    {"floor_x_min", &Member<&L::floor_x_min>, -1.5f, 2.0f, Unit::kMeters},
    {"floor_x_max", &Member<&L::floor_x_max>, -1.5f, 2.0f, Unit::kMeters},
    {"floor_z_min", &Member<&L::floor_z_min>, -1.5f, 2.5f, Unit::kMeters},
    {"floor_z_max", &Member<&L::floor_z_max>, -1.5f, 15.0f, Unit::kMeters},
    {"stand_y", &Member<&L::stand_y>, -0.5f, 1.0f, Unit::kMeters},
    {"crouch_depth", &Member<&L::crouch_depth>, 0.1f, 0.8f, Unit::kMeters},
    {"passenger_dx", &Member<&L::passenger_dx>, -0.4f, 0.4f, Unit::kMeters},
    {"passenger_dy", &Member<&L::passenger_dy>, -0.4f, 0.4f, Unit::kMeters},
    {"passenger_dz", &Member<&L::passenger_dz>, -0.4f, 0.4f, Unit::kMeters},
    {"passenger_yaw", &Member<&L::passenger_yaw>, -180.0f, 180.0f,
     Unit::kDegrees},
    {"passenger_pitch", &Member<&L::passenger_pitch>, -80.0f, 80.0f,
     Unit::kDegrees},
    {"bunk_sit_x", &Member<&L::bunk_sit_x>, -1.5f, 2.0f, Unit::kMeters},
    {"bunk_sit_y", &Member<&L::bunk_sit_y>, -1.0f, 1.0f, Unit::kMeters},
    {"bunk_sit_z", &Member<&L::bunk_sit_z>, -1.5f, 2.5f, Unit::kMeters},
    {"bunk_sit_yaw", &Member<&L::bunk_sit_yaw>, -180.0f, 180.0f,
     Unit::kDegrees},
    {"bunk_sit_pitch", &Member<&L::bunk_sit_pitch>, -80.0f, 80.0f,
     Unit::kDegrees},
    {"aisle_step_count", &Member<&L::aisle_step_count>, 0.0f,
     static_cast<float>(kMaxAisleSteps), Unit::kCount},
    {"aisle_step_1_z", &StepZ<0>, -1.5f, 15.0f, Unit::kMeters},
    {"aisle_step_1_height", &StepHeight<0>, -0.6f, 0.6f, Unit::kMeters},
    {"aisle_step_2_z", &StepZ<1>, -1.5f, 15.0f, Unit::kMeters},
    {"aisle_step_2_height", &StepHeight<1>, -0.6f, 0.6f, Unit::kMeters},
    {"aisle_step_3_z", &StepZ<2>, -1.5f, 15.0f, Unit::kMeters},
    {"aisle_step_3_height", &StepHeight<2>, -0.6f, 0.6f, Unit::kMeters},
    {"aisle_step_4_z", &StepZ<3>, -1.5f, 15.0f, Unit::kMeters},
    {"aisle_step_4_height", &StepHeight<3>, -0.6f, 0.6f, Unit::kMeters},
    {"aisle_step_5_z", &StepZ<4>, -1.5f, 15.0f, Unit::kMeters},
    {"aisle_step_5_height", &StepHeight<4>, -0.6f, 0.6f, Unit::kMeters},
    {"aisle_step_6_z", &StepZ<5>, -1.5f, 15.0f, Unit::kMeters},
    {"aisle_step_6_height", &StepHeight<5>, -0.6f, 0.6f, Unit::kMeters},
};

static_assert(kMaxAisleSteps == 6, "kFields lists six steps");

std::string Key(const std::string &truck_key, const char *field) {
  return "layouts." + truck_key + "." + field;
}

} // namespace

float CabinCenterlineX(const SPF_TruckConstants &constants) {
  const float head_x = constants.head_position.x;
  // Whichever side the driver sits on, since a right-hand drive truck's
  // camera frame is mirrored.
  return std::fabs(head_x) >= kMinHeadOffsetX ? std::fabs(head_x)
                                              : kTypicalCenterlineX;
}

CabinLayout DefaultCabinLayout(float centerline_x) {
  const float c = centerline_x;
  constexpr float kWalkwayHalfWidth = 0.2f;
  CabinLayout l{};
  l.floor_x_min = c - kWalkwayHalfWidth;
  l.floor_x_max = c + kWalkwayHalfWidth;
  l.floor_z_min = -0.5f;
  l.floor_z_max = 0.6f;
  l.stand_y = 0.2f;
  l.crouch_depth = 0.45f;
  l.passenger_dx = 0.0f;
  l.passenger_dy = 0.0f;
  l.passenger_dz = 0.0f;
  l.passenger_yaw = 0.0f;
  l.passenger_pitch = 0.0f;
  l.bunk_sit_x = c;
  l.bunk_sit_y = 0.0f;
  l.bunk_sit_z = 0.8f;
  l.bunk_sit_yaw = 0.0f;
  l.bunk_sit_pitch = 0.0f;
  return l;
}

std::span<const CabinLayoutField> CabinLayoutFields() { return kFields; }

const CabinLayoutField *FindCabinLayoutField(std::string_view name) {
  for (const CabinLayoutField &f : kFields)
    if (name == f.name)
      return &f;
  return nullptr;
}

int AisleStepCount(const CabinLayout &layout) {
  return std::clamp(static_cast<int>(std::lround(layout.aisle_step_count)), 0,
                    kMaxAisleSteps);
}

std::array<CabinStep, kMaxAisleSteps>
AisleStepsFrontToBack(const CabinLayout &layout, int &count) {
  std::array<CabinStep, kMaxAisleSteps> steps = layout.aisle_steps;
  count = AisleStepCount(layout);
  // An insertion sort: on so small an array, GCC takes std::sort for an
  // out-of-bounds access (a false positive).
  for (int i = 1; i < count; ++i)
    for (int j = i; j > 0 && steps[j].z < steps[j - 1].z; --j)
      std::swap(steps[j], steps[j - 1]);
  return steps;
}

float AisleRiseAt(const CabinLayout &layout, float z) {
  if (!layout.has_aisle)
    return 0.0f;
  float rise = 0.0f;
  for (int i = 0, n = AisleStepCount(layout); i < n; ++i)
    if (layout.aisle_steps[i].z < z)
      rise += layout.aisle_steps[i].height;
  return rise;
}

CabinLayout WithMirroredYaws(const CabinLayout &layout) {
  CabinLayout m = layout;
  m.passenger_yaw = -layout.passenger_yaw;
  m.bunk_sit_yaw = -layout.bunk_sit_yaw;
  return m;
}

void NormalizeCabinLayout(CabinLayout &layout) {
  if (layout.floor_x_min > layout.floor_x_max)
    std::swap(layout.floor_x_min, layout.floor_x_max);
  if (layout.floor_z_min > layout.floor_z_max)
    std::swap(layout.floor_z_min, layout.floor_z_max);
  layout.aisle_step_count = static_cast<float>(AisleStepCount(layout));
}

SPF_Config_Handle *CabinLayoutStore::Handle(bool create) {
  if (handle_ || !config_ || path_.empty())
    return handle_;
  // Opening a custom context creates a missing file, so not before there's
  // a layout to write.
  std::error_code ec;
  if (create || std::filesystem::exists(path_, ec))
    handle_ = config_->Cfg_CreateCustomContext(path_.c_str());
  return handle_;
}

std::optional<CabinLayout>
CabinLayoutStore::Read(SPF_Config_Handle *h,
                       const std::string &truck_key) const {
  // Save writes every field, so only an emptied layout lacks this one.
  if (!h || !config_->Cfg_HasKey(h, Key(truck_key, "floor_x_min").c_str()))
    return std::nullopt;
  CabinLayout layout = DefaultCabinLayout();
  for (const CabinLayoutField &f : kFields)
    f.Of(layout) = static_cast<float>(
        config_->Cfg_GetFloat(h, Key(truck_key, f.name).c_str(), f.Of(layout)));
  layout.has_passenger = config_->Cfg_GetBool(
      h, Key(truck_key, "has_passenger").c_str(), layout.has_passenger);
  layout.has_bunk = config_->Cfg_GetBool(h, Key(truck_key, "has_bunk").c_str(),
                                         layout.has_bunk);
  layout.has_aisle = config_->Cfg_GetBool(
      h, Key(truck_key, "has_aisle").c_str(), layout.has_aisle);
  NormalizeCabinLayout(layout);
  return layout;
}

std::optional<CabinLayout>
CabinLayoutStore::LoadCustom(const std::string &truck_key) {
  if (truck_key.empty())
    return std::nullopt;
  return Read(Handle(false), truck_key);
}

std::optional<CabinLayout>
CabinLayoutStore::LoadPreset(const std::string &truck_key) {
  const CabinLayout *preset = FindCabinPreset(truck_key);
  if (!preset)
    return std::nullopt;
  CabinLayout layout = *preset;
  NormalizeCabinLayout(layout);
  return layout;
}

void CabinLayoutStore::Save(const std::string &truck_key,
                            const CabinLayout &layout) {
  SPF_Config_Handle *h = truck_key.empty() ? nullptr : Handle(true);
  if (!h)
    return;
  for (const CabinLayoutField &f : kFields)
    config_->Cfg_SetFloat(h, Key(truck_key, f.name).c_str(), f.Of(layout));
  config_->Cfg_SetBool(h, Key(truck_key, "has_passenger").c_str(),
                       layout.has_passenger);
  config_->Cfg_SetBool(h, Key(truck_key, "has_bunk").c_str(), layout.has_bunk);
  config_->Cfg_SetBool(h, Key(truck_key, "has_aisle").c_str(),
                       layout.has_aisle);
  config_->Cfg_Save(h);
}

void CabinLayoutStore::Forget(const std::string &truck_key) {
  SPF_Config_Handle *h = truck_key.empty() ? nullptr : Handle(false);
  if (!h)
    return;
  // Emptied rather than removed: Cfg_RemoveKey doesn't work on a custom
  // context file. An empty layout reads as none.
  config_->Cfg_SetJsonString(h, ("layouts." + truck_key).c_str(), "{}");
  config_->Cfg_Save(h);
}

} // namespace motioncab
