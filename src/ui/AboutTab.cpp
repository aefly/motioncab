#include "AboutTab.hpp"

#include "SPF_Icons.h"
#include "core/Links.hpp"
#include "core/Loc.hpp"
#include "core/PluginContext.hpp"
#include "ui/OpenUrl.hpp"
#include "ui/Widgets.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

namespace motioncab::ui {

namespace {

// The tooltip shows the description and the address. A `width` of 0 fits
// the label.
void LinkButton(SPF_UI_API *ui, const std::string &label, float width,
                const char *url, const char *description) {
  const bool clicked = MutedButton(ui, label.c_str(), width);

  char tooltip[256];
  std::snprintf(tooltip, sizeof(tooltip), "%s\n%s", description, url);
  ui->UI_SetItemTooltip(tooltip);
  if (clicked)
    OpenUrl(url);
}

// From <plugin data dir>/logo.png. `id` stays null if the file is missing
// or can't be decoded, and the title falls back to plain text.
struct LogoTexture {
  void *id = nullptr;
  int width = 0;
  int height = 0;
};

// Not function-local, so DestroyLogoTexture can free it on unload.
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

} // namespace

void DestroyLogoTexture(SPF_UI_API *ui) {
  if (g_logo_texture.id && ui && ui->UI_DestroyTexture)
    ui->UI_DestroyTexture(g_logo_texture.id);
  g_logo_texture = LogoTexture{};
  g_logo_texture_tried = false;
}

void DrawAboutTab(SPF_UI_API *ui) {
  ui->UI_Dummy(0.0f, 6.0f);

  const LogoTexture &logo = GetLogoTexture(ui);
  if (logo.id && logo.width > 0 && logo.height > 0) {
    constexpr float kLogoBox = 64.0f; // aspect ratio kept
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

  // Styled text of its own, since Markdown can't center.
  SPF_TextStyle_Handle tagline_style = ui->UI_Style_Create();
  ui->UI_Style_SetColor(tagline_style, 0.63f, 0.63f, 0.63f, 1.0f);
  ui->UI_Style_SetAlign(tagline_style, SPF_TEXT_ALIGN_CENTER);
  ui->UI_TextStyled(tagline_style, "%s", loc::Tr("plugin.description"));
  ui->UI_Style_Destroy(tagline_style);

  ui->UI_Spacing();
  ui->UI_Spacing();
  DrawSectionTitle(ui, loc::Tr("ui.about.details"));
  ui->UI_Spacing();

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

  // Two buttons per row, filling the tab's width.
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

} // namespace motioncab::ui
