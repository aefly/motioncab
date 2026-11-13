#include "CabinWalkEffect.hpp"

#include "core/Keybinds.hpp"
#include "effects/Telemetry.hpp"
#include "math/Units.hpp"

#include <algorithm>
#include <cmath>
#include <tuple>
#include <utility>

namespace motioncab {

using math::kDegToRad;
using math::kPi;
using math::Lerp;
using math::SmootherStep;

namespace {

constexpr float kWalkSpeed = 1.0f; // m/s
constexpr float kBackwardShare = 0.6f;
constexpr float kSidewaysShare = 0.75f;
constexpr float kAccelerationSeconds = 0.15f;
constexpr float kDecelerationSeconds = 0.1f;
constexpr float kHeadBob = 0.012f; // meters
constexpr float kFootstepVolume = 0.8f;
// More margin once up: a parked truck still creeps a little on a slope.
constexpr float kMaxStandKmh = 1.0f;
constexpr float kMaxAwayKmh = 3.0f;
constexpr float kLongPressSeconds = 0.6f;
// For kTransitionMeters, scaled by the distance within the bounds below.
constexpr float kTransitionSeconds = 1.2f;
constexpr float kTransitionMeters = 0.7f;
constexpr float kTransitionScaleMin = 0.7f;
constexpr float kTransitionScaleMax = 1.6f;
// Sent back to the wheel (parking brake released, truck rolling).
constexpr float kHurrySeconds = 0.8f;
// Walking there by itself, it slows down as it nears the spot.
constexpr float kArriveMeters = 0.04f;
constexpr float kArriveGain = 4.0f;
constexpr float kCrouchSlowdown = 0.5f;
constexpr float kHeightTime = 0.3f;
// At walking speed. Slower, the steps shorten as well as slow down, so the
// cadence drops with the square root of the speed rather than with it.
constexpr float kStepMeters = 0.45f;
constexpr float kMinStepShare = 0.45f;
constexpr float kBobSwayRatio = 0.6f; // of the dip
constexpr float kBobRollDegPerMeter = 40.0f;
constexpr float kStepNodDeg = 0.35f;
constexpr float kHoldRotationSeconds = 0.25f;
// Standing on a seat, a tap sits there whatever the look.
constexpr float kOverSpotMeters = 0.08f;
// Shuffling in place makes no sound.
constexpr float kMinStepSoundAmount = 0.3f;
constexpr float kBobTime = 0.2f;
// The body stays upright while the parked truck leans (half on a kerb, on a
// slope), within reason.
constexpr float kMaxUprightDeg = 15.0f;
// Seen in game: +1 leaned the head further with the truck.
constexpr float kUprightRollSign = -1.0f;
// The yaw is wrapped back into +-180 every frame, so this is only headroom
// the mouse never reaches.
constexpr float kAwayYawLimitDeg = 270.0f;
constexpr float kAwayDownLimitDeg = -80.0f;

// Out of a seat (kSeatExit), in one flowing move: the trunk
// folds forward and the head dips, eyes on the floor, while the hips are
// still on the seat; the legs then push up, the head carrying on forward
// and drifting toward the aisle; the first sidestep out to it (past the
// wheel, from the driver's) starts before the body is fully up. Shares
// below are of the rise.
constexpr float kSeatRiseForwardMeters = 0.15f;
constexpr float kSeatRiseShiftMeters = 0.10f;
constexpr float kSeatRiseDipMeters = 0.06f;
constexpr float kSeatRiseDipEnd = 0.6f;
constexpr float kSeatRiseLiftStart = 0.25f; // seat-off
constexpr float kSeatRiseForwardEnd = 0.8f;
// Sitting down, the player first comes forward within this share of the
// steps, then sidesteps in. With no steps (the floor right beside the
// seat), it takes this long, overlapping the lowering.
constexpr float kSeatSitForwardShare = 0.35f;
constexpr float kSeatSitForwardSeconds = 0.5f;
constexpr float kSeatRiseLookDownDeg = 12.0f;
constexpr float kSeatRiseLookDownPeak = 0.3f;
constexpr float kSeatRiseSlowdown = 1.5f;
// Closer than this, standing up ends on the floor already: no steps.
constexpr float kMinStepOutMeters = 0.1f;
constexpr float kStepOutSteps = 2.0f;
constexpr float kStepOutSecondsPerStep = 0.6f;
// The steps start this long before the body is fully up.
constexpr float kStepOutOverlapSeconds = 0.2f;
// Each step's footfall a little before its end, so the last one isn't lost
// as the move ends: the walking bob's footfall is where its phase crosses
// a quarter-turn.
constexpr float kStepOutPhaseLead = 0.4f * kPi;
constexpr float kStepOutLookDownDeg = 3.0f;
// The share of each step's sideways move that comes in a surge as the foot
// lands, the rest spread evenly so it never stops dead.
constexpr float kStepSurgeShare = 0.6f;
// Sitting down steps over at standing height first, then lowers from a bit
// in front of and above the seat, settling back against the backrest.
constexpr float kSitStepShare = 0.4f;
constexpr float kSitLeanMeters = 0.12f;
constexpr float kSitAboveMeters = 0.10f;
// Moving between seats arcs over, more for a longer move.
constexpr float kArcMeters = 0.05f;
constexpr float kArcFullMeters = 0.6f;
// At their peak, late sitting down, as the trunk folds over the lowering.
constexpr float kSitLookDownDeg = 10.0f;
constexpr float kDirectLookDownDeg = 4.0f;
constexpr float kSitLookDownPeak = 0.6f;
constexpr float kLeanRollDeg = 3.0f;
// The view finishes turning a bit before the head gets there.
constexpr float kTurnShare = 0.85f;
// Getting up or down, the body moves over the feet (or the seat) before the
// legs lift or lower it: the horizontal part leads the vertical one by this
// share of the move.
constexpr float kVerticalLagShare = 0.12f;

float Clamp01(float t) { return std::clamp(t, 0.0f, 1.0f); }

// 1 at `peak`, 0 at both ends with zero slope.
float Bell(float t, float peak = 0.5f) {
  t = Clamp01(t);
  const float u =
      t < peak ? 0.5f * t / peak : 0.5f + 0.5f * (t - peak) / (1.0f - peak);
  const float s = std::sin(kPi * u);
  return s * s;
}

// So a turn never goes across the back, where it would jump the yaw limits.
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

  // Past a half-turn, the same look a full turn back, so the mouse never
  // reaches the limit. Not in the passenger seat, which keeps the truck's
  // own limits: they may go past 180 on purpose.
  const bool full_turn =
      !AtWheel() && !(state_ == State::kSeated && spot_ == Spot::kPassenger);
  if (full_turn && std::fabs(base_yaw_) > kPi) {
    base_yaw_ = WrapYaw(base_yaw_);
    base_rotation_ = {base_yaw_, base_pitch_};
  }

  const bool heading_to_wheel = state_ == State::kTransition &&
                                after_ == State::kSeated &&
                                after_spot_ == Spot::kDriver;
  if (!AtWheel() && !heading_to_wheel) {
    const bool unsafe = telemetry::SpeedKmh(truck) > kMaxAwayKmh ||
                        (require_parking_brake_ && !truck.parking_brake);
    if (unsafe && back_in_cabin) {
      // Driven off from another camera: no walk back to the wheel now.
      notice_ = Notice::kBackToWheel;
      camera_override_.Restore(camera);
      SnapToWheel();
    } else if (unsafe || !enabled_) {
      if (enabled_)
        notice_ = Notice::kBackToWheel;
      GoToWheel(unsafe ? kHurrySeconds : TransitionDuration(base_), unsafe);
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

  const float speed = state_ == State::kWalking
                          ? std::hypot(vel_x_.value(), vel_z_.value())
                          : 0.0f;
  const bool stepping_out = state_ == State::kTransition && SeatPath() &&
                            seat_has_steps_ && progress_ >= seat_steps_begin_ &&
                            progress_ <= seat_steps_end_;
  const float amount = std::clamp(
      bob_amount_.Update(stepping_out ? 1.0f : speed / kWalkSpeed, dt), 0.0f,
      1.0f);
  const float phase_before = bob_phase_;
  if (stepping_out) {
    // Timed on the move's steps, each one landing as it ends.
    bob_phase_ = kStepOutSteps * kPi * seat_step_u_ - kStepOutPhaseLead;
  } else {
    const float step_meters =
        kStepMeters *
        std::clamp(std::sqrt(speed / kWalkSpeed), kMinStepShare, 1.0f);
    bob_phase_ = math::WrapPhase(bob_phase_ + speed * dt / step_meters * kPi);
  }
  // A footstep where the head dips lowest, where the phase's cosine changes
  // sign.
  if (amount > kMinStepSoundAmount &&
      (std::cos(phase_before) > 0.0f) != (std::cos(bob_phase_) > 0.0f))
    footsteps_.push_back(kFootstepVolume * amount);
  const float sway = kHeadBob * kBobSwayRatio * amount * std::sin(bob_phase_);
  const float dip_share = 0.5f * (1.0f - std::cos(2.0f * bob_phase_));
  const float dip = kHeadBob * amount * dip_share;

  HeadOffset offset;
  offset.pos_x = pos_.x - base_.x + sway * CameraX() * std::cos(base_yaw_);
  offset.pos_y = pos_.y - base_.y - dip;
  offset.pos_z = pos_.z - base_.z - sway * std::sin(base_yaw_);
  offset.pitch = motion_pitch_deg_ - kStepNodDeg * amount * dip_share;
  offset.roll = kBobRollDegPerMeter * kHeadBob * amount * std::sin(bob_phase_) +
                motion_roll_deg_;

  // Upright on the feet, tilted with the cabin in a seat, changing along with
  // the move between the two. The truck's roll is a roll looking ahead and a
  // pitch looking aside, its pitch the other way around.
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
    // The player's own seat mirrored across the centerline.
    return pose(2.0f * centerline_x_ - base_.x + l.passenger_dx,
                base_.y + l.passenger_dy, base_.z + l.passenger_dz,
                l.passenger_yaw, l.passenger_pitch);
  case Spot::kBunkSit:
    return pose(l.bunk_sit_x, l.bunk_sit_y, l.bunk_sit_z, l.bunk_sit_yaw,
                l.bunk_sit_pitch);
  case Spot::kDriver:
    break;
  }
  // Looking where the game's recenter does.
  float yaw_deg = 0.0f, pitch_deg = 0.0f;
  if (camera_override_.engaged()) {
    yaw_deg = camera_override_.original_default_yaw();
    pitch_deg = camera_override_.original_default_pitch();
  }
  return {base_, yaw_deg * kDegToRad, pitch_deg * kDegToRad};
}

CabinWalkEffect::Vec3 CabinWalkEffect::Approach(Spot spot) const {
  const Vec3 p = SpotAt(spot).pos;
  const auto floor_x = [&](float x) {
    return std::clamp(x, layout_.floor_x_min, layout_.floor_x_max);
  };
  float x = floor_x(p.x);
  // In the aisle, mirroring the driver's, so getting in or out takes the
  // same sidesteps. The passenger seat is past the centerline, at +x.
  if (spot == Spot::kPassenger)
    x = std::min(x, floor_x(2.0f * centerline_x_ - floor_x(base_.x)));
  return {x, layout_.stand_y,
          std::clamp(p.z, layout_.floor_z_min, layout_.floor_z_max)};
}

CabinWalkEffect::Spot CabinWalkEffect::LookedAtSpot() const {
  // The seats are too close together in most cabins to go by distance.
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
    return; // the camera isn't resolved yet: next press
  }
  // Up from where the head really was: looking at a mirror leans it, which
  // Engage just took away.
  const std::array<float, 3> &lean = camera_override_.engage_head_offset();
  pos_ = {base_.x + lean[0], base_.y + lean[1], base_.z + lean[2]};
  StandUp();
}

void CabinWalkEffect::StandUp() {
  const Vec3 out = Approach(spot_);
  const Vec3 f = Facing(spot_);
  const Vec3 to{std::clamp(out.x + f.x * kSeatRiseForwardMeters,
                           layout_.floor_x_min, layout_.floor_x_max),
                out.y,
                std::clamp(pos_.z + f.z * kSeatRiseForwardMeters,
                           layout_.floor_z_min, layout_.floor_z_max)};
  const float up_x = pos_.x + std::clamp(to.x - pos_.x, -kSeatRiseShiftMeters,
                                         kSeatRiseShiftMeters);
  const float rise_seconds =
      kSeatRiseSlowdown * TransitionDuration(pos_, {up_x, to.y, to.z});
  const bool steps = std::fabs(to.x - up_x) >= kMinStepOutMeters;
  const float step_seconds = kStepOutSteps * kStepOutSecondsPerStep;
  const float total = steps ? rise_seconds + step_seconds -
                                  std::min(kStepOutOverlapSeconds, rise_seconds)
                            : rise_seconds;
  StartTransition(to, Path::kSeatExit, total, State::kWalking, Spot::kDriver,
                  false);
  // Up beside the seat, then out along x only: the steps keep the forward z.
  seat_up_ = steps ? Vec3{up_x, to.y, to.z} : to;
  seat_has_steps_ = steps;
  seat_dip_meters_ = SeatDip(spot_);
  seat_rise_begin_ = 0.0f;
  seat_rise_end_ = rise_seconds / total;
  seat_steps_begin_ = steps ? 1.0f - step_seconds / total : 1.0f;
  seat_steps_end_ = 1.0f;
  seat_step_u_ = 0.0f;
}

void CabinWalkEffect::SitAt(Spot spot) {
  walk_to_.reset();
  // Getting up played backwards.
  const Vec3 seat = SpotAt(spot).pos;
  const Vec3 f = Facing(spot);
  const float forward_z = std::clamp(seat.z + f.z * kSeatRiseForwardMeters,
                                     layout_.floor_z_min, layout_.floor_z_max);
  const float down_x =
      seat.x +
      std::clamp(pos_.x - seat.x, -kSeatRiseShiftMeters, kSeatRiseShiftMeters);
  const bool steps = std::fabs(pos_.x - down_x) >= kMinStepOutMeters;
  const Vec3 down{steps ? down_x : pos_.x, pos_.y, forward_z};
  const float lower_seconds =
      kSeatRiseSlowdown * TransitionDuration(down, seat);
  const float lead_seconds =
      steps ? kStepOutSteps * kStepOutSecondsPerStep : kSeatSitForwardSeconds;
  const float total = lower_seconds + lead_seconds -
                      std::min(kStepOutOverlapSeconds, lower_seconds);
  StartTransition(seat, Path::kSeatEnter, total, State::kSeated, spot, true);
  seat_up_ = down;
  seat_has_steps_ = steps;
  seat_dip_meters_ = SeatDip(spot);
  seat_steps_begin_ = 0.0f;
  seat_steps_end_ = steps ? lead_seconds / total : 0.0f;
  seat_forward_begin_ = 0.0f;
  seat_forward_end_ =
      steps ? seat_steps_end_ * kSeatSitForwardShare : lead_seconds / total;
  seat_rise_begin_ = 1.0f - lower_seconds / total;
  seat_rise_end_ = 1.0f;
  seat_step_u_ = 0.0f;
}

void CabinWalkEffect::GoToWheel(float duration, bool hurry) {
  walk_to_.reset();
  // From standing, it's sitting down; from another seat, moving over.
  const bool standing = state_ == State::kWalking;
  if (standing && !hurry) {
    SitAt(Spot::kDriver);
    return;
  }
  StartTransition(base_, standing ? Path::kSit : Path::kDirect, duration,
                  State::kSeated, Spot::kDriver, true);
}

// --- Transitions ---

CabinWalkEffect::Vec3 CabinWalkEffect::Facing(Spot spot) const {
  // Yaw 0 looks toward -z, positive is left.
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
  if (path == Path::kSeatExit || path == Path::kSeatEnter)
    bob_phase_ = -kStepOutPhaseLead;

  auto facing = [&](Spot spot) { return Facing(spot); };
  const float dx = to.x - from_.x, dy = to.y - from_.y, dz = to.z - from_.z;
  const float horizontal = std::hypot(dx, dz);
  switch (path) {
  case Path::kSeatExit:
  case Path::kSeatEnter:
    break; // keyframed in UpdateTransition
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

  // Not onto the bunk.
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

float CabinWalkEffect::SeatDip(Spot spot) {
  return spot == Spot::kBunkSit ? 0.0f : kSeatRiseDipMeters;
}

float CabinWalkEffect::SeatRise(float t) const {
  const float span = seat_rise_end_ - seat_rise_begin_;
  const float r = span > 0.0f ? Clamp01((t - seat_rise_begin_) / span) : 1.0f;
  return path_ == Path::kSeatEnter ? 1.0f - r : r;
}

CabinWalkEffect::Vec3 CabinWalkEffect::SeatPose(float t) {
  const bool entering = path_ == Path::kSeatEnter;
  const Vec3 &seat = entering ? to_ : from_;
  const Vec3 &floor = entering ? from_ : to_;
  const Vec3 &up = seat_up_;
  // How far from `up` toward the floor the steps have taken the body, and
  // the forward part of it.
  float out = 0.0f, out_z = 0.0f;
  if (seat_has_steps_) {
    seat_step_u_ = Clamp01((t - seat_steps_begin_) /
                           (seat_steps_end_ - seat_steps_begin_));
    const float steps = seat_step_u_ * kStepOutSteps;
    const float step = std::min(std::floor(steps), kStepOutSteps - 1.0f);
    const float surges =
        (step + math::SmoothStep(Clamp01(steps - step))) / kStepOutSteps;
    const float stepped =
        Lerp(SmootherStep(seat_step_u_), surges, kStepSurgeShare);
    out = entering ? 1.0f - stepped : stepped;
  }
  out_z = out;
  if (entering) {
    const float span = seat_forward_end_ - seat_forward_begin_;
    out_z = span > 0.0f ? 1.0f - SmootherStep((t - seat_forward_begin_) / span)
                        : 0.0f;
  }
  const float rise = SeatRise(t);
  return {seat.x + (up.x - seat.x) * SmootherStep(rise) +
              (floor.x - up.x) * out,
          Lerp(seat.y, up.y,
               SmootherStep((rise - kSeatRiseLiftStart) /
                            (1.0f - kSeatRiseLiftStart))) -
              seat_dip_meters_ * Bell(rise / kSeatRiseDipEnd) +
              (floor.y - up.y) * out,
          Lerp(seat.z, up.z, SmootherStep(rise / kSeatRiseForwardEnd)) +
              (floor.z - up.z) * out_z};
}

void CabinWalkEffect::UpdateTransition(float dt, SPF_Camera_API *camera) {
  progress_ = std::min(1.0f, progress_ + dt / duration_);
  const float t = progress_;
  // The player's own seat may change meanwhile.
  if (after_ == State::kSeated)
    to_ = SpotAt(after_spot_).pos;

  const Vec3 ctrl2{to_.x + ctrl2_offset_.x, to_.y + ctrl2_offset_.y,
                   to_.z + ctrl2_offset_.z};
  if (path_ == Path::kDirect) {
    pos_ = Bezier(from_, ctrl1_, ctrl2, to_, SmootherStep(t));
  } else if (SeatPath()) {
    pos_ = SeatPose(t);
  } else {
    const float span = 1.0f - kVerticalLagShare;
    const Vec3 lead = Bezier(from_, ctrl1_, ctrl2, to_, SmootherStep(t / span));
    const Vec3 lag = Bezier(from_, ctrl1_, ctrl2, to_,
                            SmootherStep((t - kVerticalLagShare) / span));
    pos_ = {lead.x, lag.y, lead.z};
  }

  float look_down = kDirectLookDownDeg, peak = 0.5f;
  if (path_ == Path::kSit) {
    look_down = kSitLookDownDeg;
    peak = kSitLookDownPeak;
  }
  motion_pitch_deg_ = -look_down * Bell(t, peak);
  if (SeatPath())
    motion_pitch_deg_ =
        -kSeatRiseLookDownDeg * Bell(SeatRise(t), kSeatRiseLookDownPeak) -
        (seat_has_steps_ ? kStepOutLookDownDeg * Bell(seat_step_u_) : 0.0f);
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
  // The passenger seat mirrors the driver's view range; the bunk looks all
  // around, as standing does.
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
    // Against a wall: slide along it rather than pushing into it.
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

  // Relative to the look: yaw 0 looks toward -z, positive is left.
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
  const std::string key = telemetry::TruckKey(constants);
  const bool right_hand_drive = telemetry::IsRightHandDrive(constants);
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
  // Only the look axes, while we turn the view for the player.
  ui_api_->UI_SetMouseBlockState(blocked, false, false);
  mouse_blocked_ = blocked;
}

void CabinWalkEffect::SetWalkKeysBlocked(bool blocked) {
  if (blocked == walk_keys_blocked_ || !keybinds_api_ || !keybinds_handle_)
    return;
  // From the next press: a key already held keeps reaching the game until
  // released.
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
  // One layout per truck model, whichever its cabin or side. Made for
  // left-hand drive, its positions suit a right-hand drive truck's mirrored
  // camera, but its looks turn the wrong way.
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
  // Saved as for left-hand drive, so both sides get it.
  layouts_->Save(truck_key_,
                 right_hand_drive_ ? WithMirroredYaws(layout_) : layout_);
}

} // namespace motioncab
