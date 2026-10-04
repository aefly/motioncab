#pragma once

#include "SPF_Camera_API.h"
#include "SPF_KeyBinds_API.h"
#include "SPF_UI_API.h"
#include "core/CameraRig.hpp"
#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "effects/manual/cabin_walk/CabinLayouts.hpp"
#include "effects/manual/cabin_walk/InteriorCameraOverride.hpp"
#include "math/SpringDamper.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace motioncab {

// Based on SPF_CabinWalk by Track'n'Truck Devs
// (https://github.com/TrackAndTruckDevs/SPF_CabinWalk),
//
// Gets the player out of the driver's seat to walk around the cabin, parked
// with the parking brake on. The stand/sit key stands up; then the walking
// keys move the head freely over the cabin floor, relative to where the
// player looks, with crouching and a light step bob. Tapping
// stand/sit again sits down at the spot looked toward (driver's seat,
// passenger's, or the bunk's edge), walking there first if needed; holding
// it goes back to the wheel. Releasing the parking brake or rolling brings
// the player back to the wheel.
//
// Not a regular Effect: it needs the player's own pose (to walk from the
// seat they set up and to turn their view when sitting down) and owns
// camera state beyond a HeadOffset (rotation limits, mouse).
// Its offset is added to EffectManager's by OnUpdate, which fades the
// driving effects out meanwhile (EffectManager::SetAtWheel).
//
// The cabin itself (floor, heights, spots) comes from a per-truck
// CabinLayout, tuned from the Quick Settings.
class CabinWalkEffect {
public:
  enum class Spot { kDriver, kPassenger, kBunkSit };
  // Why the stand/sit key was refused, or the player was sent back to the
  // wheel, for OnUpdate to tell them.
  enum class Notice { kNone, kNeedsParkingBrake, kNeedsStop, kBackToWheel };

  CabinWalkEffect(SPF_Config_API *config_api, SPF_Config_Handle *config_handle,
                  SPF_KeyBinds_API *keybinds_api,
                  SPF_KeyBinds_Handle *keybinds_handle, SPF_UI_API *ui_api,
                  CabinLayoutStore *layouts);

  bool IsEnabled() const { return enabled_; }
  void LoadConfig();

  // Advances by `dt` seconds. `base` is the player's own pose underneath
  // every offset (CameraRig::Base). Returns the offset to add to the
  // effects' one.
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    SPF_Camera_API *camera, const CameraRig::Pose &base);

  // The player's own head rotation (radians) this frame's write must use,
  // while sitting down turns their view (CameraRig::SetBaseRotation).
  const std::optional<std::pair<float, float>> &base_rotation() const {
    return base_rotation_;
  }

  // Settled in the driver's seat: nothing overridden, driving effects on.
  bool AtWheel() const {
    return state_ == State::kSeated && spot_ == Spot::kDriver;
  }

  // Loads the new truck's layout. A player away from the wheel is put back
  // in the seat at once, since the cabin they were in is gone.
  void OnTruckConstantsChanged(const SPF_TruckConstants &constants);

  // Out of the interior view: the game gets the walking keys back
  // meanwhile, to drive from another camera. Update() blocks them again
  // once back in the cabin, and puts the player straight back at the wheel
  // if the truck was set rolling meanwhile.
  void OnLeftInterior();

  // Back in the driver's seat at once, leaving the camera's values to the
  // game: after a world reload they belong to a camera that no longer
  // exists.
  void SnapToWheel();

  // Plugin unload: gives the camera's own values and the mouse back.
  void Shutdown(SPF_Camera_API *camera);

  // The notice raised since the last call, if any.
  Notice TakeNotice();

  // The footsteps to play since the last call, as their volumes (0..1),
  // for OnUpdate to hand to CabinWalkSounds.
  std::vector<float> TakeFootsteps() { return std::exchange(footsteps_, {}); }

  // --- Layout tuning (Quick Settings) ---

  // Takes the player to sit at `spot`, to check or tune its values
  // from there: from the wheel too, standing up first if allowed (parked).
  // Done on the next Update(), which has the truck and camera it needs.
  void GoToSpot(Spot spot) { go_to_request_ = spot; }

  bool truck_known() const { return !truck_key_.empty(); }
  const std::string &truck_name() const { return truck_name_; }
  const CabinLayout &layout() const { return layout_; }
  // The layout is the player's own, not the one shipped for the truck.
  bool custom_layout() const { return custom_layout_; }
  // The layout the plugin ships for this truck (its preset, else the one
  // built around its centerline), under the player's own: what a layout
  // value resets to.
  const CabinLayout &shipped_layout() const { return shipped_layout_; }
  // Uses `layout` for this truck, saved unless `save` is false (a slider
  // still being dragged).
  void SetLayout(const CabinLayout &layout, bool save = true);
  // Drops the player's layout for this truck, back to the preset shipped
  // with the plugin, or the default one.
  void ResetLayout();

private:
  enum class State { kSeated, kTransition, kWalking };
  // How a transition moves the head (see StartTransition): standing up from
  // a seat, sitting down onto one, or moving between two seated spots.
  enum class Path { kRise, kSit, kDirect };

  struct Vec3 {
    float x, y, z;
  };
  struct SpotPose {
    Vec3 pos;
    float yaw_rad, pitch_rad;
  };

  SpotPose SpotAt(Spot spot) const;
  // Where the player sitting at `spot` faces, on the floor plane.
  Vec3 Facing(Spot spot) const;
  // Where the player stands to get in or out of `spot`: straight above it,
  // pulled into the floor area.
  Vec3 Approach(Spot spot) const;

public:
  // False for a spot this truck's layout marks unusable (a folded-up
  // passenger seat, no bunk).
  bool CanSitAt(Spot spot) const {
    switch (spot) {
    case Spot::kPassenger:
      return layout_.has_passenger;
    case Spot::kBunkSit:
      return layout_.has_bunk;
    case Spot::kDriver:
      break;
    }
    return true;
  }

private:
  // The seat the player looks toward, for a tap while standing.
  Spot LookedAtSpot() const;

  void HandleStandSitKey(float dt, const SPF_TruckData &truck,
                         SPF_Camera_API *camera);
  void TryStandUp(const SPF_TruckData &truck, SPF_Camera_API *camera);
  void HandleGoTo(Spot spot, const SPF_TruckData &truck,
                  SPF_Camera_API *camera);
  void StandUp();
  void SitAt(Spot spot);
  void GoToWheel(float duration);
  void StartTransition(const Vec3 &to, Path path, float duration, State after,
                       Spot after_spot, bool turn);
  float TransitionDuration(const Vec3 &to) const {
    return TransitionDuration(pos_, to);
  }
  float TransitionDuration(const Vec3 &from, const Vec3 &to) const;
  void UpdateTransition(float dt, SPF_Camera_API *camera);
  // Cubic Bezier through p0..p3 at `u` (0..1).
  static Vec3 Bezier(const Vec3 &p0, const Vec3 &p1, const Vec3 &p2,
                     const Vec3 &p3, float u);
  void OnArrived(SPF_Camera_API *camera);
  // Seated at a layout spot: follows its values live, so a layout slider
  // moves (and turns) the player sitting there right away.
  void FollowSeatedSpot(SPF_Camera_API *camera);
  void UpdateWalking(float dt);
  float ClampToFloor(float value, float min, float max, float &velocity,
                     math::SpringDamper1D &spring) const;

  void SetMouseBlocked(bool blocked);
  // The walking and crouch keys, which the game uses at the wheel, hidden
  // from it.
  void SetWalkKeysBlocked(bool blocked);
  void SaveLayout();
  // -1 on a right-hand drive truck, whose camera mirrors the seat position
  // left to right but not the head's yaw (seen in game): a direction worked
  // out from the yaw then has its x flipped to move the seat that way.
  float CameraX() const { return right_hand_drive_ ? -1.0f : 1.0f; }
  // Picks this truck's layout: the player's, else the preset, else one
  // built around its centerline.
  void LoadLayout();

  EffectConfig config_;
  SPF_KeyBinds_API *keybinds_api_;
  SPF_KeyBinds_Handle *keybinds_handle_;
  SPF_UI_API *ui_api_;
  CabinLayoutStore *layouts_;

  bool enabled_ = true;
  bool require_parking_brake_ =
      settings::DefaultBool("settings.manual.cabin_walk.require_parking_brake");

  State state_ = State::kSeated;
  Spot spot_ = Spot::kDriver; // the seat, while kSeated
  Vec3 pos_{};                // the head, without the step bob
  // The player's own seat position and head rotation, last frame.
  Vec3 base_{};
  float base_yaw_ = 0.0f, base_pitch_ = 0.0f;
  std::optional<std::pair<float, float>> base_rotation_;

  // Current transition.
  Vec3 from_{}, to_{};
  Path path_ = Path::kDirect;
  float progress_ = 0.0f, duration_ = 1.0f; // seconds
  State after_ = State::kWalking;
  Spot after_spot_ = Spot::kDriver;
  bool turn_ = false; // turns the view to the spot's look
  float yaw_from_ = 0.0f, pitch_from_ = 0.0f;
  // The head's path: a cubic Bezier from from_ to to_ through ctrl1_ and
  // to_ + ctrl2_offset_ (relative, since to_ follows a seat that moves).
  Vec3 ctrl1_{}, ctrl2_offset_{};
  // Sideways share of the move relative to the view, -1 (left) to 1
  // (right), for the head's lean on the way to a front seat (0 otherwise).
  float lean_side_ = 0.0f;
  // The body's own motion on top of the path, in degrees: looking down at
  // where it steps or sits, leaning into the move.
  float motion_pitch_deg_ = 0.0f, motion_roll_deg_ = 0.0f;
  // The seated spot's look (degrees) last applied, to notice an edit.
  float seated_yaw_deg_ = 0.0f, seated_pitch_deg_ = 0.0f;

  // Walking: m/s along x and z, head height.
  math::SpringDamper1D vel_x_, vel_z_, height_;
  std::optional<Spot> walk_to_;       // walking by itself to sit there
  std::optional<Spot> go_to_request_; // from GoToSpot, for Update()
  math::SpringDamper1D bob_amount_;   // 0 = still, 1 = walking speed
  // 0 = with the cabin, 1 = upright: its last value, and at the start of
  // the current transition.
  float upright_now_ = 0.0f, upright_from_ = 0.0f;
  float bob_phase_ = 0.0f; // radians, one half-turn per step

  // The stand/sit key: a tap acts on release, a hold once held long enough.
  bool stand_sit_held_ = false;
  bool stand_sit_long_done_ = false;
  float stand_sit_held_time_ = 0.0f;

  InteriorCameraOverride camera_override_;
  // The camera's re-activation (InteriorCameraOverride::refreshes) puts the
  // head back to its default rotation shortly after: the rotation from
  // before is held for a moment instead.
  int seen_refreshes_ = 0;
  float hold_rotation_time_ = 0.0f; // seconds left
  std::pair<float, float> held_rotation_{};
  bool mouse_blocked_ = false;
  bool walk_keys_blocked_ = false;
  bool left_interior_ = false; // since the last Update()
  Notice notice_ = Notice::kNone;
  std::vector<float> footsteps_;

  std::string truck_key_, truck_name_;
  CabinLayout layout_ = DefaultCabinLayout();
  bool custom_layout_ = false;
  CabinLayout shipped_layout_ = DefaultCabinLayout();
  bool right_hand_drive_ = false;
  // The cabin's centerline: the passenger seat mirrors the player's across
  // it, and an uncalibrated truck's layout is built around it.
  float centerline_x_ = kTypicalCenterlineX;
};

} // namespace motioncab
