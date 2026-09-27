#include "SettingsWindow.hpp"

#include "SPF_Icons.h"
#include "core/Localization.hpp"
#include "core/PluginContext.hpp"
#include "core/ProfileManager.hpp"
#include "ui/AboutTab.hpp"
#include "ui/EffectTabs.hpp"
#include "ui/SettingsTab.hpp"
#include "ui/Widgets.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <string>

namespace motioncab {

using namespace ui;

namespace {

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
  UpdateLabelColumnWidth(ui);

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
    DrawEffectTab(ui, cfg, h, kTabs[i].id);
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
