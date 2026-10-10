#pragma once

#include "effects/Effect.hpp"

#include <memory>
#include <vector>

namespace motioncab {

// Toggling an effect fades it in or out instead of cutting it, or switching
// one off mid-motion (Steering Camera in a turn) would snap the camera. A
// disabled effect keeps updating until it has faded out, then is Reset() so
// it comes back from rest. Leaving the driver's seat and being paused by
// another effect (conflicts::kPauses) fade it out the same way.
class EffectManager {
public:
  void Register(std::unique_ptr<Effect> effect);

  void LoadAllConfig();
  // Settles every fade at once too, since the camera is being resynced
  // anyway.
  void ResetAll();

  void SetAtWheel(bool at_wheel) { at_wheel_ = at_wheel; }

  HeadOffset UpdateAndAccumulate(float dt, const SPF_TruckData &truck,
                                 const SPF_Controls &controls);

  void NotifyTruckConstants(const SPF_TruckConstants &constants);
  void NotifyCommonData(const SPF_CommonData &data);
  void NotifyTrailers(const SPF_Trailer *trailers, uint32_t count);

private:
  static constexpr float kFadeSeconds = 0.5f;

  struct Slot {
    std::unique_ptr<Effect> effect;
    float fade = 1.0f;       // the toggle's
    bool seat_reset = false; // Reset() since the driver's seat was left
    std::vector<size_t> paused_by;
    float pause_fade = 1.0f;
    bool pause_reset = false;
  };

  bool IsPaused(const Slot &slot) const;

  std::vector<Slot> effects_;
  bool at_wheel_ = true;
  float seat_fade_ = 1.0f; // shared by the NeedsDriverSeat effects
};

} // namespace motioncab
