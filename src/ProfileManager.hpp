#pragma once

#include <string>
#include <vector>

namespace motioncab {

struct PluginContext;

// Save/load/delete whole-settings snapshots as named "profiles", stored as
// individual JSON files under the plugin's data directory. Not a native SPF
// feature (SPF_Config_API only has one settings.json per plugin), so each
// profile file is its own Cfg_CreateCustomContext.
namespace profiles {

inline constexpr const char *kDefaultProfileName = "Default";

// Config key remembering the name of the last profile explicitly
// Saved/Loaded. Read back to restore the custom UI's dropdown selection on
// startup, and to know which profile RevertUnsavedChanges() should compare
// against when the plugin unloads.
inline constexpr const char *kLastProfileKey = "ui.last_profile";

// Sanitizes a user-typed name into a safe filename stem: letters, digits,
// spaces, '-', '_' only, trimmed, capped at 40 chars. Returns empty if
// nothing valid remains (callers should treat that as "invalid name").
std::string Sanitize(const std::string &name);

// Finds `name` in `names` ignoring case (Windows file names are
// case-insensitive, so "default" and "Default" are the same profile).
// Returns the entry as stored, or nullptr.
const std::string *FindIgnoreCase(const std::vector<std::string> &names,
                                  const std::string &name);

// Every setting key a profile covers (all of MotionCab's settings), in the
// order of the key tables in ProfileManager.cpp.
std::vector<const char *> AllSettingKeys();

// Lists saved profile names (without the .json extension), sorted.
std::vector<std::string> List(PluginContext &ctx);

bool Save(PluginContext &ctx, const std::string &name);
bool Load(PluginContext &ctx, const std::string &name);
bool Delete(PluginContext &ctx, const std::string &name);

// Creates the "Default" profile (MotionCab's built-in default values) if no
// profile file with that name exists yet. Safe to call every time it's
// needed: a no-op once the file is there.
void EnsureDefaultExists(PluginContext &ctx);

// Falls back to "Default" if the marked-active profile's file is gone (e.g.
// deleted while closed). If none is marked active (kLastProfileKey empty)
// and live settings exactly match a saved profile, marks that one active
// instead of leaving the UI stuck on "(Unsaved changes)". No-op otherwise.
void ResolveUnknownActiveProfile(PluginContext &ctx);

// Returns the name persisted by Save()/Load(), or empty if none. Used by
// the custom UI to initialize its dropdown selection to match reality
// after a restart, rather than defaulting to index 0.
std::string LastUsedName(PluginContext &ctx);

// True if every current live setting equals what's saved in profile
// `name`'s file. False if it doesn't exist. Used to show a "modified"
// marker next to the active profile's name instead of silently letting
// live edits drift from what's actually on disk.
bool Matches(PluginContext &ctx, const std::string &name);

// If the live settings have diverged from the active profile (the one
// named by kLastProfileKey) without an explicit Save, restores that
// profile's values over the live config. Called from OnUnload() so that
// closing the game without saving discards the tweak instead of baking it
// into settings.json and flagging "unsaved changes" forever after. A no-op
// if there's no active profile, it doesn't exist, or nothing has changed.
void RevertUnsavedChanges(PluginContext &ctx);

} // namespace profiles
} // namespace motioncab
