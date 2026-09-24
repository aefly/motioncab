#include "PluginContext.hpp"

namespace motioncab {

void PluginContext::Log(SPF_LogLevel level, const char *message) const {
  if (!core || !core->logger || !logger_handle)
    return;
  core->logger->Log(logger_handle, level, message);
}

PluginContext &Context() {
  static PluginContext instance;
  return instance;
}

} // namespace motioncab
