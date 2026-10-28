#pragma once

#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "math/SpringDamper.hpp"

#include <array>
#include <random>

namespace motioncab {

// The head moving on its own, as with a head tracker: a slow noise sway,
// micro-adjustments, resting postures and a tilt into the turn, summed. A
// move takes longer the farther it goes, and every rotation pivots on the
// neck, which moves the eyes too.
//
// Signs, as seen in game: yaw positive left, pitch negative down, roll
// positive right. A right-hand drive truck mirrors X only.
class NaturalHeadMovementEffect final : public ConfigurableEffect {
public:
  NaturalHeadMovementEffect(SPF_Config_API *config_api,
                            SPF_Config_Handle *config_handle);

  void Reset() override;
  bool NeedsDriverSeat() const override { return true; }
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;
  void OnTruckConstantsChanged(const SPF_TruckConstants &constants) override;

private:
  void LoadSettings() override;

  // In HeadOffset's order.
  enum Axis { kX, kY, kZ, kYaw, kPitch, kRoll, kAxes };

  using Pose = std::array<float, kAxes>;

  // Poses are in -1..1 of the layer's range on each axis. A move starts at
  // the speed the previous one had reached, so cutting one short doesn't
  // stop the head dead before it sets off again. The head leads and the body
  // follows: the rotations finish early, a touch past the target before
  // settling back, while the translations start a moment later and run on a
  // little past its end.
  struct Layer {
    Pose from{};
    Pose to{};
    Pose velocity{}; // at `from`, per second
    float elapsed = 0.0f;
    float duration = 1.0f; // the move's; the body's part runs on past it
    float timer = 0.0f;    // seconds until the next move

    float Value(int axis) const;
    float Velocity(int axis) const;
    // Seconds until the move, body included, comes to rest.
    float Remaining() const;
    // From wherever the layer is now, over `min_seconds` for no distance up
    // to `max_seconds` for a whole range.
    void MoveTo(const Pose &target, float min_seconds, float max_seconds);
    void Advance(float dt);
    void Reset();

  private:
    // When the axis starts and ends its part of the move, in seconds.
    float Start(int axis) const;
    float End(int axis) const;
  };

  // What the truck is doing sets how the head moves. In the order of the
  // .cpp's kModes.
  enum class Mode { kStopped, kManeuvering, kTown, kHighway };

  Mode Assess(const SPF_TruckData &truck, float dt);
  float Jitter(float mean); // `mean` times a random 0.5..1.5
  // A random wait averaging `mean`, short ones the most likely.
  float Interval(float mean);
  float Pace();             // a move's duration factor, around 1
  void DrawMicro();
  // `seconds`, when set, is how long even the shortest move takes.
  void DrawPosture(float seconds = 0.0f);

  // The settings scale each mode's values: at their defaults, a mode plays
  // exactly its values. The roll's
  // covers every layer but the turn's tilt: tilting the horizon is what
  // turns stomachs, so it can be cut on its own.
  float sway_share_ = 1.0f;
  float micro_share_ = 1.0f;
  float micro_interval_share_ = 1.0f;
  float posture_share_ = 1.0f;
  float interval_share_ = 1.0f;
  float roll_share_ = 1.0f;
  float steering_tilt_ =
      settings::Default("settings.cabin.natural_head_movement.steering_tilt");

  bool right_hand_drive_ = false;

  std::array<double, 2> sway_phase_{}; // one per octave
  double drift_phase_ = 0.0;
  Layer micro_;
  Layer posture_;
  // The posture held, so the next one isn't the same, and a lean doesn't
  // swap sides without going through neutral first. Null when neutral.
  const float *posture_shape_ = nullptr;
  float posture_side_ = 0.0f;
  // A micro-adjustment waits for a posture change only once: with postures
  // close together, it kept waiting and never came.
  bool micro_waited_ = false;
  float stopped_for_ = 0.0f;
  float since_posture_ = 0.0f; // seconds since the last posture was drawn
  Mode mode_ = Mode::kTown;
  // Unset after a reset, so the first frame takes the truck's mode as it is
  // instead of changing posture for it.
  bool mode_known_ = false;
  // The mode's sway, micro-adjustments, drift and roll, eased so a change of
  // mode doesn't step them.
  math::SpringDamper1D sway_gain_;
  math::SpringDamper1D sway_pace_;
  math::SpringDamper1D micro_gain_;
  math::SpringDamper1D drift_gain_;
  math::SpringDamper1D roll_gain_;
  // The turn is a continuous input, so it gets a spring rather than a Layer.
  math::SpringDamper1D steering_roll_;

  std::minstd_rand rng_;
};

} // namespace motioncab
