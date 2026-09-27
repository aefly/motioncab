#pragma once

#include "SPF_UI_API.h"

namespace motioncab {

// [TEMPORARY] The Quick Settings window's size on first launch (Manifest.cpp's
// Defaults_AddWindow). The player can't resize it: SettingsWindow.cpp
// keeps it at least kWindowHeight tall (taller if the About tab needs it)
// and fits its width to the tab titles.
inline constexpr int kWindowWidth = 476;
inline constexpr int kWindowHeight = 640;

// Draw callback for the "MotionCab" window declared in Manifest.cpp.
// Quick access to every effect's enabled toggle and sliders in one compact,
// collapsible window, as an alternative to navigating the native settings
// drawer. Reads/writes the same "settings.*" config keys as the native UI,
// so changes take effect immediately via the existing OnSettingChanged path.
void DrawSettingsWindow(SPF_UI_API *ui, void *user_data);

// Destroys the About tab's cached logo texture, if one was created. Called
// from OnUnload() so the texture doesn't outlive the plugin; SPF only frees
// a plugin texture when told to, not automatically on unload.
void ReleaseLogoTexture(SPF_UI_API *ui);

} // namespace motioncab
