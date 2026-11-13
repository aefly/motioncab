#include "TruckSettings.hpp"

#include "core/Keybinds.hpp"
#include "core/PluginContext.hpp"
#include "core/Settings.hpp"
#include "core/Strings.hpp"

#include "SPF_JsonReader_API.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <span>
#include <string>
#include <system_error>

namespace motioncab::truck_settings {

namespace {

// data/truck_settings.json holds "base" (the settings every truck starts
// from, taken once) and "trucks.<key>", only the settings changed in that
// truck: the others read the base.
struct Store {
  SPF_Config_Handle *handle = nullptr;
  // A file SPF couldn't parse isn't retried every frame.
  bool open_failed = false;
  // Saved once the player stops changing things: SPF would otherwise rewrite
  // the whole file (and log it) on every frame of a slider drag.
  bool dirty = false;
  std::chrono::steady_clock::time_point last_change{};
  std::string truck_key;
  std::string truck_name;
  // Set while we copy into settings.json ourselves, so OnSettingChanged
  // doesn't take our own writes for the player's.
  bool applying = false;
  // Bumped on every write, so the tuned trucks list is only rebuilt then.
  unsigned revision = 0;
};
Store g_store;

constexpr const char *kBasePrefix = "base.";
// Written last by EnsureBase: a base without it is missing or half written.
constexpr const char *kBaseCompleteKey = "base.complete";
constexpr auto kSaveDelay = std::chrono::seconds(1);
// Telemetry's display name, kept with the entry for the list of tuned trucks:
// the key alone ("scania_vehicle_scania_s_2016") isn't something to show.
constexpr const char *kNameKey = "name";

// Float settings renamed or split since a release: the old value goes into
// every new key (a rename leaves the second one null). Only settings.json is
// migrated: a rename made after per-truck settings came in must also move
// the keys stored in truck_settings.json. A setting deleted
// with no replacement doesn't belong here, its key just sits unused in old
// files.
struct LegacyFloatKey {
  const char *old_key;
  const char *new_keys[2];
};

constexpr LegacyFloatKey kLegacyFloatKeys[] = {
    {"settings.driving.steering_camera.rotation_factor_deg",
     {"settings.driving.steering_camera.rotation_left_deg",
      "settings.driving.steering_camera.rotation_right_deg"}},
    // It never trembled, it sways slowly: named tremor up to 1.1.0.
    {"settings.cabin.natural_head_movement.tremor_intensity",
     {"settings.cabin.natural_head_movement.sway_intensity", nullptr}},
};

// The same for bools. Mirror Check became part of Side Look after 1.1.0:
// its angles are Side Look's mirror glances, its toggle and options moved.
struct LegacyBoolKey {
  const char *old_key;
  const char *new_key;
};

constexpr LegacyBoolKey kLegacyBoolKeys[] = {
    {"settings.manual.mirror_check.enabled",
     "settings.manual.manual_look.mirror_on_signal"},
    {"settings.manual.mirror_check.require_stationary",
     "settings.manual.manual_look.require_stationary"},
    {"settings.manual.mirror_check.ignore_after_moving_signal",
     "settings.manual.manual_look.ignore_after_moving_signal"},
};

bool MigrateLegacyKeys(SPF_Config_API *cfg, SPF_Config_Handle *h) {
  bool migrated = false;
  for (const LegacyFloatKey &k : kLegacyFloatKeys) {
    if (!cfg->Cfg_HasKey(h, k.old_key))
      continue;
    const double value = cfg->Cfg_GetFloat(h, k.old_key, 0.0);
    for (const char *new_key : k.new_keys) {
      if (new_key)
        cfg->Cfg_SetFloat(h, new_key, value);
    }
    cfg->Cfg_RemoveKey(h, k.old_key);
    migrated = true;
  }
  for (const LegacyBoolKey &k : kLegacyBoolKeys) {
    if (!cfg->Cfg_HasKey(h, k.old_key))
      continue;
    cfg->Cfg_SetBool(h, k.new_key, cfg->Cfg_GetBool(h, k.old_key, true));
    cfg->Cfg_RemoveKey(h, k.old_key);
    migrated = true;
  }
  return migrated;
}

// Up to 1.x the looks' hold or toggle was a setting of ours. It's now each
// binding's behavior in SPF's keybind settings, which SPF had saved as
// Toggle for every player, so the old setting is carried over once.
struct LegacyToggleMode {
  const char *key;
  bool fallback;
  std::span<const keybinds::Action> actions;
};

constexpr keybinds::Action kSideLookActions[] = {
    keybinds::kLookLeft, keybinds::kLookRight, keybinds::kGlanceLeft,
    keybinds::kGlanceRight};
constexpr keybinds::Action kBlindspotActions[] = {keybinds::kBlindspotPeek};

constexpr LegacyToggleMode kLegacyToggleModes[] = {
    {"settings.manual.manual_look.toggle_mode", false, kSideLookActions},
    {"settings.manual.blindspot_viewer.toggle_mode", true, kBlindspotActions},
};

// The bindings are rebuilt on each change, so the count is read again.
void SetBehavior(PluginContext &ctx, const keybinds::Action &action,
                 SPF_ActivationBehavior behavior) {
  SPF_KeyBinds_API *kbind = ctx.core->keybinds;
  for (int i = 0;
       i < kbind->Kbind_GetBindingCount(ctx.keybinds_handle, action.id); ++i)
    kbind->Kbind_SetBindingBehavior(ctx.keybinds_handle, action.id, i,
                                    behavior);
}

bool MigrateToggleModes(PluginContext &ctx) {
  SPF_Config_API *cfg = ctx.core->config;
  SPF_KeyBinds_API *kbind = ctx.core->keybinds;
  if (!kbind || !ctx.keybinds_handle)
    return false;
  bool migrated = false;
  for (const LegacyToggleMode &mode : kLegacyToggleModes) {
    if (!cfg->Cfg_HasKey(ctx.config_handle, mode.key))
      continue;
    const SPF_ActivationBehavior behavior =
        cfg->Cfg_GetBool(ctx.config_handle, mode.key, mode.fallback)
            ? SPF_BEHAVIOR_TOGGLE
            : SPF_BEHAVIOR_HOLD;
    for (const keybinds::Action &action : mode.actions)
      SetBehavior(ctx, action, behavior);
    cfg->Cfg_RemoveKey(ctx.config_handle, mode.key);
    migrated = true;
  }
  return migrated;
}

// Up to 1.x Manual Zoom read its key raw: it zoomed while held whatever the
// binding said, and SPF had saved Toggle for it. The bindings are set to Hold
// once, so it keeps working the same; the marker keeps a Toggle the player
// picks afterwards.
constexpr const char *kZoomHoldMigratedKey = "ui.migrated_zoom_hold";

bool MigrateZoomHold(PluginContext &ctx) {
  SPF_Config_API *cfg = ctx.core->config;
  if (!ctx.core->keybinds || !ctx.keybinds_handle ||
      cfg->Cfg_GetBool(ctx.config_handle, kZoomHoldMigratedKey, false))
    return false;
  SetBehavior(ctx, keybinds::kZoom, SPF_BEHAVIOR_HOLD);
  cfg->Cfg_SetBool(ctx.config_handle, kZoomHoldMigratedKey, true);
  return true;
}

// Opening a custom context creates a missing file, so not before there's
// something to write. SPF has no call to close one: it's opened once.
SPF_Config_Handle *Handle(PluginContext &ctx, bool create) {
  if (g_store.handle || g_store.open_failed || !ctx.core || !ctx.core->config ||
      !ctx.core->environment || !ctx.environment_handle)
    return g_store.handle;
  const std::string dir = ctx.PluginDataDir();
  if (dir.empty())
    return nullptr;
  const std::string path = dir + "/truck_settings.json";
  std::error_code ec;
  if (!create && !std::filesystem::exists(path, ec))
    return nullptr;
  if (!ctx.core->environment->Env_CreatePath(ctx.environment_handle,
                                             dir.c_str()))
    return nullptr;
  g_store.handle = ctx.core->config->Cfg_CreateCustomContext(path.c_str());
  if (g_store.handle) {
    ctx.core->config->Cfg_SetAutoSave(g_store.handle, false);
  } else {
    g_store.open_failed = true;
    ctx.LogFmt(SPF_LOG_WARN, "Couldn't open %s, settings won't be per truck",
               path.c_str());
  }
  return g_store.handle;
}

std::string TruckPrefix(const std::string &truck_key) {
  return "trucks." + truck_key + ".";
}

void WriteName(SPF_Config_API *cfg, SPF_Config_Handle *store,
               const std::string &truck_key, const std::string &name) {
  if (!name.empty())
    cfg->Cfg_SetString(store, (TruckPrefix(truck_key) + kNameKey).c_str(),
                       name.c_str());
}

std::string StoredName(SPF_Config_API *cfg, SPF_Config_Handle *store,
                       const std::string &truck_key) {
  char buf[256] = "";
  cfg->Cfg_GetString(store, (TruckPrefix(truck_key) + kNameKey).c_str(), "",
                     buf, sizeof(buf));
  return buf;
}

// How many of the truck's settings differ from the base: what the player
// changed in it (a reset's defaults included).
int CountChanged(SPF_Config_API *cfg, SPF_Config_Handle *store,
                 const std::string &truck_key) {
  const std::string prefix = TruckPrefix(truck_key);
  int changed = 0;
  for (const settings::Setting &s : settings::kAll) {
    const std::string key = prefix + s.key;
    if (!cfg->Cfg_HasKey(store, key.c_str()))
      continue;
    const std::string base_key = kBasePrefix + std::string(s.key);
    if (s.type == settings::Type::kBool) {
      changed += cfg->Cfg_GetBool(store, key.c_str(), s.default_bool()) !=
                 cfg->Cfg_GetBool(store, base_key.c_str(), s.default_bool());
    } else {
      changed += static_cast<float>(
                     cfg->Cfg_GetFloat(store, key.c_str(), s.default_value)) !=
                 static_cast<float>(cfg->Cfg_GetFloat(store, base_key.c_str(),
                                                      s.default_value));
    }
  }
  return changed;
}

void CopyValue(SPF_Config_API *cfg, SPF_Config_Handle *from,
               const std::string &from_key, SPF_Config_Handle *to,
               const std::string &to_key, const settings::Setting &s) {
  if (s.type == settings::Type::kBool)
    cfg->Cfg_SetBool(
        to, to_key.c_str(),
        cfg->Cfg_GetBool(from, from_key.c_str(), s.default_bool()));
  else
    // Through a float, which is what the effects read: it also cleans up a
    // value saved with float noise (0.3499999940395355) by an older version.
    cfg->Cfg_SetFloat(to, to_key.c_str(),
                      settings::Stored(static_cast<float>(cfg->Cfg_GetFloat(
                          from, from_key.c_str(), s.default_value))));
}

// Puts the current truck's settings live: the ones changed in it, the base's
// for the rest. Without a base (the store couldn't be opened) the live
// settings are the best there is: they're left alone rather than reset.
void ApplyCurrent(PluginContext &ctx) {
  SPF_Config_API *cfg = ctx.core->config;
  SPF_Config_Handle *store = Handle(ctx, false);
  if (store && cfg->Cfg_HasKey(store, kBaseCompleteKey)) {
    const std::string truck_prefix = TruckPrefix(g_store.truck_key);
    g_store.applying = true;
    for (const settings::Setting &s : settings::kAll) {
      std::string from = truck_prefix + s.key;
      if (!cfg->Cfg_HasKey(store, from.c_str()))
        from = kBasePrefix + std::string(s.key);
      CopyValue(cfg, store, from, ctx.config_handle, s.key, s);
    }
    g_store.applying = false;
  }
  ctx.ReloadEffectsConfig();
}

} // namespace

void MigrateLegacySettings(PluginContext &ctx) {
  if (!ctx.core || !ctx.core->config || !ctx.config_handle)
    return;
  const bool keys = MigrateLegacyKeys(ctx.core->config, ctx.config_handle);
  const bool toggle_modes = MigrateToggleModes(ctx);
  const bool zoom_hold = MigrateZoomHold(ctx);
  if (!keys && !toggle_modes && !zoom_hold)
    return;
  ctx.core->config->Cfg_Save(ctx.config_handle);
  ctx.Log(SPF_LOG_INFO, "Migrated settings from an older version");
}

void EnsureBase(PluginContext &ctx) {
  if (!ctx.core || !ctx.core->config || !ctx.config_handle)
    return;
  SPF_Config_API *cfg = ctx.core->config;
  SPF_Config_Handle *store = Handle(ctx, true);
  if (!store || cfg->Cfg_HasKey(store, kBaseCompleteKey))
    return;
  for (const settings::Setting &s : settings::kAll)
    CopyValue(cfg, ctx.config_handle, s.key, store,
              kBasePrefix + std::string(s.key), s);
  cfg->Cfg_SetBool(store, kBaseCompleteKey, true);
  cfg->Cfg_Save(store);
  ctx.Log(SPF_LOG_INFO, "Saved the current settings as every truck's start");
}

void SwitchTo(PluginContext &ctx, const std::string &truck_key,
              const std::string &truck_name) {
  if (truck_key == g_store.truck_key ||
      (!truck_key.empty() &&
       (!ctx.core || !ctx.core->config || !ctx.config_handle)))
    return;
  g_store.truck_key = truck_key;
  g_store.truck_name = truck_name;
  if (truck_key.empty())
    return;
  ApplyCurrent(ctx);
  // Entries written before names were kept get theirs on the next drive.
  SPF_Config_API *cfg = ctx.core->config;
  SPF_Config_Handle *store = Handle(ctx, false);
  if (store && cfg->Cfg_HasKey(store, ("trucks." + truck_key).c_str()) &&
      StoredName(cfg, store, truck_key).empty()) {
    WriteName(cfg, store, truck_key, truck_name);
    g_store.dirty = true;
    g_store.last_change = std::chrono::steady_clock::now();
    ++g_store.revision;
  }
}

void OnLiveSettingChanged(PluginContext &ctx, const char *key) {
  if (g_store.applying || g_store.truck_key.empty() || !ctx.core ||
      !ctx.core->config || !ctx.config_handle)
    return;
  const settings::Setting *s = settings::Find(key);
  if (!s)
    return;
  SPF_Config_API *cfg = ctx.core->config;
  SPF_Config_Handle *store = Handle(ctx, true);
  if (!store)
    return;
  CopyValue(cfg, ctx.config_handle, s->key, store,
            TruckPrefix(g_store.truck_key) + s->key, *s);
  WriteName(cfg, store, g_store.truck_key, g_store.truck_name);
  ++g_store.revision;
  g_store.dirty = true;
  g_store.last_change = std::chrono::steady_clock::now();
}

void SaveIfIdle(PluginContext &ctx) {
  if (g_store.dirty &&
      std::chrono::steady_clock::now() - g_store.last_change >= kSaveDelay)
    Flush(ctx);
}

void Flush(PluginContext &ctx) {
  if (!g_store.dirty || !g_store.handle || !ctx.core || !ctx.core->config)
    return;
  ctx.core->config->Cfg_Save(g_store.handle);
  g_store.dirty = false;
}

void Shutdown(PluginContext &ctx) {
  Flush(ctx);
  // The revision carries on, so a cached list from before isn't taken for
  // the new store's.
  const unsigned revision = g_store.revision + 1;
  g_store = Store{};
  g_store.revision = revision;
}

bool ResetToDefaults(PluginContext &ctx, const std::string &truck_key) {
  if (truck_key.empty() || !ctx.core || !ctx.core->config || !ctx.config_handle)
    return false;
  const bool current = truck_key == g_store.truck_key;
  SPF_Config_API *cfg = ctx.core->config;
  SPF_Config_Handle *store = Handle(ctx, true);
  if (!store) {
    if (!current)
      return false;
    // Nowhere to keep it per truck: the live settings get the defaults all
    // the same.
    g_store.applying = true;
    settings::WriteDefaults(cfg, ctx.config_handle);
    g_store.applying = false;
    ctx.ReloadEffectsConfig();
  } else {
    std::string name = StoredName(cfg, store, truck_key);
    if (current)
      name = g_store.truck_name;
    const std::string truck = "trucks." + truck_key;
    // Emptied rather than removed: Cfg_RemoveKey doesn't work on a custom
    // context file.
    cfg->Cfg_SetJsonString(store, truck.c_str(), "{}");
    // A setting the base already has at its default reads it from there.
    const std::string prefix = truck + ".";
    for (const settings::Setting &s : settings::kAll) {
      const std::string base_key = kBasePrefix + std::string(s.key);
      const std::string key = prefix + s.key;
      if (s.type == settings::Type::kBool) {
        if (cfg->Cfg_GetBool(store, base_key.c_str(), s.default_bool()) !=
            s.default_bool())
          cfg->Cfg_SetBool(store, key.c_str(), s.default_bool());
      } else if (static_cast<float>(cfg->Cfg_GetFloat(store, base_key.c_str(),
                                                      s.default_value)) !=
                 s.default_value) {
        cfg->Cfg_SetFloat(store, key.c_str(),
                          settings::Stored(s.default_value));
      }
    }
    WriteName(cfg, store, truck_key, name);
    cfg->Cfg_Save(store);
    g_store.dirty = false;
    ++g_store.revision;
    if (!current)
      return true;
    ApplyCurrent(ctx);
  }
  // Springs and timers carried over into the new targets would snap or
  // overshoot.
  ctx.ResetEffects();
  return true;
}

const std::vector<TunedTruck> &TunedTrucks(PluginContext &ctx) {
  static std::vector<TunedTruck> s_trucks;
  static unsigned s_revision = ~0u;
  static std::string s_current;
  if (s_revision == g_store.revision && s_current == g_store.truck_key)
    return s_trucks;
  s_trucks.clear();
  SPF_Config_Handle *store = Handle(ctx, false);
  if (!store || !ctx.core->json_reader)
    return s_trucks;
  s_revision = g_store.revision;
  s_current = g_store.truck_key;

  SPF_Config_API *cfg = ctx.core->config;
  const SPF_JsonReader_API *json = ctx.core->json_reader;
  const SPF_JsonValue_Handle *trucks =
      cfg->Cfg_GetJsonValueHandle(store, "trucks");
  if (!trucks || json->Json_GetType(trucks) != SPF_JSON_TYPE_OBJECT)
    return s_trucks;
  const int count = json->Json_GetObjectSize(trucks);
  for (int i = 0; i < count; ++i) {
    char key_buf[256] = "";
    json->Json_GetMemberName(trucks, i, key_buf, sizeof(key_buf));
    TunedTruck truck{key_buf, StoredName(cfg, store, key_buf), 0};
    truck.changed = CountChanged(cfg, store, truck.key);
    if (truck.key.empty() || truck.changed == 0)
      continue;
    if (truck.key == g_store.truck_key)
      truck.name = g_store.truck_name;
    if (truck.name.empty())
      truck.name = truck.key;
    s_trucks.push_back(std::move(truck));
  }
  // The truck being driven first, then by name.
  std::ranges::sort(s_trucks, [](const TunedTruck &a, const TunedTruck &b) {
    const bool a_current = a.key == g_store.truck_key;
    const bool b_current = b.key == g_store.truck_key;
    if (a_current != b_current)
      return a_current;
    return strings::FoldCase(a.name) < strings::FoldCase(b.name);
  });
  return s_trucks;
}

bool IsApplying() { return g_store.applying; }

bool HasTruck() { return !g_store.truck_key.empty(); }

const std::string &CurrentTruckKey() { return g_store.truck_key; }

const std::string &CurrentTruckName() { return g_store.truck_name; }

} // namespace motioncab::truck_settings
