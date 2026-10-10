#include "Profiles.hpp"

#include "SPF_Environment_API.h"
#include "core/PluginContext.hpp"
#include "core/Settings.hpp"
#include "core/Strings.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>

namespace motioncab::profiles {

namespace {

namespace fs = std::filesystem;

// The Quick Settings window asks every frame, but the answers only change on
// a save, load, delete or edit: no need to rescan the folder and diff every
// key each time.
struct Cache {
  std::vector<std::string> profile_list;
  bool profile_list_dirty = true;
  bool matches = true;
  std::string matches_profile_name;
  bool matches_dirty = true;
};
Cache g_cache;

constexpr size_t kMaxNameLength = 40;

// Float settings renamed or split since a release: the old value goes into
// every new key. A setting deleted with no replacement doesn't belong here,
// its key just sits unused in old files.
struct LegacyFloatKey {
  const char *old_key;
  const char *new_keys[2];
};

constexpr LegacyFloatKey kLegacyFloatKeys[] = {
    {"settings.driving.steering_camera.rotation_factor_deg",
     {"settings.driving.steering_camera.rotation_left_deg",
      "settings.driving.steering_camera.rotation_right_deg"}},
};

// Cfg_RemoveKey doesn't work on a Cfg_CreateCustomContext file (seen in
// game), so a profile keeps its old key and would be migrated again on every
// open, overwriting the new values. Profiles are only migrated while the new
// keys are missing (`skip_if_migrated`). That test can't work on
// settings.json, which SPF fills from the manifest defaults first, but there
// the removal does stick.
bool MigrateLegacyKeys(SPF_Config_API *cfg, SPF_Config_Handle *h,
                       bool skip_if_migrated) {
  bool migrated = false;
  for (const LegacyFloatKey &k : kLegacyFloatKeys) {
    if (!cfg->Cfg_HasKey(h, k.old_key))
      continue;
    if (skip_if_migrated && cfg->Cfg_HasKey(h, k.new_keys[0]))
      continue;
    const double value = cfg->Cfg_GetFloat(h, k.old_key, 0.0);
    for (const char *new_key : k.new_keys)
      cfg->Cfg_SetFloat(h, new_key, value);
    cfg->Cfg_RemoveKey(h, k.old_key);
    migrated = true;
  }
  return migrated;
}

// Windows won't create a file named after a device ("con", "com1", ...),
// extension or not.
bool IsReservedWindowsName(const std::string &name) {
  const std::string n = strings::ToLower(name);
  if (n == "con" || n == "prn" || n == "aux" || n == "nul")
    return true;
  return n.size() == 4 && (n.rfind("com", 0) == 0 || n.rfind("lpt", 0) == 0) &&
         n[3] >= '1' && n[3] <= '9';
}

std::string ProfilesDir(PluginContext &ctx) {
  const std::string data_dir = ctx.PluginDataDir();
  return data_dir.empty() ? std::string() : data_dir + "/profiles";
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

// The config API has no way to copy a whole JSON node, so it's key by key.
void CopyAllKeys(SPF_Config_API *cfg, SPF_Config_Handle *from,
                 SPF_Config_Handle *to) {
  for (const settings::Setting &s : settings::kAll) {
    if (s.type == settings::Type::kBool)
      cfg->Cfg_SetBool(to, s.key,
                       cfg->Cfg_GetBool(from, s.key, s.default_bool()));
    else
      cfg->Cfg_SetFloat(to, s.key,
                        cfg->Cfg_GetFloat(from, s.key, s.default_value));
  }
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
  SPF_Config_Handle *h =
      ctx.core->config->Cfg_CreateCustomContext(path.c_str());
  // Profiles saved by an older version still carry the old keys.
  if (h && MigrateLegacyKeys(ctx.core->config, h, true))
    ctx.core->config->Cfg_Save(h);
  return h;
}

} // namespace

void MigrateLegacySettings(PluginContext &ctx) {
  if (!ctx.core || !ctx.core->config || !ctx.config_handle)
    return;
  if (!MigrateLegacyKeys(ctx.core->config, ctx.config_handle, false))
    return;
  ctx.core->config->Cfg_Save(ctx.config_handle);
  ctx.Log(SPF_LOG_INFO, "Migrated settings from an older version");
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
  for (const std::string &n : names) {
    if (strings::EqualsIgnoreCase(n, name))
      return &n;
  }
  return nullptr;
}

std::vector<std::string> List(PluginContext &ctx) {
  if (!g_cache.profile_list_dirty)
    return g_cache.profile_list;

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
  g_cache.profile_list = std::move(names);
  g_cache.profile_list_dirty = false;
  return g_cache.profile_list;
}

bool Save(PluginContext &ctx, const std::string &name) {
  if (!ctx.config_handle || name.empty()) {
    ctx.LogFmt(SPF_LOG_WARN, "Failed to save profile '%s'", name.c_str());
    return false;
  }

  // Checked before OpenProfileContext(), which creates the file.
  const bool existed = ProfileFileExists(ctx, name);
  SPF_Config_Handle *profile_h = OpenProfileContext(ctx, name);
  if (!profile_h) {
    ctx.LogFmt(SPF_LOG_WARN, "Failed to save profile '%s'", name.c_str());
    return false;
  }

  CopyAllKeys(ctx.core->config, ctx.config_handle, profile_h);
  ctx.core->config->Cfg_Save(profile_h);
  ctx.core->config->Cfg_SetString(ctx.config_handle, kLastProfileKey,
                                  name.c_str());
  // Unlike the bool and float setters, a Cfg_SetString on its own isn't
  // reliably on disk by the time the game exits.
  ctx.core->config->Cfg_Save(ctx.config_handle);
  ctx.LogFmt(SPF_LOG_INFO,
             existed ? "Saved profile '%s'" : "Created profile '%s'",
             name.c_str());
  g_cache.profile_list_dirty = true;
  g_cache.matches_dirty = true;
  return true;
}

bool Load(PluginContext &ctx, const std::string &name) {
  if (!ctx.config_handle || name.empty()) {
    ctx.LogFmt(SPF_LOG_WARN, "Failed to load profile '%s'", name.c_str());
    return false;
  }

  const std::string path = ProfilePath(ctx, name);
  std::error_code ec;
  if (path.empty() || !fs::exists(path, ec)) {
    ctx.LogFmt(SPF_LOG_WARN, "Failed to load profile '%s': file not found",
               name.c_str());
    return false;
  }

  SPF_Config_Handle *profile_h = OpenProfileContext(ctx, name);
  if (!profile_h) {
    ctx.LogFmt(SPF_LOG_WARN, "Failed to load profile '%s'", name.c_str());
    return false;
  }

  CopyAllKeys(ctx.core->config, profile_h, ctx.config_handle);
  ctx.ReloadEffectsConfig();
  // Springs and timers carried over into the new profile's targets would
  // snap or overshoot.
  ctx.ResetEffects();
  ctx.core->config->Cfg_SetString(ctx.config_handle, kLastProfileKey,
                                  name.c_str());
  ctx.core->config->Cfg_Save(ctx.config_handle);
  ctx.LogFmt(SPF_LOG_INFO, "Loaded profile '%s'", name.c_str());
  g_cache.matches_dirty = true;
  return true;
}

bool Delete(PluginContext &ctx, const std::string &name) {
  const std::string path = ProfilePath(ctx, name);
  std::error_code ec;
  const bool removed = !path.empty() && fs::remove(path, ec);
  if (!removed) {
    ctx.LogFmt(SPF_LOG_WARN, "Failed to delete profile '%s'", name.c_str());
    return false;
  }
  ctx.LogFmt(SPF_LOG_INFO, "Deleted profile '%s'", name.c_str());
  g_cache.profile_list_dirty = true;
  return true;
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
  settings::WriteDefaults(cfg, profile_h);
  cfg->Cfg_Save(profile_h);
  g_cache.profile_list_dirty = true;
  g_cache.matches_dirty = true;
  // Only made active if the live settings are the defaults: if the player's
  // settings.json outlived a deleted profiles folder, RevertUnsavedChanges()
  // would otherwise wipe their values on unload.
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
  for (const settings::Setting &s : settings::kAll) {
    if (s.type == settings::Type::kBool) {
      if (cfg->Cfg_GetBool(ctx.config_handle, s.key, s.default_bool()) !=
          cfg->Cfg_GetBool(profile_h, s.key, s.default_bool()))
        return false;
    } else {
      const double live =
          cfg->Cfg_GetFloat(ctx.config_handle, s.key, s.default_value);
      const double saved = cfg->Cfg_GetFloat(profile_h, s.key, s.default_value);
      if (std::fabs(live - saved) > 1e-4)
        return false;
    }
  }
  return true;
}

bool Matches(PluginContext &ctx, const std::string &name) {
  if (!g_cache.matches_dirty && g_cache.matches_profile_name == name)
    return g_cache.matches;

  g_cache.matches = MatchesUncached(ctx, name);
  g_cache.matches_profile_name = name;
  g_cache.matches_dirty = false;
  return g_cache.matches;
}

void ResolveUnknownActiveProfile(PluginContext &ctx) {
  if (!ctx.core || !ctx.core->config || !ctx.config_handle)
    return;

  const std::string last = LastUsedName(ctx);
  if (!last.empty()) {
    if (ProfileFileExists(ctx, last))
      return;
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

void InvalidateMatchCache() { g_cache.matches_dirty = true; }

void RevertUnsavedChanges(PluginContext &ctx) {
  const std::string name = LastUsedName(ctx);
  // Matches() is false for a missing file too, and opening it below would
  // create a blank one and reset the live settings to the defaults.
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
