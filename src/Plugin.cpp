#include "SPF_Camera_API.h"
#include "SPF_Config_API.h"
#include "SPF_KeyBinds_API.h"
#include "SPF_Logger_API.h"
#include "SPF_Manifest_API.h"
#include "SPF_Plugin.h"
#include "SPF_Telemetry_API.h"
#include "SPF_UI_API.h"

#include "Manifest.hpp"
#include "PluginContext.hpp"
#include "ProfileManager.hpp"
#include "effects/BodyDynamicsEffect.hpp"
#include "effects/EngineStartStopEffect.hpp"
#include "effects/EngineVibrationEffect.hpp"
#include "effects/HeadMotionEffect.hpp"
#include "effects/IdleBreathingEffect.hpp"
#include "effects/ManualLookEffect.hpp"
#include "effects/ManualZoomEffect.hpp"
#include "effects/MirrorCheckEffect.hpp"
#include "effects/RoadIrregularityEffect.hpp"
#include "effects/SpeedShakeEffect.hpp"
#include "effects/SteeringCameraEffect.hpp"
#include "effects/SuspensionEffect.hpp"
#include "ui/SettingsWindow.hpp"

#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <numbers>

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
  Context().effects.NotifyTruckConstants(*data);

  // Don't call Cam_SwitchTo here (even deferred): SPF-Framework's native
  // camera hooks already refresh the pointer via OnActivate/OnDeactivate
  // during truck reconfiguration, and the interior pos/rot getters are
  // null-safe meanwhile. Cam_SwitchTo bypasses that guard and can crash
  // during slow Quick Job reloads (750ms+), so no fixed delay is safe.
}

void OnCommonDataUpdate(const SPF_CommonData *data, void * /*user_data*/) {
  Context().effects.NotifyCommonData(*data);
}

void OnTrailersUpdate(const SPF_Trailer *trailers, uint32_t count,
                      void * /*user_data*/) {
  Context().effects.NotifyTrailers(trailers, count);
}

// ManualLookEffect/ManualZoomEffect poll these actions via
// Kbind_GetActionValue instead. Registering is still required though, an
// unregistered action stays stuck at 0.0 even with a valid manifest binding.
void OnManualLookLeftTriggered() {}
void OnManualLookRightTriggered() {}
void OnManualZoomTriggered() {}

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
  // The About tab's logo texture is the one thing this plugin allocates
  // through the UI API that SPF doesn't free on its own.
  if (Context().core && Context().core->ui)
    ReleaseLogoTexture(Context().core->ui);
  // All API pointers become invalid after this returns; handles besides
  // the logo texture above are owned by the framework.
}

void OnRegisterUI(SPF_UI_API *ui_api) {
  ui_api->UI_RegisterDrawCallback(PluginContext::kPluginName, "MotionCab",
                                  &DrawSettingsWindow, nullptr);
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

void OnActivated(const SPF_Core_API *core_api) {
  PluginContext &ctx = Context();
  ctx.core = core_api;
  ctx.logger_handle =
      core_api->logger->Log_GetContext(PluginContext::kPluginName);
  ctx.config_handle =
      core_api->config->Cfg_GetContext(PluginContext::kPluginName);

  // Acquired before effect registration: ManualLookEffect needs the
  // keybinds API/handle at construction time to poll its look-left/right
  // actions every frame.
  ctx.keybinds_handle =
      core_api->keybinds->Kbind_GetContext(PluginContext::kPluginName);
  ctx.environment_handle =
      core_api->environment->Env_GetContext(PluginContext::kPluginName);
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
  ctx.effects.Register(
      std::make_unique<MirrorCheckEffect>(core_api->config, ctx.config_handle));
  ctx.effects.Register(std::make_unique<ManualLookEffect>(
      core_api->config, ctx.config_handle, core_api->keybinds,
      ctx.keybinds_handle));
  ctx.effects.Register(std::make_unique<RoadIrregularityEffect>(
      core_api->config, ctx.config_handle));
  ctx.effects.Register(
      std::make_unique<SpeedShakeEffect>(core_api->config, ctx.config_handle));
  ctx.effects.Register(std::make_unique<EngineStartStopEffect>(
      core_api->config, ctx.config_handle));
  ctx.effects.Register(std::make_unique<BodyDynamicsEffect>(core_api->config,
                                                            ctx.config_handle));
  ctx.effects.LoadAllConfig();

  ctx.manual_zoom = std::make_unique<ManualZoomEffect>(
      core_api->config, ctx.config_handle, core_api->keybinds,
      ctx.keybinds_handle);
  ctx.manual_zoom->LoadConfig();

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
    ctx.effects.NotifyTruckConstants(initial_constants);
    core_api->telemetry->Tel_RegisterForCommonData(ctx.telemetry_handle,
                                                   OnCommonDataUpdate, nullptr);
    core_api->telemetry->Tel_RegisterForTrailers(ctx.telemetry_handle,
                                                 OnTrailersUpdate, nullptr);
  }

  if (ctx.keybinds_handle) {
    // Only functional keybinds are registered here; effect enable/disable
    // stays in the settings UI. Callbacks are no-ops since the effects poll
    // via Kbind_GetActionValue instead; registering is still required for
    // that polling to work.
    core_api->keybinds->Kbind_Register(
        ctx.keybinds_handle, "ManualLook.look_left", OnManualLookLeftTriggered);
    core_api->keybinds->Kbind_Register(ctx.keybinds_handle,
                                       "ManualLook.look_right",
                                       OnManualLookRightTriggered);
    core_api->keybinds->Kbind_Register(ctx.keybinds_handle, "ManualZoom.zoom",
                                       OnManualZoomTriggered);
    core_api->keybinds->Kbind_Register(ctx.keybinds_handle, "UI.toggle",
                                       OnToggleSettingsWindow);
  }

  ctx.LogFmt(SPF_LOG_INFO, "Version: %s", PLUGIN_VERSION);
  ctx.Log(SPF_LOG_INFO, "Plugin is running!");
}

void OnSettingChanged(SPF_Config_Handle * /*config_handle*/,
                      const char *keyPath) {
  PluginContext &ctx = Context();
  static constexpr char kSettingsPrefix[] = "settings.";
  if (std::strncmp(keyPath, kSettingsPrefix, sizeof(kSettingsPrefix) - 1) ==
      0) {
    ctx.effects.LoadAllConfig();
    if (ctx.manual_zoom)
      ctx.manual_zoom->LoadConfig();
    // A setting just changed, so the cached "matches the active profile?"
    // answer (SettingsWindow.cpp's unsaved-changes marker) is stale.
    ctx.profile_matches_dirty = true;
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
    ctx.was_interior_last_frame = false;
    ctx.has_last_update_time = false;
    return;
  }

  if (!ctx.was_interior_last_frame) {
    // Resets smoothing state on cabin re-entry, but not
    // ctx.last_applied_offset: the engine keeps our last-written offset
    // even while inactive, so it's still what must be subtracted below on
    // the first frame back. Zeroing it would bake the stale offset into the
    // live pose, compounding on every view switch.
    ctx.effects.ResetAll();
    if (ctx.manual_zoom)
      ctx.manual_zoom->Reset();
    ctx.was_interior_last_frame = true;
  }

  const float dt = ComputeDeltaTimeSeconds(ctx);
  // Acquire pairs with OnTruckDataUpdate/OnControlsUpdate's release stores
  // above: guarantees the struct reads below see the write that set the
  // flag, in case telemetry callbacks run on a different thread.
  if (dt <= 0.0f || !ctx.has_truck_data.load(std::memory_order_acquire) ||
      !ctx.has_controls_data.load(std::memory_order_acquire))
    return;

  if (ctx.manual_zoom && ctx.manual_zoom->IsEnabled())
    ctx.manual_zoom->Update(dt, ctx.core->camera);

  // Differential write: subtract our last offset from the live pose, then
  // layer the new one on top, so we don't fight free-look. Skip entirely
  // if the getters fail (e.g. camera not resolved yet), since a bogus
  // reading would ratchet the seat via a bad last_applied_offset.
  float seat_x, seat_y, seat_z;
  float yaw_rad, pitch_rad;
  const bool got_seat =
      ctx.core->camera->Cam_GetInteriorSeatPos(&seat_x, &seat_y, &seat_z);
  const bool got_rot =
      ctx.core->camera->Cam_GetInteriorHeadRot(&yaw_rad, &pitch_rad);
  if (!got_seat || !got_rot)
    return;

  constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;

  // Detect the native "recenter camera" hotkey: it snaps rotation straight
  // to the raw default with no event to hook, so infer it heuristically
  // instead (our contribution is non-trivial, and rotation jumps away from
  // what we wrote to land exactly on default) and resync like a fresh
  // cabin entry. Landing on default alone isn't enough: free-look sweeping
  // through the default while an effect is active (e.g. steering camera)
  // passes within epsilon of it, and a false positive re-bases the pose,
  // snapping the player's own look back to center.
  float default_yaw_deg, default_pitch_deg;
  if (ctx.has_last_written_rot &&
      ctx.core->camera->Cam_GetInteriorRotationDefaults(&default_yaw_deg,
                                                        &default_pitch_deg)) {
    constexpr float kEpsilonDeg = 0.01f;
    constexpr float kMeaningfulOffsetDeg = 0.5f;
    auto was_reset = [&](float live_deg, float default_deg, float written_deg,
                         float offset_deg) {
      return std::fabs(offset_deg) > kMeaningfulOffsetDeg &&
             std::fabs(live_deg - written_deg) > kMeaningfulOffsetDeg &&
             std::fabs(live_deg - default_deg) < kEpsilonDeg;
    };
    const bool yaw_was_reset =
        was_reset(yaw_rad / kDegToRad, default_yaw_deg,
                  ctx.last_written_yaw_deg, ctx.last_applied_offset.yaw);
    const bool pitch_was_reset =
        was_reset(pitch_rad / kDegToRad, default_pitch_deg,
                  ctx.last_written_pitch_deg, ctx.last_applied_offset.pitch);
    if (yaw_was_reset || pitch_was_reset) {
      ctx.effects.ResetAll();
      ctx.last_applied_offset.yaw = 0.0f;
      ctx.last_applied_offset.pitch = 0.0f;
    }
  }

  const HeadOffset offset = ctx.effects.UpdateAndAccumulate(
      dt, ctx.latest_truck_data, ctx.latest_controls_data);

  const float base_seat_x = seat_x - ctx.last_applied_offset.pos_x;
  const float base_seat_y = seat_y - ctx.last_applied_offset.pos_y;
  const float base_seat_z = seat_z - ctx.last_applied_offset.pos_z;
  ctx.core->camera->Cam_SetInteriorSeatPos(base_seat_x + offset.pos_x,
                                           base_seat_y + offset.pos_y,
                                           base_seat_z + offset.pos_z);

  // HeadOffset.yaw/pitch are degrees (see Effect.hpp); Cam_SetInteriorHeadRot
  // is documented (and its own usage example confirms) to take radians.
  const float base_yaw_rad = yaw_rad - ctx.last_applied_offset.yaw * kDegToRad;
  const float base_pitch_rad =
      pitch_rad - ctx.last_applied_offset.pitch * kDegToRad;
  const float new_yaw_rad = base_yaw_rad + offset.yaw * kDegToRad;
  const float new_pitch_rad = base_pitch_rad + offset.pitch * kDegToRad;
  ctx.core->camera->Cam_SetInteriorHeadRot(new_yaw_rad, new_pitch_rad);
  ctx.last_written_yaw_deg = new_yaw_rad / kDegToRad;
  ctx.last_written_pitch_deg = new_pitch_rad / kDegToRad;
  ctx.has_last_written_rot = true;

  float roll_deg = 0.0f;
  if (ctx.core->camera->Cam_GetInteriorRoll(&roll_deg)) {
    const float base_roll_deg = roll_deg - ctx.last_applied_offset.roll;
    ctx.core->camera->Cam_SetInteriorRoll(base_roll_deg + offset.roll);
    ctx.last_applied_offset.roll = offset.roll;
  }

  ctx.last_applied_offset.pos_x = offset.pos_x;
  ctx.last_applied_offset.pos_y = offset.pos_y;
  ctx.last_applied_offset.pos_z = offset.pos_z;
  ctx.last_applied_offset.yaw = offset.yaw;
  ctx.last_applied_offset.pitch = offset.pitch;
}

void OnGameWorldReady() {
  PluginContext &ctx = Context();
  ctx.was_interior_last_frame = false;
  ctx.has_last_update_time = false;
  ctx.has_truck_data.store(false, std::memory_order_relaxed);
  ctx.has_controls_data.store(false, std::memory_order_relaxed);
}

void OnWorldUnloaded() {
  PluginContext &ctx = Context();
  ctx.has_truck_data.store(false, std::memory_order_relaxed);
  ctx.has_controls_data.store(false, std::memory_order_relaxed);
}

} // namespace

extern "C" SPF_PLUGIN_EXPORT bool
SPF_GetManifestAPI(SPF_Manifest_API *out_api) {
  if (!out_api)
    return false;
  out_api->BuildManifest = &BuildManifest;
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
