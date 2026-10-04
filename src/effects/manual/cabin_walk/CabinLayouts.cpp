#include "CabinLayouts.hpp"

#include "effects/manual/cabin_walk/CabinPresets.hpp"

#include <cctype>
#include <cmath>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

namespace motioncab {

namespace {

using L = CabinLayout;
// The ranges the sliders allow, meters or degrees.
constexpr CabinLayoutField kFields[] = {
    {"floor_x_min", &L::floor_x_min, -1.5f, 2.0f, false},
    {"floor_x_max", &L::floor_x_max, -1.5f, 2.0f, false},
    {"floor_z_min", &L::floor_z_min, -1.5f, 2.5f, false},
    {"floor_z_max", &L::floor_z_max, -1.5f, 2.5f, false},
    {"stand_y", &L::stand_y, -0.5f, 1.0f, false},
    {"crouch_depth", &L::crouch_depth, 0.1f, 0.8f, false},
    {"passenger_dx", &L::passenger_dx, -0.4f, 0.4f, false},
    {"passenger_dy", &L::passenger_dy, -0.4f, 0.4f, false},
    {"passenger_dz", &L::passenger_dz, -0.4f, 0.4f, false},
    {"passenger_yaw", &L::passenger_yaw, -180.0f, 180.0f, true},
    {"passenger_pitch", &L::passenger_pitch, -80.0f, 80.0f, true},
    {"bunk_sit_x", &L::bunk_sit_x, -1.5f, 2.0f, false},
    {"bunk_sit_y", &L::bunk_sit_y, -1.0f, 1.0f, false},
    {"bunk_sit_z", &L::bunk_sit_z, -1.5f, 2.5f, false},
    {"bunk_sit_yaw", &L::bunk_sit_yaw, -180.0f, 180.0f, true},
    {"bunk_sit_pitch", &L::bunk_sit_pitch, -80.0f, 80.0f, true},
};

std::string Key(const std::string &truck_key, const char *field) {
  return "layouts." + truck_key + "." + field;
}

} // namespace

float CabinCenterlineX(const SPF_TruckConstants &constants) {
  const float head_x = constants.head_position.x;
  // Whichever side the driver sits on: the camera frame of a right-hand
  // drive truck is mirrored (see CabinLayout).
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
}

std::string TruckLayoutKey(const SPF_TruckConstants &constants) {
  auto clean = [](const char *s) {
    std::string out;
    for (; *s; ++s)
      out += std::isalnum(static_cast<unsigned char>(*s)) ? *s : '_';
    return out;
  };
  const std::string brand = clean(constants.brand_id);
  const std::string model = clean(constants.id);
  if (brand.empty() && model.empty())
    return {};
  return brand + "_" + model;
}

SPF_Config_Handle *CabinLayoutStore::Handle(bool create) {
  if (handle_ || !config_ || path_.empty())
    return handle_;
  // Opening a custom context creates a missing file: only once there's a
  // layout to write.
  std::error_code ec;
  if (create || std::filesystem::exists(path_, ec))
    handle_ = config_->Cfg_CreateCustomContext(path_.c_str());
  return handle_;
}

std::optional<CabinLayout>
CabinLayoutStore::Read(SPF_Config_Handle *h,
                       const std::string &truck_key) const {
  // A layout always has its walkway (Save writes every field);
  // an emptied one (see Forget) doesn't.
  if (!h || !config_->Cfg_HasKey(h, Key(truck_key, "floor_x_min").c_str()))
    return std::nullopt;
  CabinLayout layout = DefaultCabinLayout();
  for (const CabinLayoutField &f : kFields)
    layout.*f.member = static_cast<float>(config_->Cfg_GetFloat(
        h, Key(truck_key, f.name).c_str(), layout.*f.member));
  layout.has_passenger = config_->Cfg_GetBool(
      h, Key(truck_key, "has_passenger").c_str(), layout.has_passenger);
  layout.has_bunk = config_->Cfg_GetBool(h, Key(truck_key, "has_bunk").c_str(),
                                         layout.has_bunk);
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
    config_->Cfg_SetFloat(h, Key(truck_key, f.name).c_str(), layout.*f.member);
  config_->Cfg_SetBool(h, Key(truck_key, "has_passenger").c_str(),
                       layout.has_passenger);
  config_->Cfg_SetBool(h, Key(truck_key, "has_bunk").c_str(), layout.has_bunk);
  config_->Cfg_Save(h);
}

void CabinLayoutStore::Forget(const std::string &truck_key) {
  SPF_Config_Handle *h = truck_key.empty() ? nullptr : Handle(false);
  if (!h)
    return;
  // Emptied rather than removed: Cfg_RemoveKey doesn't work on a custom
  // context file (see Profiles.cpp). An empty layout reads as none.
  config_->Cfg_SetJsonString(h, ("layouts." + truck_key).c_str(), "{}");
  config_->Cfg_Save(h);
}

} // namespace motioncab
