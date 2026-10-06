#include "SPF_Camera_API.h"
#include "SPF_Config_API.h"
#include "SPF_KeyBinds_API.h"
#include "SPF_Logger_API.h"
#include "SPF_Manifest_API.h"
#include "SPF_Plugin.h"
#include "SPF_Telemetry_API.h"
#include "SPF_UI_API.h"

#include "core/Keybinds.hpp"
#include "core/Loc.hpp"
#include "core/Manifest.hpp"
#include "core/PluginContext.hpp"
#include "core/Profiles.hpp"
#include "effects/cabin/EngineStartStopEffect.hpp"
#include "effects/cabin/EngineVibrationEffect.hpp"
#include "effects/cabin/IdleBreathingEffect.hpp"
#include "effects/cabin/NaturalHeadMovementEffect.hpp"
#include "effects/driving/BodyDynamicsEffect.hpp"
#include "effects/driving/HeadMotionEffect.hpp"
#include "effects/driving/SteeringCameraEffect.hpp"
#include "effects/manual/BlindspotViewerEffect.hpp"
#include "effects/manual/ManualLookEffect.hpp"
#include "effects/manual/ManualZoomEffect.hpp"
#include "effects/manual/MirrorCheckEffect.hpp"
#include "effects/manual/cabin_walk/CabinWalkEffect.hpp"
#include "effects/road/RoadIrregularityEffect.hpp"
#include "effects/road/SpeedShakeEffect.hpp"
#include "effects/road/SuspensionEffect.hpp"
#include "ui/SettingsWindow.hpp"
#include "ui/Widgets.hpp"

#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <optional>

using namespace motioncab;

namespace {

void OnTruckDataUpdate(const SPF_TruckData *data, void * /*user_data*/) {
  PluginContext &ctx = Context();
  ctx.latest_truck_data = *data;
  // Release pairs with OnUpdate's acquire load below: guarantees the
  // struct write above is visible before the flag is, in case this
  // callback and OnUpdate ever run on different threads.
  ctx.has_truck_data.store(true, std::memory_order_release);
}

void OnControlsUpdate(const SPF_Controls *data, void * /*user_data*/) {
  PluginContext &ctx = Context();
  ctx.latest_controls_data = *data;
  ctx.has_controls_data.store(true, std::memory_order_release);
}

void OnTruckConstantsUpdate(const SPF_TruckConstants *data,
                            void * /*user_data*/) {
  PluginContext &ctx = Context();
  // Another truck (or none, in between two): the game rebuilds the interior
  // camera with the player's own pose, which holds none of our offset.
  const std::string truck = std::string(data->brand_id) + "/" + data->id;
  if (truck != ctx.truck_identity) {
    ctx.truck_identity = truck;
    ctx.camera_rig.ForgetAppliedPose();
    ctx.ResetEffects();
  }
  ctx.effects.NotifyTruckConstants(*data);
  if (ctx.cabin_walk) {
    ctx.cabin_walk->OnTruckConstantsChanged(*data);
  }

  // Don't call Cam_SwitchTo here (even deferred) to get SPF onto the new
  // truck's camera: it can crash during slow Quick Job reloads (750ms+),
  // so no fixed delay is safe. Picking up the rebuilt camera is SPF's job:
  // older releases only did it on a view change, so a truck switch in the
  // interior view left them writing to the freed camera.
}

void OnCommonDataUpdate(const SPF_CommonData *data, void * /*user_data*/) {
  Context().effects.NotifyCommonData(*data);
}

void OnTrailersUpdate(const SPF_Trailer *trailers, uint32_t count,
                      void * /*user_data*/) {
  Context().effects.NotifyTrailers(trailers, count);
}

// Callback for keybinds::kPolledActions, which the effects poll via
// Kbind_GetActionValue instead. Registering is still required though, an
// unregistered action stays stuck at 0.0 even with a valid manifest binding.
void OnPolledActionTriggered() {}

// Tells the player why Cabin Walk refused to stand up, or sent them back to
// the wheel.
void ShowCabinWalkNotice(PluginContext &ctx) {
  const char *key = nullptr;
  switch (ctx.cabin_walk->TakeNotice()) {
  case CabinWalkEffect::Notice::kNeedsParkingBrake:
    key = "ui.cabin_walk.needs_parking_brake";
    break;
  case CabinWalkEffect::Notice::kNeedsStop:
    key = "ui.cabin_walk.needs_stop";
    break;
  case CabinWalkEffect::Notice::kBackToWheel:
    key = "ui.cabin_walk.back_to_wheel";
    break;
  case CabinWalkEffect::Notice::kNone:
    break;
  }
  if (key && ctx.core->ui)
    ui::ShowToast(ctx.core->ui, SPF_NOTIFICATION_WARNING, loc::Tr(key));
}

float ComputeDeltaTimeSeconds(PluginContext &ctx) {
  const auto now = std::chrono::steady_clock::now();
  if (!ctx.has_last_update_time) {
    ctx.last_update_time = now;
    ctx.has_last_update_time = true;
    return 0.0f;
  }
  const float dt =
      std::chrono::duration<float>(now - ctx.last_update_time).count();
  ctx.last_update_time = now;
  return dt;
}

// --- SPF_Plugin_Exports lifecycle callbacks ---

void OnLoad(const SPF_Load_API *load_api) {
  PluginContext &ctx = Context();
  ctx.logger_handle =
      load_api->logger->Log_GetContext(PluginContext::kPluginName);
  ctx.config_handle =
      load_api->config->Cfg_GetContext(PluginContext::kPluginName);
}

void OnUnload() {
  // Runs before SPF's own final settings.json flush, while the config API
  // is still valid, discard unsaved tweaks here so they never reach disk.
  profiles::RevertUnsavedChanges(Context());
  // Don't leave the game's dynamic FOV switched off if we were mid-zoom.
  if (Context().manual_zoom) {
    Context().manual_zoom->RestoreFov();
    Context().manual_zoom->RestoreDynamicFov();
  }
  // Give the camera's own limits and the mouse back if the player is up in
  // the cabin. Their position is part of the offset below.
  PluginContext &ctx = Context();
  if (ctx.cabin_walk)
    ctx.cabin_walk->Shutdown(ctx.core ? ctx.core->camera : nullptr);
  ctx.cabin_walk_sounds.Shutdown(ctx.core ? ctx.core->sound : nullptr);
  // Take our offset back out of the interior pose, which the engine keeps
  // after we're gone. After ManualZoomEffect's RestoreFov above, which
  // adds the applied FOV offset back on.
  if (ctx.core && ctx.core->camera)
    ctx.camera_rig.Remove(ctx.core->camera);
  // The About tab's logo texture is the one thing this plugin allocates
  // through the UI API that SPF doesn't free on its own.
  if (Context().core && Context().core->ui)
    ui::ReleaseLogoTexture(Context().core->ui);
  loc::Reset();
  // All API pointers become invalid after this returns; handles besides
  // the logo texture above are owned by the framework.
}

void OnRegisterUI(SPF_UI_API *ui_api) {
  ui_api->UI_RegisterDrawCallback(PluginContext::kPluginName, "MotionCab",
                                  &ui::DrawSettingsWindow, nullptr);
}

void OnToggleSettingsWindow() {
  PluginContext &ctx = Context();
  if (!ctx.core || !ctx.core->config || !ctx.config_handle)
    return;

  static constexpr char kKey[] = "ui.windows.MotionCab.is_visible";
  const bool new_state =
      !ctx.core->config->Cfg_GetBool(ctx.config_handle, kKey, false);
  ctx.core->config->Cfg_SetBool(ctx.config_handle, kKey, new_state);
}

// `file` in the plugin's data directory, next to the profiles; empty (the
// file then isn't used) if the data directory isn't available.
std::string DataFilePath(PluginContext &ctx, const char *file) {
  const std::string dir = ctx.PluginDataDir();
  if (dir.empty() || !ctx.core->environment->Env_CreatePath(
                         ctx.environment_handle, dir.c_str()))
    return {};
  return dir + "/" + file;
}

void OnActivated(const SPF_Core_API *core_api) {
  PluginContext &ctx = Context();
  ctx.core = core_api;
  ctx.logger_handle =
      core_api->logger->Log_GetContext(PluginContext::kPluginName);
  ctx.LogFmt(SPF_LOG_INFO, "Version: %s", PLUGIN_VERSION);
  ctx.Log(SPF_LOG_INFO, "Starting...");
  ctx.config_handle =
      core_api->config->Cfg_GetContext(PluginContext::kPluginName);

  // Acquired before effect registration: ManualLookEffect needs the
  // keybinds API/handle at construction time to poll its look-left/right
  // actions every frame.
  ctx.keybinds_handle =
      core_api->keybinds->Kbind_GetContext(PluginContext::kPluginName);
  ctx.environment_handle =
      core_api->environment->Env_GetContext(PluginContext::kPluginName);
  ctx.localization_handle =
      core_api->localization->Loc_GetContext(PluginContext::kPluginName);
  profiles::MigrateLegacySettings(ctx);
  profiles::EnsureDefaultExists(ctx);
  profiles::ResolveUnknownActiveProfile(ctx);

  ctx.effects.Register(
      std::make_unique<HeadMotionEffect>(core_api->config, ctx.config_handle));
  ctx.effects.Register(std::make_unique<SteeringCameraEffect>(
      core_api->config, ctx.config_handle));
  ctx.effects.Register(std::make_unique<IdleBreathingEffect>(
      core_api->config, ctx.config_handle));
  ctx.effects.Register(
      std::make_unique<SuspensionEffect>(core_api->config, ctx.config_handle));
  ctx.effects.Register(std::make_unique<EngineVibrationEffect>(
      core_api->config, ctx.config_handle));
  // Mirror Check first: Manual Look reads it the same frame.
  auto mirror_check =
      std::make_unique<MirrorCheckEffect>(core_api->config, ctx.config_handle);
  const MirrorCheckEffect *mirror_check_ptr = mirror_check.get();
  ctx.effects.Register(std::move(mirror_check));
  ctx.effects.Register(std::make_unique<ManualLookEffect>(
      core_api->config, ctx.config_handle, core_api->keybinds,
      ctx.keybinds_handle, mirror_check_ptr));
  ctx.effects.Register(std::make_unique<RoadIrregularityEffect>(
      core_api->config, ctx.config_handle));
  ctx.effects.Register(
      std::make_unique<SpeedShakeEffect>(core_api->config, ctx.config_handle));
  ctx.effects.Register(std::make_unique<EngineStartStopEffect>(
      core_api->config, ctx.config_handle));
  ctx.effects.Register(std::make_unique<NaturalHeadMovementEffect>(
      core_api->config, ctx.config_handle));
  ctx.effects.Register(std::make_unique<BodyDynamicsEffect>(core_api->config,
                                                            ctx.config_handle));
  ctx.effects.Register(std::make_unique<BlindspotViewerEffect>(
      core_api->config, ctx.config_handle, core_api->keybinds,
      ctx.keybinds_handle));
  ctx.manual_zoom = std::make_unique<ManualZoomEffect>(
      core_api->config, ctx.config_handle, core_api->keybinds,
      ctx.keybinds_handle, &ctx.camera_rig);
  // The player's own layouts (the presets are compiled in).
  ctx.cabin_layouts = std::make_unique<CabinLayoutStore>(
      core_api->config, DataFilePath(ctx, "cabin_layouts.json"));
  if (const std::string dir = ctx.PluginDataDir(); !dir.empty())
    ctx.cabin_walk_sounds.SetDirectory(dir + "/sounds");
  ctx.cabin_walk = std::make_unique<CabinWalkEffect>(
      core_api->config, ctx.config_handle, core_api->keybinds,
      ctx.keybinds_handle, core_api->ui, ctx.cabin_layouts.get());
  ctx.ReloadEffectsConfig();

  ctx.telemetry_handle =
      core_api->telemetry->Tel_GetContext(PluginContext::kPluginName);
  if (ctx.telemetry_handle) {
    core_api->telemetry->Tel_RegisterForTruckData(ctx.telemetry_handle,
                                                  OnTruckDataUpdate, nullptr);
    core_api->telemetry->Tel_RegisterForControls(ctx.telemetry_handle,
                                                 OnControlsUpdate, nullptr);
    core_api->telemetry->Tel_RegisterForTruckConstants(
        ctx.telemetry_handle, OnTruckConstantsUpdate, nullptr);
    // Tel_RegisterForTruckConstants only fires on a later change event, not
    // at registration. Fetch the current snapshot now so effects gating on
    // it (e.g. is_electric_) see it even when activating mid-session.
    SPF_TruckConstants initial_constants{};
    core_api->telemetry->Tel_GetTruckConstants(
        ctx.telemetry_handle, &initial_constants, sizeof(initial_constants));
    // The current truck, so a later event for it isn't taken for a switch
    // (which would forget the offset still in the pose).
    ctx.truck_identity =
        std::string(initial_constants.brand_id) + "/" + initial_constants.id;
    ctx.effects.NotifyTruckConstants(initial_constants);
    ctx.cabin_walk->OnTruckConstantsChanged(initial_constants);
    core_api->telemetry->Tel_RegisterForCommonData(ctx.telemetry_handle,
                                                   OnCommonDataUpdate, nullptr);
    core_api->telemetry->Tel_RegisterForTrailers(ctx.telemetry_handle,
                                                 OnTrailersUpdate, nullptr);
  }

  if (ctx.keybinds_handle) {
    // Only functional keybinds are registered here; effect enable/disable
    // stays in the settings UI.
    for (const keybinds::Action &action : keybinds::kPolledActions)
      core_api->keybinds->Kbind_Register(ctx.keybinds_handle, action.id,
                                         OnPolledActionTriggered);
    core_api->keybinds->Kbind_Register(ctx.keybinds_handle,
                                       keybinds::kToggleWindow.id,
                                       OnToggleSettingsWindow);
  }

  ctx.Log(SPF_LOG_INFO, "Plugin is running!");
}

void OnSettingChanged(SPF_Config_Handle * /*config_handle*/,
                      const char *keyPath) {
  PluginContext &ctx = Context();
  static constexpr char kSettingsPrefix[] = "settings.";
  if (std::strncmp(keyPath, kSettingsPrefix, sizeof(kSettingsPrefix) - 1) ==
      0) {
    ctx.ReloadEffectsConfig();
    // A setting just changed, so the cached "matches the active profile?"
    // answer (SettingsWindow.cpp's unsaved-changes marker) is stale.
    profiles::InvalidateMatchCache();
  }
}

void OnUpdate() {
  PluginContext &ctx = Context();
  if (!ctx.core || !ctx.core->camera)
    return;

  if (ctx.core->telemetry && ctx.telemetry_handle) {
    SPF_GameState game_state{};
    ctx.core->telemetry->Tel_GetGameState(ctx.telemetry_handle, &game_state,
                                          sizeof(game_state));
    if (game_state.paused) {
      if (ctx.was_interior_last_frame && ctx.manual_zoom)
        ctx.manual_zoom->RestoreFov();
      // Leaving a truck (job, garage, save load) goes through a menu, and
      // the game saves that truck's FOV as it is then. After the zoom's
      // restore above, which adds the applied FOV offset back on.
      ctx.camera_rig.RemoveFov(ctx.core->camera);
      // SCS's "paused" telemetry flag covers native menus, not just the
      // pause screen, including F4's interior seat/FOV/lighting overlay.
      // That overlay doesn't change the reported camera type, so
      // is_interior below would otherwise stay true and effects would
      // keep animating behind it, which is most noticeable for any
      // effect that reacts while the truck is stationary.
      ctx.was_interior_last_frame = false;
      ctx.has_last_update_time = false;
      return;
    }
  }

  SPF_CameraType camera_type;
  const bool have_camera = ctx.core->camera->Cam_GetCurrentCamera(&camera_type);
  const bool is_interior = have_camera && camera_type == SPF_CAMERA_INTERIOR;

  // Per project policy, every effect is active in cabin (interior) view only.
  if (!is_interior) {
    if (ctx.was_interior_last_frame && ctx.manual_zoom)
      ctx.manual_zoom->RestoreFov();
    if (ctx.cabin_walk)
      ctx.cabin_walk->OnLeftInterior();
    ctx.was_interior_last_frame = false;
    ctx.has_last_update_time = false;
    return;
  }

  if (!ctx.was_interior_last_frame) {
    // Resets smoothing state on cabin re-entry, but not the camera rig's
    // applied offset: the engine keeps our last-written offset even while
    // inactive, so it's still what must be subtracted below on the first
    // frame back. Zeroing it would bake the stale offset into the live
    // pose, compounding on every view switch.
    ctx.ResetEffects();
    ctx.was_interior_last_frame = true;
  }

  const float dt = ComputeDeltaTimeSeconds(ctx);
  // Acquire pairs with OnTruckDataUpdate/OnControlsUpdate's release stores
  // above: guarantees the struct reads below see the write that set the
  // flag, in case telemetry callbacks run on a different thread.
  if (dt <= 0.0f || !ctx.has_truck_data.load(std::memory_order_acquire) ||
      !ctx.has_controls_data.load(std::memory_order_acquire))
    return;

  // Before ManualZoomEffect's write: the live FOV must still be last
  // frame's, ours or someone else's (a truck switch's new camera, F4).
  ctx.camera_rig.DetectExternalFovWrite(ctx.core->camera);
  if (ctx.manual_zoom && ctx.manual_zoom->IsEnabled())
    ctx.manual_zoom->Update(dt, ctx.core->camera);

  // Differential write (see CameraRig), skipped entirely while the camera
  // isn't resolved yet.
  SPF_Camera_API *camera = ctx.core->camera;
  std::optional<CameraRig::Pose> pose = ctx.camera_rig.ReadPose(camera);
  if (!pose)
    return;
  if (ctx.camera_rig.DetectNativeRecenter(camera, *pose))
    ctx.effects.ResetAll();
  ctx.camera_rig.DetectExternalSeatWrite(*pose);

  // Cabin Walk first: whether the player is at the wheel decides which
  // effects play, and sitting down turns the player's own view.
  HeadOffset walk{};
  if (ctx.cabin_walk) {
    walk = ctx.cabin_walk->Update(dt, ctx.latest_truck_data, camera,
                                  ctx.camera_rig.Base(*pose));
    if (const auto &rotation = ctx.cabin_walk->base_rotation())
      ctx.camera_rig.SetBaseRotation(*pose, rotation->first, rotation->second);
    ctx.effects.SetAtWheel(ctx.cabin_walk->AtWheel());
    ShowCabinWalkNotice(ctx);
    ctx.cabin_walk_sounds.Update(ctx.core->sound);
    for (float volume : ctx.cabin_walk->TakeFootsteps())
      ctx.cabin_walk_sounds.PlayFootstep(ctx.core->sound, volume);
  }

  HeadOffset offset = ctx.effects.UpdateAndAccumulate(dt, ctx.latest_truck_data,
                                                      ctx.latest_controls_data);
  offset.pos_x += walk.pos_x;
  offset.pos_y += walk.pos_y;
  offset.pos_z += walk.pos_z;
  offset.yaw += walk.yaw;
  offset.pitch += walk.pitch;
  offset.roll += walk.roll;
  offset.fov += walk.fov;
  ctx.camera_rig.Apply(camera, *pose, offset);
}

void OnGameWorldReady() {
  PluginContext &ctx = Context();
  // A new world starts in the driver's seat, with a new camera.
  if (ctx.cabin_walk)
    ctx.cabin_walk->SnapToWheel();
  ctx.was_interior_last_frame = false;
  ctx.has_last_update_time = false;
  ctx.has_truck_data.store(false, std::memory_order_relaxed);
  ctx.has_controls_data.store(false, std::memory_order_relaxed);
}

void OnWorldUnloaded() {
  PluginContext &ctx = Context();
  ctx.has_truck_data.store(false, std::memory_order_relaxed);
  ctx.has_controls_data.store(false, std::memory_order_relaxed);
  // SPF rebuilds its interior camera for the next world, roll back at 0:
  // subtracting the old roll offset from it would tilt the view for good.
  ctx.camera_rig.ForgetApplied();
  // The bank's handle dies with the world's sound system, and Update only
  // runs in the interior view, too late to see it go: unload it while the
  // sound system still runs, so the next world loads it again.
  ctx.cabin_walk_sounds.Shutdown(ctx.core ? ctx.core->sound : nullptr);
}

} // namespace

extern "C" SPF_PLUGIN_EXPORT bool
SPF_GetManifestAPI(SPF_Manifest_API *out_api) {
  if (!out_api)
    return false;
  out_api->BuildManifest = &manifest::Build;
  return true;
}

extern "C" SPF_PLUGIN_EXPORT bool SPF_GetPlugin(SPF_Plugin_Exports *exports) {
  if (!exports)
    return false;

  exports->OnLoad = &OnLoad;
  exports->OnUnload = &OnUnload;
  exports->OnUpdate = &OnUpdate;
  exports->OnRegisterUI = &OnRegisterUI;
  exports->OnSettingChanged = &OnSettingChanged;
  exports->OnActivated = &OnActivated;
  exports->OnGameWorldReady = &OnGameWorldReady;
  exports->OnLanguageChanged = nullptr;
  exports->OnWorldUnloaded = &OnWorldUnloaded;

  return true;
}
