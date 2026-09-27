#include "ProfileManager.hpp"

#include "PluginContext.hpp"
#include "SPF_Environment_API.h"
#include "SettingsSchema.hpp"
#include "StringUtil.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>

namespace motioncab::profiles {

namespace {

namespace fs = std::filesystem;

// Quick Settings cache: avoids re-scanning the profiles dir / re-diffing
// every key each rendered frame the window is open, since these only
// actually change on a Save/Load/Delete/edit.
struct Cache {
  std::vector<std::string> profile_list;
  bool profile_list_dirty = true;
  bool matches = true;
  std::string matches_profile_name;
  bool matches_dirty = true;
};
Cache g_cache;

constexpr size_t kMaxNameLength = 40;

// Float settings renamed/split since a released version: the old key's value
// is copied into every new key, then the old key is removed (settings.json
// only, see MigrateLegacyKeys). v1.0.x had one steering camera rotation
// amount for both turn directions.
//
// A setting deleted outright, with no replacement, doesn't belong here: its
// key just stays in settings.json and old profiles, unused, since nothing
// reads it anymore.
struct LegacyFloatKey {
  const char *old_key;
  const char *new_keys[2];
};

constexpr LegacyFloatKey kLegacyFloatKeys[] = {
    {"settings.driving.steering_camera.rotation_factor_deg",
     {"settings.driving.steering_camera.rotation_left_deg",
      "settings.driving.steering_camera.rotation_right_deg"}},
};

// Returns true if anything was migrated (the caller decides whether to save).
//
// Cfg_RemoveKey works on settings.json but not on a Cfg_CreateCustomContext
// file (confirmed in-game: a migrated profile still had the old keys after
// Cfg_Save). So for profiles the old key sticks around, and migrating on its
// presence alone would overwrite the new per-side values with the old single
// one every time the profile is opened. Profiles are therefore only migrated
// while the new keys are missing (`skip_if_migrated`). settings.json can't
// use that test, since SPF fills the new keys in from the manifest defaults
// before the plugin runs, but there the removal sticks.
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

// Windows device names can't be used as file names (with or without an
// extension), so a profile called e.g. "con" could never be saved.
bool IsReservedWindowsName(const std::string &name) {
  const std::string n = util::ToLower(name);
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

// Config keys are flat dotted strings, not a queryable nested JSON node, so
// profiles copy every setting of settings::kAll individually instead of one
// bulk JSON blob.
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
    if (util::EqualsIgnoreCase(n, name))
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
  // Unlike Cfg_SetBool/Cfg_SetFloat, a bare Cfg_SetString on the live
  // context isn't reliably flushed by the time the game actually exits.
  // Without this, a restart reads back a stale kLastProfileKey from disk.
  ctx.core->config->Cfg_Save(ctx.config_handle);
  ctx.LogFmt(SPF_LOG_INFO,
             existed ? "Saved profile '%s'" : "Created profile '%s'",
             name.c_str());
  // A new profile file may have just been created, and live config now
  // exactly matches this one, so force both caches to recompute next read.
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
  // Switching profiles mid-drive can carry stale spring/timer state into
  // the new profile's differently-scaled targets (e.g. a different
  // smoothing_time), causing a visible snap/overshoot. Reset the same way
  // Plugin.cpp does on fresh cabin-view entry.
  ctx.ResetEffects();
  ctx.core->config->Cfg_SetString(ctx.config_handle, kLastProfileKey,
                                  name.c_str());
  ctx.core->config->Cfg_Save(ctx.config_handle);
  ctx.LogFmt(SPF_LOG_INFO, "Loaded profile '%s'", name.c_str());
  // Live config just changed wholesale, so the match answer is stale.
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

void InvalidateMatchCache() { g_cache.matches_dirty = true; }

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
