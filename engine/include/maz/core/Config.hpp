#pragma once

#include <cstdint>

namespace maz::core {

// Startup configuration, populated from command-line arguments.
struct AppConfig {
    const char* title = "Maz Engine — Sandbox";
    uint32_t width = 1280;
    uint32_t height = 720;
    bool headless = false;   // --headless : no visible window, run then exit (CI/tests)
    bool vsync = true;       // --no-vsync to disable
    int frames = -1;         // --frames N : quit after N frames (<0 = run until closed)
    const char* modelPath = nullptr; // --load-model PATH : load a glTF/GLB and draw it spinning
    const char* scenePath = nullptr; // --scene PATH : load a .mazscene and render its entities

    // Audio / DAW options (used by the `daw` app).
    float toneHz = 440.0f;          // --freq HZ : oscillator frequency
    double seconds = 1.0;           // --seconds N : length of an offline render
    const char* wavPath = nullptr;  // --wav PATH : write the offline render to a WAV file
};

// Parse argv into an AppConfig. Unknown flags are logged and ignored.
AppConfig parseArgs(int argc, char** argv);

} // namespace maz::core
