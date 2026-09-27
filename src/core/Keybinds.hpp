#pragma once

#include "SPF_KeyBinds_API.h"

// MotionCab's keybind actions, declared once for the manifest (group +
// name, default key, title/description), the registration in Plugin.cpp
// and the effects/UI (full "Group.name" id).
namespace motioncab::keybinds {

struct Action {
  const char *group;
  const char *name;
  const char *id;          // "<group>.<name>"
  const char *loc_key;     // "<loc_key>.title" / ".desc" in localization/
  const char *default_key; // default keyboard binding
};

inline constexpr Action kLookLeft{"ManualLook", "look_left",
                                  "ManualLook.look_left", "keybinds.look_left",
                                  "KEY_DIVIDE"};
inline constexpr Action kLookRight{"ManualLook", "look_right",
                                   "ManualLook.look_right",
                                   "keybinds.look_right", "KEY_MULTIPLY"};
inline constexpr Action kZoom{"ManualZoom", "zoom", "ManualZoom.zoom",
                              "keybinds.zoom", "KEY_SUBTRACT"};
inline constexpr Action kBlindspotPeek{"BlindspotViewer", "peek",
                                       "BlindspotViewer.peek",
                                       "keybinds.blindspot_viewer", "KEY_F10"};
inline constexpr Action kToggleWindow{"UI", "toggle", "UI.toggle",
                                      "keybinds.ui_toggle", "KEY_F9"};

// Every action, in the order the manifest declares them.
inline constexpr Action kAllActions[] = {kLookLeft, kLookRight, kZoom,
                                         kBlindspotPeek, kToggleWindow};

// Actions the effects poll every frame via IsHeld() instead of reacting to
// a Kbind_Register callback.
inline constexpr Action kPolledActions[] = {kLookLeft, kLookRight, kZoom,
                                            kBlindspotPeek};

// Immediate physical state of the action's key, ignoring any hold/toggle
// behavior configured for the binding itself: MotionCab implements its own.
inline bool IsHeld(SPF_KeyBinds_API *api, SPF_KeyBinds_Handle *handle,
                   const Action &action) {
  return api->Kbind_GetActionValue(handle, action.id) > 0.5f;
}

// Turns a held state into a one-frame "just pressed" edge.
class PressEdge {
public:
  bool Update(bool held) {
    const bool pressed = held && !prev_held_;
    prev_held_ = held;
    return pressed;
  }
  void Reset() { prev_held_ = false; }

private:
  bool prev_held_ = false;
};

} // namespace motioncab::keybinds
