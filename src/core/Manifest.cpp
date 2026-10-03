#include "Manifest.hpp"

#include "Keybinds.hpp"
#include "Links.hpp"
#include "PluginContext.hpp"
#include "SettingsSchema.hpp"
#include "ui/SettingsWindow.hpp"

#include <string>

namespace motioncab {

namespace {

// Titles and descriptions are keys into localization/<lang>.json. A
// setting's keys are its own path under "settings." plus ".title" /
// ".desc", the scheme ui/EffectTabs.cpp relies on to find the same
// strings; groups and effects follow it too, with their own path. Every
// effect's "enabled" toggle shares one description. SPF attaches each
// entry to the JSON node at its path, so the order of these calls doesn't
// matter: the native UI follows the defaults JSON.
void AddSettingsMetadata(SPF_Manifest_Builder_Handle *h,
                         const SPF_Manifest_Builder_API *api) {
  auto add_section = [&](const std::string &path) {
    const std::string loc_key = "settings." + path;
    api->Meta_AddCustomSetting(h, path.c_str(), (loc_key + ".title").c_str(),
                               (loc_key + ".desc").c_str(), nullptr, nullptr,
                               false);
  };

  std::string_view group, effect;
  for (const settings::Setting &s : settings::kAll) {
    const settings::KeyParts parts = settings::SplitKey(s.key);
    if (parts.group != group)
      add_section(std::string(parts.group));
    if (parts.group != group || parts.effect != effect)
      add_section(std::string(parts.group) + "." + std::string(parts.effect));
    group = parts.group;
    effect = parts.effect;

    const std::string key(s.key);
    const std::string path = key.substr(std::string_view("settings.").size());
    const std::string desc =
        parts.name == "enabled" ? "settings.enabled_desc" : key + ".desc";
    if (s.type == settings::Type::kFloat) {
      api->Meta_AddCustomSetting(h, path.c_str(), (key + ".title").c_str(),
                                 desc.c_str(), "slider",
                                 settings::SliderParamsJson(s).c_str(), false);
    } else {
      api->Meta_AddCustomSetting(h, path.c_str(), (key + ".title").c_str(),
                                 desc.c_str(), nullptr, nullptr, false);
    }
  }
}

} // namespace

void BuildManifest(SPF_Manifest_Builder_Handle *h,
                   const SPF_Manifest_Builder_API *api) {

  // --- Identity ---
  api->Info_SetName(h, PluginContext::kPluginName);
  api->Info_SetVersion(h, PLUGIN_VERSION);
  api->Info_SetAuthor(h, PLUGIN_AUTHOR);
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
  AddSettingsMetadata(h, api);
}

} // namespace motioncab
