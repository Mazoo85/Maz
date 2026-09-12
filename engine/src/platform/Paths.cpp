#include "maz/platform/Paths.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

namespace maz::platform {

std::string prefPath(const char* org, const char* app, const char* file) {
    char* base = SDL_GetPrefPath(org, app);
    if (!base) {
        return std::string(file ? file : "");
    }
    std::string path(base);
    SDL_free(base);
    if (file) {
        path += file;
    }
    return path;
}

} // namespace maz::platform
