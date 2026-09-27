#pragma once

#include "SPF_Config_API.h"
#include "SPF_UI_API.h"

namespace motioncab::ui {

// The Quick Settings window's Settings tab: its own keybind, the profiles
// and the global reset.
void DrawSettingsTab(SPF_UI_API *ui, SPF_Config_API *cfg,
                     SPF_Config_Handle *h);

} // namespace motioncab::ui
