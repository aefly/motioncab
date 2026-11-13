#pragma once

#include <string>
#include <vector>

namespace motioncab {
struct PluginContext;
} // namespace motioncab

// Every setting is kept per truck model: settings.json is the live copy the
// effects read, swapped for the truck's own on a truck switch, and every
// change to it goes back to the truck being driven. A truck never tuned
// starts from the base, the player's settings when this came in.
namespace motioncab::truck_settings {

// Carries the player's values over to keys renamed or split since a release.
// Call on activation, before anything reads the settings.
void MigrateLegacySettings(PluginContext &ctx);

// Takes the live settings as the base, once: the first launch after an
// upgrade keeps the player's tuning for every truck. Call before SwitchTo.
void EnsureBase(PluginContext &ctx);

// Puts the truck's settings live and reloads the effects. An empty key (no
// truck) leaves the live settings as they are. Nothing happens if the truck
// is already the current one.
void SwitchTo(PluginContext &ctx, const std::string &truck_key,
              const std::string &truck_name);

// Records a changed setting for the current truck, from OnSettingChanged.
// It reaches the disk through SaveIfIdle or Flush.
void OnLiveSettingChanged(PluginContext &ctx, const char *key);

// Saves once the changes have settled for a moment. Call every frame.
void SaveIfIdle(PluginContext &ctx);
// Saves any pending change now.
void Flush(PluginContext &ctx);
// Saves, then forgets the file and the truck, on unload: SPF releases our
// handles, and the DLL's globals may outlive a reload.
void Shutdown(PluginContext &ctx);

// True while a truck's settings are copied into settings.json: each key
// echoes through OnSettingChanged, and SwitchTo reloads the effects once at
// the end.
bool IsApplying();

// Puts a truck back on MotionCab's defaults, whatever the base, and the
// current one's live. False when there's nothing to reset (no truck key).
bool ResetToDefaults(PluginContext &ctx, const std::string &truck_key);

struct TunedTruck {
  std::string key;
  std::string name;
  // Settings differing from the base.
  int changed = 0;
};
// The trucks with settings of their own, the one being driven first, then
// by name. Rebuilt only after a change.
const std::vector<TunedTruck> &TunedTrucks(PluginContext &ctx);

const std::string &CurrentTruckKey();

bool HasTruck();
const std::string &CurrentTruckName();

} // namespace motioncab::truck_settings
