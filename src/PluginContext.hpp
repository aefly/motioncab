#pragma once

#include "SPF_Config_API.h"
#include "SPF_Environment_API.h"
#include "SPF_Formatting_API.h"
#include "SPF_KeyBinds_API.h"
#include "SPF_Localization_API.h"
#include "SPF_Logger_API.h"
#include "SPF_Plugin.h"
#include "SPF_TelemetryData.h"
#include "SPF_Telemetry_API.h"
#include "effects/EffectManager.hpp"
#include "effects/ManualZoomEffect.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace motioncab {

// Shared state for the plugin's lifetime. One instance, owned by Plugin.cpp.
// Populated progressively as SPF lifecycle callbacks fire (see SPF_Plugin.h).
struct PluginContext {
  static constexpr const char *kPluginName = "MotionCab";

  const SPF_Core_API *core = nullptr;
  SPF_Logger_Handle *logger_handle = nullptr;
  SPF_Config_Handle *config_handle = nullptr;
  SPF_Telemetry_Handle *telemetry_handle = nullptr;
  SPF_KeyBinds_Handle *keybinds_handle = nullptr;
  SPF_Environment_Handle *environment_handle = nullptr;
  SPF_Localization_Handle *localization_handle = nullptr;

  EffectManager effects;
  std::unique_ptr<ManualZoomEffect> manual_zoom;

  // Latest truck telemetry snapshot, refreshed via Tel_RegisterForTruckData.
  SPF_TruckData latest_truck_data{};
  std::atomic<bool> has_truck_data{false};

  // Latest control input snapshot, refreshed via Tel_RegisterForControls.
  // Used instead of SPF_TruckData.effective_steering, which was found to
  // never update at runtime.
  SPF_Controls latest_controls_data{};
  std::atomic<bool> has_controls_data{false};

  // Our own offset added to the interior seat/head pose last frame.
  // Subtracted from the live pose each frame to recover the underlying
  // pose before adding the new offset (see the differential write in
  // Plugin.cpp).
  HeadOffset last_applied_offset{};
  bool was_interior_last_frame = false;
  // Head rotation we wrote last frame (degrees), to tell the native
  // recenter's snap apart from free-look passing through the default.
  float last_written_yaw_deg = 0.0f, last_written_pitch_deg = 0.0f;
  bool has_last_written_rot = false;

  std::chrono::steady_clock::time_point last_update_time{};
  bool has_last_update_time = false;

  // Quick Settings profile cache (ProfileManager.cpp): avoids re-scanning
  // the profiles dir / re-diffing ~40 keys every rendered frame the window
  // is open, since these only actually change on a Save/Load/Delete/edit.
  std::vector<std::string> cached_profile_list;
  bool profile_list_dirty = true;
  bool cached_profile_matches = true;
  std::string matches_cache_profile_name;
  bool profile_matches_dirty = true;

  void Log(SPF_LogLevel level, const char *message) const;

  template <typename... Args>
  void LogFmt(SPF_LogLevel level, const char *fmt, Args &&...args) const {
    if (!core || !core->formatting || !core->logger || !logger_handle)
      return;
    char buffer[512];
    core->formatting->Fmt_Format(buffer, sizeof(buffer), fmt, args...);
    core->logger->Log(logger_handle, level, buffer);
  }

  template <typename... Args>
  void LogThrottledFmt(SPF_LogLevel level, const char *throttle_key,
                       uint32_t throttle_ms, const char *fmt,
                       Args &&...args) const {
    if (!core || !core->formatting || !core->logger || !logger_handle)
      return;
    char buffer[512];
    core->formatting->Fmt_Format(buffer, sizeof(buffer), fmt, args...);
    core->logger->LogThrottled(logger_handle, level, throttle_key, throttle_ms,
                               buffer);
  }
};

PluginContext &Context();

} // namespace motioncab
