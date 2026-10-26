#include "PluginContext.hpp"

#include "core/Settings.hpp"
#include "effects/ConfigurableEffect.hpp"
#include "effects/manual/SideLookEffect.hpp"

namespace motioncab {

void PluginContext::Log(SPF_LogLevel level, const char *message) const {
  if (!core || !core->logger || !logger_handle)
    return;
  core->logger->Log(logger_handle, level, message);
}

void PluginContext::ReloadEffectsConfig() {
  effects.LoadAllConfig();
  if (manual_zoom)
    manual_zoom->LoadConfig();
  if (cabin_walk)
    cabin_walk->LoadConfig();
  const EffectConfig side_look(core ? core->config : nullptr, config_handle,
                               "manual", "manual_look");
  if (side_look.IsAvailable()) {
    const float smoothing = side_look.Float(
        "smoothing_time",
        settings::Default("settings.manual.manual_look.smoothing_time"));
    camera_rig.SetRecenterSmoothing(smoothing * SideLookEffect::kIntentFraction,
                                    smoothing * SideLookEffect::kHeadFraction);
  }
}

void PluginContext::ResetEffects() {
  effects.ResetAll();
  if (manual_zoom)
    manual_zoom->Reset();
}

std::string PluginContext::PluginDataDir() const {
  if (!core || !core->environment || !environment_handle)
    return {};
  char buffer[512];
  const int len = core->environment->Env_GetPluginDataDir(
      environment_handle, buffer, sizeof(buffer));
  if (len <= 0 || len >= static_cast<int>(sizeof(buffer)))
    return {};
  return std::string(buffer, static_cast<size_t>(len));
}

PluginContext &Context() {
  static PluginContext instance;
  return instance;
}

} // namespace motioncab
