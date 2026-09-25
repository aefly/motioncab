#include "Manifest.hpp"

#include "Links.hpp"
#include "PluginContext.hpp"
#include "ui/SettingsText.hpp"

namespace motioncab {

void BuildManifest(SPF_Manifest_Builder_Handle *h,
                   const SPF_Manifest_Builder_API *api) {

  // --- Identity ---
  api->Info_SetName(h, PluginContext::kPluginName);
  api->Info_SetVersion(h, PLUGIN_VERSION);
  api->Info_SetAuthor(h, PLUGIN_AUTHOR);
  api->Info_SetDescriptionLiteral(h, PLUGIN_DESCRIPTION);
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
  api->Meta_AddWindow(h, "MotionCab", "MotionCab Quick Settings",
                      "Quick access to every effect's settings, as an "
                      "alternative to the Custom Settings tab.");
  api->Meta_AddKeybind(h, "UI", "toggle", "Toggle Settings Window",
                       "Shows or hides the MotionCab Quick Settings window.");
  api->Meta_AddCustomSetting(
      h, "driving", "Driving",
      "Head and camera reactions to how the truck moves and steers.", nullptr,
      nullptr, false);
  api->Meta_AddCustomSetting(h, "driving.head_motion", "Head Motion",
                             "Dynamic head-motion effect settings.", nullptr,
                             nullptr, false);
  api->Meta_AddCustomSetting(h, "driving.head_motion.enabled",
                             "Enable Head Motion", tip::kEnabled, nullptr,
                             nullptr, false);
  api->Meta_AddCustomSetting(
      h, "driving.head_motion.sway_strength", "Sway Strength",
      tip::kHeadMotionSwayStrength, "slider",
      R"({ "min": 0.0, "max": 0.6, "format": "%.2f" })", false);
  api->Meta_AddCustomSetting(
      h, "driving.head_motion.tilt_strength", "Tilt Strength",
      tip::kHeadMotionTiltStrength, "slider",
      R"({ "min": 0.0, "max": 0.6, "format": "%.2f" })", false);
  api->Meta_AddCustomSetting(
      h, "driving.head_motion.smoothing_time", "Smoothing",
      tip::kHeadMotionSmoothing, "slider",
      R"({ "min": 0.02, "max": 0.5, "format": "%.2f s" })", false);

  api->Meta_AddCustomSetting(
      h, "driving.steering_camera", "Steering Camera",
      "Smoothed camera rotation following the steering wheel.", nullptr,
      nullptr, false);
  api->Meta_AddCustomSetting(h, "driving.steering_camera.enabled",
                             "Enable Steering Camera", tip::kEnabled, nullptr,
                             nullptr, false);
  api->Meta_AddCustomSetting(
      h, "driving.steering_camera.rotation_factor_deg", "Rotation Amount",
      tip::kSteeringCameraRotationAmount, "slider",
      R"({ "min": 20.0, "max": 60.0, "format": "%.1f deg" })", false);
  api->Meta_AddCustomSetting(
      h, "driving.steering_camera.smoothing_time", "Smoothing",
      tip::kSteeringCameraSmoothing, "slider",
      R"({ "min": 0.02, "max": 1.0, "format": "%.2f s" })", false);
  api->Meta_AddCustomSetting(
      h, "driving.steering_camera.delay_seconds", "Reaction Delay",
      tip::kSteeringCameraReactionDelay, "slider",
      R"({ "min": 0.0, "max": 0.3, "format": "%.2f s" })", false);

  api->Meta_AddCustomSetting(
      h, "cabin", "Cabin",
      "Effects from the driver and the engine inside the cabin.", nullptr,
      nullptr, false);
  api->Meta_AddCustomSetting(
      h, "cabin.idle_breathing", "Idle Breathing",
      "Subtle idle breathing motion that fades out as speed increases.",
      nullptr, nullptr, false);
  api->Meta_AddCustomSetting(h, "cabin.idle_breathing.enabled",
                             "Enable Idle Breathing", tip::kEnabled, nullptr,
                             nullptr, false);
  api->Meta_AddCustomSetting(
      h, "cabin.idle_breathing.vertical_amplitude", "Vertical Amount",
      tip::kIdleBreathingVerticalAmount, "slider",
      R"({ "min": 0.0, "max": 0.01, "format": "%.3f m" })", false);
  api->Meta_AddCustomSetting(
      h, "cabin.idle_breathing.pitch_amplitude_deg", "Head Nod Amount",
      tip::kIdleBreathingHeadNodAmount, "slider",
      R"({ "min": 0.0, "max": 1.5, "format": "%.2f deg" })", false);
  api->Meta_AddCustomSetting(
      h, "cabin.idle_breathing.breathing_rate_bpm", "Breathing Rate",
      tip::kIdleBreathingRate, "slider",
      R"({ "min": 10.0, "max": 20.0, "format": "%.0f bpm" })", false);
  api->Meta_AddCustomSetting(
      h, "cabin.idle_breathing.fade_start_kmh", "Fade Start Speed",
      tip::kIdleBreathingFadeStart, "slider",
      R"({ "min": 0.0, "max": 100.0, "format": "%.0f km/h" })", false);
  api->Meta_AddCustomSetting(
      h, "cabin.idle_breathing.fade_end_kmh", "Fade End Speed",
      tip::kIdleBreathingFadeEnd, "slider",
      R"({ "min": 0.0, "max": 100.0, "format": "%.0f km/h" })", false);

  api->Meta_AddCustomSetting(
      h, "road", "Road",
      "Effects driven by the road surface, suspension and speed.", nullptr,
      nullptr, false);
  api->Meta_AddCustomSetting(h, "road.suspension", "Suspension",
                             "Camera follows the road surface vertically, like "
                             "a seat riding the truck's suspension.",
                             nullptr, nullptr, false);
  api->Meta_AddCustomSetting(h, "road.suspension.enabled", "Enable Suspension",
                             tip::kEnabled, nullptr, nullptr, false);
  api->Meta_AddCustomSetting(
      h, "road.suspension.vertical_strength", "Vertical Strength",
      tip::kSuspensionVerticalStrength, "slider",
      R"({ "min": 0.0, "max": 2.0, "format": "%.2f" })", false);
  api->Meta_AddCustomSetting(
      h, "road.suspension.reactivity", "Reactivity", tip::kSuspensionReactivity,
      "slider", R"({ "min": 0.02, "max": 0.3, "format": "%.2f s" })", false);
  api->Meta_AddCustomSetting(
      h, "road.suspension.grade_strength", "Grade Follow",
      tip::kSuspensionGradeStrength, "slider",
      R"({ "min": 0.0, "max": 1.0, "format": "%.2f" })", false);

  api->Meta_AddCustomSetting(h, "cabin.engine_vibration", "Engine Vibration",
                             "Fine engine vibration scaled by RPM. "
                             "Automatically disabled on electric trucks.",
                             nullptr, nullptr, false);
  api->Meta_AddCustomSetting(h, "cabin.engine_vibration.enabled",
                             "Enable Engine Vibration", tip::kEnabled, nullptr,
                             nullptr, false);
  api->Meta_AddCustomSetting(h, "cabin.engine_vibration.intensity", "Intensity",
                             tip::kEngineVibrationIntensity, "slider",
                             R"({ "min": 0.0, "max": 2.0, "format": "%.2f" })",
                             false);

  api->Meta_AddCustomSetting(h, "manual", "Manual",
                             "Look and zoom effects you trigger yourself.",
                             nullptr, nullptr, false);
  api->Meta_AddCustomSetting(
      h, "manual.mirror_check", "Mirror Check",
      "Looks toward the corresponding mirror while a turn signal is on, and "
      "recenters when it's off.",
      nullptr, nullptr, false);
  api->Meta_AddCustomSetting(h, "manual.mirror_check.enabled",
                             "Enable Mirror Check", tip::kEnabled, nullptr,
                             nullptr, false);
  api->Meta_AddCustomSetting(
      h, "manual.mirror_check.look_angle_deg", "Look Angle",
      tip::kMirrorCheckLookAngle, "slider",
      R"({ "min": 15.0, "max": 50.0, "format": "%.0f deg" })", false);
  api->Meta_AddCustomSetting(
      h, "manual.mirror_check.pitch_offset_deg", "Pitch Offset",
      tip::kMirrorCheckPitchOffset, "slider",
      R"({ "min": 0.0, "max": 10.0, "format": "%.0f deg" })", false);
  api->Meta_AddCustomSetting(
      h, "manual.mirror_check.smoothing_time", "Smoothing",
      tip::kMirrorCheckSmoothing, "slider",
      R"({ "min": 0.0, "max": 0.7, "format": "%.2f s" })", false);
  api->Meta_AddCustomSetting(
      h, "manual.mirror_check.require_stationary", "Only When Stationary",
      tip::kMirrorCheckRequireStationary, nullptr, nullptr, false);
  api->Meta_AddCustomSetting(
      h, "manual.mirror_check.ignore_after_moving_signal",
      "Ignore While Moving", tip::kMirrorCheckIgnoreAfterMovingSignal, nullptr,
      nullptr, false);

  api->Meta_AddCustomSetting(
      h, "manual.manual_look", "Manual Look",
      "Smooth manual look-left/look-right glances bound to two keys.", nullptr,
      nullptr, false);
  api->Meta_AddCustomSetting(h, "manual.manual_look.enabled",
                             "Enable Manual Look", tip::kEnabled, nullptr,
                             nullptr, false);
  api->Meta_AddCustomSetting(
      h, "manual.manual_look.look_angle_deg", "Look Angle",
      tip::kManualLookLookAngle, "slider",
      R"({ "min": 20.0, "max": 90.0, "format": "%.0f deg" })", false);
  api->Meta_AddCustomSetting(
      h, "manual.manual_look.smoothing_time", "Smoothing",
      tip::kManualLookSmoothing, "slider",
      R"({ "min": 0.0, "max": 0.7, "format": "%.2f s" })", false);
  api->Meta_AddCustomSetting(h, "manual.manual_look.toggle_mode", "Toggle Mode",
                             tip::kManualLookToggleMode, nullptr, nullptr,
                             false);

  api->Meta_AddKeybind(h, "ManualLook", "look_left", "Look Left",
                       "Smoothly looks toward the left window/mirror.");
  api->Meta_AddKeybind(h, "ManualLook", "look_right", "Look Right",
                       "Smoothly looks toward the right window/mirror.");

  api->Meta_AddCustomSetting(
      h, "road.road_irregularity", "Road Irregularity",
      "Textured chatter based on the ground material under the wheels "
      "(gravel, cobblestone, dirt vs. smooth asphalt), separate from real "
      "suspension bumps. Uneven ground like dirt or grass also rocks your "
      "head from side to side.",
      nullptr, nullptr, false);
  api->Meta_AddCustomSetting(h, "road.road_irregularity.enabled",
                             "Enable Road Irregularity", tip::kEnabled, nullptr,
                             nullptr, false);
  api->Meta_AddCustomSetting(h, "road.road_irregularity.intensity", "Intensity",
                             tip::kRoadIrregularityIntensity, "slider",
                             R"({ "min": 0.0, "max": 2.0, "format": "%.2f" })",
                             false);
  api->Meta_AddCustomSetting(
      h, "road.road_irregularity.reactivity", "Reactivity",
      tip::kRoadIrregularityReactivity, "slider",
      R"({ "min": 0.02, "max": 0.2, "format": "%.2f s" })", false);

  api->Meta_AddCustomSetting(
      h, "road.speed_shake", "Speed Shake",
      "Realistic head sway while driving, growing with truck speed.", nullptr,
      nullptr, false);
  api->Meta_AddCustomSetting(h, "road.speed_shake.enabled",
                             "Enable Speed Shake", tip::kEnabled, nullptr,
                             nullptr, false);
  api->Meta_AddCustomSetting(
      h, "road.speed_shake.intensity", "Intensity", tip::kSpeedShakeIntensity,
      "slider", R"({ "min": 0.0, "max": 2.0, "format": "%.2f" })", false);
  api->Meta_AddCustomSetting(
      h, "road.speed_shake.smoothing_time", "Smoothing",
      tip::kSpeedShakeSmoothing, "slider",
      R"({ "min": 0.1, "max": 1.0, "format": "%.2f s" })", false);
  api->Meta_AddCustomSetting(
      h, "road.speed_shake.rotation", "Rotation", tip::kSpeedShakeRotation,
      "slider", R"({ "min": 0.0, "max": 2.0, "format": "%.2f" })", false);
  api->Meta_AddCustomSetting(
      h, "road.speed_shake.vertical", "Vertical", tip::kSpeedShakeVertical,
      "slider", R"({ "min": 0.0, "max": 2.0, "format": "%.2f" })", false);
  api->Meta_AddCustomSetting(
      h, "road.speed_shake.roughness", "Roughness", tip::kSpeedShakeRoughness,
      "slider", R"({ "min": 0.0, "max": 1.0, "format": "%.2f" })", false);

  api->Meta_AddCustomSetting(
      h, "driving.body_dynamics", "Body Dynamics",
      "Head leans toward the outside of corners and nods forward when "
      "braking, like a driver's body reacting to the truck's motion.",
      nullptr, nullptr, false);
  api->Meta_AddCustomSetting(h, "driving.body_dynamics.enabled",
                             "Enable Body Dynamics", tip::kEnabled, nullptr,
                             nullptr, false);
  api->Meta_AddCustomSetting(
      h, "driving.body_dynamics.lean_strength", "Lean Strength",
      tip::kBodyDynamicsLeanStrength, "slider",
      R"({ "min": 0.0, "max": 2.0, "format": "%.2f" })", false);
  api->Meta_AddCustomSetting(
      h, "driving.body_dynamics.nod_strength", "Nod Strength",
      tip::kBodyDynamicsNodStrength, "slider",
      R"({ "min": 0.0, "max": 2.0, "format": "%.2f" })", false);
  api->Meta_AddCustomSetting(
      h, "driving.body_dynamics.smoothing_time", "Smoothing",
      tip::kBodyDynamicsSmoothing, "slider",
      R"({ "min": 0.05, "max": 0.6, "format": "%.2f s" })", false);

  api->Meta_AddCustomSetting(
      h, "cabin.engine_start_stop", "Engine Start/Stop",
      "A short mechanical shudder when the engine starts or dies. "
      "Automatically disabled on electric trucks.",
      nullptr, nullptr, false);
  api->Meta_AddCustomSetting(h, "cabin.engine_start_stop.enabled",
                             "Enable Engine Start/Stop", tip::kEnabled, nullptr,
                             nullptr, false);
  api->Meta_AddCustomSetting(
      h, "cabin.engine_start_stop.intensity", "Intensity",
      tip::kEngineStartStopIntensity, "slider",
      R"({ "min": 0.0, "max": 0.3, "format": "%.2f" })", false);
  api->Meta_AddCustomSetting(
      h, "cabin.engine_start_stop.duration", "Duration",
      tip::kEngineStartStopDuration, "slider",
      R"({ "min": 0.3, "max": 2.0, "format": "%.2f s" })", false);

  api->Meta_AddCustomSetting(
      h, "manual.manual_zoom", "Manual Zoom",
      "Smooth zoom on a single dedicated key, like the native zoom key: "
      "hold to zoom in, release to ease back to normal. Independent of "
      "the native zoom keys, which jump the FOV in fixed steps and can't "
      "be smoothed directly.",
      nullptr, nullptr, false);
  api->Meta_AddCustomSetting(h, "manual.manual_zoom.enabled",
                             "Enable Manual Zoom", tip::kEnabled, nullptr,
                             nullptr, false);
  api->Meta_AddCustomSetting(
      h, "manual.manual_zoom.zoom_fov_deg", "Zoom Level",
      tip::kManualZoomZoomLevel, "slider",
      R"({ "min": 5.0, "max": 60.0, "format": "%.0f deg" })", false);
  api->Meta_AddCustomSetting(
      h, "manual.manual_zoom.smoothing_time", "Smoothing",
      tip::kManualZoomSmoothing, "slider",
      R"({ "min": 0.02, "max": 0.5, "format": "%.2f s" })", false);

  api->Meta_AddKeybind(
      h, "ManualZoom", "zoom", "Zoom",
      "Hold to smoothly zoom in; release to ease back to normal.");
}

} // namespace motioncab
