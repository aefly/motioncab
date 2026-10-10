#pragma once

#include "SPF_KeyBinds_API.h"

#include <cstring>

// Every keybind action, declared once for the manifest, Plugin.cpp's
// registration, the effects and the UI.
namespace motioncab::keybinds {

struct Action {
  const char *group;
  const char *name;
  const char *id;      // "<group>.<name>"
  const char *loc_key; // plus ".title" / ".desc"
  const char *default_key;
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
// WASD is shared with driving: CabinWalkEffect only takes it away from the
// game while the player is out of the driver's seat.
inline constexpr Action kWalkForward{"CabinWalk", "forward",
                                     "CabinWalk.forward",
                                     "keybinds.cabin_walk.forward", "KEY_W"};
inline constexpr Action kWalkBack{"CabinWalk", "back", "CabinWalk.back",
                                  "keybinds.cabin_walk.back", "KEY_S"};
inline constexpr Action kWalkLeft{"CabinWalk", "left", "CabinWalk.left",
                                  "keybinds.cabin_walk.left", "KEY_A"};
inline constexpr Action kWalkRight{"CabinWalk", "right", "CabinWalk.right",
                                   "keybinds.cabin_walk.right", "KEY_D"};
// Not Ctrl, since SPF never blocks a modifier key from the game, nor Space,
// which releases the parking brake and sends the player back to the wheel.
// C is the game's cruise control, so it gets blocked like WASD.
inline constexpr Action kCrouch{"CabinWalk", "crouch", "CabinWalk.crouch",
                                "keybinds.cabin_walk.crouch", "KEY_C"};

// In the manifest's order.
inline constexpr Action kAllActions[] = {
    kLookLeft,      kLookRight,    kGlanceLeft, kGlanceRight, kZoom,
    kBlindspotPeek, kToggleWindow, kStandSit,   kWalkForward, kWalkBack,
    kWalkLeft,      kWalkRight,    kCrouch};

// The ones read every frame rather than through a Kbind_Register callback.
inline constexpr Action kPolledActions[] = {
    kLookLeft, kLookRight,     kGlanceLeft, kGlanceRight,
    kZoom,     kBlindspotPeek, kStandSit,   kWalkForward,
    kWalkBack, kWalkLeft,      kWalkRight,  kCrouch};

// Keys the game drives with get the "manual" consume policy, so
// CabinWalkEffect decides when the game stops seeing them.
inline constexpr Action kWalkActions[] = {kWalkForward, kWalkBack, kWalkLeft,
                                          kWalkRight, kCrouch};

inline bool IsWalkAction(const Action &action) {
  for (const Action &walk : kWalkActions)
    if (std::strcmp(walk.id, action.id) == 0)
      return true;
  return false;
}

// The raw key state: the binding's own hold/toggle behavior is ignored, since
// the effects implement their own.
inline bool IsHeld(SPF_KeyBinds_API *api, SPF_KeyBinds_Handle *handle,
                   const Action &action) {
  return api->Kbind_GetActionValue(handle, action.id) > 0.5f;
}

// Whether a combination (e.g. LB + D-pad left) is bound to the action. SPF
// gives a combination priority over its keys bound alone only for callbacks,
// so an effect polling several actions has to do it itself.
inline bool HasChord(SPF_KeyBinds_API *api, SPF_KeyBinds_Handle *handle,
                     const Action &action) {
  const int count = api->Kbind_GetBindingCount(handle, action.id);
  for (int i = 0; i < count; ++i)
    if (api->Kbind_GetBindingType(handle, action.id, i) == SPF_BINDING_CHORD)
      return true;
  return false;
}

// 0 or 1 for a key, the deflection for an analog axis.
inline float Amount(SPF_KeyBinds_API *api, SPF_KeyBinds_Handle *handle,
                    const Action &action) {
  const float value = api->Kbind_GetActionValue(handle, action.id);
  return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

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
