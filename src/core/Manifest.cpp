#include "Manifest.hpp"

#include "Keybinds.hpp"
#include "Links.hpp"
#include "PluginContext.hpp"
#include "SettingsSchema.hpp"
#include "ui/SettingsWindow.hpp"

#include <string>

namespace motioncab {

void BuildManifest(SPF_Manifest_Builder_Handle *h,
                   const SPF_Manifest_Builder_API *api) {

  // --- Identity ---
  api->Info_SetName(h, PluginContext::kPluginName);
  api->Info_SetVersion(h, PLUGIN_VERSION);
  api->Info_SetAuthor(h, PLUGIN_AUTHOR);
  api->Info_SetDescriptionKey(h, "plugin.description");
  api->Info_SetMinFrameworkVersion(h, "1.2.5");
  api->Info_SetWebsiteUrl(h, links::kWebsite);
  api->Info_SetGithubUrl(h, links::kGithub);
  api->Info_SetYoutubeUrl(h, links::kYoutube);
  api->Info_SetDiscordUrl(h, links::kDiscord);

  // --- Configuration Policy ---
  // No "settings" system: MotionCab's settings are only shown in its own
  // Quick Settings window (ui/SettingsWindow.cpp), not in SPF's native
  // settings UI. SPF still saves them, since user config is allowed.
  api->Policy_SetAllowUserConfig(h, true);
  api->Policy_AddConfigurableSystem(h, "logging");
  api->Policy_AddConfigurableSystem(h, "ui");
  api->Policy_AddConfigurableSystem(h, "localization");

  // --- Default Settings ---
  // Generated from settings::kAll (core/SettingsSchema.hpp).
  api->Settings_SetJson(h, settings::DefaultsJson().c_str());

  // --- Default System ---
  // Enabling/disabling effects is handled entirely through their "enabled"
  // checkbox in the settings UI.
  api->Defaults_SetLogging(h, "info", true);
  api->Defaults_SetLocalization(h, "en");
  for (const keybinds::Action &action : keybinds::kAllActions)
    api->Defaults_AddKeybind(
        h, action.group, action.name, "keyboard", action.default_key,
        keybinds::IsWalkAction(action) ? "manual" : "always");
  // isVisible=true here only decides the very first launch ever
  api->Defaults_AddWindow(h, "MotionCab", true, true, 100, 100, kWindowWidth,
                          kWindowHeight, false, true);

  // --- UI Metadata ---
  api->Meta_AddWindow(h, "MotionCab", "window.title", "window.desc");
  for (const keybinds::Action &action : keybinds::kAllActions) {
    const std::string loc_key(action.loc_key);
    api->Meta_AddKeybind(h, action.group, action.name,
                         (loc_key + ".title").c_str(),
                         (loc_key + ".desc").c_str());
  }
}

} // namespace motioncab
