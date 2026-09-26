#include "Localization.hpp"

#include "PluginContext.hpp"

#include <unordered_map>

namespace motioncab::loc {

namespace {

// Node-based, so a cached value's c_str() survives later insertions.
std::unordered_map<std::string, std::string> g_cache;
std::string g_language;

std::string Lookup(const std::string &key) {
  PluginContext &ctx = Context();
  SPF_Localization_API *api = ctx.core ? ctx.core->localization : nullptr;
  if (!api || !ctx.localization_handle)
    return key;

  char buffer[512];
  const int len = api->Loc_GetString(ctx.localization_handle, key.c_str(),
                                     buffer, sizeof(buffer));
  if (len <= 0)
    return key;
  if (len < static_cast<int>(sizeof(buffer)))
    return std::string(buffer, static_cast<size_t>(len));

  // Truncated: the return value is then the size needed, NUL included.
  std::string value(static_cast<size_t>(len), '\0');
  const int written = api->Loc_GetString(ctx.localization_handle, key.c_str(),
                                         value.data(), len);
  value.resize(written > 0 && written < len ? static_cast<size_t>(written) : 0);
  return value;
}

} // namespace

void Sync() {
  PluginContext &ctx = Context();
  if (!ctx.core || !ctx.core->config || !ctx.config_handle)
    return;

  char lang[32];
  const int len = ctx.core->config->Cfg_GetString(
      ctx.config_handle, "localization.language", "", lang, sizeof(lang));
  if (len <= 0 || len >= static_cast<int>(sizeof(lang)) || g_language == lang)
    return;

  // SPF switches the loaded file itself, synchronously, when this setting
  // changes (Language tab, or the framework's "sync plugin languages"),
  // so only the cache needs dropping.
  g_language = lang;
  g_cache.clear();
}

const char *Tr(std::string_view key) {
  std::string k(key);
  auto it = g_cache.find(k);
  if (it == g_cache.end()) {
    std::string value = Lookup(k);
    it = g_cache.emplace(std::move(k), std::move(value)).first;
  }
  return it->second.c_str();
}

std::string Tr(
    std::string_view key,
    std::initializer_list<std::pair<std::string_view, std::string_view>> args) {
  std::string text = Tr(key);
  for (const auto &[name, value] : args) {
    const std::string token = "{" + std::string(name) + "}";
    for (size_t pos = text.find(token); pos != std::string::npos;
         pos = text.find(token, pos + value.size()))
      text.replace(pos, token.size(), value);
  }
  return text;
}

void Reset() {
  g_cache.clear();
  g_language.clear();
}

} // namespace motioncab::loc
