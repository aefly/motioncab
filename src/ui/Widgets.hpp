#pragma once

#include "SPF_UI_API.h"

#include <string>
#include <string_view>

// Look and widgets shared by the Quick Settings window's tabs.
namespace motioncab::ui {

// MotionCab's brand red (the README badges' #B82728) and its darker
// hover/active shade.
inline constexpr float kAccentR = 0xB8 / 255.0f;
inline constexpr float kAccentG = 0x27 / 255.0f;
inline constexpr float kAccentB = 0x28 / 255.0f;
inline constexpr float kAccentHoverR = 0x8A / 255.0f;
inline constexpr float kAccentHoverG = 0x10 / 255.0f;
inline constexpr float kAccentHoverB = 0x10 / 255.0f;

// MotionCab's red over SPF's own colors, for the whole window. Returns the
// number of colors to pop.
int PushBrandColors(SPF_UI_API *ui);

// Softer, slightly rounded look for the whole window instead of SPF's
// default sharp corners.
int PushBrandRounding(SPF_UI_API *ui);

// Button in the muted red used by the sliders/fields (FRAME_BG values) and the
// keybind buttons, instead of the brighter brand red of regular buttons.
bool MutedButton(SPF_UI_API *ui, const char *label, float width = 0.0f);

// How a value was edited this frame (see StepperFloat).
enum class StepperEdit {
  kNone,
  kLive, // being dragged: apply it, save once let go
  kDone, // stepped, reset, typed or let go: save it
};

// [-] value [+] filling the width: the value slid (or, with `drag`,
// dragged without a range bar), stepped by `step` with the buttons,
// Ctrl+click to type it, right-click back to `reset_value`. `tooltip`, if
// any, shows over the value.
StepperEdit StepperFloat(SPF_UI_API *ui, const char *id, float *value,
                         float min, float max, float step, const char *format,
                         float reset_value, const char *tooltip = nullptr,
                         bool drag = false);

// The step a printf-style format shows: 1 for "%.0f", 0.01 for "%.2f".
float FormatStep(const char *format);

// "<icon> <text>", for the icon-prefixed labels used all over the window.
std::string WithIcon(const char *icon, std::string_view text);

// Native SPF-style toast, matching how the framework itself communicates
void ShowToast(SPF_UI_API *ui, SPF_NotificationType type,
               const std::string &message);

// Section title followed by a thin rule running to the window's right edge
// ("Profiles ─────────"), lighter than UI_SeparatorText's full-width bar.
void DrawSectionTitle(SPF_UI_API *ui, const char *title);

// Small muted, word-wrapped note with an info icon. Wraps at the window
// edge instead of running off it, unlike a plain UI_Text/UI_TextDisabled
// call.
void DrawWrappedHint(SPF_UI_API *ui, const char *text);

// True when the framework is recent enough to draw rebind/details popups.
// Falls back to nothing on an older one, where the hint text still points to
// SPF's Keybinds menu.
bool KeybindUiAvailable();

// The bindings of one keybind action: each existing binding is a button
// (click to rebind) with a gear next to it (advanced options), plus a "+" to
// add another. The rebind/details popups are drawn by SPF itself, so the
// result is identical to, and stays in sync with, the native Settings
// window.
void DrawKeybindButtons(SPF_UI_API *ui, const char *action);

// Same label|bindings row outside a table, with the buttons placed at
// `button_x` from the row's left edge instead of the tables' 160 px label
// column, so they line up with other buttons of the Settings tab.
void DrawKeybindRowAt(SPF_UI_API *ui, const char *action, const char *label,
                      float button_x);

// Draws the modal confirmation popup `title` ("<translated title>###<id>",
// opened with UI_OpenPopup on the same "###<id>"): `message`, a confirm
// button and a Cancel one. True on the frame confirm is clicked; the popup
// closes either way.
bool ConfirmPopup(SPF_UI_API *ui, const std::string &title, const char *message,
                  const char *confirm_label);

} // namespace motioncab::ui
