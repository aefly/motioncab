#pragma once

#include "SPF_TelemetryData.h"

namespace motioncab {

// Additive offset an effect wants applied to the interior head/seat pose.
// Multiple effects' offsets are summed by EffectManager before being applied
// on top of the user's own SPF seat/head-rotation settings.
struct HeadOffset {
  float pos_x = 0.0f, pos_y = 0.0f, pos_z = 0.0f; // meters, cabin-local
  // Degrees, not radians. Plugin.cpp converts at the Cam_SetInteriorHeadRot
  // call site, to stay consistent with Cam_SetInteriorRotationDefaults
  // (degrees; a default pitch of -7.0 only makes sense as such).
  float yaw = 0.0f, pitch = 0.0f;
  // Degrees, head tilt to either side (Cam_SetInteriorRoll takes degrees).
  float roll = 0.0f;
};

// Base interface for a MotionCab head-motion effect.
// Every effect is independently toggleable and reads its own tunables from
// the plugin's "settings.<group>.<effect>.*" config section.
class Effect {
public:
  virtual ~Effect() = default;

  virtual const char *Name() const = 0;
  virtual bool IsEnabled() const = 0;
  virtual void SetEnabled(bool enabled) = 0;

  // (Re)reads tunables from config. Called on init and on OnSettingChanged.
  virtual void LoadConfig() = 0;

  // Clears internal smoothing state (e.g. on entering cabin view).
  virtual void Reset() = 0;

  // Advances the effect by `dt` seconds and returns its current contribution.
  virtual HeadOffset Update(float dt, const SPF_TruckData &truck,
                            const SPF_Controls &controls) = 0;

  // Called whenever the truck's static configuration changes (bought a new
  // truck, added/removed axles). Default no-op; only effects that need
  // per-wheel layout (e.g. front/rear classification) override this.
  virtual void
  OnTruckConstantsChanged(const SPF_TruckConstants & /*constants*/) {}

  // Called whenever common telemetry data changes (includes the substance
  // name table). Default no-op; only effects that need surface-material
  // identification override this.
  virtual void OnCommonDataChanged(const SPF_CommonData & /*data*/) {}

  // Called every frame with the current list of attached trailers (fired
  // by Tel_RegisterForTrailers). Default no-op; only effects that react to
  // trailer state (e.g. hitch/unhitch) override this.
  virtual void OnTrailersChanged(const SPF_Trailer * /*trailers*/,
                                 uint32_t /*count*/) {}
};

} // namespace motioncab
