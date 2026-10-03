#pragma once

#include "SPF_KeyBinds_API.h"

#include <cstring>

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
inline constexpr Action kGlanceLeft{"ManualLook", "glance_left",
                                    "ManualLook.glance_left",
                                    "keybinds.glance_left", "KEY_NUMPAD7"};
inline constexpr Action kGlanceRight{"ManualLook", "glance_right",
                                     "ManualLook.glance_right",
                                     "keybinds.glance_right", "KEY_NUMPAD9"};
inline constexpr Action kZoom{"ManualZoom", "zoom", "ManualZoom.zoom",
                              "keybinds.zoom", "KEY_SUBTRACT"};
inline constexpr Action kBlindspotPeek{"BlindspotViewer", "peek",
                                       "BlindspotViewer.peek",
                                       "keybinds.blindspot_viewer", "KEY_F10"};
inline constexpr Action kToggleWindow{"UI", "toggle", "UI.toggle",
                                      "keybinds.ui_toggle", "KEY_F9"};
inline constexpr Action kStandSit{
    "CabinWalk", "stand_sit", "CabinWalk.stand_sit",
    "keybinds.cabin_walk.stand_sit", "KEY_PAGEUP"};
// WASD, shared with driving: CabinWalkEffect takes them away from the game
// (Kbind_SetBlockState) only while the player is out of the driver's seat.
inline constexpr Action kWalkForward{"CabinWalk", "forward",
                                     "CabinWalk.forward",
                                     "keybinds.cabin_walk.forward", "KEY_W"};
inline constexpr Action kWalkBack{"CabinWalk", "back", "CabinWalk.back",
                                  "keybinds.cabin_walk.back", "KEY_S"};
inline constexpr Action kWalkLeft{"CabinWalk", "left", "CabinWalk.left",
                                  "keybinds.cabin_walk.left", "KEY_A"};
inline constexpr Action kWalkRight{"CabinWalk", "right", "CabinWalk.right",
                                   "keybinds.cabin_walk.right", "KEY_D"};
// C rather than Ctrl/Space: SPF never takes a modifier key away from the
// game, and Space releases the parking brake, which sends the player back to
// the wheel. C is the game's cruise control, so it's taken away like WASD.
inline constexpr Action kCrouch{"CabinWalk", "crouch", "CabinWalk.crouch",
                                "keybinds.cabin_walk.crouch", "KEY_C"};

// Every action, in the order the manifest declares them.
inline constexpr Action kAllActions[] = {
    kLookLeft,      kLookRight,    kGlanceLeft, kGlanceRight, kZoom,
    kBlindspotPeek, kToggleWindow, kStandSit,   kWalkForward, kWalkBack,
    kWalkLeft,      kWalkRight,    kCrouch};

// Actions the effects poll every frame via IsHeld() instead of reacting to
// a Kbind_Register callback.
inline constexpr Action kPolledActions[] = {
    kLookLeft, kLookRight,     kGlanceLeft, kGlanceRight,
    kZoom,     kBlindspotPeek, kStandSit,   kWalkForward,
    kWalkBack, kWalkLeft,      kWalkRight,  kCrouch};

// Actions on keys the game drives with: declared with the "manual" consume
// policy, so CabinWalkEffect decides when the game stops seeing them.
inline constexpr Action kWalkActions[] = {kWalkForward, kWalkBack, kWalkLeft,
                                          kWalkRight, kCrouch};

inline bool IsWalkAction(const Action &action) {
  for (const Action &walk : kWalkActions)
    if (std::strcmp(walk.id, action.id) == 0)
      return true;
  return false;
}

// Immediate physical state of the action's key, ignoring any hold/toggle
// behavior configured for the binding itself: MotionCab implements its own.
inline bool IsHeld(SPF_KeyBinds_API *api, SPF_KeyBinds_Handle *handle,
                   const Action &action) {
  return api->Kbind_GetActionValue(handle, action.id) > 0.5f;
}

// Whether a key combination (e.g. LB + D-pad left) is bound to the action.
// Kbind_GetActionValue doesn't give a combination priority over its own
// keys bound alone, as SPF does for the callbacks: an effect polling
// several actions has to.
inline bool HasChord(SPF_KeyBinds_API *api, SPF_KeyBinds_Handle *handle,
                     const Action &action) {
  const int count = api->Kbind_GetBindingCount(handle, action.id);
  for (int i = 0; i < count; ++i)
    if (api->Kbind_GetBindingType(handle, action.id, i) == SPF_BINDING_CHORD)
      return true;
  return false;
}

// The action's value in [0, 1]: 0/1 for a key, the deflection for an
// analog axis bound to it.
inline float Amount(SPF_KeyBinds_API *api, SPF_KeyBinds_Handle *handle,
                    const Action &action) {
  const float value = api->Kbind_GetActionValue(handle, action.id);
  return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
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
