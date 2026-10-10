#pragma once

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

// Translations for our own UI. The manifest doesn't need this: it hands
// keys to SPF, which resolves them itself.

namespace motioncab::loc {

// Picks up a language switch. Call once per frame before any Tr(): the cache
// is only cleared here, so Tr()'s pointers stay valid for the whole frame.
void Sync();

// Counts the first language too.
unsigned LanguageChangeCount();

// Despite what SPF_Localization_API.h says, there's no per-key fallback to
// English: a missing key comes back as the key itself, so every language
// file must carry every key.
const char *Tr(std::string_view key);

// Replaces every "{name}" in the translation with its value.
std::string
Tr(std::string_view key,
   std::initializer_list<std::pair<std::string_view, std::string_view>> args);

// So a plugin reload re-reads the translation files.
void Reset();

} // namespace motioncab::loc
