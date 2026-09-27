#pragma once

#include "SPF_UI_API.h"

namespace motioncab::ui {

// The Quick Settings window's About tab: logo, version, links, tips and
// credits.
void DrawAboutTab(SPF_UI_API *ui);

// SPF only frees a texture created via UI_CreateTextureFromFile/FromMemory
// when explicitly told to; it doesn't track plugin lifetime, so this must
// be called from OnUnload() or the texture leaks for the rest of the game
// session. Resets the cache too, so a plugin reload (without a full DLL
// unload) re-creates it instead of returning a dangling id.
void DestroyLogoTexture(SPF_UI_API *ui);

} // namespace motioncab::ui
