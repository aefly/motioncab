#pragma once

#include "SPF_UI_API.h"

namespace motioncab::ui {

// On first launch. The width then snaps to the tab titles and never gets
// narrower than them.
inline constexpr int kWindowWidth = 476;
inline constexpr int kWindowHeight = 640;

// The only place the settings are shown: the manifest keeps them out of
// SPF's own settings UI.
void DrawSettingsWindow(SPF_UI_API *ui, void *user_data);

// SPF doesn't free a plugin's textures on unload by itself.
void ReleaseLogoTexture(SPF_UI_API *ui);

} // namespace motioncab::ui
