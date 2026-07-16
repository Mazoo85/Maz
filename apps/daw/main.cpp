// CJC Music Station — a native DAW built on the Maz engine.
//
// Milestone A1: an FL-style step sequencer ("channel rack") on top of the A0 audio pipeline.
//   - Headless (--headless): render an offline oscillator tone, or --beat to render the demo drum
//     pattern. Optionally writes a WAV (--wav) and logs stats. No GPU/display/audio device needed,
//     so CI verifies the synth + sequencer output.
//   - Windowed: a transport (Play/Stop + BPM) and a clickable channel-rack grid driving a live
//     device — program a beat and hear it loop. A collapsible test-tone panel keeps the A0 synth.

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
#include <cstdio>
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

float peakOf(const std::vector<float>& buf) {
    float peak = 0.0f;
    for (float s : buf) {
        peak = std::max(peak, std::fabs(s));
    }
    return peak;
}

// A classic one-bar demo groove programmed onto the default kit (channels: 0 Kick, 1 Snare,
// 2 Closed Hat, 3 Open Hat, 4 Clap). Used by --beat and as the window's starting pattern.
void applyDemoBeat(audio::Sequencer& seq) {
    seq.clear();
    const int kick[] = {0, 4, 8, 10, 14};
    const int snare[] = {4, 12};
    const int chat[] = {0, 2, 4, 6, 8, 10, 12, 14};
    const int ohat[] = {2, 10};
    const int clap[] = {4, 12};
    for (int s : kick) seq.setStep(0, s, true);
    for (int s : snare) seq.setStep(1, s, true);
    for (int s : chat) seq.setStep(2, s, true);
    for (int s : ohat) seq.setStep(3, s, true);
    for (int s : clap) seq.setStep(4, s, true);
}

// Headless: render either a tone (default) or the demo beat (--beat) offline, log stats, and
// optionally write a WAV. Returns non-zero if a beat render came out silent (a real failure).
int runHeadless(const core::AppConfig& cfg) {
    audio::AudioEngine engine;
    engine.initOffline();

    if (cfg.beat) {
        engine.sequencer().setBpm(cfg.bpm);
        applyDemoBeat(engine.sequencer());
        engine.sequencer().play();
    } else {
        engine.voice().setWaveform(audio::Waveform::Sine);
        engine.noteOn(cfg.toneHz);
    }

    const std::vector<float> buf = engine.renderOffline(cfg.seconds);
    const audio::AudioConfig& acfg = engine.config();
    const int channels = acfg.channels;
    const int frames = channels > 0 ? static_cast<int>(buf.size()) / channels : 0;
    const float peak = peakOf(buf);

    if (cfg.wavPath != nullptr) {
        std::string werr;
        if (audio::writeWav16(cfg.wavPath, buf.data(), frames, channels, acfg.sampleRate, &werr)) {
            MAZ_LOG_INFO("audio: wrote %s", cfg.wavPath);
        } else {
            MAZ_LOG_ERROR("audio: WAV write failed: %s", werr.c_str());
        }
    }

    if (cfg.beat) {
        if (peak < 1e-4f) {
            MAZ_LOG_ERROR("beat: rendered silence — the sequencer produced no sound");
            return 1;
        }
        MAZ_LOG_INFO("audio: rendered %d frames @%dHz, peak %.2f, beat @%.0f BPM", frames,
                     acfg.sampleRate, static_cast<double>(peak), cfg.bpm);
    } else {
        const double estHz = estimateHz(buf, channels, acfg.sampleRate);
        MAZ_LOG_INFO("audio: rendered %d frames @%dHz, peak %.2f, est %.0f Hz", frames,
                     acfg.sampleRate, static_cast<double>(peak), estHz);
    }
    return 0;
}

// Draw the channel-rack (step sequencer) UI. Mutates the live sequencer in response to clicks.
void buildRackUI(audio::Sequencer& seq) {
    ImGui::Begin("CJC Music Station — Channel Rack");

    // Transport row.
    if (ImGui::Button(seq.playing() ? "  Stop  " : "  Play  ")) {
        if (seq.playing()) {
            seq.stop();
        } else {
            seq.play();
        }
    }
    ImGui::SameLine();
    float bpm = static_cast<float>(seq.bpm());
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::SliderFloat("BPM", &bpm, 40.0f, 240.0f, "%.0f")) {
        seq.setBpm(static_cast<double>(bpm));
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        seq.clear();
    }

    // Step grid: one row per channel, one small toggle button per step. The playhead column is
    // tinted so you can see where the transport is.
    const int steps = seq.numSteps();
    const int channels = seq.numChannels();
    const float cell = 26.0f;
    for (int c = 0; c < channels; ++c) {
        ImGui::Text("%-11s", seq.channelName(c).c_str());
        ImGui::SameLine(120.0f);
        for (int s = 0; s < steps; ++s) {
            ImGui::PushID(c * 1000 + s);
            const bool on = seq.step(c, s);
            const bool onBeat = (s % 4) == 0;
            const bool playhead = seq.playing() && s == seq.currentStep();

            ImVec4 col = on ? ImVec4(0.20f, 0.80f, 0.45f, 1.0f)
                            : ImVec4(onBeat ? 0.32f : 0.22f, 0.23f, 0.28f, 1.0f);
            if (playhead) {
                col.x = std::min(1.0f, col.x + 0.25f);
                col.y = std::min(1.0f, col.y + 0.25f);
                col.z = std::min(1.0f, col.z + 0.25f);
            }
            ImGui::PushStyleColor(ImGuiCol_Button, col);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(col.x + 0.1f, col.y + 0.1f, col.z + 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, col);
            if (ImGui::Button("##step", ImVec2(cell, cell))) {
                seq.toggle(c, s);
            }
            ImGui::PopStyleColor(3);
            if (s + 1 < steps) {
                ImGui::SameLine();
            }
            ImGui::PopID();
        }
    }

    ImGui::End();
}

// Windowed: real-time device + the channel rack + a collapsible test-tone panel.
int runWindowed(const core::AppConfig& cfg) {
    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "CJC Music Station";
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
    engine.sequencer().setBpm(cfg.bpm);
    applyDemoBeat(engine.sequencer());

    bool toneOn = false;
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
                buildRackUI(engine.sequencer());

                ImGui::Begin("Test Tone");
                if (ImGui::Button(toneOn ? "  Stop  " : "  Play  ")) {
                    toneOn = !toneOn;
                    if (toneOn) {
                        engine.noteOn(freq);
                    } else {
                        engine.noteOff();
                    }
                }
                ImGui::SameLine();
                ImGui::TextDisabled(toneOn ? "playing" : "stopped");
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
    MAZ_LOG_INFO("CJC Music Station shutting down after %d frames", rendered);
    renderer->shutdown();
    window.shutdown();
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("CJC Music Station starting (headless=%d, mode=%s)", cfg.headless,
                 cfg.beat ? "beat" : "tone");
    return cfg.headless ? runHeadless(cfg) : runWindowed(cfg);
}
