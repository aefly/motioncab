#include "EngineStartStopEffect.hpp"

#include "effects/Telemetry.hpp"
#include "math/Units.hpp"

#include <cmath>
#include <cstring>

namespace motioncab {

namespace {
constexpr float kBaseVerticalAmplitude = 0.006f; // meters
constexpr float kBasePitchAmplitude = 1.2f;      // degrees
constexpr float kJudderFrequencyHz = 8.0f;
// The stop's shudder is shorter and weaker than the start's.
constexpr float kStopDurationRatio = 0.6f;
constexpr float kStopAmplitudeRatio = 0.7f;
} // namespace

void EngineStartStopEffect::LoadSettings() {
  intensity_ = Float("intensity", intensity_);
  duration_ = Float("duration", duration_);
}

void EngineStartStopEffect::Reset() {
  shake_timer_ = 0.0f;
  phase_ = 0.0f;
  has_prev_state_ = false;
}

void EngineStartStopEffect::OnTruckConstantsChanged(
    const SPF_TruckConstants &constants) {
  is_electric_ = telemetry::IsElectric(constants);

  // This fires for more than a truck switch (trailer, Quick Job reload...),
  // and dropping the previous reading for nothing could miss a start right
  // then.
  const bool truck_actually_changed =
      std::strncmp(prev_truck_id_, constants.id, sizeof(prev_truck_id_)) != 0;
  std::strncpy(prev_truck_id_, constants.id, sizeof(prev_truck_id_) - 1);
  prev_truck_id_[sizeof(prev_truck_id_) - 1] = '\0';
  if (!truck_actually_changed)
    return;

  // The old truck's last readings could hide the new one's start, e.g. an
  // RPM already over the threshold.
  has_prev_state_ = false;
  shake_timer_ = 0.0f;
}

HeadOffset EngineStartStopEffect::Update(float dt, const SPF_TruckData &truck,
                                         const SPF_Controls & /*controls*/) {
  const bool engine_enabled_now = truck.engine_enabled;
  const float rpm = truck.engine_rpm;

  if (!has_prev_state_) {
    // No shudder for whatever state the engine is already in.
    prev_engine_enabled_ = engine_enabled_now;
    prev_rpm_ = rpm;
    has_prev_state_ = true;
  } else {
    if (prev_rpm_ <= telemetry::kRpmStartThreshold &&
        rpm > telemetry::kRpmStartThreshold) {
      shake_total_duration_ = duration_;
      shake_amplitude_scale_ = 1.0f;
      shake_timer_ = shake_total_duration_;
      phase_ = 0.0f;
    } else if (!engine_enabled_now && prev_engine_enabled_) {
      shake_total_duration_ = duration_ * kStopDurationRatio;
      shake_amplitude_scale_ = kStopAmplitudeRatio;
      shake_timer_ = shake_total_duration_;
      phase_ = 0.0f;
    }
    prev_engine_enabled_ = engine_enabled_now;
  }
  prev_rpm_ = rpm;

  if (shake_timer_ <= 0.0f)
    return {};

  shake_timer_ -= dt;
  const float envelope =
      shake_timer_ > 0.0f ? shake_timer_ / shake_total_duration_ : 0.0f;

  phase_ += math::kTwoPi * kJudderFrequencyHz * dt;
  const float wave = std::sin(phase_);

  const float amplitude_factor = intensity_ * shake_amplitude_scale_ * envelope;

  HeadOffset offset;
  offset.pos_y = kBaseVerticalAmplitude * amplitude_factor * wave;
  offset.pitch = kBasePitchAmplitude * amplitude_factor * wave;
  return offset;
}

} // namespace motioncab
