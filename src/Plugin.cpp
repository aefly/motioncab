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
  // Pairs with OnUpdate's acquire, in case telemetry ever comes from another
  // thread.
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
  // Another truck (or none, between two) gets a new interior camera, without
  // our offset in it.
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

  // No Cam_SwitchTo here to get SPF onto the new camera, even deferred: it
  // can crash during slow Quick Job reloads (750 ms and more), so no delay is
  // safe. Picking up the new camera is SPF's job (fixed in 1.2.5).
}

void OnCommonDataUpdate(const SPF_CommonData *data, void * /*user_data*/) {
  Context().effects.NotifyCommonData(*data);
}

void OnTrailersUpdate(const SPF_Trailer *trailers, uint32_t count,
                      void * /*user_data*/) {
  Context().effects.NotifyTrailers(trailers, count);
}

// The polled actions don't need a callback, but an unregistered action
// always reads 0.
void OnPolledActionTriggered() {}

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
  // Before SPF's last settings.json flush, so unsaved tweaks never reach
  // disk.
  profiles::RevertUnsavedChanges(Context());
  if (Context().manual_zoom) {
    Context().manual_zoom->RestoreFov();
    Context().manual_zoom->RestoreDynamicFov();
  }
  // Cabin Walk's position is part of the offset removed below.
  PluginContext &ctx = Context();
  if (ctx.cabin_walk)
    ctx.cabin_walk->Shutdown(ctx.core ? ctx.core->camera : nullptr);
  ctx.cabin_walk_sounds.Shutdown(ctx.core ? ctx.core->sound : nullptr);
  // The engine would keep our offset after we're gone. This has to follow
  // RestoreFov, which puts the applied FOV offset back on.
  if (ctx.core && ctx.core->camera)
    ctx.camera_rig.Remove(ctx.core->camera);
  // The one thing we allocate that SPF doesn't free for us.
  if (Context().core && Context().core->ui)
    ui::ReleaseLogoTexture(Context().core->ui);
  loc::Reset();
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

// Empty if there's no data directory, in which case the file isn't used.
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

  // Before the effects: the ones polling keys take the handle at
  // construction.
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
    // The callback only fires on a later change, so activating mid-session
    // would leave the effects without the current truck.
    SPF_TruckConstants initial_constants{};
    core_api->telemetry->Tel_GetTruckConstants(
        ctx.telemetry_handle, &initial_constants, sizeof(initial_constants));
    // So a later event for this truck isn't taken for a switch, which would
    // forget the offset still in the pose.
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
      // Leaving a truck always goes through a menu, and the game saves its
      // FOV as it is then. After RestoreFov, which puts our offset back on.
      ctx.camera_rig.RemoveFov(ctx.core->camera);
      // "Paused" covers every native menu, F4's seat/FOV overlay included,
      // which still reports the interior camera: the effects would keep
      // moving behind it otherwise.
      ctx.was_interior_last_frame = false;
      ctx.has_last_update_time = false;
      return;
    }
  }

  SPF_CameraType camera_type;
  const bool have_camera = ctx.core->camera->Cam_GetCurrentCamera(&camera_type);
  const bool is_interior = have_camera && camera_type == SPF_CAMERA_INTERIOR;

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
    // The camera rig's applied offset stays: the engine kept it in the pose
    // while we were away, and it still has to come out on the first frame
    // back. Zeroing it would pile the offset up on every view switch.
    ctx.ResetEffects();
    ctx.was_interior_last_frame = true;
  }

  const float dt = ComputeDeltaTimeSeconds(ctx);
  if (dt <= 0.0f || !ctx.has_truck_data.load(std::memory_order_acquire) ||
      !ctx.has_controls_data.load(std::memory_order_acquire))
    return;

  // Before the zoom writes, while the live FOV is still last frame's.
  ctx.camera_rig.DetectExternalFovWrite(ctx.core->camera);
  if (ctx.manual_zoom && ctx.manual_zoom->IsEnabled())
    ctx.manual_zoom->Update(dt, ctx.core->camera);

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
  // SPF's next camera starts with no roll: subtracting the old roll offset
  // from it would tilt the view for good.
  ctx.camera_rig.ForgetApplied();
  // The bank dies with this world's sound system, and nothing else would
  // notice in time: unload it now so the next world loads it again.
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
