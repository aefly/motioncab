#pragma once

namespace motioncab {

// Opens `url` in the system's default browser. SPF's UI API only opens URLs
// from text/Markdown links, so real buttons (e.g. Donate) go through here.
void OpenUrl(const char *url);

} // namespace motioncab
