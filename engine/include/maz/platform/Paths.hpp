#pragma once

#include <string>

namespace maz::platform {

// A writable, per-user, per-application directory with `file` appended (created if needed).
// Wraps SDL_GetPrefPath, e.g. ~/.local/share/<org>/<app>/<file> on Linux. Falls back to the
// bare filename (current directory) if the platform path can't be resolved.
std::string prefPath(const char* org, const char* app, const char* file);

} // namespace maz::platform
