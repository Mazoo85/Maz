// Unit tests for maz::audio — pure DSP, no audio device required. Verifies the oscillator produces
// a sane signal (correct length, bounded amplitude, non-zero energy, expected frequency) and that
// the offline engine render matches the requested duration.

#include "maz/audio/AudioEngine.hpp"
#include "maz/audio/Oscillator.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace maz;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

// Count rising zero-crossings and convert to an estimated fundamental frequency.
double estimateHz(const std::vector<float>& mono, int sampleRate) {
    if (mono.size() < 2) {
        return 0.0;
    }
    int crossings = 0;
    float prev = mono[0];
    for (size_t i = 1; i < mono.size(); ++i) {
        if (prev <= 0.0f && mono[i] > 0.0f) {
            ++crossings;
        }
        prev = mono[i];
    }
    return static_cast<double>(crossings) * static_cast<double>(sampleRate) /
           static_cast<double>(mono.size());
}

} // namespace

int main() {
    const int sampleRate = 48000;
    const int frames = sampleRate; // one second

    // --- Oscillator: pure DSP ------------------------------------------------
    audio::Oscillator osc;
    osc.setWaveform(audio::Waveform::Sine);
    osc.setAmplitude(0.5f);
    osc.noteOn(440.0f);

    std::vector<float> mono(static_cast<size_t>(frames), 0.0f);
    osc.render(mono.data(), frames, sampleRate);

    check(mono.size() == static_cast<size_t>(frames), "render fills the requested sample count");

    float peak = 0.0f;
    double energy = 0.0;
    bool inRange = true;
    for (float s : mono) {
        peak = std::max(peak, std::fabs(s));
        energy += static_cast<double>(s) * static_cast<double>(s);
        if (s < -1.0f || s > 1.0f) {
            inRange = false;
        }
    }
    check(inRange, "all samples stay within [-1, 1]");
    check(energy > 0.0, "signal carries non-zero energy");
    check(peak > 0.4f && peak <= 0.5f + 1e-4f, "peak tracks the 0.5 amplitude");

    const double hz = estimateHz(mono, sampleRate);
    check(std::fabs(hz - 440.0) < 2.0, "estimated frequency is ~440 Hz");

    // noteOff() then a full ramp should decay to silence.
    osc.noteOff();
    std::vector<float> tail(static_cast<size_t>(frames), 0.0f);
    osc.render(tail.data(), frames, sampleRate);
    check(!osc.active(), "voice goes inactive after release");
    check(std::fabs(tail.back()) < 1e-6f, "signal decays to silence after noteOff");

    // Frequency should be settable and reflected in the output.
    audio::Oscillator osc2;
    osc2.noteOn(880.0f);
    std::vector<float> mono2(static_cast<size_t>(frames), 0.0f);
    osc2.render(mono2.data(), frames, sampleRate);
    check(std::fabs(estimateHz(mono2, sampleRate) - 880.0) < 3.0, "880 Hz note renders at ~880 Hz");

    // --- Trapezoid waveform: a clipped triangle (flat tops, DC-free) ---------
    {
        const int N = 2048;
        auto hfEnergy = [](audio::Waveform w, int n) {
            double e = 0.0;
            float prev = audio::waveSample(w, 0.0);
            for (int i = 1; i <= n; ++i) {
                const float cur = audio::waveSample(w, static_cast<double>(i % n) / n);
                const float d = cur - prev;
                e += static_cast<double>(d) * d;
                prev = cur;
            }
            return e;
        };
        // DC-free: one cycle integrates to ~0 (symmetric, no offset — safe as an audio oscillator).
        double dc = 0.0;
        for (int i = 0; i < N; ++i) {
            dc += audio::waveSample(audio::Waveform::Trapezoid, static_cast<double>(i) / N);
        }
        check(std::fabs(dc / N) < 1e-3, "trapezoid is DC-free over a cycle");
        // Flat tops: samples across the peak plateau all clamp to +1, unlike a triangle which only
        // touches +1 at a single instant.
        check(std::fabs(audio::waveSample(audio::Waveform::Trapezoid, 0.02) - 1.0f) < 1e-4f &&
                  std::fabs(audio::waveSample(audio::Waveform::Trapezoid, 0.08) - 1.0f) < 1e-4f,
              "trapezoid has a flat plateau at its peak");
        check(audio::waveSample(audio::Waveform::Triangle, 0.08) < 0.9f,
              "a triangle does not (control: it is still ramping at the same phase)");
        // Harmonic brightness sits between triangle (smoothest) and square (hardest edges).
        const double tri = hfEnergy(audio::Waveform::Triangle, N);
        const double trap = hfEnergy(audio::Waveform::Trapezoid, N);
        const double sqr = hfEnergy(audio::Waveform::Square, N);
        check(trap > tri && trap < sqr, "trapezoid brightness sits between triangle and square");
    }

    // --- Step-sine waveform: a sine quantized to a step ladder (lo-fi/chiptune) ----
    {
        const int N = 2048;
        auto hfEnergy = [](audio::Waveform w, int n) {
            double e = 0.0;
            float prev = audio::waveSample(w, 0.0);
            for (int i = 1; i <= n; ++i) {
                const float cur = audio::waveSample(w, static_cast<double>(i % n) / n);
                const float d = cur - prev;
                e += static_cast<double>(d) * d;
                prev = cur;
            }
            return e;
        };
        // DC-free: the ladder is symmetric about zero, so one cycle integrates to ~0.
        double dc = 0.0;
        bool stepInRange = true;
        bool quantized = true;
        for (int i = 0; i < N; ++i) {
            const float s =
                audio::waveSample(audio::Waveform::StepSine, static_cast<double>(i) / N);
            dc += s;
            if (s < -1.0f || s > 1.0f) {
                stepInRange = false;
            }
            // Every value lands on the 0.25 ladder (4 steps per polarity).
            const float q = s * 4.0f;
            if (std::fabs(q - std::round(q)) > 1e-4f) {
                quantized = false;
            }
        }
        check(std::fabs(dc / N) < 1e-3, "step-sine is DC-free over a cycle");
        check(stepInRange, "step-sine stays within [-1, 1]");
        check(quantized, "step-sine values sit on a discrete amplitude ladder");
        // Brighter than a pure sine: the staircase edges add harmonics.
        const double sine = hfEnergy(audio::Waveform::Sine, N);
        const double step = hfEnergy(audio::Waveform::StepSine, N);
        check(step > sine, "step-sine is brighter than a pure sine (added quantization harmonics)");
        // Same fundamental as a sine: an oscillator playing it still estimates at ~440 Hz.
        audio::Oscillator so;
        so.setWaveform(audio::Waveform::StepSine);
        so.noteOn(440.0f);
        std::vector<float> sm(static_cast<size_t>(frames), 0.0f);
        so.render(sm.data(), frames, sampleRate);
        check(std::fabs(estimateHz(sm, sampleRate) - 440.0) < 3.0,
              "step-sine preserves the fundamental (~440 Hz)");
    }

    // --- Rect-sine waveform: a half-wave rectified sine (even harmonics, DC-free) ---
    {
        const int N = 2048;
        auto hfEnergy = [](audio::Waveform w, int n) {
            double e = 0.0;
            float prev = audio::waveSample(w, 0.0);
            for (int i = 1; i <= n; ++i) {
                const float cur = audio::waveSample(w, static_cast<double>(i % n) / n);
                const float d = cur - prev;
                e += static_cast<double>(d) * d;
                prev = cur;
            }
            return e;
        };
        double dc = 0.0;
        double go2 = 0.0; // Goertzel-ish power at the 2nd harmonic (even harmonic marker)
        bool rsInRange = true;
        constexpr double kTwoPi = 6.283185307179586;
        for (int i = 0; i < N; ++i) {
            const float s =
                audio::waveSample(audio::Waveform::RectSine, static_cast<double>(i) / N);
            dc += s;
            go2 += static_cast<double>(s) * std::cos(2.0 * 2.0 * kTwoPi * i / N); // 2nd harmonic
            if (s < -1.0001f || s > 1.0001f) {
                rsInRange = false;
            }
        }
        check(std::fabs(dc / N) < 1e-3, "rect-sine is DC-free over a cycle");
        check(rsInRange, "rect-sine stays within [-1, 1]");
        // A pure sine has essentially no 2nd-harmonic content; the rectified sine has a strong one.
        double sineGo2 = 0.0;
        for (int i = 0; i < N; ++i) {
            const float s = audio::waveSample(audio::Waveform::Sine, static_cast<double>(i) / N);
            sineGo2 += static_cast<double>(s) * std::cos(2.0 * 2.0 * kTwoPi * i / N);
        }
        check(std::fabs(go2) > std::fabs(sineGo2) + 50.0,
              "rect-sine carries strong even (2nd) harmonic content, unlike a pure sine");
        check(hfEnergy(audio::Waveform::RectSine, N) > hfEnergy(audio::Waveform::Sine, N),
              "rect-sine is brighter than a pure sine");
        // Same fundamental as a sine: one positive excursion per cycle → ~440 Hz.
        audio::Oscillator ro;
        ro.setWaveform(audio::Waveform::RectSine);
        ro.noteOn(440.0f);
        std::vector<float> rm(static_cast<size_t>(frames), 0.0f);
        ro.render(rm.data(), frames, sampleRate);
        check(std::fabs(estimateHz(rm, sampleRate) - 440.0) < 3.0,
              "rect-sine preserves the fundamental (~440 Hz)");
    }

    // --- AudioEngine: offline render ----------------------------------------
    audio::AudioEngine engine;
    engine.initOffline();
    engine.noteOn(440.0f);
    const std::vector<float> stereo = engine.renderOffline(1.0);
    const int channels = engine.config().channels;
    check(stereo.size() == static_cast<size_t>(frames) * static_cast<size_t>(channels),
          "offline render length matches seconds * sampleRate * channels");
    check(engine.framesRendered() == static_cast<uint64_t>(frames),
          "sample clock advanced by exactly one second of frames");
    // Left and right channels should be identical (mono voice fanned out).
    check(stereo.size() >= 2 && std::fabs(stereo[0] - stereo[1]) < 1e-6f,
          "interleaved channels carry the same mono signal");

    // Offline render honors a custom sample rate (the --samplerate export path).
    {
        audio::AudioEngine e44;
        audio::AudioConfig c44;
        c44.sampleRate = 44100;
        e44.initOffline(c44);
        check(e44.config().sampleRate == 44100, "initOffline honors a custom sample rate");
        e44.noteOn(440.0f);
        const std::vector<float> s44 = e44.renderOffline(1.0);
        const int ch44 = e44.config().channels;
        check(s44.size() == static_cast<size_t>(44100) * static_cast<size_t>(ch44),
              "a 1 s render at 44.1 kHz is 44100 frames");
        std::vector<float> mono44(44100, 0.0f);
        for (int i = 0; i < 44100; ++i) {
            mono44[static_cast<size_t>(i)] = s44[static_cast<size_t>(i) * static_cast<size_t>(ch44)];
        }
        check(std::fabs(estimateHz(mono44, 44100) - 440.0) < 3.0,
              "the 440 Hz tone renders at the correct pitch at 44.1 kHz");
    }

    // --- AudioEngine: stem export (per-bus offline bounce) -------------------
    {
        auto energyOf = [](const std::vector<float>& b) {
            double e = 0.0;
            for (float v : b) {
                e += static_cast<double>(v) * static_cast<double>(v);
            }
            return e;
        };
        audio::AudioEngine eng;
        eng.initOffline();
        audio::Sequencer& seq = eng.sequencer();
        seq.setStep(0, 0, true);                          // a kick on the drums bus
        seq.roll().addNote(audio::Note{0, 8, 60, 1.0f});  // a lead note on the lead bus
        // (no notes on roll2 → the bass bus stays silent)
        seq.play();
        const audio::AudioEngine::Stems st = eng.renderStemsOffline(0.3);
        const int stChannels = eng.config().channels;
        const size_t want = static_cast<size_t>(0.3 * 48000.0) * static_cast<size_t>(stChannels);
        check(st.drums.size() == want && st.lead.size() == want && st.bass.size() == want,
              "each stem buffer matches seconds * sampleRate * channels");
        check(energyOf(st.drums) > 0.0, "the drums stem carries the kick");
        check(energyOf(st.lead) > 0.0, "the lead stem carries the synth note");
        check(energyOf(st.bass) < 1e-6, "the bass stem is silent (no bass notes)");
        // The stems are genuinely separated, not copies of the same mix.
        check(energyOf(st.drums) != energyOf(st.lead), "drums and lead stems differ (real separation)");

        // A per-track insert (muting the drums strip) is reflected in that stem alone.
        audio::AudioEngine eng2;
        eng2.initOffline();
        audio::Sequencer& seq2 = eng2.sequencer();
        seq2.setStep(0, 0, true);
        seq2.roll().addNote(audio::Note{0, 8, 60, 1.0f});
        eng2.mixer().track(audio::MixerBus::Drums).setMuted(true);
        seq2.play();
        const audio::AudioEngine::Stems st2 = eng2.renderStemsOffline(0.3);
        check(energyOf(st2.drums) < 1e-6, "muting the drums track silences the drums stem");
        check(energyOf(st2.lead) > 0.0, "muting the drums track leaves the lead stem intact");
    }

    // --- Per-bus solo: only soloed buses are heard in the master render -----
    {
        auto masterEnergy = [](audio::AudioEngine& e, double sec) {
            const std::vector<float> b = e.renderOffline(sec);
            double en = 0.0;
            for (float v : b) {
                en += static_cast<double>(v) * static_cast<double>(v);
            }
            return en;
        };
        // A kick on the drums bus only. Soloing the (empty) lead bus silences the master.
        audio::AudioEngine soloLead;
        soloLead.initOffline();
        soloLead.sequencer().setStep(0, 0, true);
        soloLead.mixer().track(audio::MixerBus::Lead).setSoloed(true);
        soloLead.sequencer().play();
        check(masterEnergy(soloLead, 0.3) < 1e-6,
              "soloing an empty bus silences the other (non-soloed) buses");

        // Soloing the drums bus keeps the kick audible.
        audio::AudioEngine soloDrums;
        soloDrums.initOffline();
        soloDrums.sequencer().setStep(0, 0, true);
        soloDrums.mixer().track(audio::MixerBus::Drums).setSoloed(true);
        soloDrums.sequencer().play();
        check(masterEnergy(soloDrums, 0.3) > 0.0, "soloing the active bus keeps it audible");

        // Mixer group routing: route the drums bus into a submix group; the group's insert chain then
        // shapes it before master. A group at gain 0 silences the routed bus; an unmuted transparent
        // group leaves it audible — proving the routed path engages and folds back into the master.
        audio::AudioEngine grp;
        grp.initOffline();
        grp.sequencer().setStep(0, 0, true); // kick on drums
        grp.sequencer().play();
        const double routedDry = masterEnergy(grp, 0.3);
        check(routedDry > 0.0, "baseline kick is audible before grouping");

        audio::AudioEngine grpSilent;
        grpSilent.initOffline();
        grpSilent.sequencer().setStep(0, 0, true);
        const int gi = grpSilent.mixer().addGroup();
        grpSilent.mixer().group(gi).setGain(0.0f);                       // group silences its input
        grpSilent.mixer().track(audio::MixerBus::Drums).setOutput(gi);   // drums -> silent group
        grpSilent.sequencer().play();
        check(masterEnergy(grpSilent, 0.3) < 1e-6,
              "routing a bus into a gain-0 group silences it at the master");

        audio::AudioEngine grpThru;
        grpThru.initOffline();
        grpThru.sequencer().setStep(0, 0, true);
        const int gt = grpThru.mixer().addGroup();                       // transparent group (unity)
        grpThru.mixer().track(audio::MixerBus::Drums).setOutput(gt);
        grpThru.sequencer().play();
        check(masterEnergy(grpThru, 0.3) > 0.0,
              "routing a bus into a transparent group keeps it audible");

        // Nested submix routing: drums -> group0 -> group1. Silencing the DOWNSTREAM group (group1)
        // silences the master, proving the signal actually flows group0 -> group1 -> master.
        audio::AudioEngine nest;
        nest.initOffline();
        nest.sequencer().setStep(0, 0, true);
        const int g0 = nest.mixer().addGroup();
        const int g1 = nest.mixer().addGroup();
        nest.mixer().track(audio::MixerBus::Drums).setOutput(g0); // drums -> group0
        nest.mixer().group(g0).setOutput(g1);                     // group0 -> group1 (nested)
        nest.mixer().group(g1).setGain(0.0f);                     // group1 silences the chain
        nest.sequencer().play();
        check(masterEnergy(nest, 0.3) < 1e-6,
              "silencing a downstream nested group silences the whole routed chain");

        audio::AudioEngine nestThru;
        nestThru.initOffline();
        nestThru.sequencer().setStep(0, 0, true);
        const int h0 = nestThru.mixer().addGroup();
        const int h1 = nestThru.mixer().addGroup();
        nestThru.mixer().track(audio::MixerBus::Drums).setOutput(h0);
        nestThru.mixer().group(h0).setOutput(h1); // transparent nested chain stays audible
        nestThru.sequencer().play();
        check(masterEnergy(nestThru, 0.3) > 0.0,
              "a transparent nested group chain stays audible at the master");

        // Group solo (unified with bus solo). anyGroupSoloed detects a soloed group.
        {
            audio::Mixer msolo;
            check(!msolo.anyGroupSoloed(), "a fresh mixer reports no soloed group");
            msolo.addGroup();
            msolo.group(0).setSoloed(true);
            check(msolo.anyGroupSoloed(), "anyGroupSoloed detects a soloed submix group");
        }

        // Soloing an EMPTY group silences the master (nothing routes into it), proving group solo is
        // honoured — a non-soloed group carrying the kick is silenced.
        audio::AudioEngine gsoloEmpty;
        gsoloEmpty.initOffline();
        gsoloEmpty.sequencer().setStep(0, 0, true); // kick -> group0
        const int ge0 = gsoloEmpty.mixer().addGroup();
        const int ge1 = gsoloEmpty.mixer().addGroup(); // an empty second group
        gsoloEmpty.mixer().track(audio::MixerBus::Drums).setOutput(ge0);
        gsoloEmpty.mixer().group(ge1).setSoloed(true); // solo the empty group
        gsoloEmpty.sequencer().play();
        check(masterEnergy(gsoloEmpty, 0.3) < 1e-6,
              "soloing an empty submix group silences the non-soloed groups");

        // Soloing the group that carries the kick keeps it audible — and its feeding drums bus stays
        // live (a soloed group preserves its inputs).
        audio::AudioEngine gsoloActive;
        gsoloActive.initOffline();
        gsoloActive.sequencer().setStep(0, 0, true);
        const int ga0 = gsoloActive.mixer().addGroup();
        gsoloActive.mixer().track(audio::MixerBus::Drums).setOutput(ga0);
        gsoloActive.mixer().group(ga0).setSoloed(true);
        gsoloActive.sequencer().play();
        check(masterEnergy(gsoloActive, 0.3) > 0.0,
              "soloing the group that carries the signal keeps it (and its feeding bus) audible");

        // Bus solo within a shared group: drums + lead both feed one group; soloing the (empty) drums
        // bus silences the lead that also feeds the group — bus solo isolates within the group.
        audio::AudioEngine gsoloBus;
        gsoloBus.initOffline();
        gsoloBus.sequencer().roll().addNote(audio::Note{0, 8, 60, 0.9f}); // a lead note (drums empty)
        const int gb0 = gsoloBus.mixer().addGroup();
        gsoloBus.mixer().track(audio::MixerBus::Drums).setOutput(gb0);
        gsoloBus.mixer().track(audio::MixerBus::Lead).setOutput(gb0);
        gsoloBus.mixer().track(audio::MixerBus::Drums).setSoloed(true); // solo the empty drums bus
        gsoloBus.sequencer().play();
        check(masterEnergy(gsoloBus, 0.3) < 1e-6,
              "soloing an empty bus that feeds a group silences that group's other inputs");
    }

    // --- Per-bus aux send: a bus's reverb send feeds the shared reverb tail --
    {
        // Energy in the late tail (well after the dry kick has decayed) reveals the reverb.
        auto tailEnergy = [](audio::AudioEngine& e) {
            const std::vector<float> b = e.renderOffline(0.6);
            const int ch = e.config().channels;
            const size_t start = static_cast<size_t>(0.35 * 48000.0) * static_cast<size_t>(ch);
            double en = 0.0;
            for (size_t i = start; i < b.size(); ++i) {
                en += static_cast<double>(b[i]) * static_cast<double>(b[i]);
            }
            return en;
        };
        // Dry: a single kick, no sends → the tail is near silent once the kick decays.
        audio::AudioEngine dry;
        dry.initOffline();
        dry.sequencer().setStep(0, 0, true);
        dry.sequencer().play();
        const double dryTail = tailEnergy(dry);

        // Wet: the same kick with the drum bus's reverb send up → a lingering reverb tail.
        audio::AudioEngine wet;
        wet.initOffline();
        wet.sequencer().setStep(0, 0, true);
        wet.mixer().track(audio::MixerBus::Drums).setReverbSend(1.0f);
        wet.sequencer().play();
        const double wetTail = tailEnergy(wet);
        check(wetTail > dryTail * 4.0, "a per-bus reverb send feeds the shared reverb tail");

        // A submix group's OWN reverb send also feeds the shared tail: route the kick into a group and
        // raise the group's send (not the bus's).
        audio::AudioEngine wetGrp;
        wetGrp.initOffline();
        wetGrp.sequencer().setStep(0, 0, true);
        const int wg = wetGrp.mixer().addGroup();
        wetGrp.mixer().track(audio::MixerBus::Drums).setOutput(wg); // drums -> group
        wetGrp.mixer().group(wg).setReverbSend(1.0f);               // the GROUP sends to the reverb bus
        wetGrp.sequencer().play();
        check(tailEnergy(wetGrp) > dryTail * 4.0,
              "a submix group's reverb send feeds the shared reverb tail");
    }

    // --- Master output metering: peak + RMS track the rendered level ---------
    {
        audio::AudioEngine eng;
        eng.initOffline();
        check(eng.masterPeak() == 0.0f && eng.masterRms() == 0.0f,
              "a fresh engine reports zero output level");
        eng.sequencer().setStep(0, 0, true); // a loud kick on the drums
        eng.sequencer().play();
        (void)eng.renderOffline(0.05); // render a block containing the kick onset
        check(eng.masterPeak() > 0.01f && eng.masterPeak() <= 1.0f,
              "the master peak meter reflects the rendered kick (0 < peak <= 1)");
        check(eng.masterRms() > 0.0f && eng.masterRms() <= eng.masterPeak() + 1e-6f,
              "the master RMS is positive and never exceeds the peak");
    }

    // --- Undo/redo: snapshot the project, edit, then rewind and replay -------
    {
        audio::AudioEngine eng;
        eng.initOffline();
        check(!eng.canUndo() && !eng.canRedo(), "a fresh engine has nothing to undo or redo");

        eng.sequencer().setBpm(120.0);
        eng.snapshotForUndo(); // record the 120 BPM state before editing
        check(eng.canUndo() && !eng.canRedo(), "snapshot arms undo (and leaves redo empty)");

        eng.sequencer().setBpm(80.0);
        check(std::fabs(eng.sequencer().bpm() - 80.0) < 1e-9, "the edit took (80 BPM)");

        check(eng.undoEdit(), "undo succeeds when there is history");
        check(std::fabs(eng.sequencer().bpm() - 120.0) < 1e-9, "undo restores the pre-edit 120 BPM");
        check(!eng.canUndo() && eng.canRedo(), "after undo, redo is armed and undo is empty");

        check(eng.redoEdit(), "redo succeeds after an undo");
        check(std::fabs(eng.sequencer().bpm() - 80.0) < 1e-9, "redo re-applies the 80 BPM edit");

        // A snapshot also captures step-grid edits (proves it's the whole project, not just tempo).
        eng.snapshotForUndo();
        eng.sequencer().setStep(0, 5, true);
        check(eng.sequencer().step(0, 5), "the step edit took");
        check(eng.undoEdit(), "undo the step edit");
        check(!eng.sequencer().step(0, 5), "undo clears the step that was set after the snapshot");

        // Undoing past the bottom of the stack is a no-op that leaves state untouched.
        eng.clearUndoHistory();
        check(!eng.canUndo() && !eng.canRedo(), "clearUndoHistory empties both stacks");
        const double bpmBefore = eng.sequencer().bpm();
        check(!eng.undoEdit(), "undo on an empty history returns false");
        check(std::fabs(eng.sequencer().bpm() - bpmBefore) < 1e-9, "a failed undo leaves state intact");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
