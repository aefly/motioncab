#pragma once

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

// Runtime access to the translations in localization/<lang>.json, through
// SPF's Localization API. The manifest passes the same keys straight to
// SPF, so the native settings UI and the Quick Settings window share one
// set of strings.

namespace motioncab::loc {

// Picks up a language switch made in SPF's Language tab. Call once per
// frame before any Tr(): the cache is only ever cleared here, which is
// what keeps the pointers Tr() hands out valid for the rest of the frame.
void Sync();

// Translation of `key` in the active language. Despite what
// SPF_Localization_API.h says, SPF only loads that one language file, with
// no per-key fallback to English: a key missing from it comes back as the
// key itself, so every language file must carry every key.
const char *Tr(std::string_view key);

// Tr() with every "{name}" in the translation replaced by its value.
std::string Tr(
    std::string_view key,
    std::initializer_list<std::pair<std::string_view, std::string_view>> args);

// Drops the cache, so a plugin reload re-reads the translation files.
void Reset();

} // namespace motioncab::loc
