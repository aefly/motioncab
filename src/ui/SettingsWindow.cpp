#include "SettingsWindow.hpp"

#include "Links.hpp"
#include "PluginContext.hpp"
#include "ProfileManager.hpp"
#include "SPF_Icons.h"
#include "ui/OpenUrl.hpp"
#include "ui/SettingsDefaults.hpp"
#include "ui/SettingsText.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>

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

// Draws the visible part of a "Label##id" string only (ImGui's ID-hiding
// convention isn't parsed by UI_Text, so we truncate it ourselves).
void DrawVisibleLabel(SPF_UI_API *ui, const char *id_and_label) {
  const char *hash = std::strstr(id_and_label, "##");
  if (!hash) {
    ui->UI_Text(id_and_label);
    return;
  }
  char buffer[64];
  size_t len = static_cast<size_t>(hash - id_and_label);
  if (len >= sizeof(buffer))
    len = sizeof(buffer) - 1;
  std::memcpy(buffer, id_and_label, len);
  buffer[len] = '\0';
  ui->UI_Text(buffer);
}

// Toggle switch
bool DrawToggle(SPF_UI_API *ui, const char *id_and_label, bool *value,
                const char *tooltip) {
  // The icon is the only visible text; everything after "##" here (the
  // full original id_and_label, including its own "##suffix") becomes
  // the widget's unique ID. Using just the caller's suffix would collide
  // whenever an effect has more than one toggle (e.g. mirror_check's
  // "Enabled" and "Only When Stationary" both end in "##mirror_check").
  char button_id[128];
  std::snprintf(button_id, sizeof(button_id), "%s##%s",
                *value ? ICON_FA_TOGGLE_ON : ICON_FA_TOGGLE_OFF, id_and_label);

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

  ui->UI_SameLine(0.0f, 8.0f);
  DrawVisibleLabel(ui, id_and_label);
  return clicked;
}

bool DrawEnabled(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h,
                 const char *key, const char *label, const char *tooltip,
                 bool default_value) {
  bool value = cfg->Cfg_GetBool(h, key, default_value);
  if (DrawToggle(ui, label, &value, tooltip))
    cfg->Cfg_SetBool(h, key, value);
  return value;
}

// Toggle switch with no trailing label, used for the value column of a
// settings table row, where the label already lives in its own column.
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

// Begins the label|value table shared by every effect's settings body.
// Only one column ("Value") gets a header-style stretch; the label
// column is sized to fit its own content.
bool BeginSettingsTable(SPF_UI_API *ui, const char *id) {
  if (!ui->UI_BeginTable(id, 2, SPF_TABLE_FLAG_NONE, 0.0f, 0.0f, 0.0f))
    return false;
  ui->UI_TableSetupColumn("Label", SPF_TABLE_COLUMN_FLAG_WIDTH_FIXED, 160.0f,
                          0);
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
              const char *key, const char *label, const char *tooltip,
              bool default_value) {
  bool value = cfg->Cfg_GetBool(h, key, default_value);
  ui->UI_TableNextRow(SPF_TABLE_ROW_FLAG_NONE, 0.0f);
  ui->UI_TableSetColumnIndex(0);
  DrawVisibleLabel(ui, label);
  ui->UI_TableSetColumnIndex(1);
  if (DrawToggleCell(ui, key, &value, tooltip))
    cfg->Cfg_SetBool(h, key, value);
}

// One label|slider row. Right-click the slider to reset it to default.
void DrawFloat(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h,
               const char *key, const char *label, float min, float max,
               const char *format, const char *tooltip, float default_value) {
  float value = static_cast<float>(cfg->Cfg_GetFloat(h, key, default_value));
  ui->UI_TableNextRow(SPF_TABLE_ROW_FLAG_NONE, 0.0f);
  ui->UI_TableSetColumnIndex(0);
  DrawVisibleLabel(ui, label);
  ui->UI_TableSetColumnIndex(1);
  char hidden_label[112];
  std::snprintf(hidden_label, sizeof(hidden_label), "##%s", key);
  ui->UI_SetNextItemWidth(-1.0f);
  if (ui->UI_SliderFloat(hidden_label, &value, min, max, format,
                         SPF_SLIDER_FLAG_NONE))
    cfg->Cfg_SetFloat(h, key, value);
  if (ui->UI_IsItemClicked(SPF_MOUSE_BUTTON_RIGHT))
    cfg->Cfg_SetFloat(h, key, default_value);
  ui->UI_SetItemTooltip(tooltip);
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
  if (!ui->UI_CollapsingHeader(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT " Head Motion",
                               SPF_TREE_NODE_FLAG_NONE))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.driving.head_motion.enabled",
                  "Enabled##head_motion", tip::kEnabled, true);
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "head_motion_table")) {
    DrawFloat(ui, cfg, h, "settings.driving.head_motion.sway_strength",
              "Sway Strength##head_motion", 0.0f, 0.6f, "%.2f",
              tip::kHeadMotionSwayStrength, defaults::kHeadMotionSwayStrength);
    DrawFloat(ui, cfg, h, "settings.driving.head_motion.tilt_strength",
              "Tilt Strength##head_motion", 0.0f, 0.6f, "%.2f",
              tip::kHeadMotionTiltStrength, defaults::kHeadMotionTiltStrength);
    DrawFloat(ui, cfg, h, "settings.driving.head_motion.smoothing_time",
              "Smoothing##head_motion", 0.02f, 0.5f, "%.2f s",
              tip::kHeadMotionSmoothing, defaults::kHeadMotionSmoothing);
    EndSettingsTable(ui);
  }
  ui->UI_EndDisabled();
}

void DrawSteeringCamera(SPF_UI_API *ui, SPF_Config_API *cfg,
                        SPF_Config_Handle *h) {
  if (!ui->UI_CollapsingHeader(ICON_FA_ROTATE " Steering Camera",
                               SPF_TREE_NODE_FLAG_NONE))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.driving.steering_camera.enabled",
                  "Enabled##steering_camera", tip::kEnabled, true);
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "steering_camera_table")) {
    DrawFloat(ui, cfg, h,
              "settings.driving.steering_camera.rotation_factor_deg",
              "Rotation Amount##steering_camera", 20.0f, 60.0f, "%.1f deg",
              tip::kSteeringCameraRotationAmount,
              defaults::kSteeringCameraRotationAmount);
    DrawFloat(ui, cfg, h, "settings.driving.steering_camera.smoothing_time",
              "Smoothing##steering_camera", 0.02f, 1.0f, "%.2f s",
              tip::kSteeringCameraSmoothing,
              defaults::kSteeringCameraSmoothing);
    DrawFloat(ui, cfg, h, "settings.driving.steering_camera.delay_seconds",
              "Reaction Delay##steering_camera", 0.0f, 0.3f, "%.2f s",
              tip::kSteeringCameraReactionDelay,
              defaults::kSteeringCameraReactionDelay);
    EndSettingsTable(ui);
  }
  ui->UI_EndDisabled();
}

void DrawIdleBreathing(SPF_UI_API *ui, SPF_Config_API *cfg,
                       SPF_Config_Handle *h) {
  if (!ui->UI_CollapsingHeader(ICON_FA_LUNGS " Idle Breathing",
                               SPF_TREE_NODE_FLAG_NONE))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.cabin.idle_breathing.enabled",
                  "Enabled##idle_breathing", tip::kEnabled, true);
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "idle_breathing_table")) {
    DrawFloat(ui, cfg, h, "settings.cabin.idle_breathing.vertical_amplitude",
              "Vertical Amount##idle_breathing", 0.0f, 0.01f, "%.3f m",
              tip::kIdleBreathingVerticalAmount,
              defaults::kIdleBreathingVerticalAmount);
    DrawFloat(ui, cfg, h, "settings.cabin.idle_breathing.pitch_amplitude_deg",
              "Head Nod Amount##idle_breathing", 0.0f, 1.5f, "%.2f deg",
              tip::kIdleBreathingHeadNodAmount,
              defaults::kIdleBreathingHeadNodAmount);
    DrawFloat(ui, cfg, h, "settings.cabin.idle_breathing.breathing_rate_bpm",
              "Breathing Rate##idle_breathing", 10.0f, 20.0f, "%.0f bpm",
              tip::kIdleBreathingRate, defaults::kIdleBreathingRate);
    DrawFloat(ui, cfg, h, "settings.cabin.idle_breathing.fade_start_kmh",
              "Fade Start Speed##idle_breathing", 0.0f, 100.0f, "%.0f km/h",
              tip::kIdleBreathingFadeStart, defaults::kIdleBreathingFadeStart);
    DrawFloat(ui, cfg, h, "settings.cabin.idle_breathing.fade_end_kmh",
              "Fade End Speed##idle_breathing", 0.0f, 100.0f, "%.0f km/h",
              tip::kIdleBreathingFadeEnd, defaults::kIdleBreathingFadeEnd);
    EndSettingsTable(ui);
  }
  ui->UI_EndDisabled();
}

void DrawSuspension(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h) {
  if (!ui->UI_CollapsingHeader(ICON_FA_ARROWS_UP_DOWN " Suspension",
                               SPF_TREE_NODE_FLAG_NONE))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.road.suspension.enabled",
                  "Enabled##suspension", tip::kEnabled, true);
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "suspension_table")) {
    DrawFloat(ui, cfg, h, "settings.road.suspension.vertical_strength",
              "Vertical Strength##suspension", 0.0f, 2.0f, "%.2f",
              tip::kSuspensionVerticalStrength,
              defaults::kSuspensionVerticalStrength);
    DrawFloat(ui, cfg, h, "settings.road.suspension.reactivity",
              "Reactivity##suspension", 0.02f, 0.3f, "%.2f s",
              tip::kSuspensionReactivity, defaults::kSuspensionReactivity);
    DrawFloat(ui, cfg, h, "settings.road.suspension.grade_strength",
              "Grade Follow##suspension", 0.0f, 1.0f, "%.2f",
              tip::kSuspensionGradeStrength,
              defaults::kSuspensionGradeStrength);
    EndSettingsTable(ui);
  }
  ui->UI_EndDisabled();
}

void DrawSpeedShake(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h) {
  if (!ui->UI_CollapsingHeader(ICON_FA_TRUCK " Speed Shake",
                               SPF_TREE_NODE_FLAG_NONE))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.road.speed_shake.enabled",
                  "Enabled##speed_shake", tip::kEnabled, true);
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "speed_shake_table")) {
    DrawFloat(ui, cfg, h, "settings.road.speed_shake.intensity",
              "Intensity##speed_shake", 0.0f, 2.0f, "%.2f",
              tip::kSpeedShakeIntensity, defaults::kSpeedShakeIntensity);
    DrawFloat(ui, cfg, h, "settings.road.speed_shake.smoothing_time",
              "Smoothing##speed_shake", 0.1f, 1.0f, "%.2f s",
              tip::kSpeedShakeSmoothing, defaults::kSpeedShakeSmoothing);
    DrawFloat(ui, cfg, h, "settings.road.speed_shake.rotation",
              "Rotation##speed_shake", 0.0f, 2.0f, "%.2f",
              tip::kSpeedShakeRotation, defaults::kSpeedShakeRotation);
    DrawFloat(ui, cfg, h, "settings.road.speed_shake.vertical",
              "Vertical##speed_shake", 0.0f, 2.0f, "%.2f",
              tip::kSpeedShakeVertical, defaults::kSpeedShakeVertical);
    DrawFloat(ui, cfg, h, "settings.road.speed_shake.roughness",
              "Roughness##speed_shake", 0.0f, 1.0f, "%.2f",
              tip::kSpeedShakeRoughness, defaults::kSpeedShakeRoughness);
    EndSettingsTable(ui);
  }
  ui->UI_EndDisabled();
}

void DrawBodyDynamics(SPF_UI_API *ui, SPF_Config_API *cfg,
                      SPF_Config_Handle *h) {
  if (!ui->UI_CollapsingHeader(ICON_FA_PERSON " Body Dynamics",
                               SPF_TREE_NODE_FLAG_NONE))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.driving.body_dynamics.enabled",
                  "Enabled##body_dynamics", tip::kEnabled, true);
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "body_dynamics_table")) {
    DrawFloat(ui, cfg, h, "settings.driving.body_dynamics.lean_strength",
              "Lean Strength##body_dynamics", 0.0f, 2.0f, "%.2f",
              tip::kBodyDynamicsLeanStrength,
              defaults::kBodyDynamicsLeanStrength);
    DrawFloat(ui, cfg, h, "settings.driving.body_dynamics.nod_strength",
              "Nod Strength##body_dynamics", 0.0f, 2.0f, "%.2f",
              tip::kBodyDynamicsNodStrength,
              defaults::kBodyDynamicsNodStrength);
    DrawFloat(ui, cfg, h, "settings.driving.body_dynamics.smoothing_time",
              "Smoothing##body_dynamics", 0.05f, 0.6f, "%.2f s",
              tip::kBodyDynamicsSmoothing, defaults::kBodyDynamicsSmoothing);
    EndSettingsTable(ui);
  }
  ui->UI_EndDisabled();
}

void DrawRoadIrregularity(SPF_UI_API *ui, SPF_Config_API *cfg,
                          SPF_Config_Handle *h) {
  if (!ui->UI_CollapsingHeader(ICON_FA_ROAD " Road Irregularity",
                               SPF_TREE_NODE_FLAG_NONE))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.road.road_irregularity.enabled",
                  "Enabled##road_irregularity", tip::kEnabled, true);
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "road_irregularity_table")) {
    DrawFloat(ui, cfg, h, "settings.road.road_irregularity.intensity",
              "Intensity##road_irregularity", 0.0f, 2.0f, "%.2f",
              tip::kRoadIrregularityIntensity,
              defaults::kRoadIrregularityIntensity);
    DrawFloat(ui, cfg, h, "settings.road.road_irregularity.reactivity",
              "Reactivity##road_irregularity", 0.02f, 0.2f, "%.2f s",
              tip::kRoadIrregularityReactivity,
              defaults::kRoadIrregularityReactivity);
    EndSettingsTable(ui);
  }
  ui->UI_EndDisabled();
}

void DrawEngineVibration(SPF_UI_API *ui, SPF_Config_API *cfg,
                         SPF_Config_Handle *h) {
  if (!ui->UI_CollapsingHeader(ICON_FA_WAVE_SQUARE " Engine Vibration",
                               SPF_TREE_NODE_FLAG_NONE))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.cabin.engine_vibration.enabled",
                  "Enabled##engine_vibration", tip::kEnabled, true);
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "engine_vibration_table")) {
    DrawFloat(ui, cfg, h, "settings.cabin.engine_vibration.intensity",
              "Intensity##engine_vibration", 0.0f, 2.0f, "%.2f",
              tip::kEngineVibrationIntensity,
              defaults::kEngineVibrationIntensity);
    EndSettingsTable(ui);
  }
  ui->UI_EndDisabled();
}

void DrawEngineStartStop(SPF_UI_API *ui, SPF_Config_API *cfg,
                         SPF_Config_Handle *h) {
  if (!ui->UI_CollapsingHeader(ICON_FA_POWER_OFF " Engine Start/Stop",
                               SPF_TREE_NODE_FLAG_NONE))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.cabin.engine_start_stop.enabled",
                  "Enabled##engine_start_stop", tip::kEnabled, true);
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "engine_start_stop_table")) {
    DrawFloat(ui, cfg, h, "settings.cabin.engine_start_stop.intensity",
              "Intensity##engine_start_stop", 0.0f, 0.3f, "%.2f",
              tip::kEngineStartStopIntensity,
              defaults::kEngineStartStopIntensity);
    DrawFloat(ui, cfg, h, "settings.cabin.engine_start_stop.duration",
              "Duration##engine_start_stop", 0.3f, 2.0f, "%.2f s",
              tip::kEngineStartStopDuration,
              defaults::kEngineStartStopDuration);
    EndSettingsTable(ui);
  }
  ui->UI_EndDisabled();
}

// Small muted, word-wrapped note. Wraps at the window edge instead of
// running off it, unlike a plain UI_Text/UI_TextDisabled call.
void DrawWrappedHint(SPF_UI_API *ui, const char *text) {
  ui->UI_PushStyleColor(SPF_COLOR_TEXT, 0.6f, 0.6f, 0.6f, 1.0f);
  ui->UI_TextWrapped(text);
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
    if (ui->UI_Button(name[0] ? name : "(unbound)", 0.0f, 0.0f))
      kb->Kbind_OpenRebindPopup(ctx.keybinds_handle, action, i);
    ui->UI_SetItemTooltip("Click to rebind.");
    ui->UI_SameLine(0.0f, 4.0f);
    if (ui->UI_Button(ICON_FA_GEAR, 0.0f, 0.0f))
      kb->Kbind_OpenBindingDetailsPopup(ctx.keybinds_handle, action, i);
    ui->UI_SetItemTooltip("Binding options (hold/toggle, deadzone, ...).");
    ui->UI_SameLine(0.0f, 12.0f);
    ui->UI_PopID();
  }
  if (ui->UI_Button(ICON_FA_PLUS "##add", 0.0f, 0.0f))
    kb->Kbind_OpenRebindPopup(ctx.keybinds_handle, action, -1);
  ui->UI_SetItemTooltip("Add another binding.");
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
  DrawVisibleLabel(ui, label);
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
  DrawVisibleLabel(ui, label);
  ui->UI_SameLine(start_x + button_x, 0.0f);
  DrawKeybindButtons(ui, action);
}

void DrawMirrorCheck(SPF_UI_API *ui, SPF_Config_API *cfg,
                     SPF_Config_Handle *h) {
  if (!ui->UI_CollapsingHeader(ICON_FA_EYE " Mirror Check",
                               SPF_TREE_NODE_FLAG_NONE))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.manual.mirror_check.enabled",
                  "Enabled##mirror_check", tip::kEnabled, true);
  DrawWrappedHint(ui, ICON_FA_CIRCLE_INFO " Requires the game's native "
                                          "\"Left-Turn Indicator\" and "
                                          "\"Right-Turn Indicator\" keys to be "
                                          "assigned.");
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "mirror_check_table")) {
    DrawFloat(ui, cfg, h, "settings.manual.mirror_check.look_angle_deg",
              "Look Angle##mirror_check", 15.0f, 50.0f, "%.0f deg",
              tip::kMirrorCheckLookAngle, defaults::kMirrorCheckLookAngle);
    DrawFloat(ui, cfg, h, "settings.manual.mirror_check.pitch_offset_deg",
              "Pitch Offset##mirror_check", 0.0f, 10.0f, "%.0f deg",
              tip::kMirrorCheckPitchOffset, defaults::kMirrorCheckPitchOffset);
    DrawFloat(ui, cfg, h, "settings.manual.mirror_check.smoothing_time",
              "Smoothing##mirror_check", 0.0f, 0.7f, "%.2f s",
              tip::kMirrorCheckSmoothing, defaults::kMirrorCheckSmoothing);
    DrawBool(ui, cfg, h, "settings.manual.mirror_check.require_stationary",
             "Only When Stationary##mirror_check",
             tip::kMirrorCheckRequireStationary,
             defaults::kMirrorCheckRequireStationary);
    DrawBool(ui, cfg, h,
             "settings.manual.mirror_check.ignore_after_moving_signal",
             "Ignore While Moving##mirror_check",
             tip::kMirrorCheckIgnoreAfterMovingSignal,
             defaults::kMirrorCheckIgnoreAfterMovingSignal);
    EndSettingsTable(ui);
  }
  ui->UI_EndDisabled();
}

void DrawManualLook(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h) {
  if (!ui->UI_CollapsingHeader(ICON_FA_ARROWS_LEFT_RIGHT " Manual Look",
                               SPF_TREE_NODE_FLAG_NONE))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.manual.manual_look.enabled",
                  "Enabled##manual_look", tip::kEnabled, true);
  DrawWrappedHint(ui, ICON_FA_CIRCLE_INFO " These keys are separate from the "
                                          "game's native controls.");
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "manual_look_table")) {
    DrawKeybindRow(ui, "ManualLook.look_left", "Look Left");
    DrawKeybindRow(ui, "ManualLook.look_right", "Look Right");
    DrawFloat(ui, cfg, h, "settings.manual.manual_look.look_angle_deg",
              "Look Angle##manual_look", 20.0f, 90.0f, "%.0f deg",
              tip::kManualLookLookAngle, defaults::kManualLookLookAngle);
    DrawFloat(ui, cfg, h, "settings.manual.manual_look.smoothing_time",
              "Smoothing##manual_look", 0.0f, 0.7f, "%.2f s",
              tip::kManualLookSmoothing, defaults::kManualLookSmoothing);
    DrawBool(ui, cfg, h, "settings.manual.manual_look.toggle_mode",
             "Toggle Mode##manual_look", tip::kManualLookToggleMode,
             defaults::kManualLookToggleMode);
    EndSettingsTable(ui);
  }
  ui->UI_EndDisabled();
}

void DrawManualZoom(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h) {
  if (!ui->UI_CollapsingHeader(ICON_FA_MAGNIFYING_GLASS_PLUS " Manual Zoom",
                               SPF_TREE_NODE_FLAG_NONE))
    return;
  const bool enabled =
      DrawEnabled(ui, cfg, h, "settings.manual.manual_zoom.enabled",
                  "Enabled##manual_zoom", tip::kEnabled, true);
  DrawWrappedHint(ui, ICON_FA_CIRCLE_INFO " This key is separate from the "
                                          "game's native \"Zoom Interior "
                                          "Camera\" key.");
  ui->UI_BeginDisabled(!enabled);
  if (BeginSettingsTable(ui, "manual_zoom_table")) {
    DrawKeybindRow(ui, "ManualZoom.zoom", "Zoom");
    DrawFloat(ui, cfg, h, "settings.manual.manual_zoom.zoom_fov_deg",
              "Zoom Level##manual_zoom", 5.0f, 60.0f, "%.0f deg",
              tip::kManualZoomZoomLevel, defaults::kManualZoomZoomLevel);
    DrawFloat(ui, cfg, h, "settings.manual.manual_zoom.smoothing_time",
              "Smoothing##manual_zoom", 0.02f, 0.5f, "%.2f s",
              tip::kManualZoomSmoothing, defaults::kManualZoomSmoothing);
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

void DrawProfilesSection(SPF_UI_API *ui) {
  PluginContext &ctx = Context();

  DrawSectionTitle(ui, "Profiles");

  static char profile_name_buf[64] = "";
  static std::string pending_select;
  static std::string pending_overwrite;

  auto do_save = [&](const std::string &saved_name) {
    if (profiles::Save(ctx, saved_name)) {
      profile_name_buf[0] = '\0';
      pending_select = saved_name;
      ShowToast(ui, SPF_NOTIFICATION_SUCCESS,
                "Saved profile \"" + saved_name + "\".");
      return true;
    }
    ShowToast(ui, SPF_NOTIFICATION_ERROR,
              "Failed to save profile, check MotionCab.log.");
    return false;
  };

  ui->UI_SetNextItemWidth(180.0f);
  ui->UI_InputTextWithHint("##profile_name", "Profile name...",
                           profile_name_buf, sizeof(profile_name_buf),
                           SPF_INPUT_TEXT_FLAG_NONE);
  ui->UI_SameLine(0.0f, 8.0f);
  char save_label[48];
  std::snprintf(save_label, sizeof(save_label), "%s Save as Profile",
                ICON_FA_FLOPPY_DISK);
  if (MutedButton(ui, save_label)) {
    const std::string sanitized = profiles::Sanitize(profile_name_buf);
    if (sanitized.empty()) {
      ShowToast(ui, SPF_NOTIFICATION_WARNING,
                "Enter a valid profile name first.");
    } else {
      const std::vector<std::string> existing = profiles::List(ctx);
      // Case-insensitive: on Windows "default" is the same file as
      // "Default". Reuse the stored spelling so the active profile name
      // keeps matching the dropdown entry.
      const std::string *match = profiles::FindIgnoreCase(existing, sanitized);
      if (match) {
        pending_overwrite = *match;
        ui->UI_OpenPopup("Confirm Overwrite##confirm_overwrite_profile",
                         SPF_POPUP_FLAG_NONE);
      } else {
        do_save(sanitized);
      }
    }
  }
  ui->UI_SetItemTooltip(
      "Saves every current effect setting under this profile name.");

  if (ui->UI_BeginPopupModal("Confirm Overwrite##confirm_overwrite_profile",
                             nullptr, SPF_WINDOW_FLAG_NONE)) {
    char message[128];
    std::snprintf(message, sizeof(message),
                  "A profile named \"%s\" already exists. Overwrite it?",
                  pending_overwrite.c_str());
    ui->UI_TextUnformatted(message);
    ui->UI_Spacing();
    if (MutedButton(ui, "Yes, Overwrite")) {
      do_save(pending_overwrite);
      ui->UI_CloseCurrentPopup();
    }
    ui->UI_SameLine(0.0f, 8.0f);
    if (MutedButton(ui, "Cancel"))
      ui->UI_CloseCurrentPopup();
    ui->UI_EndPopup();
  }

  ui->UI_Spacing();
  const std::vector<std::string> profile_names = profiles::List(ctx);
  if (profile_names.empty()) {
    ui->UI_TextDisabled("No saved profiles yet.");
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
                              : "(Unsaved changes)";
    ui->UI_SetNextItemWidth(180.0f);
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
                      "Failed to load profile \"" + profile_names[i] +
                          "\", check MotionCab.log.");
          }
        }
      }
      ui->UI_EndCombo();
    }
    ui->UI_PopStyleVar(1);
    ui->UI_SetItemTooltip("Selecting a profile loads it immediately.");

    if (selected_profile < 0) {
      ui->UI_TextDisabled(
          "No profile matches the current settings. Pick one to load, "
          "or save these as a new profile above.");
      return;
    }

    const std::string &selected_name = profile_names[selected_profile];

    ui->UI_SameLine(0.0f, 8.0f);
    static std::chrono::steady_clock::time_point save_confirm_until{};
    const bool save_confirmed =
        std::chrono::steady_clock::now() < save_confirm_until;
    const char *update_label = save_confirmed ? ICON_FA_CHECK " Saved##save"
                                              : ICON_FA_FLOPPY_DISK
                                   " Save##save";
    if (MutedButton(ui, update_label) && do_save(selected_name))
      save_confirm_until =
          std::chrono::steady_clock::now() + std::chrono::seconds(2);
    ui->UI_SetItemTooltip(
        "Overwrites the selected profile with the current settings.");

    ui->UI_SameLine(0.0f, 4.0f);
    char delete_label[48];
    std::snprintf(delete_label, sizeof(delete_label), "%s Delete",
                  ICON_FA_TRASH);
    static std::string pending_delete;
    if (MutedButton(ui, delete_label)) {
      pending_delete = selected_name;
      ui->UI_OpenPopup("Confirm Delete##confirm_delete_profile",
                       SPF_POPUP_FLAG_NONE);
    }
    ui->UI_SetItemTooltip("Delete the selected profile.");

    if (ui->UI_BeginPopupModal("Confirm Delete##confirm_delete_profile",
                               nullptr, SPF_WINDOW_FLAG_NONE)) {
      char message[128];
      std::snprintf(message, sizeof(message),
                    "Delete profile \"%s\"? This cannot be undone.",
                    pending_delete.c_str());
      ui->UI_TextUnformatted(message);
      ui->UI_Spacing();
      if (MutedButton(ui, "Yes, Delete")) {
        if (!profiles::Delete(ctx, pending_delete)) {
          ShowToast(ui, SPF_NOTIFICATION_ERROR,
                    "Failed to delete profile \"" + pending_delete +
                        "\", check MotionCab.log.");
        } else {
          ShowToast(ui, SPF_NOTIFICATION_SUCCESS,
                    "Deleted profile \"" + pending_delete + "\".");
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
                      "Deleted, but failed to load \"Default\", check "
                      "MotionCab.log.");
          }
        }
        ui->UI_CloseCurrentPopup();
      }
      ui->UI_SameLine(0.0f, 8.0f);
      if (MutedButton(ui, "Cancel"))
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

  DrawSectionTitle(ui, "Danger Zone");
  char reset_label[48];
  std::snprintf(reset_label, sizeof(reset_label), "%s Reset All Settings",
                ICON_FA_ARROW_ROTATE_LEFT);
  if (MutedButton(ui, reset_label))
    ui->UI_OpenPopup("Confirm Reset##confirm_reset_all", SPF_POPUP_FLAG_NONE);

  if (ui->UI_BeginPopupModal("Confirm Reset##confirm_reset_all", nullptr,
                             SPF_WINDOW_FLAG_NONE)) {
    const std::string active_profile = profiles::LastUsedName(ctx);
    ui->UI_TextUnformatted(
        "Reset every MotionCab setting to its default value?");
    ui->UI_Spacing();
    if (MutedButton(ui, "Yes, Reset Everything")) {
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
      ShowToast(ui, SPF_NOTIFICATION_SUCCESS,
                "Reset every MotionCab setting to its default value.");
      ui->UI_CloseCurrentPopup();
    }
    ui->UI_SameLine(0.0f, 8.0f);
    if (MutedButton(ui, "Cancel"))
      ui->UI_CloseCurrentPopup();
    ui->UI_EndPopup();
  }
}

// --- About tab ---

// Button in the UI's brand red (the README badges' #B82728) that opens `url`
// in the browser; the description and the address are shown in its tooltip.
// `width` 0 sizes it to its label.
void LinkButton(SPF_UI_API *ui, const char *label, float width, const char *url,
                const char *description) {
  const bool clicked = MutedButton(ui, label, width);

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
  ui->UI_TextStyled(tagline_style, "%s", PLUGIN_DESCRIPTION);
  ui->UI_Style_Destroy(tagline_style);

  ui->UI_Spacing();
  ui->UI_Spacing();
  DrawSectionTitle(ui, "Details");
  ui->UI_Spacing();

  // Version and developer, left-aligned.
  char intro[256];
  std::snprintf(intro, sizeof(intro),
                ICON_FA_TAG "  Version: **%s**\n\n" ICON_FA_USER_PEN
                            "  Developer: **%s**",
                PLUGIN_VERSION, PLUGIN_AUTHOR);
  ui->UI_RenderMarkdown(intro, nullptr);

  ui->UI_Spacing();
  ui->UI_Spacing();
  DrawSectionTitle(ui, "Links");
  ui->UI_Spacing();

  // Two equal-width buttons per row, filling the tab's width.
  constexpr float kButtonGap = 8.0f;
  float avail_x = 0.0f, avail_y = 0.0f;
  ui->UI_GetContentRegionAvail(&avail_x, &avail_y);
  const float button_w = (avail_x - kButtonGap) * 0.5f;

  LinkButton(ui, ICON_FA_GLOBE " Website", button_w, links::kWebsite,
             "Documentation");
  ui->UI_SameLine(0.0f, kButtonGap);
  LinkButton(ui, ICON_FA_DISCORD " Discord", button_w, links::kDiscord,
             "Community and support");
  LinkButton(ui, ICON_FA_GITHUB " GitHub", button_w, links::kGithub,
             "Releases and bug reports");
  ui->UI_SameLine(0.0f, kButtonGap);
  LinkButton(ui, ICON_FA_YOUTUBE " YouTube", button_w, links::kYoutube,
             "Videos and showcases");

  ui->UI_Spacing();
  ui->UI_Spacing();
  DrawSectionTitle(ui, "Support");
  ui->UI_Spacing();
  ui->UI_RenderMarkdown(
      "If you enjoy MotionCab, you can support its development.", nullptr);
  ui->UI_Spacing();
  LinkButton(ui, ICON_FA_PAYPAL " Donate with PayPal", 0.0f, links::kPaypal,
             "Support MotionCab on PayPal");

  ui->UI_Spacing();
  ui->UI_Spacing();
  DrawSectionTitle(ui, "Tips");
  ui->UI_Spacing();
  ui->UI_RenderMarkdown(
      "- Right-click any slider to reset it to its default value.\n"
      "- Save your tuning as a profile in the " ICON_FA_GEAR
      " **Settings** tab.\n"
      "- Every effect works in the interior camera only.",
      nullptr);

  ui->UI_Spacing();
  ui->UI_Spacing();
  DrawSectionTitle(ui, "Credits");
  ui->UI_Spacing();
  ui->UI_RenderMarkdown("SPF Framework and contributors.", nullptr);
}

void DrawSettingsTab(SPF_UI_API *ui, SPF_Config_API *cfg,
                     SPF_Config_Handle *h) {
  DrawSectionTitle(ui, "Keybinds");
  // 180 px input/combo width + 8 px gap: where "Save as Profile" and "Save"
  // start in the profiles section, so the keybind button lines up with them.
  constexpr float kProfileButtonX = 180.0f + 8.0f;
  DrawKeybindRowAt(ui, "UI.toggle", "Toggle Window", kProfileButtonX);

  ui->UI_Spacing();
  DrawProfilesSection(ui);

  ui->UI_Spacing();
  DrawResetSection(ui, cfg, h);
}

} // namespace

void DrawSettingsWindow(SPF_UI_API *ui, void * /*user_data*/) {
  PluginContext &ctx = Context();
  if (!ctx.core || !ctx.core->config || !ctx.config_handle)
    return;

  SPF_Config_API *cfg = ctx.core->config;
  SPF_Config_Handle *h = ctx.config_handle;

  const int pushed_colors = PushBrandColors(ui);
  const int pushed_vars = PushBrandRounding(ui);

  const std::string active_profile = profiles::LastUsedName(ctx);
  if (active_profile.empty()) {
    ui->UI_TextDisabled(ICON_FA_USER " Profile: (Unsaved changes)");
  } else {
    const std::string badge =
        std::string(ICON_FA_USER " Profile: ") + active_profile;
    ui->UI_TextDisabled(badge.c_str());
    if (!profiles::Matches(ctx, active_profile)) {
      ui->UI_SameLine(0.0f, 4.0f);
      ui->UI_TextColored(1.0f, 0.6f, 0.0f, 1.0f, "*");
    }
  }
  ui->UI_Spacing();

  if (!ui->UI_BeginTabBar("MotionCabTabs", SPF_TAB_BAR_FLAG_NONE)) {
    ui->UI_PopStyleVar(pushed_vars);
    ui->UI_PopStyleColor(pushed_colors);
    return;
  }

  if (ui->UI_BeginTabItem(ICON_FA_VIDEO " Driving", nullptr,
                          SPF_TAB_ITEM_FLAG_NONE)) {
    DrawHeadMotion(ui, cfg, h);
    DrawBodyDynamics(ui, cfg, h);
    DrawSteeringCamera(ui, cfg, h);
    ui->UI_EndTabItem();
  }

  if (ui->UI_BeginTabItem(ICON_FA_ROAD " Road", nullptr,
                          SPF_TAB_ITEM_FLAG_NONE)) {
    DrawSuspension(ui, cfg, h);
    DrawRoadIrregularity(ui, cfg, h);
    DrawSpeedShake(ui, cfg, h);
    ui->UI_EndTabItem();
  }

  if (ui->UI_BeginTabItem(ICON_FA_TRUCK " Cabin", nullptr,
                          SPF_TAB_ITEM_FLAG_NONE)) {
    DrawIdleBreathing(ui, cfg, h);
    DrawEngineVibration(ui, cfg, h);
    DrawEngineStartStop(ui, cfg, h);
    ui->UI_EndTabItem();
  }

  const bool manual_tab_open = ui->UI_BeginTabItem(
      ICON_FA_HAND " Manual", nullptr, SPF_TAB_ITEM_FLAG_NONE);
  if (manual_tab_open) {
    DrawMirrorCheck(ui, cfg, h);
    DrawManualLook(ui, cfg, h);
    DrawManualZoom(ui, cfg, h);
    ui->UI_EndTabItem();
  }

  const bool settings_tab_open = ui->UI_BeginTabItem(
      ICON_FA_GEAR " Settings", nullptr, SPF_TAB_ITEM_FLAG_NONE);
  if (settings_tab_open) {
    DrawSettingsTab(ui, cfg, h);
    ui->UI_EndTabItem();
  }

  const bool about_tab_open = ui->UI_BeginTabItem(
      ICON_FA_CIRCLE_INFO " About", nullptr, SPF_TAB_ITEM_FLAG_NONE);
  if (about_tab_open) {
    DrawAboutTab(ui);
    ui->UI_EndTabItem();
  }

  ui->UI_EndTabBar();

  if (!settings_tab_open && !about_tab_open)
    ui->UI_TextDisabled(ICON_FA_CIRCLE_INFO
                        " Right-click a slider to reset it to default.");

  ui->UI_PopStyleVar(pushed_vars);
  ui->UI_PopStyleColor(pushed_colors);
}

void ReleaseLogoTexture(SPF_UI_API *ui) { DestroyLogoTexture(ui); }

} // namespace motioncab
