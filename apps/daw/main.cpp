// Maz DAW — "hello sound" (milestone A0).
//
// The first, genuinely-working slice of the DAW: a native audio pipeline that makes sound.
//   - Headless (--headless): render an offline tone, optionally write it to a WAV (--wav), and log
//     stats. No GPU/display/audio device needed, so CI can verify the synth output.
//   - Windowed: a single panel with a ▶ Play / ■ Stop button, a frequency slider, and a waveform
//     picker driving a live real-time oscillator — click Play and you hear a tone.
//
// The audio graph (AudioEngine's voice mixer + sample clock) is the seam a future FL-style step
// sequencer plugs into.

#include "maz/Engine.hpp"
#include "maz/audio/WavWriter.hpp"

// See note in apps/editor/main.cpp: quiet third-party ImGui header warnings under -Werror.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <SDL3/SDL_events.h>

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

using namespace maz;

namespace {

// Estimate a signal's fundamental frequency by counting rising zero-crossings on the left channel.
// Good enough to confirm "we generated ~440 Hz" without an FFT.
double estimateHz(const std::vector<float>& interleaved, int channels, int sampleRate) {
    const int frames = channels > 0 ? static_cast<int>(interleaved.size()) / channels : 0;
    if (frames <= 1) {
        return 0.0;
    }
    int crossings = 0;
    float prev = interleaved[0];
    for (int i = 1; i < frames; ++i) {
        const float s = interleaved[static_cast<size_t>(i) * static_cast<size_t>(channels)];
        if (prev <= 0.0f && s > 0.0f) {
            ++crossings;
        }
        prev = s;
    }
    return static_cast<double>(crossings) * static_cast<double>(sampleRate) /
           static_cast<double>(frames);
}

// Offline path: synthesize a tone, optionally write it to a WAV, and log stats CI can assert on.
int runHeadless(const core::AppConfig& cfg) {
    audio::AudioEngine engine;
    engine.initOffline();
    engine.voice().setWaveform(audio::Waveform::Sine);
    engine.noteOn(cfg.toneHz);

    const std::vector<float> buf = engine.renderOffline(cfg.seconds);
    const audio::AudioConfig& acfg = engine.config();
    const int channels = acfg.channels;
    const int frames = channels > 0 ? static_cast<int>(buf.size()) / channels : 0;

    float peak = 0.0f;
    for (int i = 0; i < frames; ++i) {
        peak = std::max(peak, std::fabs(buf[static_cast<size_t>(i) * static_cast<size_t>(channels)]));
    }
    const double estHz = estimateHz(buf, channels, acfg.sampleRate);

    if (cfg.wavPath != nullptr) {
        std::string werr;
        if (audio::writeWav16(cfg.wavPath, buf.data(), frames, channels, acfg.sampleRate, &werr)) {
            MAZ_LOG_INFO("audio: wrote %s", cfg.wavPath);
        } else {
            MAZ_LOG_ERROR("audio: WAV write failed: %s", werr.c_str());
        }
    }

    MAZ_LOG_INFO("audio: rendered %d frames @%dHz, peak %.2f, est %.0f Hz", frames, acfg.sampleRate,
                 static_cast<double>(peak), estHz);
    return 0;
}

// Windowed path: real-time device + a minimal transport UI.
int runWindowed(const core::AppConfig& cfg) {
    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz DAW";
    wc.width = cfg.width;
    wc.height = cfg.height;
    wc.headless = false;
    if (!window.init(wc)) {
        MAZ_LOG_ERROR("window init failed");
        return 1;
    }

    render::RendererConfig rc;
    rc.vsync = cfg.vsync;
    rc.allowHeadless = false;
#if defined(MAZ_DEBUG)
    rc.enableValidation = true;
#endif
    auto renderer = render::createVulkanRenderer();
    if (!renderer->init(window, rc)) {
        MAZ_LOG_ERROR("renderer init failed");
        return 1;
    }
    const bool gui = renderer->initGui(window);
    if (!gui) {
        MAZ_LOG_WARN("DAW UI unavailable (no GPU?); running without controls");
    }

    audio::AudioEngine engine;
    if (!engine.initRealtime()) {
        MAZ_LOG_WARN("audio: realtime init failed; the UI will be silent");
    }

    bool playing = false;
    float freq = cfg.toneHz;
    int waveIndex = 0; // Sine

    bool running = true;
    int rendered = 0;
    while (running && !window.shouldClose()) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (gui) {
                ImGui_ImplSDL3_ProcessEvent(&ev);
            }
            if (ev.type == SDL_EVENT_QUIT || ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                running = false;
            } else if (ev.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
                       ev.type == SDL_EVENT_WINDOW_RESIZED) {
                uint32_t w = 0, h = 0;
                window.drawableSize(w, h);
                renderer->onResize(w, h);
            }
        }

        renderer->setClearColor({0.08f, 0.09f, 0.12f, 1.0f});
        if (renderer->beginFrame()) {
            if (gui) {
                renderer->guiNewFrame();
                ImGui::Begin("Maz DAW — Transport");
                if (ImGui::Button(playing ? "  Stop  " : "  Play  ")) {
                    playing = !playing;
                    if (playing) {
                        engine.noteOn(freq);
                    } else {
                        engine.noteOff();
                    }
                }
                ImGui::SameLine();
                ImGui::TextDisabled(playing ? "playing" : "stopped");

                if (ImGui::SliderFloat("Frequency", &freq, 40.0f, 2000.0f, "%.0f Hz")) {
                    engine.voice().setFrequency(freq);
                }
                const char* waves[] = {"Sine", "Square", "Saw", "Triangle"};
                if (ImGui::Combo("Waveform", &waveIndex, waves, 4)) {
                    engine.voice().setWaveform(static_cast<audio::Waveform>(waveIndex));
                }
                ImGui::End();
            }
            renderer->endFrame();
        }

        ++rendered;
        if (cfg.frames >= 0 && rendered >= cfg.frames) {
            MAZ_LOG_INFO("reached frame cap (%d); exiting", cfg.frames);
            running = false;
        }
    }

    engine.shutdown();
    MAZ_LOG_INFO("Maz DAW shutting down after %d frames", rendered);
    renderer->shutdown();
    window.shutdown();
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("Maz DAW starting (headless=%d, freq=%.0f Hz)", cfg.headless,
                 static_cast<double>(cfg.toneHz));
    return cfg.headless ? runHeadless(cfg) : runWindowed(cfg);
}
