#pragma once

#include "SPF_UI_API.h"

namespace motioncab::ui {

// Cabin Walk's per-truck cabin layout, folded under its settings
// ("Advanced"): the truck with a reset of the player's own layout, Go There
// for each seat, and a row per layout value.
void DrawCabinLayoutPanel(SPF_UI_API *ui);

} // namespace motioncab::ui
