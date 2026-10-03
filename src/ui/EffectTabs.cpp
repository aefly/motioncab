#include "EffectTabs.hpp"

#include "SPF_Icons.h"
#include "core/Keybinds.hpp"
#include "core/Localization.hpp"
#include "core/SettingsSchema.hpp"
#include "ui/CabinLayoutPanel.hpp"
#include "ui/Widgets.hpp"

#include <algorithm>
#include <cstdio>
#include <span>
#include <string>

namespace motioncab::ui {

namespace {

// A setting's title and description live under its own config key, plus
// ".title" / ".desc".
const char *SettingTitle(const char *key) {
  return loc::Tr(std::string(key) + ".title");
}

const char *SettingDesc(const char *key) {
  return loc::Tr(std::string(key) + ".desc");
}

const char *KeybindTitle(const keybinds::Action &action) {
  return loc::Tr(std::string(action.loc_key) + ".title");
}

bool IsEnabledToggle(const settings::Setting &setting) {
  return settings::SplitKey(setting.key).name == "enabled";
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
                 const settings::Setting &setting) {
  bool value = cfg->Cfg_GetBool(h, setting.key, setting.default_bool());
  if (DrawToggleCell(ui, setting.key, &value, loc::Tr("settings.enabled_desc")))
    cfg->Cfg_SetBool(h, setting.key, value);
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
// are the settings (bar the effects' "enabled" toggles, drawn above the
// table) and the polled keybinds.
float LabelColumnWidth(SPF_UI_API *ui) {
  float widest = 0.0f;
  auto measure = [&](const char *text) {
    float w = 0.0f, h = 0.0f;
    ui->UI_CalcTextSize(text, &w, &h);
    widest = std::max(widest, w);
  };
  for (const settings::Setting &s : settings::kAll) {
    if (!IsEnabledToggle(s))
      measure(SettingTitle(s.key));
  }
  for (const keybinds::Action &action : keybinds::kPolledActions)
    measure(KeybindTitle(action));
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

// One label|toggle row.
void DrawBool(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h,
              const settings::Setting &setting) {
  bool value = cfg->Cfg_GetBool(h, setting.key, setting.default_bool());
  ui->UI_TableNextRow(SPF_TABLE_ROW_FLAG_NONE, 0.0f);
  ui->UI_TableSetColumnIndex(0);
  ui->UI_Text(SettingTitle(setting.key));
  ui->UI_TableSetColumnIndex(1);
  if (DrawToggleCell(ui, setting.key, &value, SettingDesc(setting.key)))
    cfg->Cfg_SetBool(h, setting.key, value);
}

// One label|[-] slider [+] row (StepperFloat), stepped by the format's last
// decimal. Right-click the slider to reset it to default.
// `display_scale` shows the value (and the slider's range) multiplied by
// it, e.g. to show a setting stored in km/h in mph; the stored value
// doesn't change.
void DrawSlider(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h,
                const settings::Setting &setting, const char *format,
                float display_scale) {
  const char *key = setting.key;
  float value =
      static_cast<float>(cfg->Cfg_GetFloat(h, key, setting.default_value)) *
      display_scale;
  ui->UI_TableNextRow(SPF_TABLE_ROW_FLAG_NONE, 0.0f);
  ui->UI_TableSetColumnIndex(0);
  ui->UI_Text(SettingTitle(key));
  ui->UI_TableSetColumnIndex(1);
  if (StepperFloat(ui, key, &value, setting.min * display_scale,
                   setting.max * display_scale, FormatStep(format), format,
                   setting.default_value * display_scale,
                   SettingDesc(key)) != StepperEdit::kNone)
    cfg->Cfg_SetFloat(h, key, value / display_scale);
}

// A slider row for a speed setting, stored in km/h but shown in the
// active language's unit ("ui.units.speed_system": mph for "imperial",
// e.g. English, since British and American players drive in mph).
void DrawSpeed(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h,
               const settings::Setting &setting) {
  constexpr float kMphPerKmh = 0.621371f;
  const bool imperial =
      std::string_view(loc::Tr("ui.units.speed_system")) == "imperial";
  // The unit comes from a translation, so escape any '%' before it goes
  // into a printf-style format.
  std::string format = "%.0f ";
  for (const char *c = loc::Tr(imperial ? "ui.units.mph" : "ui.units.kmh"); *c;
       ++c)
    format += *c == '%' ? std::string("%%") : std::string(1, *c);
  DrawSlider(ui, cfg, h, setting, format.c_str(), imperial ? kMphPerKmh : 1.0f);
}

void DrawSettingRow(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h,
                    const settings::Setting &setting) {
  if (setting.type == settings::Type::kBool)
    DrawBool(ui, cfg, h, setting);
  else if (setting.is_speed)
    DrawSpeed(ui, cfg, h, setting);
  else
    DrawSlider(ui, cfg, h, setting, setting.format, 1.0f);
}

// One label|bindings row for a keybind action, inside a settings table.
void DrawKeybindRow(SPF_UI_API *ui, const keybinds::Action &action) {
  if (!KeybindUiAvailable())
    return;

  ui->UI_TableNextRow(SPF_TABLE_ROW_FLAG_NONE, 0.0f);
  ui->UI_TableSetColumnIndex(0);
  ui->UI_Text(KeybindTitle(action));
  ui->UI_TableSetColumnIndex(1);
  DrawKeybindButtons(ui, action.id);
}

// One effect's section in its tab. Its settings rows come from
// settings::kAll, in that order, after its keybinds.
struct EffectUi {
  const char *icon;
  const char *prefix; // "settings.<group>.<effect>"
  bool has_hint;      // shows the "<prefix>.hint" text under its toggle
  std::span<const keybinds::Action> keybinds;
  // Drawn under its settings, for what doesn't fit a settings row.
  void (*extra)(SPF_UI_API *ui) = nullptr;
};

constexpr keybinds::Action kManualLookKeybinds[] = {
    keybinds::kLookLeft, keybinds::kLookRight, keybinds::kGlanceLeft,
    keybinds::kGlanceRight};
constexpr keybinds::Action kCabinWalkKeybinds[] = {
    keybinds::kStandSit, keybinds::kWalkForward, keybinds::kWalkBack,
    keybinds::kWalkLeft, keybinds::kWalkRight,   keybinds::kCrouch};

// Every effect, in display order; each tab shows its own group's.
constexpr EffectUi kEffects[] = {
    {ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT,
     "settings.driving.head_motion",
     false,
     {}},
    {ICON_FA_PERSON, "settings.driving.body_dynamics", false, {}},
    {ICON_FA_ROTATE, "settings.driving.steering_camera", false, {}},
    {ICON_FA_ARROWS_UP_DOWN, "settings.road.suspension", false, {}},
    {ICON_FA_ROAD, "settings.road.road_irregularity", false, {}},
    {ICON_FA_TRUCK, "settings.road.speed_shake", false, {}},
    {ICON_FA_LUNGS, "settings.cabin.idle_breathing", false, {}},
    {ICON_FA_WAVE_SQUARE, "settings.cabin.engine_vibration", false, {}},
    {ICON_FA_POWER_OFF, "settings.cabin.engine_start_stop", false, {}},
    {ICON_FA_EYE, "settings.manual.mirror_check", true, {}},
    {ICON_FA_ARROWS_LEFT_RIGHT, "settings.manual.manual_look", true,
     kManualLookKeybinds},
    {ICON_FA_MAGNIFYING_GLASS_PLUS,
     "settings.manual.manual_zoom",
     true,
     {&keybinds::kZoom, 1}},
    {ICON_FA_TRAFFIC_LIGHT,
     "settings.manual.blindspot_viewer",
     false,
     {&keybinds::kBlindspotPeek, 1}},
    {ICON_FA_PERSON_WALKING, "settings.manual.cabin_walk", true,
     kCabinWalkKeybinds, &DrawCabinLayoutPanel},
};

// Every effect section needs its "enabled" toggle in settings::kAll.
consteval bool EffectsHaveEnabledToggle() {
  for (const EffectUi &effect : kEffects) {
    bool found = false;
    for (const settings::Setting &s : settings::kAll) {
      if (settings::InEffect(s, effect.prefix) &&
          settings::SplitKey(s.key).name == "enabled")
        found = true;
    }
    if (!found)
      return false;
  }
  return true;
}

static_assert(EffectsHaveEnabledToggle(),
              "every effect in kEffects needs an \"enabled\" setting");

void DrawEffect(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h,
                const EffectUi &effect) {
  if (!EffectHeader(ui, effect.icon, effect.prefix))
    return;
  const std::string prefix(effect.prefix);
  const bool enabled =
      DrawEnabled(ui, cfg, h, *settings::Find(prefix + ".enabled"));
  if (effect.has_hint)
    DrawWrappedHint(ui, loc::Tr(prefix + ".hint"));
  ui->UI_BeginDisabled(!enabled);
  // "<effect>_table", unique per effect.
  const std::string table_id = prefix.substr(prefix.rfind('.') + 1) + "_table";
  if (BeginSettingsTable(ui, table_id.c_str())) {
    for (const keybinds::Action &action : effect.keybinds)
      DrawKeybindRow(ui, action);
    for (const settings::Setting &s : settings::kAll) {
      if (!settings::InEffect(s, effect.prefix) || IsEnabledToggle(s))
        continue;
      DrawSettingRow(ui, cfg, h, s);
    }
    EndSettingsTable(ui);
  }
  if (effect.extra)
    effect.extra(ui);
  ui->UI_EndDisabled();
}

} // namespace

void UpdateLabelColumnWidth(SPF_UI_API *ui) {
  g_label_column_w = LabelColumnWidth(ui);
}

float DrawEffectTab(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h,
                    std::string_view group) {
  const std::string group_prefix = "settings." + std::string(group) + ".";
  float folded_h = 0.0f;
  for (const EffectUi &effect : kEffects) {
    if (!std::string_view(effect.prefix).starts_with(group_prefix))
      continue;
    DrawEffect(ui, cfg, h, effect);
    // A folded section is its header alone, one frame high.
    folded_h += ui->UI_GetFrameHeightWithSpacing();
  }
  return folded_h;
}

} // namespace motioncab::ui
