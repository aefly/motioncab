#include "OpenUrl.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <shellapi.h>

namespace motioncab {

void OpenUrl(const char *url) {
  if (!url || !url[0])
    return;
  ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
}

} // namespace motioncab
