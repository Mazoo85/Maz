// Unit tests for maz::audio::Sequencer + DrumVoice — pure DSP/logic, no audio device.
// Verifies the pattern grid, transport timing (sample-accurate stepping), and that triggered
// drum voices produce sound while an empty pattern stays silent.

#include "maz/audio/DrumVoice.hpp"
#include "maz/audio/Sequencer.hpp"

#include <algorithm>
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

double rms(const std::vector<float>& buf) {
    if (buf.empty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (float s : buf) {
        sum += static_cast<double>(s) * static_cast<double>(s);
    }
    return std::sqrt(sum / static_cast<double>(buf.size()));
}

// Render `frames` of the sequencer into a fresh interleaved-stereo buffer (2*frames floats).
std::vector<float> renderMono(audio::Sequencer& seq, int frames, int sampleRate) {
    std::vector<float> buf(static_cast<size_t>(frames) * 2, 0.0f);
    seq.render(buf.data(), frames, sampleRate);
    return buf;
}

// Left/right RMS of an interleaved-stereo buffer.
double rmsChannel(const std::vector<float>& buf, int ch) {
    const size_t n = buf.size() / 2;
    if (n == 0) {
        return 0.0;
    }
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double v = static_cast<double>(buf[i * 2 + static_cast<size_t>(ch)]);
        sum += v * v;
    }
    return std::sqrt(sum / static_cast<double>(n));
}

} // namespace

int main() {
    const int sampleRate = 48000;

    // --- DrumVoice -----------------------------------------------------------
    audio::DrumVoice kick;
    kick.setType(audio::Drum::Kick);
    check(!kick.active(), "drum voice starts inactive");

    std::vector<float> quiet(4800, 0.0f);
    kick.render(quiet.data(), 4800, sampleRate);
    check(rms(quiet) == 0.0, "inactive voice renders silence");

    kick.trigger();
    check(kick.active(), "voice is active after trigger");
    std::vector<float> hit(4800, 0.0f); // 0.1 s
    kick.render(hit.data(), 4800, sampleRate);
    check(rms(hit) > 0.0, "triggered kick produces sound");

    // A closed hat is short — after ~0.3 s it should have decayed to inactive.
    audio::DrumVoice hat;
    hat.setType(audio::Drum::ClosedHat);
    hat.trigger();
    std::vector<float> tail(static_cast<size_t>(sampleRate) / 2, 0.0f);
    hat.render(tail.data(), sampleRate / 2, sampleRate);
    check(!hat.active(), "closed hat decays to inactive");

    // --- Sequencer grid ------------------------------------------------------
    audio::Sequencer seq;
    check(seq.numSteps() == 16, "default pattern is 16 steps");
    check(seq.numChannels() >= 4, "kit has at least four channels");

    seq.setStep(0, 0, true);
    check(seq.step(0, 0), "setStep switches a step on");
    seq.toggle(0, 0);
    check(!seq.step(0, 0), "toggle switches it back off");
    seq.setStep(0, 3, true);
    seq.clear();
    check(!seq.step(0, 3), "clear switches every step off");

    // Per-step velocity: a soft accent step is quieter than a full-velocity step.
    {
        audio::Sequencer full;
        full.setBpm(120.0);
        full.setStep(0, 0, true); // velocity 1.0
        full.play();
        const double loud = rms(renderMono(full, 6000, sampleRate));

        audio::Sequencer soft;
        soft.setBpm(120.0);
        soft.setStepVelocity(0, 0, 0.3f);
        soft.play();
        const double softRms = rms(renderMono(soft, 6000, sampleRate));
        check(softRms > 0.0 && softRms < loud, "a lower step velocity plays a softer hit");
        check(soft.stepVelocity(0, 0) > 0.25f && soft.stepVelocity(0, 0) < 0.35f,
              "step velocity round-trips through the grid");
    }
    // Out-of-range access is safe and reads false.
    check(!seq.step(-1, 0) && !seq.step(0, 999), "out-of-range steps read false");

    // --- Transport timing (120 BPM, 16ths @ 48 kHz → exactly 6000 samples/step) -------------
    seq.setBpm(120.0);
    seq.play();
    check(seq.playing(), "play() starts the transport");
    check(seq.currentStep() == 0, "transport starts on step 0");

    (void)renderMono(seq, 6000, sampleRate); // one step
    check(seq.currentStep() == 1, "one step of frames advances to step 1");

    for (int i = 0; i < 15; ++i) {
        (void)renderMono(seq, 6000, sampleRate);
    }
    check(seq.currentStep() == 0, "playhead wraps back to step 0 after a full bar");

    // Block size must not change the timing: 6000 frames split as 1000×6 still advances one step.
    audio::Sequencer seq2;
    seq2.setBpm(120.0);
    seq2.play();
    for (int i = 0; i < 6; ++i) {
        (void)renderMono(seq2, 1000, sampleRate);
    }
    check(seq2.currentStep() == 1, "stepping is block-size independent");

    seq2.stop();
    check(!seq2.playing(), "stop() freezes the transport");

    // --- Sound vs silence ----------------------------------------------------
    audio::Sequencer beat;
    beat.setBpm(120.0);
    beat.setStep(0, 0, true); // kick on the downbeat
    beat.play();
    const std::vector<float> firstStep = renderMono(beat, 6000, sampleRate);
    check(rms(firstStep) > 0.0, "a step with a kick produces sound");

    audio::Sequencer silent;
    silent.play(); // empty pattern
    const std::vector<float> nothing = renderMono(silent, 6000, sampleRate);
    check(rms(nothing) == 0.0, "an empty pattern renders silence");

    // --- Swing / groove ------------------------------------------------------
    {
        // At 120 BPM @ 48 kHz a straight step is 6000 samples. Swing 0.5 makes step 0 last
        // 9000 (1.5×) and step 1 last 3000 (0.5×), pushing the off-beat later while the pair still
        // sums to 12000.
        audio::Sequencer sw;
        sw.setBpm(120.0);
        sw.setSwing(0.5f);
        sw.play();
        (void)renderMono(sw, 6000, sampleRate);
        check(sw.currentStep() == 0, "swing lengthens the on-beat step (still step 0 at 6000)");
        (void)renderMono(sw, 3000, sampleRate); // total 9000
        check(sw.currentStep() == 1, "off-beat starts late (step 1 at 9000)");
        (void)renderMono(sw, 3000, sampleRate); // total 12000 → the short off-beat completed
        check(sw.currentStep() == 2, "the off-beat step is short (step 2 by 12000)");

        // Straight (swing 0) advances every 6000 samples.
        audio::Sequencer straight;
        straight.setBpm(120.0);
        straight.play();
        (void)renderMono(straight, 6000, sampleRate);
        check(straight.currentStep() == 1, "straight timing advances every 6000 samples");
    }

    // --- Per-channel mixer: volume / mute / solo ----------------------------
    {
        audio::Sequencer mix;
        mix.setBpm(120.0);
        mix.setStep(0, 0, true); // kick
        mix.setStep(1, 0, true); // snare, same step
        mix.play();
        const double full = rms(renderMono(mix, 6000, sampleRate));
        check(full > 0.0, "two channels produce sound");

        // Mute the kick → quieter than both.
        audio::Sequencer m2;
        m2.setBpm(120.0);
        m2.setStep(0, 0, true);
        m2.setStep(1, 0, true);
        m2.setChannelMute(0, true);
        m2.play();
        const double muted = rms(renderMono(m2, 6000, sampleRate));
        check(muted > 0.0 && muted < full, "muting a channel reduces the mix");

        // Mute both → silence.
        m2.setChannelMute(1, true);
        m2.play();
        check(rms(renderMono(m2, 6000, sampleRate)) == 0.0, "muting all channels is silent");

        // Solo the snare (kick also active) → only the snare sounds.
        audio::Sequencer s2;
        s2.setBpm(120.0);
        s2.setStep(0, 0, true);
        s2.setStep(1, 0, true);
        s2.setChannelSolo(1, true);
        s2.play();
        const double soloed = rms(renderMono(s2, 6000, sampleRate));
        check(soloed > 0.0 && soloed < full, "solo isolates one channel");

        // Volume 0 on the only active channel → silence.
        audio::Sequencer v2;
        v2.setBpm(120.0);
        v2.setStep(0, 0, true);
        v2.setChannelVolume(0, 0.0f);
        v2.play();
        check(rms(renderMono(v2, 6000, sampleRate)) == 0.0, "channel volume 0 is silent");

        // Pan: a hard-left channel is much louder on the left than the right.
        audio::Sequencer pan;
        pan.setBpm(120.0);
        pan.setStep(0, 0, true);
        pan.setChannelPan(0, -1.0f);
        pan.play();
        const std::vector<float> panned = renderMono(pan, 6000, sampleRate);
        check(rmsChannel(panned, 0) > rmsChannel(panned, 1) * 4.0, "hard-left pan favors the left");

        // Center pan is balanced.
        audio::Sequencer cen;
        cen.setBpm(120.0);
        cen.setStep(0, 0, true);
        cen.play();
        const std::vector<float> centered = renderMono(cen, 6000, sampleRate);
        check(std::fabs(rmsChannel(centered, 0) - rmsChannel(centered, 1)) < 1e-4,
              "center pan is balanced L/R");
    }

    // --- Humanize ------------------------------------------------------------
    {
        // Four identical kicks on the beats: without humanize their per-beat peaks are equal; with
        // humanize they vary. Deterministic, so the same render is reproducible.
        auto beatPeaks = [&](float amount) {
            audio::Sequencer h;
            h.setBpm(120.0);
            for (int s : {0, 4, 8, 12}) {
                h.setStep(0, s, true);
            }
            h.setHumanize(amount);
            h.play();
            const std::vector<float> out = renderMono(h, 16 * 6000, sampleRate);
            std::vector<float> peaks;
            for (int b = 0; b < 4; ++b) {
                float p = 0.0f;
                for (int i = b * 4 * 6000; i < b * 4 * 6000 + 6000; ++i) {
                    p = std::max(p, std::fabs(out[static_cast<size_t>(i) * 2]));
                }
                peaks.push_back(p);
            }
            return peaks;
        };
        const std::vector<float> flat = beatPeaks(0.0f);
        check(std::fabs(flat[0] - flat[1]) < 1e-4f && std::fabs(flat[0] - flat[3]) < 1e-4f,
              "without humanize, identical hits are equal");
        const std::vector<float> human = beatPeaks(0.9f);
        const bool varies = std::fabs(human[0] - human[1]) > 1e-3f ||
                            std::fabs(human[1] - human[2]) > 1e-3f ||
                            std::fabs(human[2] - human[3]) > 1e-3f;
        check(varies, "humanize makes identical hits vary in level");
    }

    // --- Arpeggiator ---------------------------------------------------------
    {
        // A held C-major triad (C E G, MIDI 60/64/67) across the bar, arp mode "up", should cycle
        // 60 → 64 → 67 → 60 … one pitch per step.
        audio::Sequencer arp;
        arp.setBpm(120.0);
        arp.roll().addNote(audio::Note{0, 16, 60, 1.0f});
        arp.roll().addNote(audio::Note{0, 16, 64, 1.0f});
        arp.roll().addNote(audio::Note{0, 16, 67, 1.0f});
        arp.setArp(true, 0); // up
        arp.play();          // strikes step 0
        check(arp.arpCurrentPitch() == 60, "arp step 0 plays the lowest held note");
        (void)renderMono(arp, 6000, sampleRate);
        check(arp.arpCurrentPitch() == 64, "arp advances up to the 2nd note");
        (void)renderMono(arp, 6000, sampleRate);
        check(arp.arpCurrentPitch() == 67, "arp advances up to the 3rd note");
        (void)renderMono(arp, 6000, sampleRate);
        check(arp.arpCurrentPitch() == 60, "arp wraps back to the lowest note");

        // Down mode reverses the order from the top.
        audio::Sequencer down;
        down.setBpm(120.0);
        down.roll().addNote(audio::Note{0, 16, 60, 1.0f});
        down.roll().addNote(audio::Note{0, 16, 64, 1.0f});
        down.roll().addNote(audio::Note{0, 16, 67, 1.0f});
        down.setArp(true, 1);
        down.play();
        check(down.arpCurrentPitch() == 67, "arp-down starts from the highest note");

        // Octave range: one held C4 with a 2-octave arp cycles C4 → C5 → C4.
        audio::Sequencer oct;
        oct.setBpm(120.0);
        oct.roll().addNote(audio::Note{0, 16, 60, 1.0f});
        oct.setArp(true, 0);
        oct.setArpOctaves(2);
        check(oct.arpOctaves() == 2, "arp octave range is settable");
        oct.play();
        check(oct.arpCurrentPitch() == 60, "octave arp starts at the root");
        (void)renderMono(oct, 6000, sampleRate);
        check(oct.arpCurrentPitch() == 72, "octave arp climbs an octave (C4 → C5)");
        (void)renderMono(oct, 6000, sampleRate);
        check(oct.arpCurrentPitch() == 60, "octave arp wraps back to the root");

        audio::Sequencer def;
        check(def.arpOctaves() == 1, "arp octave range defaults to 1");
    }

    // --- Sidechain ducking ---------------------------------------------------
    {
        // A kick on step 0 (muted so only the ducking is heard) ducks a sustained synth note; the
        // synth is quiet right after the kick and recovers over the release.
        audio::Sequencer sc;
        sc.setBpm(120.0);
        sc.setStep(0, 0, true);       // kick step (drives the sidechain)
        sc.setChannelMute(0, true);   // silence the kick itself
        sc.roll().addNote(audio::Note{0, 16, 60, 1.0f}); // sustained synth note across the bar
        sc.synth().setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        sc.setSidechain(true, 0.9f, 250.0f);
        sc.play();
        const std::vector<float> out = renderMono(sc, 16 * 6000, sampleRate);
        // Compare the synth level just after the kick vs. later in the bar.
        auto windowRms = [&](int fromFrame, int toFrame) {
            double s = 0.0;
            for (int i = fromFrame; i < toFrame; ++i) {
                const double v = static_cast<double>(out[static_cast<size_t>(i) * 2]);
                s += v * v;
            }
            return std::sqrt(s / static_cast<double>(toFrame - fromFrame));
        };
        const double early = windowRms(200, 1500);   // ducked
        const double late = windowRms(20000, 25000);  // recovered
        check(early < late * 0.7, "sidechain ducks the synth right after the kick");
        check(late > 0.0, "synth recovers after the duck");

        // Routable source: with the source set to the snare (channel 1), a hit on channel 1 — and
        // NOT the kick — drives the duck.
        audio::Sequencer sc2;
        sc2.setBpm(120.0);
        sc2.setStep(1, 0, true);     // snare step on channel 1
        sc2.setChannelMute(1, true); // silence the snare itself
        sc2.roll().addNote(audio::Note{0, 16, 60, 1.0f});
        sc2.synth().setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        sc2.setSidechain(true, 0.9f, 250.0f);
        sc2.setSidechainSource(1); // duck from the snare, not the kick
        check(sc2.sidechainSource() == 1, "sidechain source is settable");
        sc2.play();
        const std::vector<float> out2 = renderMono(sc2, 16 * 6000, sampleRate);
        auto win2 = [&](int a, int b) {
            double s = 0.0;
            for (int i = a; i < b; ++i) {
                const double v = static_cast<double>(out2[static_cast<size_t>(i) * 2]);
                s += v * v;
            }
            return std::sqrt(s / static_cast<double>(b - a));
        };
        check(win2(200, 1500) < win2(20000, 25000) * 0.7,
              "the routed (snare) channel drives the sidechain duck");
    }

    // --- Arrangement: patterns + playlist -----------------------------------
    audio::Sequencer arr;
    check(arr.patternCount() == 1, "starts with one pattern");
    arr.setStep(0, 0, true); // kick on step 0 of pattern 0
    const int p1 = arr.addPattern();
    check(p1 == 1 && arr.patternCount() == 2, "addPattern appends");
    arr.selectPattern(p1);
    check(!arr.step(0, 0), "a fresh pattern is empty (independent grid)");
    arr.setStep(1, 0, true); // snare on step 0 of pattern 1
    arr.selectPattern(0);
    check(arr.step(0, 0) && !arr.step(1, 0), "patterns keep independent grids");

    // Song mode: playlist [0,1] should switch the current pattern at each bar boundary.
    arr.setBpm(120.0); // 6000 samples/step → 96000 samples/bar (16 steps)
    arr.setPlaylist({0, 1});
    arr.setSongMode(true);
    arr.play();
    check(arr.currentPattern() == 0, "song mode starts on the first playlist pattern");
    // Render one full bar (16 steps × 6000).
    (void)renderMono(arr, 16 * 6000, sampleRate);
    check(arr.currentPattern() == 1, "advances to the next playlist pattern after a bar");
    (void)renderMono(arr, 16 * 6000, sampleRate);
    check(arr.currentPattern() == 0, "playlist wraps back to the start");

    // Play-once: with song loop off, the transport stops at the end of the playlist.
    {
        audio::Sequencer once;
        once.setBpm(120.0);
        once.setPlaylist({0, 1});
        once.setSongMode(true);
        once.setSongLoop(false);
        check(!once.songLoop(), "song loop is settable off");
        once.play();
        (void)renderMono(once, 16 * 6000, sampleRate); // bar 1 → entry 1
        check(once.playing(), "play-once still playing during the arrangement");
        (void)renderMono(once, 16 * 6000, sampleRate); // bar 2 ends → would wrap → stop
        check(!once.playing(), "play-once stops at the end of the playlist");

        // With loop on (default), it keeps playing past the end.
        audio::Sequencer looped;
        looped.setBpm(120.0);
        looped.setPlaylist({0, 1});
        looped.setSongMode(true);
        looped.play();
        (void)renderMono(looped, 3 * 16 * 6000, sampleRate); // three bars
        check(looped.playing(), "a looping song keeps playing past the playlist end");
        check(looped.songLoop(), "song loop defaults to on");
    }

    // --- Metronome: accented clicks on each beat ------------------------------
    {
        // Empty pattern → silent except for the metronome. At 120 BPM a beat is 0.5 s = 24000
        // frames; over 2 s we expect 4 beats (4 clicks).
        audio::Sequencer metro;
        metro.setBpm(120.0);
        metro.clear();
        metro.roll().clear();
        metro.setMetronome(true);
        metro.play();
        const int frames = sampleRate * 2; // 2 seconds
        const std::vector<float> out = renderMono(metro, frames, sampleRate);
        check(rms(out) > 0.0, "metronome produces sound on an empty pattern");

        // Count click onsets: windows (per 24000-frame beat) that contain energy.
        int clicks = 0;
        const int beatFrames = 24000;
        for (int b = 0; b < 4; ++b) {
            double e = 0.0;
            // The click sits at the start of each beat window (~first 40 ms).
            for (int i = b * beatFrames; i < b * beatFrames + 3000 && i < frames; ++i) {
                e += static_cast<double>(out[static_cast<size_t>(i) * 2]) *
                     static_cast<double>(out[static_cast<size_t>(i) * 2]);
            }
            if (e > 1e-4) {
                ++clicks;
            }
        }
        check(clicks == 4, "metronome clicks once per beat (4 beats in 2 s @120 BPM)");

        // Between clicks (e.g. mid-beat) it is silent.
        double midEnergy = 0.0;
        for (int i = 12000; i < 20000; ++i) {
            midEnergy += static_cast<double>(out[static_cast<size_t>(i) * 2]) *
                         static_cast<double>(out[static_cast<size_t>(i) * 2]);
        }
        check(midEnergy < 1e-6, "metronome is silent between clicks");

        // Disabled → silence on an empty pattern.
        audio::Sequencer off;
        off.setMetronome(false);
        off.play();
        const std::vector<float> q = renderMono(off, sampleRate, sampleRate);
        check(rms(q) == 0.0, "metronome off leaves an empty pattern silent");
    }

    // --- Per-note probability -------------------------------------------------
    {
        // Count how many of N loops a lead note actually sounds at a given probability.
        auto soundedLoops = [&](float prob, int loops) {
            audio::Sequencer s;
            s.setBpm(120.0);
            audio::Note n{0, 2, 60, 1.0f, prob};
            s.roll().addNote(n);
            s.synth().setEnvelope(0.001f, 0.01f, 0.9f, 0.02f);
            s.play();
            int sounded = 0;
            const int loopFrames = 16 * 6000; // one 16-step bar @120 BPM
            for (int l = 0; l < loops; ++l) {
                const std::vector<float> out = renderMono(s, loopFrames, sampleRate);
                double e = 0.0;
                for (int i = 0; i < 3000; ++i) {
                    e += static_cast<double>(out[static_cast<size_t>(i) * 2]) *
                         static_cast<double>(out[static_cast<size_t>(i) * 2]);
                }
                if (e > 1e-4) {
                    ++sounded;
                }
            }
            return sounded;
        };

        check(soundedLoops(1.0f, 8) == 8, "a note at probability 1.0 always sounds");
        check(soundedLoops(0.0f, 8) == 0, "a note at probability 0.0 never sounds");
        const int half = soundedLoops(0.5f, 32);
        check(half > 4 && half < 28, "a note at probability 0.5 sounds some loops but not all");

        audio::Note def;
        check(def.probability == 1.0f, "notes default to probability 1.0");
    }

    // --- Global transpose ----------------------------------------------------
    {
        // A held A3 (220 Hz) note; transposing up an octave should render ~440 Hz.
        auto pitchHz = [&](int transpose) {
            audio::Sequencer s;
            s.setBpm(120.0);
            s.roll().addNote(audio::Note{0, 16, 57, 1.0f}); // A3 = 220 Hz
            s.synth().setWaveform(audio::Waveform::Saw);
            s.synth().setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
            s.synth().setFilter(20000.0f, 0.7f, 0.0f);
            s.setTranspose(transpose);
            s.play();
            const std::vector<float> out = renderMono(s, 6000, sampleRate);
            // Rising zero-crossings on the left channel → fundamental frequency.
            int crossings = 0;
            for (size_t i = 1; i < 6000; ++i) {
                if (out[(i - 1) * 2] <= 0.0f && out[i * 2] > 0.0f) {
                    ++crossings;
                }
            }
            return static_cast<double>(crossings) * sampleRate / 6000.0;
        };
        check(std::fabs(pitchHz(0) - 220.0) < 6.0, "no transpose plays the written pitch");
        check(std::fabs(pitchHz(12) - 440.0) < 12.0, "transpose +12 raises the note an octave");
        check(std::fabs(pitchHz(-12) - 110.0) < 6.0, "transpose -12 lowers the note an octave");

        audio::Sequencer def;
        check(def.transpose() == 0, "transpose defaults to 0");
    }

    // --- Clone pattern: an independent copy ----------------------------------
    {
        audio::Sequencer s;
        s.setStep(0, 0, true);
        s.setStep(1, 4, true);
        s.setStepProbability(1, 4, 0.5f);
        s.setStepRatchet(0, 0, 3);
        s.roll().addNote(audio::Note{2, 4, 64, 0.7f});
        s.setPatternName(0, "Verse");

        const int c = s.clonePattern(0);
        check(s.patternCount() == 2 && c == 1, "clone appends a new pattern");
        check(s.patternName(1) == "Verse copy", "clone names itself '<src> copy'");

        s.selectPattern(1);
        check(s.step(0, 0) && s.step(1, 4), "clone copies the drum steps");
        check(std::fabs(s.stepProbability(1, 4) - 0.5f) < 0.01f && s.stepRatchet(0, 0) == 3,
              "clone copies per-step probability and ratchet");
        check(s.roll().notes().size() == 1 && s.roll().notes()[0].pitch == 64,
              "clone copies the piano-roll notes");

        // Editing the clone does not touch the original.
        s.setStep(3, 8, true);
        s.selectPattern(0);
        check(!s.step(3, 8), "editing the clone leaves the original untouched");
    }

    // --- Melodic bus pan: place the lead in the stereo field -----------------
    {
        auto leadLR = [&](float pan) {
            audio::Sequencer s;
            s.setBpm(120.0);
            s.roll().addNote(audio::Note{0, 16, 60, 1.0f}); // a sustained lead note
            s.synth().setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
            s.setLeadPan(pan);
            s.play();
            const std::vector<float> out = renderMono(s, 6000, sampleRate);
            return std::make_pair(rmsChannel(out, 0), rmsChannel(out, 1));
        };

        const auto center = leadLR(0.0f);
        check(std::fabs(center.first - center.second) < 1e-4, "lead pan 0 is centered (L == R)");

        const auto left = leadLR(-1.0f);
        check(left.first > left.second * 5.0, "lead pan -1 sends the lead to the left channel");

        const auto right = leadLR(1.0f);
        check(right.second > right.first * 5.0, "lead pan +1 sends the lead to the right channel");

        // Bass pan is independent and defaults to center.
        audio::Sequencer d;
        check(d.leadPan() == 0.0f && d.bassPan() == 0.0f, "melodic buses default to center");
    }

    // --- Steps per beat: grid subdivision ------------------------------------
    {
        // At 120 BPM a beat is 0.5 s. With 4 steps/beat a step is 6000 frames; with 8 steps/beat a
        // step is 3000 frames. Advancing one step's worth of frames should land on step 1 either way.
        audio::Sequencer a;
        a.setBpm(120.0);
        check(a.stepsPerBeat() == 4, "default is 4 steps per beat");
        a.play();
        (void)renderMono(a, 6000, sampleRate);
        check(a.currentStep() == 1, "at 4 steps/beat one step is 6000 frames @120 BPM");

        audio::Sequencer b;
        b.setBpm(120.0);
        b.setStepsPerBeat(8);
        check(b.stepsPerBeat() == 8, "steps-per-beat is settable");
        b.play();
        (void)renderMono(b, 3000, sampleRate);
        check(b.currentStep() == 1, "at 8 steps/beat one step is 3000 frames @120 BPM");
        (void)renderMono(b, 3000, sampleRate);
        check(b.currentStep() == 2, "the finer grid keeps advancing evenly");

        // Clamped to 1–8.
        audio::Sequencer c;
        c.setStepsPerBeat(99);
        check(c.stepsPerBeat() == 8, "steps-per-beat clamps to 8");
        c.setStepsPerBeat(0);
        check(c.stepsPerBeat() == 1, "steps-per-beat clamps to 1");
    }

    // --- Pattern length: resizable step count --------------------------------
    {
        audio::Sequencer s;
        check(s.numSteps() == 16, "default pattern length is 16");
        s.setStep(0, 4, true);
        s.setStep(1, 12, true);
        s.setStepVelocity(2, 3, 0.5f);

        s.setNumSteps(32);
        check(s.numSteps() == 32, "pattern grows to 32 steps");
        check(s.step(0, 4) && s.step(1, 12), "existing steps survive a grow");
        check(!s.step(0, 20), "new steps start empty");
        s.setStep(0, 24, true); // a step only reachable at the longer length
        check(s.step(0, 24), "steps in the extended range are usable");

        s.setNumSteps(8);
        check(s.numSteps() == 8, "pattern shrinks to 8 steps");
        check(s.step(0, 4), "steps within the new length survive a shrink");
        s.setNumSteps(16);
        check(!s.step(1, 12), "steps beyond a shrink are gone (not restored)");

        // A 32-step pattern loops over its full length: at 120 BPM a step is 6000 frames, so 32
        // steps advance the transport through step 31 and wrap to 0.
        audio::Sequencer t;
        t.setNumSteps(32);
        t.setBpm(120.0);
        t.play();
        (void)renderMono(t, 31 * 6000, sampleRate);
        check(t.currentStep() == 31, "32-step pattern reaches step 31");
        (void)renderMono(t, 6000, sampleRate);
        check(t.currentStep() == 0, "32-step pattern wraps after its full length");
    }

    // --- Drum decay: the tail length scales ----------------------------------
    {
        // Count how long a kick stays active for a short vs long decay multiplier.
        auto activeFrames = [&](float mul) {
            audio::DrumVoice k;
            k.setType(audio::Drum::Kick);
            k.setDecay(mul);
            k.trigger(1.0f);
            int frames = 0;
            while (k.active() && frames < sampleRate) {
                std::vector<float> b(64, 0.0f);
                k.render(b.data(), 64, sampleRate);
                frames += 64;
            }
            return frames;
        };
        const int shortT = activeFrames(0.5f);
        const int longT = activeFrames(3.0f);
        check(longT > shortT * 2, "a longer decay multiplier lengthens the drum tail");

        audio::Sequencer d;
        check(d.channelDecay(0) == 1.0f, "channel decay defaults to 1");
    }

    // --- Drum tuning: a tuned kick shifts pitch ------------------------------
    {
        // A kick's fundamental rises when tuned up. Estimate its pitch from the sustained tail.
        auto kickHz = [&](float semis) {
            audio::DrumVoice k;
            k.setType(audio::Drum::Kick);
            k.setTune(semis);
            k.trigger(1.0f);
            std::vector<float> buf(4800, 0.0f);
            k.render(buf.data(), 4800, sampleRate);
            // Zero-crossing rate over the body (skip the initial pitch sweep).
            int crossings = 0;
            for (int i = 1201; i < 4800; ++i) {
                if (buf[static_cast<size_t>(i - 1)] <= 0.0f && buf[static_cast<size_t>(i)] > 0.0f) {
                    ++crossings;
                }
            }
            return static_cast<double>(crossings) * sampleRate / (4800 - 1200);
        };
        const double base = kickHz(0.0f);
        const double up = kickHz(12.0f); // +1 octave
        check(base > 0.0, "kick has a measurable pitch");
        check(up > base * 1.6, "tuning the kick up an octave raises its pitch");

        audio::Sequencer d;
        check(d.channelTune(0) == 0.0f, "channels default to 0 tune");
    }

    // --- Drum drive: saturation adds harmonics -------------------------------
    {
        auto renderKick = [&](float drive) {
            audio::DrumVoice k;
            k.setType(audio::Drum::Kick);
            k.setDrive(drive);
            k.trigger(1.0f);
            std::vector<float> buf(4800, 0.0f);
            k.render(buf.data(), 4800, sampleRate);
            return buf;
        };
        // Level-independent brightness: HF (first-difference) energy over total energy.
        auto brightness = [](const std::vector<float>& b) {
            double hf = 0.0, en = 0.0;
            for (size_t i = 1; i < b.size(); ++i) {
                const double d = static_cast<double>(b[i] - b[i - 1]);
                hf += d * d;
                en += static_cast<double>(b[i]) * b[i];
            }
            return en > 0.0 ? hf / en : 0.0;
        };
        const std::vector<float> clean = renderKick(0.0f);
        const std::vector<float> driven = renderKick(0.9f);
        check(brightness(driven) > brightness(clean) * 1.2, "drum drive adds harmonics (brighter)");

        audio::DrumVoice dv;
        check(dv.drive() == 0.0f, "drum drive defaults to 0 (clean)");
        audio::Sequencer d;
        check(d.channelDrive(0) == 0.0f, "channel drive defaults to 0");
    }

    // --- Flam: a grace hit followed by the main hit --------------------------
    {
        auto renderFlam = [&](float flamMs) {
            audio::Sequencer s;
            s.setBpm(120.0);
            s.setStep(2, 0, true); // closed hat — short, so a second onset is easy to see
            s.setChannelFlam(2, flamMs);
            s.play();
            return renderMono(s, 6000, sampleRate);
        };
        auto energyAt = [](const std::vector<float>& out, int start, int len) {
            double e = 0.0;
            for (int i = start; i < start + len && i < 6000; ++i) {
                e += static_cast<double>(out[static_cast<size_t>(i) * 2]) *
                     static_cast<double>(out[static_cast<size_t>(i) * 2]);
            }
            return e;
        };
        const std::vector<float> noFlam = renderFlam(0.0f);
        const std::vector<float> flam = renderFlam(30.0f); // 30 ms → main hit ~1440 frames in
        const double ref = energyAt(noFlam, 0, 500);
        check(ref > 1e-4, "the flam grace/first hit sounds at the step start");
        // Without flam the hat has decayed by ~1440 frames; with flam the full hit lands there.
        check(energyAt(noFlam, 1440, 500) < ref * 0.2, "no flam → single hit, decayed by 30 ms");
        check(energyAt(flam, 1440, 500) > ref * 0.3, "flam fires the main hit after the grace");

        audio::Sequencer dd;
        check(dd.channelFlam(0) == 0.0f, "channel flam defaults to 0 (off)");
    }

    // --- Tom voice + per-channel drum type -----------------------------------
    {
        audio::DrumVoice tom;
        tom.setType(audio::Drum::Tom);
        tom.trigger(1.0f);
        std::vector<float> buf(4800, 0.0f);
        tom.render(buf.data(), 4800, sampleRate);
        check(rms(buf) > 0.0, "tom produces sound");
        // Tom is tonal — count zero crossings over the sustained body (skip the initial sweep).
        int crossings = 0;
        for (int i = 1201; i < 4800; ++i) {
            if (buf[static_cast<size_t>(i - 1)] <= 0.0f && buf[static_cast<size_t>(i)] > 0.0f) {
                ++crossings;
            }
        }
        check(crossings > 0, "tom has a measurable pitch");

        // Per-channel type selection: any row can be reassigned to any drum voice.
        audio::Sequencer s;
        s.setChannelType(0, audio::Drum::Tom);
        check(s.channelType(0) == audio::Drum::Tom, "a channel's drum type can be reassigned");
        audio::Sequencer d;
        check(d.channelType(0) == audio::Drum::Kick, "channel 0 defaults to the kick");
    }

    // --- Channel rotate: shift a step row around the bar ---------------------
    {
        audio::Sequencer s;
        s.setStep(0, 0, true);
        s.setStep(0, 1, true);
        s.setStepRatchet(0, 0, 3); // travels with step 0
        s.rotateChannel(0, 2);     // shift later by 2
        check(!s.step(0, 0) && !s.step(0, 1) && s.step(0, 2) && s.step(0, 3),
              "rotate shifts the step row later");
        check(s.stepRatchet(0, 2) == 3, "rotate carries the per-step ratchet with its step");

        // Wraparound: the last step rolls to the front.
        audio::Sequencer w;
        const int last = w.numSteps() - 1;
        w.setStep(0, last, true);
        w.rotateChannel(0, 1);
        check(w.step(0, 0) && !w.step(0, last), "rotate wraps around the pattern length");

        // Negative offset shifts earlier.
        audio::Sequencer nb;
        nb.setStep(0, 2, true);
        nb.rotateChannel(0, -2);
        check(nb.step(0, 0) && !nb.step(0, 2), "a negative rotate shifts the row earlier");

        // A full-length rotate is a no-op.
        audio::Sequencer fr;
        fr.setStep(0, 5, true);
        fr.rotateChannel(0, fr.numSteps());
        check(fr.step(0, 5), "rotating by the full length leaves the row unchanged");
    }

    // --- Euclidean fill: evenly distributed pulses ---------------------------
    {
        audio::Sequencer s; // default 16 steps
        // 4 pulses over 16 steps → a hit every 4 steps (0, 4, 8, 12).
        const int placed = s.euclidFill(0, 4);
        check(placed == 4, "euclid places exactly the requested pulse count");
        check(s.step(0, 0) && s.step(0, 4) && s.step(0, 8) && s.step(0, 12),
              "4 pulses over 16 steps land on the quarter beats");
        check(!s.step(0, 1) && !s.step(0, 5), "off-grid steps stay empty");

        // The hit count always matches the requested pulses (even for uneven divisions).
        bool countsMatch = true;
        for (int k = 0; k <= 16; ++k) {
            audio::Sequencer e;
            if (e.euclidFill(0, k) != k) {
                countsMatch = false;
            }
        }
        check(countsMatch, "euclid hit count matches the requested pulses for every k in 0..16");

        // A previous pattern is replaced, and 0 pulses clears the row.
        audio::Sequencer c;
        c.setStep(0, 3, true);
        c.euclidFill(0, 0);
        check(!c.step(0, 3), "a zero-pulse euclid fill clears the row");
    }

    // --- Choke: a voice can be silenced mid-ring -----------------------------
    {
        audio::DrumVoice oh;
        oh.setType(audio::Drum::OpenHat); // long tail (~0.28 s)
        oh.trigger(1.0f);
        std::vector<float> a(480, 0.0f);
        oh.render(a.data(), 480, sampleRate); // 10 ms
        check(oh.active() && rms(a) > 0.0, "open hat rings after trigger");
        oh.choke();
        std::vector<float> b(960, 0.0f);
        oh.render(b.data(), 960, sampleRate); // 20 ms — the ~4 ms choke fade completes
        check(!oh.active(), "a choked voice goes silent");
    }

    // --- Choke groups: closed hat cuts off open hat --------------------------
    {
        auto tailEnergy = [&](bool chokeOn) {
            audio::Sequencer s;
            s.setBpm(120.0);           // step = 6000 frames
            s.setStep(3, 0, true);     // open hat on step 0 (long tail)
            s.setStep(2, 1, true);     // closed hat on step 1
            s.setChannelChokeGroup(3, chokeOn ? 1 : 0);
            s.setChannelChokeGroup(2, chokeOn ? 1 : 0);
            s.play();
            const std::vector<float> out = renderMono(s, 12000, sampleRate);
            // Energy in the open-hat tail AFTER the closed hat hits (well past step 1's onset).
            double e = 0.0;
            for (int i = 7200; i < 12000; ++i) {
                e += static_cast<double>(out[static_cast<size_t>(i) * 2]) *
                     static_cast<double>(out[static_cast<size_t>(i) * 2]);
            }
            return e;
        };
        const double choked = tailEnergy(true);
        const double open = tailEnergy(false);
        check(choked < open * 0.5, "a choke group cuts the open-hat tail when the closed hat hits");

        // Default kit already puts the two hats in a choke group.
        audio::Sequencer d;
        check(d.channelChokeGroup(2) == d.channelChokeGroup(3) && d.channelChokeGroup(2) != 0,
              "default kit chokes the closed and open hats together");
    }

    // --- Per-step ratchet -----------------------------------------------------
    {
        // A step slot is 6000 frames @120 BPM. A ratchet of R places hits at 0, 6000/R, 2·6000/R…
        // Render one slot and measure energy in a short window at each expected hit position.
        auto render1 = [&](int ratchet) {
            audio::Sequencer s;
            s.setBpm(120.0);
            s.setStep(2, 0, true); // closed hat — short, decays fast between ratchet hits
            s.setStepRatchet(2, 0, ratchet);
            s.play();
            return renderMono(s, 6000, sampleRate);
        };
        auto energyAt = [](const std::vector<float>& out, int start, int len) {
            double e = 0.0;
            for (int i = start; i < start + len && i < 6000; ++i) {
                e += static_cast<double>(out[static_cast<size_t>(i) * 2]) *
                     static_cast<double>(out[static_cast<size_t>(i) * 2]);
            }
            return e;
        };

        const std::vector<float> one = render1(1);
        const std::vector<float> four = render1(4);
        const std::vector<float> two = render1(2);
        const double ref = energyAt(one, 0, 500); // a single hat hit's onset energy

        check(ref > 1e-4, "non-ratcheted step hits at the start of the slot");
        // The single hat has decayed away well before the quarter points.
        check(energyAt(one, 1500, 500) < ref * 0.2 && energyAt(one, 3000, 500) < ref * 0.2,
              "a single hat has no extra hits mid-slot");

        // 4× ratchet: a fresh hat at each quarter (0, 1500, 3000, 4500).
        check(energyAt(four, 1500, 500) > ref * 0.3 && energyAt(four, 3000, 500) > ref * 0.3 &&
                  energyAt(four, 4500, 500) > ref * 0.3,
              "4x ratchet fires fresh hits at each quarter of the step");

        // 2× ratchet: a fresh hat at the half-way point (3000), but not at the 1500 quarter.
        check(energyAt(two, 3000, 500) > ref * 0.3 && energyAt(two, 1500, 500) < ref * 0.2,
              "2x ratchet fires at the half, not the quarter, of the step");

        audio::Sequencer d;
        check(d.stepRatchet(0, 0) == 1, "steps default to ratchet 1");
    }

    // --- Per-step probability -------------------------------------------------
    {
        // Count how many times a kick on step 0 fires over N bars at a given probability.
        auto countHits = [&](float prob, int bars) {
            audio::Sequencer s;
            s.setBpm(120.0);
            s.setStep(0, 0, true);
            s.setStepProbability(0, 0, prob);
            s.play();
            int hits = 0;
            const int barFrames = 16 * 6000; // 16 steps × 6000 frames @120 BPM
            for (int b = 0; b < bars; ++b) {
                const std::vector<float> out = renderMono(s, barFrames, sampleRate);
                // The kick sits at the start of the bar; energy in the first 3000 frames = a hit.
                double e = 0.0;
                for (int i = 0; i < 3000; ++i) {
                    e += static_cast<double>(out[static_cast<size_t>(i) * 2]) *
                         static_cast<double>(out[static_cast<size_t>(i) * 2]);
                }
                if (e > 1e-4) {
                    ++hits;
                }
            }
            return hits;
        };

        check(countHits(1.0f, 8) == 8, "probability 1.0 fires every bar");
        check(countHits(0.0f, 8) == 0, "probability 0.0 never fires");
        const int half = countHits(0.5f, 32);
        check(half > 4 && half < 28, "probability 0.5 fires some bars but not all");

        // Default probability is 1.0 (always).
        audio::Sequencer d;
        check(d.stepProbability(0, 0) == 1.0f, "steps default to probability 1.0");
    }

    // --- Count-in: a bar of clicks before the pattern starts ------------------
    {
        // A loud kick on every step; with a 1-bar count-in, the pattern must stay silent (clicks
        // only) for the first bar, then the kick sounds. At 120 BPM a 16-step bar is 96000 frames.
        audio::Sequencer ci;
        ci.setBpm(120.0);
        for (int s = 0; s < ci.numSteps(); ++s) {
            ci.setStep(0, s, true); // kick on every step
        }
        ci.setCountInBars(1);
        ci.play();
        check(ci.countingIn(), "count-in is active right after play()");

        const int bar = 96000;
        const std::vector<float> firstBar = renderMono(ci, bar, sampleRate);
        check(!ci.countingIn(), "count-in ends after one bar");
        const std::vector<float> secondBar = renderMono(ci, bar, sampleRate);

        // Both bars have sound, but bar 2 (kicks + clicks) is far louder than bar 1 (clicks only).
        const double e1 = rms(firstBar);
        const double e2 = rms(secondBar);
        check(e1 > 0.0, "count-in bar plays clicks");
        check(e2 > e1 * 3.0, "the pattern (kicks) only sounds after the count-in");
    }

    // --- Per-bus stems sum back to the mixed render (behaviour preservation) ---
    {
        // One sequencer renders the mixed output; an identically-programmed one renders the three
        // stems, and tanh(drums + lead + bass) must match the mixed render sample-for-sample.
        auto program = [&](audio::Sequencer& s) {
            s.setBpm(128.0);
            s.setStep(0, 0, true);
            s.setStep(2, 2, true);
            s.roll().addNote(audio::Note{0, 4, 60, 0.9f});
            s.roll2().addNote(audio::Note{0, 8, 40, 0.8f});
            s.synth().setEnvelope(0.002f, 0.05f, 0.7f, 0.1f);
            s.synth2().setEnvelope(0.002f, 0.05f, 0.7f, 0.1f);
            s.play();
        };
        const int frames = 12000;
        audio::Sequencer mixed;
        program(mixed);
        std::vector<float> mixOut(static_cast<size_t>(frames) * 2, 0.0f);
        mixed.render(mixOut.data(), frames, sampleRate);

        audio::Sequencer stems;
        program(stems);
        std::vector<float> d(static_cast<size_t>(frames) * 2, 0.0f);
        std::vector<float> l(static_cast<size_t>(frames) * 2, 0.0f);
        std::vector<float> b(static_cast<size_t>(frames) * 2, 0.0f);
        stems.renderStems(d.data(), l.data(), b.data(), frames, sampleRate);

        double maxDiff = 0.0;
        for (size_t i = 0; i < mixOut.size(); ++i) {
            const float summed =
                static_cast<float>(std::tanh(static_cast<double>(d[i]) + static_cast<double>(l[i]) +
                                             static_cast<double>(b[i])));
            maxDiff = std::max(maxDiff, std::fabs(static_cast<double>(mixOut[i] - summed)));
        }
        check(maxDiff < 1e-6, "tanh(drums+lead+bass) stems equal the mixed render");

        // The stems are genuinely separate: drums carry energy, lead carries energy.
        check(rms(d) > 0.0 && rms(l) > 0.0, "drums and lead stems each carry sound");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
