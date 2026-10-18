#pragma once

#include <cctype>
#include <string>
#include <string_view>

namespace motioncab::strings {

inline std::string ToLower(std::string_view s) {
  std::string out(s);
  for (char &c : out)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

inline bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i])))
      return false;
  }
  return true;
}

namespace detail {

inline char32_t LowerCodePoint(char32_t c) {
  if (c < 0x80)
    return (c >= 'A' && c <= 'Z') ? c + 0x20 : c;
  // Latin-1: À..Þ, but × isn't a letter.
  if (c >= 0xC0 && c <= 0xDE && c != 0xD7)
    return c + 0x20;
  // Latin Extended-A (Czech, Polish, Turkish...) pairs each capital with the
  // next code point, starting on an even one in some runs and an odd one in
  // others.
  if (c == 0x130) // Turkish dotted İ
    return 'i';
  if (c == 0x178) // Ÿ
    return 0xFF;
  if ((c >= 0x100 && c <= 0x137) || (c >= 0x14A && c <= 0x177))
    return c | 1;
  if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E))
    return (c & 1) ? c + 1 : c;
  // Cyrillic: Ѐ..Џ, А..Я, and Ukrainian Ґ.
  if (c >= 0x400 && c <= 0x40F)
    return c + 0x50;
  if (c >= 0x410 && c <= 0x42F)
    return c + 0x20;
  if (c == 0x490)
    return 0x491;
  return c;
}

inline void AppendUtf8(std::string &out, char32_t c) {
  if (c < 0x80) {
    out += static_cast<char>(c);
  } else if (c < 0x800) {
    out += static_cast<char>(0xC0 | (c >> 6));
    out += static_cast<char>(0x80 | (c & 0x3F));
  } else if (c < 0x10000) {
    out += static_cast<char>(0xE0 | (c >> 12));
    out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (c & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (c >> 18));
    out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (c & 0x3F));
  }
}

} // namespace detail

// Lowercases UTF-8 for a case-insensitive search in any of our languages:
// ToLower only knows ASCII. Malformed bytes are copied as they are.
inline std::string FoldCase(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  size_t i = 0;
  while (i < s.size()) {
    const auto lead = static_cast<unsigned char>(s[i]);
    const size_t len = lead < 0x80           ? 1
                       : (lead >> 5) == 0x6  ? 2
                       : (lead >> 4) == 0xE  ? 3
                       : (lead >> 3) == 0x1E ? 4
                                             : 0;
    if (len == 0 || i + len > s.size()) {
      out += s[i++];
      continue;
    }
    char32_t c = len == 1 ? lead : lead & (0x7F >> len);
    bool valid = true;
    for (size_t k = 1; k < len; ++k) {
      const auto cont = static_cast<unsigned char>(s[i + k]);
      if ((cont & 0xC0) != 0x80) {
        valid = false;
        break;
      }
      c = (c << 6) | (cont & 0x3F);
    }
    if (!valid) {
      out += s[i++];
      continue;
    }
    detail::AppendUtf8(out, detail::LowerCodePoint(c));
    i += len;
  }
  return out;
}

} // namespace motioncab::strings
