#include "SettingsWindow.hpp"

#include "SPF_Icons.h"
#include "core/Keybinds.hpp"
#include "core/Links.hpp"
#include "core/Localization.hpp"
#include "core/PluginContext.hpp"
#include "core/ProfileManager.hpp"
#include "core/SettingsSchema.hpp"
#include "ui/OpenUrl.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <span>
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
              const settings::Setting &setting) {
  bool value = cfg->Cfg_GetBool(h, setting.key, setting.default_bool());
  ui->UI_TableNextRow(SPF_TABLE_ROW_FLAG_NONE, 0.0f);
  ui->UI_TableSetColumnIndex(0);
  ui->UI_Text(SettingTitle(setting.key));
  ui->UI_TableSetColumnIndex(1);
  if (DrawToggleCell(ui, setting.key, &value, SettingDesc(setting.key)))
    cfg->Cfg_SetBool(h, setting.key, value);
}

// One label|slider row. Right-click the slider to reset it to default.
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
  char hidden_label[112];
  std::snprintf(hidden_label, sizeof(hidden_label), "##%s", key);
  ui->UI_SetNextItemWidth(-1.0f);
  if (ui->UI_SliderFloat(hidden_label, &value, setting.min * display_scale,
                         setting.max * display_scale, format,
                         SPF_SLIDER_FLAG_NONE))
    cfg->Cfg_SetFloat(h, key, value / display_scale);
  if (ui->UI_IsItemClicked(SPF_MOUSE_BUTTON_RIGHT))
    cfg->Cfg_SetFloat(h, key, setting.default_value);
  ui->UI_SetItemTooltip(SettingDesc(key));
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
void DrawKeybindRow(SPF_UI_API *ui, const keybinds::Action &action) {
  if (!KeybindUiAvailable())
    return;

  ui->UI_TableNextRow(SPF_TABLE_ROW_FLAG_NONE, 0.0f);
  ui->UI_TableSetColumnIndex(0);
  ui->UI_Text(KeybindTitle(action));
  ui->UI_TableSetColumnIndex(1);
  DrawKeybindButtons(ui, action.id);
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

// One effect's section in its tab. Its settings rows come from
// settings::kAll, in that order, after its keybinds.
struct EffectUi {
  const char *icon;
  const char *prefix; // "settings.<group>.<effect>"
  bool has_hint;      // shows the "<prefix>.hint" text under its toggle
  std::span<const keybinds::Action> keybinds;
};

constexpr keybinds::Action kManualLookKeybinds[] = {keybinds::kLookLeft,
                                                    keybinds::kLookRight};

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
      if (settings::InEffect(s, effect.prefix) && !IsEnabledToggle(s))
        DrawSettingRow(ui, cfg, h, s);
    }
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
          ShowToast(
              ui, SPF_NOTIFICATION_ERROR,
              loc::Tr("ui.profiles.delete_failed", {{"name", pending_delete}}));
        } else {
          ShowToast(
              ui, SPF_NOTIFICATION_SUCCESS,
              loc::Tr("ui.profiles.deleted_toast", {{"name", pending_delete}}));
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
      settings::WriteDefaults(cfg, h);
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

  if (!ui->UI_CreateTextureFromFile)
    return g_logo_texture;
  const std::string dir = Context().PluginDataDir();
  if (dir.empty())
    return g_logo_texture;

  const std::string path = dir + "/logo.png";
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

  LinkButton(ui, WithIcon(ICON_FA_GLOBE, loc::Tr("ui.about.website")), button_w,
             links::kWebsite, loc::Tr("ui.about.website_desc"));
  ui->UI_SameLine(0.0f, kButtonGap);
  LinkButton(ui, WithIcon(ICON_FA_DISCORD, loc::Tr("ui.about.discord")),
             button_w, links::kDiscord, loc::Tr("ui.about.discord_desc"));
  LinkButton(ui, WithIcon(ICON_FA_GITHUB, loc::Tr("ui.about.github")), button_w,
             links::kGithub, loc::Tr("ui.about.github_desc"));
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
  const std::string settings_tab = WithIcon(
      ICON_FA_GEAR, "**" + std::string(loc::Tr("ui.tabs.settings")) + "**");
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
  DrawKeybindRowAt(ui, keybinds::kToggleWindow.id, toggle_label,
                   field_w + kGap);

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
// The first tabs, one per settings group; the rest (Settings, About) aren't
// effect tabs.
constexpr size_t kEffectTabCount = 4;

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
// shrinks them itself, for the frame until FitWindowSize widens the
// window. Call right before UI_BeginTabBar.
std::array<float, kTabCount> StretchTabs(SPF_UI_API *ui, const TabMetrics &m) {
  std::array<float, kTabCount> widths{};
  float avail_x = 0.0f, avail_y = 0.0f;
  ui->UI_GetContentRegionAvail(&avail_x, &avail_y);
  const int spare = static_cast<int>(std::floor(avail_x - m.total));
  if (spare < 0)
    return widths;
  const int count = static_cast<int>(kTabCount);
  for (int i = 0; i < count; ++i) {
    const int extra_px = spare / count + (i < spare % count ? 1 : 0);
    widths[i] = m.width[i] + static_cast<float>(extra_px);
  }
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
    return ui->UI_BeginTabItem(label.c_str(), nullptr, SPF_TAB_ITEM_FLAG_NONE);
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

// The window height at which the About tab's content exactly fits, as of
// the last frame it was open; 0 until then. It depends on the language
// (text length and wrapping), so FitWindowSize grows the window to it
// rather than leaving About with a scrollbar.
float g_about_fit_h = 0.0f;

// Starts the scrolling region a tab's content is drawn in, filling the
// window's height below the tab bar (minus the footer line, if the tab has
// one). Expanding a section then shows a scrollbar in it, and the tab bar
// stays in view. Pair with UI_EndChild.
void BeginTabContent(SPF_UI_API *ui, const TabInfo &tab, bool with_footer) {
  float avail_x = 0.0f, avail_y = 0.0f;
  ui->UI_GetContentRegionAvail(&avail_x, &avail_y);
  const float footer_h =
      with_footer ? ui->UI_GetTextLineHeightWithSpacing() : 0.0f;
  const std::string id = std::string("##content_") + tab.id;
  ui->UI_BeginChild(id.c_str(), 0.0f,
                    std::max(1.0f, std::floor(avail_y - footer_h)), false,
                    SPF_WINDOW_FLAG_NONE);
}

// The window height at which the content drawn so far in a tab's region
// exactly fits it. Call inside the region, after its content, with
// `region_top` the window-local Y the region started at: a borderless child
// has no padding, and the cursor sits one ItemSpacing below the last item,
// in content coordinates (unaffected by the scroll position).
float ContentFitHeight(SPF_UI_API *ui, float region_top) {
  SPF_Style_Handle *style = ui->UI_GetStyle();
  float spacing_x = 0.0f, spacing_y = 0.0f, pad_x = 0.0f, pad_y = 0.0f;
  ui->UI_Style_GetItemSpacing(style, &spacing_x, &spacing_y);
  ui->UI_Style_GetWindowPadding(style, &pad_x, &pad_y);
  return std::ceil(region_top + ui->UI_GetCursorPosY() - spacing_y + pad_y);
}

// [TEMPORARY] Sizes the window, which the player can't resize (see OnRegisterUI
// in Plugin.cpp): exactly wide enough for the tab titles, so none gets cut with
// "..." (many translations are longer than English), and kWindowHeight tall, or
// taller if the About tab needs it in this language (g_about_fit_h), but never
// taller than the screen. The other tabs' content scrolls within it.
void FitWindowSize(SPF_UI_API *ui, const TabMetrics &m) {
  float win_pad_x = 0.0f, win_pad_y = 0.0f;
  ui->UI_Style_GetWindowPadding(ui->UI_GetStyle(), &win_pad_x, &win_pad_y);
  // ImGui only shrinks tabs once they overflow the window's width minus
  // WindowPadding on both sides by 1 px or more, and everything is whole
  // pixels, so this is the exact width (the tab content scrolls in its own
  // region, so its scrollbar never narrows the tab bar).
  const float target_w = std::round(m.total + win_pad_x * 2.0f);

  float target_h = std::max(static_cast<float>(kWindowHeight), g_about_fit_h);
  float vp_w = 0.0f, vp_h = 0.0f;
  ui->UI_GetMainViewportSize(&vp_w, &vp_h);
  if (vp_h > 0.0f)
    target_h = std::min(target_h, std::floor(vp_h));

  float win_w = 0.0f, win_h = 0.0f;
  ui->UI_GetWindowSize(&win_w, &win_h);
  if (std::fabs(win_w - target_w) < 0.5f && std::fabs(win_h - target_h) < 0.5f)
    return;
  ui->UI_SetWindowSize(target_w, target_h, SPF_COND_ALWAYS);
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
  FitWindowSize(ui, tab_metrics);
  const std::array<float, kTabCount> tab_widths = StretchTabs(ui, tab_metrics);
  if (!ui->UI_BeginTabBar("MotionCabTabs", SPF_TAB_BAR_FLAG_NONE)) {
    ui->UI_PopStyleVar(pushed_vars);
    ui->UI_PopStyleColor(pushed_colors);
    return;
  }

  // The effect tabs: each one's id is its settings group.
  for (size_t i = 0; i < kEffectTabCount; ++i) {
    if (!BeginTab(ui, kTabs[i], tab_widths[i], tab_metrics))
      continue;
    BeginTabContent(ui, kTabs[i], true);
    const std::string group_prefix =
        std::string("settings.") + kTabs[i].id + ".";
    for (const EffectUi &effect : kEffects) {
      if (std::string_view(effect.prefix).starts_with(group_prefix))
        DrawEffect(ui, cfg, h, effect);
    }
    ui->UI_EndChild();
    ui->UI_EndTabItem();
  }

  const bool settings_tab_open =
      BeginTab(ui, kTabs[4], tab_widths[4], tab_metrics);
  if (settings_tab_open) {
    BeginTabContent(ui, kTabs[4], false);
    DrawSettingsTab(ui, cfg, h);
    ui->UI_EndChild();
    ui->UI_EndTabItem();
  }

  const bool about_tab_open =
      BeginTab(ui, kTabs[5], tab_widths[5], tab_metrics);
  if (about_tab_open) {
    const float region_top = ui->UI_GetCursorPosY();
    BeginTabContent(ui, kTabs[5], false);
    DrawAboutTab(ui);
    g_about_fit_h = ContentFitHeight(ui, region_top);
    ui->UI_EndChild();
    ui->UI_EndTabItem();
  }

  ui->UI_EndTabBar();

  if (!settings_tab_open && !about_tab_open)
    ui->UI_TextDisabled(
        WithIcon(ICON_FA_CIRCLE_INFO, loc::Tr("ui.slider_reset_hint")).c_str());

  ui->UI_PopStyleVar(pushed_vars);
  ui->UI_PopStyleColor(pushed_colors);
}

void ReleaseLogoTexture(SPF_UI_API *ui) { DestroyLogoTexture(ui); }

} // namespace motioncab
