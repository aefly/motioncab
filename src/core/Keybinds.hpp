#pragma once

#include "SPF_KeyBinds_API.h"

// MotionCab's keybind actions, named once for the manifest (group + name),
// the registration in Plugin.cpp and the effects/UI (full "Group.name" id).
namespace motioncab::keybinds {

struct Action {
  const char *group;
  const char *name;
  const char *id; // "<group>.<name>"
};

inline constexpr Action kLookLeft{"ManualLook", "look_left",
                                  "ManualLook.look_left"};
inline constexpr Action kLookRight{"ManualLook", "look_right",
                                   "ManualLook.look_right"};
inline constexpr Action kZoom{"ManualZoom", "zoom", "ManualZoom.zoom"};
inline constexpr Action kBlindspotPeek{"BlindspotViewer", "peek",
                                       "BlindspotViewer.peek"};
inline constexpr Action kToggleWindow{"UI", "toggle", "UI.toggle"};

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
