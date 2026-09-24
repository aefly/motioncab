#include "EffectManager.hpp"

namespace motioncab {

void EffectManager::Register(std::unique_ptr<Effect> effect) {
  effects_.push_back(std::move(effect));
}

void EffectManager::LoadAllConfig() {
  for (auto &effect : effects_)
    effect->LoadConfig();
}

void EffectManager::ResetAll() {
  for (auto &effect : effects_)
    effect->Reset();
}

HeadOffset EffectManager::UpdateAndAccumulate(float dt,
                                              const SPF_TruckData &truck,
                                              const SPF_Controls &controls) {
  HeadOffset total;
  for (auto &effect : effects_) {
    if (!effect->IsEnabled())
      continue;
    const HeadOffset contribution = effect->Update(dt, truck, controls);
    total.pos_x += contribution.pos_x;
    total.pos_y += contribution.pos_y;
    total.pos_z += contribution.pos_z;
    total.yaw += contribution.yaw;
    total.pitch += contribution.pitch;
    total.roll += contribution.roll;
  }
  return total;
}

void EffectManager::NotifyTruckConstants(const SPF_TruckConstants &constants) {
  for (auto &effect : effects_)
    effect->OnTruckConstantsChanged(constants);
}

void EffectManager::NotifyCommonData(const SPF_CommonData &data) {
  for (auto &effect : effects_)
    effect->OnCommonDataChanged(data);
}

void EffectManager::NotifyTrailers(const SPF_Trailer *trailers,
                                   uint32_t count) {
  for (auto &effect : effects_)
    effect->OnTrailersChanged(trailers, count);
}

} // namespace motioncab
