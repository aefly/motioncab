#pragma once

#include "SPF_Config_API.h"
#include "SPF_UI_API.h"

#include <string_view>

// The effect tabs of the Quick Settings window (one per settings group).
namespace motioncab::ui {

// Sizes the settings tables' label column to the widest label in the
// active language. Call once per frame, before any DrawEffectTab().
void UpdateLabelColumnWidth(SPF_UI_API *ui);

// Every effect of settings group `group` ("driving", "road", ...), each in
// its own collapsing section. Returns the height the tab's content takes
// with every section folded (as the cursor's height after it: one
// ItemSpacing below the last header), whatever is expanded.
float DrawEffectTab(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h,
                    std::string_view group);

} // namespace motioncab::ui
