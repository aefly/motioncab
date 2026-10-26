#include "EffectTabs.hpp"

#include "SPF_Icons.h"
#include "core/Conflicts.hpp"
#include "core/Keybinds.hpp"
#include "core/Loc.hpp"
#include "core/Settings.hpp"
#include "core/Strings.hpp"
#include "ui/CabinLayoutPanel.hpp"
#include "ui/Widgets.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

namespace motioncab::ui {

namespace {

// Under the setting's own config key, plus ".title" / ".desc".
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

bool IsModified(SPF_Config_API *cfg, SPF_Config_Handle *h,
                const settings::Setting &setting) {
  if (setting.type == settings::Type::kBool)
    return cfg->Cfg_GetBool(h, setting.key, setting.default_bool()) !=
           setting.default_bool();
  const float value = static_cast<float>(
      cfg->Cfg_GetFloat(h, setting.key, setting.default_value));
  // Stored as a double: a value stepped back to its default may not land on
  // it exactly.
  return std::fabs(value - setting.default_value) >
         1e-4f * std::max(1.0f, std::fabs(setting.default_value));
}

// A bare icon button, for a table's value column where the label has its
// own. The config key doubles as a unique ID.
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

// Its label goes after the switch.
bool DrawEnabled(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h,
                 const settings::Setting &setting) {
  bool value = cfg->Cfg_GetBool(h, setting.key, setting.default_bool());
  if (DrawToggleCell(ui, setting.key, &value, loc::Tr("settings.enabled_desc")))
    cfg->Cfg_SetBool(h, setting.key, value);
  ui->UI_SameLine(0.0f, 8.0f);
  ui->UI_Text(loc::Tr("ui.enabled"));
  return value;
}

// The "###" ID keeps it open or closed across a language switch. Search
// results get IDs of their own, so they never fold or unfold the tabs'
// sections: a new search opens them all.
bool EffectHeader(SPF_UI_API *ui, const char *icon, const char *prefix,
                  const std::string &search_id = {}) {
  std::string label = WithIcon(icon, SettingTitle(prefix)) + "###";
  if (!search_id.empty()) {
    label += search_id;
    ui->UI_SetNextItemOpen(true, SPF_COND_ONCE);
  }
  label += prefix;
  return ui->UI_CollapsingHeader(label.c_str(), SPF_TREE_NODE_FLAG_NONE);
}

float g_label_column_w = 160.0f;

// So no label runs into its slider and every table's columns line up. The
// "enabled" toggles are drawn above the tables, not in them.
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
  return widest + ModifiedMarkWidth(ui);
}

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

void DrawBool(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h,
              const settings::Setting &setting) {
  bool value = cfg->Cfg_GetBool(h, setting.key, setting.default_bool());
  ui->UI_TableNextRow(SPF_TABLE_ROW_FLAG_NONE, 0.0f);
  ui->UI_TableSetColumnIndex(0);
  ui->UI_Text(SettingTitle(setting.key));
  if (IsModified(cfg, h, setting))
    DrawModifiedMark(ui, loc::Tr("ui.modified"));
  ui->UI_TableSetColumnIndex(1);
  if (DrawToggleCell(ui, setting.key, &value, SettingDesc(setting.key)))
    cfg->Cfg_SetBool(h, setting.key, value);
}

// Stepped by the format's last decimal. `display_scale` only changes what's
// shown, e.g. mph for a setting stored in km/h.
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
  if (IsModified(cfg, h, setting))
    DrawModifiedMark(ui, loc::Tr("ui.modified"));
  ui->UI_TableSetColumnIndex(1);
  if (StepperFloat(ui, key, &value, setting.min * display_scale,
                   setting.max * display_scale, FormatStep(format), format,
                   setting.default_value * display_scale,
                   SettingDesc(key)) != StepperEdit::kNone)
    cfg->Cfg_SetFloat(h, key, value / display_scale);
}

// In the language's unit: English is "imperial", since British and American
// players drive in mph.
void DrawSpeed(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h,
               const settings::Setting &setting) {
  constexpr float kMphPerKmh = 0.621371f;
  const bool imperial =
      std::string_view(loc::Tr("ui.units.speed_system")) == "imperial";
  // The unit comes from a translation and goes into a printf format.
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

void DrawKeybindRow(SPF_UI_API *ui, const keybinds::Action &action) {
  if (!KeybindUiAvailable())
    return;

  ui->UI_TableNextRow(SPF_TABLE_ROW_FLAG_NONE, 0.0f);
  ui->UI_TableSetColumnIndex(0);
  ui->UI_Text(KeybindTitle(action));
  ui->UI_TableSetColumnIndex(1);
  DrawKeybindButtons(ui, action.id);
}

// Its settings rows come from settings::kAll, after its keybinds.
struct EffectUi {
  const char *icon;
  const char *prefix; // "settings.<group>.<effect>"
  bool has_hint;      // "<prefix>.hint" under its toggle
  std::span<const keybinds::Action> keybinds;
  // For what doesn't fit a settings row.
  void (*extra)(SPF_UI_API *ui) = nullptr;
};

constexpr keybinds::Action kManualLookKeybinds[] = {
    keybinds::kLookLeft, keybinds::kLookRight, keybinds::kGlanceLeft,
    keybinds::kGlanceRight, keybinds::kLookForward};
constexpr keybinds::Action kCabinWalkKeybinds[] = {
    keybinds::kStandSit, keybinds::kWalkForward, keybinds::kWalkBack,
    keybinds::kWalkLeft, keybinds::kWalkRight,   keybinds::kCrouch};

// In display order.
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
    {ICON_FA_PERSON_RAYS, "settings.cabin.natural_head_movement", false, {}},
    {ICON_FA_ARROWS_LEFT_RIGHT, "settings.manual.manual_look", true,
     kManualLookKeybinds},
    {ICON_FA_MAGNIFYING_GLASS_PLUS,
     "settings.manual.manual_zoom",
     false,
     {&keybinds::kZoom, 1}},
    {ICON_FA_TRAFFIC_LIGHT,
     "settings.manual.blindspot_viewer",
     false,
     {&keybinds::kBlindspotPeek, 1}},
    {ICON_FA_PERSON_WALKING, "settings.manual.cabin_walk", true,
     kCabinWalkKeybinds, &DrawCabinLayoutPanel},
};

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

bool IsEffectEnabled(SPF_Config_API *cfg, SPF_Config_Handle *h,
                     std::string_view prefix) {
  const settings::Setting &toggle =
      *settings::Find(std::string(prefix) + ".enabled");
  return cfg->Cfg_GetBool(h, toggle.key, toggle.default_bool());
}

void DrawPausesHint(SPF_UI_API *ui, std::string_view prefix) {
  bool any = false;
  for (const conflicts::Pause &p : conflicts::kPauses) {
    if (prefix != p.effect)
      continue;
    if (!any) {
      DrawWrappedHint(ui, loc::Tr("settings.pauses"));
      ui->UI_PushStyleColor(SPF_COLOR_TEXT, 0.6f, 0.6f, 0.6f, 1.0f);
      any = true;
    }
    ui->UI_BulletText(SettingTitle(p.paused));
  }
  if (any)
    ui->UI_PopStyleColor(1);
}

// Returns whether it's paused.
bool DrawPausedByHint(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h,
                      std::string_view prefix) {
  std::string names;
  for (const conflicts::Pause &p : conflicts::kPauses) {
    if (prefix != p.paused || !IsEffectEnabled(cfg, h, p.effect))
      continue;
    if (!names.empty())
      names += ", ";
    names += SettingTitle(p.effect);
  }
  if (names.empty())
    return false;
  DrawWrappedHint(ui,
                  loc::Tr("settings.paused_by", {{"effects", names}}).c_str());
  return true;
}

// A search's words, folded. Empty when there's no search.
using SearchWords = std::vector<std::string>;

bool HasAllWords(std::string_view text, const SearchWords &words) {
  const std::string folded = strings::FoldCase(text);
  return std::ranges::all_of(words, [&](const std::string &word) {
    return folded.find(word) != std::string::npos;
  });
}

// The effect's title counts toward each of its rows.
bool RowMatches(const EffectUi &effect, std::string_view title,
                std::string_view desc, const SearchWords &words) {
  if (words.empty())
    return true;
  std::string text(SettingTitle(effect.prefix));
  text.append("\n").append(title).append("\n").append(desc);
  return HasAllWords(text, words);
}

// An effect whose title matches shows whole.
bool TitleMatches(const EffectUi &effect, const SearchWords &words) {
  return words.empty() || HasAllWords(SettingTitle(effect.prefix), words);
}

bool SettingRowMatches(const EffectUi &effect, const settings::Setting &s,
                       const SearchWords &words) {
  return RowMatches(effect, SettingTitle(s.key), SettingDesc(s.key), words);
}

bool KeybindRowMatches(const EffectUi &effect, const keybinds::Action &action,
                       const SearchWords &words) {
  return RowMatches(effect, KeybindTitle(action), {}, words);
}

bool EffectMatches(const EffectUi &effect, const SearchWords &words) {
  if (TitleMatches(effect, words))
    return true;
  if (KeybindUiAvailable() &&
      std::ranges::any_of(effect.keybinds, [&](const keybinds::Action &a) {
        return KeybindRowMatches(effect, a, words);
      }))
    return true;
  return std::ranges::any_of(settings::kAll, [&](const settings::Setting &s) {
    return settings::InEffect(s, effect.prefix) && !IsEnabledToggle(s) &&
           SettingRowMatches(effect, s, words);
  });
}

// With search words, only the rows that match, unless the title does.
void DrawEffect(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h,
                const EffectUi &effect, const SearchWords &words = {},
                const std::string &search_id = {}) {
  if (!EffectHeader(ui, effect.icon, effect.prefix, search_id))
    return;
  const bool whole = TitleMatches(effect, words);
  const SearchWords no_filter;
  const SearchWords &filter = whole ? no_filter : words;
  const std::string prefix(effect.prefix);
  const bool enabled =
      DrawEnabled(ui, cfg, h, *settings::Find(prefix + ".enabled"));
  if (effect.has_hint)
    DrawWrappedHint(ui, loc::Tr(prefix + ".hint"));
  DrawPausesHint(ui, effect.prefix);
  const bool paused = DrawPausedByHint(ui, cfg, h, effect.prefix);
  ui->UI_BeginDisabled(!enabled || paused);
  const std::string table_id = prefix.substr(prefix.rfind('.') + 1) + "_table";
  if (BeginSettingsTable(ui, table_id.c_str())) {
    for (const keybinds::Action &action : effect.keybinds) {
      if (KeybindRowMatches(effect, action, filter))
        DrawKeybindRow(ui, action);
    }
    for (const settings::Setting &s : settings::kAll) {
      if (!settings::InEffect(s, effect.prefix) || IsEnabledToggle(s) ||
          !SettingRowMatches(effect, s, filter))
        continue;
      DrawSettingRow(ui, cfg, h, s);
    }
    EndSettingsTable(ui);
  }
  if (effect.extra && whole)
    effect.extra(ui);
  ui->UI_EndDisabled();
}

char g_search[64] = "";
// Bumped as a search starts, for its sections' IDs.
unsigned g_search_generation = 0;

SearchWords SplitSearch(std::string_view query) {
  SearchWords words;
  const std::string folded = strings::FoldCase(query);
  size_t start = 0;
  while (start < folded.size()) {
    const size_t end = std::min(folded.find(' ', start), folded.size());
    if (end > start)
      words.push_back(folded.substr(start, end - start));
    start = end + 1;
  }
  return words;
}

} // namespace

bool DrawSearchBar(SPF_UI_API *ui) {
  const bool was_searching = !SplitSearch(g_search).empty();

  float avail_x = 0.0f, avail_y = 0.0f;
  ui->UI_GetContentRegionAvail(&avail_x, &avail_y);
  constexpr float kButtonGap = 8.0f;
  float clear_w = 0.0f;
  if (was_searching) {
    float pad_x = 0.0f, pad_y = 0.0f, text_w = 0.0f, text_h = 0.0f;
    ui->UI_Style_GetFramePadding(ui->UI_GetStyle(), &pad_x, &pad_y);
    ui->UI_CalcTextSize(ICON_FA_XMARK, &text_w, &text_h);
    clear_w = text_w + pad_x * 2.0f + kButtonGap;
  }

  const std::string hint =
      WithIcon(ICON_FA_MAGNIFYING_GLASS, loc::Tr("ui.search.hint"));
  ui->UI_SetNextItemWidth(std::max(1.0f, avail_x - clear_w));
  ui->UI_InputTextWithHint("##search", hint.c_str(), g_search, sizeof(g_search),
                           SPF_INPUT_TEXT_FLAG_NONE);
  if (was_searching) {
    ui->UI_SameLine(0.0f, kButtonGap);
    if (MutedButton(ui, ICON_FA_XMARK "##clear_search"))
      g_search[0] = '\0';
    ui->UI_SetItemTooltip(loc::Tr("ui.search.clear_tip"));
  }

  const bool searching = !SplitSearch(g_search).empty();
  if (searching && !was_searching)
    ++g_search_generation;
  return searching;
}

float DrawSearchResults(SPF_UI_API *ui, SPF_Config_API *cfg,
                        SPF_Config_Handle *h) {
  const SearchWords words = SplitSearch(g_search);
  const std::string search_id =
      "search" + std::to_string(g_search_generation) + "_";
  float folded_h = 0.0f;
  std::string_view group;
  for (const EffectUi &effect : kEffects) {
    if (!EffectMatches(effect, words))
      continue;
    // Under its tab's name, since the results mix every group.
    const std::string_view effect_group =
        settings::SplitKey(effect.prefix).group;
    if (effect_group != group) {
      group = effect_group;
      DrawSectionTitle(ui,
                       loc::Tr("settings." + std::string(group) + ".title"));
      folded_h += ui->UI_GetTextLineHeightWithSpacing();
    }
    DrawEffect(ui, cfg, h, effect, words, search_id);
    folded_h += ui->UI_GetFrameHeightWithSpacing();
  }
  if (group.empty()) {
    DrawWrappedHint(
        ui, loc::Tr("ui.search.no_results", {{"query", g_search}}).c_str());
    folded_h = ui->UI_GetCursorPosY();
  }
  return folded_h;
}

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
    // A folded section is its header alone.
    folded_h += ui->UI_GetFrameHeightWithSpacing();
  }
  return folded_h;
}

} // namespace motioncab::ui
