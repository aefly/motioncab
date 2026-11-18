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
  const CabinLayoutField *field = FindCabinLayoutField(name);
  return field ? *field : CabinLayoutFields().front();
}

std::string Label(const std::string &key) { return loc::Tr(key.c_str()); }

// Both seats' looks share theirs, and the aisle's steps theirs.
std::string Tip(std::string_view name) {
  std::string_view tip = name;
  if (name.ends_with("_yaw"))
    tip = "yaw";
  else if (name.ends_with("_pitch"))
    tip = "pitch";
  else if (name.starts_with("aisle_step_") && name.ends_with("_z"))
    tip = "aisle_step_z";
  else if (name.starts_with("aisle_step_") && name.ends_with("_height"))
    tip = "aisle_step_height";
  return Label("ui.cabin_walk.layout.tip." + std::string(tip));
}

// Shown in centimeters (stored in meters), degrees or as a count.
// Right-click goes back to the shipped layout's value. Applies live, saved
// when let go.
void DrawValueRow(SPF_UI_API *ui, CabinWalkEffect &walk, const char *name,
                  const std::string &label, bool indent = false) {
  using Unit = CabinLayoutField::Unit;
  const CabinLayoutField &field = FieldNamed(name);
  CabinLayout layout = walk.layout();
  float &value = field.Of(layout);
  const float scale = field.unit == Unit::kMeters ? 100.0f : 1.0f;
  const char *format = field.unit == Unit::kMeters    ? "%.1f cm"
                       : field.unit == Unit::kDegrees ? "%.0f\xc2\xb0"
                                                      : "%.0f";

  ui->UI_TableNextRow(SPF_TABLE_ROW_FLAG_NONE, 0.0f);
  ui->UI_TableNextColumn();
  if (indent)
    ui->UI_Indent(Px(ui, 12.0f));
  ui->UI_AlignTextToFramePadding();
  ui->UI_Text(label.c_str());
  if (indent)
    ui->UI_Unindent(Px(ui, 12.0f));
  ui->UI_TableNextColumn();
  float shown = value * scale;
  const float shipped = field.Of(walk.shipped_layout());
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

bool BeginRows(SPF_UI_API *ui, const char *id) {
  if (!ui->UI_BeginTable(id, 2, SPF_TABLE_FLAG_NONE, 0.0f, 0.0f, 0.0f))
    return false;
  ui->UI_TableSetupColumn("name", SPF_TABLE_COLUMN_FLAG_WIDTH_FIXED, 0.0f, 0);
  ui->UI_TableSetupColumn("value", SPF_TABLE_COLUMN_FLAG_WIDTH_STRETCH, 0.0f,
                          0);
  return true;
}

// To tune a seat from there.
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
      ui->UI_SameLine(0.0f, Px(ui, 6.0f));
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

// A passenger seat folded up, a day cab's missing bunk, a bus's steps.
void DrawUsableRow(SPF_UI_API *ui, CabinWalkEffect &walk,
                   bool CabinLayout::*member, const char *tip) {
  CabinLayout layout = walk.layout();
  bool &usable = layout.*member;
  ui->UI_TableNextRow(SPF_TABLE_ROW_FLAG_NONE, 0.0f);
  ui->UI_TableNextColumn();
  ui->UI_Indent(Px(ui, 12.0f));
  ui->UI_AlignTextToFramePadding();
  ui->UI_Text(loc::Tr("ui.cabin_walk.layout.usable"));
  ui->UI_Unindent(Px(ui, 12.0f));
  ui->UI_TableNextColumn();
  const std::string id = std::string("###usable_") + tip;
  if (ui->UI_Checkbox(id.c_str(), &usable))
    walk.SetLayout(layout);
  ui->UI_SetItemTooltip(Tip(tip).c_str());
}

// The aisle's steps, numbered as the player set them, whatever their order
// along it.
void DrawAisleRows(SPF_UI_API *ui, CabinWalkEffect &walk) {
  DrawValueRow(ui, walk, "aisle_step_count",
               Label("ui.cabin_walk.layout.aisle_step_count"), true);
  for (int i = 0, n = AisleStepCount(walk.layout()); i < n; ++i) {
    const std::string number = std::to_string(i + 1);
    for (const char *part : {"z", "height"}) {
      const std::string name = "aisle_step_" + number + "_" + part;
      DrawValueRow(
          ui, walk, name.c_str(),
          loc::Tr(
              (std::string("ui.cabin_walk.layout.aisle_step_") + part).c_str(),
              {{"n", number}}),
          true);
    }
  }
}

// The reset only shows for the player's own layout.
void DrawTruckRow(SPF_UI_API *ui, CabinWalkEffect &walk) {
  ui->UI_TableNextRow(SPF_TABLE_ROW_FLAG_NONE, 0.0f);
  ui->UI_TableNextColumn();
  ui->UI_AlignTextToFramePadding();
  ui->UI_Text(loc::Tr("ui.cabin_walk.layout.truck"));
  ui->UI_TableNextColumn();
  ui->UI_AlignTextToFramePadding();
  ui->UI_Text(WithIcon(ICON_FA_TRUCK, walk.truck_name()).c_str());
  ui->UI_SameLine(0.0f, Px(ui, 12.0f));
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
  DrawUsableRow(ui, walk, &CabinLayout::has_passenger, "has_passenger");
  if (walk.layout().has_passenger)
    rows({"passenger_dx", "passenger_dy", "passenger_dz", "passenger_yaw",
          "passenger_pitch"});
  title("bunk");
  DrawUsableRow(ui, walk, &CabinLayout::has_bunk, "has_bunk");
  if (walk.layout().has_bunk)
    rows({"bunk_sit_x", "bunk_sit_y", "bunk_sit_z", "bunk_sit_yaw",
          "bunk_sit_pitch"});
  title("steps");
  DrawUsableRow(ui, walk, &CabinLayout::has_aisle, "has_aisle");
  if (walk.layout().has_aisle)
    DrawAisleRows(ui, walk);
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
  // Paused or in another view, Cabin Walk doesn't run, so an edit wouldn't
  // show.
  if (!walk || !walk->truck_known() || !ctx.was_interior_last_frame) {
    ui->UI_TextDisabled(loc::Tr("ui.cabin_walk.layout.no_truck"));
    return;
  }

  DrawValues(ui, *walk);
}

} // namespace motioncab::ui
