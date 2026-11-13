#pragma once

#include "effects/Effect.hpp"

#include <memory>
#include <vector>

namespace motioncab {

// Toggling an effect fades it in or out instead of cutting it, or switching
// one off mid-motion (Steering Camera in a turn) would snap the camera. A
// disabled effect keeps updating until it has faded out, then is Reset() so
// it comes back from rest. Leaving the driver's seat and being paused by
// another effect (conflicts::kPauses) fade it out the same way. Zooming in
// fades out all but the looks the player asked for.
class EffectManager {
public:
  void Register(std::unique_ptr<Effect> effect);

  void LoadAllConfig();
  // Settles every fade at once too, since the camera is being resynced
  // anyway.
  void ResetAll();
  // The game's recenter key only puts the rotation back: the rest glides out.
  void Recenter();

  void SetAtWheel(bool at_wheel) { at_wheel_ = at_wheel; }
  void SetZoomed(bool zoomed) { zoomed_ = zoomed; }

  HeadOffset UpdateAndAccumulate(float dt, const SPF_TruckData &truck,
                                 const SPF_Controls &controls);

  void NotifyTruckConstants(const SPF_TruckConstants &constants);
  void NotifyCommonData(const SPF_CommonData &data);
  void NotifyTrailers(const SPF_Trailer *trailers, uint32_t count);

private:
  static constexpr float kFadeSeconds = 0.5f;
  static constexpr float kRecenterGlideSeconds = 0.4f;
  // About as long as the zoom's FOV takes to narrow, so the motion is gone
  // by the time it would be magnified.
  static constexpr float kZoomFadeSeconds = 0.3f;

  struct Slot {
    std::unique_ptr<Effect> effect;
    float fade = 1.0f;       // the toggle's
    bool seat_reset = false; // Reset() since the driver's seat was left
    struct PausedBy {
      size_t index;
      bool while_in_use;
    };
    std::vector<PausedBy> paused_by;
    float pause_fade = 1.0f;
    bool pause_reset = false;
  };

  struct Pause {
    // By an effect enabled: faded out, then Reset().
    bool full = false;
    // By effects in use (Effect::InUse), the most: weighs the effect down
    // directly, without a Reset().
    float in_use = 0.0f;
  };
  Pause PauseOf(const Slot &slot) const;

  std::vector<Slot> effects_;
  bool at_wheel_ = true;
  float seat_fade_ = 1.0f; // shared by the NeedsDriverSeat effects
  bool zoomed_ = false;
  float zoom_fade_ = 1.0f; // shared by all but the IsPlayerTriggered ones
  HeadOffset last_total_{};
  HeadOffset recenter_residual_{};
  float recenter_glide_ = 0.0f;
};

} // namespace motioncab
