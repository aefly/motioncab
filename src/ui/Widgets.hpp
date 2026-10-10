#pragma once

#include "SPF_UI_API.h"

#include <string>
#include <string_view>

// Look and widgets shared by the Quick Settings window's tabs.
namespace motioncab::ui {

// The README badges' #B82728, and a darker shade for hover.
inline constexpr float kAccentR = 0xB8 / 255.0f;
inline constexpr float kAccentG = 0x27 / 255.0f;
inline constexpr float kAccentB = 0x28 / 255.0f;
inline constexpr float kAccentHoverR = 0x8A / 255.0f;
inline constexpr float kAccentHoverG = 0x10 / 255.0f;
inline constexpr float kAccentHoverB = 0x10 / 255.0f;

// Both return how many to pop.
int PushBrandColors(SPF_UI_API *ui);
int PushBrandRounding(SPF_UI_API *ui);

// In the sliders' muted red rather than the brighter brand red of regular
// buttons.
bool MutedButton(SPF_UI_API *ui, const char *label, float width = 0.0f);

enum class StepperEdit {
  kNone,
  kLive, // being dragged: apply it, save once let go
  kDone, // stepped, reset, typed or let go: save it
};

// [-] slider [+] filling the width. Ctrl+click types the value, right-click
// puts `reset_value` back. With `drag`, the value is dragged without a range
// bar.
StepperEdit StepperFloat(SPF_UI_API *ui, const char *id, float *value,
                         float min, float max, float step, const char *format,
                         float reset_value, const char *tooltip = nullptr,
                         bool drag = false);

// 1 for "%.0f", 0.01 for "%.2f".
float FormatStep(const char *format);

std::string WithIcon(const char *icon, std::string_view text);

void ShowToast(SPF_UI_API *ui, SPF_NotificationType type,
               const std::string &message);

// "Profiles ─────────", lighter than UI_SeparatorText's full-width bar.
void DrawSectionTitle(SPF_UI_API *ui, const char *title);

// Muted, with an info icon, and wrapped at the window's edge rather than
// running off it.
void DrawWrappedHint(SPF_UI_API *ui, const char *text);

// Whether the framework can draw the rebind popups. Without them, the hint
// text still points to SPF's Keybinds menu.
bool KeybindUiAvailable();

// A button per binding to rebind it, a gear for its options and a "+" for
// another. SPF draws the popups itself, so they match its own Settings
// window.
void DrawKeybindButtons(SPF_UI_API *ui, const char *action);

// The same row outside a table, with the buttons at `button_x` to line up
// with the Settings tab's other buttons.
void DrawKeybindRowAt(SPF_UI_API *ui, const char *action, const char *label,
                      float button_x);

// `title` is "<translated title>###<id>", opened with UI_OpenPopup on the
// same "###<id>". True on the frame confirm is clicked.
bool ConfirmPopup(SPF_UI_API *ui, const std::string &title, const char *message,
                  const char *confirm_label);

} // namespace motioncab::ui
