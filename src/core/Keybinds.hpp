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
// Meant to replace the game's look forward, which holds the view ahead until
// the mouse moves, pulling back every look we add.
inline constexpr Action kLookForward{"ManualLook", "look_forward",
                                     "ManualLook.look_forward",
                                     "keybinds.look_forward", "KEY_NUMPAD5"};
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
    kLookLeft, kLookRight,     kGlanceLeft,   kGlanceRight, kLookForward,
    kZoom,     kBlindspotPeek, kToggleWindow, kStandSit,    kWalkForward,
    kWalkBack, kWalkLeft,      kWalkRight,    kCrouch};

// The ones read every frame rather than through a Kbind_Register callback.
inline constexpr Action kPolledActions[] = {
    kLookLeft, kLookRight,     kGlanceLeft, kGlanceRight, kLookForward,
    kZoom,     kBlindspotPeek, kStandSit,   kWalkForward, kWalkBack,
    kWalkLeft, kWalkRight,     kCrouch};

// Side Look's looks start out as Hold in SPF's keybind settings; every other
// action keeps SPF's default, Toggle.
inline constexpr Action kHoldByDefault[] = {kLookLeft, kLookRight, kGlanceLeft,
                                            kGlanceRight};

inline bool IsHoldByDefault(const Action &action) {
  for (const Action &hold : kHoldByDefault)
    if (std::strcmp(hold.id, action.id) == 0)
      return true;
  return false;
}

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

// The raw key state, the binding's Hold or Toggle ignored: for the keys an
// effect times itself (a tap or a hold of the same key).
inline bool IsHeld(SPF_KeyBinds_API *api, SPF_KeyBinds_Handle *handle,
                   const Action &action) {
  return api->Kbind_GetActionValue(handle, action.id) > 0.5f;
}

// Whether the action is on, per the player's Hold or Toggle for each of its
// bindings in SPF's keybind settings. SPF also gives a combination priority
// over its keys bound alone.
inline bool IsActive(SPF_KeyBinds_API *api, SPF_KeyBinds_Handle *handle,
                     const Action &action) {
  return api->Kbind_IsActionActive(handle, action.id);
}

// Turns the action off, a toggled one or a held one until pressed again.
inline void EndActive(SPF_KeyBinds_API *api, SPF_KeyBinds_Handle *handle,
                      const Action &action) {
  api->Kbind_ResetActionState(handle, action.id);
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
