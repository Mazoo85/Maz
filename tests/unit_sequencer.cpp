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
