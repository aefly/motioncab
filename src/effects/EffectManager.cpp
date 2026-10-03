#include "EffectManager.hpp"

#include "math/Units.hpp"

#include <algorithm>

namespace motioncab {

void EffectManager::Register(std::unique_ptr<Effect> effect) {
  effects_.push_back({std::move(effect)});
}

void EffectManager::LoadAllConfig() {
  for (auto &slot : effects_)
    slot.effect->LoadConfig();
}

void EffectManager::ResetAll() {
  for (auto &slot : effects_) {
    slot.effect->Reset();
    slot.fade = slot.effect->IsEnabled() ? 1.0f : 0.0f;
    slot.seat_reset = false;
  }
  seat_fade_ = at_wheel_ ? 1.0f : 0.0f;
}

HeadOffset EffectManager::UpdateAndAccumulate(float dt,
                                              const SPF_TruckData &truck,
                                              const SPF_Controls &controls) {
  const float step = dt / kFadeSeconds;
  seat_fade_ = at_wheel_ ? std::min(1.0f, seat_fade_ + step)
                         : std::max(0.0f, seat_fade_ - step);
  HeadOffset total;
  for (auto &slot : effects_) {
    Effect &effect = *slot.effect;
    float seat_weight = 1.0f;
    if (effect.NeedsDriverSeat()) {
      if (seat_fade_ <= 0.0f) {
        if (!slot.seat_reset)
          effect.Reset(); // start from rest back in the seat
        slot.seat_reset = true;
        continue;
      }
      slot.seat_reset = false;
      seat_weight = math::SmoothStep(seat_fade_);
    }
    if (effect.IsEnabled()) {
      slot.fade = std::min(1.0f, slot.fade + step);
    } else {
      if (slot.fade <= 0.0f)
        continue;
      slot.fade = std::max(0.0f, slot.fade - step);
      if (slot.fade <= 0.0f) {
        effect.Reset(); // start from rest once re-enabled
        continue;
      }
    }
    const HeadOffset contribution = effect.Update(dt, truck, controls);
    const float weight = math::SmoothStep(slot.fade) * seat_weight;
    total.pos_x += contribution.pos_x * weight;
    total.pos_y += contribution.pos_y * weight;
    total.pos_z += contribution.pos_z * weight;
    total.yaw += contribution.yaw * weight;
    total.pitch += contribution.pitch * weight;
    total.roll += contribution.roll * weight;
    total.fov += contribution.fov * weight;
  }
  return total;
}

void EffectManager::NotifyTruckConstants(const SPF_TruckConstants &constants) {
  for (auto &slot : effects_)
    slot.effect->OnTruckConstantsChanged(constants);
}

void EffectManager::NotifyCommonData(const SPF_CommonData &data) {
  for (auto &slot : effects_)
    slot.effect->OnCommonDataChanged(data);
}

void EffectManager::NotifyTrailers(const SPF_Trailer *trailers,
                                   uint32_t count) {
  for (auto &slot : effects_)
    slot.effect->OnTrailersChanged(trailers, count);
}

} // namespace motioncab
