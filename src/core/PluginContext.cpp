#include "PluginContext.hpp"

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
