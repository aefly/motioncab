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
// (https://github.com/TrackAndTruckDevs/SPF_CabinWalk).
//
// Gets the player up from the driver's seat to walk around the parked cabin.
// Tapping stand/sit while up sits down at the seat looked toward, walking
// there first; holding it goes back to the wheel. Releasing the parking
// brake or rolling sends the player back to the wheel.
//
// Not a regular Effect: it needs the player's own pose, to walk from the
// seat they set up and to turn their view when sitting down, and it changes
// more of the camera than a HeadOffset can (rotation limits, mouse).
class CabinWalkEffect {
public:
  enum class Spot { kDriver, kPassenger, kBunkSit };
  // What to tell the player when they're refused or sent back to the wheel.
  enum class Notice { kNone, kNeedsParkingBrake, kNeedsStop, kBackToWheel };

  CabinWalkEffect(SPF_Config_API *config_api, SPF_Config_Handle *config_handle,
                  SPF_KeyBinds_API *keybinds_api,
                  SPF_KeyBinds_Handle *keybinds_handle, SPF_UI_API *ui_api,
                  CabinLayoutStore *layouts);

  bool IsEnabled() const { return enabled_; }
  void LoadConfig();

  // `base` is the player's own pose, under every offset.
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    SPF_Camera_API *camera, const CameraRig::Pose &base);

  // Set while Cabin Walk turns the player's own view (sitting down), in
  // radians, for CameraRig::SetBaseRotation.
  const std::optional<std::pair<float, float>> &base_rotation() const {
    return base_rotation_;
  }

  bool AtWheel() const {
    return state_ == State::kSeated && spot_ == Spot::kDriver;
  }

  // A player away from the wheel is put back at once: the cabin they were in
  // is gone.
  void OnTruckConstantsChanged(const SPF_TruckConstants &constants);

  // Gives the walking keys back to the game, which may be driven from
  // another camera meanwhile.
  void OnLeftInterior();

  // Leaves the camera's values alone: after a world reload they belong to a
  // camera that no longer exists.
  void SnapToWheel();

  void Shutdown(SPF_Camera_API *camera);

  Notice TakeNotice();

  // Their volumes, 0..1.
  std::vector<float> TakeFootsteps() { return std::exchange(footsteps_, {}); }

  // --- Layout tuning (Quick Settings) ---

  // For checking a spot's values from there. Done on the next Update(),
  // which has the truck and camera it needs.
  void GoToSpot(Spot spot) { go_to_request_ = spot; }

  bool truck_known() const { return !truck_key_.empty(); }
  const std::string &truck_name() const { return truck_name_; }
  const CabinLayout &layout() const { return layout_; }
  // The player's own layout rather than the shipped one.
  bool custom_layout() const { return custom_layout_; }
  // What a layout value resets to.
  const CabinLayout &shipped_layout() const { return shipped_layout_; }
  // `save` is false while a slider is still being dragged.
  void SetLayout(const CabinLayout &layout, bool save = true);
  void ResetLayout();

private:
  enum class State { kSeated, kTransition, kWalking };
  // kDirect moves between two seated spots.
  enum class Path { kRise, kSit, kDirect };

  struct Vec3 {
    float x, y, z;
  };
  struct SpotPose {
    Vec3 pos;
    float yaw_rad, pitch_rad;
  };

  SpotPose SpotAt(Spot spot) const;
  // On the floor plane.
  Vec3 Facing(Spot spot) const;
  // Where the player stands to get in or out of `spot`.
  Vec3 Approach(Spot spot) const;

public:
  // The layout can mark a seat unusable (folded up, no bunk).
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
  static Vec3 Bezier(const Vec3 &p0, const Vec3 &p1, const Vec3 &p2,
                     const Vec3 &p3, float u);
  void OnArrived(SPF_Camera_API *camera);
  // So a layout slider moves the player sitting there right away.
  void FollowSeatedSpot(SPF_Camera_API *camera);
  void UpdateWalking(float dt);
  float ClampToFloor(float value, float min, float max, float &velocity,
                     math::SpringDamper1D &spring) const;

  void SetMouseBlocked(bool blocked);
  void SetWalkKeysBlocked(bool blocked);
  void SaveLayout();
  // A right-hand drive truck's camera mirrors the seat position but not the
  // yaw (seen in game), so a direction worked out from the yaw needs its x
  // flipped.
  float CameraX() const { return right_hand_drive_ ? -1.0f : 1.0f; }
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
  Spot spot_ = Spot::kDriver; // while kSeated
  Vec3 pos_{};                // the head, without the step bob
  // The player's own pose.
  Vec3 base_{};
  float base_yaw_ = 0.0f, base_pitch_ = 0.0f;
  std::optional<std::pair<float, float>> base_rotation_;

  Vec3 from_{}, to_{};
  Path path_ = Path::kDirect;
  float progress_ = 0.0f, duration_ = 1.0f;
  State after_ = State::kWalking;
  Spot after_spot_ = Spot::kDriver;
  bool turn_ = false; // to the spot's look
  float yaw_from_ = 0.0f, pitch_from_ = 0.0f;
  // The second control point is relative to to_, which follows a seat that
  // may move.
  Vec3 ctrl1_{}, ctrl2_offset_{};
  // -1 (left) to 1 (right) relative to the view, for leaning into the move.
  float lean_side_ = 0.0f;
  // Looking down at where the body steps or sits, leaning into the move.
  float motion_pitch_deg_ = 0.0f, motion_roll_deg_ = 0.0f;
  // Last applied, to notice an edit.
  float seated_yaw_deg_ = 0.0f, seated_pitch_deg_ = 0.0f;

  math::SpringDamper1D vel_x_, vel_z_, height_;
  std::optional<Spot> walk_to_; // walking there by itself to sit down
  std::optional<Spot> go_to_request_;
  math::SpringDamper1D bob_amount_; // 1 at walking speed
  // 0 tilts with the cabin, 1 stays upright.
  float upright_now_ = 0.0f, upright_from_ = 0.0f;
  float bob_phase_ = 0.0f; // a half-turn per step

  // A tap acts on release, a hold once held long enough.
  bool stand_sit_held_ = false;
  bool stand_sit_long_done_ = false;
  float stand_sit_held_time_ = 0.0f;

  InteriorCameraOverride camera_override_;
  // Re-activating the camera recenters the head a moment later, so the
  // rotation from before is held meanwhile.
  int seen_refreshes_ = 0;
  float hold_rotation_time_ = 0.0f;
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
  // The passenger seat mirrors the player's across it.
  float centerline_x_ = kTypicalCenterlineX;
};

} // namespace motioncab
