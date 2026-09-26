#pragma once

#include "Effect.hpp"

#include <memory>
#include <vector>

namespace motioncab {

// Owns and drives all MotionCab head-motion effects. Add new effects here
// as they're implemented; each stays independently toggleable/configurable.
//
// Toggling an effect fades its contribution in or out over kFadeSeconds
// instead of cutting it: an effect switched off mid-motion (e.g. Steering
// Camera in a turn) would otherwise snap the camera. A disabled effect
// keeps being updated until it has faded out, then is Reset() so it starts
// from rest once re-enabled.
class EffectManager {
public:
  void Register(std::unique_ptr<Effect> effect);

  void LoadAllConfig();
  // Also settles every fade instantly (in for enabled effects, out for
  // disabled ones), since the camera is being resynced anyway.
  void ResetAll();

  // Advances every enabled (or still fading out) effect and returns the
  // summed, fade-weighted offset to apply.
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
  static constexpr float kFadeSeconds = 0.5f;

  struct Slot {
    std::unique_ptr<Effect> effect;
    float fade = 1.0f; // 0 = contributes nothing, 1 = full contribution
  };
  std::vector<Slot> effects_;
};

} // namespace motioncab
