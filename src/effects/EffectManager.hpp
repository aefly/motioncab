#pragma once

#include "Effect.hpp"

#include <memory>
#include <vector>

namespace motioncab {

// Owns and drives all MotionCab head-motion effects. Add new effects here
// as they're implemented; each stays independently toggleable/configurable.
class EffectManager {
public:
  void Register(std::unique_ptr<Effect> effect);

  void LoadAllConfig();
  void ResetAll();

  // Advances every enabled effect and returns the summed offset to apply.
  HeadOffset UpdateAndAccumulate(float dt, const SPF_TruckData &truck,
                                 const SPF_Controls &controls);

  // Broadcasts a truck-configuration change to every registered effect.
  void NotifyTruckConstants(const SPF_TruckConstants &constants);

  // Broadcasts a common-data change (includes the substance name table) to
  // every registered effect.
  void NotifyCommonData(const SPF_CommonData &data);

  // Broadcasts the current trailer list (fired every frame) to every
  // registered effect.
  void NotifyTrailers(const SPF_Trailer *trailers, uint32_t count);

private:
  std::vector<std::unique_ptr<Effect>> effects_;
};

} // namespace motioncab
