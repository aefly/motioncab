#pragma once

#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "math/SpringDamper.hpp"

#include <array>
#include <random>

namespace motioncab {

// Moves the head on its own, as with a head tracker. Summed layers: a
// noise tremor, micro-adjustments, resting postures (body shapes, wider and
// slower once stopped a while than driving) and a tilt into the turn with
// the steering wheel. Moves ease with math::SmootherStep, longer the
// farther they go (bar the switch between idle and driving, a set time),
// and every rotation pivots on the neck, which moves the eyes too. Pauses
// Idle Breathing and Speed Shake (conflicts::kPauses).
// SCS local space: X right, Y up, Z back; yaw positive left, pitch
// negative down, roll positive right (X and roll mirrored on a right-hand
// drive truck).
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

  // The axes the head moves along, in HeadOffset's order.
  enum Axis { kX, kY, kZ, kYaw, kPitch, kRoll, kAxes };

  using Pose = std::array<float, kAxes>;

  // A layer's pose per axis, in -1..1 of its range, moving from `from` to
  // `to` over `duration` seconds.
  struct Layer {
    Pose from{};
    Pose to{};
    float elapsed = 0.0f;
    float duration = 1.0f;
    float timer = 0.0f; // seconds until the next move

    float Value(int axis) const;
    // Starts a move from where the layer is now to `target`, over
    // `min_seconds` for no distance up to `max_seconds` for a whole range.
    void MoveTo(const Pose &target, float min_seconds, float max_seconds);
    void Advance(float dt);
    void Reset();
  };

  float Jitter(float mean); // `mean` times a random 0.5..1.5
  void DrawMicro();
  // Draws a posture of the current mode and moves to it, and sets the time
  // until the next.
  void DrawPosture();

  float tremor_intensity_ = settings::Default(
      "settings.cabin.natural_head_movement.tremor_intensity");
  float micro_intensity_ =
      settings::Default("settings.cabin.natural_head_movement.micro_intensity");
  float posture_intensity_ = settings::Default(
      "settings.cabin.natural_head_movement.posture_intensity");
  // seconds, mean time between two postures
  float posture_interval_ = settings::Default(
      "settings.cabin.natural_head_movement.posture_interval");
  // scales the tilt into the turn
  float steering_tilt_ =
      settings::Default("settings.cabin.natural_head_movement.steering_tilt");

  // The camera's X is mirrored on a right-hand drive truck.
  bool right_hand_drive_ = false;

  // The tremor's two noise octaves, in noise cells, wrapped with
  // math::WrapNoisePhase since they run for the whole session.
  std::array<double, 2> tremor_phase_{};
  Layer micro_;
  Layer posture_;
  float stopped_for_ = 0.0f; // seconds under the idle speed
  bool idle_ = false;        // the posture mode, see the class comment
  // Degrees of roll, following the steering wheel: a continuous input, so a
  // spring rather than a Layer's moves.
  math::SpringDamper1D steering_roll_;

  std::minstd_rand rng_;
};

} // namespace motioncab
