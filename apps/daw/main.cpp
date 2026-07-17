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
#include "maz/audio/MidiReader.hpp"
#include "maz/audio/MidiWriter.hpp"
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
// `fm` switches the synth to its FM engine with a bell/electric-piano voicing; `wt` switches it to
// the wavetable engine with an envelope-swept scan for an evolving pluck.
void applyDemoMelody(audio::Sequencer& seq, bool fm = false, bool wt = false) {
    seq.roll().clear();
    if (wt) {
        seq.synth().setMode(audio::SynthMode::Wavetable);
        seq.synth().setWavetablePosition(0.15f);
        seq.synth().setWavetableMorph(0.7f); // envelope sweeps the table for movement
        seq.synth().setEnvelope(0.004f, 0.20f, 0.45f, 0.20f);
        seq.synth().setFilter(6000.0f, 2.0f, 4000.0f);
    } else if (fm) {
        seq.synth().setMode(audio::SynthMode::FM);
        seq.synth().setFmRatio(2.0f);
        seq.synth().setFmIndex(4.0f);
        seq.synth().setEnvelope(0.002f, 0.35f, 0.25f, 0.30f);
    } else {
        seq.synth().setMode(audio::SynthMode::Subtractive);
        seq.synth().setWaveform(audio::Waveform::Saw);
        seq.synth().setEnvelope(0.005f, 0.09f, 0.55f, 0.14f);
        seq.synth().setFilter(1200.0f, 5.0f, 3500.0f);       // resonant sweep for a classic pluck
        seq.synth().setOscillators(18.0f, 0.7f, 0.4f, 0.0f); // detuned + sub for a fat lead
    }

    // Bass on the second instrument (roll2 / synth2).
    seq.synth2().setMode(audio::SynthMode::Subtractive);
    seq.synth2().setWaveform(audio::Waveform::Saw);
    seq.synth2().setEnvelope(0.005f, 0.15f, 0.7f, 0.10f);
    seq.synth2().setFilter(600.0f, 3.0f, 700.0f);
    seq.synth2().setOscillators(0.0f, 0.0f, 0.5f, 0.0f); // sub for weight
    seq.synth2().setGain(0.32f);
    seq.roll2().clear();
    const int bass[] = {36, 36, 43, 41}; // C2 C2 G2 F2 roots
    for (int i = 0; i < 4; ++i) {
        audio::Note n;
        n.startStep = i * 4;
        n.lengthSteps = 4;
        n.pitch = bass[i];
        n.velocity = 0.9f;
        seq.roll2().addNote(n);
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

// A multi-pattern demo arrangement: pattern 0 is the main groove, pattern 1 is a busier fill, and
// the playlist chains them into a short song (three bars of groove, one of fill, looped).
void applyDemoSong(audio::Sequencer& seq, bool fm, bool wt = false) {
    seq.clearArrangement();
    seq.selectPattern(0);
    applyDemoBeat(seq);
    applyDemoMelody(seq, fm, wt);

    const int fill = seq.addPattern();
    seq.selectPattern(fill);
    for (int s = 0; s < seq.numSteps(); ++s) {
        seq.setStep(1, s, true); // snare roll across the bar
    }
    for (int s : {0, 4, 8, 12}) {
        seq.setStep(0, s, true); // kick on the beat
    }
    applyDemoMelody(seq, fm, wt);

    seq.selectPattern(0);
    seq.setPlaylist({0, 0, 0, fill});
    seq.setSongMode(true);
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
    engine.sequencer().setSidechain(true, 0.55f, 180.0f); // subtle pump on the melodic bus
    // Per-bus insert strip: glue-compress just the drum bus (exercises the mixer-track path).
    mx.track(audio::MixerBus::Drums).compressor().setEnabled(true);
    mx.track(audio::MixerBus::Drums).compressor().setThresholdDb(-14.0f);
    mx.track(audio::MixerBus::Drums).compressor().setRatio(4.0f);
    mx.track(audio::MixerBus::Drums).compressor().setMakeupDb(2.0f);
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
    const bool sequencing = cfg.beat || cfg.melody || cfg.song || loading ||
                            cfg.projectSavePath != nullptr || cfg.midiInPath != nullptr;
    bool appliedBeat = false;
    bool appliedMelody = false;

    if (sequencing) {
        engine.sequencer().setBpm(cfg.bpm);
        engine.sequencer().setSwing(static_cast<float>(cfg.swing));
        if (loading) {
            std::string lerr;
            if (!audio::loadProject(cfg.projectLoadPath, engine.sequencer(), engine.mixer(),
                                    engine.automation(), &lerr)) {
                MAZ_LOG_ERROR("project load failed: %s", lerr.c_str());
                return 1;
            }
            MAZ_LOG_INFO("project: loaded %s", cfg.projectLoadPath);
        } else {
            if (cfg.song) {
                applyDemoSong(engine.sequencer(), cfg.fm, cfg.wavetable);
                appliedBeat = true;
                appliedMelody = true;
            } else {
                // No explicit pattern flags (e.g. bare --save) → the full demo (beat + melody).
                const bool anyPattern = cfg.beat || cfg.melody;
                if (cfg.beat || !anyPattern) {
                    applyDemoBeat(engine.sequencer());
                    appliedBeat = true;
                }
                if (cfg.melody || !anyPattern) {
                    applyDemoMelody(engine.sequencer(), cfg.fm, cfg.wavetable);
                    appliedMelody = true;
                }
            }
            applyDemoMixer(engine);
            if (cfg.automate) {
                applyDemoAuto(engine);
            }
        }
        if (cfg.midiInPath != nullptr) {
            std::string ierr;
            if (audio::readMidi(cfg.midiInPath, engine.sequencer(), &ierr)) {
                MAZ_LOG_INFO("midi: imported %s", cfg.midiInPath);
                appliedMelody = true;
            } else {
                MAZ_LOG_ERROR("midi import failed: %s", ierr.c_str());
                return 1;
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

        if (cfg.pluginPath != nullptr) {
            std::string perr;
            if (engine.mixer().plugin().load(cfg.pluginPath, engine.config().sampleRate, &perr)) {
                engine.mixer().plugin().setEnabled(true);
                MAZ_LOG_INFO("plugin: loaded %s", cfg.pluginPath);
            } else {
                MAZ_LOG_ERROR("plugin load failed: %s", perr.c_str());
                return 1;
            }
        }

        if (cfg.clapPath != nullptr) {
            std::string cerr;
            if (engine.mixer().clap().load(cfg.clapPath, engine.config().sampleRate, 4096, &cerr)) {
                engine.mixer().clap().setEnabled(true);
                MAZ_LOG_INFO("clap: loaded %s", cfg.clapPath);
            } else {
                MAZ_LOG_ERROR("clap load failed: %s", cerr.c_str());
                return 1;
            }
        }

        if (cfg.vst3Path != nullptr) {
            std::string verr;
            if (engine.mixer().vst3().load(cfg.vst3Path, engine.config().sampleRate, 4096, &verr)) {
                engine.mixer().vst3().setEnabled(true);
                MAZ_LOG_INFO("vst3: loaded %s", cfg.vst3Path);
            } else {
                MAZ_LOG_ERROR("vst3 load failed: %s", verr.c_str());
                return 1;
            }
        }

        if (cfg.midiPath != nullptr) {
            std::string merr;
            if (audio::writeMidi(cfg.midiPath, engine.sequencer(), 96, &merr)) {
                MAZ_LOG_INFO("midi: exported %s", cfg.midiPath);
            } else {
                MAZ_LOG_ERROR("midi export failed: %s", merr.c_str());
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

    // Stem export: bounce drums / lead / bass to separate WAVs by isolating each bus.
    if (cfg.stemsPrefix != nullptr && sequencing) {
        audio::Sequencer& seq = engine.sequencer();
        struct Stem {
            const char* name;
            float drum, lead, bass;
        };
        const Stem stems[] = {{"drums", 1.0f, 0.0f, 0.0f},
                              {"lead", 0.0f, 1.0f, 0.0f},
                              {"bass", 0.0f, 0.0f, 1.0f}};
        for (const Stem& st : stems) {
            seq.setDrumGain(st.drum);
            seq.setSynthGain(st.lead);
            seq.setBassGain(st.bass);
            seq.stop();
            engine.mixer().reset();
            seq.play();
            const std::vector<float> stemBuf = engine.renderOffline(cfg.seconds);
            const std::string p = std::string(cfg.stemsPrefix) + "_" + st.name + ".wav";
            std::string serr;
            if (audio::writeWav16(p, stemBuf.data(), static_cast<int>(stemBuf.size()) / channels,
                                  channels, acfg.sampleRate, &serr)) {
                MAZ_LOG_INFO("stems: wrote %s", p.c_str());
            } else {
                MAZ_LOG_ERROR("stems: WAV write failed: %s", serr.c_str());
            }
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
    int lengths[] = {8, 16, 24, 32, 48, 64};
    int lenIdx = 1;
    for (int i = 0; i < 6; ++i) {
        if (lengths[i] == seq.numSteps()) lenIdx = i;
    }
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::Combo("Length", &lenIdx, "8\0" "16\0" "24\0" "32\0" "48\0" "64\0\0")) {
        seq.setNumSteps(lengths[lenIdx]);
    }
    ImGui::SameLine();
    int spb = seq.stepsPerBeat();
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::SliderInt("/beat", &spb, 1, 8)) {
        seq.setStepsPerBeat(spb);
    }
    ImGui::SameLine();
    float swing = seq.swing();
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::SliderFloat("Swing", &swing, 0.0f, 0.75f, "%.2f")) {
        seq.setSwing(swing);
    }
    ImGui::SameLine();
    float humanize = seq.humanize();
    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::SliderFloat("Humanize", &humanize, 0.0f, 1.0f, "%.2f")) {
        seq.setHumanize(humanize);
    }
    ImGui::SameLine();
    bool metro = seq.metronome();
    if (ImGui::Checkbox("Metronome", &metro)) {
        seq.setMetronome(metro);
    }
    ImGui::SameLine();
    int countIn = seq.countInBars();
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::SliderInt("Count-in", &countIn, 0, 4, "%d bars")) {
        seq.setCountInBars(countIn);
    }
    ImGui::SameLine();
    int transpose = seq.transpose();
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::SliderInt("Transpose", &transpose, -24, 24, "%d st")) {
        seq.setTranspose(transpose);
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
        ImGui::PushID(c);
        ImGui::Text("%-9s", seq.channelName(c).c_str());
        ImGui::SameLine(96.0f);
        // Mute / Solo / volume strip.
        const bool mute = seq.channelMute(c);
        if (mute) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.80f, 0.20f, 0.20f, 1.0f));
        if (ImGui::Button("M", ImVec2(20, 20))) seq.setChannelMute(c, !mute);
        if (mute) ImGui::PopStyleColor();
        ImGui::SameLine();
        const bool solo = seq.channelSolo(c);
        if (solo) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.70f, 0.20f, 1.0f));
        if (ImGui::Button("S", ImVec2(20, 20))) seq.setChannelSolo(c, !solo);
        if (solo) ImGui::PopStyleColor();
        ImGui::SameLine();
        float vol = seq.channelVolume(c);
        ImGui::SetNextItemWidth(58.0f);
        if (ImGui::SliderFloat("##vol", &vol, 0.0f, 1.5f, "%.1f")) seq.setChannelVolume(c, vol);
        ImGui::SameLine();
        float pan = seq.channelPan(c);
        ImGui::SetNextItemWidth(58.0f);
        if (ImGui::SliderFloat("##pan", &pan, -1.0f, 1.0f, "%.1f")) seq.setChannelPan(c, pan);
        ImGui::SameLine();
        int choke = seq.channelChokeGroup(c);
        ImGui::SetNextItemWidth(52.0f);
        if (ImGui::SliderInt("##choke", &choke, 0, 4, choke == 0 ? "choke -" : "choke %d"))
            seq.setChannelChokeGroup(c, choke);
        ImGui::SameLine();
        float tune = seq.channelTune(c);
        ImGui::SetNextItemWidth(58.0f);
        if (ImGui::SliderFloat("##tune", &tune, -24.0f, 24.0f, "%.0f st"))
            seq.setChannelTune(c, tune);
        ImGui::SameLine();
        float decay = seq.channelDecay(c);
        ImGui::SetNextItemWidth(58.0f);
        if (ImGui::SliderFloat("##decay", &decay, 0.25f, 4.0f, "d%.2f"))
            seq.setChannelDecay(c, decay);
        ImGui::SameLine();
        for (int s = 0; s < steps; ++s) {
            ImGui::PushID(c * 1000 + s);
            const bool on = seq.step(c, s);
            const float vel = seq.stepVelocity(c, s);
            const bool onBeat = (s % 4) == 0;
            const bool playhead = seq.playing() && s == seq.currentStep();

            // On steps are green, brightness scaled by velocity (right-click cycles the accent).
            const float g = 0.35f + 0.45f * vel;
            ImVec4 col = on ? ImVec4(0.15f, g, 0.30f + 0.15f * vel, 1.0f)
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
            // Right-click an active step to cycle its accent: full → medium → soft → full.
            if (on && ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                const float next = vel > 0.8f ? 0.6f : (vel > 0.45f ? 0.3f : 1.0f);
                seq.setStepVelocity(c, s, next);
            }
            // Scroll over an active step to set its trigger probability; Shift+scroll sets its
            // ratchet (1–4). A tooltip shows both when they differ from the default.
            if (on && ImGui::IsItemHovered()) {
                const float wheel = ImGui::GetIO().MouseWheel;
                if (wheel != 0.0f) {
                    if (ImGui::GetIO().KeyShift) {
                        seq.setStepRatchet(c, s, seq.stepRatchet(c, s) + (wheel > 0.0f ? 1 : -1));
                    } else {
                        seq.setStepProbability(c, s, seq.stepProbability(c, s) + wheel * 0.1f);
                    }
                }
                const float pr = seq.stepProbability(c, s);
                const int rt = seq.stepRatchet(c, s);
                if (pr < 0.999f || rt > 1) {
                    ImGui::SetTooltip("prob %.0f%%  ratchet x%d", pr * 100.0f, rt);
                }
            }
            ImGui::PopStyleColor(3);
            if (s + 1 < steps) {
                ImGui::SameLine();
            }
            ImGui::PopID();
        }
        ImGui::PopID(); // channel row
    }

    ImGui::End();
}

// Draw the piano-roll UI: pitch rows (high at top) × steps. Clicking a cell toggles a note.
void buildPianoRollUI(audio::Sequencer& seq) {
    ImGui::Begin("CJC Music Station — Piano Roll");
    // Lane selector: edit the lead instrument (roll) or the bass (roll2).
    static int lane = 0;
    ImGui::TextUnformatted("Lane:");
    ImGui::SameLine();
    ImGui::RadioButton("Lead", &lane, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Bass", &lane, 1);
    audio::PianoRoll& roll = (lane == 1) ? seq.roll2() : seq.roll();
    ImGui::TextDisabled("Click cells to place notes; each lane plays its own synth.");

    // Chord tool: drop a whole chord (root + quality) at a chosen step/length.
    static int chordRoot = 60, chordStep = 0, chordLen = 4, chordType = 0;
    const char* chordNames[] = {"Maj", "Min", "Dom7", "Maj7", "Min7", "Dim", "Aug", "Sus2", "Sus4"};
    ImGui::SetNextItemWidth(70.0f);
    ImGui::InputInt("root##chord", &chordRoot);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    ImGui::Combo("##chordtype", &chordType, chordNames, 9);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.0f);
    ImGui::InputInt("@##chordstep", &chordStep);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.0f);
    ImGui::InputInt("len##chord", &chordLen);
    ImGui::SameLine();
    if (ImGui::Button("Add chord")) {
        roll.addChord(chordStep, chordLen, chordRoot, static_cast<audio::Chord>(chordType));
    }
    ImGui::SameLine();
    static int quantDiv = 4;
    ImGui::SetNextItemWidth(70.0f);
    ImGui::InputInt("##quantdiv", &quantDiv);
    ImGui::SameLine();
    if (ImGui::Button("Quantize")) {
        roll.quantize(quantDiv);
    }
    ImGui::SameLine();
    static int scaleRoot = 60; // C
    static int scaleType = 0;
    ImGui::SetNextItemWidth(60.0f);
    ImGui::InputInt("root##scale", &scaleRoot);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(130.0f);
    const char* scaleNames[] = {"Major",       "Minor",         "Dorian",
                                "Phrygian",    "Lydian",        "Mixolydian",
                                "Locrian",     "Harm. Minor",   "Mel. Minor",
                                "Penta. Major", "Penta. Minor", "Blues"};
    ImGui::Combo("##scaletype", &scaleType, scaleNames, IM_ARRAYSIZE(scaleNames));
    ImGui::SameLine();
    if (ImGui::Button("Snap to scale")) {
        roll.snapToScale(scaleRoot, static_cast<audio::Scale>(scaleType));
    }

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
            // Scroll over a placed note to set its trigger probability; tooltip shows it when < 100%.
            if (on && ImGui::IsItemHovered()) {
                const float wheel = ImGui::GetIO().MouseWheel;
                if (wheel != 0.0f) {
                    roll.setNoteProbability(pitch, s, roll.noteProbability(pitch, s) + wheel * 0.1f);
                }
                const float pr = roll.noteProbability(pitch, s);
                if (pr < 0.999f) {
                    ImGui::SetTooltip("prob %.0f%%", pr * 100.0f);
                }
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

    // Arpeggiator (drives the piano roll).
    bool arp = seq.arpOn();
    int arpMode = seq.arpMode();
    bool arpCh = ImGui::Checkbox("Arpeggiator", &arp);
    ImGui::SameLine();
    const char* arpModes[] = {"Up", "Down", "Up-Down"};
    ImGui::SetNextItemWidth(120.0f);
    arpCh |= ImGui::Combo("##arpmode", &arpMode, arpModes, 3);
    if (arpCh) {
        seq.setArp(arp, arpMode);
    }
    ImGui::SameLine();
    int arpOct = seq.arpOctaves();
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::SliderInt("oct##arp", &arpOct, 1, 4)) {
        seq.setArpOctaves(arpOct);
    }
    ImGui::Separator();

    int mode = static_cast<int>(syn.mode());
    const char* modes[] = {"Subtractive", "FM", "Wavetable"};
    if (ImGui::Combo("Engine", &mode, modes, 3)) {
        syn.setMode(static_cast<audio::SynthMode>(mode));
    }
    if (syn.mode() == audio::SynthMode::Subtractive) {
        int w = static_cast<int>(syn.waveform());
        const char* waves[] = {"Sine", "Square", "Saw", "Triangle"};
        if (ImGui::Combo("Waveform", &w, waves, 4)) {
            syn.setWaveform(static_cast<audio::Waveform>(w));
        }
        float detune = syn.detuneCents();
        float osc2 = syn.osc2Level();
        float sub = syn.subLevel();
        float noise = syn.noiseLevel();
        bool och = false;
        och |= ImGui::SliderFloat("Detune (cents)", &detune, 0.0f, 50.0f, "%.1f");
        och |= ImGui::SliderFloat("Osc 2", &osc2, 0.0f, 1.0f, "%.2f");
        och |= ImGui::SliderFloat("Sub", &sub, 0.0f, 1.0f, "%.2f");
        och |= ImGui::SliderFloat("Noise", &noise, 0.0f, 1.0f, "%.2f");
        if (och) {
            syn.setOscillators(detune, osc2, sub, noise);
        }
        int uni = syn.unisonVoices();
        float uniDet = syn.unisonDetune();
        bool uch = ImGui::SliderInt("Unison", &uni, 1, 7);
        uch |= ImGui::SliderFloat("Uni detune", &uniDet, 0.0f, 50.0f, "%.1f");
        if (uch) {
            syn.setUnison(uni, uniDet);
        }
        int subw = static_cast<int>(syn.subWaveform());
        const char* subWaves[] = {"Sine", "Square", "Saw", "Triangle"};
        if (ImGui::Combo("Sub wave", &subw, subWaves, 4))
            syn.setSubWaveform(static_cast<audio::Waveform>(subw));
        float ncol = syn.noiseColor();
        if (ImGui::SliderFloat("Noise color", &ncol, 0.0f, 1.0f, "%.2f")) syn.setNoiseColor(ncol);
        bool sync = syn.hardSync();
        if (ImGui::Checkbox("Hard sync", &sync)) syn.setHardSync(sync);
        float syncRatio = syn.syncRatio();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("Sync ratio", &syncRatio, 1.0f, 8.0f, "%.2f"))
            syn.setSyncRatio(syncRatio);
        float pw = syn.pulseWidth();
        if (ImGui::SliderFloat("Pulse width", &pw, 0.02f, 0.98f, "%.2f")) syn.setPulseWidth(pw);
    } else if (syn.mode() == audio::SynthMode::FM) {
        float ratio = syn.fmRatio();
        if (ImGui::SliderFloat("FM Ratio", &ratio, 0.5f, 8.0f, "%.2f")) syn.setFmRatio(ratio);
        float index = syn.fmIndex();
        if (ImGui::SliderFloat("FM Index", &index, 0.0f, 10.0f, "%.2f")) syn.setFmIndex(index);
    } else {
        float pos = syn.wavetablePosition();
        if (ImGui::SliderFloat("WT Position", &pos, 0.0f, 1.0f, "%.2f"))
            syn.setWavetablePosition(pos);
        float morph = syn.wavetableMorph();
        if (ImGui::SliderFloat("WT Env Morph", &morph, 0.0f, 1.0f, "%.2f"))
            syn.setWavetableMorph(morph);
        // Four morph-frame selectors (frame 0 → 3 as the position sweeps).
        const char* waves[] = {"Sine", "Square", "Saw", "Triangle"};
        int fr[4];
        bool frCh = false;
        for (int k = 0; k < 4; ++k) {
            fr[k] = static_cast<int>(syn.wavetableFrame(k));
            ImGui::PushID(k);
            ImGui::SetNextItemWidth(90.0f);
            frCh |= ImGui::Combo("##wtframe", &fr[k], waves, 4);
            ImGui::PopID();
            if (k < 3) ImGui::SameLine();
        }
        if (frCh) {
            syn.setWavetableFrames(static_cast<audio::Waveform>(fr[0]),
                                   static_cast<audio::Waveform>(fr[1]),
                                   static_cast<audio::Waveform>(fr[2]),
                                   static_cast<audio::Waveform>(fr[3]));
        }
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
    float glide = syn.glide();
    if (ImGui::SliderFloat("Glide (portamento)", &glide, 0.0f, 1.0f, "%.3f s")) syn.setGlide(glide);
    float vibRate = syn.vibratoRate();
    float vibDepth = syn.vibratoDepth();
    bool vch = ImGui::SliderFloat("Vibrato rate", &vibRate, 0.0f, 12.0f, "%.1f Hz");
    vch |= ImGui::SliderFloat("Vibrato depth", &vibDepth, 0.0f, 100.0f, "%.0f cents");
    if (vch) {
        syn.setVibrato(vibRate, vibDepth);
    }
    float peAmt = syn.pitchEnvAmount();
    float peTime = syn.pitchEnvTime();
    bool pech = ImGui::SliderFloat("Pitch env", &peAmt, -24.0f, 24.0f, "%.0f st");
    pech |= ImGui::SliderFloat("Pitch env time", &peTime, 0.001f, 0.5f, "%.3f s");
    if (pech) {
        syn.setPitchEnv(peAmt, peTime);
    }

    ImGui::SeparatorText("Filter (resonant low-pass)");
    float cutoff = syn.filterCutoff();
    float reso = syn.filterResonance();
    float envAmt = syn.filterEnvAmount();
    bool fch = false;
    fch |= ImGui::SliderFloat("Cutoff", &cutoff, 20.0f, 20000.0f, "%.0f Hz", ImGuiSliderFlags_Logarithmic);
    fch |= ImGui::SliderFloat("Resonance", &reso, 0.5f, 20.0f, "%.1f");
    fch |= ImGui::SliderFloat("Env->Cutoff", &envAmt, 0.0f, 10000.0f, "%.0f Hz");
    if (fch) {
        syn.setFilter(cutoff, reso, envAmt);
    }

    ImGui::SeparatorText("Sampler");
    bool useSampler = seq.useSampler();
    if (ImGui::Checkbox("Use sampler for melody", &useSampler)) {
        seq.setUseSampler(useSampler);
    }
    ImGui::Text("loaded: %s",
                seq.sampler().loaded() ? seq.sampler().path().c_str() : "(none)");
    bool rev = seq.sampler().reverse();
    if (ImGui::Checkbox("Reverse", &rev)) seq.sampler().setReverse(rev);
    ImGui::SameLine();
    bool lp = seq.sampler().loop();
    if (ImGui::Checkbox("Loop", &lp)) seq.sampler().setLoop(lp);
    ImGui::SameLine();
    float startOff = seq.sampler().startOffset();
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::SliderFloat("Start", &startOff, 0.0f, 0.99f, "%.2f"))
        seq.sampler().setStartOffset(startOff);
    float smpAtk = seq.sampler().attack();
    float smpRel = seq.sampler().release();
    bool smpEnvCh = false;
    ImGui::SetNextItemWidth(110.0f);
    smpEnvCh |= ImGui::SliderFloat("Atk##smp", &smpAtk, 0.001f, 0.5f, "%.3f s");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    smpEnvCh |= ImGui::SliderFloat("Rel##smp", &smpRel, 0.001f, 1.0f, "%.3f s");
    if (smpEnvCh) {
        seq.sampler().setAmpEnv(smpAtk, smpRel);
    }
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

// Draw a compact panel for the second (bass) instrument.
void buildBassUI(audio::SynthInstrument& syn) {
    ImGui::Begin("CJC Music Station — Bass Synth");
    int w = static_cast<int>(syn.waveform());
    const char* waves[] = {"Sine", "Square", "Saw", "Triangle"};
    if (ImGui::Combo("Waveform##bass", &w, waves, 4)) {
        syn.setWaveform(static_cast<audio::Waveform>(w));
    }
    float a = syn.attack(), d = syn.decay(), s = syn.sustain(), r = syn.release();
    bool ech = false;
    ech |= ImGui::SliderFloat("Attack##bass", &a, 0.001f, 1.0f, "%.3f s");
    ech |= ImGui::SliderFloat("Decay##bass", &d, 0.001f, 1.0f, "%.3f s");
    ech |= ImGui::SliderFloat("Sustain##bass", &s, 0.0f, 1.0f, "%.2f");
    ech |= ImGui::SliderFloat("Release##bass", &r, 0.001f, 2.0f, "%.3f s");
    if (ech) {
        syn.setEnvelope(a, d, s, r);
    }
    float bglide = syn.glide();
    if (ImGui::SliderFloat("Glide##bass", &bglide, 0.0f, 1.0f, "%.3f s")) syn.setGlide(bglide);
    float cutoff = syn.filterCutoff(), reso = syn.filterResonance(), env = syn.filterEnvAmount();
    bool fch = false;
    fch |= ImGui::SliderFloat("Cutoff##bass", &cutoff, 20.0f, 20000.0f, "%.0f Hz", ImGuiSliderFlags_Logarithmic);
    fch |= ImGui::SliderFloat("Resonance##bass", &reso, 0.5f, 20.0f, "%.1f");
    if (fch) {
        syn.setFilter(cutoff, reso, env);
    }
    float sub = syn.subLevel();
    if (ImGui::SliderFloat("Sub##bass", &sub, 0.0f, 1.0f, "%.2f")) {
        syn.setOscillators(syn.detuneCents(), syn.osc2Level(), sub, syn.noiseLevel());
    }
    float gain = syn.gain();
    if (ImGui::SliderFloat("Level##bass", &gain, 0.0f, 1.0f, "%.2f")) {
        syn.setGain(gain);
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
    float ceiling = mx.limiterCeiling();
    if (ImGui::SliderFloat("Ceiling", &ceiling, 0.1f, 1.0f, "%.2f")) {
        mx.setLimiterCeiling(ceiling);
    }
    float drums = seq.drumGain();
    if (ImGui::SliderFloat("Drums", &drums, 0.0f, 2.0f, "%.2f")) {
        seq.setDrumGain(drums);
    }
    float synth = seq.synthGain();
    if (ImGui::SliderFloat("Lead", &synth, 0.0f, 2.0f, "%.2f")) {
        seq.setSynthGain(synth);
    }
    ImGui::SameLine();
    float leadPan = seq.leadPan();
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::SliderFloat("pan##lead", &leadPan, -1.0f, 1.0f, "%.2f")) {
        seq.setLeadPan(leadPan);
    }
    float bass = seq.bassGain();
    if (ImGui::SliderFloat("Bass", &bass, 0.0f, 2.0f, "%.2f")) {
        seq.setBassGain(bass);
    }
    ImGui::SameLine();
    float bassPan = seq.bassPan();
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::SliderFloat("pan##bass", &bassPan, -1.0f, 1.0f, "%.2f")) {
        seq.setBassPan(bassPan);
    }

    // Sidechain (kick ducks the synth bus).
    bool sc = seq.sidechainOn();
    float scAmt = seq.sidechainAmount();
    float scRel = seq.sidechainReleaseMs();
    bool scChanged = ImGui::Checkbox("Sidechain", &sc);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    scChanged |= ImGui::SliderFloat("amt##sc", &scAmt, 0.0f, 1.0f, "%.2f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    scChanged |= ImGui::SliderFloat("rel ms##sc", &scRel, 20.0f, 500.0f, "%.0f");
    if (scChanged) {
        seq.setSidechain(sc, scAmt, scRel);
    }

    ImGui::SeparatorText("Master FX");
    {
        bool en = mx.peq().enabled();
        if (ImGui::Checkbox("Parametric EQ", &en)) mx.peq().setEnabled(en);
        float lowDb = mx.peq().lowGain();
        float midDb = mx.peq().midGain();
        float midF = mx.peq().midFreq();
        float midQ = mx.peq().midQ();
        float highDb = mx.peq().highGain();
        bool ch = false;
        ImGui::SetNextItemWidth(90.0f);
        ch |= ImGui::SliderFloat("Low dB##peq", &lowDb, -18.0f, 18.0f, "%.1f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        ch |= ImGui::SliderFloat("High dB##peq", &highDb, -18.0f, 18.0f, "%.1f");
        ImGui::SetNextItemWidth(90.0f);
        ch |= ImGui::SliderFloat("Mid dB##peq", &midDb, -18.0f, 18.0f, "%.1f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ch |= ImGui::SliderFloat("Mid Hz##peq", &midF, 200.0f, 8000.0f, "%.0f");
        if (ch) {
            mx.peq().setLowGain(lowDb);
            mx.peq().setMid(midF, midQ, midDb);
            mx.peq().setHighGain(highDb);
        }
    }
    {
        bool en = mx.tilt().enabled();
        if (ImGui::Checkbox("Tilt EQ", &en)) mx.tilt().setEnabled(en);
        float t = mx.tilt().tilt();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("dark<>bright##tilt", &t, -12.0f, 12.0f, "%.1f dB")) mx.tilt().setTilt(t);
    }
    {
        bool en = mx.exciter().enabled();
        if (ImGui::Checkbox("Exciter", &en)) mx.exciter().setEnabled(en);
        float xover = mx.exciter().crossover();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("Hz##exciter", &xover, 1000.0f, 12000.0f, "%.0f"))
            mx.exciter().setCrossover(xover);
        float amt = mx.exciter().amount();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("amount##exciter", &amt, 0.0f, 1.0f, "%.2f"))
            mx.exciter().setAmount(amt);
    }
    {
        bool en = mx.eq().enabled();
        if (ImGui::Checkbox("Low-Pass EQ", &en)) mx.eq().setEnabled(en);
        float cutoff = mx.eq().cutoff();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("Hz##eq", &cutoff, 200.0f, 18000.0f, "%.0f")) mx.eq().setCutoff(cutoff);
    }
    {
        bool en = mx.highpass().enabled();
        if (ImGui::Checkbox("High-Pass", &en)) mx.highpass().setEnabled(en);
        float cutoff = mx.highpass().cutoff();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("Hz##hp", &cutoff, 10.0f, 2000.0f, "%.0f")) mx.highpass().setCutoff(cutoff);
    }
    {
        bool en = mx.distortion().enabled();
        if (ImGui::Checkbox("Distortion", &en)) mx.distortion().setEnabled(en);
        float drive = mx.distortion().drive();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("drive##dist", &drive, 1.0f, 20.0f, "%.1f")) mx.distortion().setDrive(drive);
        int curve = static_cast<int>(mx.distortion().curve());
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::Combo("##distcurve", &curve, "Soft\0Hard\0Fold\0SineFold\0\0"))
            mx.distortion().setCurve(static_cast<audio::Distortion::Curve>(curve));
    }
    {
        bool en = mx.tape().enabled();
        if (ImGui::Checkbox("Tape Sat", &en)) mx.tape().setEnabled(en);
        float drive = mx.tape().drive();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("drive##tape", &drive, 1.0f, 12.0f, "%.1f")) mx.tape().setDrive(drive);
        float warmth = mx.tape().warmth();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("warmth##tape", &warmth, 0.0f, 1.0f, "%.2f")) mx.tape().setWarmth(warmth);
    }
    {
        bool en = mx.ringmod().enabled();
        if (ImGui::Checkbox("Ring Mod", &en)) mx.ringmod().setEnabled(en);
        float freq = mx.ringmod().freq();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("Hz##ring", &freq, 1.0f, 4000.0f, "%.0f")) mx.ringmod().setFreq(freq);
        float wet = mx.ringmod().mix();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("mix##ring", &wet, 0.0f, 1.0f, "%.2f")) mx.ringmod().setMix(wet);
    }
    {
        bool en = mx.bitcrusher().enabled();
        if (ImGui::Checkbox("Bitcrusher", &en)) mx.bitcrusher().setEnabled(en);
        float bits = mx.bitcrusher().bits();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("bits##crush", &bits, 1.0f, 16.0f, "%.0f")) mx.bitcrusher().setBits(bits);
    }
    {
        bool en = mx.gate().enabled();
        if (ImGui::Checkbox("Gate", &en)) mx.gate().setEnabled(en);
        float thr = mx.gate().thresholdDb();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("dB##gate", &thr, -80.0f, 0.0f, "%.0f")) mx.gate().setThresholdDb(thr);
        float hold = mx.gate().holdMs();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("hold##gate", &hold, 0.0f, 500.0f, "%.0f ms")) mx.gate().setHoldMs(hold);
    }
    {
        bool en = mx.compressor().enabled();
        if (ImGui::Checkbox("Compressor", &en)) mx.compressor().setEnabled(en);
        float thr = mx.compressor().thresholdDb();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("dB##cmp", &thr, -48.0f, 0.0f, "%.0f")) mx.compressor().setThresholdDb(thr);
        float knee = mx.compressor().kneeDb();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("knee##cmp", &knee, 0.0f, 24.0f, "%.0f")) mx.compressor().setKneeDb(knee);
    }
    {
        bool en = mx.transient().enabled();
        if (ImGui::Checkbox("Transient", &en)) mx.transient().setEnabled(en);
        float atk = mx.transient().attack();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("attack##trans", &atk, -1.0f, 1.0f, "%.2f"))
            mx.transient().setAttack(atk);
        float sus = mx.transient().sustain();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("sustain##trans", &sus, -1.0f, 1.0f, "%.2f"))
            mx.transient().setSustain(sus);
    }
    {
        bool en = mx.chorus().enabled();
        if (ImGui::Checkbox("Chorus", &en)) mx.chorus().setEnabled(en);
        float wet = mx.chorus().mix();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("mix##cho", &wet, 0.0f, 1.0f, "%.2f")) mx.chorus().setMix(wet);
    }
    {
        bool en = mx.flanger().enabled();
        if (ImGui::Checkbox("Flanger", &en)) mx.flanger().setEnabled(en);
        float wet = mx.flanger().mix();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::SliderFloat("mix##fla", &wet, 0.0f, 1.0f, "%.2f")) mx.flanger().setMix(wet);
        float fb = mx.flanger().feedback();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::SliderFloat("fb##fla", &fb, 0.0f, 0.95f, "%.2f")) mx.flanger().setFeedback(fb);
    }
    {
        bool en = mx.phaser().enabled();
        if (ImGui::Checkbox("Phaser", &en)) mx.phaser().setEnabled(en);
        float wet = mx.phaser().mix();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("mix##phs", &wet, 0.0f, 1.0f, "%.2f")) mx.phaser().setMix(wet);
    }
    {
        bool en = mx.delay().enabled();
        if (ImGui::Checkbox("Delay", &en)) mx.delay().setEnabled(en);
        float wet = mx.delay().mix();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("mix##dly", &wet, 0.0f, 1.0f, "%.2f")) mx.delay().setMix(wet);
        ImGui::SameLine();
        bool pp = mx.delay().pingPong();
        if (ImGui::Checkbox("Ping-pong", &pp)) mx.delay().setPingPong(pp);
        float damp = mx.delay().damping();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("damp##dly", &damp, 0.0f, 1.0f, "%.2f")) mx.delay().setDamping(damp);
    }
    {
        bool en = mx.reverb().enabled();
        if (ImGui::Checkbox("Reverb", &en)) mx.reverb().setEnabled(en);
        float wet = mx.reverb().mix();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("mix##rev", &wet, 0.0f, 1.0f, "%.2f")) mx.reverb().setMix(wet);
        float pre = mx.reverb().preDelayMs();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("pre-delay##rev", &pre, 0.0f, 250.0f, "%.0f ms"))
            mx.reverb().setPreDelayMs(pre);
        float rw = mx.reverb().width();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("width##rev", &rw, 0.0f, 2.0f, "%.2f")) mx.reverb().setWidth(rw);
    }
    {
        bool en = mx.widener().enabled();
        if (ImGui::Checkbox("Stereo Widener", &en)) mx.widener().setEnabled(en);
        float w = mx.widener().width();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("width##wide", &w, 0.0f, 2.0f, "%.2f")) mx.widener().setWidth(w);
    }
    {
        bool en = mx.autopan().enabled();
        if (ImGui::Checkbox("Auto-Pan", &en)) mx.autopan().setEnabled(en);
        float rate = mx.autopan().rate();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("rate##apan", &rate, 0.05f, 12.0f, "%.2f Hz")) mx.autopan().setRate(rate);
        float depth = mx.autopan().depth();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("depth##apan", &depth, 0.0f, 1.0f, "%.2f")) mx.autopan().setDepth(depth);
    }
    {
        bool en = mx.monobass().enabled();
        if (ImGui::Checkbox("Mono Bass", &en)) mx.monobass().setEnabled(en);
        float x = mx.monobass().crossover();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("Hz##mono", &x, 20.0f, 500.0f, "%.0f")) mx.monobass().setCrossover(x);
    }

    ImGui::SeparatorText("Send / Return Buses");
    {
        float rSend = mx.reverbSend();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("Reverb send", &rSend, 0.0f, 1.0f, "%.2f")) mx.setReverbSend(rSend);
        float rRoom = mx.reverbReturn().roomSize();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("room##rsend", &rRoom, 0.0f, 1.0f, "%.2f"))
            mx.reverbReturn().setRoomSize(rRoom);

        float dSend = mx.delaySend();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("Delay send", &dSend, 0.0f, 1.0f, "%.2f")) mx.setDelaySend(dSend);
        float dTime = mx.delayReturn().time();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("ms##dsend", &dTime, 10.0f, 1000.0f, "%.0f"))
            mx.delayReturn().setTime(dTime);
    }

    ImGui::SeparatorText("Insert Strips (per bus)");
    {
        const char* busNames[] = {"Drums", "Lead", "Bass"};
        for (int t = 0; t < audio::Mixer::trackCount(); ++t) {
            audio::MixerTrack& tr = mx.track(t);
            ImGui::PushID(t);
            ImGui::TextUnformatted(busNames[t]);
            ImGui::SameLine();
            bool muted = tr.muted();
            if (ImGui::Checkbox("Mute##trk", &muted)) tr.setMuted(muted);
            ImGui::SameLine();
            float g = tr.gain();
            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::SliderFloat("gain##trk", &g, 0.0f, 2.0f, "%.2f")) tr.setGain(g);

            bool eqEn = tr.eq().enabled();
            if (ImGui::Checkbox("EQ##trk", &eqEn)) tr.eq().setEnabled(eqEn);
            ImGui::SameLine();
            bool distEn = tr.distortion().enabled();
            if (ImGui::Checkbox("Drive##trk", &distEn)) tr.distortion().setEnabled(distEn);
            ImGui::SameLine();
            bool compEn = tr.compressor().enabled();
            if (ImGui::Checkbox("Comp##trk", &compEn)) tr.compressor().setEnabled(compEn);
            ImGui::PopID();
        }
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

// Draw the arrangement panel: pattern selector, add-pattern, song-mode toggle, and the playlist.
void buildArrangementUI(audio::Sequencer& seq) {
    ImGui::Begin("CJC Music Station — Arrangement");

    ImGui::Text("Pattern:");
    for (int i = 0; i < seq.patternCount(); ++i) {
        ImGui::SameLine();
        char label[16];
        std::snprintf(label, sizeof(label), "P%d", i + 1);
        if (ImGui::RadioButton(label, seq.currentPattern() == i)) {
            seq.selectPattern(i);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Add")) {
        seq.selectPattern(seq.addPattern());
    }
    ImGui::SameLine();
    if (ImGui::Button("Clone")) {
        seq.selectPattern(seq.clonePattern(seq.currentPattern()));
    }

    // Edit the current pattern's name.
    char nameBuf[64];
    std::snprintf(nameBuf, sizeof(nameBuf), "%s", seq.patternName(seq.currentPattern()).c_str());
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
        seq.setPatternName(seq.currentPattern(), nameBuf);
    }

    bool song = seq.songMode();
    if (ImGui::Checkbox("Song mode (play the playlist)", &song)) {
        seq.setSongMode(song);
    }
    ImGui::SameLine();
    bool songLoop = seq.songLoop();
    if (ImGui::Checkbox("Loop song", &songLoop)) {
        seq.setSongLoop(songLoop);
    }

    ImGui::Text("Playlist:");
    ImGui::SameLine();
    if (seq.playlist().empty()) {
        ImGui::TextDisabled("(empty)");
    } else {
        for (int idx : seq.playlist()) {
            ImGui::SameLine();
            ImGui::Text("P%d", idx + 1);
        }
    }
    if (ImGui::Button("Append current")) {
        seq.appendToPlaylist(seq.currentPattern());
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear playlist")) {
        seq.clearPlaylist();
    }

    ImGui::End();
}

// Windowed: real-time device + the channel rack, piano roll, mixer, automation, arrangement, tone.
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
    applyDemoMelody(engine.sequencer(), cfg.fm, cfg.wavetable);
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
                buildBassUI(engine.sequencer().synth2());
                buildMixerUI(engine);
                buildAutomationUI(engine.automation());
                buildArrangementUI(engine.sequencer());

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
