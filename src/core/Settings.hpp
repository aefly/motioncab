#pragma once

#include "SPF_Config_API.h"

#include <string>
#include <string_view>

// Every MotionCab setting, declared once: the manifest's defaults, the Quick
// Settings window, the profiles and the effects' own defaults all come from
// this table.
namespace motioncab::settings {

enum class Type { kBool, kFloat };

struct Setting {
  const char *key; // "settings.<group>.<effect>.<name>"
  Type type;
  float default_value; // 0 or 1 for a bool
  // Float only: the slider the Quick Settings window shows.
  float min = 0.0f;
  float max = 0.0f;
  const char *format = nullptr; // printf format of the slider's value
  // Stored in km/h, but shown by the Quick Settings window in the active
  // language's unit (km/h or mph).
  bool is_speed = false;

  constexpr bool default_bool() const { return default_value != 0.0f; }
};

constexpr Setting Bool(const char *key, bool default_value) {
  return {key, Type::kBool, default_value ? 1.0f : 0.0f};
}

constexpr Setting Float(const char *key, float default_value, float min,
                        float max, const char *format) {
  return {key, Type::kFloat, default_value, min, max, format};
}

constexpr Setting Speed(const char *key, float default_value, float min,
                        float max) {
  return {key, Type::kFloat, default_value, min, max, "%.0f km/h", true};
}

// In the order of the manifest's default settings JSON. Each group's and
// each effect's settings must stay together (checked below).
inline constexpr Setting kAll[] = {
    // driving.head_motion
    Bool("settings.driving.head_motion.enabled", true),
    Float("settings.driving.head_motion.sway_strength", 0.2f, 0.0f, 0.6f,
          "%.2f"),
    Float("settings.driving.head_motion.tilt_strength", 0.15f, 0.0f, 0.6f,
          "%.2f"),
    Float("settings.driving.head_motion.smoothing_time", 0.2f, 0.02f, 0.5f,
          "%.2f s"),
    // driving.body_dynamics
    Bool("settings.driving.body_dynamics.enabled", true),
    Float("settings.driving.body_dynamics.lean_strength", 1.0f, 0.0f, 2.0f,
          "%.2f"),
    Float("settings.driving.body_dynamics.nod_strength", 1.0f, 0.0f, 2.0f,
          "%.2f"),
    Float("settings.driving.body_dynamics.smoothing_time", 0.25f, 0.05f, 0.6f,
          "%.2f s"),
    // driving.steering_camera
    Bool("settings.driving.steering_camera.enabled", true),
    Float("settings.driving.steering_camera.rotation_left_deg", 30.0f, 20.0f,
          60.0f, "%.1f deg"),
    Float("settings.driving.steering_camera.rotation_right_deg", 30.0f, 20.0f,
          60.0f, "%.1f deg"),
    Float("settings.driving.steering_camera.center_zone_pct", 15.0f, 0.0f,
          50.0f, "%.0f%%"),
    Float("settings.driving.steering_camera.smoothing_time", 0.5f, 0.02f, 1.0f,
          "%.2f s"),
    Float("settings.driving.steering_camera.delay_seconds", 0.2f, 0.0f, 0.3f,
          "%.2f s"),
    Bool("settings.driving.steering_camera.disable_in_reverse", true),
    // road.suspension
    Bool("settings.road.suspension.enabled", true),
    Float("settings.road.suspension.vertical_strength", 1.0f, 0.0f, 2.0f,
          "%.2f"),
    Float("settings.road.suspension.reactivity", 0.25f, 0.1f, 0.3f, "%.2f s"),
    Float("settings.road.suspension.grade_strength", 0.5f, 0.0f, 1.0f, "%.2f"),
    // road.road_irregularity
    Bool("settings.road.road_irregularity.enabled", true),
    Float("settings.road.road_irregularity.intensity", 1.0f, 0.0f, 2.0f,
          "%.2f"),
    Float("settings.road.road_irregularity.reactivity", 0.05f, 0.02f, 0.2f,
          "%.2f s"),
    // road.speed_shake
    Bool("settings.road.speed_shake.enabled", true),
    Float("settings.road.speed_shake.intensity", 1.0f, 0.0f, 2.0f, "%.2f"),
    Float("settings.road.speed_shake.smoothing_time", 0.3f, 0.1f, 1.0f,
          "%.2f s"),
    Float("settings.road.speed_shake.rotation", 1.0f, 0.0f, 2.0f, "%.2f"),
    Float("settings.road.speed_shake.vertical", 1.0f, 0.0f, 2.0f, "%.2f"),
    Float("settings.road.speed_shake.roughness", 0.5f, 0.0f, 1.0f, "%.2f"),
    // cabin.idle_breathing
    Bool("settings.cabin.idle_breathing.enabled", true),
    Float("settings.cabin.idle_breathing.vertical_amplitude", 0.002f, 0.0f,
          0.01f, "%.3f m"),
    Float("settings.cabin.idle_breathing.pitch_amplitude_deg", 0.15f, 0.0f,
          1.5f, "%.2f deg"),
    Float("settings.cabin.idle_breathing.breathing_rate_bpm", 15.0f, 10.0f,
          20.0f, "%.0f bpm"),
    Speed("settings.cabin.idle_breathing.fade_start_kmh", 30.0f, 0.0f, 100.0f),
    Speed("settings.cabin.idle_breathing.fade_end_kmh", 50.0f, 0.0f, 100.0f),
    // cabin.engine_vibration
    Bool("settings.cabin.engine_vibration.enabled", true),
    Float("settings.cabin.engine_vibration.intensity", 1.0f, 0.0f, 2.0f,
          "%.2f"),
    // cabin.engine_start_stop
    Bool("settings.cabin.engine_start_stop.enabled", true),
    Float("settings.cabin.engine_start_stop.intensity", 0.1f, 0.0f, 0.3f,
          "%.2f"),
    Float("settings.cabin.engine_start_stop.duration", 1.0f, 0.3f, 2.0f,
          "%.2f s"),
    // cabin.natural_head_movement
    Bool("settings.cabin.natural_head_movement.enabled", false),
    Float("settings.cabin.natural_head_movement.tremor_intensity", 5.0f, 0.0f,
          8.0f, "%.2f"),
    Float("settings.cabin.natural_head_movement.micro_intensity", 2.5f, 0.0f,
          5.0f, "%.2f"),
    Float("settings.cabin.natural_head_movement.posture_intensity", 1.3f, 0.0f,
          2.0f, "%.2f"),
    Float("settings.cabin.natural_head_movement.posture_interval", 15.0f, 5.0f,
          60.0f, "%.0f s"),
    Float("settings.cabin.natural_head_movement.steering_tilt", 1.5f, 0.0f,
          3.0f, "%.2f"),
    // manual.mirror_check
    Bool("settings.manual.mirror_check.enabled", true),
    Float("settings.manual.mirror_check.left_angle_deg", 28.0f, 15.0f, 50.0f,
          "%.0f deg"),
    Float("settings.manual.mirror_check.left_pitch_deg", 0.0f, -10.0f, 10.0f,
          "%.0f deg"),
    Float("settings.manual.mirror_check.left_fov_deg", 0.0f, -20.0f, 20.0f,
          "%.0f deg"),
    Float("settings.manual.mirror_check.right_angle_deg", 28.0f, 15.0f, 50.0f,
          "%.0f deg"),
    Float("settings.manual.mirror_check.right_pitch_deg", 0.0f, -10.0f, 10.0f,
          "%.0f deg"),
    Float("settings.manual.mirror_check.right_fov_deg", 0.0f, -20.0f, 20.0f,
          "%.0f deg"),
    Float("settings.manual.mirror_check.smoothing_time", 0.35f, 0.0f, 0.7f,
          "%.2f s"),
    Bool("settings.manual.mirror_check.require_stationary", true),
    Bool("settings.manual.mirror_check.ignore_after_moving_signal", true),
    // manual.manual_look
    Bool("settings.manual.manual_look.enabled", true),
    Float("settings.manual.manual_look.look_left_deg", 45.0f, 20.0f, 90.0f,
          "%.0f deg"),
    Float("settings.manual.manual_look.look_right_deg", 45.0f, 20.0f, 90.0f,
          "%.0f deg"),
    Float("settings.manual.manual_look.glance_left_deg", 20.0f, 5.0f, 45.0f,
          "%.0f deg"),
    Float("settings.manual.manual_look.glance_left_pitch_deg", 0.0f, -10.0f,
          10.0f, "%.0f deg"),
    Float("settings.manual.manual_look.glance_left_fov_deg", 0.0f, -20.0f,
          20.0f, "%.0f deg"),
    Float("settings.manual.manual_look.glance_right_deg", 30.0f, 5.0f, 45.0f,
          "%.0f deg"),
    Float("settings.manual.manual_look.glance_right_pitch_deg", 0.0f, -10.0f,
          10.0f, "%.0f deg"),
    Float("settings.manual.manual_look.glance_right_fov_deg", 0.0f, -20.0f,
          20.0f, "%.0f deg"),
    Float("settings.manual.manual_look.smoothing_time", 0.35f, 0.0f, 0.7f,
          "%.2f s"),
    Bool("settings.manual.manual_look.toggle_mode", false),
    // manual.manual_zoom
    Bool("settings.manual.manual_zoom.enabled", true),
    Float("settings.manual.manual_zoom.zoom_fov_deg", 40.0f, 5.0f, 60.0f,
          "%.0f deg"),
    Float("settings.manual.manual_zoom.smoothing_time", 0.25f, 0.02f, 0.5f,
          "%.2f s"),
    // manual.blindspot_viewer
    Bool("settings.manual.blindspot_viewer.enabled", true),
    Float("settings.manual.blindspot_viewer.pos_x", -0.06f, -0.5f, 0.5f,
          "%.2f m"),
    Float("settings.manual.blindspot_viewer.pos_y", -0.1f, -0.5f, 0.5f,
          "%.2f m"),
    Float("settings.manual.blindspot_viewer.pos_z", -0.88f, -1.5f, 0.5f,
          "%.2f m"),
    Float("settings.manual.blindspot_viewer.yaw_deg", -1.7f, -45.0f, 45.0f,
          "%.1f deg"),
    Float("settings.manual.blindspot_viewer.pitch_deg", 33.0f, -60.0f, 60.0f,
          "%.1f deg"),
    Float("settings.manual.blindspot_viewer.roll_deg", 3.0f, -15.0f, 15.0f,
          "%.1f deg"),
    Float("settings.manual.blindspot_viewer.fov_offset_deg", 10.0f, -30.0f,
          30.0f, "%.0f deg"),
    Float("settings.manual.blindspot_viewer.smoothing_time", 0.6f, 0.2f, 1.5f,
          "%.2f s"),
    Bool("settings.manual.blindspot_viewer.toggle_mode", true),
    // manual.cabin_walk
    Bool("settings.manual.cabin_walk.enabled", true),
    Bool("settings.manual.cabin_walk.require_parking_brake", true),
};

// "settings.<group>.<effect>.<name>" split into its parts.
struct KeyParts {
  std::string_view group, effect, name;
};

constexpr KeyParts SplitKey(std::string_view key) {
  key.remove_prefix(std::string_view("settings.").size());
  const size_t group_end = key.find('.');
  const size_t effect_end = key.find('.', group_end + 1);
  return {key.substr(0, group_end),
          key.substr(group_end + 1, effect_end - group_end - 1),
          key.substr(effect_end + 1)};
}

// True if `setting` belongs to the effect whose settings live under
// `effect_prefix` ("settings.<group>.<effect>").
constexpr bool InEffect(const Setting &setting,
                        std::string_view effect_prefix) {
  const std::string_view key(setting.key);
  return key.size() > effect_prefix.size() && key.starts_with(effect_prefix) &&
         key[effect_prefix.size()] == '.';
}

// The manifest's JSON needs each group's and each
// effect's settings in one run: a group or effect seen again after another
// one would be a duplicate JSON key.
// Each key is split once: splitting them all again for every pair runs
// past clang's constant evaluation step limit.
consteval bool SettingsAreGrouped() {
  constexpr size_t n = std::size(kAll);
  KeyParts parts[n];
  for (size_t i = 0; i < n; ++i)
    parts[i] = SplitKey(kAll[i].key);
  for (size_t i = 1; i < n; ++i) {
    const KeyParts &cur = parts[i];
    const KeyParts &prev = parts[i - 1];
    const bool new_group = cur.group != prev.group;
    const bool new_effect = new_group || cur.effect != prev.effect;
    for (size_t j = 0; j + 1 < i; ++j) {
      const KeyParts &old = parts[j];
      if (new_group && old.group == cur.group)
        return false;
      if (new_effect && old.group == cur.group && old.effect == cur.effect)
        return false;
    }
  }
  return true;
}
static_assert(SettingsAreGrouped(),
              "keep each group's and each effect's settings together");

// Default value of `key`, checked at compile time: an unknown key doesn't
// compile. For the effects' member initializers.
consteval float Default(std::string_view key) {
  for (const Setting &s : kAll) {
    if (key == s.key && s.type == Type::kFloat)
      return s.default_value;
  }
  throw "unknown float setting key";
}

// Slider minimum of `key`, checked the same way: for an effect to floor a
// value saved before the range was narrowed.
consteval float Min(std::string_view key) {
  for (const Setting &s : kAll) {
    if (key == s.key && s.type == Type::kFloat)
      return s.min;
  }
  throw "unknown float setting key";
}

consteval bool DefaultBool(std::string_view key) {
  for (const Setting &s : kAll) {
    if (key == s.key && s.type == Type::kBool)
      return s.default_bool();
  }
  throw "unknown bool setting key";
}

// The setting with this key, or nullptr.
const Setting *Find(std::string_view key);

// The manifest's default settings JSON (Settings_SetJson), without the
// "settings." root.
std::string DefaultsJson();

// Writes every setting's default value into `handle`: the live config for
// "Reset to Defaults", or a new profile file.
void WriteDefaults(SPF_Config_API *cfg, SPF_Config_Handle *handle);

} // namespace motioncab::settings
