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
    bool beat = false;              // --beat : render the demo step-sequencer pattern (not a tone)
    bool melody = false;            // --melody : render the demo piano-roll melody
    bool fm = false;                // --fm : use the FM synth engine for the demo melody
    bool wavetable = false;         // --wt : use the wavetable synth engine for the demo melody
    bool automate = false;          // --auto : enable a demo auto-filter sweep
    const char* samplePath = nullptr; // --sample PATH : load a WAV into the sampler for the melody
    bool song = false;              // --song : build a multi-pattern arrangement (playlist) demo
    double bpm = 120.0;             // --bpm N : sequencer tempo
    double swing = 0.0;             // --swing N : swing/groove amount (0..0.75)
    const char* projectSavePath = nullptr; // --save PATH : write the project to a .cjc file
    const char* projectLoadPath = nullptr; // --load PATH : load a .cjc project and render it
    const char* midiPath = nullptr;        // --midi PATH : export the pattern as a .mid file
    const char* midiInPath = nullptr;      // --importmidi PATH : import a .mid into the pattern
    const char* stemsPrefix = nullptr;     // --stems PREFIX : bounce drums/lead/bass to separate WAVs
    const char* pluginPath = nullptr;      // --plugin PATH : load a native audio plugin (.so)
    const char* clapPath = nullptr;        // --clap PATH : load a CLAP-format plugin (.clap)
    const char* vst3Path = nullptr;        // --vst3 PATH : load a VST3-format plugin (.vst3)
};

// Parse argv into an AppConfig. Unknown flags are logged and ignored.
AppConfig parseArgs(int argc, char** argv);

} // namespace maz::core
