#include "maz/core/Config.hpp"

#include "maz/core/Log.hpp"

#include <cstdlib>
#include <cstring>

namespace maz::core {

AppConfig parseArgs(int argc, char** argv) {
    AppConfig cfg;
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (std::strcmp(arg, "--headless") == 0) {
            cfg.headless = true;
        } else if (std::strcmp(arg, "--no-vsync") == 0) {
            cfg.vsync = false;
        } else if (std::strcmp(arg, "--vsync") == 0) {
            cfg.vsync = true;
        } else if (std::strcmp(arg, "--frames") == 0 && i + 1 < argc) {
            cfg.frames = std::atoi(argv[++i]);
        } else if (std::strcmp(arg, "--width") == 0 && i + 1 < argc) {
            cfg.width = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(arg, "--height") == 0 && i + 1 < argc) {
            cfg.height = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(arg, "--load-model") == 0 && i + 1 < argc) {
            cfg.modelPath = argv[++i];
        } else if (std::strcmp(arg, "--scene") == 0 && i + 1 < argc) {
            cfg.scenePath = argv[++i];
        } else if (std::strcmp(arg, "--freq") == 0 && i + 1 < argc) {
            cfg.toneHz = static_cast<float>(std::atof(argv[++i]));
        } else if (std::strcmp(arg, "--seconds") == 0 && i + 1 < argc) {
            cfg.seconds = std::atof(argv[++i]);
        } else if (std::strcmp(arg, "--wav") == 0 && i + 1 < argc) {
            cfg.wavPath = argv[++i];
        } else {
            MAZ_LOG_WARN("ignoring unknown argument: %s", arg);
        }
    }
    // A headless run with no frame cap would never exit; give it a sane default.
    if (cfg.headless && cfg.frames < 0) {
        cfg.frames = 10;
    }
    return cfg;
}

} // namespace maz::core
