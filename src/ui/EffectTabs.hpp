#pragma once

#include "SPF_Config_API.h"
#include "SPF_UI_API.h"

#include <string_view>

// One tab per settings group, and the search across them.
namespace motioncab::ui {

// To the widest label in the active language. Call once per frame, before
// any DrawEffectTab().
void UpdateLabelColumnWidth(SPF_UI_API *ui);

// Returns the height the tab would take with every section folded, whatever
// is expanded.
float DrawEffectTab(SPF_UI_API *ui, SPF_Config_API *cfg, SPF_Config_Handle *h,
                    std::string_view group);

// Returns whether a search is typed in, to draw DrawSearchResults() in place
// of the tabs.
bool DrawSearchBar(SPF_UI_API *ui);

// Every effect of every group with a title, setting or keybind matching each
// word of the search, in any case. Returns the height it would take with
// every section folded, like DrawEffectTab().
float DrawSearchResults(SPF_UI_API *ui, SPF_Config_API *cfg,
                        SPF_Config_Handle *h);

} // namespace motioncab::ui
