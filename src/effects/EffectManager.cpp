#include "EffectManager.hpp"

#include "core/Conflicts.hpp"
#include "math/Units.hpp"

#include <algorithm>

namespace motioncab {

void EffectManager::Register(std::unique_ptr<Effect> effect) {
  effects_.push_back({std::move(effect)});
  // The new one can pause, or be paused by, any other.
  for (size_t i = 0; i < effects_.size(); ++i) {
    effects_[i].paused_by.clear();
    for (size_t j = 0; j < effects_.size(); ++j) {
      if (conflicts::Pauses(effects_[j].effect->Id(), effects_[i].effect->Id()))
        effects_[i].paused_by.push_back(j);
    }
  }
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
    slot.pause_fade = IsPaused(slot) ? 0.0f : 1.0f;
    slot.pause_reset = false;
  }
  seat_fade_ = at_wheel_ ? 1.0f : 0.0f;
}

bool EffectManager::IsPaused(const Slot &slot) const {
  // Only while the pausing effect actually plays: one that needs the
  // driver's seat lets the others back while the player is up.
  return std::ranges::any_of(slot.paused_by, [this](size_t i) {
    const Effect &pauser = *effects_[i].effect;
    return pauser.IsEnabled() && (at_wheel_ || !pauser.NeedsDriverSeat());
  });
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
    // The toggle's fade keeps moving while the seat or a pause holds the
    // effect back: an effect turned off while paused would otherwise play for
    // a moment once unpaused.
    if (effect.IsEnabled()) {
      slot.fade = std::min(1.0f, slot.fade + step);
    } else if (slot.fade > 0.0f) {
      slot.fade = std::max(0.0f, slot.fade - step);
      if (slot.fade <= 0.0f)
        effect.Reset();
    }
    slot.pause_fade = IsPaused(slot) ? std::max(0.0f, slot.pause_fade - step)
                                     : std::min(1.0f, slot.pause_fade + step);
    if (slot.fade <= 0.0f)
      continue;
    float context_weight = 1.0f;
    if (effect.NeedsDriverSeat()) {
      if (seat_fade_ <= 0.0f) {
        if (!slot.seat_reset)
          effect.Reset();
        slot.seat_reset = true;
        continue;
      }
      slot.seat_reset = false;
      context_weight = math::SmoothStep(seat_fade_);
    }
    if (slot.pause_fade <= 0.0f) {
      if (!slot.pause_reset)
        effect.Reset();
      slot.pause_reset = true;
      continue;
    }
    slot.pause_reset = false;
    context_weight *= math::SmoothStep(slot.pause_fade);
    const HeadOffset contribution = effect.Update(dt, truck, controls);
    const float weight = math::SmoothStep(slot.fade) * context_weight;
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
