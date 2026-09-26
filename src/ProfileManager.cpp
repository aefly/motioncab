#include "ProfileManager.hpp"

#include "PluginContext.hpp"
#include "SPF_Environment_API.h"
#include "ui/SettingsDefaults.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iterator>

namespace motioncab::profiles {

namespace {

namespace fs = std::filesystem;

constexpr size_t kMaxNameLength = 40;

// Config keys are flat dotted strings, not a queryable nested JSON node, so
// profiles copy every known key individually instead of one bulk JSON blob.
// Mirrors the key list already spread across Manifest.cpp/SettingsWindow.cpp;
// defaults reuse the same constants as the reset buttons
// (ui/SettingsDefaults.hpp).
struct BoolKey {
  const char *key;
  bool default_value;
};
struct FloatKey {
  const char *key;
  float default_value;
};

constexpr BoolKey kBoolKeys[] = {
    {"settings.driving.head_motion.enabled", true},
    {"settings.driving.steering_camera.enabled", true},
    {"settings.cabin.idle_breathing.enabled", true},
    {"settings.road.suspension.enabled", true},
    {"settings.cabin.engine_vibration.enabled", true},
    {"settings.manual.mirror_check.enabled", true},
    {"settings.manual.manual_look.enabled", true},
    {"settings.road.road_irregularity.enabled", true},
    {"settings.road.speed_shake.enabled", true},
    {"settings.cabin.engine_start_stop.enabled", true},
    {"settings.driving.body_dynamics.enabled", true},
    {"settings.manual.manual_zoom.enabled", true},
    {"settings.manual.mirror_check.require_stationary",
     defaults::kMirrorCheckRequireStationary},
    {"settings.manual.mirror_check.ignore_after_moving_signal",
     defaults::kMirrorCheckIgnoreAfterMovingSignal},
    {"settings.manual.manual_look.toggle_mode",
     defaults::kManualLookToggleMode},
};

constexpr FloatKey kFloatKeys[] = {
    {"settings.driving.head_motion.sway_strength",
     defaults::kHeadMotionSwayStrength},
    {"settings.driving.head_motion.tilt_strength",
     defaults::kHeadMotionTiltStrength},
    {"settings.driving.head_motion.smoothing_time",
     defaults::kHeadMotionSmoothing},
    {"settings.driving.steering_camera.rotation_factor_deg",
     defaults::kSteeringCameraRotationAmount},
    {"settings.driving.steering_camera.smoothing_time",
     defaults::kSteeringCameraSmoothing},
    {"settings.driving.steering_camera.delay_seconds",
     defaults::kSteeringCameraReactionDelay},
    {"settings.cabin.idle_breathing.vertical_amplitude",
     defaults::kIdleBreathingVerticalAmount},
    {"settings.cabin.idle_breathing.pitch_amplitude_deg",
     defaults::kIdleBreathingHeadNodAmount},
    {"settings.cabin.idle_breathing.breathing_rate_bpm",
     defaults::kIdleBreathingRate},
    {"settings.cabin.idle_breathing.fade_start_kmh",
     defaults::kIdleBreathingFadeStart},
    {"settings.cabin.idle_breathing.fade_end_kmh",
     defaults::kIdleBreathingFadeEnd},
    {"settings.road.suspension.vertical_strength",
     defaults::kSuspensionVerticalStrength},
    {"settings.road.suspension.reactivity", defaults::kSuspensionReactivity},
    {"settings.road.suspension.grade_strength",
     defaults::kSuspensionGradeStrength},
    {"settings.cabin.engine_vibration.intensity",
     defaults::kEngineVibrationIntensity},
    {"settings.manual.mirror_check.look_angle_deg",
     defaults::kMirrorCheckLookAngle},
    {"settings.manual.mirror_check.pitch_offset_deg",
     defaults::kMirrorCheckPitchOffset},
    {"settings.manual.mirror_check.smoothing_time",
     defaults::kMirrorCheckSmoothing},
    {"settings.manual.manual_look.look_angle_deg",
     defaults::kManualLookLookAngle},
    {"settings.manual.manual_look.smoothing_time",
     defaults::kManualLookSmoothing},
    {"settings.road.road_irregularity.intensity",
     defaults::kRoadIrregularityIntensity},
    {"settings.road.road_irregularity.reactivity",
     defaults::kRoadIrregularityReactivity},
    {"settings.road.speed_shake.intensity", defaults::kSpeedShakeIntensity},
    {"settings.road.speed_shake.smoothing_time",
     defaults::kSpeedShakeSmoothing},
    {"settings.road.speed_shake.rotation", defaults::kSpeedShakeRotation},
    {"settings.road.speed_shake.vertical", defaults::kSpeedShakeVertical},
    {"settings.road.speed_shake.roughness", defaults::kSpeedShakeRoughness},
    {"settings.driving.body_dynamics.lean_strength",
     defaults::kBodyDynamicsLeanStrength},
    {"settings.driving.body_dynamics.nod_strength",
     defaults::kBodyDynamicsNodStrength},
    {"settings.driving.body_dynamics.smoothing_time",
     defaults::kBodyDynamicsSmoothing},
    {"settings.cabin.engine_start_stop.intensity",
     defaults::kEngineStartStopIntensity},
    {"settings.cabin.engine_start_stop.duration",
     defaults::kEngineStartStopDuration},
    {"settings.manual.manual_zoom.zoom_fov_deg",
     defaults::kManualZoomZoomLevel},
    {"settings.manual.manual_zoom.smoothing_time",
     defaults::kManualZoomSmoothing},
};

std::string ToLower(const std::string &s) {
  std::string out = s;
  for (char &c : out)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

// Windows device names can't be used as file names (with or without an
// extension), so a profile called e.g. "con" could never be saved.
bool IsReservedWindowsName(const std::string &name) {
  const std::string n = ToLower(name);
  if (n == "con" || n == "prn" || n == "aux" || n == "nul")
    return true;
  return n.size() == 4 && (n.rfind("com", 0) == 0 || n.rfind("lpt", 0) == 0) &&
         n[3] >= '1' && n[3] <= '9';
}

std::string ProfilesDir(PluginContext &ctx) {
  if (!ctx.core || !ctx.core->environment || !ctx.environment_handle)
    return {};
  char buffer[512];
  const int len = ctx.core->environment->Env_GetPluginDataDir(
      ctx.environment_handle, buffer, sizeof(buffer));
  if (len <= 0)
    return {};
  return std::string(buffer, static_cast<size_t>(len)) + "/profiles";
}

std::string ProfilePath(PluginContext &ctx, const std::string &name) {
  const std::string dir = ProfilesDir(ctx);
  if (dir.empty())
    return {};
  return dir + "/" + name + ".json";
}

bool ProfileFileExists(PluginContext &ctx, const std::string &name) {
  const std::string path = ProfilePath(ctx, name);
  std::error_code ec;
  return !path.empty() && fs::exists(path, ec);
}

void CopyAllKeys(SPF_Config_API *cfg, SPF_Config_Handle *from,
                 SPF_Config_Handle *to) {
  for (const BoolKey &k : kBoolKeys)
    cfg->Cfg_SetBool(to, k.key, cfg->Cfg_GetBool(from, k.key, k.default_value));
  for (const FloatKey &k : kFloatKeys)
    cfg->Cfg_SetFloat(to, k.key,
                      cfg->Cfg_GetFloat(from, k.key, k.default_value));
}

SPF_Config_Handle *OpenProfileContext(PluginContext &ctx,
                                      const std::string &name) {
  if (!ctx.core || !ctx.core->config || !ctx.core->environment ||
      !ctx.environment_handle)
    return nullptr;
  const std::string dir = ProfilesDir(ctx);
  if (dir.empty() || !ctx.core->environment->Env_CreatePath(
                         ctx.environment_handle, dir.c_str()))
    return nullptr;
  const std::string path = ProfilePath(ctx, name);
  if (path.empty())
    return nullptr;
  return ctx.core->config->Cfg_CreateCustomContext(path.c_str());
}

} // namespace

std::vector<const char *> AllSettingKeys() {
  std::vector<const char *> keys;
  keys.reserve(std::size(kBoolKeys) + std::size(kFloatKeys));
  for (const BoolKey &k : kBoolKeys)
    keys.push_back(k.key);
  for (const FloatKey &k : kFloatKeys)
    keys.push_back(k.key);
  return keys;
}

std::string Sanitize(const std::string &name) {
  std::string out;
  for (char c : name) {
    if (out.size() >= kMaxNameLength)
      break;
    if (std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '-' ||
        c == '_')
      out.push_back(c);
  }
  const size_t start = out.find_first_not_of(' ');
  if (start == std::string::npos)
    return {};
  const size_t end = out.find_last_not_of(' ');
  out = out.substr(start, end - start + 1);
  if (IsReservedWindowsName(out))
    return {};
  return out;
}

const std::string *FindIgnoreCase(const std::vector<std::string> &names,
                                  const std::string &name) {
  const std::string lower = ToLower(name);
  for (const std::string &n : names) {
    if (ToLower(n) == lower)
      return &n;
  }
  return nullptr;
}

std::vector<std::string> List(PluginContext &ctx) {
  if (!ctx.profile_list_dirty)
    return ctx.cached_profile_list;

  std::vector<std::string> names;
  const std::string dir = ProfilesDir(ctx);
  if (!dir.empty()) {
    std::error_code ec;
    if (fs::exists(dir, ec) && fs::is_directory(dir, ec)) {
      for (const auto &entry : fs::directory_iterator(dir, ec)) {
        if (ec)
          break;
        if (!entry.is_regular_file())
          continue;
        if (entry.path().extension() == ".json")
          names.push_back(entry.path().stem().string());
      }
      std::sort(names.begin(), names.end());
    }
  }
  ctx.cached_profile_list = std::move(names);
  ctx.profile_list_dirty = false;
  return ctx.cached_profile_list;
}

bool Save(PluginContext &ctx, const std::string &name) {
  if (!ctx.config_handle || name.empty())
    return false;

  SPF_Config_Handle *profile_h = OpenProfileContext(ctx, name);
  if (!profile_h)
    return false;

  CopyAllKeys(ctx.core->config, ctx.config_handle, profile_h);
  ctx.core->config->Cfg_Save(profile_h);
  ctx.core->config->Cfg_SetString(ctx.config_handle, kLastProfileKey,
                                  name.c_str());
  // Unlike Cfg_SetBool/Cfg_SetFloat, a bare Cfg_SetString on the live
  // context isn't reliably flushed by the time the game actually exits.
  // Without this, a restart reads back a stale kLastProfileKey from disk.
  ctx.core->config->Cfg_Save(ctx.config_handle);
  ctx.LogFmt(SPF_LOG_INFO, "Saved profile '%s'", name.c_str());
  // A new profile file may have just been created, and live config now
  // exactly matches this one, so force both caches to recompute next read.
  ctx.profile_list_dirty = true;
  ctx.profile_matches_dirty = true;
  return true;
}

bool Load(PluginContext &ctx, const std::string &name) {
  if (!ctx.config_handle || name.empty())
    return false;

  const std::string path = ProfilePath(ctx, name);
  std::error_code ec;
  if (path.empty() || !fs::exists(path, ec))
    return false;

  SPF_Config_Handle *profile_h = OpenProfileContext(ctx, name);
  if (!profile_h)
    return false;

  CopyAllKeys(ctx.core->config, profile_h, ctx.config_handle);
  ctx.effects.LoadAllConfig();
  if (ctx.manual_zoom)
    ctx.manual_zoom->LoadConfig();
  // Switching profiles mid-drive can carry stale spring/timer state into
  // the new profile's differently-scaled targets (e.g. a different
  // smoothing_time), causing a visible snap/overshoot. Reset the same way
  // Plugin.cpp does on fresh cabin-view entry.
  ctx.effects.ResetAll();
  if (ctx.manual_zoom)
    ctx.manual_zoom->Reset();
  ctx.core->config->Cfg_SetString(ctx.config_handle, kLastProfileKey,
                                  name.c_str());
  ctx.core->config->Cfg_Save(ctx.config_handle);
  ctx.LogFmt(SPF_LOG_INFO, "Loaded profile '%s'", name.c_str());
  // Live config just changed wholesale, so the match answer is stale.
  ctx.profile_matches_dirty = true;
  return true;
}

bool Delete(PluginContext &ctx, const std::string &name) {
  const std::string path = ProfilePath(ctx, name);
  if (path.empty())
    return false;
  std::error_code ec;
  const bool removed = fs::remove(path, ec);
  if (removed)
    ctx.profile_list_dirty = true;
  return removed;
}

void EnsureDefaultExists(PluginContext &ctx) {
  const std::string path = ProfilePath(ctx, kDefaultProfileName);
  std::error_code ec;
  if (path.empty() || fs::exists(path, ec))
    return;

  SPF_Config_Handle *profile_h = OpenProfileContext(ctx, kDefaultProfileName);
  if (!profile_h)
    return;

  SPF_Config_API *cfg = ctx.core->config;
  for (const BoolKey &k : kBoolKeys)
    cfg->Cfg_SetBool(profile_h, k.key, k.default_value);
  for (const FloatKey &k : kFloatKeys)
    cfg->Cfg_SetFloat(profile_h, k.key, k.default_value);
  cfg->Cfg_Save(profile_h);
  ctx.profile_list_dirty = true;
  ctx.profile_matches_dirty = true;
  // Only claim it as active if live settings really equal the defaults just
  // written. If they differ (e.g. settings.json was customized but the
  // profiles folder is gone), marking Default active would make
  // RevertUnsavedChanges() overwrite the user's values with defaults on
  // unload. Callers that want Default applied Load() it explicitly.
  if (Matches(ctx, kDefaultProfileName)) {
    cfg->Cfg_SetString(ctx.config_handle, kLastProfileKey, kDefaultProfileName);
    cfg->Cfg_Save(ctx.config_handle);
  }
  ctx.LogFmt(SPF_LOG_INFO, "Created '%s' profile", kDefaultProfileName);
}

std::string LastUsedName(PluginContext &ctx) {
  if (!ctx.core || !ctx.core->config || !ctx.config_handle)
    return {};
  char buffer[64];
  const int len = ctx.core->config->Cfg_GetString(
      ctx.config_handle, kLastProfileKey, "", buffer, sizeof(buffer));
  if (len <= 0)
    return {};
  return std::string(buffer, static_cast<size_t>(len));
}

bool MatchesUncached(PluginContext &ctx, const std::string &name) {
  if (!ProfileFileExists(ctx, name))
    return false;
  if (!ctx.core || !ctx.core->config || !ctx.config_handle)
    return false;

  SPF_Config_Handle *profile_h = OpenProfileContext(ctx, name);
  if (!profile_h)
    return false;

  SPF_Config_API *cfg = ctx.core->config;
  for (const BoolKey &k : kBoolKeys) {
    if (cfg->Cfg_GetBool(ctx.config_handle, k.key, k.default_value) !=
        cfg->Cfg_GetBool(profile_h, k.key, k.default_value))
      return false;
  }
  for (const FloatKey &k : kFloatKeys) {
    const double live =
        cfg->Cfg_GetFloat(ctx.config_handle, k.key, k.default_value);
    const double saved = cfg->Cfg_GetFloat(profile_h, k.key, k.default_value);
    if (std::fabs(live - saved) > 1e-4)
      return false;
  }
  return true;
}

bool Matches(PluginContext &ctx, const std::string &name) {
  if (!ctx.profile_matches_dirty && ctx.matches_cache_profile_name == name)
    return ctx.cached_profile_matches;

  ctx.cached_profile_matches = MatchesUncached(ctx, name);
  ctx.matches_cache_profile_name = name;
  ctx.profile_matches_dirty = false;
  return ctx.cached_profile_matches;
}

void ResolveUnknownActiveProfile(PluginContext &ctx) {
  if (!ctx.core || !ctx.core->config || !ctx.config_handle)
    return;

  const std::string last = LastUsedName(ctx);
  if (!last.empty()) {
    if (ProfileFileExists(ctx, last))
      return;
    // Active profile's file is gone (e.g. deleted on disk), so fall back
    // to "Default" instead of leaving the UI on a dead name.
    ctx.LogFmt(SPF_LOG_INFO,
               "Active profile '%s' no longer exists on disk, loading '%s'",
               last.c_str(), kDefaultProfileName);
    EnsureDefaultExists(ctx);
    Load(ctx, kDefaultProfileName);
    return;
  }

  for (const std::string &name : List(ctx)) {
    if (!Matches(ctx, name))
      continue;
    ctx.core->config->Cfg_SetString(ctx.config_handle, kLastProfileKey,
                                    name.c_str());
    ctx.core->config->Cfg_Save(ctx.config_handle);
    ctx.LogFmt(SPF_LOG_INFO, "Resolved active profile to '%s' on startup",
               name.c_str());
    return;
  }
}

void RevertUnsavedChanges(PluginContext &ctx) {
  const std::string name = LastUsedName(ctx);
  // The exists check must come before Matches(): Matches() also returns
  // false for a missing file, and without this we'd fall through to
  // OpenProfileContext() below, which would create a blank profile file
  // and reset live settings to hardcoded defaults on unload.
  if (name.empty() || !ProfileFileExists(ctx, name) || Matches(ctx, name))
    return;

  SPF_Config_Handle *profile_h = OpenProfileContext(ctx, name);
  if (!profile_h)
    return;

  CopyAllKeys(ctx.core->config, profile_h, ctx.config_handle);
  ctx.LogFmt(SPF_LOG_INFO,
             "Discarded unsaved changes, restored profile '%s' on unload",
             name.c_str());
}

} // namespace motioncab::profiles
