#pragma once

#include "Effect.hpp"
#include "math/SpringDamper.hpp"

#include "SPF_Config_API.h"
#include "ui/SettingsDefaults.hpp"

namespace motioncab {

// Vertical "seat on the road" motion from per-wheel suspension telemetry,
// plus an optional grade-follow (raises head downhill, lowers uphill) that
// cancels braking/accelerating dive using front/rear deflection asymmetry.
//
// The bump baseline is captured once at startup then frozen, not
// continuously re-tracked, so a sustained bump/hill can't get learned in
// as the new normal. Re-calibrated on truck swap or trailer (dis)connect.
class SuspensionEffect final : public Effect {
public:
  SuspensionEffect(SPF_Config_API *config_api, SPF_Config_Handle *config_handle)
      : config_api_(config_api), config_handle_(config_handle) {}

  const char *Name() const override { return "suspension"; }
  bool IsEnabled() const override { return enabled_; }
  void SetEnabled(bool enabled) override { enabled_ = enabled; }

  void LoadConfig() override;
  void Reset() override;
  HeadOffset Update(float dt, const SPF_TruckData &truck,
                    const SPF_Controls &controls) override;
  void OnTruckConstantsChanged(const SPF_TruckConstants &constants) override;
  void OnTrailersChanged(const SPF_Trailer *trailers, uint32_t count) override;

private:
  void RecalibrateBaseline();

  SPF_Config_API *config_api_;
  SPF_Config_Handle *config_handle_;

  bool enabled_ = true;
  float vertical_strength_ = defaults::kSuspensionVerticalStrength;
  float reactivity_ =
      defaults::kSuspensionReactivity; // seconds, spring time constant for
                                       // how quickly the seat follows the
                                       // road
  float grade_strength_ =
      defaults::kSuspensionGradeStrength; // 0 = disabled; see class comment

  uint32_t wheel_count_ =
      0; // from SPF_TruckConstants; array slots beyond this are unused

  // Front/rear classification for squat correction, from wheel position.z
  // (SCS vehicle space: Z = backward, so smaller z = further front).
  bool is_front_[SPF_TELEMETRY_WHEEL_MAX_COUNT] = {};
  float axle_separation_ =
      0.0f; // meters, avg rear z - avg front z; 0 = correction unavailable
            // (no clean front/rear split, e.g. single-axle layout)

  bool has_prev_trailer_state_ = false;
  bool prev_trailer_connected_ = false;

  float calibration_elapsed_ = 0.0f;
  bool baseline_locked_ = false;
  math::SpringDamper1D baseline_;       // captured once, then frozen
  math::SpringDamper1D delta_baseline_; // front-minus-rear deflection at
                                        // rest, captured alongside baseline_
  math::SpringDamper1D pitch_baseline_; // chassis's own resting pitch
                                        // (never exactly 0), same as above
  math::SpringDamper1D bump_y_;         // reactive vertical follow-through
  math::SpringDamper1D grade_y_;        // reactive grade-follow vertical offset
};

} // namespace motioncab
