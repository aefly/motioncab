#pragma once

#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "math/SpringDamper.hpp"

#include <array>
#include <random>

namespace motioncab {

// The head moving on its own, as with a head tracker: a noise tremor,
// micro-adjustments, resting postures and a tilt into the turn, summed. A
// move takes longer the farther it goes, and every rotation pivots on the
// neck, which moves the eyes too.
//
// Signs, as seen in game: yaw positive left, pitch negative down, roll
// positive right. A right-hand drive truck mirrors X and roll.
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

  // Poses are in -1..1 of the layer's range on each axis.
  struct Layer {
    Pose from{};
    Pose to{};
    float elapsed = 0.0f;
    float duration = 1.0f;
    float timer = 0.0f; // seconds until the next move

    float Value(int axis) const;
    // From wherever the layer is now, over `min_seconds` for no distance up
    // to `max_seconds` for a whole range.
    void MoveTo(const Pose &target, float min_seconds, float max_seconds);
    void Advance(float dt);
    void Reset();
  };

  float Jitter(float mean); // `mean` times a random 0.5..1.5
  void DrawMicro();
  void DrawPosture();

  float tremor_intensity_ = settings::Default(
      "settings.cabin.natural_head_movement.tremor_intensity");
  float micro_intensity_ =
      settings::Default("settings.cabin.natural_head_movement.micro_intensity");
  float posture_intensity_ = settings::Default(
      "settings.cabin.natural_head_movement.posture_intensity");
  float posture_interval_ = settings::Default(
      "settings.cabin.natural_head_movement.posture_interval");
  float steering_tilt_ =
      settings::Default("settings.cabin.natural_head_movement.steering_tilt");

  bool right_hand_drive_ = false;

  std::array<double, 2> tremor_phase_{}; // one per octave
  Layer micro_;
  Layer posture_;
  float stopped_for_ = 0.0f;
  // Stopped a while, the postures are wider and slower.
  bool idle_ = false;
  // The steering is a continuous input, so it gets a spring rather than a
  // Layer.
  math::SpringDamper1D steering_roll_;

  std::minstd_rand rng_;
};

} // namespace motioncab
