#pragma once

#include "effects/Effect.hpp"

#include "SPF_Config_API.h"

#include <string>

namespace motioncab {

// Reads "settings.<group>.<effect>.<setting>" by setting name alone.
class EffectConfig {
public:
  EffectConfig(SPF_Config_API *api, SPF_Config_Handle *handle,
               const char *group, const char *effect)
      : api_(api), handle_(handle),
        prefix_(std::string("settings.") + group + "." + effect + ".") {}

  bool IsAvailable() const { return api_ && handle_; }

  // Call only when IsAvailable().
  float Float(const char *setting, float fallback) const {
    return static_cast<float>(
        api_->Cfg_GetFloat(handle_, Key(setting).c_str(), fallback));
  }
  bool Bool(const char *setting, bool fallback) const {
    return api_->Cfg_GetBool(handle_, Key(setting).c_str(), fallback);
  }

private:
  std::string Key(const char *setting) const { return prefix_ + setting; }

  SPF_Config_API *api_;
  SPF_Config_Handle *handle_;
  std::string prefix_;
};

// Handles the "enabled" toggle and the config access, so an effect only
// reads its own tunables in LoadSettings().
class ConfigurableEffect : public Effect {
public:
  bool IsEnabled() const override { return enabled_; }
  void SetEnabled(bool enabled) override { enabled_ = enabled; }
  std::string_view Id() const override { return id_; }

  void LoadConfig() final {
    if (!config_.IsAvailable())
      return;
    enabled_ = config_.Bool("enabled", enabled_);
    LoadSettings();
  }

protected:
  ConfigurableEffect(SPF_Config_API *config_api,
                     SPF_Config_Handle *config_handle, const char *group,
                     const char *name)
      : config_(config_api, config_handle, group, name),
        id_(std::string("settings.") + group + "." + name) {}

  virtual void LoadSettings() = 0;

  float Float(const char *setting, float fallback) const {
    return config_.Float(setting, fallback);
  }
  bool Bool(const char *setting, bool fallback) const {
    return config_.Bool(setting, fallback);
  }

private:
  EffectConfig config_;
  std::string id_;
  bool enabled_ = true;
};

} // namespace motioncab
