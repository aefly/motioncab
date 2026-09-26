#include "SettingsWindow.hpp"

#include "Links.hpp"
#include "Localization.hpp"
#include "PluginContext.hpp"
#include "ProfileManager.hpp"
#include "SPF_Icons.h"
#include "ui/OpenUrl.hpp"
#include "ui/SettingsDefaults.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>
#include <string_view>

namespace motioncab {

namespace {

constexpr float kAccentR = 0xB8 / 255.0f;
constexpr float kAccentG = 0x27 / 255.0f;
constexpr float kAccentB = 0x28 / 255.0f;
constexpr float kAccentHoverR = 0x8A / 255.0f;
constexpr float kAccentHoverG = 0x10 / 255.0f;
constexpr float kAccentHoverB = 0x10 / 255.0f;

int PushBrandColors(SPF_UI_API *ui) {
  ui->UI_PushStyleColor(SPF_COLOR_BORDER, kAccentR, kAccentG, kAccentB, 0.5f);
  ui->UI_PushStyleColor(SPF_COLOR_NAV_HIGHLIGHT, kAccentR, kAccentG, kAccentB,
                        1.0f);
  ui->UI_PushStyleColor(SPF_COLOR_HEADER, kAccentR, kAccentG, kAccentB, 0.45f);
  ui->UI_PushStyleColor(SPF_COLOR_HEADER_HOVERED, kAccentR, kAccentG, kAccentB,
                        0.25f);
  ui->UI_PushStyleColor(SPF_COLOR_HEADER_ACTIVE, kAccentHoverR, kAccentHoverG,
                        kAccentHoverB, 0.9f);
  ui->UI_PushStyleColor(SPF_COLOR_SLIDER_GRAB, kAccentR, kAccentG, kAccentB,
                        1.0f);
  ui->UI_PushStyleColor(SPF_COLOR_SLIDER_GRAB_ACTIVE, kAccentHoverR,
                        kAccentHoverG, kAccentHoverB, 1.0f);
  ui->UI_PushStyleColor(SPF_COLOR_FRAME_BG, kAccentR, kAccentG, kAccentB,
                        0.25f);
  ui->UI_PushStyleColor(SPF_COLOR_TAB_ACTIVE, kAccentR, kAccentG, kAccentB,
                        1.0f);
  ui->UI_PushStyleColor(SPF_COLOR_TAB_HOVERED, kAccentR, kAccentG, kAccentB,
                        0.8f);
  ui->UI_PushStyleColor(SPF_COLOR_TAB, kAccentR, kAccentG, kAccentB, 0.25f);
  ui->UI_PushStyleColor(SPF_COLOR_TAB_UNFOCUSED_ACTIVE, kAccentR, kAccentG,
                        kAccentB, 0.9f);
  ui->UI_PushStyleColor(SPF_COLOR_TAB_UNFOCUSED, kAccentR, kAccentG, kAccentB,
                        0.2f);
  ui->UI_PushStyleColor(SPF_COLOR_FRAME_BG_HOVERED, kAccentR, kAccentG,
                        kAccentB, 0.35f);
  ui->UI_PushStyleColor(SPF_COLOR_FRAME_BG_ACTIVE, kAccentHoverR, kAccentHoverG,
                        kAccentHoverB, 0.55f);
  ui->UI_PushStyleColor(SPF_COLOR_BUTTON, kAccentR, kAccentG, kAccentB, 0.8f);
  ui->UI_PushStyleColor(SPF_COLOR_BUTTON_HOVERED, kAccentR, kAccentG, kAccentB,
                        1.0f);
  ui->UI_PushStyleColor(SPF_COLOR_BUTTON_ACTIVE, kAccentHoverR, kAccentHoverG,
                        kAccentHoverB, 1.0f);
  return 18;
}

// Softer, slightly rounded look for the whole window instead of SPF's
// default sharp corners.
int PushBrandRounding(SPF_UI_API *ui) {
  ui->UI_PushStyleVarFloat(SPF_STYLE_VAR_WINDOW_ROUNDING, 8.0f);
  ui->UI_PushStyleVarFloat(SPF_STYLE_VAR_CHILD_ROUNDING, 8.0f);
  ui->UI_PushStyleVarFloat(SPF_STYLE_VAR_POPUP_ROUNDING, 8.0f);
  ui->UI_PushStyleVarFloat(SPF_STYLE_VAR_FRAME_ROUNDING, 4.0f);
  ui->UI_PushStyleVarFloat(SPF_STYLE_VAR_GRAB_ROUNDING, 4.0f);
  ui->UI_PushStyleVarFloat(SPF_STYLE_VAR_TAB_ROUNDING, 6.0f);
  ui->UI_PushStyleVarFloat(SPF_STYLE_VAR_SCROLLBAR_ROUNDING, 6.0f);
  return 7;
}

// Button in the muted red used by the sliders/fields (FRAME_BG values) and the
// keybind buttons, instead of the brighter brand red of regular buttons.
bool MutedButton(SPF_UI_API *ui, const char *label, float width = 0.0f) {
  ui->UI_PushStyleColor(SPF_COLOR_BUTTON, kAccentR, kAccentG, kAccentB, 0.25f);
  ui->UI_PushStyleColor(SPF_COLOR_BUTTON_HOVERED, kAccentR, kAccentG, kAccentB,
                        0.35f);
  ui->UI_PushStyleColor(SPF_COLOR_BUTTON_ACTIVE, kAccentHoverR, kAccentHoverG,
                        kAccentHoverB, 0.55f);
  const bool clicked = ui->UI_Button(label, width, 0.0f);
  ui->UI_PopStyleColor(3);
  return clicked;
}

// "<icon> <text>", for the icon-prefixed labels used all over the window.
std::string WithIcon(const char *icon, std::string_view text) {
  return std::string(icon) + " " + std::string(text);
}

// A setting's title and description live under its own config key (see
// the UI Metadata comment in Manifest.cpp), shared with the native UI.
const char *SettingTitle(const char *key) {
  return loc::Tr(std::string(key) + ".title");
}

const char *SettingDesc(const char *key) {
  return loc::Tr(std::string(key) + ".desc");
}

// Toggle switch drawn as a bare icon button, e.g. for the value column of a
// settings table row, where the label already lives in its own column. The
// config key doubles as the button's ID so it stays unique.
bool DrawToggleCell(SPF_UI_API *ui, const char *key, bool *value,
                    const char *tooltip) {
  char button_id[128];
  std::snprintf(button_id, sizeof(button_id), "%s##%s",
                *value ? ICON_FA_TOGGLE_ON : ICON_FA_TOGGLE_OFF, key);

  ui->UI_PushStyleColor(SPF_COLOR_BUTTON, 0.0f, 0.0f, 0.0f, 0.0f);
  ui->UI_PushStyleColor(SPF_COLOR_BUTTON_HOVERED, 0.0f, 0.0f, 0.0f, 0.0f);
  ui->UI_PushStyleColor(SPF_COLOR_BUTTON_ACTIVE, 0.0f, 0.0f, 0.0f, 0.0f);
  if (*value)
    ui->UI_PushStyleColor(SPF_COLOR_TEXT, kAccentR, kAccentG, kAccentB, 1.0f);
  else
    ui->UI_PushStyleColor(SPF_COLOR_TEXT, 0.5f, 0.5f, 0.5f, 1.0f);

  const bool clicked = ui->UI_Button(button_id, 0.0f, 0.0f);
  ui->UI_PopStyleColor(4);
  if (clicked)
    *value = !*value;
  ui->UI_SetItemTooltip(tooltip);
  return clicked;
}

// An effect's "Enabled" toggle, with its label after the switch.
bool DrawEnabled(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h,
                 const char *key, bool default_value) {
  bool value = cfg->Cfg_GetBool(h, key, default_value);
  if (DrawToggleCell(ui, key, &value, loc::Tr("settings.enabled_desc")))
    cfg->Cfg_SetBool(h, key, value);
  ui->UI_SameLine(0.0f, 8.0f);
  ui->UI_Text(loc::Tr("ui.enabled"));
  return value;
}

// Collapsing header of the effect whose settings live under `prefix`
// ("settings.<group>.<effect>"). The "###" ID keeps its open/closed state
// across a language switch.
bool EffectHeader(SPF_UI_API *ui, const char *icon, const char *prefix) {
  const std::string label =
      WithIcon(icon, SettingTitle(prefix)) + "###" + prefix;
  return ui->UI_CollapsingHeader(label.c_str(), SPF_TREE_NODE_FLAG_NONE);
}

// Width of the settings tables' label column, set by DrawSettingsWindow
// every frame from LabelColumnWidth().
float g_label_column_w = 160.0f;

// The widest label any settings table can show in the active language, so
// none runs into its slider and every table's columns line up. Table rows
// are the profile-covered settings (bar the effects' "enabled" toggles,
// drawn above the table) and the keybind rows.
float LabelColumnWidth(SPF_UI_API *ui) {
  static const char *const kKeybindTitles[] = {
      "keybinds.look_left.title",
      "keybinds.look_right.title",
      "keybinds.zoom.title",
  };
  static const std::vector<const char *> kSettingKeys =
      profiles::AllSettingKeys();

  float widest = 0.0f;
  auto measure = [&](const char *text) {
    float w = 0.0f, h = 0.0f;
    ui->UI_CalcTextSize(text, &w, &h);
    widest = std::max(widest, w);
  };
  for (const char *key : kSettingKeys) {
    const std::string_view k(key);
    if (!k.ends_with(".enabled"))
      measure(SettingTitle(key));
  }
  for (const char *key : kKeybindTitles)
    measure(loc::Tr(key));
  return widest;
}

// Begins the label|value table shared by every effect's settings body.
// Only one column ("Value") gets a header-style stretch; the label
// column is as wide as the widest label (see LabelColumnWidth).
bool BeginSettingsTable(SPF_UI_API *ui, const char *id) {
  if (!ui->UI_BeginTable(id, 2, SPF_TABLE_FLAG_NONE, 0.0f, 0.0f, 0.0f))
    return false;
  ui->UI_TableSetupColumn("Label", SPF_TABLE_COLUMN_FLAG_WIDTH_FIXED,
                          g_label_column_w, 0);
  ui->UI_TableSetupColumn("Value", SPF_TABLE_COLUMN_FLAG_WIDTH_STRETCH, 0.0f,
                          0);
  return true;
}

void EndSettingsTable(SPF_UI_API *ui) { ui->UI_EndTable(); }

// Native SPF-style toast, matching how the framework itself communicates
void ShowToast(SPF_UI_API *ui, SPF_NotificationType type,
               const std::string &message) {
  SPF_Notification_Params params{};
  params.type = type;
  params.message = message.c_str();
  params.mode = SPF_NOTIF_MODE_STACK;
  params.duration = -1.0f;
  ui->UI_ShowNotification(&params);
}

// One label|toggle row.
void DrawBool(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h,
              const char *key, bool default_value) {
  bool value = cfg->Cfg_GetBool(h, key, default_value);
  ui->UI_TableNextRow(SPF_TABLE_ROW_FLAG_NONE, 0.0f);
  ui->UI_TableSetColumnIndex(0);
  ui->UI_Text(SettingTitle(key));
  ui->UI_TableSetColumnIndex(1);
  if (DrawToggleCell(ui, key, &value, SettingDesc(key)))
    cfg->Cfg_SetBool(h, key, value);
}

// One label|slider row. Right-click the slider to reset it to default.
// `display_scale` shows the value (and `min`/`max`) multiplied by it, e.g.
// to show a setting stored in km/h in mph; the stored value doesn't change.
void DrawFloat(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h,
               const char *key, float min, float max, const char *format,
               float default_value, float display_scale = 1.0f) {
  float value = static_cast<float>(cfg->Cfg_GetFloat(h, key, default_value)) *
                display_scale;
  ui->UI_TableNextRow(SPF_TABLE_ROW_FLAG_NONE, 0.0f);
  ui->UI_TableSetColumnIndex(0);
  ui->UI_Text(SettingTitle(key));
  ui->UI_TableSetColumnIndex(1);
  char hidden_label[112];
  std::snprintf(hidden_label, sizeof(hidden_label), "##%s", key);
  ui->UI_SetNextItemWidth(-1.0f);
  if (ui->UI_SliderFloat(hidden_label, &value, min * display_scale,
                         max * display_scale, format, SPF_SLIDER_FLAG_NONE))
    cfg->Cfg_SetFloat(h, key, value / display_scale);
  if (ui->UI_IsItemClicked(SPF_MOUSE_BUTTON_RIGHT))
    cfg->Cfg_SetFloat(h, key, default_value);
  ui->UI_SetItemTooltip(SettingDesc(key));
}

// A slider row for a speed setting, stored in km/h but shown in the
// active language's unit ("ui.units.speed_system": mph for "imperial",
// e.g. English, since British and American players drive in mph).
void DrawSpeed(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h,
               const char *key, float min_kmh, float max_kmh,
               float default_kmh) {
  constexpr float kMphPerKmh = 0.621371f;
  const bool imperial =
      std::string_view(loc::Tr("ui.units.speed_system")) == "imperial";
  // The unit comes from a translation, so escape any '%' before it goes
  // into a printf-style format.
  std::string format = "%.0f ";
  for (const char *c = loc::Tr(imperial ? "ui.units.mph" : "ui.units.kmh");
       *c; ++c)
    format += *c == '%' ? std::string("%%") : std::string(1, *c);
  DrawFloat(ui, cfg, h, key, min_kmh, max_kmh, format.c_str(), default_kmh,
            imperial ? kMphPerKmh : 1.0f);
}

// --- Per-effect defaults, reused by the tab-level and global reset
// buttons drawn in DrawSettingsWindow below. ---

void ResetHeadMotion(SPF_Config_API *cfg, SPF_Config_Handle *h) {
  cfg->Cfg_SetBool(h, "settings.driving.head_motion.enabled", true);
  cfg->Cfg_SetFloat(h, "settings.driving.head_motion.sway_strength",
                    defaults::kHeadMotionSwayStrength);
  cfg->Cfg_SetFloat(h, "settings.driving.head_motion.tilt_strength",
                    defaults::kHeadMotionTiltStrength);
  cfg->Cfg_SetFloat(h, "settings.driving.head_motion.smoothing_time",
                    defaults::kHeadMotionSmoothing);
}

void ResetSteeringCamera(SPF_Config_API *cfg, SPF_Config_Handle *h) {
  cfg->Cfg_SetBool(h, "settings.driving.steering_camera.enabled", true);
  cfg->Cfg_SetFloat(h, "settings.driving.steering_camera.rotation_factor_deg",
                    defaults::kSteeringCameraRotationAmount);
  cfg->Cfg_SetFloat(h, "settings.driving.steering_camera.smoothing_time",
                    defaults::kSteeringCameraSmoothing);
  cfg->Cfg_SetFloat(h, "settings.driving.steering_camera.delay_seconds",
                    defaults::kSteeringCameraReactionDelay);
}

void ResetIdleBreathing(SPF_Config_API *cfg, SPF_Config_Handle *h) {
  cfg->Cfg_SetBool(h, "settings.cabin.idle_breathing.enabled", true);
  cfg->Cfg_SetFloat(h, "settings.cabin.idle_breathing.vertical_amplitude",
                    defaults::kIdleBreathingVerticalAmount);
  cfg->Cfg_SetFloat(h, "settings.cabin.idle_breathing.pitch_amplitude_deg",
                    defaults::kIdleBreathingHeadNodAmount);
  cfg->Cfg_SetFloat(h, "settings.cabin.idle_breathing.breathing_rate_bpm",
                    defaults::kIdleBreathingRate);
  cfg->Cfg_SetFloat(h, "settings.cabin.idle_breathing.fade_start_kmh",
                    defaults::kIdleBreathingFadeStart);
  cfg->Cfg_SetFloat(h, "settings.cabin.idle_breathing.fade_end_kmh",
                    defaults::kIdleBreathingFadeEnd);
}

void ResetSuspension(SPF_Config_API *cfg, SPF_Config_Handle *h) {
  cfg->Cfg_SetBool(h, "settings.road.suspension.enabled", true);
  cfg->Cfg_SetFloat(h, "settings.road.suspension.vertical_strength",
                    defaults::kSuspensionVerticalStrength);
  cfg->Cfg_SetFloat(h, "settings.road.suspension.reactivity",
                    defaults::kSuspensionReactivity);
  cfg->Cfg_SetFloat(h, "settings.road.suspension.grade_strength",
                    defaults::kSuspensionGradeStrength);
}

void ResetRoadIrregularity(SPF_Config_API *cfg, SPF_Config_Handle *h) {
  cfg->Cfg_SetBool(h, "settings.road.road_irregularity.enabled", true);
  cfg->Cfg_SetFloat(h, "settings.road.road_irregularity.intensity",
                    defaults::kRoadIrregularityIntensity);
  cfg->Cfg_SetFloat(h, "settings.road.road_irregularity.reactivity",
                    defaults::kRoadIrregularityReactivity);
}

void ResetSpeedShake(SPF_Config_API *cfg, SPF_Config_Handle *h) {
  cfg->Cfg_SetBool(h, "settings.road.speed_shake.enabled", true);
  cfg->Cfg_SetFloat(h, "settings.road.speed_shake.intensity",
                    defaults::kSpeedShakeIntensity);
  cfg->Cfg_SetFloat(h, "settings.road.speed_shake.smoothing_time",
                    defaults::kSpeedShakeSmoothing);
  cfg->Cfg_SetFloat(h, "settings.road.speed_shake.rotation",
                    defaults::kSpeedShakeRotation);
  cfg->Cfg_SetFloat(h, "settings.road.speed_shake.vertical",
                    defaults::kSpeedShakeVertical);
  cfg->Cfg_SetFloat(h, "settings.road.speed_shake.roughness",
                    defaults::kSpeedShakeRoughness);
}

void ResetBodyDynamics(SPF_Config_API *cfg, SPF_Config_Handle *h) {
  cfg->Cfg_SetBool(h, "settings.driving.body_dynamics.enabled", true);
  cfg->Cfg_SetFloat(h, "settings.driving.body_dynamics.lean_strength",
                    defaults::kBodyDynamicsLeanStrength);
  cfg->Cfg_SetFloat(h, "settings.driving.body_dynamics.nod_strength",
                    defaults::kBodyDynamicsNodStrength);
  cfg->Cfg_SetFloat(h, "settings.driving.body_dynamics.smoothing_time",
                    defaults::kBodyDynamicsSmoothing);
}

void ResetEngineVibration(SPF_Config_API *cfg, SPF_Config_Handle *h) {
  cfg->Cfg_SetBool(h, "settings.cabin.engine_vibration.enabled", true);
  cfg->Cfg_SetFloat(h, "settings.cabin.engine_vibration.intensity",
                    defaults::kEngineVibrationIntensity);
}

void ResetEngineStartStop(SPF_Config_API *cfg, SPF_Config_Handle *h) {
  cfg->Cfg_SetBool(h, "settings.cabin.engine_start_stop.enabled", true);
  cfg->Cfg_SetFloat(h, "settings.cabin.engine_start_stop.intensity",
                    defaults::kEngineStartStopIntensity);
  cfg->Cfg_SetFloat(h, "settings.cabin.engine_start_stop.duration",
                    defaults::kEngineStartStopDuration);
}

void ResetMirrorCheck(SPF_Config_API *cfg, SPF_Config_Handle *h) {
  cfg->Cfg_SetBool(h, "settings.manual.mirror_check.enabled", true);
  cfg->Cfg_SetFloat(h, "settings.manual.mirror_check.look_angle_deg",
                    defaults::kMirrorCheckLookAngle);
  cfg->Cfg_SetFloat(h, "settings.manual.mirror_check.pitch_offset_deg",
                    defaults::kMirrorCheckPitchOffset);
  cfg->Cfg_SetFloat(h, "settings.manual.mirror_check.smoothing_time",
                    defaults::kMirrorCheckSmoothing);
  cfg->Cfg_SetBool(h, "settings.manual.mirror_check.require_stationary",
                   defaults::kMirrorCheckRequireStationary);
  cfg->Cfg_SetBool(h, "settings.manual.mirror_check.ignore_after_moving_signal",
                   defaults::kMirrorCheckIgnoreAfterMovingSignal);
}

void ResetManualLook(SPF_Config_API *cfg, SPF_Config_Handle *h) {
  cfg->Cfg_SetBool(h, "settings.manual.manual_look.enabled", true);
  cfg->Cfg_SetFloat(h, "settings.manual.manual_look.look_angle_deg",
                    defaults::kManualLookLookAngle);
  cfg->Cfg_SetFloat(h, "settings.manual.manual_look.smoothing_time",
                    defaults::kManualLookSmoothing);
  cfg->Cfg_SetBool(h, "settings.manual.manual_look.toggle_mode",
                   defaults::kManualLookToggleMode);
}

void ResetManualZoom(SPF_Config_API *cfg, SPF_Config_Handle *h) {
  cfg->Cfg_SetBool(h, "settings.manual.manual_zoom.enabled", true);
  cfg->Cfg_SetFloat(h, "settings.manual.manual_zoom.zoom_fov_deg",
                    defaults::kManualZoomZoomLevel);
  cfg->Cfg_SetFloat(h, "settings.manual.manual_zoom.smoothing_time",
                    defaults::kManualZoomSmoothing);
}

// --- Per-effect draw functions ---

void DrawHeadMotion(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h) {
  if (!EffectHeader(ui, ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT,
                    "settings.driving.head_motion"))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.driving.head_motion.enabled", true);
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "head_motion_table")) {
    DrawFloat(ui, cfg, h, "settings.driving.head_motion.sway_strength", 0.0f,
              0.6f, "%.2f", defaults::kHeadMotionSwayStrength);
    DrawFloat(ui, cfg, h, "settings.driving.head_motion.tilt_strength", 0.0f,
              0.6f, "%.2f", defaults::kHeadMotionTiltStrength);
    DrawFloat(ui, cfg, h, "settings.driving.head_motion.smoothing_time", 0.02f,
              0.5f, "%.2f s", defaults::kHeadMotionSmoothing);
    EndSettingsTable(ui);
  }
  ui->UI_EndDisabled();
}

void DrawSteeringCamera(SPF_UI_API *ui, SPF_Config_API *cfg,
                        SPF_Config_Handle *h) {
  if (!EffectHeader(ui, ICON_FA_ROTATE, "settings.driving.steering_camera"))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.driving.steering_camera.enabled", true);
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "steering_camera_table")) {
    DrawFloat(ui, cfg, h,
              "settings.driving.steering_camera.rotation_factor_deg", 20.0f,
              60.0f, "%.1f deg", defaults::kSteeringCameraRotationAmount);
    DrawFloat(ui, cfg, h, "settings.driving.steering_camera.smoothing_time",
              0.02f, 1.0f, "%.2f s", defaults::kSteeringCameraSmoothing);
    DrawFloat(ui, cfg, h, "settings.driving.steering_camera.delay_seconds",
              0.0f, 0.3f, "%.2f s", defaults::kSteeringCameraReactionDelay);
    EndSettingsTable(ui);
  }
  ui->UI_EndDisabled();
}

void DrawIdleBreathing(SPF_UI_API *ui, SPF_Config_API *cfg,
                       SPF_Config_Handle *h) {
  if (!EffectHeader(ui, ICON_FA_LUNGS, "settings.cabin.idle_breathing"))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.cabin.idle_breathing.enabled", true);
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "idle_breathing_table")) {
    DrawFloat(ui, cfg, h, "settings.cabin.idle_breathing.vertical_amplitude",
              0.0f, 0.01f, "%.3f m", defaults::kIdleBreathingVerticalAmount);
    DrawFloat(ui, cfg, h, "settings.cabin.idle_breathing.pitch_amplitude_deg",
              0.0f, 1.5f, "%.2f deg", defaults::kIdleBreathingHeadNodAmount);
    DrawFloat(ui, cfg, h, "settings.cabin.idle_breathing.breathing_rate_bpm",
              10.0f, 20.0f, "%.0f bpm", defaults::kIdleBreathingRate);
    DrawSpeed(ui, cfg, h, "settings.cabin.idle_breathing.fade_start_kmh", 0.0f,
              100.0f, defaults::kIdleBreathingFadeStart);
    DrawSpeed(ui, cfg, h, "settings.cabin.idle_breathing.fade_end_kmh", 0.0f,
              100.0f, defaults::kIdleBreathingFadeEnd);
    EndSettingsTable(ui);
  }
  ui->UI_EndDisabled();
}

void DrawSuspension(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h) {
  if (!EffectHeader(ui, ICON_FA_ARROWS_UP_DOWN, "settings.road.suspension"))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.road.suspension.enabled", true);
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "suspension_table")) {
    DrawFloat(ui, cfg, h, "settings.road.suspension.vertical_strength", 0.0f,
              2.0f, "%.2f", defaults::kSuspensionVerticalStrength);
    DrawFloat(ui, cfg, h, "settings.road.suspension.reactivity", 0.02f, 0.3f,
              "%.2f s", defaults::kSuspensionReactivity);
    DrawFloat(ui, cfg, h, "settings.road.suspension.grade_strength", 0.0f, 1.0f,
              "%.2f", defaults::kSuspensionGradeStrength);
    EndSettingsTable(ui);
  }
  ui->UI_EndDisabled();
}

void DrawSpeedShake(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h) {
  if (!EffectHeader(ui, ICON_FA_TRUCK, "settings.road.speed_shake"))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.road.speed_shake.enabled", true);
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "speed_shake_table")) {
    DrawFloat(ui, cfg, h, "settings.road.speed_shake.intensity", 0.0f, 2.0f,
              "%.2f", defaults::kSpeedShakeIntensity);
    DrawFloat(ui, cfg, h, "settings.road.speed_shake.smoothing_time", 0.1f,
              1.0f, "%.2f s", defaults::kSpeedShakeSmoothing);
    DrawFloat(ui, cfg, h, "settings.road.speed_shake.rotation", 0.0f, 2.0f,
              "%.2f", defaults::kSpeedShakeRotation);
    DrawFloat(ui, cfg, h, "settings.road.speed_shake.vertical", 0.0f, 2.0f,
              "%.2f", defaults::kSpeedShakeVertical);
    DrawFloat(ui, cfg, h, "settings.road.speed_shake.roughness", 0.0f, 1.0f,
              "%.2f", defaults::kSpeedShakeRoughness);
    EndSettingsTable(ui);
  }
  ui->UI_EndDisabled();
}

void DrawBodyDynamics(SPF_UI_API *ui, SPF_Config_API *cfg,
                      SPF_Config_Handle *h) {
  if (!EffectHeader(ui, ICON_FA_PERSON, "settings.driving.body_dynamics"))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.driving.body_dynamics.enabled", true);
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "body_dynamics_table")) {
    DrawFloat(ui, cfg, h, "settings.driving.body_dynamics.lean_strength", 0.0f,
              2.0f, "%.2f", defaults::kBodyDynamicsLeanStrength);
    DrawFloat(ui, cfg, h, "settings.driving.body_dynamics.nod_strength", 0.0f,
              2.0f, "%.2f", defaults::kBodyDynamicsNodStrength);
    DrawFloat(ui, cfg, h, "settings.driving.body_dynamics.smoothing_time",
              0.05f, 0.6f, "%.2f s", defaults::kBodyDynamicsSmoothing);
    EndSettingsTable(ui);
  }
  ui->UI_EndDisabled();
}

void DrawRoadIrregularity(SPF_UI_API *ui, SPF_Config_API *cfg,
                          SPF_Config_Handle *h) {
  if (!EffectHeader(ui, ICON_FA_ROAD, "settings.road.road_irregularity"))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.road.road_irregularity.enabled", true);
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "road_irregularity_table")) {
    DrawFloat(ui, cfg, h, "settings.road.road_irregularity.intensity", 0.0f,
              2.0f, "%.2f", defaults::kRoadIrregularityIntensity);
    DrawFloat(ui, cfg, h, "settings.road.road_irregularity.reactivity", 0.02f,
              0.2f, "%.2f s", defaults::kRoadIrregularityReactivity);
    EndSettingsTable(ui);
  }
  ui->UI_EndDisabled();
}

void DrawEngineVibration(SPF_UI_API *ui, SPF_Config_API *cfg,
                         SPF_Config_Handle *h) {
  if (!EffectHeader(ui, ICON_FA_WAVE_SQUARE, "settings.cabin.engine_vibration"))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.cabin.engine_vibration.enabled", true);
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "engine_vibration_table")) {
    DrawFloat(ui, cfg, h, "settings.cabin.engine_vibration.intensity", 0.0f,
              2.0f, "%.2f", defaults::kEngineVibrationIntensity);
    EndSettingsTable(ui);
  }
  ui->UI_EndDisabled();
}

void DrawEngineStartStop(SPF_UI_API *ui, SPF_Config_API *cfg,
                         SPF_Config_Handle *h) {
  if (!EffectHeader(ui, ICON_FA_POWER_OFF, "settings.cabin.engine_start_stop"))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.cabin.engine_start_stop.enabled", true);
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "engine_start_stop_table")) {
    DrawFloat(ui, cfg, h, "settings.cabin.engine_start_stop.intensity", 0.0f,
              0.3f, "%.2f", defaults::kEngineStartStopIntensity);
    DrawFloat(ui, cfg, h, "settings.cabin.engine_start_stop.duration", 0.3f,
              2.0f, "%.2f s", defaults::kEngineStartStopDuration);
    EndSettingsTable(ui);
  }
  ui->UI_EndDisabled();
}

// Small muted, word-wrapped note with an info icon. Wraps at the window
// edge instead of running off it, unlike a plain UI_Text/UI_TextDisabled
// call.
void DrawWrappedHint(SPF_UI_API *ui, const char *text) {
  ui->UI_PushStyleColor(SPF_COLOR_TEXT, 0.6f, 0.6f, 0.6f, 1.0f);
  ui->UI_TextWrapped(WithIcon(ICON_FA_CIRCLE_INFO, text).c_str());
  ui->UI_PopStyleColor(1);
}

// The bindings of one keybind action: each existing binding is a button
// (click to rebind) with a gear next to it (advanced options), plus a "+" to
// add another. The rebind/details popups are drawn by SPF itself, so the
// result is identical to, and stays in sync with, the native Settings
// window.
void DrawKeybindButtons(SPF_UI_API *ui, const char *action) {
  PluginContext &ctx = Context();
  SPF_KeyBinds_API *kb = ctx.core ? ctx.core->keybinds : nullptr;

  // Same muted red as the sliders/fields (FRAME_BG values), instead of the
  // brighter brand red used by regular buttons.
  ui->UI_PushStyleColor(SPF_COLOR_BUTTON, kAccentR, kAccentG, kAccentB, 0.25f);
  ui->UI_PushStyleColor(SPF_COLOR_BUTTON_HOVERED, kAccentR, kAccentG, kAccentB,
                        0.35f);
  ui->UI_PushStyleColor(SPF_COLOR_BUTTON_ACTIVE, kAccentHoverR, kAccentHoverG,
                        kAccentHoverB, 0.55f);
  ui->UI_PushID_Str(action);
  const int count = kb->Kbind_GetBindingCount(ctx.keybinds_handle, action);
  for (int i = 0; i < count; ++i) {
    ui->UI_PushID_Int(i);
    char name[128] = {};
    kb->Kbind_GetBindingDisplayName(ctx.keybinds_handle, action, i, name,
                                    sizeof(name));
    if (ui->UI_Button(name[0] ? name : loc::Tr("ui.keybind.unbound"), 0.0f,
                      0.0f))
      kb->Kbind_OpenRebindPopup(ctx.keybinds_handle, action, i);
    ui->UI_SetItemTooltip(loc::Tr("ui.keybind.rebind_tip"));
    ui->UI_SameLine(0.0f, 4.0f);
    if (ui->UI_Button(ICON_FA_GEAR, 0.0f, 0.0f))
      kb->Kbind_OpenBindingDetailsPopup(ctx.keybinds_handle, action, i);
    ui->UI_SetItemTooltip(loc::Tr("ui.keybind.options_tip"));
    ui->UI_SameLine(0.0f, 12.0f);
    ui->UI_PopID();
  }
  if (ui->UI_Button(ICON_FA_PLUS "##add", 0.0f, 0.0f))
    kb->Kbind_OpenRebindPopup(ctx.keybinds_handle, action, -1);
  ui->UI_SetItemTooltip(loc::Tr("ui.keybind.add_tip"));
  ui->UI_PopID();
  ui->UI_PopStyleColor(3);
}

// True when the framework is recent enough to draw rebind/details popups.
// Falls back to nothing on an older one, where the hint text still points to
// SPF's Keybinds menu.
bool KeybindUiAvailable() {
  PluginContext &ctx = Context();
  SPF_KeyBinds_API *kb = ctx.core ? ctx.core->keybinds : nullptr;
  return kb && ctx.keybinds_handle && kb->Kbind_OpenRebindPopup &&
         kb->Kbind_OpenBindingDetailsPopup && kb->Kbind_GetBindingDisplayName;
}

// One label|bindings row for a keybind action, inside a settings table.
void DrawKeybindRow(SPF_UI_API *ui, const char *action, const char *label) {
  if (!KeybindUiAvailable())
    return;

  ui->UI_TableNextRow(SPF_TABLE_ROW_FLAG_NONE, 0.0f);
  ui->UI_TableSetColumnIndex(0);
  ui->UI_Text(label);
  ui->UI_TableSetColumnIndex(1);
  DrawKeybindButtons(ui, action);
}

// Same label|bindings row outside a table, with the buttons placed at
// `button_x` from the row's left edge instead of the tables' 160 px label
// column, so they line up with other buttons of the Settings tab.
void DrawKeybindRowAt(SPF_UI_API *ui, const char *action, const char *label,
                      float button_x) {
  if (!KeybindUiAvailable())
    return;

  const float start_x = ui->UI_GetCursorPosX();
  ui->UI_AlignTextToFramePadding();
  ui->UI_Text(label);
  ui->UI_SameLine(start_x + button_x, 0.0f);
  DrawKeybindButtons(ui, action);
}

void DrawMirrorCheck(SPF_UI_API *ui, SPF_Config_API *cfg,
                     SPF_Config_Handle *h) {
  if (!EffectHeader(ui, ICON_FA_EYE, "settings.manual.mirror_check"))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.manual.mirror_check.enabled", true);
  DrawWrappedHint(ui, loc::Tr("settings.manual.mirror_check.hint"));
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "mirror_check_table")) {
    DrawFloat(ui, cfg, h, "settings.manual.mirror_check.look_angle_deg", 15.0f,
              50.0f, "%.0f deg", defaults::kMirrorCheckLookAngle);
    DrawFloat(ui, cfg, h, "settings.manual.mirror_check.pitch_offset_deg", 0.0f,
              10.0f, "%.0f deg", defaults::kMirrorCheckPitchOffset);
    DrawFloat(ui, cfg, h, "settings.manual.mirror_check.smoothing_time", 0.0f,
              0.7f, "%.2f s", defaults::kMirrorCheckSmoothing);
    DrawBool(ui, cfg, h, "settings.manual.mirror_check.require_stationary",
             defaults::kMirrorCheckRequireStationary);
    DrawBool(ui, cfg, h,
             "settings.manual.mirror_check.ignore_after_moving_signal",
             defaults::kMirrorCheckIgnoreAfterMovingSignal);
    EndSettingsTable(ui);
  }
  ui->UI_EndDisabled();
}

void DrawManualLook(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h) {
  if (!EffectHeader(ui, ICON_FA_ARROWS_LEFT_RIGHT,
                    "settings.manual.manual_look"))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.manual.manual_look.enabled", true);
  DrawWrappedHint(ui, loc::Tr("settings.manual.manual_look.hint"));
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "manual_look_table")) {
    DrawKeybindRow(ui, "ManualLook.look_left",
                   loc::Tr("keybinds.look_left.title"));
    DrawKeybindRow(ui, "ManualLook.look_right",
                   loc::Tr("keybinds.look_right.title"));
    DrawFloat(ui, cfg, h, "settings.manual.manual_look.look_angle_deg", 20.0f,
              90.0f, "%.0f deg", defaults::kManualLookLookAngle);
    DrawFloat(ui, cfg, h, "settings.manual.manual_look.smoothing_time", 0.0f,
              0.7f, "%.2f s", defaults::kManualLookSmoothing);
    DrawBool(ui, cfg, h, "settings.manual.manual_look.toggle_mode",
             defaults::kManualLookToggleMode);
    EndSettingsTable(ui);
  }
  ui->UI_EndDisabled();
}

void DrawManualZoom(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h) {
  if (!EffectHeader(ui, ICON_FA_MAGNIFYING_GLASS_PLUS,
                    "settings.manual.manual_zoom"))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.manual.manual_zoom.enabled", true);
  DrawWrappedHint(ui, loc::Tr("settings.manual.manual_zoom.hint"));
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "manual_zoom_table")) {
    DrawKeybindRow(ui, "ManualZoom.zoom", loc::Tr("keybinds.zoom.title"));
    DrawFloat(ui, cfg, h, "settings.manual.manual_zoom.zoom_fov_deg", 5.0f,
              60.0f, "%.0f deg", defaults::kManualZoomZoomLevel);
    DrawFloat(ui, cfg, h, "settings.manual.manual_zoom.smoothing_time", 0.02f,
              0.5f, "%.2f s", defaults::kManualZoomSmoothing);
    EndSettingsTable(ui);
  }
  ui->UI_EndDisabled();
}

// Section title followed by a thin rule running to the window's right edge
// ("Profiles ─────────"), lighter than UI_SeparatorText's full-width bar.
void DrawSectionTitle(SPF_UI_API *ui, const char *title) {
  float start_x = 0.0f, start_y = 0.0f, avail_x = 0.0f, avail_y = 0.0f;
  ui->UI_GetCursorScreenPos(&start_x, &start_y);
  ui->UI_GetContentRegionAvail(&avail_x, &avail_y);

  ui->UI_TextDisabled(title);

  float min_x, min_y, max_x, max_y;
  ui->UI_GetItemRectMin(&min_x, &min_y);
  ui->UI_GetItemRectMax(&max_x, &max_y);
  constexpr uint32_t kRuleColor =
      (110u << 24) | (140u << 16) | (140u << 8) | 140u;
  const float y = (min_y + max_y) * 0.5f;
  ui->UI_DrawList_AddLine(ui->UI_GetWindowDrawList(), max_x + 8.0f, y,
                          start_x + avail_x, y, kRuleColor, 1.0f);
}

// "###" IDs of the confirmation popups: their visible title is translated,
// and OpenPopup/BeginPopupModal must still agree on the same ID.
constexpr const char *kOverwritePopupId = "###confirm_overwrite_profile";
constexpr const char *kDeletePopupId = "###confirm_delete_profile";
constexpr const char *kResetPopupId = "###confirm_reset_all";

// `field_w`: width of the name field and the profile dropdown, which the
// buttons next to them follow.
void DrawProfilesSection(SPF_UI_API *ui, float field_w) {
  PluginContext &ctx = Context();

  DrawSectionTitle(ui, loc::Tr("ui.profiles.section"));

  static char profile_name_buf[64] = "";
  static std::string pending_select;
  static std::string pending_overwrite;

  auto do_save = [&](const std::string &saved_name) {
    if (profiles::Save(ctx, saved_name)) {
      profile_name_buf[0] = '\0';
      pending_select = saved_name;
      ShowToast(ui, SPF_NOTIFICATION_SUCCESS,
                loc::Tr("ui.profiles.saved_toast", {{"name", saved_name}}));
      return true;
    }
    ShowToast(ui, SPF_NOTIFICATION_ERROR, loc::Tr("ui.profiles.save_failed"));
    return false;
  };

  ui->UI_SetNextItemWidth(field_w);
  ui->UI_InputTextWithHint("##profile_name", loc::Tr("ui.profiles.name_hint"),
                           profile_name_buf, sizeof(profile_name_buf),
                           SPF_INPUT_TEXT_FLAG_NONE);
  ui->UI_SameLine(0.0f, 8.0f);
  const std::string save_label =
      WithIcon(ICON_FA_FLOPPY_DISK, loc::Tr("ui.profiles.save_as")) +
      "###save_as";
  if (MutedButton(ui, save_label.c_str())) {
    const std::string sanitized = profiles::Sanitize(profile_name_buf);
    if (sanitized.empty()) {
      ShowToast(ui, SPF_NOTIFICATION_WARNING,
                loc::Tr("ui.profiles.invalid_name"));
    } else {
      const std::vector<std::string> existing = profiles::List(ctx);
      // Case-insensitive: on Windows "default" is the same file as
      // "Default". Reuse the stored spelling so the active profile name
      // keeps matching the dropdown entry.
      const std::string *match = profiles::FindIgnoreCase(existing, sanitized);
      if (match) {
        pending_overwrite = *match;
        ui->UI_OpenPopup(kOverwritePopupId, SPF_POPUP_FLAG_NONE);
      } else {
        do_save(sanitized);
      }
    }
  }
  ui->UI_SetItemTooltip(loc::Tr("ui.profiles.save_as_tip"));

  const std::string overwrite_title =
      loc::Tr("ui.profiles.overwrite_title") + std::string(kOverwritePopupId);
  if (ui->UI_BeginPopupModal(overwrite_title.c_str(), nullptr,
                             SPF_WINDOW_FLAG_NONE)) {
    ui->UI_TextUnformatted(
        loc::Tr("ui.profiles.overwrite_message", {{"name", pending_overwrite}})
            .c_str());
    ui->UI_Spacing();
    if (MutedButton(ui, loc::Tr("ui.profiles.overwrite_confirm"))) {
      do_save(pending_overwrite);
      ui->UI_CloseCurrentPopup();
    }
    ui->UI_SameLine(0.0f, 8.0f);
    if (MutedButton(ui, loc::Tr("ui.cancel")))
      ui->UI_CloseCurrentPopup();
    ui->UI_EndPopup();
  }

  ui->UI_Spacing();
  const std::vector<std::string> profile_names = profiles::List(ctx);
  if (profile_names.empty()) {
    ui->UI_TextDisabled(loc::Tr("ui.profiles.none"));
  } else {
    // -1 means "no profile matches the current live settings", e.g. a
    // manual edit was made since the last Load()/Save(). Shown as its own
    // placeholder rather than defaulting to index 0, which would falsely
    // claim whatever profile sorts first (often "Default") is active.
    static int selected_profile = -2; // -2: not yet synced this session
    static bool selection_initialized = false;
    if (!selection_initialized) {
      selection_initialized = true;
      const std::string last_used = profiles::LastUsedName(ctx);
      const auto it =
          std::find(profile_names.begin(), profile_names.end(), last_used);
      selected_profile = (!last_used.empty() && it != profile_names.end())
                             ? static_cast<int>(it - profile_names.begin())
                             : -1;
    }
    if (!pending_select.empty()) {
      // Only updates which entry the dropdown shows as selected, no
      // Load() here. Saving already copied the current live settings into
      // this profile's file, so reloading it right back would just be a
      // redundant round-trip to the same values.
      const auto it =
          std::find(profile_names.begin(), profile_names.end(), pending_select);
      selected_profile = it != profile_names.end()
                             ? static_cast<int>(it - profile_names.begin())
                             : -1;
      pending_select.clear();
    }
    if (selected_profile >= static_cast<int>(profile_names.size()))
      selected_profile = static_cast<int>(profile_names.size()) - 1;

    const char *preview = selected_profile >= 0
                              ? profile_names[selected_profile].c_str()
                              : loc::Tr("ui.unsaved_changes");
    ui->UI_SetNextItemWidth(field_w);
    ui->UI_PushStyleVarFloat(SPF_STYLE_VAR_POPUP_BORDERSIZE, 1.0f);
    const bool combo_open =
        ui->UI_BeginCombo("##profile_select", preview, SPF_COMBO_FLAG_NONE);
    if (combo_open) {
      for (size_t i = 0; i < profile_names.size(); ++i) {
        const bool is_selected = static_cast<int>(i) == selected_profile;
        if (ui->UI_Selectable(profile_names[i].c_str(), is_selected,
                              SPF_SELECTABLE_FLAG_NONE, 0.0f, 0.0f)) {
          if (profiles::Load(ctx, profile_names[i])) {
            selected_profile = static_cast<int>(i);
          } else {
            ShowToast(ui, SPF_NOTIFICATION_ERROR,
                      loc::Tr("ui.profiles.load_failed",
                              {{"name", profile_names[i]}}));
          }
        }
      }
      ui->UI_EndCombo();
    }
    ui->UI_PopStyleVar(1);
    ui->UI_SetItemTooltip(loc::Tr("ui.profiles.select_tip"));

    if (selected_profile < 0) {
      ui->UI_TextDisabled(loc::Tr("ui.profiles.no_match"));
      return;
    }

    const std::string &selected_name = profile_names[selected_profile];

    ui->UI_SameLine(0.0f, 8.0f);
    static std::chrono::steady_clock::time_point save_confirm_until{};
    const bool save_confirmed =
        std::chrono::steady_clock::now() < save_confirm_until;
    const std::string update_label =
        (save_confirmed
             ? WithIcon(ICON_FA_CHECK, loc::Tr("ui.profiles.saved"))
             : WithIcon(ICON_FA_FLOPPY_DISK, loc::Tr("ui.profiles.save"))) +
        "###save";
    if (MutedButton(ui, update_label.c_str()) && do_save(selected_name))
      save_confirm_until =
          std::chrono::steady_clock::now() + std::chrono::seconds(2);
    ui->UI_SetItemTooltip(loc::Tr("ui.profiles.save_tip"));

    ui->UI_SameLine(0.0f, 4.0f);
    const std::string delete_label =
        WithIcon(ICON_FA_TRASH, loc::Tr("ui.profiles.delete")) + "###delete";
    static std::string pending_delete;
    if (MutedButton(ui, delete_label.c_str())) {
      pending_delete = selected_name;
      ui->UI_OpenPopup(kDeletePopupId, SPF_POPUP_FLAG_NONE);
    }
    ui->UI_SetItemTooltip(loc::Tr("ui.profiles.delete_tip"));

    const std::string delete_title =
        loc::Tr("ui.profiles.delete_title") + std::string(kDeletePopupId);
    if (ui->UI_BeginPopupModal(delete_title.c_str(), nullptr,
                               SPF_WINDOW_FLAG_NONE)) {
      ui->UI_TextUnformatted(
          loc::Tr("ui.profiles.delete_message", {{"name", pending_delete}})
              .c_str());
      ui->UI_Spacing();
      if (MutedButton(ui, loc::Tr("ui.profiles.delete_confirm"))) {
        if (!profiles::Delete(ctx, pending_delete)) {
          ShowToast(ui, SPF_NOTIFICATION_ERROR,
                    loc::Tr("ui.profiles.delete_failed",
                            {{"name", pending_delete}}));
        } else {
          ShowToast(ui, SPF_NOTIFICATION_SUCCESS,
                    loc::Tr("ui.profiles.deleted_toast",
                            {{"name", pending_delete}}));
          // Deleting the active profile shouldn't leave its values loaded,
          // so fall back to "Default", recreating it first if it's what
          // just got deleted.
          profiles::EnsureDefaultExists(ctx);
          if (profiles::Load(ctx, profiles::kDefaultProfileName)) {
            const std::vector<std::string> refreshed = profiles::List(ctx);
            const auto it = std::find(refreshed.begin(), refreshed.end(),
                                      profiles::kDefaultProfileName);
            selected_profile = it != refreshed.end()
                                   ? static_cast<int>(it - refreshed.begin())
                                   : -1;
          } else {
            ShowToast(ui, SPF_NOTIFICATION_ERROR,
                      loc::Tr("ui.profiles.fallback_failed",
                              {{"name", profiles::kDefaultProfileName}}));
          }
        }
        ui->UI_CloseCurrentPopup();
      }
      ui->UI_SameLine(0.0f, 8.0f);
      if (MutedButton(ui, loc::Tr("ui.cancel")))
        ui->UI_CloseCurrentPopup();
      ui->UI_EndPopup();
    }
  }
}

// Global reset, last in the tab and away from the effect tabs so it isn't
// clicked by accident.
void DrawResetSection(SPF_UI_API *ui, SPF_Config_API *cfg,
                      SPF_Config_Handle *h) {
  PluginContext &ctx = Context();

  DrawSectionTitle(ui, loc::Tr("ui.reset.section"));
  const std::string reset_label =
      WithIcon(ICON_FA_ARROW_ROTATE_LEFT, loc::Tr("ui.reset.button")) +
      "###reset_all";
  if (MutedButton(ui, reset_label.c_str()))
    ui->UI_OpenPopup(kResetPopupId, SPF_POPUP_FLAG_NONE);

  const std::string title =
      loc::Tr("ui.reset.title") + std::string(kResetPopupId);
  if (ui->UI_BeginPopupModal(title.c_str(), nullptr, SPF_WINDOW_FLAG_NONE)) {
    const std::string active_profile = profiles::LastUsedName(ctx);
    ui->UI_TextUnformatted(loc::Tr("ui.reset.message"));
    ui->UI_Spacing();
    if (MutedButton(ui, loc::Tr("ui.reset.confirm"))) {
      ResetHeadMotion(cfg, h);
      ResetSteeringCamera(cfg, h);
      ResetIdleBreathing(cfg, h);
      ResetSuspension(cfg, h);
      ResetRoadIrregularity(cfg, h);
      ResetSpeedShake(cfg, h);
      ResetBodyDynamics(cfg, h);
      ResetEngineVibration(cfg, h);
      ResetEngineStartStop(cfg, h);
      ResetMirrorCheck(cfg, h);
      ResetManualLook(cfg, h);
      ResetManualZoom(cfg, h);
      if (!active_profile.empty())
        profiles::Save(ctx, active_profile);
      ShowToast(ui, SPF_NOTIFICATION_SUCCESS, loc::Tr("ui.reset.done_toast"));
      ui->UI_CloseCurrentPopup();
    }
    ui->UI_SameLine(0.0f, 8.0f);
    if (MutedButton(ui, loc::Tr("ui.cancel")))
      ui->UI_CloseCurrentPopup();
    ui->UI_EndPopup();
  }
}

// --- About tab ---

// Button in the UI's brand red (the README badges' #B82728) that opens `url`
// in the browser; the description and the address are shown in its tooltip.
// `width` 0 sizes it to its label.
void LinkButton(SPF_UI_API *ui, const std::string &label, float width,
                const char *url, const char *description) {
  const bool clicked = MutedButton(ui, label.c_str(), width);

  char tooltip[256];
  std::snprintf(tooltip, sizeof(tooltip), "%s\n%s", description, url);
  ui->UI_SetItemTooltip(tooltip);
  if (clicked)
    OpenUrl(url);
}

// Logo shown in the About tab, loaded once from <plugin data dir>/logo.png.
// `id` stays null if the file is missing or can't be decoded, in which case
// the tab falls back to plain text for the title.
struct LogoTexture {
  void *id = nullptr;
  int width = 0;
  int height = 0;
};

// Namespace-scope (not function-local) so DestroyLogoTexture below can also
// reach it, to free the texture on plugin unload.
LogoTexture g_logo_texture;
bool g_logo_texture_tried = false;

const LogoTexture &GetLogoTexture(SPF_UI_API *ui) {
  if (g_logo_texture_tried)
    return g_logo_texture;
  g_logo_texture_tried = true;

  PluginContext &ctx = Context();
  if (!ctx.core || !ctx.core->environment || !ctx.environment_handle ||
      !ui->UI_CreateTextureFromFile)
    return g_logo_texture;

  char dir[512];
  const int len = ctx.core->environment->Env_GetPluginDataDir(
      ctx.environment_handle, dir, sizeof(dir));
  if (len <= 0 || len >= static_cast<int>(sizeof(dir)))
    return g_logo_texture;

  const std::string path =
      std::string(dir, static_cast<size_t>(len)) + "/logo.png";
  g_logo_texture.id = ui->UI_CreateTextureFromFile(
      path.c_str(), &g_logo_texture.width, &g_logo_texture.height);
  return g_logo_texture;
}

// SPF only frees a texture created via UI_CreateTextureFromFile/FromMemory
// when explicitly told to; it doesn't track plugin lifetime, so this must
// be called from OnUnload() or the texture leaks for the rest of the game
// session. Resets the cache too, so a plugin reload (without a full DLL
// unload) re-creates it instead of returning a dangling id.
void DestroyLogoTexture(SPF_UI_API *ui) {
  if (g_logo_texture.id && ui && ui->UI_DestroyTexture)
    ui->UI_DestroyTexture(g_logo_texture.id);
  g_logo_texture = LogoTexture{};
  g_logo_texture_tried = false;
}

void DrawAboutTab(SPF_UI_API *ui) {
  // A little breathing room from the tab bar above the logo.
  ui->UI_Dummy(0.0f, 6.0f);

  // Title: the logo image in place of the plugin name, via SPF's image API,
  // centered; falls back to plain colored text if the file couldn't load.
  const LogoTexture &logo = GetLogoTexture(ui);
  if (logo.id && logo.width > 0 && logo.height > 0) {
    constexpr float kLogoBox = 64.0f; // fitted into a square, aspect kept
    const float scale =
        kLogoBox / static_cast<float>(std::max(logo.width, logo.height));
    const float w = static_cast<float>(logo.width) * scale;
    float avail_x = 0.0f, avail_y = 0.0f;
    ui->UI_GetContentRegionAvail(&avail_x, &avail_y);
    const float offset_x = (avail_x - w) * 0.5f;
    if (offset_x > 0.0f)
      ui->UI_SetCursorPosX(ui->UI_GetCursorPosX() + offset_x);
    ui->UI_Image(logo.id, w, static_cast<float>(logo.height) * scale);
  } else {
    char title_md[64];
    std::snprintf(title_md, sizeof(title_md), "# <#B82728>%s</>",
                  PluginContext::kPluginName);
    ui->UI_RenderMarkdown(title_md, nullptr);
  }

  ui->UI_Spacing();

  // Tagline, centered: its own styled text since Markdown has no alignment
  // option of its own.
  SPF_TextStyle_Handle tagline_style = ui->UI_Style_Create();
  ui->UI_Style_SetColor(tagline_style, 0.63f, 0.63f, 0.63f, 1.0f);
  ui->UI_Style_SetAlign(tagline_style, SPF_TEXT_ALIGN_CENTER);
  ui->UI_TextStyled(tagline_style, "%s", loc::Tr("plugin.description"));
  ui->UI_Style_Destroy(tagline_style);

  ui->UI_Spacing();
  ui->UI_Spacing();
  DrawSectionTitle(ui, loc::Tr("ui.about.details"));
  ui->UI_Spacing();

  // Version and developer, left-aligned.
  const std::string intro =
      std::string(ICON_FA_TAG "  ") +
      loc::Tr("ui.about.version", {{"version", PLUGIN_VERSION}}) +
      "\n\n" ICON_FA_USER_PEN "  " +
      loc::Tr("ui.about.developer", {{"author", PLUGIN_AUTHOR}});
  ui->UI_RenderMarkdown(intro.c_str(), nullptr);

  ui->UI_Spacing();
  ui->UI_Spacing();
  DrawSectionTitle(ui, loc::Tr("ui.about.links"));
  ui->UI_Spacing();

  // Two equal-width buttons per row, filling the tab's width.
  constexpr float kButtonGap = 8.0f;
  float avail_x = 0.0f, avail_y = 0.0f;
  ui->UI_GetContentRegionAvail(&avail_x, &avail_y);
  const float button_w = (avail_x - kButtonGap) * 0.5f;

  LinkButton(ui, WithIcon(ICON_FA_GLOBE, loc::Tr("ui.about.website")),
             button_w, links::kWebsite, loc::Tr("ui.about.website_desc"));
  ui->UI_SameLine(0.0f, kButtonGap);
  LinkButton(ui, WithIcon(ICON_FA_DISCORD, loc::Tr("ui.about.discord")),
             button_w, links::kDiscord, loc::Tr("ui.about.discord_desc"));
  LinkButton(ui, WithIcon(ICON_FA_GITHUB, loc::Tr("ui.about.github")),
             button_w, links::kGithub, loc::Tr("ui.about.github_desc"));
  ui->UI_SameLine(0.0f, kButtonGap);
  LinkButton(ui, WithIcon(ICON_FA_YOUTUBE, loc::Tr("ui.about.youtube")),
             button_w, links::kYoutube, loc::Tr("ui.about.youtube_desc"));

  ui->UI_Spacing();
  ui->UI_Spacing();
  DrawSectionTitle(ui, loc::Tr("ui.about.support"));
  ui->UI_Spacing();
  ui->UI_RenderMarkdown(loc::Tr("ui.about.support_text"), nullptr);
  ui->UI_Spacing();
  LinkButton(ui, WithIcon(ICON_FA_PAYPAL, loc::Tr("ui.about.donate")), 0.0f,
             links::kPaypal, loc::Tr("ui.about.donate_desc"));

  ui->UI_Spacing();
  ui->UI_Spacing();
  DrawSectionTitle(ui, loc::Tr("ui.about.tips"));
  ui->UI_Spacing();
  const std::string settings_tab =
      WithIcon(ICON_FA_GEAR, "**" + std::string(loc::Tr("ui.tabs.settings")) +
                                 "**");
  const std::string tips =
      "- " + std::string(loc::Tr("ui.about.tip_reset")) + "\n- " +
      loc::Tr("ui.about.tip_profiles", {{"settings_tab", settings_tab}}) +
      "\n- " + loc::Tr("ui.about.tip_interior");
  ui->UI_RenderMarkdown(tips.c_str(), nullptr);

  ui->UI_Spacing();
  ui->UI_Spacing();
  DrawSectionTitle(ui, loc::Tr("ui.about.credits"));
  ui->UI_Spacing();
  ui->UI_RenderMarkdown(loc::Tr("ui.about.credits_text"), nullptr);
}

void DrawSettingsTab(SPF_UI_API *ui, SPF_Config_API *cfg,
                     SPF_Config_Handle *h) {
  DrawSectionTitle(ui, loc::Tr("ui.keybind.section"));
  // The keybind button and the profiles section's buttons all start at
  // the same x, just past the profile name field (180 px, or wider when
  // the translated keybind label needs the room).
  constexpr float kGap = 8.0f;
  const char *toggle_label = loc::Tr("ui.keybind.toggle_window");
  float label_w = 0.0f, label_h = 0.0f;
  ui->UI_CalcTextSize(toggle_label, &label_w, &label_h);
  const float field_w = std::max(180.0f, label_w);
  DrawKeybindRowAt(ui, "UI.toggle", toggle_label, field_w + kGap);

  ui->UI_Spacing();
  DrawProfilesSection(ui, field_w);

  ui->UI_Spacing();
  DrawResetSection(ui, cfg, h);
}

struct TabInfo {
  const char *icon;
  const char *title_key;
  const char *id;
};

// The window's tabs, in display order.
constexpr TabInfo kTabs[] = {
    {ICON_FA_VIDEO, "settings.driving.title", "driving"},
    {ICON_FA_ROAD, "settings.road.title", "road"},
    {ICON_FA_TRUCK, "settings.cabin.title", "cabin"},
    {ICON_FA_HAND, "settings.manual.title", "manual"},
    {ICON_FA_GEAR, "ui.tabs.settings", "settings"},
    {ICON_FA_CIRCLE_INFO, "ui.tabs.about", "about"},
};

constexpr size_t kTabCount = std::size(kTabs);

std::string TabTitle(const TabInfo &tab) {
  return WithIcon(tab.icon, loc::Tr(tab.title_key));
}

// Mirrors ImGui's TabBarLayout(): each tab's own width is its title
// (rounded up to a whole pixel) + FramePadding on both sides + 1 px, and
// tabs are ItemInnerSpacing apart. Everything is whole pixels.
struct TabMetrics {
  float width[kTabCount];
  float gap;
  float frame_pad_y;
  float total; // all tabs and the gaps between them
};

TabMetrics MeasureTabs(SPF_UI_API *ui) {
  SPF_Style_Handle *style = ui->UI_GetStyle();
  float pad_x = 0.0f, pad_y = 0.0f, spacing_x = 0.0f, spacing_y = 0.0f;
  ui->UI_Style_GetFramePadding(style, &pad_x, &pad_y);
  ui->UI_Style_GetItemSpacing(style, &spacing_x, &spacing_y);

  TabMetrics m{};
  // The gap is ItemInnerSpacing.x, which the SDK can't read; SPF's style
  // sets it to the same 4 px as ItemSpacing.y, scaled and truncated the
  // same way at every UI scale.
  m.gap = spacing_y;
  m.frame_pad_y = pad_y;
  m.total = m.gap * static_cast<float>(kTabCount - 1);
  for (size_t i = 0; i < kTabCount; ++i) {
    float text_w = 0.0f, text_h = 0.0f;
    ui->UI_CalcTextSize(TabTitle(kTabs[i]).c_str(), &text_w, &text_h);
    m.width[i] = text_w + pad_x * 2.0f + 1.0f;
    m.total += m.width[i];
  }
  return m;
}

// Widths that make the tabs fill the whole tab bar, spreading the spare
// room evenly (whole pixels, the leftover ones going to the first tabs).
// All 0 when the tabs don't even fit at their own width: ImGui then
// shrinks them itself, for the frame until FitWindowToTabs widens the
// window. Call right before UI_BeginTabBar.
std::array<float, kTabCount> StretchTabs(SPF_UI_API *ui,
                                         const TabMetrics &m) {
  std::array<float, kTabCount> widths{};
  float avail_x = 0.0f, avail_y = 0.0f;
  ui->UI_GetContentRegionAvail(&avail_x, &avail_y);
  const int spare = static_cast<int>(std::floor(avail_x - m.total));
  if (spare < 0)
    return widths;
  const int count = static_cast<int>(kTabCount);
  for (int i = 0; i < count; ++i)
    widths[i] = m.width[i] + static_cast<float>(spare / count +
                                                 (i < spare % count ? 1 : 0));
  return widths;
}

// Tab whose "###" ID keeps it selected across a language switch. With a
// `width`, the tab is stretched to it and its title drawn centered, since
// ImGui always left-aligns tab titles.
bool BeginTab(SPF_UI_API *ui, const TabInfo &tab, float width,
              const TabMetrics &m) {
  const std::string title = TabTitle(tab);
  if (width <= 0.0f) {
    const std::string label = title + "###tab_" + tab.id;
    return ui->UI_BeginTabItem(label.c_str(), nullptr,
                               SPF_TAB_ITEM_FLAG_NONE);
  }

  const std::string label = std::string("###tab_") + tab.id;
  ui->UI_SetNextItemWidth(width);
  const bool open =
      ui->UI_BeginTabItem(label.c_str(), nullptr, SPF_TAB_ITEM_FLAG_NONE);

  float min_x = 0.0f, min_y = 0.0f, max_x = 0.0f, max_y = 0.0f;
  float text_w = 0.0f, text_h = 0.0f;
  float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
  ui->UI_GetItemRectMin(&min_x, &min_y);
  ui->UI_GetItemRectMax(&max_x, &max_y);
  ui->UI_CalcTextSize(title.c_str(), &text_w, &text_h);
  ui->UI_GetStyleColor(SPF_COLOR_TEXT, &r, &g, &b, &a);
  // Same vertical placement as ImGui's own tab titles.
  ui->UI_DrawList_AddText(ui->UI_GetWindowDrawList(),
                          std::floor(min_x + (max_x - min_x - text_w) * 0.5f),
                          min_y + m.frame_pad_y,
                          ui->UI_ColorConvertFloat4ToU32(r, g, b, a),
                          title.c_str());
  return open;
}

// True on the first frame after a language switch. `seen` must start at
// the count from the window's first frame, so the language SPF starts with
// doesn't count as a switch (that would undo the size the player left the
// window at last session).
bool LanguageSwitched(unsigned &seen) {
  const unsigned now = loc::LanguageChangeCount();
  const bool switched = now != seen;
  seen = now;
  return switched;
}

// Keeps the window exactly wide enough for the tab titles, so none gets
// cut with "..." (many translations are longer than English): on the
// first launch and on a language switch it snaps to that width, 1 px
// narrower would cut a tab. In between, the player can widen it freely
// but not narrow it past that limit. SPF saves the size like any manual
// resize.
void FitWindowToTabs(SPF_UI_API *ui, const TabMetrics &m) {
  static unsigned s_seen_lang = loc::LanguageChangeCount();
  static bool s_first_frame = true;

  float win_pad_x = 0.0f, win_pad_y = 0.0f;
  ui->UI_Style_GetWindowPadding(ui->UI_GetStyle(), &win_pad_x, &win_pad_y);
  // ImGui only shrinks tabs once they overflow the window's width minus
  // WindowPadding on both sides by 1 px or more, and everything is whole
  // pixels, so this is the exact limit (without the vertical scrollbar,
  // which FitWindowToContent keeps hidden).
  const float required = std::round(m.total + win_pad_x * 2.0f);

  float win_w = 0.0f, win_h = 0.0f;
  ui->UI_GetWindowSize(&win_w, &win_h);

  // The first launch ever is the window still at its manifest default;
  // later launches keep whatever width the player left it at.
  const bool first_launch =
      s_first_frame &&
      std::fabs(win_w - static_cast<float>(defaults::kWindowWidth)) < 0.5f;
  s_first_frame = false;
  const bool snap = LanguageSwitched(s_seen_lang) || first_launch;

  if (snap ? std::fabs(win_w - required) < 0.5f : win_w >= required)
    return;
  ui->UI_SetWindowSize(required, win_h, SPF_COND_ALWAYS);
}

// Grows the window to its content's height so it never needs a vertical
// scrollbar (e.g. after expanding a section or switching tabs), and shrinks
// it back to the player's own height when the content gets shorter. A
// language switch resets that height to the default, like FitWindowToTabs.
// Never goes past the bottom of the screen. Call after drawing all the
// content.
void FitWindowToContent(SPF_UI_API *ui) {
  // The player's own height, and the height we forced on top of it (0
  // when the window is at the player's height).
  static float s_user_h = 0.0f;
  static float s_auto_h = 0.0f;
  static unsigned s_seen_lang = loc::LanguageChangeCount();

  SPF_Style_Handle *style = ui->UI_GetStyle();
  float pad_x = 0.0f, pad_y = 0.0f, spacing_x = 0.0f, spacing_y = 0.0f;
  ui->UI_Style_GetWindowPadding(style, &pad_x, &pad_y);
  ui->UI_Style_GetItemSpacing(style, &spacing_x, &spacing_y);

  float win_w = 0.0f, win_h = 0.0f;
  ui->UI_GetWindowSize(&win_w, &win_h);

  // Any height we didn't set ourselves is the player's choice, until the
  // next language switch.
  if (s_auto_h == 0.0f || std::fabs(win_h - s_auto_h) > 0.5f) {
    s_user_h = win_h;
    s_auto_h = 0.0f;
  }
  if (LanguageSwitched(s_seen_lang))
    s_user_h = static_cast<float>(defaults::kWindowHeight);

  // The cursor sits one ItemSpacing below the last item, in window-local
  // coordinates that already include the title bar and top padding.
  float required = std::ceil(ui->UI_GetCursorPosY() - spacing_y + pad_y);

  float win_x = 0.0f, win_y = 0.0f, vp_x = 0.0f, vp_y = 0.0f, vp_w = 0.0f,
        vp_h = 0.0f;
  ui->UI_GetWindowPos(&win_x, &win_y);
  ui->UI_GetMainViewportPos(&vp_x, &vp_y);
  ui->UI_GetMainViewportSize(&vp_w, &vp_h);
  const float max_h = vp_y + vp_h - win_y;
  if (max_h > 0.0f)
    required = std::min(required, max_h);

  const float target = std::max(s_user_h, required);
  if (std::fabs(target - win_h) <= 0.5f)
    return;

  ui->UI_SetWindowSize(win_w, target, SPF_COND_ALWAYS);
  s_auto_h = target > s_user_h + 0.5f ? target : 0.0f;
}

} // namespace

void DrawSettingsWindow(SPF_UI_API *ui, void * /*user_data*/) {
  PluginContext &ctx = Context();
  if (!ctx.core || !ctx.core->config || !ctx.config_handle)
    return;

  SPF_Config_API *cfg = ctx.core->config;
  SPF_Config_Handle *h = ctx.config_handle;

  loc::Sync();
  g_label_column_w = LabelColumnWidth(ui);

  const int pushed_colors = PushBrandColors(ui);
  const int pushed_vars = PushBrandRounding(ui);

  const std::string active_profile = profiles::LastUsedName(ctx);
  const std::string badge = WithIcon(
      ICON_FA_USER,
      loc::Tr("ui.profile_badge",
              {{"name", active_profile.empty() ? loc::Tr("ui.unsaved_changes")
                                               : active_profile}}));
  ui->UI_TextDisabled(badge.c_str());
  if (!active_profile.empty()) {
    if (!profiles::Matches(ctx, active_profile)) {
      ui->UI_SameLine(0.0f, 4.0f);
      ui->UI_TextColored(1.0f, 0.6f, 0.0f, 1.0f, "*");
    }
  }
  ui->UI_Spacing();

  const TabMetrics tab_metrics = MeasureTabs(ui);
  FitWindowToTabs(ui, tab_metrics);
  const std::array<float, kTabCount> tab_widths =
      StretchTabs(ui, tab_metrics);
  if (!ui->UI_BeginTabBar("MotionCabTabs", SPF_TAB_BAR_FLAG_NONE)) {
    ui->UI_PopStyleVar(pushed_vars);
    ui->UI_PopStyleColor(pushed_colors);
    return;
  }

  if (BeginTab(ui, kTabs[0], tab_widths[0], tab_metrics)) {
    DrawHeadMotion(ui, cfg, h);
    DrawBodyDynamics(ui, cfg, h);
    DrawSteeringCamera(ui, cfg, h);
    ui->UI_EndTabItem();
  }

  if (BeginTab(ui, kTabs[1], tab_widths[1], tab_metrics)) {
    DrawSuspension(ui, cfg, h);
    DrawRoadIrregularity(ui, cfg, h);
    DrawSpeedShake(ui, cfg, h);
    ui->UI_EndTabItem();
  }

  if (BeginTab(ui, kTabs[2], tab_widths[2], tab_metrics)) {
    DrawIdleBreathing(ui, cfg, h);
    DrawEngineVibration(ui, cfg, h);
    DrawEngineStartStop(ui, cfg, h);
    ui->UI_EndTabItem();
  }

  const bool manual_tab_open =
      BeginTab(ui, kTabs[3], tab_widths[3], tab_metrics);
  if (manual_tab_open) {
    DrawMirrorCheck(ui, cfg, h);
    DrawManualLook(ui, cfg, h);
    DrawManualZoom(ui, cfg, h);
    ui->UI_EndTabItem();
  }

  const bool settings_tab_open =
      BeginTab(ui, kTabs[4], tab_widths[4], tab_metrics);
  if (settings_tab_open) {
    DrawSettingsTab(ui, cfg, h);
    ui->UI_EndTabItem();
  }

  const bool about_tab_open =
      BeginTab(ui, kTabs[5], tab_widths[5], tab_metrics);
  if (about_tab_open) {
    DrawAboutTab(ui);
    ui->UI_EndTabItem();
  }

  ui->UI_EndTabBar();

  if (!settings_tab_open && !about_tab_open)
    ui->UI_TextDisabled(
        WithIcon(ICON_FA_CIRCLE_INFO, loc::Tr("ui.slider_reset_hint")).c_str());

  FitWindowToContent(ui);

  ui->UI_PopStyleVar(pushed_vars);
  ui->UI_PopStyleColor(pushed_colors);
}

void ReleaseLogoTexture(SPF_UI_API *ui) { DestroyLogoTexture(ui); }

} // namespace motioncab
