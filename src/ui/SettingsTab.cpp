#include "SettingsTab.hpp"

#include "SPF_Icons.h"
#include "core/Keybinds.hpp"
#include "core/Loc.hpp"
#include "core/PluginContext.hpp"
#include "core/TruckSettings.hpp"
#include "ui/Widgets.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace motioncab::ui {

namespace {

// The popup's title is translated, so it goes by a "###" ID.
constexpr const char *kResetPopupId = "###confirm_reset_all";

// The truck the settings belong to, in the brand color: every change goes to
// it alone. Dimmed while they can't be changed.
void DrawTruckBanner(SPF_UI_API *ui) {
  const EditBlock block = SettingsEditBlock();
  const bool has_truck = block == EditBlock::kNone;
  const std::string text =
      WithIcon(ICON_FA_TRUCK,
               block == EditBlock::kNoTruck ? loc::Tr("ui.no_truck")
               : block == EditBlock::kPaused
                   ? loc::Tr("ui.paused")
                   : loc::Tr("ui.truck_badge",
                             {{"name", truck_settings::CurrentTruckName()}}));

  float x = 0.0f, y = 0.0f, avail_x = 0.0f, avail_y = 0.0f;
  ui->UI_GetCursorScreenPos(&x, &y);
  ui->UI_GetContentRegionAvail(&avail_x, &avail_y);
  const float pad = Px(ui, 6.0f);
  const float bar_w = Px(ui, 3.0f);
  const float h = ui->UI_GetTextLineHeight() + pad * 2.0f;

  const SPF_DrawList_Handle dl = ui->UI_GetWindowDrawList();
  ui->UI_DrawList_AddRectFilled(
      dl, x, y, x + avail_x, y + h,
      ui->UI_ColorConvertFloat4ToU32(kAccentR, kAccentG, kAccentB,
                                     has_truck ? 0.18f : 0.06f),
      Px(ui, 4.0f), SPF_DRAW_FLAG_NONE);
  ui->UI_DrawList_AddRectFilled(
      dl, x, y, x + bar_w, y + h,
      ui->UI_ColorConvertFloat4ToU32(kAccentR, kAccentG, kAccentB,
                                     has_truck ? 1.0f : 0.35f),
      Px(ui, 4.0f), SPF_DRAW_FLAG_ROUND_CORNERS_LEFT);

  ui->UI_SetCursorScreenPos(x + bar_w + pad * 1.5f, y + pad);
  if (has_truck)
    ui->UI_Text(text.c_str());
  else
    ui->UI_TextDisabled(text.c_str());
  // Takes the banner's whole height in the layout.
  ui->UI_SetCursorScreenPos(x, y);
  ui->UI_Dummy(avail_x, h);
}

// The truck the reset popup is about: the current one, or one of the list.
struct PendingReset {
  std::string key;
  std::string name;
};
PendingReset g_pending_reset;

void AskReset(SPF_UI_API *ui, std::string key, std::string name) {
  g_pending_reset = {std::move(key), std::move(name)};
  ui->UI_OpenPopup(kResetPopupId, SPF_POPUP_FLAG_NONE);
}

// Every truck with settings of its own, the one being driven on top. Past
// kVisibleRows the list scrolls on its own, so a long one doesn't push the
// rest of the page away.
void DrawTunedTrucks(SPF_UI_API *ui) {
  PluginContext &ctx = Context();
  const std::vector<truck_settings::TunedTruck> &trucks =
      truck_settings::TunedTrucks(ctx);
  DrawSectionTitle(ui, loc::Tr("ui.trucks.section",
                               {{"count", std::to_string(trucks.size())}})
                           .c_str());
  if (trucks.empty()) {
    ui->UI_TextDisabled(loc::Tr("ui.trucks.none"));
    return;
  }

  constexpr size_t kVisibleRows = 6;
  const bool scrolls = trucks.size() > kVisibleRows;
  // Plain rows: no background (the brand colors turn every other row bright
  // red) and no borders.
  int flags = SPF_TABLE_FLAG_NONE;
  if (scrolls)
    flags |= SPF_TABLE_FLAG_SCROLL_Y;
  const float height =
      scrolls ? ui->UI_GetTextLineHeightWithSpacing() * kVisibleRows : 0.0f;
  if (!ui->UI_BeginTable("##tuned_trucks", 3,
                         static_cast<SPF_TableFlags>(flags), 0.0f, height,
                         0.0f))
    return;
  ui->UI_TableSetupColumn("Truck", SPF_TABLE_COLUMN_FLAG_WIDTH_STRETCH, 0.0f,
                          0);
  ui->UI_TableSetupColumn("Changed", SPF_TABLE_COLUMN_FLAG_WIDTH_FIXED, 0.0f,
                          0);
  ui->UI_TableSetupColumn("Reset", SPF_TABLE_COLUMN_FLAG_WIDTH_FIXED, 0.0f, 0);

  // Opened after the table: a popup opened inside it would get the table's
  // ID and never match the one drawn below.
  const truck_settings::TunedTruck *to_reset = nullptr;
  const bool blocked = SettingsEditBlock() != EditBlock::kNone;
  for (size_t i = 0; i < trucks.size(); ++i) {
    const truck_settings::TunedTruck &truck = trucks[i];
    const bool current = truck.key == truck_settings::CurrentTruckKey();
    ui->UI_TableNextRow(SPF_TABLE_ROW_FLAG_NONE, 0.0f);
    ui->UI_TableSetColumnIndex(0);
    // The truck being driven stands out: its icon, and brighter.
    if (current)
      ui->UI_TextUnformatted(WithIcon(ICON_FA_TRUCK, truck.name).c_str());
    else
      ui->UI_TextDisabled(truck.name.c_str());
    ui->UI_TableSetColumnIndex(1);
    ui->UI_TextDisabled(std::to_string(truck.changed).c_str());
    ui->UI_SetItemTooltip(
        loc::Tr("ui.trucks.changed", {{"count", std::to_string(truck.changed)}})
            .c_str());
    ui->UI_TableSetColumnIndex(2);
    ui->UI_PushID_Int(static_cast<int>(i));
    // Like the reset below, not for the truck being driven while its
    // settings can't be changed.
    ui->UI_BeginDisabled(current && blocked);
    // Only an icon until hovered, so the list reads as plain text.
    ui->UI_PushStyleColor(SPF_COLOR_BUTTON, 0.0f, 0.0f, 0.0f, 0.0f);
    ui->UI_PushStyleColor(SPF_COLOR_BUTTON_HOVERED, kAccentR, kAccentG,
                          kAccentB, 0.35f);
    ui->UI_PushStyleColor(SPF_COLOR_BUTTON_ACTIVE, kAccentHoverR, kAccentHoverG,
                          kAccentHoverB, 0.55f);
    ui->UI_PushStyleColor(SPF_COLOR_TEXT, 0.6f, 0.6f, 0.6f, 1.0f);
    if (ui->UI_SmallButton(ICON_FA_ARROW_ROTATE_LEFT))
      to_reset = &truck;
    ui->UI_PopStyleColor(4);
    ui->UI_EndDisabled();
    ui->UI_SetItemTooltip(loc::Tr("ui.trucks.reset_tip"));
    ui->UI_PopID();
  }
  ui->UI_EndTable();
  if (to_reset)
    AskReset(ui, to_reset->key, to_reset->name);
}

// Greyed out with the effect pages, when there's no truck to reset.
void DrawResetSection(SPF_UI_API *ui) {
  DrawSectionTitle(ui, loc::Tr("ui.reset.section"));
  ui->UI_BeginDisabled(SettingsEditBlock() != EditBlock::kNone);
  const std::string reset_label =
      WithIcon(ICON_FA_ARROW_ROTATE_LEFT, loc::Tr("ui.reset.button")) +
      "###reset_all";
  if (MutedButton(ui, reset_label.c_str()))
    AskReset(ui, truck_settings::CurrentTruckKey(),
             truck_settings::CurrentTruckName());
  ui->UI_EndDisabled();
}

void DrawResetPopup(SPF_UI_API *ui) {
  const std::string title =
      loc::Tr("ui.reset.title") + std::string(kResetPopupId);
  const std::string message =
      loc::Tr("ui.reset.message", {{"truck", g_pending_reset.name}});
  // The truck may have been left while the popup was open.
  if (ConfirmPopup(ui, title, message.c_str(), loc::Tr("ui.reset.confirm")) &&
      truck_settings::ResetToDefaults(Context(), g_pending_reset.key)) {
    ShowToast(
        ui, SPF_NOTIFICATION_SUCCESS,
        loc::Tr("ui.reset.done_toast", {{"truck", g_pending_reset.name}}));
  }
}

} // namespace

void DrawSettingsTab(SPF_UI_API *ui, SPF_Config_API * /*cfg*/,
                     SPF_Config_Handle * /*h*/) {
  DrawTruckBanner(ui);
  ui->UI_Spacing();

  DrawSectionTitle(ui, loc::Tr("ui.keybind.section"));
  // The keybind button lines up just past the label, with room for the
  // translated label if it needs more.
  const float gap = Px(ui, 8.0f);
  const char *toggle_label = loc::Tr("ui.keybind.toggle_window");
  float label_w = 0.0f, label_h = 0.0f;
  ui->UI_CalcTextSize(toggle_label, &label_w, &label_h);
  const float field_w = std::max(Px(ui, 180.0f), label_w);
  DrawKeybindRowAt(ui, keybinds::kToggleWindow.id, toggle_label, field_w + gap);

  ui->UI_Spacing();
  DrawTunedTrucks(ui);
  ui->UI_Spacing();
  DrawResetSection(ui);
  DrawResetPopup(ui);
}

} // namespace motioncab::ui
