#include "Widgets.hpp"

#include "SPF_Icons.h"
#include "core/Loc.hpp"
#include "core/PluginContext.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace motioncab::ui {

namespace {

// The muted red of the sliders/fields (their FRAME_BG values), for buttons
// that shouldn't stand out like the brand red regular ones. Pop 3 colors.
void PushMutedButtonColors(SPF_UI_API *ui) {
  ui->UI_PushStyleColor(SPF_COLOR_BUTTON, kAccentR, kAccentG, kAccentB, 0.25f);
  ui->UI_PushStyleColor(SPF_COLOR_BUTTON_HOVERED, kAccentR, kAccentG, kAccentB,
                        0.35f);
  ui->UI_PushStyleColor(SPF_COLOR_BUTTON_ACTIVE, kAccentHoverR, kAccentHoverG,
                        kAccentHoverB, 0.55f);
}

} // namespace

int PushBrandColors(SPF_UI_API *ui) {
  ui->UI_PushStyleColor(SPF_COLOR_BORDER, kAccentR, kAccentG, kAccentB, 0.5f);
  ui->UI_PushStyleColor(SPF_COLOR_NAV_HIGHLIGHT, kAccentR, kAccentG, kAccentB,
                        1.0f);
  ui->UI_PushStyleColor(SPF_COLOR_HEADER, kAccentR, kAccentG, kAccentB, 0.45f);
  ui->UI_PushStyleColor(SPF_COLOR_HEADER_HOVERED, kAccentR, kAccentG, kAccentB,
                        0.25f);
  ui->UI_PushStyleColor(SPF_COLOR_HEADER_ACTIVE, kAccentHoverR, kAccentHoverG,
                        kAccentHoverB, 0.9f);
  ui->UI_PushStyleColor(SPF_COLOR_SLIDER_GRAB, kAccentR, kAccentG, kAccentB,
                        1.0f);
  ui->UI_PushStyleColor(SPF_COLOR_SLIDER_GRAB_ACTIVE, kAccentHoverR,
                        kAccentHoverG, kAccentHoverB, 1.0f);
  ui->UI_PushStyleColor(SPF_COLOR_FRAME_BG, kAccentR, kAccentG, kAccentB,
                        0.25f);
  ui->UI_PushStyleColor(SPF_COLOR_TAB_ACTIVE, kAccentR, kAccentG, kAccentB,
                        1.0f);
  ui->UI_PushStyleColor(SPF_COLOR_TAB_HOVERED, kAccentR, kAccentG, kAccentB,
                        0.8f);
  ui->UI_PushStyleColor(SPF_COLOR_TAB, kAccentR, kAccentG, kAccentB, 0.25f);
  ui->UI_PushStyleColor(SPF_COLOR_TAB_UNFOCUSED_ACTIVE, kAccentR, kAccentG,
                        kAccentB, 0.9f);
  ui->UI_PushStyleColor(SPF_COLOR_TAB_UNFOCUSED, kAccentR, kAccentG, kAccentB,
                        0.2f);
  ui->UI_PushStyleColor(SPF_COLOR_FRAME_BG_HOVERED, kAccentR, kAccentG,
                        kAccentB, 0.35f);
  ui->UI_PushStyleColor(SPF_COLOR_FRAME_BG_ACTIVE, kAccentHoverR, kAccentHoverG,
                        kAccentHoverB, 0.55f);
  ui->UI_PushStyleColor(SPF_COLOR_BUTTON, kAccentR, kAccentG, kAccentB, 0.8f);
  ui->UI_PushStyleColor(SPF_COLOR_BUTTON_HOVERED, kAccentR, kAccentG, kAccentB,
                        1.0f);
  ui->UI_PushStyleColor(SPF_COLOR_BUTTON_ACTIVE, kAccentHoverR, kAccentHoverG,
                        kAccentHoverB, 1.0f);
  return 18;
}

int PushBrandRounding(SPF_UI_API *ui) {
  ui->UI_PushStyleVarFloat(SPF_STYLE_VAR_WINDOW_ROUNDING, 8.0f);
  ui->UI_PushStyleVarFloat(SPF_STYLE_VAR_CHILD_ROUNDING, 8.0f);
  ui->UI_PushStyleVarFloat(SPF_STYLE_VAR_POPUP_ROUNDING, 8.0f);
  ui->UI_PushStyleVarFloat(SPF_STYLE_VAR_FRAME_ROUNDING, 4.0f);
  ui->UI_PushStyleVarFloat(SPF_STYLE_VAR_GRAB_ROUNDING, 4.0f);
  ui->UI_PushStyleVarFloat(SPF_STYLE_VAR_TAB_ROUNDING, 6.0f);
  ui->UI_PushStyleVarFloat(SPF_STYLE_VAR_SCROLLBAR_ROUNDING, 6.0f);
  return 7;
}

StepperEdit StepperFloat(SPF_UI_API *ui, const char *id, float *value,
                         float min, float max, float step, const char *format,
                         float reset_value, const char *tooltip, bool drag) {
  StepperEdit edit = StepperEdit::kNone;
  // Square buttons at both ends, the value filling the width between.
  const float button = ui->UI_GetFrameHeight();
  constexpr float kGap = 4.0f;
  float avail_w = 0.0f, avail_h = 0.0f;
  ui->UI_GetContentRegionAvail(&avail_w, &avail_h);
  ui->UI_PushID_Str(id);
  auto step_button = [&](const char *icon, float sign) {
    if (MutedButton(ui, icon, button)) {
      *value = std::clamp(*value + sign * step, min, max);
      edit = StepperEdit::kDone;
    }
  };
  step_button(ICON_FA_MINUS "###minus", -1.0f);
  ui->UI_SameLine(0.0f, kGap);
  ui->UI_SetNextItemWidth(std::max(avail_w - 2.0f * (button + kGap), 40.0f));
  const bool moved =
      drag ? ui->UI_DragFloat("###value", value, (max - min) / 500.0f, min, max,
                              format, SPF_SLIDER_FLAG_ALWAYS_CLAMP)
           : ui->UI_SliderFloat("###value", value, min, max, format,
                                SPF_SLIDER_FLAG_ALWAYS_CLAMP);
  if (moved && edit == StepperEdit::kNone)
    edit = StepperEdit::kLive;
  if (ui->UI_IsItemDeactivatedAfterEdit())
    edit = StepperEdit::kDone;
  if (ui->UI_IsItemClicked(SPF_MOUSE_BUTTON_RIGHT)) {
    *value = reset_value;
    edit = StepperEdit::kDone;
  }
  if (tooltip)
    ui->UI_SetItemTooltip(tooltip);
  ui->UI_SameLine(0.0f, kGap);
  step_button(ICON_FA_PLUS "###plus", 1.0f);
  ui->UI_PopID();
  return edit;
}

float FormatStep(const char *format) {
  // "%.<n>f": n decimals.
  const char *dot = std::strstr(format, "%.");
  if (!dot || !std::isdigit(static_cast<unsigned char>(dot[2])))
    return 1.0f;
  return std::pow(10.0f, -static_cast<float>(dot[2] - '0'));
}

bool MutedButton(SPF_UI_API *ui, const char *label, float width) {
  PushMutedButtonColors(ui);
  const bool clicked = ui->UI_Button(label, width, 0.0f);
  ui->UI_PopStyleColor(3);
  return clicked;
}

std::string WithIcon(const char *icon, std::string_view text) {
  return std::string(icon) + " " + std::string(text);
}

void ShowToast(SPF_UI_API *ui, SPF_NotificationType type,
               const std::string &message) {
  SPF_Notification_Params params{};
  params.type = type;
  params.message = message.c_str();
  params.mode = SPF_NOTIF_MODE_STACK;
  params.duration = -1.0f;
  ui->UI_ShowNotification(&params);
}

void DrawSectionTitle(SPF_UI_API *ui, const char *title) {
  float start_x = 0.0f, start_y = 0.0f, avail_x = 0.0f, avail_y = 0.0f;
  ui->UI_GetCursorScreenPos(&start_x, &start_y);
  ui->UI_GetContentRegionAvail(&avail_x, &avail_y);

  ui->UI_TextDisabled(title);

  float min_x, min_y, max_x, max_y;
  ui->UI_GetItemRectMin(&min_x, &min_y);
  ui->UI_GetItemRectMax(&max_x, &max_y);
  constexpr uint32_t kRuleColor =
      (110u << 24) | (140u << 16) | (140u << 8) | 140u;
  const float y = (min_y + max_y) * 0.5f;
  ui->UI_DrawList_AddLine(ui->UI_GetWindowDrawList(), max_x + 8.0f, y,
                          start_x + avail_x, y, kRuleColor, 1.0f);
}

void DrawWrappedHint(SPF_UI_API *ui, const char *text) {
  ui->UI_PushStyleColor(SPF_COLOR_TEXT, 0.6f, 0.6f, 0.6f, 1.0f);
  ui->UI_TextWrapped(WithIcon(ICON_FA_CIRCLE_INFO, text).c_str());
  ui->UI_PopStyleColor(1);
}

bool KeybindUiAvailable() {
  PluginContext &ctx = Context();
  SPF_KeyBinds_API *kb = ctx.core ? ctx.core->keybinds : nullptr;
  return kb && ctx.keybinds_handle && kb->Kbind_OpenRebindPopup &&
         kb->Kbind_OpenBindingDetailsPopup && kb->Kbind_GetBindingDisplayName;
}

void DrawKeybindButtons(SPF_UI_API *ui, const char *action) {
  PluginContext &ctx = Context();
  SPF_KeyBinds_API *kb = ctx.core ? ctx.core->keybinds : nullptr;

  PushMutedButtonColors(ui);
  ui->UI_PushID_Str(action);
  const int count = kb->Kbind_GetBindingCount(ctx.keybinds_handle, action);
  for (int i = 0; i < count; ++i) {
    ui->UI_PushID_Int(i);
    char name[128] = {};
    kb->Kbind_GetBindingDisplayName(ctx.keybinds_handle, action, i, name,
                                    sizeof(name));
    if (ui->UI_Button(name[0] ? name : loc::Tr("ui.keybind.unbound"), 0.0f,
                      0.0f))
      kb->Kbind_OpenRebindPopup(ctx.keybinds_handle, action, i);
    ui->UI_SetItemTooltip(loc::Tr("ui.keybind.rebind_tip"));
    ui->UI_SameLine(0.0f, 4.0f);
    if (ui->UI_Button(ICON_FA_GEAR, 0.0f, 0.0f))
      kb->Kbind_OpenBindingDetailsPopup(ctx.keybinds_handle, action, i);
    ui->UI_SetItemTooltip(loc::Tr("ui.keybind.options_tip"));
    ui->UI_SameLine(0.0f, 12.0f);
    ui->UI_PopID();
  }
  if (ui->UI_Button(ICON_FA_PLUS "##add", 0.0f, 0.0f))
    kb->Kbind_OpenRebindPopup(ctx.keybinds_handle, action, -1);
  ui->UI_SetItemTooltip(loc::Tr("ui.keybind.add_tip"));
  ui->UI_PopID();
  ui->UI_PopStyleColor(3);
}

void DrawKeybindRowAt(SPF_UI_API *ui, const char *action, const char *label,
                      float button_x) {
  if (!KeybindUiAvailable())
    return;

  const float start_x = ui->UI_GetCursorPosX();
  ui->UI_AlignTextToFramePadding();
  ui->UI_Text(label);
  ui->UI_SameLine(start_x + button_x, 0.0f);
  DrawKeybindButtons(ui, action);
}

bool ConfirmPopup(SPF_UI_API *ui, const std::string &title, const char *message,
                  const char *confirm_label) {
  if (!ui->UI_BeginPopupModal(title.c_str(), nullptr, SPF_WINDOW_FLAG_NONE))
    return false;
  ui->UI_TextUnformatted(message);
  ui->UI_Spacing();
  const bool confirmed = MutedButton(ui, confirm_label);
  if (confirmed)
    ui->UI_CloseCurrentPopup();
  ui->UI_SameLine(0.0f, 8.0f);
  if (MutedButton(ui, loc::Tr("ui.cancel")))
    ui->UI_CloseCurrentPopup();
  ui->UI_EndPopup();
  return confirmed;
}

} // namespace motioncab::ui
