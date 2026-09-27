#include "SettingsSchema.hpp"

#include <format>

namespace motioncab::settings {

namespace {

// Shortest form that reads back as the same float, always with a decimal
// point so JSON parses it as a float, not an integer.
std::string JsonNumber(float value) {
  std::string s = std::format("{}", value);
  if (s.find_first_of(".e") == std::string::npos)
    s += ".0";
  return s;
}

} // namespace

const Setting *Find(std::string_view key) {
  for (const Setting &s : kAll) {
    if (key == s.key)
      return &s;
  }
  return nullptr;
}

std::string DefaultsJson() {
  // kAll keeps each group's and effect's settings together (see
  // SettingsAreGrouped), so a change of group/effect closes the previous one.
  std::string json = "{";
  std::string_view group, effect;
  for (const Setting &s : kAll) {
    const KeyParts parts = SplitKey(s.key);
    const bool new_group = parts.group != group;
    const bool new_effect = new_group || parts.effect != effect;
    if (new_effect && !effect.empty())
      json += "}";
    if (new_group && !group.empty())
      json += "},";
    else if (new_effect && !effect.empty())
      json += ",";
    if (new_group)
      json += std::format("\"{}\":{{", parts.group);
    if (new_effect)
      json += std::format("\"{}\":{{", parts.effect);
    else
      json += ",";
    json += std::format("\"{}\":", parts.name);
    json += s.type == Type::kBool ? (s.default_bool() ? "true" : "false")
                                  : JsonNumber(s.default_value);
    group = parts.group;
    effect = parts.effect;
  }
  json += "}}}";
  return json;
}

std::string SliderParamsJson(const Setting &setting) {
  return std::format(R"({{ "min": {}, "max": {}, "format": "{}" }})",
                     JsonNumber(setting.min), JsonNumber(setting.max),
                     setting.format);
}

void WriteDefaults(SPF_Config_API *cfg, SPF_Config_Handle *handle) {
  for (const Setting &s : kAll) {
    if (s.type == Type::kBool)
      cfg->Cfg_SetBool(handle, s.key, s.default_bool());
    else
      cfg->Cfg_SetFloat(handle, s.key, s.default_value);
  }
}

} // namespace motioncab::settings
