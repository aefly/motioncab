#include "CabinLayoutPanel.hpp"

#include "SPF_Icons.h"
#include "core/Loc.hpp"
#include "core/PluginContext.hpp"
#include "effects/manual/cabin_walk/CabinLayouts.hpp"
#include "ui/Widgets.hpp"

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

namespace motioncab::ui {

namespace {

constexpr const char *kResetPopupId = "###confirm_reset_cabin_layout";

using Spot = CabinWalkEffect::Spot;

const CabinLayoutField &FieldNamed(std::string_view name) {
  for (const CabinLayoutField &field : CabinLayoutFields())
    if (name == field.name)
      return field;
  return CabinLayoutFields().front();
}

std::string Label(const std::string &key) { return loc::Tr(key.c_str()); }

// A value's tooltip: what it moves and which way. Both seats' looks share
// theirs.
std::string Tip(std::string_view name) {
  std::string_view tip = name;
  if (name.ends_with("_yaw"))
    tip = "yaw";
  else if (name.ends_with("_pitch"))
    tip = "pitch";
  return Label("ui.cabin_walk.layout.tip." + std::string(tip));
}

// A value's row: its name, then [-] value [+] (StepperFloat). The value is
// in centimeters (meters stored) or degrees, stepped by 1 cm or 1 degree,
// dragged, typed with Ctrl+click, or right-clicked back to the shipped
// layout's. Applies live; saved when let go.
void DrawValueRow(SPF_UI_API *ui, CabinWalkEffect &walk, const char *name,
                  const std::string &label, bool indent = false) {
  const CabinLayoutField &field = FieldNamed(name);
  CabinLayout layout = walk.layout();
  float &value = layout.*field.member;
  const float scale = field.angle ? 1.0f : 100.0f;
  const char *format = field.angle ? "%.0f\xc2\xb0" : "%.1f cm";

  ui->UI_TableNextRow(SPF_TABLE_ROW_FLAG_NONE, 0.0f);
  ui->UI_TableNextColumn();
  if (indent)
    ui->UI_Indent(12.0f);
  ui->UI_AlignTextToFramePadding();
  ui->UI_Text(label.c_str());
  if (indent)
    ui->UI_Unindent(12.0f);
  ui->UI_TableNextColumn();
  float shown = value * scale;
  const float shipped = walk.shipped_layout().*field.member;
  const std::string tip = Tip(name);
  switch (StepperFloat(ui, name, &shown, field.min * scale, field.max * scale,
                       1.0f, format, shipped * scale, tip.c_str(), true)) {
  case StepperEdit::kLive:
    value = shown / scale;
    walk.SetLayout(layout, false);
    break;
  case StepperEdit::kDone:
    value = shown / scale;
    walk.SetLayout(layout);
    break;
  case StepperEdit::kNone:
    break;
  }
}

// A table of rows: the names' column as wide as they need.
bool BeginRows(SPF_UI_API *ui, const char *id) {
  if (!ui->UI_BeginTable(id, 2, SPF_TABLE_FLAG_NONE, 0.0f, 0.0f, 0.0f))
    return false;
  ui->UI_TableSetupColumn("name", SPF_TABLE_COLUMN_FLAG_WIDTH_FIXED, 0.0f, 0);
  ui->UI_TableSetupColumn("value", SPF_TABLE_COLUMN_FLAG_WIDTH_STRETCH, 0.0f,
                          0);
  return true;
}

// Going to sit at a seat (standing up first if at the wheel), to tune it
// from there: a row of its own, the buttons where the values start.
void DrawGoToRow(SPF_UI_API *ui, CabinWalkEffect &walk) {
  ui->UI_TableNextRow(SPF_TABLE_ROW_FLAG_NONE, 0.0f);
  ui->UI_TableNextColumn();
  ui->UI_AlignTextToFramePadding();
  ui->UI_Text(loc::Tr("ui.cabin_walk.layout.go"));
  ui->UI_TableNextColumn();
  bool first = true;
  for (const auto &[spot, group] : {std::pair{Spot::kPassenger, "passenger"},
                                    std::pair{Spot::kBunkSit, "bunk"}}) {
    if (!first)
      ui->UI_SameLine(0.0f, 6.0f);
    first = false;
    const std::string go =
        WithIcon(ICON_FA_PERSON_WALKING,
                 Label(std::string("ui.cabin_walk.layout.group.") + group)) +
        "###go_" + group;
    ui->UI_BeginDisabled(!walk.CanSitAt(spot));
    if (MutedButton(ui, go.c_str()))
      walk.GoToSpot(spot);
    ui->UI_EndDisabled();
  }
}

// Whether a seat can be sat in: a passenger seat folded up, a day cab's
// missing bunk.
void DrawUsableRow(SPF_UI_API *ui, CabinWalkEffect &walk, Spot spot) {
  CabinLayout layout = walk.layout();
  bool &usable =
      spot == Spot::kPassenger ? layout.has_passenger : layout.has_bunk;
  ui->UI_TableNextRow(SPF_TABLE_ROW_FLAG_NONE, 0.0f);
  ui->UI_TableNextColumn();
  ui->UI_Indent(12.0f);
  ui->UI_AlignTextToFramePadding();
  ui->UI_Text(loc::Tr("ui.cabin_walk.layout.usable"));
  ui->UI_Unindent(12.0f);
  ui->UI_TableNextColumn();
  const std::string id = std::string("###usable_") +
                         (spot == Spot::kPassenger ? "passenger" : "bunk");
  if (ui->UI_Checkbox(id.c_str(), &usable))
    walk.SetLayout(layout);
  ui->UI_SetItemTooltip(
      Tip(spot == Spot::kPassenger ? "has_passenger" : "has_bunk").c_str());
}

// The truck, and dropping the player's own layout for it (only then).
void DrawTruckRow(SPF_UI_API *ui, CabinWalkEffect &walk) {
  ui->UI_TableNextRow(SPF_TABLE_ROW_FLAG_NONE, 0.0f);
  ui->UI_TableNextColumn();
  ui->UI_AlignTextToFramePadding();
  ui->UI_Text(loc::Tr("ui.cabin_walk.layout.truck"));
  ui->UI_TableNextColumn();
  ui->UI_AlignTextToFramePadding();
  ui->UI_Text(WithIcon(ICON_FA_TRUCK, walk.truck_name()).c_str());
  ui->UI_SameLine(0.0f, 12.0f);
  const std::string reset = WithIcon(ICON_FA_ARROW_ROTATE_LEFT,
                                     loc::Tr("ui.cabin_walk.layout.reset")) +
                            "###reset_cabin_layout";
  ui->UI_BeginDisabled(!walk.custom_layout());
  if (MutedButton(ui, reset.c_str()))
    ui->UI_OpenPopup(kResetPopupId, SPF_POPUP_FLAG_NONE);
  ui->UI_EndDisabled();
  const std::string title =
      loc::Tr("ui.cabin_walk.layout.reset_title") + std::string(kResetPopupId);
  if (ConfirmPopup(ui, title,
                   loc::Tr("ui.cabin_walk.layout.reset_message",
                           {{"truck", walk.truck_name()}})
                       .c_str(),
                   loc::Tr("ui.cabin_walk.layout.reset_confirm")))
    walk.ResetLayout();
}

// The truck and going to a seat, then every value by part of the cabin: the
// walkway's limits, the standing height and crouch, then each seat (whether
// it can be used, where it is and where it looks).
void DrawValues(SPF_UI_API *ui, CabinWalkEffect &walk) {
  if (!BeginRows(ui, "cabin_values"))
    return;
  auto title = [&](const char *group) {
    ui->UI_TableNextRow(SPF_TABLE_ROW_FLAG_NONE, 0.0f);
    ui->UI_TableNextColumn();
    ui->UI_AlignTextToFramePadding();
    ui->UI_Text(
        Label(std::string("ui.cabin_walk.layout.group.") + group).c_str());
  };
  auto rows = [&](std::initializer_list<const char *> names) {
    for (const char *name : names)
      DrawValueRow(ui, walk, name,
                   Label(std::string("ui.cabin_walk.layout.") + name), true);
  };
  DrawTruckRow(ui, walk);
  DrawGoToRow(ui, walk);
  title("walkway");
  rows({"floor_z_min", "floor_z_max", "floor_x_min", "floor_x_max"});
  title("heights");
  rows({"stand_y", "crouch_depth"});
  title("passenger");
  DrawUsableRow(ui, walk, Spot::kPassenger);
  rows({"passenger_dx", "passenger_dy", "passenger_dz", "passenger_yaw",
        "passenger_pitch"});
  title("bunk");
  DrawUsableRow(ui, walk, Spot::kBunkSit);
  rows({"bunk_sit_x", "bunk_sit_y", "bunk_sit_z", "bunk_sit_yaw",
        "bunk_sit_pitch"});
  ui->UI_EndTable();
}

} // namespace

void DrawCabinLayoutPanel(SPF_UI_API *ui) {
  const PluginContext &ctx = Context();
  CabinWalkEffect *walk = ctx.cabin_walk.get();
  const std::string advanced =
      WithIcon(ICON_FA_SLIDERS, loc::Tr("ui.cabin_walk.layout.advanced")) +
      "###cabin_layout_advanced";
  if (!ui->UI_CollapsingHeader(advanced.c_str(), SPF_TREE_NODE_FLAG_NONE))
    return;
  // Only from the cabin, the game running: paused (menus) or in another
  // view, Cabin Walk doesn't run, so an edit wouldn't show.
  if (!walk || !walk->truck_known() || !ctx.was_interior_last_frame) {
    ui->UI_TextDisabled(loc::Tr("ui.cabin_walk.layout.no_truck"));
    return;
  }

  DrawValues(ui, *walk);
}

} // namespace motioncab::ui
