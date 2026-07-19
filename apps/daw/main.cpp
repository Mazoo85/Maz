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

    // Stem export (FL-style): bounce drums / lead / bass to separate WAVs. Each bus is rendered
    // through its own mixer-track insert strip but NOT the master chain (pre-master stems), in a
    // single pass — so they can be mixed/mastered downstream without the master FX baked in twice.
    if (cfg.stemsPrefix != nullptr && sequencing) {
        engine.sequencer().stop();
        engine.sequencer().play();
        const audio::AudioEngine::Stems st = engine.renderStemsOffline(cfg.seconds);
        struct Out {
            const char* name;
            const std::vector<float>* buf;
        };
        const Out outs[] = {{"drums", &st.drums}, {"lead", &st.lead}, {"bass", &st.bass}};
        for (const Out& o : outs) {
            const std::string p = std::string(cfg.stemsPrefix) + "_" + o.name + ".wav";
            const int sf = channels > 0 ? static_cast<int>(o.buf->size()) / channels : 0;
            std::string serr;
            if (audio::writeWav16(p, o.buf->data(), sf, channels, acfg.sampleRate, &serr)) {
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
    float metroLvl = seq.metronomeLevel();
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::SliderFloat("lvl##metro", &metroLvl, 0.0f, 1.0f, "%.2f")) {
        seq.setMetronomeLevel(metroLvl);
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
        {
            const char* drumNames[] = {"Kick",    "Snare",   "ClosedHat",  "OpenHat", "Clap",
                                       "Tom",     "Cowbell", "Rimshot",    "Crash",   "Ride",
                                       "Shaker",  "Clave",   "Tambourine", "Conga",   "Woodblock",
                                       "Bongo",   "Triangle", "808",     "Zap",    "Riser",  "808 Snare", "808 Hat", "808 Clap", "Snap", "Timbale"};
            int dt = static_cast<int>(seq.channelType(c));
            ImGui::SetNextItemWidth(90.0f);
            if (ImGui::Combo("##drumtype", &dt, drumNames, IM_ARRAYSIZE(drumNames)))
                seq.setChannelType(c, static_cast<audio::Drum>(dt));
        }
        ImGui::SameLine();
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
        float drive = seq.channelDrive(c);
        ImGui::SetNextItemWidth(58.0f);
        if (ImGui::SliderFloat("##drive", &drive, 0.0f, 1.0f, "dr%.2f"))
            seq.setChannelDrive(c, drive);
        ImGui::SameLine();
        float flam = seq.channelFlam(c);
        ImGui::SetNextItemWidth(58.0f);
        if (ImGui::SliderFloat("##flam", &flam, 0.0f, 50.0f, "fl%.0f"))
            seq.setChannelFlam(c, flam);
        ImGui::SameLine();
        float penv = seq.channelPitchEnv(c);
        ImGui::SetNextItemWidth(58.0f);
        if (ImGui::SliderFloat("##punch", &penv, 0.0f, 2.0f, "pn%.2f"))
            seq.setChannelPitchEnv(c, penv);
        ImGui::SameLine();
        float petime = seq.channelPitchEnvTime(c);
        ImGui::SetNextItemWidth(58.0f);
        if (ImGui::SliderFloat("##penvtime", &petime, 0.25f, 4.0f, "pt%.2f"))
            seq.setChannelPitchEnvTime(c, petime);
        ImGui::SameLine();
        float tone = seq.channelTone(c);
        ImGui::SetNextItemWidth(70.0f);
        if (ImGui::SliderFloat("##tone", &tone, 200.0f, 20000.0f, "to%.0f"))
            seq.setChannelTone(c, tone);
        ImGui::SameLine();
        float snap = seq.channelSnap(c);
        ImGui::SetNextItemWidth(58.0f);
        if (ImGui::SliderFloat("##snap", &snap, 0.0f, 1.0f, "sn%.2f"))
            seq.setChannelSnap(c, snap);
        ImGui::SameLine();
        ImGui::PushID(c * 7 + 5);
        if (ImGui::SmallButton("<")) seq.rotateChannel(c, -1);
        ImGui::SameLine();
        if (ImGui::SmallButton(">")) seq.rotateChannel(c, 1);
        ImGui::SameLine();
        static int euclidPulses[64] = {0};
        int& ep = euclidPulses[c < 64 ? c : 0];
        ImGui::SetNextItemWidth(40.0f);
        ImGui::InputInt("##eucn", &ep, 0, 0);
        ImGui::SameLine();
        if (ImGui::SmallButton("Eu")) seq.euclidFill(c, ep);
        ImGui::PopID();
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
                    if (ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyShift) {
                        seq.setStepNudge(c, s, seq.stepNudge(c, s) + (wheel > 0.0f ? 5 : -5));
                    } else if (ImGui::GetIO().KeyCtrl) {
                        seq.setStepTune(c, s, seq.stepTune(c, s) + (wheel > 0.0f ? 1 : -1));
                    } else if (ImGui::GetIO().KeyShift) {
                        seq.setStepRatchet(c, s, seq.stepRatchet(c, s) + (wheel > 0.0f ? 1 : -1));
                    } else {
                        seq.setStepProbability(c, s, seq.stepProbability(c, s) + wheel * 0.1f);
                    }
                }
                const float pr = seq.stepProbability(c, s);
                const int rt = seq.stepRatchet(c, s);
                const int tn = seq.stepTune(c, s);
                const int nd = seq.stepNudge(c, s);
                if (pr < 0.999f || rt > 1 || tn != 0 || nd != 0) {
                    ImGui::SetTooltip("prob %.0f%%  ratchet x%d  pitch %+d st  nudge %d%%",
                                      pr * 100.0f, rt, tn, nd);
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
    const char* chordNames[] = {"Maj",  "Min",  "Dom7", "Maj7", "Min7", "Dim",   "Aug",
                                "Sus2", "Sus4", "Maj6", "Min6", "Maj9", "Min9",  "Dom9",
                                "Add9", "Dim7", "m7b5", "11th", "13th"};
    ImGui::SetNextItemWidth(70.0f);
    ImGui::InputInt("root##chord", &chordRoot);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    ImGui::Combo("##chordtype", &chordType, chordNames, IM_ARRAYSIZE(chordNames));
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
    if (ImGui::Button("Harmonize")) {
        roll.harmonize(static_cast<audio::Chord>(chordType)); // thicken the whole line into chords
    }
    ImGui::SameLine();
    static int quantDiv = 4;
    static float quantStrength = 1.0f;
    ImGui::SetNextItemWidth(70.0f);
    ImGui::InputInt("##quantdiv", &quantDiv);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    ImGui::SliderFloat("str##quant", &quantStrength, 0.0f, 1.0f, "%.2f");
    ImGui::SameLine();
    if (ImGui::Button("Quantize")) {
        roll.quantizeStrength(quantDiv, quantStrength);
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
                                "Penta. Major", "Penta. Minor", "Blues",
                                "Whole Tone",  "Chromatic",     "Phryg. Dom.",
                                "Hungarian Min."};
    ImGui::Combo("##scaletype", &scaleType, scaleNames, IM_ARRAYSIZE(scaleNames));
    ImGui::SameLine();
    if (ImGui::Button("Snap to scale")) {
        roll.snapToScale(scaleRoot, static_cast<audio::Scale>(scaleType));
    }
    ImGui::SameLine();
    static int diaDegrees = 2;
    ImGui::SetNextItemWidth(40.0f);
    ImGui::InputInt("##diadeg", &diaDegrees, 0, 0);
    ImGui::SameLine();
    if (ImGui::Button("Transpose in key")) {
        roll.transposeDiatonic(diaDegrees, scaleRoot, static_cast<audio::Scale>(scaleType));
    }
    ImGui::SameLine();
    static int mutateSeed = 1;
    if (ImGui::Button("Mutate")) {
        // Randomize ~40% of notes by up to ±2 scale degrees, staying in key; bump the seed each
        // click so repeated presses give fresh variations.
        roll.mutate(0.4f, scaleRoot, static_cast<audio::Scale>(scaleType), 2,
                    static_cast<uint32_t>(mutateSeed++));
    }
    ImGui::SameLine();
    static int strumStep = 1;
    ImGui::SetNextItemWidth(60.0f);
    ImGui::InputInt("##strumstep", &strumStep);
    ImGui::SameLine();
    if (ImGui::Button("Strum")) {
        roll.strum(strumStep);
    }
    ImGui::SameLine();
    if (ImGui::Button("Legato")) {
        roll.legato();
    }
    ImGui::SameLine();
    static int invertPivot = 60; // middle C
    ImGui::SetNextItemWidth(60.0f);
    ImGui::InputInt("##invpivot", &invertPivot);
    ImGui::SameLine();
    if (ImGui::Button("Invert")) {
        roll.invert(invertPivot);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reverse")) {
        roll.reverseTime();
    }
    ImGui::SameLine();
    static float humanizeAmt = 0.3f;
    static uint32_t humanizeSeed = 1u;
    ImGui::SetNextItemWidth(70.0f);
    ImGui::SliderFloat("##humamt", &humanizeAmt, 0.0f, 1.0f, "%.2f");
    ImGui::SameLine();
    if (ImGui::Button("Humanize")) {
        roll.randomizeVelocity(humanizeAmt, humanizeSeed++);
    }
    ImGui::SameLine();
    static int dupOffset = 16;
    ImGui::SetNextItemWidth(60.0f);
    ImGui::InputInt("##dupoff", &dupOffset, 0, 0);
    ImGui::SameLine();
    if (ImGui::Button("Duplicate")) {
        roll.duplicate(dupOffset);
    }
    ImGui::SameLine();
    static int timingRange = 1;
    static uint32_t timingSeed = 1u;
    ImGui::SetNextItemWidth(50.0f);
    ImGui::InputInt("##timerand", &timingRange, 0, 0);
    ImGui::SameLine();
    if (ImGui::Button("Rand time")) {
        roll.randomizeTiming(timingRange, timingSeed++);
    }
    ImGui::SameLine();
    static float rampFrom = 0.4f, rampTo = 1.0f;
    ImGui::SetNextItemWidth(60.0f);
    ImGui::SliderFloat("##rampfrom", &rampFrom, 0.0f, 1.0f, "%.2f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.0f);
    ImGui::SliderFloat("##rampto", &rampTo, 0.0f, 1.0f, "%.2f");
    ImGui::SameLine();
    if (ImGui::Button("Vel ramp")) {
        roll.velocityRamp(rampFrom, rampTo);
    }
    ImGui::SameLine();
    static int chopPieces = 4;
    ImGui::SetNextItemWidth(50.0f);
    ImGui::InputInt("##choppieces", &chopPieces, 0, 0);
    ImGui::SameLine();
    if (ImGui::Button("Chop")) {
        roll.chop(chopPieces);
    }
    ImGui::SameLine();
    static int echoReps = 3;
    static int echoGap = 4;
    static float echoDecay = 0.6f;
    ImGui::SetNextItemWidth(40.0f);
    ImGui::InputInt("##echoreps", &echoReps, 0, 0);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(40.0f);
    ImGui::InputInt("##echogap", &echoGap, 0, 0);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70.0f);
    ImGui::SliderFloat("##echodecay", &echoDecay, 0.0f, 1.0f, "%.2f");
    ImGui::SameLine();
    if (ImGui::Button("Echo")) {
        roll.echo(echoReps, echoGap, echoDecay);
    }
    ImGui::SameLine();
    static int flamGap = 1;
    static float flamVel = 0.5f;
    ImGui::SetNextItemWidth(40.0f);
    ImGui::InputInt("##flamgap", &flamGap, 0, 0);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70.0f);
    ImGui::SliderFloat("##flamvel", &flamVel, 0.0f, 1.0f, "%.2f");
    ImGui::SameLine();
    if (ImGui::Button("Flam")) {
        roll.flam(flamGap, flamVel);
    }
    ImGui::SameLine();
    static int arpLen = 2;
    static int arpBakeMode = 0;
    ImGui::SetNextItemWidth(50.0f);
    ImGui::InputInt("##arplen", &arpLen, 0, 0);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    const char* arpBakeModes[] = {"up", "down", "up-dn"};
    ImGui::Combo("##arpbakemode", &arpBakeMode, arpBakeModes, 3);
    ImGui::SameLine();
    if (ImGui::Button("Arp notes")) {
        roll.arpeggiate(arpLen, arpBakeMode);
    }
    ImGui::SameLine();
    static int transposeSemis = 12;
    ImGui::SetNextItemWidth(50.0f);
    ImGui::InputInt("##transposesemis", &transposeSemis, 0, 0);
    ImGui::SameLine();
    if (ImGui::Button("Transpose")) {
        roll.transpose(transposeSemis);
    }
    ImGui::SameLine();
    static int limitLo = 48, limitHi = 72; // C3..C5 default range
    ImGui::SetNextItemWidth(46.0f);
    ImGui::InputInt("##limitlo", &limitLo, 0, 0);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(46.0f);
    ImGui::InputInt("##limithi", &limitHi, 0, 0);
    ImGui::SameLine();
    if (ImGui::Button("Limit")) {
        roll.limitToRange(limitLo, limitHi);
    }
    ImGui::SameLine();
    static int shiftSteps = 1;
    ImGui::SetNextItemWidth(50.0f);
    ImGui::InputInt("##shiftsteps", &shiftSteps, 0, 0);
    ImGui::SameLine();
    if (ImGui::Button("Shift")) {
        roll.shift(shiftSteps);
    }
    ImGui::SameLine();
    static float stretchFactor = 2.0f;
    ImGui::SetNextItemWidth(50.0f);
    ImGui::InputFloat("##stretchfactor", &stretchFactor, 0.0f, 0.0f, "%.2f");
    ImGui::SameLine();
    if (ImGui::Button("Stretch")) {
        roll.stretch(stretchFactor);
    }
    ImGui::SameLine();
    static float gateFactor = 0.5f;
    ImGui::SetNextItemWidth(50.0f);
    ImGui::InputFloat("##gatefactor", &gateFactor, 0.0f, 0.0f, "%.2f");
    ImGui::SameLine();
    if (ImGui::Button("Gate")) {
        roll.scaleLengths(gateFactor);
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
            // Scroll over a placed note to set its trigger probability; Ctrl+scroll sets its fine
            // tune (cents). Tooltips show either when non-default.
            if (on && ImGui::IsItemHovered()) {
                const float wheel = ImGui::GetIO().MouseWheel;
                if (wheel != 0.0f) {
                    if (ImGui::GetIO().KeyShift) {
                        roll.setNoteRoll(pitch, s, roll.noteRoll(pitch, s) + (wheel > 0.0f ? 1 : -1));
                    } else if (ImGui::GetIO().KeyCtrl) {
                        roll.setNoteFineTune(pitch, s, roll.noteFineTune(pitch, s) + wheel * 5.0f);
                    } else {
                        roll.setNoteProbability(pitch, s, roll.noteProbability(pitch, s) + wheel * 0.1f);
                    }
                }
                const float pr = roll.noteProbability(pitch, s);
                const float ft = roll.noteFineTune(pitch, s);
                const int rl = roll.noteRoll(pitch, s);
                if (rl > 1) {
                    ImGui::SetTooltip("roll x%d", rl);
                } else if (ft != 0.0f) {
                    ImGui::SetTooltip("fine %+.0f cents", ft);
                } else if (pr < 0.999f) {
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
    const char* arpModes[] = {"Up", "Down", "Up-Down", "Random", "As-played", "Chord"};
    ImGui::SetNextItemWidth(120.0f);
    arpCh |= ImGui::Combo("##arpmode", &arpMode, arpModes, 6);
    if (arpCh) {
        seq.setArp(arp, arpMode);
    }
    ImGui::SameLine();
    int arpOct = seq.arpOctaves();
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::SliderInt("oct##arp", &arpOct, 1, 4)) {
        seq.setArpOctaves(arpOct);
    }
    ImGui::SameLine();
    float arpGate = seq.arpGate();
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::SliderFloat("gate##arp", &arpGate, 0.05f, 1.0f, "%.2f")) {
        seq.setArpGate(arpGate);
    }
    ImGui::SameLine();
    int arpRate = seq.arpRate();
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::SliderInt("rate##arp", &arpRate, 1, 8, "x%d")) {
        seq.setArpRate(arpRate);
    }
    ImGui::Separator();

    int mode = static_cast<int>(syn.mode());
    const char* modes[] = {"Subtractive", "FM", "Wavetable", "Pluck", "Organ", "Phase Dist"};
    if (ImGui::Combo("Engine", &mode, modes, IM_ARRAYSIZE(modes))) {
        syn.setMode(static_cast<audio::SynthMode>(mode));
    }
    if (syn.mode() == audio::SynthMode::PhaseDistortion) {
        float pd = syn.pdAmount();
        if (ImGui::SliderFloat("Amount##pd", &pd, 0.0f, 1.0f, "%.2f")) syn.setPdAmount(pd);
    }
    if (syn.mode() == audio::SynthMode::Pluck) {
        float pd = syn.pluckDamping();
        if (ImGui::SliderFloat("Damping##pluck", &pd, 0.0f, 1.0f, "%.2f"))
            syn.setPluckDamping(pd);
        float pp = syn.pluckPosition();
        if (ImGui::SliderFloat("Position##pluck", &pp, 0.0f, 0.99f, "%.2f"))
            syn.setPluckPosition(pp);
    }
    if (syn.mode() == audio::SynthMode::Organ) {
        // A row of 8 drawbar sliders (harmonics 1..8).
        for (int b = 0; b < audio::SynthInstrument::kOrganBars; ++b) {
            if (b > 0) ImGui::SameLine();
            float lvl = syn.organBar(b);
            ImGui::PushID(b);
            if (ImGui::VSliderFloat("##organbar", ImVec2(18, 60), &lvl, 0.0f, 1.0f, ""))
                syn.setOrganBar(b, lvl);
            ImGui::PopID();
        }
        float perc = syn.organPercussion();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("Percussion##organ", &perc, 0.0f, 1.0f, "%.2f"))
            syn.setOrganPercussion(perc);
        ImGui::SameLine();
        bool third = syn.organPercThird();
        if (ImGui::Checkbox("3rd##organperc", &third)) syn.setOrganPercThird(third);
    }
    if (syn.mode() == audio::SynthMode::Subtractive) {
        int w = static_cast<int>(syn.waveform());
        const char* waves[] = {"Sine", "Square", "Saw", "Triangle", "Trap", "Step"};
        if (ImGui::Combo("Waveform", &w, waves, 6)) {
            syn.setWaveform(static_cast<audio::Waveform>(w));
        }
        float detune = syn.detuneCents();
        float osc2 = syn.osc2Level();
        float sub = syn.subLevel();
        float noise = syn.noiseLevel();
        bool och = false;
        och |= ImGui::SliderFloat("Detune (cents)", &detune, 0.0f, 50.0f, "%.1f");
        och |= ImGui::SliderFloat("Osc 2", &osc2, 0.0f, 1.0f, "%.2f");
        float osc2semi = syn.osc2Semitones();
        if (ImGui::SliderFloat("Osc 2 coarse", &osc2semi, -24.0f, 24.0f, "%.0f st"))
            syn.setOsc2Semitones(osc2semi);
        int osc2w = static_cast<int>(syn.osc2Waveform());
        const char* osc2Waves[] = {"Sine", "Square", "Saw", "Triangle", "Trap", "Step"};
        if (ImGui::Combo("Osc 2 wave", &osc2w, osc2Waves, 6))
            syn.setOsc2Waveform(static_cast<audio::Waveform>(osc2w));
        float ring = syn.ringMod();
        if (ImGui::SliderFloat("Ring mod", &ring, 0.0f, 1.0f, "%.2f")) syn.setRingMod(ring);
        float osc3 = syn.osc3Level();
        if (ImGui::SliderFloat("Osc 3", &osc3, 0.0f, 1.0f, "%.2f")) syn.setOsc3Level(osc3);
        float osc3semi = syn.osc3Semitones();
        if (ImGui::SliderFloat("Osc 3 coarse", &osc3semi, -24.0f, 24.0f, "%.0f st"))
            syn.setOsc3Semitones(osc3semi);
        int osc3w = static_cast<int>(syn.osc3Waveform());
        const char* osc3Waves[] = {"Sine", "Square", "Saw", "Triangle", "Trap", "Step"};
        if (ImGui::Combo("Osc 3 wave", &osc3w, osc3Waves, 6))
            syn.setOsc3Waveform(static_cast<audio::Waveform>(osc3w));
        float osc3fine = syn.osc3FineTune();
        if (ImGui::SliderFloat("Osc 3 fine", &osc3fine, -100.0f, 100.0f, "%.0f ct"))
            syn.setOsc3FineTune(osc3fine);
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
        const char* subWaves[] = {"Sine", "Square", "Saw", "Triangle", "Trap", "Step"};
        if (ImGui::Combo("Sub wave", &subw, subWaves, 6))
            syn.setSubWaveform(static_cast<audio::Waveform>(subw));
        int subOct = syn.subOctave();
        const char* subOcts[] = {"-1 oct", "-2 oct"};
        int subOctIdx = subOct == 2 ? 1 : 0;
        if (ImGui::Combo("Sub octave", &subOctIdx, subOcts, 2))
            syn.setSubOctave(subOctIdx == 1 ? 2 : 1);
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
        float pwmRate = syn.pwmLfoRate();
        float pwmDepth = syn.pwmLfoDepth();
        bool pwmCh = ImGui::SliderFloat("PWM LFO Hz", &pwmRate, 0.0f, 12.0f, "%.2f");
        pwmCh |= ImGui::SliderFloat("PWM LFO depth", &pwmDepth, 0.0f, 0.48f, "%.2f");
        if (pwmCh) syn.setPwmLfo(pwmRate, pwmDepth);
    } else if (syn.mode() == audio::SynthMode::FM) {
        float ratio = syn.fmRatio();
        if (ImGui::SliderFloat("FM Ratio", &ratio, 0.5f, 8.0f, "%.2f")) syn.setFmRatio(ratio);
        float index = syn.fmIndex();
        if (ImGui::SliderFloat("FM Index", &index, 0.0f, 10.0f, "%.2f")) syn.setFmIndex(index);
        float fb = syn.fmFeedback();
        if (ImGui::SliderFloat("FM Feedback", &fb, 0.0f, 1.0f, "%.2f")) syn.setFmFeedback(fb);
        float velFm = syn.velToFmIndex();
        if (ImGui::SliderFloat("Vel>FM Index", &velFm, 0.0f, 10.0f, "%.2f")) syn.setVelToFmIndex(velFm);
    } else {
        float pos = syn.wavetablePosition();
        if (ImGui::SliderFloat("WT Position", &pos, 0.0f, 1.0f, "%.2f"))
            syn.setWavetablePosition(pos);
        float morph = syn.wavetableMorph();
        if (ImGui::SliderFloat("WT Env Morph", &morph, 0.0f, 1.0f, "%.2f"))
            syn.setWavetableMorph(morph);
        float velWt = syn.velToWavePosition();
        if (ImGui::SliderFloat("Vel>WT Pos", &velWt, 0.0f, 1.0f, "%.2f"))
            syn.setVelToWavePosition(velWt);
        float wtLfoRate = syn.wavetableLfoRate();
        float wtLfoDepth = syn.wavetableLfoDepth();
        bool wtLfoCh = ImGui::SliderFloat("WT LFO Hz", &wtLfoRate, 0.0f, 20.0f, "%.2f");
        wtLfoCh |= ImGui::SliderFloat("WT LFO Depth", &wtLfoDepth, 0.0f, 1.0f, "%.2f");
        if (wtLfoCh) {
            syn.setWavetableLfo(wtLfoRate, wtLfoDepth);
        }
        // Four morph-frame selectors (frame 0 → 3 as the position sweeps).
        const char* waves[] = {"Sine", "Square", "Saw", "Triangle", "Trap", "Step"};
        int fr[4];
        bool frCh = false;
        for (int k = 0; k < 4; ++k) {
            fr[k] = static_cast<int>(syn.wavetableFrame(k));
            ImGui::PushID(k);
            ImGui::SetNextItemWidth(90.0f);
            frCh |= ImGui::Combo("##wtframe", &fr[k], waves, 6);
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

    int octave = syn.octave();
    if (ImGui::SliderInt("Octave", &octave, -2, 2)) syn.setOctave(octave);
    bool synMono = syn.mono();
    if (ImGui::Checkbox("Mono", &synMono)) syn.setMono(synMono);
    float drift = syn.drift();
    if (ImGui::SliderFloat("Analog drift", &drift, 0.0f, 50.0f, "%.1f cents")) syn.setDrift(drift);
    float phaseRand = syn.startPhaseRandom();
    if (ImGui::SliderFloat("Phase random", &phaseRand, 0.0f, 1.0f, "%.2f"))
        syn.setStartPhaseRandom(phaseRand);

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
    float velSens = syn.velSensitivity();
    if (ImGui::SliderFloat("Vel->Amp", &velSens, 0.0f, 1.0f, "%.2f")) syn.setVelSensitivity(velSens);
    float glide = syn.glide();
    if (ImGui::SliderFloat("Glide (portamento)", &glide, 0.0f, 1.0f, "%.3f s")) syn.setGlide(glide);
    bool glideLegato = syn.glideLegato();
    if (ImGui::Checkbox("Glide legato-only", &glideLegato)) syn.setGlideLegato(glideLegato);
    float vibRate = syn.vibratoRate();
    float vibDepth = syn.vibratoDepth();
    bool vch = ImGui::SliderFloat("Vibrato rate", &vibRate, 0.0f, 12.0f, "%.1f Hz");
    vch |= ImGui::SliderFloat("Vibrato depth", &vibDepth, 0.0f, 100.0f, "%.0f cents");
    if (vch) {
        syn.setVibrato(vibRate, vibDepth);
    }
    float vibDelay = syn.vibratoDelay();
    if (ImGui::SliderFloat("Vibrato delay", &vibDelay, 0.0f, 2.0f, "%.2f s"))
        syn.setVibratoDelay(vibDelay);
    int viShape = static_cast<int>(syn.vibratoShape());
    const char* viShapes[] = {"Sine", "Square", "Saw", "Triangle", "Trap", "Step"};
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::Combo("Vib shape##vi", &viShape, viShapes, 6))
        syn.setVibratoShape(static_cast<audio::Waveform>(viShape));
    ImGui::SameLine();
    bool viSH = syn.vibratoSampleHold();
    if (ImGui::Checkbox("S&H##vi", &viSH)) syn.setVibratoSampleHold(viSH); // random stepped pitch
    bool viSync = syn.vibratoSync();
    if (ImGui::Checkbox("Vibrato sync", &viSync)) syn.setVibratoSync(viSync);
    ImGui::SameLine();
    int viDiv = syn.vibratoSyncDivision();
    const char* viDivs[] = {"1/1", "1/2", "1/4", "1/8", "1/8T", "1/16"};
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::Combo("##vidiv", &viDiv, viDivs, 6)) syn.setVibratoSyncDivision(viDiv);
    float peAmt = syn.pitchEnvAmount();
    float peTime = syn.pitchEnvTime();
    bool pech = ImGui::SliderFloat("Pitch env", &peAmt, -24.0f, 24.0f, "%.0f st");
    pech |= ImGui::SliderFloat("Pitch env time", &peTime, 0.001f, 0.5f, "%.3f s");
    if (pech) {
        syn.setPitchEnv(peAmt, peTime);
    }
    float naAmt = syn.noiseAttackAmount();
    float naDecay = syn.noiseAttackDecay();
    bool nach = ImGui::SliderFloat("Noise attack", &naAmt, 0.0f, 1.0f, naAmt <= 0.0f ? "off" : "%.2f");
    nach |= ImGui::SliderFloat("Noise atk decay", &naDecay, 1.0f, 200.0f, "%.0f ms");
    if (nach) {
        syn.setNoiseAttack(naAmt, naDecay);
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
    int fmode = static_cast<int>(syn.filterMode());
    const char* fmodes[] = {"Low-pass", "High-pass", "Band-pass", "Notch"};
    if (ImGui::Combo("Filter type", &fmode, fmodes, 4))
        syn.setFilterMode(static_cast<audio::StateVariableFilter::Mode>(fmode));
    ImGui::SameLine();
    bool f24 = syn.filterSlope() >= 24;
    if (ImGui::Checkbox("24 dB##fslope", &f24)) syn.setFilterSlope(f24 ? 24 : 12);
    float fenvDepth = syn.filterEnvDepth();
    if (ImGui::SliderFloat("Filter env depth", &fenvDepth, -12000.0f, 12000.0f, "%.0f Hz"))
        syn.setFilterEnvDepth(fenvDepth);
    float fa = syn.filterEnvAttack(), fd = syn.filterEnvDecay(), fs = syn.filterEnvSustain(),
          fr2 = syn.filterEnvRelease();
    bool fenvCh = false;
    fenvCh |= ImGui::SliderFloat("F.Env A", &fa, 0.001f, 1.0f, "%.3f s");
    fenvCh |= ImGui::SliderFloat("F.Env D", &fd, 0.001f, 1.0f, "%.3f s");
    fenvCh |= ImGui::SliderFloat("F.Env S", &fs, 0.0f, 1.0f, "%.2f");
    fenvCh |= ImGui::SliderFloat("F.Env R", &fr2, 0.001f, 2.0f, "%.3f s");
    if (fenvCh) syn.setFilterEnvelope(fa, fd, fs, fr2);
    float velCut = syn.velToCutoff();
    if (ImGui::SliderFloat("Vel->Cutoff", &velCut, 0.0f, 15000.0f, "%.0f Hz"))
        syn.setVelToCutoff(velCut);
    float velAtk = syn.velToAttack();
    if (ImGui::SliderFloat("Vel->Attack", &velAtk, 0.0f, 1.0f, velAtk <= 0.0f ? "off" : "%.2f"))
        syn.setVelToAttack(velAtk);
    float keyTrack = syn.filterKeyTrack();
    if (ImGui::SliderFloat("Key track", &keyTrack, 0.0f, 1.0f, "%.2f"))
        syn.setFilterKeyTrack(keyTrack);
    float fDrive = syn.filterDrive();
    if (ImGui::SliderFloat("Filter drive", &fDrive, 0.0f, 1.0f, "%.2f"))
        syn.setFilterDrive(fDrive);
    float fLfoRate = syn.filterLfoRate();
    float fLfoDepth = syn.filterLfoDepth();
    bool flch = ImGui::SliderFloat("Cutoff LFO Hz", &fLfoRate, 0.0f, 20.0f, "%.2f");
    flch |= ImGui::SliderFloat("Cutoff LFO oct", &fLfoDepth, 0.0f, 4.0f, "%.2f");
    if (flch) syn.setFilterLfo(fLfoRate, fLfoDepth);
    bool flSync = syn.filterLfoSync();
    if (ImGui::Checkbox("Cutoff LFO sync", &flSync)) syn.setFilterLfoSync(flSync);
    ImGui::SameLine();
    int flDiv = syn.filterLfoSyncDivision();
    const char* flDivs[] = {"1/1", "1/2", "1/4", "1/8", "1/8T", "1/16"};
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::Combo("##fldiv", &flDiv, flDivs, 6)) syn.setFilterLfoSyncDivision(flDiv);
    int flShape = static_cast<int>(syn.filterLfoShape());
    const char* flShapes[] = {"Sine", "Square", "Saw", "Triangle", "Trap", "Step"};
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::Combo("LFO shape##fl", &flShape, flShapes, 6))
        syn.setFilterLfoShape(static_cast<audio::Waveform>(flShape));
    ImGui::SameLine();
    bool flSH = syn.filterLfoSampleHold();
    if (ImGui::Checkbox("S&H##fl", &flSH)) syn.setFilterLfoSampleHold(flSH); // random stepped cutoff
    float aLfoRate = syn.ampLfoRate(), aLfoDepth = syn.ampLfoDepth();
    bool alch = ImGui::SliderFloat("Tremolo Hz", &aLfoRate, 0.0f, 20.0f, "%.2f");
    alch |= ImGui::SliderFloat("Tremolo depth", &aLfoDepth, 0.0f, 1.0f, "%.2f");
    if (alch) syn.setAmpLfo(aLfoRate, aLfoDepth);
    bool alSync = syn.ampLfoSync();
    if (ImGui::Checkbox("Tremolo sync", &alSync)) syn.setAmpLfoSync(alSync);
    ImGui::SameLine();
    int alDiv = syn.ampLfoSyncDivision();
    const char* alDivs[] = {"1/1", "1/2", "1/4", "1/8", "1/8T", "1/16"};
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::Combo("##aldiv", &alDiv, alDivs, 6)) syn.setAmpLfoSyncDivision(alDiv);
    int alShape = static_cast<int>(syn.ampLfoShape());
    const char* alShapes[] = {"Sine", "Square", "Saw", "Triangle", "Trap", "Step"};
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::Combo("Trem shape##al", &alShape, alShapes, 6))
        syn.setAmpLfoShape(static_cast<audio::Waveform>(alShape));
    ImGui::SameLine();
    bool alSH = syn.ampLfoSampleHold();
    if (ImGui::Checkbox("S&H##al", &alSH)) syn.setAmpLfoSampleHold(alSH); // random stepped tremolo

    ImGui::SeparatorText("Sampler");
    bool useSampler = seq.useSampler();
    if (ImGui::Checkbox("Use sampler for melody", &useSampler)) {
        seq.setUseSampler(useSampler);
    }
    ImGui::Text("loaded: %s",
                seq.sampler().loaded() ? seq.sampler().path().c_str() : "(none)");
    if (seq.sampler().loaded()) {
        ImGui::Text("peak: %.3f", seq.sampler().samplePeak());
        ImGui::SameLine();
        if (ImGui::Button("Normalize")) seq.sampler().normalize();
        ImGui::SameLine();
        static float fadeMs = 5.0f;
        ImGui::SetNextItemWidth(70.0f);
        ImGui::InputFloat("##fadems", &fadeMs, 0.0f, 0.0f, "%.1f");
        ImGui::SameLine();
        if (ImGui::Button("Fade edges")) seq.sampler().fadeEdges(fadeMs);
        ImGui::SameLine();
        static float xfadeMs = 10.0f;
        ImGui::SetNextItemWidth(70.0f);
        ImGui::InputFloat("##xfadems", &xfadeMs, 0.0f, 0.0f, "%.1f");
        ImGui::SameLine();
        if (ImGui::Button("Loop xfade")) seq.sampler().crossfadeLoop(xfadeMs);
    }
    bool rev = seq.sampler().reverse();
    if (ImGui::Checkbox("Reverse", &rev)) seq.sampler().setReverse(rev);
    ImGui::SameLine();
    bool lp = seq.sampler().loop();
    if (ImGui::Checkbox("Loop", &lp)) seq.sampler().setLoop(lp);
    ImGui::SameLine();
    bool pp = seq.sampler().pingPong();
    if (ImGui::Checkbox("Ping-pong", &pp)) seq.sampler().setPingPong(pp);
    ImGui::SameLine();
    bool smono = seq.sampler().mono();
    if (ImGui::Checkbox("Mono", &smono)) seq.sampler().setMono(smono);
    ImGui::SameLine();
    bool skey = seq.sampler().keyTrack();
    if (ImGui::Checkbox("Key track", &skey)) seq.sampler().setKeyTrack(skey);
    ImGui::SameLine();
    float startOff = seq.sampler().startOffset();
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::SliderFloat("Start", &startOff, 0.0f, 0.99f, "%.2f"))
        seq.sampler().setStartOffset(startOff);
    float sdrive = seq.sampler().drive();
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::SliderFloat("Drive##smp", &sdrive, 0.0f, 1.0f, sdrive <= 0.0f ? "clean" : "%.2f"))
        seq.sampler().setDrive(sdrive);
    ImGui::SameLine();
    float sglide = seq.sampler().glide();
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::SliderFloat("Glide##smp", &sglide, 0.0f, 1.0f, sglide <= 0.0f ? "off" : "%.2fs"))
        seq.sampler().setGlide(sglide);
    ImGui::SameLine();
    bool sglideleg = seq.sampler().glideLegato();
    if (ImGui::Checkbox("Legato##smp", &sglideleg)) seq.sampler().setGlideLegato(sglideleg);
    float svelatk = seq.sampler().velToAttack();
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::SliderFloat("Vel->Atk##smp", &svelatk, 0.0f, 1.0f, svelatk <= 0.0f ? "off" : "%.2f"))
        seq.sampler().setVelToAttack(svelatk);
    ImGui::SameLine();
    float svelstart = seq.sampler().velToStart();
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::SliderFloat("Vel->Start##smp", &svelstart, 0.0f, 1.0f, svelstart <= 0.0f ? "off" : "%.2f"))
        seq.sampler().setVelToStart(svelstart);
    float loopS = seq.sampler().loopStart(), loopE = seq.sampler().loopEnd();
    bool loopCh = false;
    ImGui::SetNextItemWidth(110.0f);
    loopCh |= ImGui::SliderFloat("Loop start", &loopS, 0.0f, 0.99f, "%.2f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    loopCh |= ImGui::SliderFloat("Loop end", &loopE, 0.01f, 1.0f, "%.2f");
    if (loopCh) seq.sampler().setLoopRegion(loopS, loopE);
    int slices = seq.sampler().slices();
    ImGui::SetNextItemWidth(140.0f);
    if (ImGui::SliderInt("Slices", &slices, 1, 32, slices == 1 ? "off" : "%d"))
        seq.sampler().setSlices(slices);
    float smpAtk = seq.sampler().attack();
    float smpDec = seq.sampler().ampDecay();
    float smpSus = seq.sampler().ampSustain();
    float smpRel = seq.sampler().release();
    bool smpEnvCh = false;
    ImGui::SetNextItemWidth(110.0f);
    smpEnvCh |= ImGui::SliderFloat("Atk##smp", &smpAtk, 0.001f, 0.5f, "%.3f s");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    smpEnvCh |= ImGui::SliderFloat("Dec##smp", &smpDec, 0.001f, 1.0f, "%.3f s");
    ImGui::SetNextItemWidth(110.0f);
    smpEnvCh |= ImGui::SliderFloat("Sus##smp", &smpSus, 0.0f, 1.0f, "%.2f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    smpEnvCh |= ImGui::SliderFloat("Rel##smp", &smpRel, 0.001f, 1.0f, "%.3f s");
    if (smpEnvCh) {
        seq.sampler().setAmpEnv(smpAtk, smpRel);
        seq.sampler().setAmpDecay(smpDec);
        seq.sampler().setAmpSustain(smpSus);
    }
    float smpVel = seq.sampler().velSensitivity();
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::SliderFloat("Vel→vol##smp", &smpVel, 0.0f, 1.0f, "%.2f"))
        seq.sampler().setVelSensitivity(smpVel);
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
    float smpDetune = seq.sampler().detuneCents();
    if (ImGui::SliderFloat("Fine tune", &smpDetune, -1200.0f, 1200.0f, "%.0f c"))
        seq.sampler().setDetuneCents(smpDetune);
    float smpCut = seq.sampler().filterCutoff();
    float smpRes = seq.sampler().filterResonance();
    bool sfch = ImGui::SliderFloat("Filter cutoff##smp", &smpCut, 20.0f, 20000.0f, "%.0f Hz");
    sfch |= ImGui::SliderFloat("Filter reso##smp", &smpRes, 0.5f, 20.0f, "%.1f");
    if (sfch) seq.sampler().setFilter(smpCut, smpRes);
    float smpFeDepth = seq.sampler().filterEnvDepth();
    if (ImGui::SliderFloat("F.Env depth##smp", &smpFeDepth, -12000.0f, 12000.0f, "%.0f Hz"))
        seq.sampler().setFilterEnvDepth(smpFeDepth);
    float smpFVelo = seq.sampler().filterVelo();
    if (ImGui::SliderFloat("F.Vel->cutoff##smp", &smpFVelo, 0.0f, 12000.0f, "%.0f Hz"))
        seq.sampler().setFilterVelo(smpFVelo);
    float smpFkt = seq.sampler().filterKeyTrack();
    if (ImGui::SliderFloat("F.Key track##smp", &smpFkt, 0.0f, 1.0f, smpFkt <= 0.0f ? "off" : "%.2f"))
        seq.sampler().setFilterKeyTrack(smpFkt);
    float sfa = seq.sampler().filterEnvAttack(), sfd = seq.sampler().filterEnvDecay();
    float sfs = seq.sampler().filterEnvSustain(), sfr = seq.sampler().filterEnvRelease();
    bool fech = ImGui::SliderFloat("F.Env A##smp", &sfa, 0.0001f, 2.0f, "%.3f");
    fech |= ImGui::SliderFloat("F.Env D##smp", &sfd, 0.0001f, 2.0f, "%.3f");
    fech |= ImGui::SliderFloat("F.Env S##smp", &sfs, 0.0f, 1.0f, "%.2f");
    fech |= ImGui::SliderFloat("F.Env R##smp", &sfr, 0.0001f, 2.0f, "%.3f");
    if (fech) seq.sampler().setFilterEnvelope(sfa, sfd, sfs, sfr);
    float speDepth = seq.sampler().pitchEnvDepth(), speTime = seq.sampler().pitchEnvTime();
    bool spech = ImGui::SliderFloat("P.Env depth##smp", &speDepth, -36.0f, 36.0f, "%.1f st");
    spech |= ImGui::SliderFloat("P.Env time##smp", &speTime, 0.001f, 2.0f, "%.3f s");
    if (spech) seq.sampler().setPitchEnv(speDepth, speTime);

    ImGui::End();
}

// Draw a compact panel for the second (bass) instrument.
void buildBassUI(audio::SynthInstrument& syn) {
    ImGui::Begin("CJC Music Station — Bass Synth");
    int w = static_cast<int>(syn.waveform());
    const char* waves[] = {"Sine", "Square", "Saw", "Triangle", "Trap", "Step"};
    if (ImGui::Combo("Waveform##bass", &w, waves, 6)) {
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
    float bdrift = syn.drift();
    if (ImGui::SliderFloat("Drift##bass", &bdrift, 0.0f, 50.0f, "%.1f cents")) syn.setDrift(bdrift);
    float cutoff = syn.filterCutoff(), reso = syn.filterResonance(), env = syn.filterEnvAmount();
    bool fch = false;
    fch |= ImGui::SliderFloat("Cutoff##bass", &cutoff, 20.0f, 20000.0f, "%.0f Hz", ImGuiSliderFlags_Logarithmic);
    fch |= ImGui::SliderFloat("Resonance##bass", &reso, 0.5f, 20.0f, "%.1f");
    if (fch) {
        syn.setFilter(cutoff, reso, env);
    }
    float bfLfoRate = syn.filterLfoRate(), bfLfoDepth = syn.filterLfoDepth();
    bool bflch = ImGui::SliderFloat("Cutoff LFO Hz##bass", &bfLfoRate, 0.0f, 20.0f, "%.2f");
    bflch |= ImGui::SliderFloat("Cutoff LFO oct##bass", &bfLfoDepth, 0.0f, 4.0f, "%.2f");
    if (bflch) syn.setFilterLfo(bfLfoRate, bfLfoDepth);
    int bflShape = static_cast<int>(syn.filterLfoShape());
    const char* bflShapes[] = {"Sine", "Square", "Saw", "Triangle", "Trap", "Step"};
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::Combo("LFO shape##bassfl", &bflShape, bflShapes, 6))
        syn.setFilterLfoShape(static_cast<audio::Waveform>(bflShape));
    float baLfoRate = syn.ampLfoRate(), baLfoDepth = syn.ampLfoDepth();
    bool balch = ImGui::SliderFloat("Tremolo Hz##bass", &baLfoRate, 0.0f, 20.0f, "%.2f");
    balch |= ImGui::SliderFloat("Tremolo depth##bass", &baLfoDepth, 0.0f, 1.0f, "%.2f");
    if (balch) syn.setAmpLfo(baLfoRate, baLfoDepth);
    int balShape = static_cast<int>(syn.ampLfoShape());
    const char* balShapes[] = {"Sine", "Square", "Saw", "Triangle", "Trap", "Step"};
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::Combo("Trem shape##bassal", &balShape, balShapes, 6))
        syn.setAmpLfoShape(static_cast<audio::Waveform>(balShape));
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
    // Master output level meter (peak bar + RMS readout).
    {
        const float peak = engine.masterPeak();
        ImGui::ProgressBar(peak > 1.0f ? 1.0f : peak, ImVec2(-1.0f, 0.0f));
        ImGui::Text("out: peak %.2f  rms %.2f%s", peak, engine.masterRms(),
                    peak >= 0.999f ? "  CLIP" : "");
    }
    float ceiling = mx.limiterCeiling();
    if (ImGui::SliderFloat("Ceiling", &ceiling, 0.1f, 1.0f, "%.2f")) {
        mx.setLimiterCeiling(ceiling);
    }
    float balance = mx.masterBalance();
    if (ImGui::SliderFloat("Balance", &balance, -1.0f, 1.0f, "%.2f")) {
        mx.setMasterBalance(balance);
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
    float scAtk = seq.sidechainAttackMs();
    bool scChanged = ImGui::Checkbox("Sidechain", &sc);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    scChanged |= ImGui::SliderFloat("amt##sc", &scAmt, 0.0f, 1.0f, "%.2f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    scChanged |= ImGui::SliderFloat("atk ms##sc", &scAtk, 0.0f, 200.0f, "%.0f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    scChanged |= ImGui::SliderFloat("rel ms##sc", &scRel, 20.0f, 500.0f, "%.0f");
    if (scChanged) {
        seq.setSidechain(sc, scAmt, scRel, scAtk);
    }
    ImGui::SameLine();
    int scSrc = seq.sidechainSource();
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::SliderInt("src##sc", &scSrc, 0, seq.numChannels() - 1,
                         seq.channelName(scSrc < seq.numChannels() ? scSrc : 0).c_str())) {
        seq.setSidechainSource(scSrc);
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
        float mid2Db = mx.peq().mid2Gain();
        float mid2F = mx.peq().mid2Freq();
        float mid2Q = mx.peq().mid2Q();
        float mid3Db = mx.peq().mid3Gain();
        float mid3F = mx.peq().mid3Freq();
        float mid3Q = mx.peq().mid3Q();
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
        ImGui::SetNextItemWidth(90.0f);
        ch |= ImGui::SliderFloat("Mid2 dB##peq", &mid2Db, -18.0f, 18.0f, "%.1f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ch |= ImGui::SliderFloat("Mid2 Hz##peq", &mid2F, 200.0f, 12000.0f, "%.0f");
        ImGui::SetNextItemWidth(90.0f);
        ch |= ImGui::SliderFloat("Mid3 dB##peq", &mid3Db, -18.0f, 18.0f, "%.1f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ch |= ImGui::SliderFloat("Mid3 Hz##peq", &mid3F, 200.0f, 16000.0f, "%.0f");
        if (ch) {
            mx.peq().setLowGain(lowDb);
            mx.peq().setMid(midF, midQ, midDb);
            mx.peq().setMid2(mid2F, mid2Q, mid2Db);
            mx.peq().setMid3(mid3F, mid3Q, mid3Db);
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
        float tpivot = mx.tilt().pivot();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("pivot##tilt", &tpivot, 100.0f, 8000.0f, "%.0f Hz")) mx.tilt().setPivot(tpivot);
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
        bool en = mx.filter().enabled();
        if (ImGui::Checkbox("Filter", &en)) mx.filter().setEnabled(en);
        int fmode = static_cast<int>(mx.filter().mode());
        const char* fmodes[] = {"LP", "HP", "BP", "Notch"};
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        if (ImGui::Combo("mode##mf", &fmode, fmodes, IM_ARRAYSIZE(fmodes)))
            mx.filter().setMode(static_cast<audio::StateVariableFilter::Mode>(fmode));
        float fcut = mx.filter().cutoff();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("cutoff##mf", &fcut, 20.0f, 20000.0f, "%.0f Hz"))
            mx.filter().setCutoff(fcut);
        float freso = mx.filter().resonance();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("reso##mf", &freso, 0.5f, 20.0f, "%.1f"))
            mx.filter().setResonance(freso);
        float fdrive = mx.filter().drive();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("drive##mf", &fdrive, 0.0f, 1.0f, fdrive <= 0.0f ? "clean" : "%.2f"))
            mx.filter().setDrive(fdrive);
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
        if (ImGui::Combo("##distcurve", &curve, "Soft\0Hard\0Fold\0SineFold\0Tube\0\0"))
            mx.distortion().setCurve(static_cast<audio::Distortion::Curve>(curve));
        float dtone = mx.distortion().tone();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("tone##dist", &dtone, 200.0f, 20000.0f, "%.0f Hz")) mx.distortion().setTone(dtone);
        float dout = mx.distortion().outputDb();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("out##dist", &dout, -24.0f, 24.0f, "%.1f dB")) mx.distortion().setOutputDb(dout);
        float dbias = mx.distortion().bias();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("bias##dist", &dbias, -1.0f, 1.0f, dbias == 0.0f ? "symmetric" : "%.2f"))
            mx.distortion().setBias(dbias);
    }
    {
        auto& ac = mx.ampCab();
        bool en = ac.enabled();
        if (ImGui::Checkbox("Amp/Cab", &en)) ac.setEnabled(en);
        float drive = ac.drive();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::SliderFloat("drive##amp", &drive, 0.0f, 1.0f, "%.2f")) ac.setDrive(drive);
        float pres = ac.presence();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::SliderFloat("presence##amp", &pres, 0.0f, 1.0f, "%.2f")) ac.setPresence(pres);
        float tone = ac.tone();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("tone##amp", &tone, 1500.0f, 8000.0f, "%.0f Hz")) ac.setTone(tone);
        float amix = ac.mix();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::SliderFloat("mix##amp", &amix, 0.0f, 1.0f, "%.2f")) ac.setMix(amix);
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
        float wf = mx.tape().wowFlutter();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("wow/flut##tape", &wf, 0.0f, 1.0f, "%.2f")) mx.tape().setWowFlutter(wf);
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
        const char* ringCarriers[] = {"Sine", "Square", "Saw", "Triangle"};
        int rc = static_cast<int>(mx.ringmod().carrier());
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::Combo("carrier##ring", &rc, ringCarriers, IM_ARRAYSIZE(ringCarriers)))
            mx.ringmod().setCarrier(static_cast<audio::RingMod::Carrier>(rc));
    }
    {
        bool en = mx.pitchShifter().enabled();
        if (ImGui::Checkbox("Pitch Shifter", &en)) mx.pitchShifter().setEnabled(en);
        float st = mx.pitchShifter().semitones();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("semis##pitch", &st, -24.0f, 24.0f, "%.0f st"))
            mx.pitchShifter().setSemitones(st);
        float pwet = mx.pitchShifter().mix();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("mix##pitch", &pwet, 0.0f, 1.0f, "%.2f")) mx.pitchShifter().setMix(pwet);
        float pfb = mx.pitchShifter().feedback();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("fb##pitch", &pfb, 0.0f, 0.95f, pfb <= 0.0f ? "off" : "%.2f"))
            mx.pitchShifter().setFeedback(pfb);
    }
    {
        bool en = mx.freqShifter().enabled();
        if (ImGui::Checkbox("Freq Shifter", &en)) mx.freqShifter().setEnabled(en);
        float hz = mx.freqShifter().shiftHz();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(130.0f);
        if (ImGui::SliderFloat("Hz##fshift", &hz, -2000.0f, 2000.0f, "%.0f Hz"))
            mx.freqShifter().setShiftHz(hz);
        float fwet = mx.freqShifter().mix();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("mix##fshift", &fwet, 0.0f, 1.0f, "%.2f"))
            mx.freqShifter().setMix(fwet);
    }
    {
        bool en = mx.bitcrusher().enabled();
        if (ImGui::Checkbox("Bitcrusher", &en)) mx.bitcrusher().setEnabled(en);
        float bits = mx.bitcrusher().bits();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("bits##crush", &bits, 1.0f, 16.0f, "%.0f")) mx.bitcrusher().setBits(bits);
        float ctone = mx.bitcrusher().tone();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("tone##crush", &ctone, 200.0f, 20000.0f, "%.0f Hz"))
            mx.bitcrusher().setTone(ctone);
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
        float gsc = mx.gate().sidechainHpf();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("key HPF##gate", &gsc, 0.0f, 2000.0f, "%.0f Hz")) mx.gate().setSidechainHpf(gsc);
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
        float cmix = mx.compressor().mix();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("mix##cmp", &cmix, 0.0f, 1.0f, "%.2f")) mx.compressor().setMix(cmix);
        float scHpf = mx.compressor().sidechainHpf();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("SC HPF##cmp", &scHpf, 0.0f, 500.0f, "%.0f Hz"))
            mx.compressor().setSidechainHpf(scHpf);
        ImGui::SameLine();
        bool autoMk = mx.compressor().autoMakeup();
        if (ImGui::Checkbox("Auto MU##cmp", &autoMk)) mx.compressor().setAutoMakeup(autoMk);
        ImGui::SameLine();
        float la = mx.compressor().lookaheadMs();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("lookahead##cmp", &la, 0.0f, 10.0f, la <= 0.0f ? "no lookahead" : "%.1f ms"))
            mx.compressor().setLookaheadMs(la);
        ImGui::SameLine();
        bool rms = mx.compressor().rmsDetection();
        if (ImGui::Checkbox("RMS##cmp", &rms)) mx.compressor().setRmsDetection(rms);
        ImGui::SameLine();
        ImGui::Text("GR %.1f dB", mx.compressor().gainReductionDb());
    }
    {
        auto& mb = mx.multiband();
        bool en = mb.enabled();
        if (ImGui::Checkbox("Multiband", &en)) mb.setEnabled(en);
        float clo = mb.crossoverLow(), chi = mb.crossoverHigh();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::SliderFloat("lo/mid Hz##mb", &clo, 20.0f, 2000.0f, "%.0f")) mb.setCrossoverLow(clo);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::SliderFloat("mid/hi Hz##mb", &chi, 200.0f, 18000.0f, "%.0f")) mb.setCrossoverHigh(chi);
        const char* bandName[3] = {"low", "mid", "high"};
        for (int b = 0; b < audio::MultibandCompressor::kBands; ++b) {
            ImGui::PushID(b);
            float t = mb.bandThreshold(b), rr = mb.bandRatio(b);
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::SliderFloat("thr dB##mb", &t, -60.0f, 0.0f, "%.0f")) mb.setBandThreshold(b, t);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::SliderFloat("ratio##mb", &rr, 1.0f, 20.0f, "%.1f")) mb.setBandRatio(b, rr);
            ImGui::SameLine();
            ImGui::TextUnformatted(bandName[b]);
            ImGui::PopID();
        }
        float atk = mb.attackMs(), rel = mb.releaseMs();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("atk ms##mb", &atk, 0.1f, 200.0f, "%.1f")) mb.setAttackMs(atk);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("rel ms##mb", &rel, 1.0f, 1000.0f, "%.0f")) mb.setReleaseMs(rel);
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
        auto& mt = mx.multibandTransient();
        bool en = mt.enabled();
        if (ImGui::Checkbox("Multiband Transient", &en)) mt.setEnabled(en);
        float clo = mt.crossoverLow(), chi = mt.crossoverHigh();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::SliderFloat("lo/mid##mbt", &clo, 20.0f, 2000.0f, "%.0f")) mt.setCrossoverLow(clo);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::SliderFloat("mid/hi##mbt", &chi, 200.0f, 18000.0f, "%.0f")) mt.setCrossoverHigh(chi);
        const char* bandNm[3] = {"low", "mid", "high"};
        for (int b = 0; b < audio::MultibandTransientShaper::kBands; ++b) {
            ImGui::PushID(200 + b);
            float a = mt.attack(b);
            ImGui::SetNextItemWidth(110.0f);
            if (ImGui::SliderFloat((std::string("atk ") + bandNm[b]).c_str(), &a, -1.0f, 1.0f, "%.2f"))
                mt.setAttack(b, a);
            ImGui::SameLine();
            float s = mt.sustain(b);
            ImGui::SetNextItemWidth(110.0f);
            if (ImGui::SliderFloat((std::string("sus ") + bandNm[b]).c_str(), &s, -1.0f, 1.0f, "%.2f"))
                mt.setSustain(b, s);
            ImGui::PopID();
        }
    }
    {
        bool en = mx.deEsser().enabled();
        if (ImGui::Checkbox("De-Esser", &en)) mx.deEsser().setEnabled(en);
        float thr = mx.deEsser().thresholdDb();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::SliderFloat("dB##deess", &thr, -60.0f, 0.0f, "%.0f")) mx.deEsser().setThresholdDb(thr);
        float freq = mx.deEsser().frequency();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::SliderFloat("Hz##deess", &freq, 1000.0f, 16000.0f, "%.0f"))
            mx.deEsser().setFrequency(freq);
        float amt = mx.deEsser().amount();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::SliderFloat("amt##deess", &amt, 0.0f, 1.0f, "%.2f")) mx.deEsser().setAmount(amt);
    }
    {
        auto& dq = mx.dynamicEq();
        bool en = dq.enabled();
        if (ImGui::Checkbox("Dynamic EQ", &en)) dq.setEnabled(en);
        float freq = dq.frequency();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::SliderFloat("Hz##dyneq", &freq, 40.0f, 18000.0f, "%.0f")) dq.setFrequency(freq);
        float q = dq.q();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        if (ImGui::SliderFloat("Q##dyneq", &q, 0.3f, 10.0f, "%.1f")) dq.setQ(q);
        float thr = dq.thresholdDb();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::SliderFloat("thr dB##dyneq", &thr, -60.0f, 0.0f, "%.0f")) dq.setThresholdDb(thr);
        float range = dq.rangeDb();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("range dB##dyneq", &range, -24.0f, 24.0f, "%.1f")) dq.setRangeDb(range);
        float atk = dq.attackMs();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::SliderFloat("atk##dyneq", &atk, 0.1f, 200.0f, "%.1f ms")) dq.setAttackMs(atk);
        float rel = dq.releaseMs();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::SliderFloat("rel##dyneq", &rel, 1.0f, 1000.0f, "%.0f ms")) dq.setReleaseMs(rel);
    }
    {
        bool en = mx.chorus().enabled();
        if (ImGui::Checkbox("Chorus", &en)) mx.chorus().setEnabled(en);
        float wet = mx.chorus().mix();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("mix##cho", &wet, 0.0f, 1.0f, "%.2f")) mx.chorus().setMix(wet);
        float cfb = mx.chorus().feedback();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("fb##cho", &cfb, 0.0f, 0.9f, "%.2f")) mx.chorus().setFeedback(cfb);
        float cwidth = mx.chorus().width();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("width##cho", &cwidth, 0.0f, 2.0f, "%.2f")) mx.chorus().setWidth(cwidth);
        int cvoices = mx.chorus().voices();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderInt("voices##cho", &cvoices, 1, 3, cvoices == 1 ? "1 (single)" : "%d (ensemble)"))
            mx.chorus().setVoices(cvoices);
        const char* modDivs[audio::kModSyncDivisions];
        for (int d = 0; d < audio::kModSyncDivisions; ++d) modDivs[d] = audio::modSyncDivisionName(d);
        bool csync = mx.chorus().sync();
        ImGui::SameLine();
        if (ImGui::Checkbox("Sync##cho", &csync)) mx.chorus().setSync(csync);
        ImGui::SameLine();
        int cdiv = mx.chorus().syncDivision();
        ImGui::SetNextItemWidth(70.0f);
        if (ImGui::Combo("div##cho", &cdiv, modDivs, audio::kModSyncDivisions))
            mx.chorus().setSyncDivision(cdiv);
    }
    {
        bool en = mx.vibrato().enabled();
        if (ImGui::Checkbox("Vibrato", &en)) mx.vibrato().setEnabled(en);
        float vrate = mx.vibrato().rate();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("rate##vib", &vrate, 0.0f, 14.0f, "%.2f Hz"))
            mx.vibrato().setRate(vrate);
        float vdepth = mx.vibrato().depth();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("depth##vib", &vdepth, 0.0f, 20.0f, "%.1f ms"))
            mx.vibrato().setDepth(vdepth);
        const char* vModDivs[audio::kModSyncDivisions];
        for (int d = 0; d < audio::kModSyncDivisions; ++d) vModDivs[d] = audio::modSyncDivisionName(d);
        bool vsync = mx.vibrato().sync();
        ImGui::SameLine();
        if (ImGui::Checkbox("Sync##vib", &vsync)) mx.vibrato().setSync(vsync);
        ImGui::SameLine();
        int vdiv = mx.vibrato().syncDivision();
        ImGui::SetNextItemWidth(70.0f);
        if (ImGui::Combo("div##vib", &vdiv, vModDivs, audio::kModSyncDivisions))
            mx.vibrato().setSyncDivision(vdiv);
    }
    {
        bool en = mx.rotary().enabled();
        if (ImGui::Checkbox("Rotary", &en)) mx.rotary().setEnabled(en);
        float rrate = mx.rotary().rate();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("rate##rot", &rrate, 0.1f, 12.0f, "%.2f Hz"))
            mx.rotary().setRate(rrate);
        ImGui::SameLine();
        if (ImGui::SmallButton("slow##rot")) mx.rotary().setRate(0.8f);
        ImGui::SameLine();
        if (ImGui::SmallButton("fast##rot")) mx.rotary().setRate(6.5f);
        float rdepth = mx.rotary().depth();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("depth##rot", &rdepth, 0.0f, 1.0f, "%.2f"))
            mx.rotary().setDepth(rdepth);
        float rmix = mx.rotary().mix();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("mix##rot", &rmix, 0.0f, 1.0f, "%.2f")) mx.rotary().setMix(rmix);
        float rdrive = mx.rotary().drive();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("drive##rot", &rdrive, 0.0f, 1.0f, rdrive <= 0.0f ? "clean" : "%.2f"))
            mx.rotary().setDrive(rdrive);
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
        const char* modDivsF[audio::kModSyncDivisions];
        for (int d = 0; d < audio::kModSyncDivisions; ++d) modDivsF[d] = audio::modSyncDivisionName(d);
        bool finv = mx.flanger().invert();
        ImGui::SameLine();
        if (ImGui::Checkbox("Invert##fla", &finv)) mx.flanger().setInvert(finv);
        bool fsync = mx.flanger().sync();
        ImGui::SameLine();
        if (ImGui::Checkbox("Sync##fla", &fsync)) mx.flanger().setSync(fsync);
        ImGui::SameLine();
        int fdiv = mx.flanger().syncDivision();
        ImGui::SetNextItemWidth(70.0f);
        if (ImGui::Combo("div##fla", &fdiv, modDivsF, audio::kModSyncDivisions))
            mx.flanger().setSyncDivision(fdiv);
    }
    {
        bool en = mx.phaser().enabled();
        if (ImGui::Checkbox("Phaser", &en)) mx.phaser().setEnabled(en);
        float wet = mx.phaser().mix();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("mix##phs", &wet, 0.0f, 1.0f, "%.2f")) mx.phaser().setMix(wet);
        int pstages = mx.phaser().stages();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderInt("stages##phs", &pstages, 2, 12)) mx.phaser().setStages(pstages);
        const char* modDivsP[audio::kModSyncDivisions];
        for (int d = 0; d < audio::kModSyncDivisions; ++d) modDivsP[d] = audio::modSyncDivisionName(d);
        bool pstereo = mx.phaser().stereo();
        ImGui::SameLine();
        if (ImGui::Checkbox("Stereo##phs", &pstereo)) mx.phaser().setStereo(pstereo);
        bool psync = mx.phaser().sync();
        ImGui::SameLine();
        if (ImGui::Checkbox("Sync##phs", &psync)) mx.phaser().setSync(psync);
        ImGui::SameLine();
        int pdiv = mx.phaser().syncDivision();
        ImGui::SetNextItemWidth(70.0f);
        if (ImGui::Combo("div##phs", &pdiv, modDivsP, audio::kModSyncDivisions))
            mx.phaser().setSyncDivision(pdiv);
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
        float fbLc = mx.delay().feedbackLowCut();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("lo-cut##dly", &fbLc, 0.0f, 1000.0f, "%.0f Hz")) mx.delay().setFeedbackLowCut(fbLc);
        float modD = mx.delay().modDepth();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("mod ms##dly", &modD, 0.0f, 20.0f, "%.1f")) mx.delay().setModDepth(modD);
        float modR = mx.delay().modRate();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("mod Hz##dly", &modR, 0.0f, 10.0f, "%.2f")) mx.delay().setModRate(modR);
        float dduck = mx.delay().duck();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("duck##dly", &dduck, 0.0f, 1.0f, "%.2f")) mx.delay().setDuck(dduck);
        bool sync = mx.delay().sync();
        if (ImGui::Checkbox("Sync##dly", &sync)) mx.delay().setSync(sync);
        ImGui::SameLine();
        int div = mx.delay().syncDivision();
        const char* divNames[audio::Delay::kSyncDivisions];
        for (int d = 0; d < audio::Delay::kSyncDivisions; ++d)
            divNames[d] = audio::Delay::syncDivisionName(d);
        ImGui::SetNextItemWidth(80.0f);
        if (ImGui::Combo("div##dly", &div, divNames, audio::Delay::kSyncDivisions))
            mx.delay().setSyncDivision(div);
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
        bool frz = mx.reverb().freeze();
        ImGui::SameLine();
        if (ImGui::Checkbox("freeze##rev", &frz)) mx.reverb().setFreeze(frz);
        float duck = mx.reverb().duck();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("duck##rev", &duck, 0.0f, 1.0f, "%.2f")) mx.reverb().setDuck(duck);
        float lc = mx.reverb().wetLowCut();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("lo-cut##rev", &lc, 0.0f, 2000.0f, "%.0f Hz")) mx.reverb().setWetLowCut(lc);
        float hc = mx.reverb().wetHighCut();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("hi-cut##rev", &hc, 500.0f, 20000.0f, "%.0f Hz")) mx.reverb().setWetHighCut(hc);
        float gate = mx.reverb().gateMs();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("gate##rev", &gate, 0.0f, 1000.0f, gate <= 0.0f ? "gate off" : "%.0f ms"))
            mx.reverb().setGateMs(gate);
        float shim = mx.reverb().shimmer();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("shimmer##rev", &shim, 0.0f, 1.0f, shim <= 0.0f ? "shimmer off" : "%.2f"))
            mx.reverb().setShimmer(shim);
        float mdep = mx.reverb().modDepth();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("mod##rev", &mdep, 0.0f, 6.0f, mdep <= 0.0f ? "static" : "%.2f ms"))
            mx.reverb().setModDepth(mdep);
        float mrate = mx.reverb().modRate();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("mod Hz##rev", &mrate, 0.0f, 8.0f, "%.2f")) mx.reverb().setModRate(mrate);
    }
    {
        bool en = mx.convolver().enabled();
        if (ImGui::Checkbox("Convolver", &en)) mx.convolver().setEnabled(en);
        float dec = mx.convolver().decay();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("decay s##conv", &dec, 0.05f, 0.5f, "%.2f")) mx.convolver().setDecay(dec);
        float tn = mx.convolver().tone();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("tone Hz##conv", &tn, 500.0f, 18000.0f, "%.0f")) mx.convolver().setTone(tn);
        float mix = mx.convolver().mix();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("mix##conv", &mix, 0.0f, 1.0f, "%.2f")) mx.convolver().setMix(mix);
    }
    {
        bool en = mx.widener().enabled();
        if (ImGui::Checkbox("Stereo Widener", &en)) mx.widener().setEnabled(en);
        float w = mx.widener().width();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("width##wide", &w, 0.0f, 2.0f, "%.2f")) mx.widener().setWidth(w);
        float bm = mx.widener().bassMonoHz();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("bass mono Hz##wide", &bm, 0.0f, 500.0f, "%.0f")) mx.widener().setBassMonoHz(bm);
    }
    {
        bool en = mx.imager().enabled();
        if (ImGui::Checkbox("Stereo Imager", &en)) mx.imager().setEnabled(en);
        float clo = mx.imager().crossoverLow(), chi = mx.imager().crossoverHigh();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::SliderFloat("lo/mid##img", &clo, 20.0f, 2000.0f, "%.0f")) mx.imager().setCrossoverLow(clo);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::SliderFloat("mid/hi##img", &chi, 200.0f, 18000.0f, "%.0f")) mx.imager().setCrossoverHigh(chi);
        const char* imgBand[3] = {"low W", "mid W", "high W"};
        for (int b = 0; b < audio::StereoImager::kBands; ++b) {
            float w = mx.imager().bandWidth(b);
            ImGui::PushID(b);
            ImGui::SetNextItemWidth(130.0f);
            if (ImGui::SliderFloat(imgBand[b], &w, 0.0f, 2.0f, "%.2f")) mx.imager().setBandWidth(b, w);
            ImGui::PopID();
            if (b < 2) ImGui::SameLine();
        }
    }
    {
        auto& sat = mx.multibandSaturator();
        bool en = sat.enabled();
        if (ImGui::Checkbox("Multiband Saturator", &en)) sat.setEnabled(en);
        float clo = sat.crossoverLow(), chi = sat.crossoverHigh();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::SliderFloat("lo/mid##mbsat", &clo, 20.0f, 2000.0f, "%.0f")) sat.setCrossoverLow(clo);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::SliderFloat("mid/hi##mbsat", &chi, 200.0f, 18000.0f, "%.0f")) sat.setCrossoverHigh(chi);
        const char* satBand[3] = {"low drive", "mid drive", "high drive"};
        for (int b = 0; b < audio::MultibandSaturator::kBands; ++b) {
            float d = sat.drive(b);
            ImGui::PushID(100 + b);
            ImGui::SetNextItemWidth(130.0f);
            if (ImGui::SliderFloat(satBand[b], &d, 0.0f, 1.0f, "%.2f")) sat.setDrive(b, d);
            ImGui::PopID();
            if (b < 2) ImGui::SameLine();
        }
    }
    {
        bool en = mx.stereoEnhancer().enabled();
        if (ImGui::Checkbox("Stereo Enhancer", &en)) mx.stereoEnhancer().setEnabled(en);
        float ms = mx.stereoEnhancer().delayMs();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("ms##senh", &ms, 0.0f, 40.0f, "%.1f")) mx.stereoEnhancer().setDelayMs(ms);
        float amt = mx.stereoEnhancer().amount();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("amt##senh", &amt, 0.0f, 1.0f, "%.2f")) mx.stereoEnhancer().setAmount(amt);
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
        int apShape = static_cast<int>(mx.autopan().shape());
        const char* apShapes[] = {"Sine", "Triangle", "Square", "Saw"};
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::Combo("shape##apan", &apShape, apShapes, 4))
            mx.autopan().setShape(static_cast<audio::AutoPan::Shape>(apShape));
        bool apsync = mx.autopan().sync();
        ImGui::SameLine();
        if (ImGui::Checkbox("Sync##apan", &apsync)) mx.autopan().setSync(apsync);
        ImGui::SameLine();
        const char* modDivsA[audio::kModSyncDivisions];
        for (int d = 0; d < audio::kModSyncDivisions; ++d) modDivsA[d] = audio::modSyncDivisionName(d);
        int apdiv = mx.autopan().syncDivision();
        ImGui::SetNextItemWidth(70.0f);
        if (ImGui::Combo("div##apan", &apdiv, modDivsA, audio::kModSyncDivisions))
            mx.autopan().setSyncDivision(apdiv);
    }
    {
        bool en = mx.monobass().enabled();
        if (ImGui::Checkbox("Mono Bass", &en)) mx.monobass().setEnabled(en);
        float x = mx.monobass().crossover();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat("Hz##mono", &x, 20.0f, 500.0f, "%.0f")) mx.monobass().setCrossover(x);
    }
    {
        bool en = mx.subbass().enabled();
        if (ImGui::Checkbox("Sub Bass", &en)) mx.subbass().setEnabled(en);
        float amt = mx.subbass().amount();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("amt##sub", &amt, 0.0f, 1.0f, "%.2f")) mx.subbass().setAmount(amt);
        float cut = mx.subbass().cutoff();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("track Hz##sub", &cut, 40.0f, 320.0f, "%.0f")) mx.subbass().setCutoff(cut);
        float tn = mx.subbass().tone();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("tone Hz##sub", &tn, 60.0f, 1000.0f, "%.0f")) mx.subbass().setTone(tn);
    }
    {
        bool en = mx.octaver().enabled();
        if (ImGui::Checkbox("Octaver", &en)) mx.octaver().setEnabled(en);
        float amt = mx.octaver().amount();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("up##oct", &amt, 0.0f, 1.0f, "%.2f")) mx.octaver().setAmount(amt);
        float tn = mx.octaver().tone();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("tone Hz##oct", &tn, 500.0f, 18000.0f, "%.0f")) mx.octaver().setTone(tn);
    }
    {
        bool en = mx.autowah().enabled();
        if (ImGui::Checkbox("Auto-Wah", &en)) mx.autowah().setEnabled(en);
        float base = mx.autowah().baseHz();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("base Hz##wah", &base, 40.0f, 2000.0f, "%.0f"))
            mx.autowah().setBaseHz(base);
        float range = mx.autowah().rangeHz();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("range##wah", &range, 0.0f, 8000.0f, "%.0f"))
            mx.autowah().setRangeHz(range);
        float sens = mx.autowah().sensitivity();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("sens##wah", &sens, 0.0f, 1.0f, "%.2f")) mx.autowah().setSensitivity(sens);
        float reso = mx.autowah().resonance();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("reso##wah", &reso, 0.5f, 20.0f, "%.1f")) mx.autowah().setResonance(reso);
        bool down = mx.autowah().downward();
        ImGui::SameLine();
        if (ImGui::Checkbox("down##wah", &down)) mx.autowah().setDownward(down);
        float wahMix = mx.autowah().mix();
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::SliderFloat("mix##wah", &wahMix, 0.0f, 1.0f, "%.2f")) mx.autowah().setMix(wahMix);
    }
    {
        bool en = mx.comb().enabled();
        if (ImGui::Checkbox("Comb Resonator", &en)) mx.comb().setEnabled(en);
        float freq = mx.comb().frequency();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("Hz##comb", &freq, 20.0f, 2000.0f, "%.0f")) mx.comb().setFrequency(freq);
        float fb = mx.comb().feedback();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("ring##comb", &fb, 0.0f, 0.98f, "%.2f")) mx.comb().setFeedback(fb);
        float mix = mx.comb().mix();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("mix##comb", &mix, 0.0f, 1.0f, "%.2f")) mx.comb().setMix(mix);
        float cdamp = mx.comb().damping();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("damp##comb", &cdamp, 0.0f, 1.0f, "%.2f")) mx.comb().setDamping(cdamp);
    }
    {
        auto& cr = mx.chordResonator();
        bool en = cr.enabled();
        if (ImGui::Checkbox("Chord Resonator", &en)) cr.setEnabled(en);
        int root = cr.rootNote();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::SliderInt("root##chres", &root, 24, 84, "MIDI %d")) cr.setRootNote(root);
        int chord = static_cast<int>(cr.chord());
        const char* chords[] = {"Major", "Minor", "Dom7", "Min7", "Sus4", "Octaves"};
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::Combo("chord##chres", &chord, chords, 6))
            cr.setChord(static_cast<audio::ChordResonator::Chord>(chord));
        float fb = cr.feedback();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("ring##chres", &fb, 0.0f, 0.98f, "%.2f")) cr.setFeedback(fb);
        float damp = cr.damping();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("damp##chres", &damp, 0.0f, 1.0f, "%.2f")) cr.setDamping(damp);
        float mix = cr.mix();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("mix##chres", &mix, 0.0f, 1.0f, "%.2f")) cr.setMix(mix);
    }
    {
        bool en = mx.tremolo().enabled();
        if (ImGui::Checkbox("Tremolo", &en)) mx.tremolo().setEnabled(en);
        float rate = mx.tremolo().rate();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("Hz##trem", &rate, 0.05f, 30.0f, "%.2f")) mx.tremolo().setRate(rate);
        float depth = mx.tremolo().depth();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("depth##trem", &depth, 0.0f, 1.0f, "%.2f")) mx.tremolo().setDepth(depth);
        int shape = static_cast<int>(mx.tremolo().shape());
        const char* shapes[] = {"Sine", "Square (gate)", "Triangle", "Saw"};
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::Combo("shape##trem", &shape, shapes, 4))
            mx.tremolo().setShape(static_cast<audio::Tremolo::Shape>(shape));
        bool tsync = mx.tremolo().sync();
        if (ImGui::Checkbox("Sync##trem", &tsync)) mx.tremolo().setSync(tsync);
        ImGui::SameLine();
        int tdiv = mx.tremolo().syncDivision();
        const char* tdivNames[audio::Tremolo::kSyncDivisions];
        for (int d = 0; d < audio::Tremolo::kSyncDivisions; ++d)
            tdivNames[d] = audio::Tremolo::syncDivisionName(d);
        ImGui::SetNextItemWidth(80.0f);
        if (ImGui::Combo("div##trem", &tdiv, tdivNames, audio::Tremolo::kSyncDivisions))
            mx.tremolo().setSyncDivision(tdiv);
    }
    {
        bool en = mx.stepGate().enabled();
        if (ImGui::Checkbox("Step Gate", &en)) mx.stepGate().setEnabled(en);
        ImGui::SameLine();
        bool gsync = mx.stepGate().sync();
        if (ImGui::Checkbox("Sync##sg", &gsync)) mx.stepGate().setSync(gsync);
        ImGui::SameLine();
        if (gsync) {
            int gdiv = mx.stepGate().syncDivision();
            const char* gdivNames[audio::kModSyncDivisions];
            for (int d = 0; d < audio::kModSyncDivisions; ++d)
                gdivNames[d] = audio::modSyncDivisionName(d);
            ImGui::SetNextItemWidth(80.0f);
            if (ImGui::Combo("div##sg", &gdiv, gdivNames, audio::kModSyncDivisions))
                mx.stepGate().setSyncDivision(gdiv);
        } else {
            float grate = mx.stepGate().rate();
            ImGui::SetNextItemWidth(90.0f);
            if (ImGui::SliderFloat("cyc/s##sg", &grate, 0.1f, 12.0f, "%.2f"))
                mx.stepGate().setRate(grate);
        }
        // 16 vertical step-level sliders drawn as a pattern row.
        for (int s = 0; s < audio::StepGate::kSteps; ++s) {
            if (s > 0) ImGui::SameLine();
            float lvl = mx.stepGate().step(s);
            ImGui::PushID(s);
            if (ImGui::VSliderFloat("##sgstep", ImVec2(16, 60), &lvl, 0.0f, 1.0f, ""))
                mx.stepGate().setStep(s, lvl);
            ImGui::PopID();
        }
        float gmix = mx.stepGate().mix();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("mix##sg", &gmix, 0.0f, 1.0f, "%.2f")) mx.stepGate().setMix(gmix);
    }
    {
        bool en = mx.stereoDelay().enabled();
        if (ImGui::Checkbox("Stereo Delay", &en)) mx.stereoDelay().setEnabled(en);
        float lms = mx.stereoDelay().leftMs();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::SliderFloat("L ms##sd", &lms, 1.0f, 1000.0f, "%.0f"))
            mx.stereoDelay().setLeftMs(lms);
        float rms = mx.stereoDelay().rightMs();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::SliderFloat("R ms##sd", &rms, 1.0f, 1000.0f, "%.0f"))
            mx.stereoDelay().setRightMs(rms);
        float fb = mx.stereoDelay().feedback();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::SliderFloat("fb##sd", &fb, 0.0f, 0.95f, "%.2f")) mx.stereoDelay().setFeedback(fb);
        float mix = mx.stereoDelay().mix();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::SliderFloat("mix##sd", &mix, 0.0f, 1.0f, "%.2f")) mx.stereoDelay().setMix(mix);
        float sdDamp = mx.stereoDelay().damping();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::SliderFloat("damp##sd", &sdDamp, 0.0f, 1.0f, "%.2f")) mx.stereoDelay().setDamping(sdDamp);
        float sdLc = mx.stereoDelay().feedbackLowCut();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("lo-cut##sd", &sdLc, 0.0f, 1000.0f, "%.0f Hz")) mx.stereoDelay().setFeedbackLowCut(sdLc);
        bool sdping = mx.stereoDelay().pingPong();
        if (ImGui::Checkbox("Ping-pong##sd", &sdping)) mx.stereoDelay().setPingPong(sdping);
        ImGui::SameLine();
        bool sdsync = mx.stereoDelay().sync();
        if (ImGui::Checkbox("Sync##sd", &sdsync)) mx.stereoDelay().setSync(sdsync);
        const char* sddivNames[audio::Delay::kSyncDivisions];
        for (int d = 0; d < audio::Delay::kSyncDivisions; ++d)
            sddivNames[d] = audio::Delay::syncDivisionName(d);
        ImGui::SameLine();
        int ldiv = mx.stereoDelay().leftDivision();
        ImGui::SetNextItemWidth(70.0f);
        if (ImGui::Combo("L##sddiv", &ldiv, sddivNames, audio::Delay::kSyncDivisions))
            mx.stereoDelay().setLeftDivision(ldiv);
        ImGui::SameLine();
        int rdiv = mx.stereoDelay().rightDivision();
        ImGui::SetNextItemWidth(70.0f);
        if (ImGui::Combo("R##sddiv", &rdiv, sddivNames, audio::Delay::kSyncDivisions))
            mx.stereoDelay().setRightDivision(rdiv);
    }
    {
        auto& rd = mx.reverseDelay();
        bool en = rd.enabled();
        if (ImGui::Checkbox("Reverse Delay", &en)) rd.setEnabled(en);
        float t = rd.timeMs();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("time##rd", &t, 20.0f, 1000.0f, "%.0f ms")) rd.setTimeMs(t);
        float fb = rd.feedback();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::SliderFloat("fb##rd", &fb, 0.0f, 0.95f, "%.2f")) rd.setFeedback(fb);
        float mix = rd.mix();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::SliderFloat("mix##rd", &mix, 0.0f, 1.0f, "%.2f")) rd.setMix(mix);
    }
    {
        auto& mt = mx.multiTapDelay();
        bool en = mt.enabled();
        if (ImGui::Checkbox("Multi-Tap Delay", &en)) mt.setEnabled(en);
        float t = mt.timeMs();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("time##mt", &t, 10.0f, 1000.0f, "%.0f ms")) mt.setTimeMs(t);
        int taps = mt.taps();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::SliderInt("taps##mt", &taps, 1, audio::MultiTapDelay::kMaxTaps)) mt.setTaps(taps);
        float decay = mt.decay();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::SliderFloat("decay##mt", &decay, 0.0f, 1.0f, "%.2f")) mt.setDecay(decay);
        float spread = mt.spread();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::SliderFloat("spread##mt", &spread, 0.0f, 1.0f, "%.2f")) mt.setSpread(spread);
        float mmix = mt.mix();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::SliderFloat("mix##mt", &mmix, 0.0f, 1.0f, "%.2f")) mt.setMix(mmix);
    }
    {
        bool en = mx.formant().enabled();
        if (ImGui::Checkbox("Formant", &en)) mx.formant().setEnabled(en);
        int vowel = static_cast<int>(mx.formant().vowel());
        const char* vowels[] = {"A", "E", "I", "O", "U"};
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        if (ImGui::Combo("vowel##fmt", &vowel, vowels, 5))
            mx.formant().setVowel(static_cast<audio::FormantFilter::Vowel>(vowel));
        float mix = mx.formant().mix();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("mix##fmt", &mix, 0.0f, 1.0f, "%.2f")) mx.formant().setMix(mix);
        bool morphEn = mx.formant().morphEnabled();
        if (ImGui::Checkbox("morph##fmt", &morphEn)) mx.formant().setMorphEnabled(morphEn);
        ImGui::SameLine();
        float morphPos = mx.formant().morph();
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::SliderFloat("A-E-I-O-U##fmt", &morphPos, 0.0f, 4.0f, "%.2f"))
            mx.formant().setMorph(morphPos);
    }
    {
        auto& vc = mx.vocoder();
        bool en = vc.enabled();
        if (ImGui::Checkbox("Vocoder", &en)) vc.setEnabled(en);
        int carrier = static_cast<int>(vc.carrier());
        const char* carriers[] = {"Saw", "Noise"};
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::Combo("carrier##voc", &carrier, carriers, 2))
            vc.setCarrier(static_cast<audio::Vocoder::Carrier>(carrier));
        float chz = vc.carrierHz();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("pitch##voc", &chz, 20.0f, 1000.0f, "%.0f Hz")) vc.setCarrierHz(chz);
        float rel = vc.releaseMs();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("release##voc", &rel, 1.0f, 500.0f, "%.0f ms")) vc.setReleaseMs(rel);
        float vmix = vc.mix();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("mix##voc", &vmix, 0.0f, 1.0f, "%.2f")) vc.setMix(vmix);
    }
    {
        bool en = mx.utility().enabled();
        if (ImGui::Checkbox("Utility", &en)) mx.utility().setEnabled(en);
        float gdb = mx.utility().gainDb();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("dB##util", &gdb, -24.0f, 24.0f, "%.1f")) mx.utility().setGainDb(gdb);
        bool invL = mx.utility().invertL();
        ImGui::SameLine();
        if (ImGui::Checkbox("invL##util", &invL)) mx.utility().setInvertL(invL);
        bool invR = mx.utility().invertR();
        ImGui::SameLine();
        if (ImGui::Checkbox("invR##util", &invR)) mx.utility().setInvertR(invR);
        bool mono = mx.utility().mono();
        ImGui::SameLine();
        if (ImGui::Checkbox("mono##util", &mono)) mx.utility().setMono(mono);
        float uwidth = mx.utility().width();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("width##util", &uwidth, 0.0f, 2.0f, "%.2f")) mx.utility().setWidth(uwidth);
    }
    {
        bool en = mx.limiter().enabled();
        if (ImGui::Checkbox("Limiter", &en)) mx.limiter().setEnabled(en);
        float gain = mx.limiter().inputGainDb();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::SliderFloat("gain##lim", &gain, 0.0f, 36.0f, "%.1f")) mx.limiter().setInputGainDb(gain);
        float ceil = mx.limiter().ceilingDb();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::SliderFloat("ceil##lim", &ceil, -24.0f, 0.0f, "%.1f")) mx.limiter().setCeilingDb(ceil);
        float rel = mx.limiter().releaseMs();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::SliderFloat("rel##lim", &rel, 1.0f, 1000.0f, "%.0f")) mx.limiter().setReleaseMs(rel);
        float look = mx.limiter().lookaheadMs();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::SliderFloat("look##lim", &look, 0.1f, 10.0f, "%.1f")) mx.limiter().setLookaheadMs(look);
        ImGui::SameLine();
        ImGui::Text("GR %.1f dB", mx.limiter().gainReductionDb());
    }
    {
        auto& lv = mx.leveler();
        bool en = lv.enabled();
        if (ImGui::Checkbox("Leveler", &en)) lv.setEnabled(en);
        float tgt = lv.targetDb();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::SliderFloat("target##lvl", &tgt, -36.0f, 0.0f, "%.1f dB")) lv.setTargetDb(tgt);
        float resp = lv.responseMs();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("response##lvl", &resp, 50.0f, 5000.0f, "%.0f ms")) lv.setResponseMs(resp);
        float mg = lv.maxGainDb();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::SliderFloat("max##lvl", &mg, 0.0f, 24.0f, "%.1f dB")) lv.setMaxGainDb(mg);
        ImGui::SameLine();
        ImGui::Text("%.1f dB", lv.gainDb());
    }
    {
        bool en = mx.clipper().enabled();
        if (ImGui::Checkbox("Clipper", &en)) mx.clipper().setEnabled(en);
        float drive = mx.clipper().driveDb();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::SliderFloat("drive##clip", &drive, 0.0f, 36.0f, "%.1f")) mx.clipper().setDriveDb(drive);
        float ceil = mx.clipper().ceiling();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::SliderFloat("ceil##clip", &ceil, 0.05f, 1.0f, "%.2f")) mx.clipper().setCeiling(ceil);
        float hard = mx.clipper().hardness();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::SliderFloat("hard##clip", &hard, 0.0f, 1.0f, "%.2f")) mx.clipper().setHardness(hard);
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
            bool soloed = tr.soloed();
            if (ImGui::Checkbox("Solo##trk", &soloed)) tr.setSoloed(soloed);
            ImGui::SameLine();
            float g = tr.gain();
            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::SliderFloat("gain##trk", &g, 0.0f, 2.0f, "%.2f")) tr.setGain(g);
            ImGui::SameLine();
            float pan = tr.pan();
            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::SliderFloat("pan##trk", &pan, -1.0f, 1.0f, "%.2f")) tr.setPan(pan);
            ImGui::SameLine();
            float rsend = tr.reverbSend();
            ImGui::SetNextItemWidth(90.0f);
            if (ImGui::SliderFloat("rev send##trk", &rsend, 0.0f, 1.0f, "%.2f")) tr.setReverbSend(rsend);
            ImGui::SameLine();
            float dsend = tr.delaySend();
            ImGui::SetNextItemWidth(90.0f);
            if (ImGui::SliderFloat("dly send##trk", &dsend, 0.0f, 1.0f, "%.2f")) tr.setDelaySend(dsend);

            bool eqEn = tr.eq().enabled();
            if (ImGui::Checkbox("EQ##trk", &eqEn)) tr.eq().setEnabled(eqEn);
            ImGui::SameLine();
            bool distEn = tr.distortion().enabled();
            if (ImGui::Checkbox("Drive##trk", &distEn)) tr.distortion().setEnabled(distEn);
            ImGui::SameLine();
            bool compEn = tr.compressor().enabled();
            if (ImGui::Checkbox("Comp##trk", &compEn)) tr.compressor().setEnabled(compEn);
            ImGui::SameLine();
            bool hpEn = tr.highpass().enabled();
            if (ImGui::Checkbox("HP##trk", &hpEn)) tr.highpass().setEnabled(hpEn);
            ImGui::SameLine();
            float hpCut = tr.highpass().cutoff();
            ImGui::SetNextItemWidth(90.0f);
            if (ImGui::SliderFloat("HP Hz##trk", &hpCut, 20.0f, 1000.0f, "%.0f"))
                tr.highpass().setCutoff(hpCut);
            bool gEn = tr.gate().enabled();
            if (ImGui::Checkbox("Gate##trk", &gEn)) tr.gate().setEnabled(gEn);
            ImGui::SameLine();
            float gThr = tr.gate().thresholdDb();
            ImGui::SetNextItemWidth(90.0f);
            if (ImGui::SliderFloat("gate dB##trk", &gThr, -80.0f, 0.0f, "%.0f"))
                tr.gate().setThresholdDb(gThr);
            bool trEn = tr.transientShaper().enabled();
            if (ImGui::Checkbox("Trans##trk", &trEn)) tr.transientShaper().setEnabled(trEn);
            ImGui::SameLine();
            float trAtt = tr.transientShaper().attack();
            ImGui::SetNextItemWidth(90.0f);
            if (ImGui::SliderFloat("attack##trk", &trAtt, -1.0f, 1.0f, "%.2f"))
                tr.transientShaper().setAttack(trAtt);
            ImGui::SameLine();
            float trSus = tr.transientShaper().sustain();
            ImGui::SetNextItemWidth(90.0f);
            if (ImGui::SliderFloat("sustain##trk", &trSus, -1.0f, 1.0f, "%.2f"))
                tr.transientShaper().setSustain(trSus);
            bool seEn = tr.stereoEnhancer().enabled();
            if (ImGui::Checkbox("Wide##trk", &seEn)) tr.stereoEnhancer().setEnabled(seEn);
            ImGui::SameLine();
            float seAmt = tr.stereoEnhancer().amount();
            ImGui::SetNextItemWidth(90.0f);
            if (ImGui::SliderFloat("width##trk", &seAmt, 0.0f, 1.0f, "%.2f"))
                tr.stereoEnhancer().setAmount(seAmt);
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
        const char* shapes[] = {"Sine", "Square", "Saw", "Triangle", "Trap", "Step"};
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::Combo("shape", &shape, shapes, 6)) {
            lane.lfo.shape = static_cast<audio::Waveform>(shape);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        ImGui::SliderFloat("rate", &lane.lfo.rateHz, 0.05f, 8.0f, "%.2f Hz");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);
        ImGui::DragFloatRange2("range", &lane.lo, &lane.hi, 1.0f);
        ImGui::SameLine();
        ImGui::Checkbox("sync", &lane.sync);
        ImGui::SameLine();
        const char* adivNames[audio::Automation::kSyncDivisions];
        for (int d = 0; d < audio::Automation::kSyncDivisions; ++d)
            adivNames[d] = audio::Automation::syncDivisionName(d);
        ImGui::SetNextItemWidth(90.0f);
        ImGui::Combo("div##auto", &lane.syncDiv, adivNames, audio::Automation::kSyncDivisions);
        ImGui::SameLine();
        ImGui::Checkbox("S&H", &lane.lfo.sampleHold); // random stepped LFO
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
    // Loop region over the playlist: [start, end). end <= start = whole playlist.
    int loopStart = seq.songLoopStart();
    int loopEnd = seq.songLoopEnd();
    const int plLen = static_cast<int>(seq.playlist().size());
    ImGui::SetNextItemWidth(80.0f);
    bool lrCh = ImGui::InputInt("loop start##song", &loopStart, 1, 1);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    lrCh |= ImGui::InputInt("loop end##song", &loopEnd, 1, 1);
    if (lrCh) seq.setSongLoopRange(loopStart, loopEnd);
    ImGui::SameLine();
    if (ImGui::SmallButton("clear region##song")) seq.setSongLoopRange(0, 0);
    ImGui::SameLine();
    if (seq.songLoopEnd() > seq.songLoopStart())
        ImGui::TextDisabled("looping [%d,%d)", seq.songLoopStart(), seq.songLoopEnd());
    else
        ImGui::TextDisabled("(full playlist, %d)", plLen);

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
                const char* waves[] = {"Sine", "Square", "Saw", "Triangle", "Trap", "Step"};
                if (ImGui::Combo("Waveform", &waveIndex, waves, 6)) {
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
