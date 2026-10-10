#pragma once

#include "SPF_UI_API.h"

namespace motioncab::ui {

void DrawAboutTab(SPF_UI_API *ui);

// Call from OnUnload(): SPF doesn't free a plugin's textures by itself, so
// it would leak for the rest of the session. A reload then creates it anew
// rather than reusing a dangling id.
void DestroyLogoTexture(SPF_UI_API *ui);

} // namespace motioncab::ui
