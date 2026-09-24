#include "EngineStartStopEffect.hpp"

#include <cmath>
#include <cstring>
#include <numbers>

namespace motioncab {

namespace {
constexpr float kBaseVerticalAmplitude = 0.006f; // meters
constexpr float kBasePitchAmplitude = 1.2f;      // degrees
constexpr float kJudderFrequencyHz = 8.0f;       // Hz
constexpr float kStopDurationRatio = 0.6f;  // of duration_, for the stop shake
constexpr float kStopAmplitudeRatio = 0.7f; // of intensity_, for the stop shake
} // namespace

void EngineStartStopEffect::LoadConfig() {
  if (!config_api_ || !config_handle_)
    return;

  enabled_ = config_api_->Cfg_GetBool(
      config_handle_, "settings.cabin.engine_start_stop.enabled", enabled_);
  intensity_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.cabin.engine_start_stop.intensity",
      intensity_));
  duration_ = static_cast<float>(config_api_->Cfg_GetFloat(
      config_handle_, "settings.cabin.engine_start_stop.duration", duration_));
}

void EngineStartStopEffect::Reset() {
  shake_timer_ = 0.0f;
  phase_ = 0.0f;
  has_prev_state_ = false;
}

void EngineStartStopEffect::OnTruckConstantsChanged(
    const SPF_TruckConstants &constants) {
  is_electric_ = constants.adblue_capacity <= 0.01f;

  // OnTruckConstantsChanged fires for more than just a truck purchase
  // (trailer (dis)connect, a Quick Job cancel reload, etc.), so only
  // re-baseline when the truck model actually changed, otherwise a real
  // RPM crossing in flight at the exact same moment would silently lose
  // its "previous" reading and never fire the start shudder.
  const bool truck_actually_changed =
      std::strncmp(prev_truck_id_, constants.id, sizeof(prev_truck_id_)) != 0;
  std::strncpy(prev_truck_id_, constants.id, sizeof(prev_truck_id_) - 1);
  prev_truck_id_[sizeof(prev_truck_id_) - 1] = '\0';
  if (!truck_actually_changed)
    return;

  // A truck swap leaves prev_rpm_/prev_engine_enabled_ holding the OLD
  // truck's last values, which can make the new truck's start/stop edge
  // undetectable (e.g. old rpm already above threshold, so the new
  // engine's climb through it never reads as a crossing). Re-baseline the
  // same way Reset() does, matching SuspensionEffect's truck-swap handling.
  has_prev_state_ = false;
  shake_timer_ = 0.0f;
}

HeadOffset EngineStartStopEffect::Update(float dt, const SPF_TruckData &truck,
                                         const SPF_Controls & /*controls*/) {
  const bool engine_enabled_now = truck.engine_enabled;
  const float rpm = truck.engine_rpm;

  if (!has_prev_state_) {
    // First frame: just observe, don't fire a shudder for whatever state
    // the engine happens to already be in when the plugin activates.
    prev_engine_enabled_ = engine_enabled_now;
    prev_rpm_ = rpm;
    has_prev_state_ = true;
  } else {
    if (prev_rpm_ <= kRpmStartThreshold && rpm > kRpmStartThreshold) {
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

  phase_ += 2.0f * std::numbers::pi_v<float> * kJudderFrequencyHz * dt;
  const float wave = std::sin(phase_);

  const float amplitude_factor = intensity_ * shake_amplitude_scale_ * envelope;

  HeadOffset offset;
  offset.pos_y = kBaseVerticalAmplitude * amplitude_factor * wave;
  offset.pitch = kBasePitchAmplitude * amplitude_factor * wave;
  return offset;
}

} // namespace motioncab
