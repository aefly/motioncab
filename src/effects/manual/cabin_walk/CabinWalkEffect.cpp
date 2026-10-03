#include "CabinWalkEffect.hpp"

#include "core/Keybinds.hpp"
#include "effects/TelemetryUtil.hpp"
#include "math/Units.hpp"

#include <algorithm>
#include <cmath>
#include <tuple>
#include <utility>

namespace motioncab {

using math::kDegToRad;
using math::kPi;

namespace {

// Walking speed (m/s), slower backward and sideways, and how long it takes
// to get up to speed and to stop (stopping is quicker).
constexpr float kWalkSpeed = 1.0f;
constexpr float kBackwardShare = 0.6f;
constexpr float kSidewaysShare = 0.75f;
constexpr float kAccelerationSeconds = 0.15f;
constexpr float kDecelerationSeconds = 0.1f;
// How deep the head dips each step (meters).
constexpr float kHeadBob = 0.012f;
// The footsteps' volume (0..1) at walking speed.
constexpr float kFootstepVolume = 0.8f;
// Standing up needs the truck at rest; above kMaxAwayKmh the player is
// sent back to the wheel (a parked truck still creeps a little on a slope).
constexpr float kMaxStandKmh = 1.0f;
constexpr float kMaxAwayKmh = 3.0f;
// Holding stand/sit this long goes back to the wheel instead of the tap's
// "sit at the nearest spot".
constexpr float kLongPressSeconds = 0.6f;
// Standing up or sitting down takes kTransitionSeconds for kTransitionMeters,
// scaled by the actual distance within the bounds below.
constexpr float kTransitionSeconds = 1.2f;
constexpr float kTransitionMeters = 0.7f;
constexpr float kTransitionScaleMin = 0.7f;
constexpr float kTransitionScaleMax = 1.6f;
// Hurried back to the wheel (parking brake released, truck rolling).
constexpr float kHurrySeconds = 0.8f;
// Walking by itself to a spot: close enough to sit down from, and how
// soon it slows down before it (speed = distance * gain).
constexpr float kArriveMeters = 0.04f;
constexpr float kArriveGain = 4.0f;
// Crouching slows walking down by this fraction.
constexpr float kCrouchSlowdown = 0.5f;
// Head height changes (crouching).
constexpr float kHeightTime = 0.3f;
// Step bob: one dip per step of kStepMeters, a sideways sway of this
// fraction of the dip, and a slight roll along with the sway.
constexpr float kStepMeters = 0.45f;
constexpr float kBobSwayRatio = 0.6f;
constexpr float kBobRollDegPerMeter = 40.0f;
// How long the head's rotation is held after the camera's re-activation,
// which recenters it a frame or so later.
constexpr float kHoldRotationSeconds = 0.25f;
// Within this distance of a seat (on the floor plane), a tap sits there
// whatever the look.
constexpr float kOverSpotMeters = 0.08f;
// No footstep sound below this share of walking speed (shuffling in place).
constexpr float kMinStepSoundAmount = 0.3f;
constexpr float kBobTime = 0.2f;
// Standing, the body stays upright while the parked truck leans (half on a
// kerb, on a slope): the head tilts against the truck's tilt, turning
// along with standing up and sitting down, within a sane bound.
constexpr float kMaxUprightDeg = 15.0f;
// The camera's roll turns the head the other way from the truck's roll
// (seen in game: +1 leaned the head further with the truck).
constexpr float kUprightRollSign = -1.0f;
// Turning all the way around while up, and looking down at the floor. The
// yaw is wrapped back into +-180 every frame (see Update), so this is just
// headroom the mouse never reaches: 90 degrees past a half-turn in a frame.
constexpr float kAwayYawLimitDeg = 270.0f;
constexpr float kAwayDownLimitDeg = -80.0f;

// Standing up leans the head forward (over the feet) and a bit down before
// rising; the rise then ends straight up.
constexpr float kRiseLeanMeters = 0.12f;
constexpr float kRiseDipMeters = 0.04f;
constexpr float kRiseEndLiftMeters = 0.10f;
// Sitting down steps over at standing height first, then lowers from a bit
// in front of and above the seat, settling back against the backrest.
constexpr float kSitStepShare = 0.4f;
constexpr float kSitLeanMeters = 0.12f;
constexpr float kSitAboveMeters = 0.10f;
// Moving between seated spots arcs over, more for a longer move.
constexpr float kArcMeters = 0.05f;
constexpr float kArcFullMeters = 0.6f;
// Looking down during the move, and leaning into it on the way to the
// driver's or the passenger seat (degrees, at mid-move).
constexpr float kRiseLookDownDeg = 8.0f;
constexpr float kSitLookDownDeg = 10.0f;
constexpr float kDirectLookDownDeg = 4.0f;
constexpr float kLeanRollDeg = 3.0f;
// The view turns to a seat's look a bit before the head gets there.
constexpr float kTurnShare = 0.85f;

float Clamp01(float t) { return std::clamp(t, 0.0f, 1.0f); }

// 0 to 1 with zero speed and acceleration at both ends (quintic), so a move
// neither starts nor stops with a jolt.
float SmootherStep(float t) {
  t = Clamp01(t);
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

// 0 at both ends, 1 at mid-move, with zero slope at the ends.
float Bell(float t) {
  const float s = std::sin(kPi * Clamp01(t));
  return s * s;
}

float Lerp(float a, float b, float t) { return a + (b - a) * t; }

// Into [-pi, pi], so a turn never goes across the back: the yaw limits stop
// at +-180 degrees, and a turn across them would jump.
float WrapYaw(float yaw_rad) { return std::remainder(yaw_rad, 2.0f * kPi); }

} // namespace

CabinWalkEffect::CabinWalkEffect(SPF_Config_API *config_api,
                                 SPF_Config_Handle *config_handle,
                                 SPF_KeyBinds_API *keybinds_api,
                                 SPF_KeyBinds_Handle *keybinds_handle,
                                 SPF_UI_API *ui_api, CabinLayoutStore *layouts)
    : config_(config_api, config_handle, "manual", "cabin_walk"),
      keybinds_api_(keybinds_api), keybinds_handle_(keybinds_handle),
      ui_api_(ui_api), layouts_(layouts) {
  height_.SetTimeConstant(kHeightTime);
  bob_amount_.SetTimeConstant(kBobTime);
}

void CabinWalkEffect::LoadConfig() {
  if (!config_.IsAvailable())
    return;
  enabled_ = config_.Bool("enabled", enabled_);
  require_parking_brake_ =
      config_.Bool("require_parking_brake", require_parking_brake_);
}

HeadOffset CabinWalkEffect::Update(float dt, const SPF_TruckData &truck,
                                   SPF_Camera_API *camera,
                                   const CameraRig::Pose &base) {
  // First frame back from another view (see OnLeftInterior).
  const bool back_in_cabin = std::exchange(left_interior_, false);
  base_ = {base.seat_x, base.seat_y, base.seat_z};
  base_yaw_ = base.yaw_rad;
  base_pitch_ = base.pitch_rad;
  base_rotation_.reset();
  if (hold_rotation_time_ > 0.0f) {
    hold_rotation_time_ -= dt;
    std::tie(base_yaw_, base_pitch_) = held_rotation_;
    base_rotation_ = held_rotation_;
    if (hold_rotation_time_ <= 0.0f)
      camera_override_.EndRefresh(camera);
  }

  // Turning around freely: past a half-turn, the same look a full turn
  // back, before the mouse reaches the widened limit. (Not in a seat that
  // keeps the truck's own limits, which may go past 180 on purpose.)
  const bool full_turn =
      !AtWheel() && !(state_ == State::kSeated && spot_ == Spot::kPassenger);
  if (full_turn && std::fabs(base_yaw_) > kPi) {
    base_yaw_ = WrapYaw(base_yaw_);
    base_rotation_ = {base_yaw_, base_pitch_};
  }

  // Anything that makes being up unsafe (or unwanted) brings the player
  // back, unless already on the way to the wheel.
  const bool heading_to_wheel = state_ == State::kTransition &&
                                after_ == State::kSeated &&
                                after_spot_ == Spot::kDriver;
  if (!AtWheel() && !heading_to_wheel) {
    const bool unsafe = telemetry::SpeedKmh(truck) > kMaxAwayKmh ||
                        (require_parking_brake_ && !truck.parking_brake);
    if (unsafe && back_in_cabin) {
      // It became unsafe while out of the interior view (driving from
      // another camera): no walk back to the wheel in the middle of it.
      notice_ = Notice::kBackToWheel;
      camera_override_.Restore(camera);
      SnapToWheel();
    } else if (unsafe || !enabled_) {
      if (enabled_)
        notice_ = Notice::kBackToWheel;
      GoToWheel(unsafe ? kHurrySeconds : TransitionDuration(base_));
    }
  }

  if (enabled_) {
    HandleStandSitKey(dt, truck, camera);
    if (go_to_request_)
      HandleGoTo(*std::exchange(go_to_request_, std::nullopt), truck, camera);
  }

  switch (state_) {
  case State::kSeated:
    if (spot_ == Spot::kDriver)
      pos_ = base_;
    else
      FollowSeatedSpot(camera);
    break;
  case State::kTransition:
    UpdateTransition(dt, camera);
    break;
  case State::kWalking:
    UpdateWalking(dt);
    break;
  }

  if (camera_override_.refreshes() != seen_refreshes_) {
    seen_refreshes_ = camera_override_.refreshes();
    held_rotation_ = base_rotation_.value_or(std::pair{base_yaw_, base_pitch_});
    hold_rotation_time_ = kHoldRotationSeconds;
  }

  camera_override_.KeepNearPlane(camera);
  SetMouseBlocked(state_ == State::kTransition && turn_);
  SetWalkKeysBlocked(!AtWheel());

  // Step bob, fading in and out with the walking speed.
  const float speed = state_ == State::kWalking
                          ? std::hypot(vel_x_.value(), vel_z_.value())
                          : 0.0f;
  const float amount =
      std::clamp(bob_amount_.Update(speed / kWalkSpeed, dt), 0.0f, 1.0f);
  const float phase_before = bob_phase_;
  bob_phase_ = math::WrapPhase(bob_phase_ + speed * dt / kStepMeters * kPi);
  // A footstep where the head dips lowest, a quarter-turn into each step:
  // where the phase's cosine changes sign.
  if (amount > kMinStepSoundAmount &&
      (std::cos(phase_before) > 0.0f) != (std::cos(bob_phase_) > 0.0f))
    footsteps_.push_back(kFootstepVolume * amount);
  const float sway = kHeadBob * kBobSwayRatio * amount * std::sin(bob_phase_);
  const float dip =
      kHeadBob * amount * 0.5f * (1.0f - std::cos(2.0f * bob_phase_));

  HeadOffset offset;
  offset.pos_x = pos_.x - base_.x + sway * CameraX() * std::cos(base_yaw_);
  offset.pos_y = pos_.y - base_.y - dip;
  offset.pos_z = pos_.z - base_.z - sway * std::sin(base_yaw_);
  offset.pitch = motion_pitch_deg_;
  offset.roll = kBobRollDegPerMeter * kHeadBob * amount * std::sin(bob_phase_) +
                motion_roll_deg_;

  // Upright against the truck's tilt, seen from where the head looks (yaw
  // positive to the left): the truck's roll is a roll looking ahead and a
  // pitch looking aside, its pitch the other way around.
  // Upright on the feet, with the cabin in a seat: it changes along with
  // the move between the two (standing up, sitting down), not after it.
  const float on_feet = state_ == State::kWalking ? 1.0f : 0.0f;
  const float upright =
      state_ == State::kTransition
          ? Lerp(upright_from_, after_ == State::kWalking ? 1.0f : 0.0f,
                 SmootherStep(progress_))
          : on_feet;
  upright_now_ = upright;
  if (upright > 1e-3f) {
    const auto tilt_deg = [](float turns) {
      return std::clamp(turns * 360.0f, -kMaxUprightDeg, kMaxUprightDeg);
    };
    const float pitch = tilt_deg(truck.world_placement.orientation.pitch);
    const float roll = tilt_deg(truck.world_placement.orientation.roll);
    const float c = std::cos(base_yaw_), s = std::sin(base_yaw_);
    offset.pitch += upright * (-pitch * c + roll * s);
    offset.roll += upright * kUprightRollSign * (roll * c + pitch * s);
  }
  return offset;
}

// --- Spots ---

CabinWalkEffect::SpotPose CabinWalkEffect::SpotAt(Spot spot) const {
  const CabinLayout &l = layout_;
  auto pose = [](float x, float y, float z, float yaw_deg, float pitch_deg) {
    return SpotPose{{x, y, z}, yaw_deg * kDegToRad, pitch_deg * kDegToRad};
  };
  switch (spot) {
  case Spot::kPassenger:
    // The player's own seat mirrored across the centerline: sat at the same
    // height, depth and distance from the center as at the wheel.
    return pose(2.0f * centerline_x_ - base_.x + l.passenger_dx,
                base_.y + l.passenger_dy, base_.z + l.passenger_dz,
                l.passenger_yaw, l.passenger_pitch);
  case Spot::kBunkSit:
    return pose(l.bunk_sit_x, l.bunk_sit_y, l.bunk_sit_z, l.bunk_sit_yaw,
                l.bunk_sit_pitch);
  case Spot::kDriver:
    break;
  }
  // The player's own seat, looking where the truck's recenter does.
  float yaw_deg = 0.0f, pitch_deg = 0.0f;
  if (camera_override_.engaged()) {
    yaw_deg = camera_override_.original_default_yaw();
    pitch_deg = camera_override_.original_default_pitch();
  }
  return {base_, yaw_deg * kDegToRad, pitch_deg * kDegToRad};
}

CabinWalkEffect::Vec3 CabinWalkEffect::Approach(Spot spot) const {
  const Vec3 p = SpotAt(spot).pos;
  return {std::clamp(p.x, layout_.floor_x_min, layout_.floor_x_max),
          layout_.stand_y,
          std::clamp(p.z, layout_.floor_z_min, layout_.floor_z_max)};
}

CabinWalkEffect::Spot CabinWalkEffect::LookedAtSpot() const {
  // The seat whose direction is closest to the look, on the floor plane:
  // the seats are too close together in most cabins to go by distance.
  const float look_x = -CameraX() * std::sin(base_yaw_);
  const float look_z = -std::cos(base_yaw_);
  Spot best = Spot::kDriver;
  float best_cos = -2.0f;
  for (Spot spot : {Spot::kDriver, Spot::kPassenger, Spot::kBunkSit}) {
    if (!CanSitAt(spot))
      continue;
    const Vec3 p = SpotAt(spot).pos;
    const float dx = p.x - pos_.x, dz = p.z - pos_.z;
    const float distance = std::hypot(dx, dz);
    // Standing on top of it, where the look points doesn't tell.
    const float cos = distance < kOverSpotMeters
                          ? 1.0f
                          : (dx * look_x + dz * look_z) / distance;
    if (cos > best_cos) {
      best_cos = cos;
      best = spot;
    }
  }
  return best;
}

// --- Keys ---

void CabinWalkEffect::HandleStandSitKey(float dt, const SPF_TruckData &truck,
                                        SPF_Camera_API *camera) {
  if (!keybinds_api_ || !keybinds_handle_)
    return;
  const bool held =
      keybinds::IsHeld(keybinds_api_, keybinds_handle_, keybinds::kStandSit);
  bool tap = false, long_press = false;
  if (held) {
    stand_sit_held_time_ = stand_sit_held_ ? stand_sit_held_time_ + dt : 0.0f;
    if (!stand_sit_long_done_ && stand_sit_held_time_ >= kLongPressSeconds) {
      long_press = true;
      stand_sit_long_done_ = true;
    }
  } else {
    tap = stand_sit_held_ && !stand_sit_long_done_;
    stand_sit_long_done_ = false;
  }
  stand_sit_held_ = held;

  switch (state_) {
  case State::kSeated:
    if (spot_ == Spot::kDriver) {
      if (tap || long_press)
        TryStandUp(truck, camera);
    } else if (long_press) {
      StandUp();
      walk_to_ = Spot::kDriver;
    } else if (tap) {
      StandUp();
    }
    break;
  case State::kWalking:
    if (long_press)
      walk_to_ = Spot::kDriver;
    else if (tap)
      walk_to_ = LookedAtSpot();
    break;
  case State::kTransition:
    break;
  }
}

void CabinWalkEffect::TryStandUp(const SPF_TruckData &truck,
                                 SPF_Camera_API *camera) {
  if (telemetry::SpeedKmh(truck) > kMaxStandKmh) {
    notice_ = Notice::kNeedsStop;
    return;
  }
  if (require_parking_brake_ && !truck.parking_brake) {
    notice_ = Notice::kNeedsParkingBrake;
    return;
  }
  if (!camera_override_.Engage(camera)) {
    // Camera not resolved yet: try again on the next press.
    return;
  }
  // Up from where the head really was: leaning toward a mirror, the truck's
  // own head offset for that look, now gone.
  const std::array<float, 3> &lean = camera_override_.engage_head_offset();
  pos_ = {base_.x + lean[0], base_.y + lean[1], base_.z + lean[2]};
  StandUp();
}

void CabinWalkEffect::StandUp() {
  const Vec3 to = Approach(spot_);
  StartTransition(to, Path::kRise, TransitionDuration(to), State::kWalking,
                  Spot::kDriver, false);
}

void CabinWalkEffect::SitAt(Spot spot) {
  const Vec3 to = SpotAt(spot).pos;
  StartTransition(to, Path::kSit, TransitionDuration(to), State::kSeated, spot,
                  true);
}

void CabinWalkEffect::GoToWheel(float duration) {
  walk_to_.reset();
  // From standing, it's sitting down; from another seat, moving over.
  const bool standing = state_ == State::kWalking;
  StartTransition(base_, standing ? Path::kSit : Path::kDirect, duration,
                  State::kSeated, Spot::kDriver, true);
}

// --- Transitions ---

CabinWalkEffect::Vec3 CabinWalkEffect::Facing(Spot spot) const {
  // Yaw 0 looks toward -z, positive yaw turns left.
  const float yaw = SpotAt(spot).yaw_rad;
  return Vec3{-CameraX() * std::sin(yaw), 0.0f, -std::cos(yaw)};
}

void CabinWalkEffect::StartTransition(const Vec3 &to, Path path, float duration,
                                      State after, Spot after_spot, bool turn) {
  from_ = pos_;
  to_ = to;
  upright_from_ = upright_now_;
  path_ = path;
  progress_ = 0.0f;
  duration_ = std::max(duration, 0.05f);
  after_ = after;
  after_spot_ = after_spot;
  turn_ = turn;
  yaw_from_ = WrapYaw(base_yaw_);
  pitch_from_ = base_pitch_;

  auto facing = [&](Spot spot) { return Facing(spot); };
  const float dx = to.x - from_.x, dy = to.y - from_.y, dz = to.z - from_.z;
  const float horizontal = std::hypot(dx, dz);
  switch (path) {
  case Path::kRise: {
    const Vec3 f = facing(spot_);
    ctrl1_ = {from_.x + f.x * kRiseLeanMeters, from_.y - kRiseDipMeters,
              from_.z + f.z * kRiseLeanMeters};
    ctrl2_offset_ = {0.0f, -kRiseEndLiftMeters, 0.0f};
    break;
  }
  case Path::kSit: {
    const Vec3 f = facing(after_spot);
    ctrl1_ = {from_.x + dx * kSitStepShare, from_.y,
              from_.z + dz * kSitStepShare};
    ctrl2_offset_ = {f.x * kSitLeanMeters, kSitAboveMeters,
                     f.z * kSitLeanMeters};
    break;
  }
  case Path::kDirect: {
    // Arcs over a sideways move, barely over a mostly vertical one.
    const float arc = kArcMeters * Clamp01(horizontal / kArcFullMeters) *
                      horizontal / (horizontal + std::fabs(dy) + 1e-3f);
    ctrl1_ = {from_.x + dx / 3.0f, from_.y + dy / 3.0f + arc,
              from_.z + dz / 3.0f};
    ctrl2_offset_ = {-dx / 3.0f, -dy / 3.0f + arc, -dz / 3.0f};
    break;
  }
  }

  // Sideways share of the move as seen from where the player looks, for
  // leaning into it on the way to a front seat (not the bunk).
  const bool front_seat =
      after == State::kSeated && after_spot != Spot::kBunkSit;
  const float right =
      CameraX() * dx * std::cos(base_yaw_) - dz * std::sin(base_yaw_);
  lean_side_ = front_seat
                   ? std::clamp(right / std::max(horizontal, 0.2f), -1.0f, 1.0f)
                   : 0.0f;

  state_ = State::kTransition;
}

float CabinWalkEffect::TransitionDuration(const Vec3 &from,
                                          const Vec3 &to) const {
  const float dx = to.x - from.x, dy = to.y - from.y, dz = to.z - from.z;
  const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
  return kTransitionSeconds * std::clamp(distance / kTransitionMeters,
                                         kTransitionScaleMin,
                                         kTransitionScaleMax);
}

CabinWalkEffect::Vec3 CabinWalkEffect::Bezier(const Vec3 &p0, const Vec3 &p1,
                                              const Vec3 &p2, const Vec3 &p3,
                                              float u) {
  const float a = (1.0f - u) * (1.0f - u) * (1.0f - u);
  const float b = 3.0f * (1.0f - u) * (1.0f - u) * u;
  const float c = 3.0f * (1.0f - u) * u * u;
  const float d = u * u * u;
  return {a * p0.x + b * p1.x + c * p2.x + d * p3.x,
          a * p0.y + b * p1.y + c * p2.y + d * p3.y,
          a * p0.z + b * p1.z + c * p2.z + d * p3.z};
}

void CabinWalkEffect::UpdateTransition(float dt, SPF_Camera_API *camera) {
  progress_ = std::min(1.0f, progress_ + dt / duration_);
  const float t = progress_;
  // The seat follows the player's own seat, should it change meanwhile.
  if (after_ == State::kSeated)
    to_ = SpotAt(after_spot_).pos;

  // Along the Bezier path, eased so the body starts and stops smoothly.
  const Vec3 ctrl2{to_.x + ctrl2_offset_.x, to_.y + ctrl2_offset_.y,
                   to_.z + ctrl2_offset_.z};
  pos_ = Bezier(from_, ctrl1_, ctrl2, to_, SmootherStep(t));

  // The body's own motion on top: looking down at where it goes, leaning
  // into a sideways move.
  const float look_down = path_ == Path::kRise  ? kRiseLookDownDeg
                          : path_ == Path::kSit ? kSitLookDownDeg
                                                : kDirectLookDownDeg;
  motion_pitch_deg_ = -look_down * Bell(t);
  motion_roll_deg_ = kLeanRollDeg * lean_side_ * Bell(t);

  if (turn_) {
    const SpotPose target = SpotAt(after_spot_);
    const float r = SmootherStep(t / kTurnShare);
    base_rotation_ = {Lerp(yaw_from_, WrapYaw(target.yaw_rad), r),
                      Lerp(pitch_from_, target.pitch_rad, r)};
  }

  if (progress_ >= 1.0f)
    OnArrived(camera);
}

void CabinWalkEffect::OnArrived(SPF_Camera_API *camera) {
  state_ = after_;
  pos_ = to_;
  motion_pitch_deg_ = 0.0f;
  motion_roll_deg_ = 0.0f;
  vel_x_.Reset();
  vel_z_.Reset();
  height_.Reset(pos_.y);
  const InteriorCameraOverride::Limits &own =
      camera_override_.original_limits();
  const InteriorCameraOverride::Limits away{kAwayYawLimitDeg, -kAwayYawLimitDeg,
                                            own.up, kAwayDownLimitDeg};
  if (state_ == State::kWalking) {
    camera_override_.SetLimits(camera, away);
    camera_override_.SetDefaults(camera,
                                 camera_override_.original_default_yaw(),
                                 camera_override_.original_default_pitch());
    return;
  }

  spot_ = after_spot_;
  if (spot_ == Spot::kDriver) {
    camera_override_.Restore(camera);
    return;
  }
  // The passenger seat mirrors the driver's view range; the bunk allows
  // looking all around, as standing does. The recenter key looks where the
  // spot does.
  if (spot_ == Spot::kPassenger)
    camera_override_.SetLimits(camera,
                               {-own.right, -own.left, own.up, own.down});
  else
    camera_override_.SetLimits(camera, away);
  const SpotPose pose = SpotAt(spot_);
  seated_yaw_deg_ = pose.yaw_rad / kDegToRad;
  seated_pitch_deg_ = pose.pitch_rad / kDegToRad;
  camera_override_.SetDefaults(camera, seated_yaw_deg_, seated_pitch_deg_);
}

void CabinWalkEffect::FollowSeatedSpot(SPF_Camera_API *camera) {
  const SpotPose pose = SpotAt(spot_);
  pos_ = pose.pos;
  const float yaw_deg = pose.yaw_rad / kDegToRad;
  const float pitch_deg = pose.pitch_rad / kDegToRad;
  if (yaw_deg == seated_yaw_deg_ && pitch_deg == seated_pitch_deg_)
    return;
  // The spot's look was edited: look that way, and recenter to it.
  seated_yaw_deg_ = yaw_deg;
  seated_pitch_deg_ = pitch_deg;
  base_rotation_ = {WrapYaw(pose.yaw_rad), pose.pitch_rad};
  camera_override_.SetDefaults(camera, yaw_deg, pitch_deg);
}

// --- Walking ---

float CabinWalkEffect::ClampToFloor(float value, float min, float max,
                                    float &velocity,
                                    math::SpringDamper1D &spring) const {
  const float clamped = std::clamp(value, min, max);
  if (clamped != value) {
    // Against a wall: slide along it instead of pushing into it.
    spring.Reset();
    velocity = 0.0f;
  }
  return clamped;
}

void CabinWalkEffect::UpdateWalking(float dt) {
  const auto amount = [&](const keybinds::Action &action) {
    return keybinds::Amount(keybinds_api_, keybinds_handle_, action);
  };
  float forward = amount(keybinds::kWalkForward) - amount(keybinds::kWalkBack);
  float side = amount(keybinds::kWalkRight) - amount(keybinds::kWalkLeft);
  const float crouch = amount(keybinds::kCrouch);
  const float length = std::hypot(forward, side);
  if (length > 1.0f) {
    forward /= length;
    side /= length;
  }
  if (length > 0.05f)
    walk_to_.reset(); // the player takes over
  if (forward < 0.0f)
    forward *= kBackwardShare;
  side *= kSidewaysShare;

  // Relative to where the player looks: yaw 0 looks toward -z, positive
  // yaw turns left (toward -x, flipped in a mirrored camera, see CameraX).
  const float speed = kWalkSpeed * (1.0f - kCrouchSlowdown * crouch);
  float want_x = CameraX() *
                 (-std::sin(base_yaw_) * forward + std::cos(base_yaw_) * side) *
                 speed;
  float want_z =
      (-std::cos(base_yaw_) * forward - std::sin(base_yaw_) * side) * speed;

  if (walk_to_) {
    const Vec3 target = Approach(*walk_to_);
    const float dx = target.x - pos_.x, dz = target.z - pos_.z;
    const float distance = std::hypot(dx, dz);
    if (distance < kArriveMeters) {
      const Spot spot = *walk_to_;
      walk_to_.reset();
      SitAt(spot);
      return;
    }
    const float approach = std::min(kWalkSpeed, distance * kArriveGain);
    want_x = dx / distance * approach;
    want_z = dz / distance * approach;
  }

  // Getting up to speed, or slowing down and stopping (quicker).
  const bool speeding_up =
      std::hypot(want_x, want_z) >= std::hypot(vel_x_.value(), vel_z_.value());
  const float response =
      speeding_up ? kAccelerationSeconds : kDecelerationSeconds;
  vel_x_.SetTimeConstant(response);
  vel_z_.SetTimeConstant(response);
  float vx = vel_x_.Update(want_x, dt);
  float vz = vel_z_.Update(want_z, dt);
  pos_.x = ClampToFloor(pos_.x + vx * dt, layout_.floor_x_min,
                        layout_.floor_x_max, vx, vel_x_);
  pos_.z = ClampToFloor(pos_.z + vz * dt, layout_.floor_z_min,
                        layout_.floor_z_max, vz, vel_z_);

  pos_.y = height_.Update(layout_.stand_y - crouch * layout_.crouch_depth, dt);
}

// --- Lifecycle ---

void CabinWalkEffect::OnTruckConstantsChanged(
    const SPF_TruckConstants &constants) {
  const std::string key = TruckLayoutKey(constants);
  const bool right_hand_drive = IsRightHandDrive(constants);
  // Another cabin on the same model can move the driver's seat.
  const float centerline_x = CabinCenterlineX(constants);
  if (key == truck_key_ && right_hand_drive == right_hand_drive_ &&
      centerline_x == centerline_x_)
    return;
  truck_key_ = key;
  right_hand_drive_ = right_hand_drive;
  camera_override_.SetRightHandDrive(right_hand_drive);
  centerline_x_ = centerline_x;
  truck_name_ = std::string(constants.brand) + " " + constants.name;
  LoadLayout();
  if (!AtWheel())
    SnapToWheel();
}

void CabinWalkEffect::OnLeftInterior() {
  SetWalkKeysBlocked(false);
  left_interior_ = true;
}

void CabinWalkEffect::SnapToWheel() {
  state_ = State::kSeated;
  spot_ = Spot::kDriver;
  walk_to_.reset();
  pos_ = base_;
  base_rotation_.reset();
  motion_pitch_deg_ = 0.0f;
  motion_roll_deg_ = 0.0f;
  camera_override_.Forget();
  SetMouseBlocked(false);
  SetWalkKeysBlocked(false);
}

void CabinWalkEffect::Shutdown(SPF_Camera_API *camera) {
  if (camera)
    camera_override_.Restore(camera);
  SetMouseBlocked(false);
  SetWalkKeysBlocked(false);
}

CabinWalkEffect::Notice CabinWalkEffect::TakeNotice() {
  return std::exchange(notice_, Notice::kNone);
}

void CabinWalkEffect::SetMouseBlocked(bool blocked) {
  if (blocked == mouse_blocked_ || !ui_api_)
    return;
  // Only the look axes: the view is being turned for the player.
  ui_api_->UI_SetMouseBlockState(blocked, false, false);
  mouse_blocked_ = blocked;
}

void CabinWalkEffect::SetWalkKeysBlocked(bool blocked) {
  if (blocked == walk_keys_blocked_ || !keybinds_api_ || !keybinds_handle_)
    return;
  // Takes effect from the next press: a key already held when the player
  // stands up keeps driving until released.
  for (const keybinds::Action &action : keybinds::kWalkActions)
    keybinds_api_->Kbind_SetBlockState(keybinds_handle_, action.id, blocked);
  walk_keys_blocked_ = blocked;
}

// --- Layout tuning ---

void CabinWalkEffect::HandleGoTo(Spot spot, const SPF_TruckData &truck,
                                 SPF_Camera_API *camera) {
  if (state_ == State::kTransition || !CanSitAt(spot))
    return;
  if (AtWheel()) {
    TryStandUp(truck, camera);
    if (state_ != State::kTransition)
      return; // refused, the notice says why
  } else if (state_ == State::kSeated) {
    if (spot_ == spot)
      return;
    StandUp();
  }
  walk_to_ = spot;
}

void CabinWalkEffect::SetLayout(const CabinLayout &layout, bool save) {
  layout_ = layout;
  // Sat in (or walking to) a spot just marked unusable: back on the feet.
  if (state_ == State::kSeated && !CanSitAt(spot_))
    StandUp();
  if (walk_to_ && !CanSitAt(*walk_to_))
    walk_to_.reset();
  if (save)
    SaveLayout();
}

void CabinWalkEffect::ResetLayout() {
  if (layouts_ && custom_layout_)
    layouts_->Forget(truck_key_);
  LoadLayout();
}

void CabinWalkEffect::LoadLayout() {
  // One layout per truck model, whichever its cabin or side: the player's
  // own, else the preset, else one built around the centerline. Made for
  // left-hand drive, it has the right positions for a right-hand drive
  // truck's mirrored camera, but its looks turn the wrong way.
  auto fit = [&](const CabinLayout &layout) {
    return right_hand_drive_ ? WithMirroredYaws(layout) : layout;
  };
  std::optional<CabinLayout> custom, preset;
  if (layouts_ && !truck_key_.empty()) {
    custom = layouts_->LoadCustom(truck_key_);
    preset = layouts_->LoadPreset(truck_key_);
  }
  shipped_layout_ = fit(preset.value_or(DefaultCabinLayout(centerline_x_)));
  layout_ = custom ? fit(*custom) : shipped_layout_;
  custom_layout_ = custom.has_value();
}

void CabinWalkEffect::SaveLayout() {
  NormalizeCabinLayout(layout_);
  custom_layout_ = true;
  if (truck_key_.empty() || !layouts_)
    return;
  // Saved as for left-hand drive, so both sides get it: a right-hand drive
  // truck's looks turned back (see CabinLayout).
  layouts_->Save(truck_key_,
                 right_hand_drive_ ? WithMirroredYaws(layout_) : layout_);
}

} // namespace motioncab
