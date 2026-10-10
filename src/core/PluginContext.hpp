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
#include "core/CameraRig.hpp"
#include "effects/EffectManager.hpp"
#include "effects/manual/ManualZoomEffect.hpp"
#include "effects/manual/cabin_walk/CabinLayouts.hpp"
#include "effects/manual/cabin_walk/CabinWalkEffect.hpp"
#include "effects/manual/cabin_walk/CabinWalkSounds.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>

namespace motioncab {

// Filled in bit by bit as SPF's lifecycle callbacks fire.
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
  std::unique_ptr<CabinLayoutStore> cabin_layouts;
  std::unique_ptr<CabinWalkEffect> cabin_walk;
  CabinWalkSounds cabin_walk_sounds;

  SPF_TruckData latest_truck_data{};
  std::atomic<bool> has_truck_data{false};

  // Our steering source: SPF_TruckData.effective_steering never updates.
  SPF_Controls latest_controls_data{};
  std::atomic<bool> has_controls_data{false};

  CameraRig camera_rig;
  // "<brand_id>/<id>", to tell a truck switch from a change to the same
  // truck.
  std::string truck_identity;
  bool was_interior_last_frame = false;

  std::chrono::steady_clock::time_point last_update_time{};
  bool has_last_update_time = false;

  // Use these two rather than going through each effect by hand, so the
  // ones held apart aren't forgotten.
  void ReloadEffectsConfig();
  // Leaves Cabin Walk alone: a player up in the cabin stays there across a
  // view switch or a recenter.
  void ResetEffects();

  // Empty if the environment API isn't there yet or the call fails.
  std::string PluginDataDir() const;

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
