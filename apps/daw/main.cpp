// CJC Music Station — a native DAW built on the Maz engine.
//
// Milestone A3: an FL-style step sequencer ("channel rack") + piano roll + a mixer with a master
// effect chain, on top of the A0 audio pipeline.
//   - Headless (--headless): render an offline oscillator tone, or --beat / --melody to render the
//     demo drum pattern and/or piano-roll melody (through a demo mixer: compressor + reverb).
//     Optionally writes a WAV (--wav) and logs stats. No GPU/display/audio device needed, so CI
//     verifies the synth + sequencer + effects output.
//   - Windowed: a transport (Play/Stop + BPM), a channel-rack grid, a piano-roll grid, and a mixer
//     (bus faders + master effect chain) driving a live device. Plus a test-tone panel.

#include "maz/Engine.hpp"
#include "maz/audio/Pitch.hpp"
#include "maz/audio/ProjectIO.hpp"
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

// A simple C-major arpeggio riff on the piano roll (MIDI: C4=60). Each note is two 16th-steps long.
// `fm` switches the synth to its FM engine with a bell/electric-piano voicing.
void applyDemoMelody(audio::Sequencer& seq, bool fm = false) {
    seq.roll().clear();
    if (fm) {
        seq.synth().setMode(audio::SynthMode::FM);
        seq.synth().setFmRatio(2.0f);
        seq.synth().setFmIndex(4.0f);
        seq.synth().setEnvelope(0.002f, 0.35f, 0.25f, 0.30f);
    } else {
        seq.synth().setMode(audio::SynthMode::Subtractive);
        seq.synth().setWaveform(audio::Waveform::Saw);
        seq.synth().setEnvelope(0.005f, 0.09f, 0.55f, 0.14f);
    }
    const int pitches[] = {60, 64, 67, 72, 71, 67, 64, 60}; // C E G C  B G E C
    for (int i = 0; i < 8; ++i) {
        audio::Note n;
        n.startStep = i * 2;
        n.lengthSteps = 2;
        n.pitch = pitches[i];
        n.velocity = 0.9f;
        seq.roll().addNote(n);
    }
}

// A tasteful default mixer for the demos: gentle bus compression + a touch of reverb.
void applyDemoMixer(audio::AudioEngine& engine) {
    audio::Mixer& mx = engine.mixer();
    mx.compressor().setEnabled(true);
    mx.compressor().setThresholdDb(-16.0f);
    mx.compressor().setRatio(3.0f);
    mx.compressor().setMakeupDb(3.0f);
    mx.reverb().setEnabled(true);
    mx.reverb().setRoomSize(0.6f);
    mx.reverb().setMix(0.18f);
}

// A demo automation: a slow triangle LFO sweeping the master low-pass cutoff — a classic filter
// "wobble" over the loop.
void applyDemoAuto(audio::AudioEngine& engine) {
    audio::AutoLane& lane = engine.automation().lane(audio::AutoTarget::FilterCutoff);
    lane.enabled = true;
    lane.lfo.shape = audio::Waveform::Triangle;
    lane.lfo.rateHz = 0.5f; // one sweep every two seconds
    lane.lo = 500.0f;
    lane.hi = 7000.0f;
}

// Headless: render a tone (default), a demo beat/melody, or a loaded .cjc project offline, log
// stats, optionally write a WAV (--wav) and/or save the project (--save). Returns non-zero on a
// silent sequenced render or a failed load/save (real failures).
int runHeadless(const core::AppConfig& cfg) {
    audio::AudioEngine engine;
    engine.initOffline();

    const bool loading = cfg.projectLoadPath != nullptr;
    const bool sequencing = cfg.beat || cfg.melody || loading || cfg.projectSavePath != nullptr;
    bool appliedBeat = false;
    bool appliedMelody = false;

    if (sequencing) {
        engine.sequencer().setBpm(cfg.bpm);
        if (loading) {
            std::string lerr;
            if (!audio::loadProject(cfg.projectLoadPath, engine.sequencer(), engine.mixer(),
                                    engine.automation(), &lerr)) {
                MAZ_LOG_ERROR("project load failed: %s", lerr.c_str());
                return 1;
            }
            MAZ_LOG_INFO("project: loaded %s", cfg.projectLoadPath);
        } else {
            // No explicit pattern flags (e.g. bare --save) → save the full demo (beat + melody).
            const bool anyPattern = cfg.beat || cfg.melody;
            if (cfg.beat || !anyPattern) {
                applyDemoBeat(engine.sequencer());
                appliedBeat = true;
            }
            if (cfg.melody || !anyPattern) {
                applyDemoMelody(engine.sequencer(), cfg.fm);
                appliedMelody = true;
            }
            applyDemoMixer(engine);
            if (cfg.automate) {
                applyDemoAuto(engine);
            }
            if (cfg.samplePath != nullptr) {
                std::string se;
                if (engine.sequencer().sampler().load(cfg.samplePath, &se)) {
                    engine.sequencer().sampler().setBasePitch(60);
                    engine.sequencer().sampler().setGain(0.7f);
                    engine.sequencer().setUseSampler(true);
                    MAZ_LOG_INFO("sampler: loaded %s", cfg.samplePath);
                } else {
                    MAZ_LOG_ERROR("sampler load failed: %s", se.c_str());
                    return 1;
                }
            }
        }

        if (cfg.projectSavePath != nullptr) {
            std::string serr;
            if (audio::saveProject(cfg.projectSavePath, engine.sequencer(), engine.mixer(),
                                   engine.automation(), &serr)) {
                MAZ_LOG_INFO("project: saved %s", cfg.projectSavePath);
            } else {
                MAZ_LOG_ERROR("project save failed: %s", serr.c_str());
                return 1;
            }
        }

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

    if (sequencing) {
        if (peak < 1e-4f) {
            MAZ_LOG_ERROR("sequencer rendered silence — no sound was produced");
            return 1;
        }
        const char* label = loading ? "project"
                            : (appliedBeat && appliedMelody) ? "song"
                            : appliedBeat                    ? "beat"
                                                             : "melody";
        MAZ_LOG_INFO("audio: rendered %d frames @%dHz, peak %.2f, %s @%.0f BPM", frames,
                     acfg.sampleRate, static_cast<double>(peak), label, engine.sequencer().bpm());
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

// Draw the piano-roll UI: pitch rows (high at top) × steps. Clicking a cell toggles a note.
void buildPianoRollUI(audio::Sequencer& seq) {
    audio::PianoRoll& roll = seq.roll();
    ImGui::Begin("CJC Music Station — Piano Roll");
    ImGui::TextDisabled("Click cells to place notes. The synth plays them on the shared transport.");

    const int steps = roll.numSteps();
    const int rows = roll.numPitches();
    const int low = roll.lowPitch();
    const float cell = 20.0f;

    ImGui::BeginChild("roll_grid", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    for (int r = 0; r < rows; ++r) {
        const int pitch = low + (rows - 1 - r); // top row = highest pitch
        const bool black = [&] {
            switch (pitch % 12) {
            case 1: case 3: case 6: case 8: case 10:
                return true;
            default:
                return false;
            }
        }();
        ImGui::Text("%-3s%d", audio::pitchClassName(pitch), audio::midiOctave(pitch));
        ImGui::SameLine(52.0f);
        for (int s = 0; s < steps; ++s) {
            ImGui::PushID(pitch * 1000 + s);
            const bool on = roll.hasNote(pitch, s);
            const bool onBeat = (s % 4) == 0;
            const bool playhead = seq.playing() && s == seq.currentStep();

            ImVec4 col = on ? ImVec4(0.30f, 0.60f, 0.95f, 1.0f)
                            : ImVec4(black ? 0.16f : 0.24f, black ? 0.17f : 0.25f,
                                     onBeat ? 0.34f : (black ? 0.20f : 0.30f), 1.0f);
            if (playhead && !on) {
                col.z = std::min(1.0f, col.z + 0.20f);
            }
            ImGui::PushStyleColor(ImGuiCol_Button, col);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(col.x + 0.1f, col.y + 0.1f, col.z + 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, col);
            if (ImGui::Button("##cell", ImVec2(cell, cell))) {
                roll.toggle(pitch, s);
            }
            ImGui::PopStyleColor(3);
            if (s + 1 < steps) {
                ImGui::SameLine();
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

// Draw the synth panel: engine (subtractive/FM), waveform or FM params, the ADSR envelope, and the
// sampler (load a WAV, use it for the melody).
void buildSynthUI(audio::Sequencer& seq) {
    audio::SynthInstrument& syn = seq.synth();
    ImGui::Begin("CJC Music Station — Synth");

    int mode = static_cast<int>(syn.mode());
    const char* modes[] = {"Subtractive", "FM"};
    if (ImGui::Combo("Engine", &mode, modes, 2)) {
        syn.setMode(static_cast<audio::SynthMode>(mode));
    }
    if (syn.mode() == audio::SynthMode::Subtractive) {
        int w = static_cast<int>(syn.waveform());
        const char* waves[] = {"Sine", "Square", "Saw", "Triangle"};
        if (ImGui::Combo("Waveform", &w, waves, 4)) {
            syn.setWaveform(static_cast<audio::Waveform>(w));
        }
    } else {
        float ratio = syn.fmRatio();
        if (ImGui::SliderFloat("FM Ratio", &ratio, 0.5f, 8.0f, "%.2f")) syn.setFmRatio(ratio);
        float index = syn.fmIndex();
        if (ImGui::SliderFloat("FM Index", &index, 0.0f, 10.0f, "%.2f")) syn.setFmIndex(index);
    }

    ImGui::SeparatorText("Envelope");
    float a = syn.attack();
    float d = syn.decay();
    float s = syn.sustain();
    float r = syn.release();
    bool changed = false;
    changed |= ImGui::SliderFloat("Attack", &a, 0.001f, 1.0f, "%.3f s");
    changed |= ImGui::SliderFloat("Decay", &d, 0.001f, 1.0f, "%.3f s");
    changed |= ImGui::SliderFloat("Sustain", &s, 0.0f, 1.0f, "%.2f");
    changed |= ImGui::SliderFloat("Release", &r, 0.001f, 2.0f, "%.3f s");
    if (changed) {
        syn.setEnvelope(a, d, s, r);
    }

    ImGui::SeparatorText("Sampler");
    bool useSampler = seq.useSampler();
    if (ImGui::Checkbox("Use sampler for melody", &useSampler)) {
        seq.setUseSampler(useSampler);
    }
    ImGui::Text("loaded: %s",
                seq.sampler().loaded() ? seq.sampler().path().c_str() : "(none)");
    static char pathBuf[256] = "";
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputText("wav path", pathBuf, sizeof(pathBuf));
    ImGui::SameLine();
    if (ImGui::Button("Load WAV")) {
        std::string se;
        if (seq.sampler().load(pathBuf, &se)) {
            seq.setUseSampler(true);
            MAZ_LOG_INFO("sampler: loaded %s", pathBuf);
        } else {
            MAZ_LOG_ERROR("sampler load failed: %s", se.c_str());
        }
    }
    int base = seq.sampler().basePitch();
    if (ImGui::SliderInt("Base note", &base, 24, 96)) {
        seq.sampler().setBasePitch(base);
    }

    ImGui::End();
}

// Draw the mixer: master + bus faders and the master effect chain (enable + a key knob each).
void buildMixerUI(audio::AudioEngine& engine) {
    audio::Mixer& mx = engine.mixer();
    audio::Sequencer& seq = engine.sequencer();
    ImGui::Begin("CJC Music Station — Mixer");

    // Project file I/O to a fixed path next to the app.
    static const char* kProjectPath = "project.cjc";
    if (ImGui::Button("Save Project")) {
        std::string serr;
        if (audio::saveProject(kProjectPath, seq, mx, engine.automation(), &serr)) {
            MAZ_LOG_INFO("project: saved %s", kProjectPath);
        } else {
            MAZ_LOG_ERROR("project save failed: %s", serr.c_str());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Project")) {
        std::string lerr;
        if (audio::loadProject(kProjectPath, seq, mx, engine.automation(), &lerr)) {
            MAZ_LOG_INFO("project: loaded %s", kProjectPath);
        } else {
            MAZ_LOG_ERROR("project load failed: %s", lerr.c_str());
        }
    }
    ImGui::Separator();

    float master = mx.masterGain();
    if (ImGui::SliderFloat("Master", &master, 0.0f, 1.5f, "%.2f")) {
        mx.setMasterGain(master);
    }
    float drums = seq.drumGain();
    if (ImGui::SliderFloat("Drums", &drums, 0.0f, 2.0f, "%.2f")) {
        seq.setDrumGain(drums);
    }
    float synth = seq.synthGain();
    if (ImGui::SliderFloat("Synth", &synth, 0.0f, 2.0f, "%.2f")) {
        seq.setSynthGain(synth);
    }

    ImGui::SeparatorText("Master FX");
    {
        bool en = mx.eq().enabled();
        if (ImGui::Checkbox("Low-Pass EQ", &en)) mx.eq().setEnabled(en);
        float cutoff = mx.eq().cutoff();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("Hz##eq", &cutoff, 200.0f, 18000.0f, "%.0f")) mx.eq().setCutoff(cutoff);
    }
    {
        bool en = mx.compressor().enabled();
        if (ImGui::Checkbox("Compressor", &en)) mx.compressor().setEnabled(en);
        float thr = mx.compressor().thresholdDb();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("dB##cmp", &thr, -48.0f, 0.0f, "%.0f")) mx.compressor().setThresholdDb(thr);
    }
    {
        bool en = mx.delay().enabled();
        if (ImGui::Checkbox("Delay", &en)) mx.delay().setEnabled(en);
        float wet = mx.delay().mix();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("mix##dly", &wet, 0.0f, 1.0f, "%.2f")) mx.delay().setMix(wet);
    }
    {
        bool en = mx.reverb().enabled();
        if (ImGui::Checkbox("Reverb", &en)) mx.reverb().setEnabled(en);
        float wet = mx.reverb().mix();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("mix##rev", &wet, 0.0f, 1.0f, "%.2f")) mx.reverb().setMix(wet);
    }

    ImGui::End();
}

// Draw the automation panel: one row per target with an enable, LFO shape, rate, and lo/hi range.
void buildAutomationUI(audio::Automation& automation) {
    ImGui::Begin("CJC Music Station — Automation");
    ImGui::TextDisabled("LFOs sweep a parameter over time, synced to the transport.");
    for (int i = 0; i < audio::Automation::count(); ++i) {
        audio::AutoLane& lane = automation.lane(i);
        ImGui::PushID(i);
        ImGui::Checkbox(audio::Automation::targetName(static_cast<audio::AutoTarget>(i)),
                        &lane.enabled);
        int shape = static_cast<int>(lane.lfo.shape);
        const char* shapes[] = {"Sine", "Square", "Saw", "Triangle"};
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::Combo("shape", &shape, shapes, 4)) {
            lane.lfo.shape = static_cast<audio::Waveform>(shape);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        ImGui::SliderFloat("rate", &lane.lfo.rateHz, 0.05f, 8.0f, "%.2f Hz");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);
        ImGui::DragFloatRange2("range", &lane.lo, &lane.hi, 1.0f);
        ImGui::PopID();
        ImGui::Separator();
    }
    ImGui::End();
}

// Windowed: real-time device + the channel rack, piano roll, mixer, automation, and a test tone.
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
    applyDemoMelody(engine.sequencer(), cfg.fm);
    applyDemoMixer(engine);
    if (cfg.automate) {
        applyDemoAuto(engine);
    }

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
                buildPianoRollUI(engine.sequencer());
                buildSynthUI(engine.sequencer());
                buildMixerUI(engine);
                buildAutomationUI(engine.automation());

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
