#include "Manifest.hpp"

#include "Links.hpp"
#include "PluginContext.hpp"

namespace motioncab {

void BuildManifest(SPF_Manifest_Builder_Handle *h,
                   const SPF_Manifest_Builder_API *api) {

  // --- Identity ---
  api->Info_SetName(h, PluginContext::kPluginName);
  api->Info_SetVersion(h, PLUGIN_VERSION);
  api->Info_SetAuthor(h, PLUGIN_AUTHOR);
  // No Info_SetDescriptionLiteral: SPF shows a literal over the key, which
  // would leave the description untranslated.
  api->Info_SetDescriptionKey(h, "plugin.description");
  api->Info_SetMinFrameworkVersion(h, "1.2.4");
  api->Info_SetWebsiteUrl(h, links::kWebsite);
  api->Info_SetGithubUrl(h, links::kGithub);
  api->Info_SetYoutubeUrl(h, links::kYoutube);
  api->Info_SetDiscordUrl(h, links::kDiscord);

  // --- Configuration Policy ---
  api->Policy_SetAllowUserConfig(h, true);
  api->Policy_AddConfigurableSystem(h, "settings");
  api->Policy_AddConfigurableSystem(h, "logging");
  api->Policy_AddConfigurableSystem(h, "ui");
  api->Policy_AddConfigurableSystem(h, "localization");

  // --- Default Settings ---
  const char *defaults = R"json({
    "driving": {
      "head_motion": {
        "enabled": true,
        "sway_strength": 0.2,
        "tilt_strength": 0.15,
        "smoothing_time": 0.2
      },
      "body_dynamics": {
        "enabled": true,
        "lean_strength": 1.0,
        "nod_strength": 1.0,
        "smoothing_time": 0.25
      },
      "steering_camera": {
        "enabled": true,
        "rotation_factor_deg": 30.0,
        "smoothing_time": 0.5,
        "delay_seconds": 0.2
      }
    },
    "road": {
      "suspension": {
        "enabled": true,
        "vertical_strength": 1.0,
        "reactivity": 0.25,
        "grade_strength": 0.5
      },
      "road_irregularity": {
        "enabled": true,
        "intensity": 1.0,
        "reactivity": 0.05
      },
      "speed_shake": {
        "enabled": true,
        "intensity": 1.0,
        "smoothing_time": 0.3,
        "rotation": 1.0,
        "vertical": 1.0,
        "roughness": 0.5
      }
    },
    "cabin": {
      "idle_breathing": {
        "enabled": true,
        "vertical_amplitude": 0.002,
        "pitch_amplitude_deg": 0.15,
        "breathing_rate_bpm": 15.0,
        "fade_start_kmh": 30.0,
        "fade_end_kmh": 50.0
      },
      "engine_vibration": {
        "enabled": true,
        "intensity": 1.0
      },
      "engine_start_stop": {
        "enabled": true,
        "intensity": 0.1,
        "duration": 1.0
      }
    },
    "manual": {
      "mirror_check": {
        "enabled": true,
        "look_angle_deg": 28.0,
        "pitch_offset_deg": 3.0,
        "smoothing_time": 0.35,
        "require_stationary": true,
        "ignore_after_moving_signal": true
      },
      "manual_look": {
        "enabled": true,
        "look_angle_deg": 45.0,
        "smoothing_time": 0.35,
        "toggle_mode": false
      },
      "manual_zoom": {
        "enabled": true,
        "zoom_fov_deg": 40.0,
        "smoothing_time": 0.25
      }
    }
  }
)json";
  api->Settings_SetJson(h, defaults);

  // --- Default System ---
  // Enabling/disabling effects is handled entirely through their "enabled"
  // checkbox in the settings UI.
  api->Defaults_SetLogging(h, "info", true);
  api->Defaults_SetLocalization(h, "en");
  api->Defaults_AddKeybind(h, "ManualLook", "look_left", "keyboard",
                           "KEY_DIVIDE", "always");
  api->Defaults_AddKeybind(h, "ManualLook", "look_right", "keyboard",
                           "KEY_MULTIPLY", "always");
  api->Defaults_AddKeybind(h, "ManualZoom", "zoom", "keyboard", "KEY_SUBTRACT",
                           "always");
  api->Defaults_AddKeybind(h, "UI", "toggle", "keyboard", "KEY_F9", "always");
  // isVisible=true here only decides the very first launch ever
  api->Defaults_AddWindow(h, "MotionCab", true, true, 100, 100, 476, 640, false,
                          true);

  // --- UI Metadata ---
  // Titles and descriptions are keys into localization/<lang>.json. A
  // setting's keys are its own path under "settings." plus ".title" /
  // ".desc", the scheme SettingsWindow.cpp relies on to find the same
  // strings. Every effect's "enabled" toggle shares one description.
  api->Meta_AddWindow(h, "MotionCab", "window.title", "window.desc");
  api->Meta_AddKeybind(h, "UI", "toggle", "keybinds.ui_toggle.title",
                       "keybinds.ui_toggle.desc");

  api->Meta_AddCustomSetting(h, "driving", "settings.driving.title",
                             "settings.driving.desc", nullptr, nullptr, false);

  api->Meta_AddCustomSetting(
      h, "driving.head_motion", "settings.driving.head_motion.title",
      "settings.driving.head_motion.desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(h, "driving.head_motion.enabled",
                             "settings.driving.head_motion.enabled.title",
                             "settings.enabled_desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(
      h, "driving.head_motion.sway_strength",
      "settings.driving.head_motion.sway_strength.title",
      "settings.driving.head_motion.sway_strength.desc", "slider",
      R"({ "min": 0.0, "max": 0.6, "format": "%.2f" })", false);
  api->Meta_AddCustomSetting(
      h, "driving.head_motion.tilt_strength",
      "settings.driving.head_motion.tilt_strength.title",
      "settings.driving.head_motion.tilt_strength.desc", "slider",
      R"({ "min": 0.0, "max": 0.6, "format": "%.2f" })", false);
  api->Meta_AddCustomSetting(
      h, "driving.head_motion.smoothing_time",
      "settings.driving.head_motion.smoothing_time.title",
      "settings.driving.head_motion.smoothing_time.desc", "slider",
      R"({ "min": 0.02, "max": 0.5, "format": "%.2f s" })", false);

  api->Meta_AddCustomSetting(
      h, "driving.steering_camera", "settings.driving.steering_camera.title",
      "settings.driving.steering_camera.desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(h, "driving.steering_camera.enabled",
                             "settings.driving.steering_camera.enabled.title",
                             "settings.enabled_desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(
      h, "driving.steering_camera.rotation_factor_deg",
      "settings.driving.steering_camera.rotation_factor_deg.title",
      "settings.driving.steering_camera.rotation_factor_deg.desc", "slider",
      R"({ "min": 20.0, "max": 60.0, "format": "%.1f deg" })", false);
  api->Meta_AddCustomSetting(
      h, "driving.steering_camera.smoothing_time",
      "settings.driving.steering_camera.smoothing_time.title",
      "settings.driving.steering_camera.smoothing_time.desc", "slider",
      R"({ "min": 0.02, "max": 1.0, "format": "%.2f s" })", false);
  api->Meta_AddCustomSetting(
      h, "driving.steering_camera.delay_seconds",
      "settings.driving.steering_camera.delay_seconds.title",
      "settings.driving.steering_camera.delay_seconds.desc", "slider",
      R"({ "min": 0.0, "max": 0.3, "format": "%.2f s" })", false);

  api->Meta_AddCustomSetting(h, "cabin", "settings.cabin.title",
                             "settings.cabin.desc", nullptr, nullptr, false);

  api->Meta_AddCustomSetting(
      h, "cabin.idle_breathing", "settings.cabin.idle_breathing.title",
      "settings.cabin.idle_breathing.desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(h, "cabin.idle_breathing.enabled",
                             "settings.cabin.idle_breathing.enabled.title",
                             "settings.enabled_desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(
      h, "cabin.idle_breathing.vertical_amplitude",
      "settings.cabin.idle_breathing.vertical_amplitude.title",
      "settings.cabin.idle_breathing.vertical_amplitude.desc", "slider",
      R"({ "min": 0.0, "max": 0.01, "format": "%.3f m" })", false);
  api->Meta_AddCustomSetting(
      h, "cabin.idle_breathing.pitch_amplitude_deg",
      "settings.cabin.idle_breathing.pitch_amplitude_deg.title",
      "settings.cabin.idle_breathing.pitch_amplitude_deg.desc", "slider",
      R"({ "min": 0.0, "max": 1.5, "format": "%.2f deg" })", false);
  api->Meta_AddCustomSetting(
      h, "cabin.idle_breathing.breathing_rate_bpm",
      "settings.cabin.idle_breathing.breathing_rate_bpm.title",
      "settings.cabin.idle_breathing.breathing_rate_bpm.desc", "slider",
      R"({ "min": 10.0, "max": 20.0, "format": "%.0f bpm" })", false);
  api->Meta_AddCustomSetting(
      h, "cabin.idle_breathing.fade_start_kmh",
      "settings.cabin.idle_breathing.fade_start_kmh.title",
      "settings.cabin.idle_breathing.fade_start_kmh.desc", "slider",
      R"({ "min": 0.0, "max": 100.0, "format": "%.0f km/h" })", false);
  api->Meta_AddCustomSetting(
      h, "cabin.idle_breathing.fade_end_kmh",
      "settings.cabin.idle_breathing.fade_end_kmh.title",
      "settings.cabin.idle_breathing.fade_end_kmh.desc", "slider",
      R"({ "min": 0.0, "max": 100.0, "format": "%.0f km/h" })", false);

  api->Meta_AddCustomSetting(h, "road", "settings.road.title",
                             "settings.road.desc", nullptr, nullptr, false);

  api->Meta_AddCustomSetting(
      h, "road.suspension", "settings.road.suspension.title",
      "settings.road.suspension.desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(
      h, "road.suspension.enabled", "settings.road.suspension.enabled.title",
      "settings.enabled_desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(
      h, "road.suspension.vertical_strength",
      "settings.road.suspension.vertical_strength.title",
      "settings.road.suspension.vertical_strength.desc", "slider",
      R"({ "min": 0.0, "max": 2.0, "format": "%.2f" })", false);
  api->Meta_AddCustomSetting(
      h, "road.suspension.reactivity",
      "settings.road.suspension.reactivity.title",
      "settings.road.suspension.reactivity.desc", "slider",
      R"({ "min": 0.02, "max": 0.3, "format": "%.2f s" })", false);
  api->Meta_AddCustomSetting(
      h, "road.suspension.grade_strength",
      "settings.road.suspension.grade_strength.title",
      "settings.road.suspension.grade_strength.desc", "slider",
      R"({ "min": 0.0, "max": 1.0, "format": "%.2f" })", false);

  api->Meta_AddCustomSetting(
      h, "cabin.engine_vibration", "settings.cabin.engine_vibration.title",
      "settings.cabin.engine_vibration.desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(h, "cabin.engine_vibration.enabled",
                             "settings.cabin.engine_vibration.enabled.title",
                             "settings.enabled_desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(
      h, "cabin.engine_vibration.intensity",
      "settings.cabin.engine_vibration.intensity.title",
      "settings.cabin.engine_vibration.intensity.desc", "slider",
      R"({ "min": 0.0, "max": 2.0, "format": "%.2f" })", false);

  api->Meta_AddCustomSetting(h, "manual", "settings.manual.title",
                             "settings.manual.desc", nullptr, nullptr, false);

  api->Meta_AddCustomSetting(
      h, "manual.mirror_check", "settings.manual.mirror_check.title",
      "settings.manual.mirror_check.desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(h, "manual.mirror_check.enabled",
                             "settings.manual.mirror_check.enabled.title",
                             "settings.enabled_desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(
      h, "manual.mirror_check.look_angle_deg",
      "settings.manual.mirror_check.look_angle_deg.title",
      "settings.manual.mirror_check.look_angle_deg.desc", "slider",
      R"({ "min": 15.0, "max": 50.0, "format": "%.0f deg" })", false);
  api->Meta_AddCustomSetting(
      h, "manual.mirror_check.pitch_offset_deg",
      "settings.manual.mirror_check.pitch_offset_deg.title",
      "settings.manual.mirror_check.pitch_offset_deg.desc", "slider",
      R"({ "min": 0.0, "max": 10.0, "format": "%.0f deg" })", false);
  api->Meta_AddCustomSetting(
      h, "manual.mirror_check.smoothing_time",
      "settings.manual.mirror_check.smoothing_time.title",
      "settings.manual.mirror_check.smoothing_time.desc", "slider",
      R"({ "min": 0.0, "max": 0.7, "format": "%.2f s" })", false);
  api->Meta_AddCustomSetting(
      h, "manual.mirror_check.require_stationary",
      "settings.manual.mirror_check.require_stationary.title",
      "settings.manual.mirror_check.require_stationary.desc", nullptr, nullptr,
      false);
  api->Meta_AddCustomSetting(
      h, "manual.mirror_check.ignore_after_moving_signal",
      "settings.manual.mirror_check.ignore_after_moving_signal.title",
      "settings.manual.mirror_check.ignore_after_moving_signal.desc", nullptr,
      nullptr, false);

  api->Meta_AddCustomSetting(
      h, "manual.manual_look", "settings.manual.manual_look.title",
      "settings.manual.manual_look.desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(h, "manual.manual_look.enabled",
                             "settings.manual.manual_look.enabled.title",
                             "settings.enabled_desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(
      h, "manual.manual_look.look_angle_deg",
      "settings.manual.manual_look.look_angle_deg.title",
      "settings.manual.manual_look.look_angle_deg.desc", "slider",
      R"({ "min": 20.0, "max": 90.0, "format": "%.0f deg" })", false);
  api->Meta_AddCustomSetting(
      h, "manual.manual_look.smoothing_time",
      "settings.manual.manual_look.smoothing_time.title",
      "settings.manual.manual_look.smoothing_time.desc", "slider",
      R"({ "min": 0.0, "max": 0.7, "format": "%.2f s" })", false);
  api->Meta_AddCustomSetting(
      h, "manual.manual_look.toggle_mode",
      "settings.manual.manual_look.toggle_mode.title",
      "settings.manual.manual_look.toggle_mode.desc", nullptr, nullptr, false);

  api->Meta_AddKeybind(h, "ManualLook", "look_left", "keybinds.look_left.title",
                       "keybinds.look_left.desc");
  api->Meta_AddKeybind(h, "ManualLook", "look_right",
                       "keybinds.look_right.title", "keybinds.look_right.desc");

  api->Meta_AddCustomSetting(
      h, "road.road_irregularity", "settings.road.road_irregularity.title",
      "settings.road.road_irregularity.desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(h, "road.road_irregularity.enabled",
                             "settings.road.road_irregularity.enabled.title",
                             "settings.enabled_desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(
      h, "road.road_irregularity.intensity",
      "settings.road.road_irregularity.intensity.title",
      "settings.road.road_irregularity.intensity.desc", "slider",
      R"({ "min": 0.0, "max": 2.0, "format": "%.2f" })", false);
  api->Meta_AddCustomSetting(
      h, "road.road_irregularity.reactivity",
      "settings.road.road_irregularity.reactivity.title",
      "settings.road.road_irregularity.reactivity.desc", "slider",
      R"({ "min": 0.02, "max": 0.2, "format": "%.2f s" })", false);

  api->Meta_AddCustomSetting(
      h, "road.speed_shake", "settings.road.speed_shake.title",
      "settings.road.speed_shake.desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(
      h, "road.speed_shake.enabled", "settings.road.speed_shake.enabled.title",
      "settings.enabled_desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(
      h, "road.speed_shake.intensity",
      "settings.road.speed_shake.intensity.title",
      "settings.road.speed_shake.intensity.desc", "slider",
      R"({ "min": 0.0, "max": 2.0, "format": "%.2f" })", false);
  api->Meta_AddCustomSetting(
      h, "road.speed_shake.smoothing_time",
      "settings.road.speed_shake.smoothing_time.title",
      "settings.road.speed_shake.smoothing_time.desc", "slider",
      R"({ "min": 0.1, "max": 1.0, "format": "%.2f s" })", false);
  api->Meta_AddCustomSetting(
      h, "road.speed_shake.rotation",
      "settings.road.speed_shake.rotation.title",
      "settings.road.speed_shake.rotation.desc", "slider",
      R"({ "min": 0.0, "max": 2.0, "format": "%.2f" })", false);
  api->Meta_AddCustomSetting(
      h, "road.speed_shake.vertical",
      "settings.road.speed_shake.vertical.title",
      "settings.road.speed_shake.vertical.desc", "slider",
      R"({ "min": 0.0, "max": 2.0, "format": "%.2f" })", false);
  api->Meta_AddCustomSetting(
      h, "road.speed_shake.roughness",
      "settings.road.speed_shake.roughness.title",
      "settings.road.speed_shake.roughness.desc", "slider",
      R"({ "min": 0.0, "max": 1.0, "format": "%.2f" })", false);

  api->Meta_AddCustomSetting(
      h, "driving.body_dynamics", "settings.driving.body_dynamics.title",
      "settings.driving.body_dynamics.desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(h, "driving.body_dynamics.enabled",
                             "settings.driving.body_dynamics.enabled.title",
                             "settings.enabled_desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(
      h, "driving.body_dynamics.lean_strength",
      "settings.driving.body_dynamics.lean_strength.title",
      "settings.driving.body_dynamics.lean_strength.desc", "slider",
      R"({ "min": 0.0, "max": 2.0, "format": "%.2f" })", false);
  api->Meta_AddCustomSetting(
      h, "driving.body_dynamics.nod_strength",
      "settings.driving.body_dynamics.nod_strength.title",
      "settings.driving.body_dynamics.nod_strength.desc", "slider",
      R"({ "min": 0.0, "max": 2.0, "format": "%.2f" })", false);
  api->Meta_AddCustomSetting(
      h, "driving.body_dynamics.smoothing_time",
      "settings.driving.body_dynamics.smoothing_time.title",
      "settings.driving.body_dynamics.smoothing_time.desc", "slider",
      R"({ "min": 0.05, "max": 0.6, "format": "%.2f s" })", false);

  api->Meta_AddCustomSetting(
      h, "cabin.engine_start_stop", "settings.cabin.engine_start_stop.title",
      "settings.cabin.engine_start_stop.desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(h, "cabin.engine_start_stop.enabled",
                             "settings.cabin.engine_start_stop.enabled.title",
                             "settings.enabled_desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(
      h, "cabin.engine_start_stop.intensity",
      "settings.cabin.engine_start_stop.intensity.title",
      "settings.cabin.engine_start_stop.intensity.desc", "slider",
      R"({ "min": 0.0, "max": 0.3, "format": "%.2f" })", false);
  api->Meta_AddCustomSetting(
      h, "cabin.engine_start_stop.duration",
      "settings.cabin.engine_start_stop.duration.title",
      "settings.cabin.engine_start_stop.duration.desc", "slider",
      R"({ "min": 0.3, "max": 2.0, "format": "%.2f s" })", false);

  api->Meta_AddCustomSetting(
      h, "manual.manual_zoom", "settings.manual.manual_zoom.title",
      "settings.manual.manual_zoom.desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(h, "manual.manual_zoom.enabled",
                             "settings.manual.manual_zoom.enabled.title",
                             "settings.enabled_desc", nullptr, nullptr, false);
  api->Meta_AddCustomSetting(
      h, "manual.manual_zoom.zoom_fov_deg",
      "settings.manual.manual_zoom.zoom_fov_deg.title",
      "settings.manual.manual_zoom.zoom_fov_deg.desc", "slider",
      R"({ "min": 5.0, "max": 60.0, "format": "%.0f deg" })", false);
  api->Meta_AddCustomSetting(
      h, "manual.manual_zoom.smoothing_time",
      "settings.manual.manual_zoom.smoothing_time.title",
      "settings.manual.manual_zoom.smoothing_time.desc", "slider",
      R"({ "min": 0.02, "max": 0.5, "format": "%.2f s" })", false);

  api->Meta_AddKeybind(h, "ManualZoom", "zoom", "keybinds.zoom.title",
                       "keybinds.zoom.desc");
}

} // namespace motioncab
