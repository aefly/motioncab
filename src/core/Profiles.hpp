#pragma once

#include <string>
#include <vector>

namespace motioncab {
struct PluginContext;
} // namespace motioncab

// Named snapshots of all the settings. SPF only gives a plugin one
// settings.json, so each profile is its own file, opened through
// Cfg_CreateCustomContext.
namespace motioncab::profiles {

inline constexpr const char *kDefaultProfileName = "Default";

// The last profile saved or loaded: the active one, which the dropdown shows
// after a restart and RevertUnsavedChanges() goes back to.
inline constexpr const char *kLastProfileKey = "ui.last_profile";

// Turns a typed name into a safe file name (letters, digits, spaces, '-' and
// '_', trimmed, 40 chars at most). Empty means the name is invalid.
std::string Sanitize(const std::string &name);

// Ignores case like Windows file names do: "default" is the "Default"
// profile. Returns the entry as stored.
const std::string *FindIgnoreCase(const std::vector<std::string> &names,
                                  const std::string &name);

// Carries the player's values over to keys renamed or split since a release
// (profiles get the same when opened). Call on activation, before anything
// reads the settings.
void MigrateLegacySettings(PluginContext &ctx);

// Sorted, without the .json extension.
std::vector<std::string> List(PluginContext &ctx);

bool Save(PluginContext &ctx, const std::string &name);
bool Load(PluginContext &ctx, const std::string &name);
bool Delete(PluginContext &ctx, const std::string &name);

// Holds the built-in defaults. Does nothing once the file exists.
void EnsureDefaultExists(PluginContext &ctx);

// Falls back to "Default" if the active profile's file is gone (deleted
// while the game was closed). With no active profile, adopts a saved one the
// live settings match, rather than showing "(Unsaved changes)" forever.
void ResolveUnknownActiveProfile(PluginContext &ctx);

// Empty if no profile was ever saved or loaded.
std::string LastUsedName(PluginContext &ctx);

// Whether the live settings are exactly what profile `name` holds, for the
// UI's "modified" marker. False if it doesn't exist.
bool Matches(PluginContext &ctx, const std::string &name);

// Call whenever a setting changes outside this module, since Matches()
// caches its answer.
void InvalidateMatchCache();

// Puts the active profile's values back over unsaved edits. Called on
// unload, so quitting without saving discards a tweak instead of baking it
// into settings.json with "unsaved changes" flagged forever after.
void RevertUnsavedChanges(PluginContext &ctx);

} // namespace motioncab::profiles
