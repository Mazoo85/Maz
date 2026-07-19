// Unit tests for the A3 mixer + effects — pure DSP, no audio device. Each effect is exercised on a
// known stereo signal and checked for the behaviour that defines it (echo tap, gain reduction,
// high-cut, reverb tail) plus the mixer's master gain and transparent-by-default passthrough.

#include "maz/audio/Effects.hpp"
#include "maz/audio/Mixer.hpp"

#include <cmath>
#include <cstdint>
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

constexpr double kTwoPi = 6.283185307179586;

// Build an interleaved-stereo sine of `freq` Hz and amplitude `amp` for `frames` frames.
std::vector<float> sineStereo(int frames, double freq, double amp, int sampleRate) {
    std::vector<float> b(static_cast<size_t>(frames) * 2, 0.0f);
    for (int i = 0; i < frames; ++i) {
        const float s = static_cast<float>(amp * std::sin(kTwoPi * freq * i / sampleRate));
        b[static_cast<size_t>(i) * 2] = s;
        b[static_cast<size_t>(i) * 2 + 1] = s;
    }
    return b;
}

float peakRange(const std::vector<float>& b, int fromFrame, int toFrame) {
    float p = 0.0f;
    for (int i = fromFrame; i < toFrame; ++i) {
        p = std::max(p, std::fabs(b[static_cast<size_t>(i) * 2]));
    }
    return p;
}

double rms(const std::vector<float>& b) {
    if (b.empty()) {
        return 0.0;
    }
    double s = 0.0;
    for (float v : b) {
        s += static_cast<double>(v) * static_cast<double>(v);
    }
    return std::sqrt(s / static_cast<double>(b.size()));
}

} // namespace

int main() {
    const int sr = 48000;

    // --- Delay: an impulse re-appears one delay-time later --------------------
    {
        audio::Delay delay;
        delay.setEnabled(true);
        delay.setTime(100.0f); // 100 ms → 4800 frames
        delay.setFeedback(0.4f);
        delay.setMix(1.0f); // fully wet: only the echoes remain
        std::vector<float> buf(static_cast<size_t>(sr) / 2 * 2, 0.0f); // 0.5 s
        buf[0] = 1.0f;
        buf[1] = 1.0f;
        delay.process(buf.data(), sr / 2, sr);
        check(std::fabs(buf[0]) < 0.01f, "wet delay removes the dry impulse at t=0");
        check(std::fabs(buf[static_cast<size_t>(4800) * 2]) > 0.5f, "echo appears at the delay time");
    }

    // --- Delay damping: repeats lose their high end -------------------------
    {
        // Feed a bright buzzy tone; with damping the wet echoes carry less high-frequency energy.
        auto wetHF = [&](float damp) {
            audio::Delay d;
            d.setEnabled(true);
            d.setTime(50.0f);
            d.setFeedback(0.7f);
            d.setMix(1.0f); // fully wet: measure only the (filtered) echoes
            d.setDamping(damp);
            std::vector<float> b = sineStereo(sr / 2, 3000.0, 0.5, sr); // 3 kHz, 0.5 s
            d.process(b.data(), sr / 2, sr);
            double s = 0.0; // first-difference energy ~ high-frequency content
            for (size_t i = 2; i < b.size(); i += 2) {
                const double diff = static_cast<double>(b[i] - b[i - 2]);
                s += diff * diff;
            }
            return s;
        };
        check(wetHF(0.7f) < wetHF(0.0f) * 0.7, "delay damping rolls off the echoes' high end");
    }

    // --- Delay modulation: the repeats wobble in pitch (analog character) ----
    {
        // Std-dev of the wet's per-window frequency: a modulated delay makes it wobble; a clean one
        // holds steady. (10 windows across a 1 s render of a wet-only 300 Hz tone.)
        auto freqWobble = [&](float modDepth) {
            audio::Delay d;
            d.setEnabled(true);
            d.setTime(20.0f);
            d.setFeedback(0.0f); // a single clean echo
            d.setMix(1.0f);      // wet only
            d.setModDepth(modDepth);
            d.setModRate(3.0f);
            std::vector<float> b = sineStereo(sr, 300.0, 0.6, sr); // 1 s, 300 Hz
            d.process(b.data(), sr, sr);
            const int win = sr / 10;
            std::vector<double> freqs;
            for (int w = 0; w < 10; ++w) {
                int cross = 0;
                const int a = w * win + sr / 20; // skip the first 50 ms (initial delay fill)
                const int e = (w + 1) * win;
                for (int i = a + 1; i < e; ++i) {
                    const float p = b[static_cast<size_t>(2 * (i - 1))];
                    const float c = b[static_cast<size_t>(2 * i)];
                    if ((p <= 0.0f && c > 0.0f) || (p >= 0.0f && c < 0.0f)) {
                        ++cross;
                    }
                }
                freqs.push_back(static_cast<double>(cross) / 2.0 /
                                (static_cast<double>(e - a - 1) / sr));
            }
            double mean = 0.0;
            for (double f : freqs) {
                mean += f;
            }
            mean /= static_cast<double>(freqs.size());
            double var = 0.0;
            for (double f : freqs) {
                var += (f - mean) * (f - mean);
            }
            return std::sqrt(var / static_cast<double>(freqs.size()));
        };
        const double clean = freqWobble(0.0f);
        const double modded = freqWobble(6.0f);
        check(modded > clean + 3.0, "delay modulation wobbles the repeats' pitch (higher freq spread)");
    }

    // --- Delay feedback low-cut: repeats shed their low end ------------------
    {
        // A short low-frequency burst then silence, echoed with high feedback. With the feedback
        // low-cut engaged each repeat loses more of its lows, so the late echoes carry less energy.
        auto lateEnergy = [&](float lowCut) {
            audio::Delay d;
            d.setEnabled(true);
            d.setTime(50.0f);
            d.setFeedback(0.85f);
            d.setMix(1.0f); // fully wet: measure only the echoes
            d.setFeedbackLowCut(lowCut);
            std::vector<float> b(static_cast<size_t>(sr / 2) * 2, 0.0f); // 0.5 s of silence
            const int burst = sr / 20;                                   // a 50 ms 100 Hz burst up front
            for (int i = 0; i < burst; ++i) {
                const float s = static_cast<float>(0.5 * std::sin(kTwoPi * 100.0 * i / sr));
                b[static_cast<size_t>(i) * 2] = s;
                b[static_cast<size_t>(i) * 2 + 1] = s;
            }
            d.process(b.data(), sr / 2, sr);
            double e = 0.0; // energy in the last ~140 ms (several echoes in)
            for (int i = sr / 2 - sr / 7; i < sr / 2; ++i) {
                e += static_cast<double>(b[static_cast<size_t>(i) * 2]) * b[static_cast<size_t>(i) * 2];
            }
            return e;
        };
        check(lateEnergy(400.0f) < lateEnergy(0.0f) * 0.7,
              "delay feedback low-cut thins the echoes' low end over repeats");
        audio::Delay dd;
        check(dd.feedbackLowCut() == 0.0f, "delay feedback low-cut defaults to off");

        // Feedback drive: as the repeats build under high feedback, the tanh saturates them and adds
        // harmonics. A 300 Hz tone's 3rd harmonic (900 Hz) appears in the driven tail, not the clean.
        auto thirdHarm = [&](float drive) {
            audio::Delay d;
            d.setEnabled(true);
            d.setTime(30.0f);
            d.setFeedback(0.85f);
            d.setMix(1.0f);
            d.setFeedbackDrive(drive);
            std::vector<float> b(static_cast<size_t>(sr) * 2, 0.0f); // 1 s stereo
            const int burst = sr / 5; // a 0.2 s 300 Hz burst to build the loop up
            for (int i = 0; i < burst; ++i) {
                const float s = static_cast<float>(0.5 * std::sin(kTwoPi * 300.0 * i / sr));
                b[static_cast<size_t>(i) * 2] = s;
                b[static_cast<size_t>(i) * 2 + 1] = s;
            }
            d.process(b.data(), sr, sr);
            const std::vector<float> tail(b.end() - static_cast<long>(sr) * 2 / 3, b.end()); // last ~0.33s
            const double w = 2.0 * 3.14159265358979 * 900.0 / sr;
            const double c = 2.0 * std::cos(w);
            double s1 = 0.0, s2 = 0.0;
            for (size_t i = 0; i < tail.size(); i += 2) {
                const double s0 = static_cast<double>(tail[i]) + c * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            return s1 * s1 + s2 * s2 - c * s1 * s2;
        };
        check(thirdHarm(0.9f) > thirdHarm(0.0f) * 5.0 + 1.0,
              "delay feedback drive saturates the repeats (adds harmonics)");
        check(audio::Delay().feedbackDrive() == 0.0f, "delay feedback drive defaults to clean");
    }

    // --- Delay ducking: the wet echoes step out of the way of a loud dry -----
    {
        auto wetRms = [&](float duck) {
            audio::Delay d;
            d.setEnabled(true);
            d.setTime(120.0f);
            d.setFeedback(0.4f);
            d.setMix(1.0f); // fully wet, so the output is the (ducked) echo
            d.setDuck(duck);
            std::vector<float> buf = sineStereo(sr, 220.0, 0.8, sr); // 1 s sustained loud tone
            d.process(buf.data(), sr, sr);
            double s = 0.0;
            for (int i = sr / 2; i < sr; ++i) { // steady region (past the first echo)
                const double v = buf[static_cast<size_t>(i) * 2];
                s += v * v;
            }
            return std::sqrt(s / (sr - sr / 2));
        };
        const double off = wetRms(0.0f);
        const double ducked = wetRms(1.0f);
        check(off > 0.0, "delay produces wet output with ducking off");
        check(ducked < off * 0.6, "delay ducking suppresses the echoes while the dry is loud");
        audio::Delay dd2;
        check(dd2.duck() == 0.0f, "delay ducking defaults to 0 (off)");
    }

    // --- Reverse delay: each chunk plays back time-reversed -------------------
    {
        audio::ReverseDelay rd;
        rd.setEnabled(true);
        rd.setTimeMs(100.0f); // 100 ms chunk → 4800 frames at 48 kHz
        rd.setFeedback(0.0f);
        rd.setMix(1.0f); // fully wet → only the reversed echo remains
        const int chunk = sr / 10; // 4800
        // Put a single click in the MIDDLE of the first chunk, silence elsewhere for two chunks.
        std::vector<float> buf(static_cast<size_t>(chunk) * 2 * 2, 0.0f); // two chunks, stereo
        const int clickAt = chunk / 4; // 1200 samples into chunk 0
        buf[static_cast<size_t>(clickAt) * 2] = 1.0f;
        buf[static_cast<size_t>(clickAt) * 2 + 1] = 1.0f;
        rd.process(buf.data(), chunk * 2, sr);
        // The reversed echo of chunk 0 plays during chunk 1; the click at index `clickAt` emerges at
        // the mirrored index (chunk-1-clickAt) within that second chunk.
        int peak = -1;
        float pv = 0.0f;
        for (int i = 0; i < chunk; ++i) {
            const float v = std::fabs(buf[static_cast<size_t>(chunk + i) * 2]);
            if (v > pv) {
                pv = v;
                peak = i;
            }
        }
        const int expected = chunk - 1 - clickAt;
        check(pv > 0.5f, "reverse delay produces the echo one chunk later");
        check(std::abs(peak - expected) < 50, "reverse delay plays the chunk back time-reversed");
        // Disabled → transparent.
        audio::ReverseDelay off;
        std::vector<float> q = sineStereo(sr / 4, 300.0, 0.5, sr);
        std::vector<float> ref = q;
        off.process(q.data(), sr / 4, sr);
        check(q == ref, "a disabled reverse delay is transparent");
        audio::ReverseDelay def;
        check(!def.enabled() && std::fabs(def.timeMs() - 300.0f) < 1e-3f,
              "reverse delay defaults to off at a 300 ms chunk");
    }

    // --- Multi-tap delay: a burst of echoes at successive tap times ----------
    {
        audio::MultiTapDelay mt;
        mt.setEnabled(true);
        mt.setTimeMs(100.0f); // base = 100 ms → 4800 frames
        mt.setTaps(3);
        mt.setDecay(0.5f);
        mt.setSpread(1.0f); // hard alternating pan
        mt.setMix(1.0f);    // fully wet
        const int base = sr / 10; // 4800
        std::vector<float> buf(static_cast<size_t>(sr) / 2 * 2, 0.0f); // 0.5 s
        buf[0] = 1.0f;
        buf[1] = 1.0f;
        mt.process(buf.data(), sr / 2, sr);
        auto peakAt = [&](int frame, int ch) {
            float p = 0.0f;
            for (int i = frame - 40; i <= frame + 40; ++i)
                if (i >= 0) p = std::max(p, std::fabs(buf[static_cast<size_t>(i) * 2 + ch]));
            return p;
        };
        // Tap 1 at 1×base (left-biased), tap 2 at 2×base (right-biased), tap 3 at 3×base (left).
        check(peakAt(base, 0) > 0.3f, "multi-tap: first echo lands at the base time");
        // Tap 2 is panned hard right, so it shows on the right channel.
        check(peakAt(2 * base, 1) > 0.05f, "multi-tap: second echo lands at 2× the base time");
        check(peakAt(2 * base, 1) < peakAt(base, 0), "multi-tap: successive taps decay");
        // Alternating pan: tap 1 leans left, tap 2 leans right.
        check(peakAt(base, 0) > peakAt(base, 1) * 2.0f, "multi-tap: the first tap pans left");
        check(peakAt(2 * base, 1) > peakAt(2 * base, 0) * 2.0f, "multi-tap: the second tap pans right");

        // Feedback: the longest tap (3×base) re-injects, so a second cluster appears — its first tap
        // lands at 4×base, a position that carries nothing in the finite (no-feedback) burst.
        auto peakAt4x = [&](float fb) {
            audio::MultiTapDelay m;
            m.setEnabled(true);
            m.setTimeMs(100.0f);
            m.setTaps(3);
            m.setDecay(0.6f);
            m.setSpread(1.0f);
            m.setMix(1.0f);
            m.setFeedback(fb);
            std::vector<float> b(static_cast<size_t>(sr) * 2, 0.0f); // 1 s
            b[0] = 1.0f;
            b[1] = 1.0f;
            m.process(b.data(), sr, sr);
            const int f4 = 4 * (sr / 10); // 4×base
            float p = 0.0f;
            for (int i = f4 - 40; i <= f4 + 40; ++i) {
                if (i >= 0) {
                    p = std::max(p, std::fabs(b[static_cast<size_t>(i) * 2])); // left (2nd cluster tap 0)
                }
            }
            return p;
        };
        check(peakAt4x(0.7f) > 0.02f, "multi-tap feedback repeats the cluster (a 4th-position echo)");
        check(peakAt4x(0.0f) < 0.005f, "without feedback the multi-tap burst is finite (no repeat)");
        check(audio::MultiTapDelay().feedback() == 0.0f, "multi-tap feedback defaults to off");
        // Disabled → transparent; defaults.
        audio::MultiTapDelay off;
        std::vector<float> q = sineStereo(sr / 4, 300.0, 0.5, sr);
        const std::vector<float> ref = q;
        off.process(q.data(), sr / 4, sr);
        check(q == ref, "a disabled multi-tap delay is transparent");
        check(!audio::MultiTapDelay().enabled() && audio::MultiTapDelay().taps() == 3,
              "multi-tap delay defaults to off with 3 taps");
    }

    // --- Ping-pong delay: echoes of a left-only impulse bounce L → R → L ------
    {
        audio::Delay pp;
        pp.setEnabled(true);
        pp.setTime(100.0f); // 4800 frames per echo
        pp.setFeedback(0.6f);
        pp.setMix(1.0f);
        pp.setPingPong(true);
        std::vector<float> buf(static_cast<size_t>(sr) / 2 * 2, 0.0f);
        buf[0] = 1.0f; // left channel only
        pp.process(buf.data(), sr / 2, sr);
        const float e1L = std::fabs(buf[static_cast<size_t>(4800) * 2]);
        const float e1R = std::fabs(buf[static_cast<size_t>(4800) * 2 + 1]);
        const float e2L = std::fabs(buf[static_cast<size_t>(9600) * 2]);
        const float e2R = std::fabs(buf[static_cast<size_t>(9600) * 2 + 1]);
        check(e1L > 0.5f && e1R < 0.05f, "ping-pong: first echo stays on the source (left) channel");
        check(e2R > 0.3f && e2L < 0.05f, "ping-pong: second echo bounces to the right channel");
    }

    // --- LowPass: high frequencies are attenuated ----------------------------
    {
        audio::LowPass lp;
        lp.setEnabled(true);
        lp.setCutoff(500.0f);
        std::vector<float> hi = sineStereo(sr, 12000.0, 1.0, sr); // 12 kHz, well above cutoff
        const double before = rms(hi);
        lp.process(hi.data(), sr, sr);
        check(rms(hi) < before * 0.5, "low-pass attenuates a 12 kHz tone");
    }

    // --- Compressor: a loud signal comes out quieter -------------------------
    {
        audio::Compressor comp;
        comp.setEnabled(true);
        comp.setThresholdDb(-18.0f);
        comp.setRatio(4.0f);
        comp.setMakeupDb(0.0f);
        std::vector<float> loud = sineStereo(sr, 220.0, 0.8, sr); // above threshold
        const float inPeak = 0.8f;
        comp.process(loud.data(), sr, sr);
        const float outPeak = peakRange(loud, sr / 2, sr); // measure after it settles
        check(outPeak < inPeak * 0.6f, "compressor reduces a loud signal's peak");
        check(outPeak > 0.0f, "compressor still passes signal");
        check(comp.gainReductionDb() < -1.0f, "the GR meter reports gain reduction on a loud signal");

        // A quiet signal (below threshold) draws no gain reduction.
        audio::Compressor quiet;
        quiet.setEnabled(true);
        quiet.setThresholdDb(-18.0f);
        quiet.setRatio(4.0f);
        std::vector<float> soft = sineStereo(sr, 220.0, 0.05, sr); // ~-26 dB, below threshold
        quiet.process(soft.data(), sr, sr);
        check(quiet.gainReductionDb() > -0.01f, "the GR meter reads ~0 when not compressing");

        // Auto makeup: it derives a positive makeup gain from threshold/ratio, so the compressed
        // output is louder than the same settings with no makeup — and matches the manual makeup path.
        auto compAt = [&](bool autoMk) {
            audio::Compressor c;
            c.setEnabled(true);
            c.setThresholdDb(-18.0f);
            c.setRatio(4.0f);
            c.setAutoMakeup(autoMk);
            std::vector<float> b = sineStereo(sr, 220.0, 0.8, sr);
            c.process(b.data(), sr, sr);
            return peakRange(b, sr / 2, sr);
        };
        check(compAt(true) > compAt(false) * 1.2f,
              "auto makeup lifts the compressed output level vs no makeup");
        audio::Compressor am;
        am.setThresholdDb(-18.0f);
        am.setRatio(4.0f);
        am.setAutoMakeup(true);
        check(am.effectiveMakeupDb() > 0.0f && !audio::Compressor().autoMakeup(),
              "auto makeup derives a positive gain and defaults to off");
    }

    // --- Compressor lookahead: transients are caught before they overshoot ---
    {
        // Silence, then a sudden loud tone. Without lookahead the onset slips through before the
        // attack engages (an overshoot spike); with lookahead the gain is already applied.
        auto onsetPeak = [&](float lookaheadMs) {
            audio::Compressor c;
            c.setEnabled(true);
            c.setThresholdDb(-24.0f);
            c.setRatio(8.0f);
            c.setAttackMs(5.0f);
            c.setReleaseMs(100.0f);
            c.setMix(1.0f);
            c.setLookaheadMs(lookaheadMs);
            const int frames = sr / 2;
            std::vector<float> b(static_cast<size_t>(frames) * 2, 0.0f);
            for (int i = 0; i < frames; ++i) {
                const float s = (i > frames * 2 / 5)
                                    ? 0.9f * static_cast<float>(std::sin(2.0 * 3.14159265358979 * 200.0 * i / sr))
                                    : 0.0f;
                b[static_cast<size_t>(i) * 2] = s;
                b[static_cast<size_t>(i) * 2 + 1] = s;
            }
            c.process(b.data(), frames, sr);
            float pk = 0.0f;
            for (float v : b) pk = std::max(pk, std::fabs(v));
            return pk;
        };
        check(onsetPeak(0.0f) > onsetPeak(5.0f) * 2.0f,
              "lookahead catches the transient onset a plain compressor overshoots");
        // Lookahead 0 (default) is off and adds no latency.
        audio::Compressor dc;
        check(dc.lookaheadMs() == 0.0f, "compressor lookahead defaults to 0 (off)");
    }

    // --- Compressor RMS detection: a brief transient is compressed less than in peak mode ----
    {
        // A quiet steady tone with a short loud burst. Peak detection clamps the burst hard; RMS
        // detection (a ~10 ms average) barely rises over the brief burst, so it passes louder.
        auto burstPeak = [&](bool rms) {
            audio::Compressor c;
            c.setEnabled(true);
            c.setThresholdDb(-30.0f);
            c.setRatio(10.0f);
            c.setAttackMs(1.0f);
            c.setReleaseMs(120.0f);
            c.setMix(1.0f);
            c.setRmsDetection(rms);
            const int frames = sr / 4;
            const int burstStart = frames / 2;
            const int burstLen = sr / 400; // ~2.5 ms burst
            std::vector<float> b(static_cast<size_t>(frames) * 2, 0.0f);
            for (int i = 0; i < frames; ++i) {
                const bool inBurst = i >= burstStart && i < burstStart + burstLen;
                const float amp = inBurst ? 0.9f : 0.02f; // loud burst over a quiet bed
                const float s = amp * static_cast<float>(std::sin(2.0 * 3.14159265358979 * 300.0 * i / sr));
                b[static_cast<size_t>(i) * 2] = s;
                b[static_cast<size_t>(i) * 2 + 1] = s;
            }
            c.process(b.data(), frames, sr);
            float pk = 0.0f;
            for (int i = burstStart; i < burstStart + burstLen; ++i) {
                pk = std::max(pk, std::fabs(b[static_cast<size_t>(i) * 2]));
            }
            return pk;
        };
        check(burstPeak(true) > burstPeak(false) * 1.15f,
              "RMS detection lets a brief transient through louder than peak detection");
        check(!audio::Compressor().rmsDetection(), "compressor detection defaults to peak (RMS off)");
    }

    // --- Compressor stereo link: linked pulls both channels down; unlinked spares the quiet one ----
    {
        // Loud on the left (above threshold), quiet on the right (below). Measure the right channel's
        // level: linked drags it down with the left, unlinked leaves it near untouched.
        auto rightLevel = [&](bool link) {
            audio::Compressor c;
            c.setEnabled(true);
            c.setThresholdDb(-24.0f);
            c.setRatio(8.0f);
            c.setAttackMs(1.0f);
            c.setReleaseMs(50.0f);
            c.setMakeupDb(0.0f);
            c.setMix(1.0f);
            c.setStereoLink(link);
            std::vector<float> b(static_cast<size_t>(sr) * 2, 0.0f);
            for (int i = 0; i < sr; ++i) {
                const float s = static_cast<float>(std::sin(2.0 * 3.14159265358979 * 200.0 * i / sr));
                b[static_cast<size_t>(i) * 2] = 0.9f * s;       // loud left
                b[static_cast<size_t>(i) * 2 + 1] = 0.1f * s;   // quiet right
            }
            c.process(b.data(), sr, sr);
            double e = 0.0;
            int n = 0;
            for (int i = sr / 2; i < sr; ++i) { // settled second half
                const double v = static_cast<double>(b[static_cast<size_t>(i) * 2 + 1]);
                e += v * v;
                ++n;
            }
            return std::sqrt(e / n);
        };
        check(rightLevel(false) > rightLevel(true) * 1.3,
              "unlinked compression spares the quiet channel (per-channel gain reduction)");
        check(audio::Compressor().stereoLink(), "compressor defaults to stereo-linked");
    }

    // --- Compressor sidechain HPF: lows don't drive the detection ------------
    {
        // A loud, pure low tone (60 Hz) above the threshold.
        auto compressedRms = [&](float scHpf) {
            audio::Compressor c;
            c.setEnabled(true);
            c.setThresholdDb(-18.0f);
            c.setRatio(8.0f);
            c.setMakeupDb(0.0f);
            c.setSidechainHpf(scHpf);
            std::vector<float> low = sineStereo(sr, 60.0, 0.8, sr);
            c.process(low.data(), sr, sr);
            std::vector<float> tail(low.begin() + static_cast<long>(sr), low.end()); // settled 2nd half
            return rms(tail);
        };
        const double noHpf = compressedRms(0.0f);   // detects the 60 Hz → compresses
        const double withHpf = compressedRms(300.0f); // HPF removes the 60 Hz → barely compresses
        check(withHpf > noHpf * 1.3,
              "sidechain HPF keeps a low tone from triggering the compressor (louder output)");
    }

    // --- Compressor knee: a soft knee compresses just below the threshold ----
    {
        // A signal a few dB below the threshold: a hard knee leaves it alone; a wide soft knee
        // already applies some gain reduction (softer, earlier onset).
        auto outPeakAt = [&](float knee) {
            audio::Compressor c;
            c.setEnabled(true);
            c.setThresholdDb(-12.0f);
            c.setRatio(4.0f);
            c.setKneeDb(knee);
            // 0.2 linear ≈ -14 dB: below the -12 dB threshold but inside a 12 dB knee (-18..-6 dB).
            std::vector<float> b = sineStereo(sr, 220.0, 0.2, sr);
            c.process(b.data(), sr, sr);
            return peakRange(b, sr / 2, sr);
        };
        const float hard = outPeakAt(0.0f);   // hard knee → untouched (~0.2)
        const float soft = outPeakAt(12.0f);   // soft knee → already reduced a touch
        check(hard > 0.19f, "hard knee leaves a sub-threshold signal untouched");
        check(soft < hard * 0.98f, "a soft knee compresses just below the threshold");
    }

    // --- Compressor mix: parallel (NY) compression sits between dry and wet --
    {
        auto outPeakAt = [&](float mix) {
            audio::Compressor c;
            c.setEnabled(true);
            c.setThresholdDb(-18.0f);
            c.setRatio(6.0f);
            c.setMix(mix);
            std::vector<float> b = sineStereo(sr, 220.0, 0.8, sr); // well above threshold
            c.process(b.data(), sr, sr);
            return peakRange(b, sr / 2, sr);
        };
        const float dry = outPeakAt(0.0f);      // no compression → ~0.8
        const float parallel = outPeakAt(0.5f); // blend
        const float wet = outPeakAt(1.0f);      // fully compressed
        check(dry > 0.78f, "compressor mix 0 leaves the signal dry");
        check(wet < dry * 0.7f, "compressor mix 1 fully compresses");
        check(parallel < dry && parallel > wet, "parallel mix sits between dry and fully compressed");
        audio::Compressor dc;
        check(std::fabs(dc.mix() - 1.0f) < 1e-6f, "compressor mix defaults to 1 (fully wet)");
    }

    // --- Limiter: a brickwall that guarantees the ceiling --------------------
    {
        // A signal well above the ceiling must come out capped at (or under) the ceiling — including
        // the very first transient, which the look-ahead catches before it reaches the output.
        audio::Limiter lim;
        lim.setEnabled(true);
        lim.setCeilingDb(-6.0f); // ≈ 0.501 linear
        lim.setLookaheadMs(2.0f);
        lim.setReleaseMs(50.0f);
        const float ceilLin = std::pow(10.0f, -6.0f / 20.0f);
        std::vector<float> hot = sineStereo(sr / 2, 200.0, 0.95, sr); // loud, above the ceiling
        lim.process(hot.data(), sr / 2, sr);
        float pk = 0.0f;
        for (float v : hot) {
            pk = std::max(pk, std::fabs(v));
        }
        check(pk <= ceilLin * 1.001f, "limiter keeps the whole signal at or under the ceiling");
        check(pk > ceilLin * 0.9f, "limiter pushes the loud signal up to the ceiling");
        check(lim.gainReductionDb() < -1.0f, "limiter GR meter reports reduction on a loud signal");

        // The first sample of a hard transient does not overshoot (look-ahead does its job).
        audio::Limiter lim2;
        lim2.setEnabled(true);
        lim2.setCeilingDb(-6.0f);
        lim2.setLookaheadMs(1.5f);
        std::vector<float> step(400 * 2, 0.0f);
        for (size_t i = 0; i < step.size(); ++i) {
            step[i] = 0.9f; // an instant full-scale-ish DC step
        }
        lim2.process(step.data(), 400, sr);
        float stepPk = 0.0f;
        for (float v : step) {
            stepPk = std::max(stepPk, std::fabs(v));
        }
        check(stepPk <= ceilLin * 1.001f, "limiter's look-ahead catches the first transient");

        // A quiet signal below the ceiling passes essentially untouched (just delayed).
        audio::Limiter lim3;
        lim3.setEnabled(true);
        lim3.setCeilingDb(-6.0f);
        std::vector<float> quiet = sineStereo(sr / 4, 200.0, 0.2, sr);
        const double before = rms(quiet);
        lim3.process(quiet.data(), sr / 4, sr);
        check(std::fabs(rms(quiet) - before) < before * 0.05, "limiter leaves a sub-ceiling signal alone");
        check(std::fabs(lim3.gainReductionDb()) < 0.01f, "limiter GR meter reads ~0 on a sub-ceiling signal");

        audio::Limiter dl;
        check(!dl.enabled() && std::fabs(dl.ceilingDb() + 0.3f) < 1e-4f,
              "limiter defaults to off with a −0.3 dB ceiling");
        check(std::fabs(dl.gainReductionDb()) < 1e-6f, "limiter GR meter defaults to 0");
    }

    // --- Leveler: slow AGC converges loud and quiet passages toward a target ---
    {
        // A 300 Hz tone that is loud (0.8) for 0.5 s then quiet (0.1) for 0.5 s.
        auto build = [&]() {
            std::vector<float> b(static_cast<size_t>(sr) * 2, 0.0f);
            for (int i = 0; i < sr; ++i) {
                const float amp = i < sr / 2 ? 0.8f : 0.1f;
                const float s = amp * static_cast<float>(std::sin(2.0 * 3.14159265358979 * 300.0 * i / sr));
                b[static_cast<size_t>(i) * 2] = s;
                b[static_cast<size_t>(i) * 2 + 1] = s;
            }
            return b;
        };
        auto rmsWin = [&](const std::vector<float>& b, int a, int c) {
            double e = 0.0;
            int n = 0;
            for (int i = a; i < c; ++i) {
                e += static_cast<double>(b[static_cast<size_t>(i) * 2]) * b[static_cast<size_t>(i) * 2];
                ++n;
            }
            return std::sqrt(e / n);
        };
        std::vector<float> in = build();
        const double inLoud = rmsWin(in, sr * 3 / 10, sr * 45 / 100); // settled loud tail
        const double inQuiet = rmsWin(in, sr * 8 / 10, sr);            // settled quiet tail
        std::vector<float> out = build();
        audio::Leveler lv;
        lv.setEnabled(true);
        lv.setTargetDb(-12.0f);
        lv.setResponseMs(150.0f);
        lv.setMaxGainDb(24.0f);
        lv.process(out.data(), sr, sr);
        const double outLoud = rmsWin(out, sr * 3 / 10, sr * 45 / 100);
        const double outQuiet = rmsWin(out, sr * 8 / 10, sr);
        check(outLoud < inLoud, "leveler eases the loud passage down");
        check(outQuiet > inQuiet, "leveler lifts the quiet passage up");
        check(outLoud / outQuiet < (inLoud / inQuiet) * 0.6,
              "leveler pulls loud and quiet toward a common level");
        // Disabled → transparent; defaults.
        std::vector<float> q = build();
        const std::vector<float> ref = q;
        audio::Leveler off;
        off.process(q.data(), sr, sr);
        check(q == ref, "a disabled leveler is transparent");
        check(!audio::Leveler().enabled() && std::fabs(audio::Leveler().targetDb() + 12.0f) < 1e-4f,
              "leveler defaults to off at a −12 dB target");
    }

    // --- Clipper: an instantaneous soft/hard ceiling -------------------------
    {
        // Hard clip (hardness 1): a signal driven well past the ceiling is flat-topped exactly at it.
        audio::Clipper clip;
        clip.setEnabled(true);
        clip.setDriveDb(6.0f);   // ×~2 → pushes a 0.95 sine to ~1.9
        clip.setCeiling(0.5f);
        clip.setHardness(1.0f);
        std::vector<float> hot = sineStereo(sr / 4, 200.0, 0.95, sr);
        clip.process(hot.data(), sr / 4, sr);
        float pk = 0.0f;
        for (float v : hot) {
            pk = std::max(pk, std::fabs(v));
        }
        check(pk <= 0.5f * 1.001f, "hard clipper keeps every sample at or under the ceiling");
        check(pk > 0.5f * 0.99f, "hard clipper drives the loud signal up to the ceiling");

        // Soft clip (hardness 0): a tanh knee also respects the ceiling but rounds rather than flat-tops
        // (its peak asymptotes just below the ceiling).
        audio::Clipper soft;
        soft.setEnabled(true);
        soft.setDriveDb(6.0f);
        soft.setCeiling(0.5f);
        soft.setHardness(0.0f);
        std::vector<float> hot2 = sineStereo(sr / 4, 200.0, 0.95, sr);
        soft.process(hot2.data(), sr / 4, sr);
        float pk2 = 0.0f;
        for (float v : hot2) {
            pk2 = std::max(pk2, std::fabs(v));
        }
        check(pk2 < 0.5f, "soft clipper's tanh knee stays under the ceiling (no flat-top)");

        // A sub-ceiling signal with no drive passes through untouched (hard knee never engages).
        audio::Clipper clean;
        clean.setEnabled(true);
        clean.setDriveDb(0.0f);
        clean.setCeiling(0.9f);
        clean.setHardness(1.0f);
        std::vector<float> quiet = sineStereo(sr / 4, 200.0, 0.2, sr);
        const double before = rms(quiet);
        clean.process(quiet.data(), sr / 4, sr);
        check(std::fabs(rms(quiet) - before) < before * 1e-4, "clipper leaves a sub-ceiling signal alone");

        // Drive boosts level before the ceiling engages (a quiet signal gets louder, un-clipped).
        audio::Clipper drv;
        drv.setEnabled(true);
        drv.setDriveDb(12.0f); // ×~3.98
        drv.setCeiling(0.9f);
        drv.setHardness(1.0f);
        std::vector<float> low = sineStereo(sr / 4, 200.0, 0.1, sr);
        const double lowBefore = rms(low);
        drv.process(low.data(), sr / 4, sr);
        check(rms(low) > lowBefore * 3.5, "clipper drive boosts a quiet signal's level");

        audio::Clipper dc;
        check(!dc.enabled() && std::fabs(dc.ceiling() - 0.9f) < 1e-4f &&
                  std::fabs(dc.hardness() - 1.0f) < 1e-4f,
              "clipper defaults to off with a 0.9 ceiling and a hard knee");

        // Parallel mix: below 1 the untouched (louder) dry is blended back, so peaks poke back above
        // the ceiling that fully-wet clipping holds them under.
        auto peakAtMix = [&](float mix) {
            audio::Clipper cl;
            cl.setEnabled(true);
            cl.setDriveDb(0.0f);
            cl.setCeiling(0.5f);
            cl.setHardness(1.0f);
            cl.setMix(mix);
            std::vector<float> b = sineStereo(sr / 4, 200.0, 1.0, sr); // peaks at ~1.0, above 0.5
            cl.process(b.data(), sr / 4, sr);
            float p = 0.0f;
            for (float v : b) {
                p = std::max(p, std::fabs(v));
            }
            return p;
        };
        check(peakAtMix(1.0f) <= 0.5f * 1.001f, "fully-wet clipper holds the peak at the ceiling");
        check(peakAtMix(0.3f) > 0.7f, "parallel clipping (mix < 1) lets the dry peak back through");
        check(std::fabs(audio::Clipper().mix() - 1.0f) < 1e-6f, "clipper mix defaults to 1 (fully clipped)");
    }

    // --- Multiband compressor: per-band, exact reconstruction ----------------
    {
        auto tailRms = [](const std::vector<float>& b, int fromFrame) {
            double s = 0.0;
            int c = 0;
            for (int i = fromFrame; i < static_cast<int>(b.size()) / 2; ++i) {
                s += static_cast<double>(b[static_cast<size_t>(i) * 2]) * b[static_cast<size_t>(i) * 2];
                ++c;
            }
            return c > 0 ? std::sqrt(s / c) : 0.0;
        };

        // Enabled with every ratio at 1 → the band split reconstructs the input exactly (transparent).
        audio::MultibandCompressor flat;
        flat.setEnabled(true);
        for (int b = 0; b < audio::MultibandCompressor::kBands; ++b) {
            flat.setBandRatio(b, 1.0f);
        }
        std::vector<float> in = sineStereo(sr / 2, 200.0, 0.5, sr);
        std::vector<float> out = in;
        flat.process(out.data(), sr / 2, sr);
        double maxDiff = 0.0;
        for (size_t i = 0; i < in.size(); ++i) {
            maxDiff = std::max(maxDiff, static_cast<double>(std::fabs(out[i] - in[i])));
        }
        check(maxDiff < 1e-5, "multiband with all ratios at 1 reconstructs the input exactly");

        // Compress only the LOW band: a loud low sine is reduced, a loud high sine is left alone.
        auto lowComp = [&]() {
            audio::MultibandCompressor m;
            m.setEnabled(true);
            m.setCrossoverLow(250.0f);
            m.setCrossoverHigh(2500.0f);
            m.setBandThreshold(0, -40.0f);
            m.setBandRatio(0, 8.0f); // squash the lows hard
            m.setBandRatio(1, 1.0f);
            m.setBandRatio(2, 1.0f);
            return m;
        };
        std::vector<float> low = sineStereo(sr / 2, 40.0, 0.9, sr); // deep in the low band
        const double lowBefore = tailRms(low, sr / 4);
        audio::MultibandCompressor mLow = lowComp();
        mLow.process(low.data(), sr / 2, sr);
        check(tailRms(low, sr / 4) < lowBefore * 0.5,
              "multiband low-band compression squashes a loud low tone");

        std::vector<float> high = sineStereo(sr / 2, 6000.0, 0.9, sr); // in the high band
        const double highBefore = tailRms(high, sr / 4);
        audio::MultibandCompressor mHigh = lowComp(); // same low-band-only settings
        mHigh.process(high.data(), sr / 2, sr);
        check(tailRms(high, sr / 4) > highBefore * 0.9,
              "low-band compression leaves a high tone essentially untouched (band independence)");

        audio::MultibandCompressor dm;
        check(!dm.enabled() && dm.bandRatio(0) == 3.0f && dm.crossoverLow() == 250.0f,
              "multiband defaults to off (3 bands, 250/2500 Hz crossovers)");
    }

    // --- Tempo-synced delay: time tracks the transport --------------------
    {
        audio::Delay d;
        d.setSync(true);
        // At 120 BPM a quarter note is 500 ms. Division 2 = 1/4, 4 = 1/8, 1 = 1/2.
        d.setSyncDivision(2);
        d.updateTempo(120.0);
        check(std::fabs(d.time() - 500.0f) < 0.5f, "synced delay tracks a quarter note (500 ms @120)");
        d.setSyncDivision(4); // 1/8
        d.updateTempo(120.0);
        check(std::fabs(d.time() - 250.0f) < 0.5f, "1/8 division halves the delay time");
        d.setSyncDivision(1); // 1/2
        d.updateTempo(140.0);
        check(std::fabs(d.time() - (60000.0f / 140.0f * 2.0f)) < 0.5f,
              "the synced time follows the BPM and division");

        // With sync off, updateTempo leaves the manual time alone.
        audio::Delay m;
        m.setTime(333.0f);
        m.updateTempo(120.0);
        check(std::fabs(m.time() - 333.0f) < 1e-3f, "updateTempo is a no-op when sync is off");

        audio::Delay dd;
        check(!dd.sync(), "delay tempo sync defaults to off");
    }

    // --- Tempo-synced phaser: sweep rate tracks the transport ---------------
    {
        audio::Phaser p;
        p.setSync(true);
        p.setSyncDivision(2); // 1/4 → 2 Hz @120
        p.updateTempo(120.0);
        check(std::fabs(p.rate() - 2.0f) < 0.01f, "synced phaser runs at 2 Hz for 1/4 @120 BPM");
        audio::Phaser pm;
        pm.setRate(0.5f);
        pm.updateTempo(120.0);
        check(std::fabs(pm.rate() - 0.5f) < 1e-3f, "phaser updateTempo is a no-op when sync is off");
        check(!audio::Phaser().sync(), "phaser tempo sync defaults to off");
    }

    // --- Tempo-synced chorus & flanger: LFO rate tracks the transport -------
    {
        audio::Chorus c;
        c.setSync(true);
        c.setSyncDivision(2); // 1/4 → 2 Hz @120
        c.updateTempo(120.0);
        check(std::fabs(c.rate() - 2.0f) < 0.01f, "synced chorus runs at 2 Hz for 1/4 @120 BPM");
        audio::Chorus cm;
        cm.setRate(0.9f);
        cm.updateTempo(120.0);
        check(std::fabs(cm.rate() - 0.9f) < 1e-3f, "chorus updateTempo is a no-op when sync is off");
        check(!audio::Chorus().sync(), "chorus tempo sync defaults to off");

        audio::Flanger f;
        f.setSync(true);
        f.setSyncDivision(3); // 1/8 → 4 Hz @120
        f.updateTempo(120.0);
        check(std::fabs(f.rate() - 4.0f) < 0.01f, "synced flanger runs at 4 Hz for 1/8 @120 BPM");
        audio::Flanger fm;
        fm.setRate(0.3f);
        fm.updateTempo(120.0);
        check(std::fabs(fm.rate() - 0.3f) < 1e-3f, "flanger updateTempo is a no-op when sync is off");
        check(!audio::Flanger().sync(), "flanger tempo sync defaults to off");

        audio::AutoPan ap;
        ap.setSync(true);
        ap.setSyncDivision(2); // 1/4 → 2 Hz @120
        ap.updateTempo(120.0);
        check(std::fabs(ap.rate() - 2.0f) < 0.01f, "synced auto-pan runs at 2 Hz for 1/4 @120 BPM");
        audio::AutoPan apm;
        apm.setRate(1.0f);
        apm.updateTempo(120.0);
        check(std::fabs(apm.rate() - 1.0f) < 1e-3f, "auto-pan updateTempo is a no-op when sync is off");
        check(!audio::AutoPan().sync(), "auto-pan tempo sync defaults to off");
    }

    // --- Tempo-synced stereo delay: independent L/R times track the tempo ----
    {
        audio::StereoDelay sd;
        sd.setSync(true);
        sd.setLeftDivision(2);  // 1/4 → 500 ms @120
        sd.setRightDivision(4); // 1/8 → 250 ms @120
        sd.updateTempo(120.0);
        check(std::fabs(sd.leftMs() - 500.0f) < 0.5f, "synced stereo delay: left tracks 1/4 (500 ms)");
        check(std::fabs(sd.rightMs() - 250.0f) < 0.5f, "synced stereo delay: right tracks 1/8 (250 ms)");

        audio::StereoDelay m;
        m.setLeftMs(180.0f);
        m.setRightMs(240.0f);
        m.updateTempo(120.0);
        check(std::fabs(m.leftMs() - 180.0f) < 1e-3f && std::fabs(m.rightMs() - 240.0f) < 1e-3f,
              "stereo delay updateTempo is a no-op when sync is off");

        audio::StereoDelay dd;
        check(!dd.sync(), "stereo delay tempo sync defaults to off");
    }

    // --- Tempo-synced tremolo (trance gate): rate tracks the transport -------
    {
        audio::Tremolo t;
        t.setSync(true);
        // At 120 BPM (2 beats/s): 1/4 = 2 Hz, 1/8 = 4 Hz, 1/16 = 8 Hz.
        t.setSyncDivision(2); // 1/4
        t.updateTempo(120.0);
        check(std::fabs(t.rate() - 2.0f) < 0.01f, "synced tremolo runs at 2 Hz for 1/4 @120 BPM");
        t.setSyncDivision(3); // 1/8
        t.updateTempo(120.0);
        check(std::fabs(t.rate() - 4.0f) < 0.01f, "1/8 division doubles the gate rate");
        t.setSyncDivision(5); // 1/16
        t.updateTempo(140.0);
        check(std::fabs(t.rate() - (140.0f / 60.0f * 4.0f)) < 0.01f,
              "the synced rate follows the BPM and division");

        audio::Tremolo m;
        m.setRate(7.0f);
        m.updateTempo(120.0);
        check(std::fabs(m.rate() - 7.0f) < 1e-3f, "tremolo updateTempo is a no-op when sync is off");

        audio::Tremolo dd;
        check(!dd.sync(), "tremolo tempo sync defaults to off");
    }

    // --- De-esser: ducks the high band, leaves the low band alone ------------
    {
        // A loud high-frequency tone (well above the crossover) should be attenuated; a low tone
        // (below the crossover) should pass essentially untouched.
        auto deessed = [&](double freq) {
            audio::DeEsser de;
            de.setEnabled(true);
            de.setFrequency(5000.0f);
            de.setThresholdDb(-30.0f);
            de.setAmount(1.0f);
            std::vector<float> buf = sineStereo(sr / 2, freq, 0.8, sr);
            const double before = rms(buf);
            de.process(buf.data(), sr / 2, sr);
            return rms(buf) / (before + 1e-12);
        };
        const double hiRatio = deessed(9000.0); // sibilant band → reduced
        const double loRatio = deessed(300.0);  // body → untouched
        check(hiRatio < 0.7, "de-esser attenuates the loud high band");
        check(loRatio > 0.9, "de-esser leaves the low band essentially untouched");

        // Disabled or below threshold → transparent.
        audio::DeEsser off;
        std::vector<float> q = sineStereo(sr / 4, 9000.0, 0.8, sr);
        const double qb = rms(q);
        off.process(q.data(), sr / 4, sr);
        check(std::fabs(rms(q) - qb) < 1e-6, "a disabled de-esser is transparent");

        audio::DeEsser dd;
        check(!dd.enabled() && std::fabs(dd.frequency() - 6000.0f) < 1e-3f,
              "de-esser defaults to off at a 6 kHz crossover");
    }

    // --- Dynamic EQ: a tunable band cuts/boosts only when it crosses threshold ----
    {
        // RMS of a settled window (second half) of a mono tone run through a dynamic-EQ band at 3 kHz.
        auto ratioAt = [&](double freq, double amp, float rangeDb, float thrDb) {
            audio::DynamicEq dq;
            dq.setEnabled(true);
            dq.setFrequency(3000.0f);
            dq.setQ(2.0f);
            dq.setThresholdDb(thrDb);
            dq.setRangeDb(rangeDb);
            std::vector<float> b = sineStereo(sr, freq, amp, sr);
            double before = 0.0;
            for (int i = sr / 2; i < sr; ++i) before += static_cast<double>(b[static_cast<size_t>(i) * 2]) *
                                                          static_cast<double>(b[static_cast<size_t>(i) * 2]);
            dq.process(b.data(), sr, sr);
            double after = 0.0;
            for (int i = sr / 2; i < sr; ++i) after += static_cast<double>(b[static_cast<size_t>(i) * 2]) *
                                                       static_cast<double>(b[static_cast<size_t>(i) * 2]);
            return std::sqrt(after / (before + 1e-18));
        };
        // A loud tone at the band centre, over threshold, with a dynamic cut → strongly attenuated.
        check(ratioAt(3000.0, 0.8, -12.0f, -40.0f) < 0.4,
              "dynamic EQ cuts a loud tone at the band centre");
        // A loud tone far from the band → the detector never engages, so it passes ~untouched.
        check(ratioAt(300.0, 0.8, -12.0f, -40.0f) > 0.95,
              "dynamic EQ leaves an out-of-band tone alone (band-selective)");
        // Boost mode: a loud in-band tone is lifted.
        check(ratioAt(3000.0, 0.4, 12.0f, -40.0f) > 1.5,
              "dynamic EQ boosts a loud in-band tone when range is positive");
        // A quiet in-band tone below threshold passes bit-exact (engagement is exactly 0).
        {
            audio::DynamicEq dq;
            dq.setEnabled(true);
            dq.setFrequency(3000.0f);
            dq.setRangeDb(-12.0f);
            dq.setThresholdDb(-6.0f); // well above the quiet tone's band level
            std::vector<float> b = sineStereo(sr / 4, 3000.0, 0.01, sr);
            std::vector<float> ref = b;
            dq.process(b.data(), sr / 4, sr);
            check(b == ref, "dynamic EQ below threshold is bit-exact transparent");
        }
        // Disabled → transparent.
        {
            audio::DynamicEq off;
            std::vector<float> b = sineStereo(sr / 4, 3000.0, 0.8, sr);
            std::vector<float> ref = b;
            off.process(b.data(), sr / 4, sr);
            check(b == ref, "a disabled dynamic EQ is transparent");
        }
        audio::DynamicEq def;
        check(!def.enabled() && std::fabs(def.frequency() - 3000.0f) < 1e-3f,
              "dynamic EQ defaults to off at 3 kHz");
    }

    // --- Stereo enhancer: widens a mono source ------------------------------
    {
        // A mono input (L == R) comes out decorrelated: the right channel is delayed, so L and R
        // differ (stereo width) while the left channel itself is untouched.
        std::vector<float> orig = sineStereo(sr / 4, 300.0, 0.7, sr);
        std::vector<float> buf = orig;
        audio::StereoEnhancer se;
        se.setEnabled(true);
        se.setDelayMs(7.0f); // not an integer number of periods → the delayed copy is decorrelated
        se.setAmount(1.0f);
        se.process(buf.data(), sr / 4, sr);
        double lDiff = 0.0, lr = 0.0;
        for (int i = 2000; i < sr / 4; ++i) {
            lDiff += std::fabs(buf[static_cast<size_t>(i) * 2] - orig[static_cast<size_t>(i) * 2]);
            lr += std::fabs(buf[static_cast<size_t>(i) * 2] - buf[static_cast<size_t>(i) * 2 + 1]);
        }
        check(lDiff < 1e-4, "stereo enhancer leaves the left channel untouched");
        check(lr > 50.0, "stereo enhancer decorrelates a mono source into a wide image");

        std::vector<float> q = orig;
        audio::StereoEnhancer off;
        off.process(q.data(), sr / 4, sr);
        check(q == orig, "a disabled stereo enhancer is transparent");

        audio::StereoEnhancer dd;
        check(!dd.enabled() && std::fabs(dd.delayMs() - 12.0f) < 1e-3f,
              "stereo enhancer defaults to off at a 12 ms offset");
    }

    // --- Reverb: an impulse leaves a decaying tail ---------------------------
    {
        audio::Reverb rev;
        rev.setEnabled(true);
        rev.setRoomSize(0.8f);
        rev.setMix(1.0f);
        std::vector<float> buf(static_cast<size_t>(sr) / 2 * 2, 0.0f);
        buf[0] = 1.0f;
        buf[1] = 1.0f;
        rev.process(buf.data(), sr / 2, sr);
        double tail = 0.0;
        for (int i = sr / 20; i < sr / 2; ++i) { // energy after 50 ms
            tail += std::fabs(static_cast<double>(buf[static_cast<size_t>(i) * 2]));
        }
        check(tail > 0.0, "reverb produces a tail after the impulse");
    }

    // --- Reverb ducking: the wet steps out of the way of a loud dry ----------
    {
        auto wetRms = [&](float duck) {
            audio::Reverb rev;
            rev.setEnabled(true);
            rev.setRoomSize(0.8f);
            rev.setMix(1.0f); // fully wet, so the output is the (ducked) tail
            rev.setDuck(duck);
            std::vector<float> buf = sineStereo(sr, 220.0, 0.8, sr); // 1 s sustained loud tone
            rev.process(buf.data(), sr, sr);
            double s = 0.0;
            for (int i = sr / 2; i < sr; ++i) { // steady tail region
                const double v = buf[static_cast<size_t>(i) * 2];
                s += v * v;
            }
            return std::sqrt(s / (sr - sr / 2));
        };
        const double off = wetRms(0.0f);
        const double ducked = wetRms(1.0f);
        check(off > 0.0, "reverb produces wet output with ducking off");
        check(ducked < off * 0.6, "ducking suppresses the wet while the dry is loud");

        audio::Reverb dd;
        check(dd.duck() == 0.0f, "reverb ducking defaults to 0 (off)");
    }

    // --- Gated reverb: the tail is cut off after the hold time ----------------
    {
        // Energy in the late window (after 250 ms), fed a 50 ms burst then silence.
        auto lateEnergy = [&](float gateMs) {
            audio::Reverb rev;
            rev.setEnabled(true);
            rev.setRoomSize(0.85f);
            rev.setMix(1.0f); // fully wet — the output is the tail
            rev.setGateMs(gateMs);
            std::vector<float> buf(static_cast<size_t>(sr) / 2 * 2, 0.0f); // 0.5 s stereo
            for (int i = 0; i < sr / 20; ++i) {                            // 50 ms burst
                const float s = static_cast<float>(0.6 * std::sin(2.0 * 3.14159265 * 220.0 * i / sr));
                buf[static_cast<size_t>(i) * 2] = s;
                buf[static_cast<size_t>(i) * 2 + 1] = s;
            }
            rev.process(buf.data(), sr / 2, sr);
            double e = 0.0;
            for (int i = sr / 4; i < sr / 2; ++i) { // after 250 ms
                const double v = buf[static_cast<size_t>(i) * 2];
                e += v * v;
            }
            return e;
        };
        audio::Reverb dg;
        check(dg.gateMs() == 0.0f, "reverb gate defaults to 0 (off)");
        const double natural = lateEnergy(0.0f);
        const double gated = lateEnergy(100.0f);
        check(natural > 0.0, "an ungated reverb rings on well after the burst");
        check(gated < natural * 0.15, "a gated reverb cuts the tail after the hold time");
    }

    // --- Reverb freeze: the tail is held instead of decaying ------------------
    {
        auto lateTail = [&](bool freeze) {
            audio::Reverb rev;
            rev.setEnabled(true);
            rev.setRoomSize(0.5f);
            rev.setMix(1.0f);
            std::vector<float> imp(200 * 2, 0.0f);
            imp[0] = 1.0f;
            imp[1] = 1.0f;
            rev.process(imp.data(), 200, sr); // seed a tail
            rev.setFreeze(freeze);
            std::vector<float> sil(static_cast<size_t>(sr) * 2 * 2, 0.0f); // 2 s stereo silence
            rev.process(sil.data(), sr * 2, sr);
            double e = 0.0;
            for (int i = sr; i < sr * 2; ++i) { // energy in the final second
                const double v = static_cast<double>(sil[static_cast<size_t>(i) * 2]);
                e += v * v;
            }
            return e;
        };
        check(lateTail(true) > lateTail(false) * 5.0,
              "freeze holds the reverb tail far longer than a normal decay");
        audio::Reverb dr;
        check(!dr.freeze(), "reverb freeze defaults off");
    }

    // --- Reverb shimmer: an octave-up halo appears in the wet, and stays stable ----
    {
        // Goertzel power at a frequency on the left channel.
        auto power = [](const std::vector<float>& b, double f, int srate) {
            const double w = 2.0 * 3.14159265358979 * f / srate;
            const double c = 2.0 * std::cos(w);
            double s1 = 0.0, s2 = 0.0;
            for (size_t i = 0; i < b.size(); i += 2) {
                const double s0 = static_cast<double>(b[i]) + c * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            return s1 * s1 + s2 * s2 - c * s1 * s2;
        };
        // Run a sustained 200 Hz tone through a fully-wet reverb and return the last-0.5 s window +
        // the whole-run peak (for a stability check).
        auto run = [&](float shimmer, double* octPower, float* peak) {
            audio::Reverb rev;
            rev.setEnabled(true);
            rev.setRoomSize(0.7f);
            rev.setMix(1.0f); // fully wet, so the output is the (shimmered) tail
            rev.setShimmer(shimmer);
            std::vector<float> b(static_cast<size_t>(sr) * 2 * 2, 0.0f); // 2 s stereo
            for (int i = 0; i < sr * 2; ++i) {
                const float s = 0.3f * static_cast<float>(std::sin(2.0 * 3.14159265358979 * 200.0 * i / sr));
                b[static_cast<size_t>(i) * 2] = s;
                b[static_cast<size_t>(i) * 2 + 1] = s;
            }
            rev.process(b.data(), sr * 2, sr);
            float pk = 0.0f;
            for (float v : b) {
                pk = std::max(pk, std::fabs(v));
            }
            *peak = pk;
            std::vector<float> tail(b.end() - static_cast<long>(sr), b.end()); // last 0.5 s (stereo)
            *octPower = power(tail, 400.0, sr); // octave-up of the 200 Hz input
        };
        double octOff = 0.0, octOn = 0.0;
        float pkOff = 0.0f, pkOn = 0.0f;
        run(0.0f, &octOff, &pkOff);
        run(0.8f, &octOn, &pkOn);
        check(octOn > octOff * 20.0 + 1.0,
              "shimmer injects a strong octave-up halo into the wet tail");
        check(pkOn < 4.0f, "shimmer stays bounded (no runaway feedback)");
        // shimmer = 0 leaves the reverb bit-for-bit unchanged (the shimmer path is skipped).
        auto tailOf = [&](float shimmer) {
            audio::Reverb rev;
            rev.setEnabled(true);
            rev.setRoomSize(0.6f);
            rev.setMix(0.5f);
            rev.setShimmer(shimmer);
            std::vector<float> b(static_cast<size_t>(sr) / 2 * 2, 0.0f);
            b[0] = 1.0f;
            b[1] = 1.0f;
            rev.process(b.data(), sr / 2, sr);
            return b;
        };
        std::vector<float> zeroA = tailOf(0.0f);
        std::vector<float> zeroB = tailOf(0.0f);
        bool identical = zeroA.size() == zeroB.size();
        for (size_t i = 0; identical && i < zeroA.size(); ++i) {
            if (zeroA[i] != zeroB[i]) identical = false;
        }
        check(identical, "a shimmer-off reverb is deterministic (path fully skipped)");
        audio::Reverb ds;
        check(ds.shimmer() == 0.0f, "reverb shimmer defaults to 0 (off)");
    }

    // --- Reverb tail modulation: a lush moving tail smears the static spectral peak -----
    {
        auto power = [](const std::vector<float>& b, double f, int srate) {
            const double w = 2.0 * 3.14159265358979 * f / srate;
            const double c = 2.0 * std::cos(w);
            double s1 = 0.0, s2 = 0.0;
            for (size_t i = 0; i < b.size(); i += 2) {
                const double s0 = static_cast<double>(b[i]) + c * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            return s1 * s1 + s2 * s2 - c * s1 * s2;
        };
        // Sustain a 500 Hz tone through a fully-wet reverb; measure how concentrated the last-0.5 s
        // tail's energy is at exactly 500 Hz. Modulation should smear that peak (lower concentration).
        auto run = [&](float modDepth, double* peakConc, float* peak) {
            audio::Reverb rev;
            rev.setEnabled(true);
            rev.setRoomSize(0.8f);
            rev.setMix(1.0f);
            rev.setModDepth(modDepth);
            rev.setModRate(2.0f);
            std::vector<float> b(static_cast<size_t>(sr) * 2 * 2, 0.0f); // 2 s stereo
            for (int i = 0; i < sr * 2; ++i) {
                const float s = 0.3f * static_cast<float>(std::sin(2.0 * 3.14159265358979 * 500.0 * i / sr));
                b[static_cast<size_t>(i) * 2] = s;
                b[static_cast<size_t>(i) * 2 + 1] = s;
            }
            rev.process(b.data(), sr * 2, sr);
            float pk = 0.0f;
            for (float v : b) {
                pk = std::max(pk, std::fabs(v));
            }
            *peak = pk;
            std::vector<float> tail(b.end() - static_cast<long>(sr), b.end()); // last 0.5 s (stereo)
            double total = 0.0;
            for (size_t k = 0; k < tail.size(); k += 2) {
                total += static_cast<double>(tail[k]) * tail[k];
            }
            *peakConc = total > 0.0 ? power(tail, 500.0, sr) / total : 0.0;
        };
        double concOff = 0.0, concOn = 0.0;
        float pkOff = 0.0f, pkOn = 0.0f;
        run(0.0f, &concOff, &pkOff);
        run(3.0f, &concOn, &pkOn);
        check(concOn < concOff * 0.9,
              "reverb tail modulation smears the static spectral peak (a lusher, moving tail)");
        check(pkOn < 4.0f, "the modulated reverb tail stays bounded");
        audio::Reverb dm;
        check(dm.modDepth() == 0.0f, "reverb tail modulation defaults to 0 (off)");
    }

    // --- Reverb width: narrow the wet tail to mono --------------------------
    {
        auto sideEnergy = [&](float width) {
            audio::Reverb rev;
            rev.setEnabled(true);
            rev.setRoomSize(0.8f);
            rev.setMix(1.0f); // fully wet
            rev.setWidth(width);
            std::vector<float> buf(static_cast<size_t>(sr) / 2 * 2, 0.0f);
            buf[0] = 1.0f;
            buf[1] = 1.0f;
            rev.process(buf.data(), sr / 2, sr);
            double e = 0.0;
            for (size_t i = 0; i + 1 < buf.size(); i += 2) {
                const double s = 0.5 * (static_cast<double>(buf[i]) - static_cast<double>(buf[i + 1]));
                e += s * s;
            }
            return e;
        };
        const double natural = sideEnergy(1.0f);
        check(natural > 0.0, "the natural reverb tail is stereo (has side energy)");
        check(sideEnergy(0.0f) < natural * 0.01, "width 0 collapses the reverb tail to mono");
    }

    // --- Reverb pre-delay: the tail onset is pushed back --------------------
    {
        auto earlyEnergy = [&](float preMs) {
            audio::Reverb rev;
            rev.setEnabled(true);
            rev.setRoomSize(0.8f);
            rev.setMix(1.0f);
            rev.setPreDelayMs(preMs);
            std::vector<float> buf(static_cast<size_t>(sr) / 2 * 2, 0.0f);
            buf[0] = 1.0f;
            buf[1] = 1.0f;
            rev.process(buf.data(), sr / 2, sr);
            // Energy in the first 40 ms — a long pre-delay should leave this window near-silent.
            double e = 0.0;
            for (int i = 0; i < sr / 25; ++i) {
                e += std::fabs(static_cast<double>(buf[static_cast<size_t>(i) * 2]));
            }
            return e;
        };
        const double noPre = earlyEnergy(0.0f);
        const double withPre = earlyEnergy(120.0f); // 120 ms pre-delay
        check(withPre < noPre * 0.2, "pre-delay pushes the reverb tail past the early window");
    }

    // --- Ring modulator: multiplies by an internal carrier ------------------
    {
        // A constant (DC) input × a 300 Hz carrier → a pure 300 Hz tone at the output.
        audio::RingMod rm;
        rm.setEnabled(true);
        rm.setFreq(300.0f);
        rm.setMix(1.0f);
        std::vector<float> dc(static_cast<size_t>(sr) * 2, 0.5f); // 1 s of DC 0.5, both channels
        rm.process(dc.data(), sr, sr);
        // Estimate the output frequency from left-channel rising zero-crossings.
        int crossings = 0;
        for (int i = 1; i < sr; ++i) {
            if (dc[static_cast<size_t>(i - 1) * 2] <= 0.0f && dc[static_cast<size_t>(i) * 2] > 0.0f) {
                ++crossings;
            }
        }
        check(std::abs(crossings - 300) < 5, "ring-mod of DC yields a tone at the carrier frequency");

        // Mix 0 → transparent.
        audio::RingMod off;
        off.setFreq(300.0f);
        off.setMix(0.0f);
        off.setEnabled(true);
        std::vector<float> sig = sineStereo(1000, 220.0, 0.5, sr);
        const std::vector<float> ref = sig;
        off.process(sig.data(), 1000, sr);
        bool same = true;
        for (size_t i = 0; i < sig.size(); ++i) {
            if (std::fabs(sig[i] - ref[i]) > 1e-6f) {
                same = false;
                break;
            }
        }
        check(same, "ring-mod at mix 0 is transparent");

        // Carrier waveform: a square carrier carries many harmonics, each mirroring the input into a
        // fresh sideband pair, so its output is measurably brighter (more first-difference / HF
        // energy) than the classic sine carrier at the same frequency and mix.
        auto ringHf = [&](audio::RingMod::Carrier c) {
            audio::RingMod r;
            r.setEnabled(true);
            r.setFreq(200.0f);
            r.setMix(1.0f);
            r.setCarrier(c);
            std::vector<float> b = sineStereo(sr, 300.0, 0.5, sr);
            r.process(b.data(), sr, sr);
            double e = 0.0;
            for (int i = 1; i < sr; ++i) {
                const double d = static_cast<double>(b[static_cast<size_t>(i) * 2] -
                                                     b[static_cast<size_t>(i - 1) * 2]);
                e += d * d;
            }
            return e;
        };
        check(ringHf(audio::RingMod::Carrier::Square) > ringHf(audio::RingMod::Carrier::Sine) * 1.5,
              "square-carrier ring mod is brighter than sine (more sidebands)");
        check(audio::RingMod().carrier() == audio::RingMod::Carrier::Sine,
              "ring-mod carrier defaults to sine");
    }

    // --- Pitch shifter: shifts pitch up/down without changing length ---------
    {
        // Rising zero-crossing rate on the left channel = fundamental frequency estimate.
        auto freqOf = [&](const std::vector<float>& b) {
            int cross = 0;
            for (size_t i = 4; i + 1 < b.size(); i += 2) {
                if (b[i - 2] <= 0.0f && b[i] > 0.0f) {
                    ++cross;
                }
            }
            const double dur = static_cast<double>(b.size() / 2) / sr;
            return static_cast<double>(cross) / dur;
        };
        auto shifted = [&](float semis) {
            audio::PitchShifter ps;
            ps.setEnabled(true);
            ps.setSemitones(semis);
            ps.setMix(1.0f);
            std::vector<float> b = sineStereo(sr, 440.0, 0.5, sr);
            ps.process(b.data(), sr, sr);
            // Measure the settled second half (past the buffer fill).
            return std::vector<float>(b.begin() + static_cast<std::ptrdiff_t>(b.size() / 2), b.end());
        };
        check(freqOf(shifted(12.0f)) > 780.0 && freqOf(shifted(12.0f)) < 980.0,
              "pitch shifter up an octave roughly doubles the frequency (~880 Hz)");
        check(freqOf(shifted(-12.0f)) > 180.0 && freqOf(shifted(-12.0f)) < 280.0,
              "pitch shifter down an octave roughly halves the frequency (~220 Hz)");
        check(std::fabs(freqOf(shifted(0.0f)) - 440.0) < 40.0,
              "pitch shifter at 0 semitones keeps the pitch (~440 Hz)");
        audio::PitchShifter dp;
        check(dp.semitones() == 0.0f && !dp.enabled(), "pitch shifter defaults to 0 st and off");

        // Feedback: each pass re-shifts the signal, so an octave-up shift cascades ever higher — the
        // tail's dominant frequency climbs well above the single-pass shift, and stays bounded.
        auto shiftedFb = [&](float fb, float* peakOut) {
            audio::PitchShifter ps;
            ps.setEnabled(true);
            ps.setSemitones(12.0f);
            ps.setMix(1.0f);
            ps.setFeedback(fb);
            std::vector<float> b = sineStereo(sr, 440.0, 0.5, sr);
            ps.process(b.data(), sr, sr);
            float pk = 0.0f;
            for (float v : b) {
                pk = std::max(pk, std::fabs(v));
            }
            *peakOut = pk;
            const std::vector<float> tail(b.begin() + static_cast<std::ptrdiff_t>(b.size() / 2), b.end());
            return freqOf(tail);
        };
        float pkNo = 0.0f, pkFb = 0.0f;
        const double freqNo = shiftedFb(0.0f, &pkNo);
        const double freqFb = shiftedFb(0.7f, &pkFb);
        check(freqFb > freqNo * 1.8,
              "pitch feedback cascades the shift upward (tail dominant frequency climbs)");
        check(pkFb < 8.0f, "pitch feedback stays bounded");
        check(audio::PitchShifter().feedback() == 0.0f, "pitch shifter feedback defaults to off");
    }

    // --- Frequency shifter: single-sideband heterodyne (Hz shift, not a ratio) ------
    {
        // Goertzel power at a given frequency, on the left channel.
        auto goertzel = [](const std::vector<float>& b, double f, int srate) {
            const double w = 2.0 * 3.14159265358979 * f / srate;
            const double c = 2.0 * std::cos(w);
            double s1 = 0.0, s2 = 0.0;
            for (size_t i = 0; i < b.size(); i += 2) {
                const double s0 = static_cast<double>(b[i]) + c * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            return s1 * s1 + s2 * s2 - c * s1 * s2;
        };
        // A 1000 Hz tone shifted +200 Hz should land at 1200 Hz, with the 800 Hz image suppressed.
        audio::FrequencyShifter fs;
        fs.setEnabled(true);
        fs.setShiftHz(200.0f);
        fs.setMix(1.0f);
        std::vector<float> b = sineStereo(sr, 1000.0, 0.5, sr);
        fs.process(b.data(), sr, sr);
        const double up = goertzel(b, 1200.0, sr);
        const double image = goertzel(b, 800.0, sr);
        const double leak = goertzel(b, 1000.0, sr);
        check(up > image * 10.0, "frequency shifter puts energy in the shifted sideband, not the image");
        check(up > leak * 10.0, "frequency shifter suppresses the original (carrier) frequency");

        // A negative shift moves the tone down to 800 Hz instead.
        audio::FrequencyShifter fd;
        fd.setEnabled(true);
        fd.setShiftHz(-200.0f);
        std::vector<float> bd = sineStereo(sr, 1000.0, 0.5, sr);
        fd.process(bd.data(), sr, sr);
        check(goertzel(bd, 800.0, sr) > goertzel(bd, 1200.0, sr) * 10.0,
              "a negative shift moves the tone down (SSB, opposite sideband)");

        // Disabled → bit-identical passthrough; sensible defaults.
        audio::FrequencyShifter off;
        std::vector<float> a = sineStereo(sr / 4, 440.0, 0.5, sr);
        std::vector<float> a2 = a;
        off.process(a2.data(), sr / 4, sr);
        bool same = true;
        for (size_t i = 0; i < a.size(); ++i) {
            if (a[i] != a2[i]) {
                same = false;
            }
        }
        check(same, "disabled frequency shifter is a bit-identical passthrough");
        check(audio::FrequencyShifter().shiftHz() == 0.0f && !audio::FrequencyShifter().enabled(),
              "frequency shifter defaults to 0 Hz and off");
    }

    // --- Distortion: adds harmonics to a sine (raises high-frequency content) ------
    {
        audio::Distortion dist;
        dist.setEnabled(true);
        dist.setDrive(8.0f);
        dist.setMix(1.0f);
        std::vector<float> sine = sineStereo(sr, 200.0, 0.5, sr);
        auto hf = [](const std::vector<float>& b) {
            double s = 0.0;
            for (size_t i = 2; i < b.size(); i += 2) {
                const double d = static_cast<double>(b[i] - b[i - 2]);
                s += d * d;
            }
            return s;
        };
        const double before = hf(sine);
        dist.process(sine.data(), sr, sr);
        check(hf(sine) > before, "distortion adds high-frequency harmonics");

        // Post tone: a low tone setting rolls off the harmonics the drive added.
        auto distHf = [&](float tone) {
            audio::Distortion d;
            d.setEnabled(true);
            d.setDrive(8.0f);
            d.setMix(1.0f);
            d.setTone(tone);
            std::vector<float> b = sineStereo(sr / 2, 400.0, 0.5, sr); // harmonics at 1200, 2000, …
            d.process(b.data(), sr / 2, sr);
            double s = 0.0;
            for (size_t i = 2; i < b.size(); i += 2) {
                const double dd = static_cast<double>(b[i] - b[i - 2]);
                s += dd * dd;
            }
            return s;
        };
        check(distHf(700.0f) < distHf(20000.0f) * 0.7,
              "distortion post tone darkens the drive (rolls off added highs)");
        audio::Distortion dt;
        check(dt.tone() == 20000.0f, "distortion tone defaults to open (20 kHz)");

        // Output trim: a post-shaper level control that scales the whole output linearly.
        auto distOut = [&](float outDb) {
            audio::Distortion d;
            d.setEnabled(true);
            d.setDrive(8.0f);
            d.setMix(1.0f);
            d.setCurve(audio::Distortion::Curve::Hard);
            d.setOutputDb(outDb);
            std::vector<float> b = sineStereo(sr / 2, 300.0, 0.5, sr);
            d.process(b.data(), sr / 2, sr);
            return rms(b);
        };
        const double outAt0 = distOut(0.0f);
        const double outAtMinus6 = distOut(-6.0f);
        check(std::fabs(outAtMinus6 - outAt0 * 0.5011872) < outAt0 * 0.02,
              "distortion output trim scales the level (−6 dB ≈ half)");
        check(dt.outputDb() == 0.0f, "distortion output trim defaults to 0 dB (unity)");

        // Bias/asymmetry: a symmetric curve makes only odd harmonics; bias adds even ones (2nd).
        auto goertzel = [](const std::vector<float>& b, double f, int srate) {
            const double w = 2.0 * 3.14159265358979 * f / srate;
            const double c = 2.0 * std::cos(w);
            double s1 = 0.0, s2 = 0.0;
            for (size_t i = 0; i < b.size(); i += 2) {
                const double s0 = static_cast<double>(b[i]) + c * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            return s1 * s1 + s2 * s2 - c * s1 * s2;
        };
        auto second = [&](float bias) {
            audio::Distortion d;
            d.setEnabled(true);
            d.setDrive(6.0f);
            d.setMix(1.0f);
            d.setCurve(audio::Distortion::Curve::Soft); // symmetric on its own → odd harmonics only
            d.setBias(bias);
            std::vector<float> b = sineStereo(sr, 220.0, 0.5, sr);
            d.process(b.data(), sr, sr);
            std::vector<float> tail(b.begin() + static_cast<long>(sr) / 2, b.end()); // skip settling
            return goertzel(tail, 440.0, sr); // the 2nd (even) harmonic
        };
        const double evenSym = second(0.0f);
        const double evenBias = second(0.5f);
        check(evenBias > evenSym * 20.0 + 1.0,
              "distortion bias adds a strong even (2nd) harmonic a symmetric curve lacks");

        // bias 0 leaves the shaper bit-for-bit identical to the classic symmetric path.
        auto proc = [&](float bias) {
            audio::Distortion d;
            d.setEnabled(true);
            d.setDrive(5.0f);
            d.setMix(1.0f);
            d.setBias(bias);
            std::vector<float> b = sineStereo(sr / 4, 300.0, 0.6, sr);
            d.process(b.data(), sr / 4, sr);
            return b;
        };
        std::vector<float> noBias = proc(0.0f);
        audio::Distortion base;
        base.setEnabled(true);
        base.setDrive(5.0f);
        base.setMix(1.0f);
        std::vector<float> ref = sineStereo(sr / 4, 300.0, 0.6, sr);
        base.process(ref.data(), sr / 4, sr);
        bool identical = noBias.size() == ref.size();
        for (size_t i = 0; identical && i < noBias.size(); ++i) {
            if (noBias[i] != ref[i]) {
                identical = false;
            }
        }
        check(identical, "distortion bias 0 is bit-for-bit the symmetric shaper");
        audio::Distortion db;
        check(db.bias() == 0.0f, "distortion bias defaults to 0 (symmetric)");
    }

    // --- Amp/Cab: preamp drive + a speaker-cabinet frequency voicing ---------
    {
        auto power = [](const std::vector<float>& b, double f, int srate) {
            const double w = 2.0 * 3.14159265358979 * f / srate;
            const double c = 2.0 * std::cos(w);
            double s1 = 0.0, s2 = 0.0;
            for (size_t i = 0; i < b.size(); i += 2) {
                const double s0 = static_cast<double>(b[i]) + c * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            return s1 * s1 + s2 * s2 - c * s1 * s2;
        };
        // Cabinet rolloff: a 6 kHz tone comes out attenuated relative to a 1 kHz tone (same input).
        auto cabRatio = [&](double f) {
            audio::AmpCab a;
            a.setEnabled(true);
            a.setDrive(0.3f);
            a.setPresence(0.4f);
            a.setTone(5000.0f);
            a.setMix(1.0f);
            std::vector<float> b = sineStereo(sr / 2, f, 0.3, sr);
            const double before = rms(b);
            a.process(b.data(), sr / 2, sr);
            return rms(b) / (before + 1e-12);
        };
        check(cabRatio(6000.0) < cabRatio(1000.0) * 0.8,
              "amp/cab rolls off the highs (speaker cabinet voicing)");
        // Preamp drive adds harmonics: a 200 Hz tone gains a strong 3rd harmonic (600 Hz).
        auto third = [&](float drive) {
            audio::AmpCab a;
            a.setEnabled(true);
            a.setDrive(drive);
            a.setPresence(0.0f);
            a.setTone(8000.0f);
            a.setMix(1.0f);
            std::vector<float> b = sineStereo(sr / 2, 200.0, 0.3, sr);
            a.process(b.data(), sr / 2, sr);
            return power(b, 600.0, sr);
        };
        check(third(0.9f) > third(0.0f) * 20.0 + 1.0, "amp/cab preamp drive adds harmonics");
        // Presence lifts the ~2.5 kHz bite region.
        auto presRms = [&](float p) {
            audio::AmpCab a;
            a.setEnabled(true);
            a.setDrive(0.1f);
            a.setPresence(p);
            a.setTone(8000.0f);
            a.setMix(1.0f);
            std::vector<float> b = sineStereo(sr / 2, 2500.0, 0.3, sr);
            a.process(b.data(), sr / 2, sr);
            return rms(b);
        };
        check(presRms(1.0f) > presRms(0.0f) * 1.4, "amp/cab presence lifts the bite region");
        // Disabled → transparent; defaults.
        audio::AmpCab off;
        std::vector<float> q = sineStereo(sr / 4, 500.0, 0.5, sr);
        const std::vector<float> ref = q;
        off.process(q.data(), sr / 4, sr);
        check(q == ref, "a disabled amp/cab is transparent");
        check(!audio::AmpCab().enabled(), "amp/cab defaults to off");
    }

    // --- Distortion curves: each mode shapes differently --------------------
    {
        auto shape = [&](audio::Distortion::Curve c, float x) {
            audio::Distortion d;
            d.setEnabled(true);
            d.setDrive(2.0f);
            d.setMix(1.0f);
            d.setCurve(c);
            std::vector<float> b(2, x); // one stereo frame at level x
            d.process(b.data(), 1, sr);
            return b[0];
        };
        using C = audio::Distortion::Curve;
        // Hard clip caps at ±1: a hot input (x*drive = 1.6) clamps exactly to 1.
        check(std::fabs(shape(C::Hard, 0.8f) - 1.0f) < 1e-5f, "hard-clip caps at +1");
        // Soft (tanh) stays below the hard-clip ceiling for the same input.
        check(shape(C::Soft, 0.8f) < shape(C::Hard, 0.8f), "soft curve is gentler than hard clip");
        // Wavefolder reflects past ±1: at x*drive = 1.6 → 2 - 1.6 = 0.4 (not clipped high).
        check(std::fabs(shape(C::Fold, 0.8f) - 0.4f) < 1e-4f, "wavefolder reflects past the rails");
        // Sine-fold wraps: at x*drive = 1.6, sin(1.6*pi/2) is well below 1.
        check(shape(C::SineFold, 0.8f) < 0.85f, "sine-fold wraps the waveform");
        // The three shaping curves give distinct outputs for the same input.
        check(std::fabs(shape(C::Hard, 0.8f) - shape(C::Fold, 0.8f)) > 0.1f,
              "hard and fold curves differ");
        // Tube is asymmetric: the positive half is louder than the negative.
        check(shape(C::Tube, 0.8f) > -shape(C::Tube, -0.8f),
              "tube curve shapes the positive half harder than the negative (asymmetric)");

        // That asymmetry adds a DC/even-harmonic offset on a symmetric sine that the symmetric
        // curves don't.
        auto meanOf = [&](audio::Distortion::Curve c) {
            audio::Distortion d;
            d.setEnabled(true);
            d.setDrive(3.0f);
            d.setMix(1.0f);
            d.setCurve(c);
            std::vector<float> b = sineStereo(sr, 200.0, 0.8, sr);
            d.process(b.data(), sr, sr);
            double s = 0.0;
            for (size_t i = 0; i < b.size(); i += 2) {
                s += static_cast<double>(b[i]);
            }
            return s / static_cast<double>(b.size() / 2);
        };
        check(std::fabs(meanOf(C::Soft)) < 0.02, "a symmetric curve leaves no DC offset");
        check(meanOf(C::Tube) > 0.05, "the tube curve introduces a DC/even-harmonic offset");
    }

    // --- Vibrato: a fully-wet swept delay wobbles the pitch of a steady tone -
    {
        // Windowed rising-zero-crossing frequency of the left channel over `win`-sample windows.
        auto winFreqs = [&](const std::vector<float>& s, int win) {
            std::vector<double> f;
            const int n = static_cast<int>(s.size() / 2);
            for (int start = 0; start + win <= n; start += win) {
                int cross = 0;
                for (int i = start + 1; i < start + win; ++i) {
                    if (s[static_cast<size_t>(i - 1) * 2] <= 0.0f &&
                        s[static_cast<size_t>(i) * 2] > 0.0f) {
                        ++cross;
                    }
                }
                f.push_back(static_cast<double>(cross) * sr / win);
            }
            return f;
        };
        auto spread = [](const std::vector<double>& f) {
            double lo = 1e9, hi = -1e9;
            for (double v : f) {
                lo = std::min(lo, v);
                hi = std::max(hi, v);
            }
            return f.empty() ? 0.0 : hi - lo;
        };

        // Dry control: a steady 440 Hz tone has near-constant windowed frequency.
        std::vector<float> dry = sineStereo(sr, 440.0, 0.5, sr);
        const double drySpread = spread(winFreqs(dry, sr / 20)); // 50 ms windows

        // Vibrato on: the pitch wobbles, so the windowed frequency swings widely.
        audio::Vibrato vib;
        vib.setEnabled(true);
        vib.setRate(6.0f);
        vib.setDepth(8.0f);
        std::vector<float> wet = sineStereo(sr, 440.0, 0.5, sr);
        vib.process(wet.data(), sr, sr);
        const double wetSpread = spread(winFreqs(wet, sr / 20));
        check(wetSpread > 50.0 && wetSpread > drySpread,
              "vibrato modulates the pitch (windowed frequency swings widely)");
        check(rms(wet) > 0.0, "vibrato passes signal");

        // Depth 0: no wobble — the windowed frequency stays as steady as the dry control.
        audio::Vibrato flat;
        flat.setEnabled(true);
        flat.setRate(6.0f);
        flat.setDepth(0.0f);
        std::vector<float> fb = sineStereo(sr, 440.0, 0.5, sr);
        flat.process(fb.data(), sr, sr);
        check(spread(winFreqs(fb, sr / 20)) < wetSpread,
              "vibrato at depth 0 does not wobble the pitch");

        // Disabled → bit-identical passthrough.
        audio::Vibrato off;
        std::vector<float> a = sineStereo(sr / 4, 440.0, 0.5, sr);
        std::vector<float> b2 = a;
        off.process(b2.data(), sr / 4, sr);
        bool identical = true;
        for (size_t i = 0; i < a.size(); ++i) {
            if (a[i] != b2[i]) {
                identical = false;
            }
        }
        check(identical, "disabled vibrato is a bit-identical passthrough");
        check(!audio::Vibrato().enabled(), "vibrato defaults to disabled");

        // Tempo sync: the LFO rate locks to the transport (1/8 @ 120 BPM = 4 Hz).
        audio::Vibrato vs;
        vs.setSync(true);
        vs.setSyncDivision(3); // 1/8 in the shared modulation division set
        vs.updateTempo(120.0);
        check(std::fabs(vs.rate() - 4.0f) < 0.01f, "synced vibrato runs at 4 Hz for 1/8 @120 BPM");

        // LFO shape: a Square-shaped vibrato jumps the delay between two values (a two-pitch trill),
        // producing a different output than the smooth Sine — while both stay bounded. Sine is the
        // default and unchanged.
        auto shapeOut = [&](audio::Vibrato::Shape shp) {
            audio::Vibrato v;
            v.setEnabled(true);
            v.setRate(6.0f);
            v.setDepth(6.0f);
            v.setShape(shp);
            std::vector<float> b = sineStereo(sr / 2, 330.0, 0.5, sr);
            v.process(b.data(), sr / 2, sr);
            return b;
        };
        const std::vector<float> sineV = shapeOut(audio::Vibrato::Shape::Sine);
        const std::vector<float> sqV = shapeOut(audio::Vibrato::Shape::Square);
        double diff = 0.0;
        float sqPk = 0.0f;
        for (size_t i = 0; i < sineV.size(); ++i) {
            diff += std::fabs(static_cast<double>(sineV[i] - sqV[i]));
            sqPk = std::max(sqPk, std::fabs(sqV[i]));
        }
        check(diff > 1.0, "a square-shaped vibrato differs from the sine shape (two-pitch trill)");
        check(sqPk < 1.5f, "the square-shaped vibrato stays bounded");
        check(audio::Vibrato().shape() == audio::Vibrato::Shape::Sine, "vibrato shape defaults to sine");
    }

    // --- Rotary (Leslie): amplitude + Doppler modulation + stereo rotation ---
    {
        // Windowed left-channel RMS spread (amplitude modulation) and rising-zero-crossing frequency
        // spread (Doppler pitch modulation) over 50 ms windows.
        auto rmsSpread = [&](const std::vector<float>& s, int win) {
            double lo = 1e9, hi = -1e9;
            const int n = static_cast<int>(s.size() / 2);
            for (int start = 0; start + win <= n; start += win) {
                double e = 0.0;
                for (int i = 0; i < win; ++i) {
                    const float l = s[static_cast<size_t>((start + i) * 2)];
                    e += static_cast<double>(l) * l;
                }
                const double r = std::sqrt(e / win);
                lo = std::min(lo, r);
                hi = std::max(hi, r);
            }
            return hi - lo;
        };
        auto freqSpread = [&](const std::vector<float>& s, int win) {
            double lo = 1e9, hi = -1e9;
            const int n = static_cast<int>(s.size() / 2);
            for (int start = 0; start + win <= n; start += win) {
                int cross = 0;
                for (int i = start + 1; i < start + win; ++i) {
                    if (s[static_cast<size_t>(i - 1) * 2] <= 0.0f && s[static_cast<size_t>(i) * 2] > 0.0f)
                        ++cross;
                }
                const double f = static_cast<double>(cross) * sr / win;
                lo = std::min(lo, f);
                hi = std::max(hi, f);
            }
            return hi - lo;
        };
        const int win = sr / 20; // 50 ms
        std::vector<float> dry = sineStereo(sr, 440.0, 0.5, sr);
        const double dryRmsSpread = rmsSpread(dry, win);
        const double dryFreqSpread = freqSpread(dry, win);

        audio::Rotary rot;
        rot.setEnabled(true);
        rot.setRate(5.0f);
        rot.setDepth(0.9f);
        std::vector<float> b = sineStereo(sr, 440.0, 0.5, sr);
        rot.process(b.data(), sr, sr);
        check(rmsSpread(b, win) > dryRmsSpread + 0.02,
              "rotary amplitude-modulates the signal (level swings as the horn rotates)");
        check(freqSpread(b, win) > dryFreqSpread + 10.0,
              "rotary Doppler-modulates the pitch (windowed frequency swings)");
        // Stereo rotation: the two mics are in opposition, so L and R differ.
        double lr = 0.0;
        for (int i = sr / 2; i < sr; ++i) {
            lr += std::fabs(static_cast<double>(b[static_cast<size_t>(i) * 2] -
                                                b[static_cast<size_t>(i) * 2 + 1]));
        }
        check(lr > 1.0, "rotary decorrelates the stereo image (rotating mics)");

        // Disabled → bit-identical passthrough; sensible defaults.
        audio::Rotary off;
        std::vector<float> a = sineStereo(sr / 4, 440.0, 0.5, sr);
        std::vector<float> a2 = a;
        off.process(a2.data(), sr / 4, sr);
        bool same = true;
        for (size_t i = 0; i < a.size(); ++i) {
            if (a[i] != a2[i]) same = false;
        }
        check(same, "a disabled rotary is a bit-identical passthrough");
        check(!audio::Rotary().enabled(), "rotary defaults to off");

        // Preamp drive: overdriving the rotating signal adds harmonics (more high-frequency content).
        auto rotHf = [&](float drive) {
            audio::Rotary rd;
            rd.setEnabled(true);
            rd.setRate(6.0f);
            rd.setDepth(0.3f);
            rd.setMix(1.0f);
            rd.setDrive(drive);
            std::vector<float> rb = sineStereo(sr / 4, 300.0, 0.7, sr);
            rd.process(rb.data(), sr / 4, sr);
            double e = 0.0;
            for (size_t i = 2; i < rb.size(); i += 2) {
                const double d = static_cast<double>(rb[i] - rb[i - 2]); // left-channel first difference
                e += d * d;
            }
            return e;
        };
        check(rotHf(0.8f) > rotHf(0.0f) * 1.2,
              "rotary preamp drive adds harmonics (tube grit on the swirl)");
        check(audio::Rotary().drive() == 0.0f, "rotary drive defaults to clean (0)");
    }

    // --- Chorus: a dry mono signal becomes wet + decorrelated ----------------
    {
        audio::Chorus chorus;
        chorus.setEnabled(true);
        chorus.setMix(0.5f);
        std::vector<float> sig = sineStereo(sr, 440.0, 0.5, sr);
        chorus.process(sig.data(), sr, sr);
        // After the modulated delay, left and right differ (quadrature LFO widens the image).
        double diff = 0.0;
        for (int i = sr / 2; i < sr; ++i) {
            diff += std::fabs(static_cast<double>(sig[static_cast<size_t>(i) * 2] -
                                                  sig[static_cast<size_t>(i) * 2 + 1]));
        }
        check(diff > 0.0, "chorus decorrelates the stereo image");
        check(rms(sig) > 0.0, "chorus still passes signal");

        // Feedback: routing the wet back changes the sound vs a clean (feedback 0) chorus.
        auto chorusOut = [&](float fb) {
            audio::Chorus c;
            c.setEnabled(true);
            c.setMix(0.7f);
            c.setFeedback(fb);
            std::vector<float> b = sineStereo(sr / 2, 440.0, 0.5, sr);
            c.process(b.data(), sr / 2, sr);
            return b;
        };
        const std::vector<float> clean = chorusOut(0.0f);
        const std::vector<float> resonant = chorusOut(0.8f);
        double fbDiff = 0.0;
        for (size_t i = 0; i < clean.size(); ++i) {
            fbDiff += std::fabs(static_cast<double>(clean[i] - resonant[i]));
        }
        check(fbDiff > 1.0, "chorus feedback changes the sound (deeper/resonant)");
        audio::Chorus dc;
        check(dc.feedback() == 0.0f, "chorus feedback defaults to 0");

        // Stereo width: width 0 collapses the wet to mono (L==R); width 1 keeps it decorrelated.
        auto lrDiff = [&](float width) {
            audio::Chorus c;
            c.setEnabled(true);
            c.setMix(1.0f); // fully wet, so width acts on the whole output
            c.setWidth(width);
            std::vector<float> b = sineStereo(sr, 440.0, 0.5, sr);
            c.process(b.data(), sr, sr);
            double d = 0.0;
            for (int i = sr / 2; i < sr; ++i) {
                d += std::fabs(static_cast<double>(b[static_cast<size_t>(i) * 2] -
                                                   b[static_cast<size_t>(i) * 2 + 1]));
            }
            return d;
        };
        check(lrDiff(0.0f) < 1e-3, "chorus width 0 collapses the wet to mono (L==R)");
        check(lrDiff(1.0f) > 1.0, "chorus width 1 keeps the wet decorrelated (wide)");
        audio::Chorus dw;
        check(std::fabs(dw.width() - 1.0f) < 1e-6f, "chorus width defaults to 1 (natural)");

        // Ensemble voices: each voice is a modulated delay tap, so an impulse produces one echo per
        // voice. Count the distinct echo clusters on the left channel (LFO nearly frozen so the taps
        // sit at stable, well-separated delays).
        auto tapCount = [&](int voices) {
            audio::Chorus c;
            c.setEnabled(true);
            c.setMix(1.0f);
            c.setDepth(8.0f);
            c.setRate(0.01f); // ~frozen LFO over the short window → stable tap delays
            c.setVoices(voices);
            std::vector<float> b(static_cast<size_t>(sr) / 10 * 2, 0.0f); // 100 ms stereo
            b[0] = 1.0f;
            b[1] = 1.0f; // an impulse
            c.process(b.data(), sr / 10, sr);
            float peak = 0.0f;
            for (size_t i = 0; i < b.size(); i += 2) {
                peak = std::max(peak, std::fabs(b[i]));
            }
            const float thr = 0.3f * peak;
            int clusters = 0;
            bool inCluster = false;
            for (size_t i = 0; i < b.size(); i += 2) {
                const float v = std::fabs(b[i]);
                if (v > thr && !inCluster) {
                    ++clusters;
                    inCluster = true;
                } else if (v <= thr) {
                    inCluster = false;
                }
            }
            return clusters;
        };
        check(tapCount(1) >= 1, "a single-voice chorus produces a delay tap");
        check(tapCount(3) > tapCount(1), "a 3-voice ensemble chorus produces more delay taps");
        check(audio::Chorus().voices() == 1, "chorus defaults to a single voice");
    }

    // --- Parametric EQ: shelves boost/cut their band -------------------------
    {
        // A +12 dB low shelf makes a low (80 Hz) tone louder; a -12 dB cut makes it quieter.
        std::vector<float> lowTone = sineStereo(sr, 80.0, 0.3, sr);
        const double flat = rms(lowTone);

        audio::ParametricEQ boost;
        boost.setEnabled(true);
        boost.setLowGain(12.0f);
        std::vector<float> up = sineStereo(sr, 80.0, 0.3, sr);
        boost.process(up.data(), sr, sr);
        check(rms(up) > flat * 1.5, "low-shelf boost raises low-end level");

        audio::ParametricEQ cut;
        cut.setEnabled(true);
        cut.setLowGain(-12.0f);
        std::vector<float> down = sineStereo(sr, 80.0, 0.3, sr);
        cut.process(down.data(), sr, sr);
        check(rms(down) < flat * 0.7, "low-shelf cut lowers low-end level");

        // Second, independent mid bell: boosting it lifts a tone at its own centre frequency.
        std::vector<float> midTone = sineStereo(sr, 3500.0, 0.3, sr);
        const double midFlat = rms(midTone);
        audio::ParametricEQ eq2;
        eq2.setEnabled(true);
        eq2.setMid2(3500.0f, 3.0f, 12.0f);
        std::vector<float> m2 = sineStereo(sr, 3500.0, 0.3, sr);
        eq2.process(m2.data(), sr, sr);
        check(rms(m2) > midFlat * 1.3, "second mid band boost lifts a tone at its centre frequency");

        // Default bands (all 0 dB, incl. the new 2nd mid) are transparent — no level change.
        audio::ParametricEQ flatEq;
        flatEq.setEnabled(true);
        std::vector<float> pass = sineStereo(sr, 3500.0, 0.3, sr);
        flatEq.process(pass.data(), sr, sr);
        check(std::fabs(rms(pass) - midFlat) < midFlat * 0.02,
              "an EQ with default bands (0 dB 2nd mid) is transparent");
        audio::ParametricEQ deq;
        check(std::fabs(deq.mid2Gain()) < 1e-6f, "2nd mid band defaults to 0 dB");

        // Third, independent mid bell: boosting it lifts a tone at its own centre frequency, while a
        // tone away from all three mids is left alone (confirming it's a localized band).
        std::vector<float> hiTone = sineStereo(sr, 7000.0, 0.3, sr);
        const double hiFlat = rms(hiTone);
        audio::ParametricEQ eq3;
        eq3.setEnabled(true);
        eq3.setMid3(7000.0f, 3.0f, 12.0f);
        std::vector<float> m3 = sineStereo(sr, 7000.0, 0.3, sr);
        eq3.process(m3.data(), sr, sr);
        check(rms(m3) > hiFlat * 1.3, "third mid band boost lifts a tone at its centre frequency");
        std::vector<float> away = sineStereo(sr, 500.0, 0.3, sr);
        const double awayFlat = rms(away);
        eq3.process(away.data(), sr, sr);
        check(std::fabs(rms(away) - awayFlat) < awayFlat * 0.05,
              "third mid band leaves a tone far from its centre alone");
        check(std::fabs(deq.mid3Gain()) < 1e-6f, "3rd mid band defaults to 0 dB");
    }

    // --- Master resonant filter (DJ filter): LP/HP attenuate their stop-bands -----
    {
        // Low-pass at 500 Hz strongly attenuates a 5 kHz tone but passes a 200 Hz tone.
        auto lpAt = [&](double toneHz) {
            audio::MasterFilter f;
            f.setEnabled(true);
            f.setMode(audio::StateVariableFilter::Mode::LowPass);
            f.setCutoff(500.0f);
            std::vector<float> b = sineStereo(sr, toneHz, 0.5, sr);
            f.process(b.data(), sr, sr);
            return rms(b);
        };
        check(lpAt(5000.0) < lpAt(200.0) * 0.3, "master LP filter cuts highs, passes lows");

        // High-pass at 2 kHz attenuates a 200 Hz tone but passes a 5 kHz tone.
        auto hpAt = [&](double toneHz) {
            audio::MasterFilter f;
            f.setEnabled(true);
            f.setMode(audio::StateVariableFilter::Mode::HighPass);
            f.setCutoff(2000.0f);
            std::vector<float> b = sineStereo(sr, toneHz, 0.5, sr);
            f.process(b.data(), sr, sr);
            return rms(b);
        };
        check(hpAt(200.0) < hpAt(5000.0) * 0.3, "master HP filter cuts lows, passes highs");

        // Resonance boosts a tone sitting right at the cutoff.
        auto resoAt = [&](float reso) {
            audio::MasterFilter f;
            f.setEnabled(true);
            f.setMode(audio::StateVariableFilter::Mode::LowPass);
            f.setCutoff(1000.0f);
            f.setResonance(reso);
            std::vector<float> b = sineStereo(sr, 1000.0, 0.5, sr);
            f.process(b.data(), sr, sr);
            return rms(b);
        };
        check(resoAt(10.0f) > resoAt(0.7f) * 1.3, "filter resonance peaks a tone at the cutoff");

        // Disabled → bit-identical passthrough; defaults are transparent (wide-open LP, off).
        audio::MasterFilter off;
        std::vector<float> a = sineStereo(sr / 4, 440.0, 0.5, sr);
        std::vector<float> a2 = a;
        off.process(a2.data(), sr / 4, sr);
        bool same = true;
        for (size_t i = 0; i < a.size(); ++i) {
            if (a[i] != a2[i]) same = false;
        }
        check(same, "a disabled master filter is a bit-identical passthrough");
        check(!audio::MasterFilter().enabled() && audio::MasterFilter().cutoff() > 19000.0f,
              "master filter defaults to off and wide open");

        // Drive: overdriving into a wide-open filter adds harmonics (more high-frequency content).
        auto hfEnergy = [](const std::vector<float>& b) {
            double e = 0.0;
            for (size_t i = 2; i < b.size(); i += 2) {
                const double d = static_cast<double>(b[i] - b[i - 2]); // left-channel first difference
                e += d * d;
            }
            return e;
        };
        auto driven = [&](float drive) {
            audio::MasterFilter f;
            f.setEnabled(true);
            f.setCutoff(20000.0f); // wide open, so the drive harmonics pass through
            f.setResonance(0.7f);
            f.setDrive(drive);
            std::vector<float> b = sineStereo(sr / 4, 300.0, 0.8, sr);
            f.process(b.data(), sr / 4, sr);
            return b;
        };
        check(hfEnergy(driven(0.8f)) > hfEnergy(driven(0.0f)) * 1.2,
              "master filter drive adds harmonics (analog growl)");
        check(audio::MasterFilter().drive() == 0.0f, "master filter drive defaults to clean (0)");
    }

    // --- Multiband stereo imager: per-band width over a pure-side signal -----
    {
        // A pure-"side" stereo tone at `f` (L = +sin, R = -sin); its side energy = 2*sum|L|.
        auto sideTone = [&](double f) {
            std::vector<float> b(static_cast<size_t>(sr) * 2, 0.0f);
            for (int i = 0; i < sr; ++i) {
                const float s = 0.5f * static_cast<float>(std::sin(2.0 * 3.14159265358979 * f * i / sr));
                b[static_cast<size_t>(i) * 2] = s;
                b[static_cast<size_t>(i) * 2 + 1] = -s;
            }
            return b;
        };
        auto sideEnergy = [&](const std::vector<float>& b) {
            double e = 0.0;
            for (int i = sr / 2; i < sr; ++i) {
                e += std::fabs(static_cast<double>(b[static_cast<size_t>(i) * 2] -
                                                   b[static_cast<size_t>(i) * 2 + 1]));
            }
            return e;
        };
        // A deep-bass (60 Hz) side tone, well below a raised low crossover so the one-pole split
        // captures it almost entirely in the low band: the low-band width control scales its side.
        auto lowSideAt = [&](float lowW) {
            audio::StereoImager im;
            im.setEnabled(true);
            im.setCrossoverLow(500.0f);
            im.setBandWidth(0, lowW);
            im.setBandWidth(1, 1.0f);
            im.setBandWidth(2, 1.0f);
            std::vector<float> b = sideTone(60.0);
            im.process(b.data(), sr, sr);
            return sideEnergy(b);
        };
        const double base = lowSideAt(1.0f);
        check(lowSideAt(0.0f) < base * 0.3, "imager low-band width 0 collapses the low side to mono");
        check(lowSideAt(2.0f) > base * 1.5, "imager low-band width 2 widens the low side");
        // Band independence: widening only the HIGH band leaves the low-band side ~unchanged.
        auto highWidenLowSide = [&]() {
            audio::StereoImager im;
            im.setEnabled(true);
            im.setCrossoverLow(500.0f);
            im.setBandWidth(0, 1.0f);
            im.setBandWidth(1, 1.0f);
            im.setBandWidth(2, 2.0f);
            std::vector<float> b = sideTone(60.0);
            im.process(b.data(), sr, sr);
            return sideEnergy(b);
        };
        check(std::fabs(highWidenLowSide() - base) < base * 0.2,
              "imager high-band width doesn't move the low band (band independence)");

        // All widths at 1 → transparent (exact mid/side reconstruction).
        audio::StereoImager flat;
        flat.setEnabled(true);
        flat.setBandWidth(0, 1.0f);
        flat.setBandWidth(1, 1.0f);
        flat.setBandWidth(2, 1.0f);
        std::vector<float> t = sineStereo(sr / 4, 300.0, 0.5, sr);
        std::vector<float> tref = t;
        flat.process(t.data(), sr / 4, sr);
        bool same = true;
        for (size_t i = 0; i < t.size(); ++i) {
            if (std::fabs(t[i] - tref[i]) > 1e-4f) same = false;
        }
        check(same, "an imager with all band widths at 1 is transparent");
        check(!audio::StereoImager().enabled(), "stereo imager defaults to off");
    }

    // --- Multiband saturator: per-band drive adds harmonics to only that band ----
    {
        auto power = [](const std::vector<float>& b, double f, int srate) {
            const double w = 2.0 * 3.14159265358979 * f / srate;
            const double c = 2.0 * std::cos(w);
            double s1 = 0.0, s2 = 0.0;
            for (size_t i = 0; i < b.size(); i += 2) {
                const double s0 = static_cast<double>(b[i]) + c * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            return s1 * s1 + s2 * s2 - c * s1 * s2;
        };
        // A low (100 Hz) sine through the saturator; tanh is odd, so driving the LOW band should add a
        // measurable 3rd harmonic (300 Hz), while driving only the HIGH band should not.
        auto thirdHarmonic = [&](int driveBand) {
            audio::MultibandSaturator sat;
            sat.setEnabled(true);
            sat.setCrossoverLow(300.0f);  // 100 Hz is well inside the low band
            sat.setCrossoverHigh(3000.0f);
            if (driveBand >= 0) sat.setDrive(driveBand, 1.0f);
            std::vector<float> b = sineStereo(sr / 2, 100.0, 0.5, sr);
            sat.process(b.data(), sr / 2, sr);
            return power(b, 300.0, sr);
        };
        const double clean = thirdHarmonic(-1);       // no drive
        const double lowDriven = thirdHarmonic(0);    // low-band drive
        const double highDriven = thirdHarmonic(2);   // high-band drive (100 Hz not in it)
        check(lowDriven > clean * 20.0 + 1.0,
              "low-band drive adds a 3rd harmonic to a low tone");
        check(highDriven < lowDriven * 0.1,
              "high-band drive leaves a low tone's harmonics alone (band independence)");
        // All drives 0 → exact reconstruction (the split sums back to the input).
        audio::MultibandSaturator flat;
        flat.setEnabled(true);
        std::vector<float> t = sineStereo(sr / 4, 440.0, 0.5, sr);
        std::vector<float> tref = t;
        flat.process(t.data(), sr / 4, sr);
        bool same = true;
        for (size_t i = 0; i < t.size(); ++i) {
            if (std::fabs(t[i] - tref[i]) > 1e-5f) same = false;
        }
        check(same, "a saturator with all band drives at 0 is transparent");
        check(audio::MultibandSaturator().drive(0) == 0.0f &&
                  !audio::MultibandSaturator().enabled(),
              "multiband saturator defaults to off/clean");
    }

    // --- Multiband transient shaper: per-band attack targets its own band -----
    {
        // A high-frequency (5 kHz) burst with a sharp onset, mostly in the high band.
        auto burst = [&]() {
            std::vector<float> b(static_cast<size_t>(sr) / 10 * 2, 0.0f); // 100 ms stereo
            for (int i = 0; i < sr / 10; ++i) {
                const float env = std::exp(-static_cast<float>(i) / (0.02f * sr));
                const float s = env * static_cast<float>(std::sin(2.0 * 3.14159265358979 * 5000.0 * i / sr));
                b[static_cast<size_t>(i) * 2] = s;
                b[static_cast<size_t>(i) * 2 + 1] = s;
            }
            return b;
        };
        auto onsetPeak = [&](const std::vector<float>& b) {
            float p = 0.0f;
            for (int i = 0; i < sr / 200; ++i) p = std::max(p, std::fabs(b[static_cast<size_t>(i) * 2]));
            return p;
        };
        auto shaped = [&](float loAtk, float hiAtk) {
            audio::MultibandTransientShaper mt;
            mt.setEnabled(true);
            mt.setCrossoverLow(200.0f);
            mt.setCrossoverHigh(2000.0f);
            mt.setAttack(0, loAtk);
            mt.setAttack(2, hiAtk);
            std::vector<float> b = burst();
            mt.process(b.data(), sr / 10, sr);
            return onsetPeak(b);
        };
        const float flat = onsetPeak(burst());
        const float hiBoost = shaped(0.0f, 1.0f); // boost the high band's attack
        const float loBoost = shaped(1.0f, 0.0f); // boost the low band's attack
        check(hiBoost > flat * 2.0f, "high-band attack lifts a high burst's onset");
        check(hiBoost > loBoost * 2.0f,
              "attack targets its own band (high boost >> low boost on a high burst)");
        // All bands flat → exact reconstruction (transparent).
        audio::MultibandTransientShaper mt;
        mt.setEnabled(true);
        std::vector<float> t = sineStereo(sr / 4, 440.0, 0.5, sr);
        std::vector<float> ref = t;
        mt.process(t.data(), sr / 4, sr);
        bool same = true;
        for (size_t i = 0; i < t.size(); ++i)
            if (std::fabs(t[i] - ref[i]) > 1e-5f) same = false;
        check(same, "a flat multiband transient shaper is transparent");
        check(audio::MultibandTransientShaper().attack(0) == 0.0f &&
                  !audio::MultibandTransientShaper().enabled(),
              "multiband transient shaper defaults to off/flat");
    }

    // --- Bitcrusher: quantization changes the signal but keeps energy --------
    {
        audio::Bitcrusher crush;
        crush.setEnabled(true);
        crush.setBits(3.0f);
        crush.setDownsample(8.0f);
        crush.setMix(1.0f);
        std::vector<float> sig = sineStereo(sr / 10, 440.0, 0.5, sr);
        const std::vector<float> ref = sig;
        crush.process(sig.data(), sr / 10, sr);
        double changed = 0.0;
        for (size_t i = 0; i < sig.size(); ++i) {
            changed += std::fabs(static_cast<double>(sig[i] - ref[i]));
        }
        check(changed > 0.0, "bitcrusher alters the signal");
        check(rms(sig) > 0.0, "bitcrusher still passes signal");

        // Post tone: a low LP setting darkens the crushed output (rolls off the aliasing/quant fizz).
        auto crushHf = [&](float tone) {
            audio::Bitcrusher c;
            c.setEnabled(true);
            c.setBits(4.0f);
            c.setDownsample(6.0f);
            c.setMix(1.0f);
            c.setTone(tone);
            std::vector<float> b = sineStereo(sr / 2, 500.0, 0.5, sr);
            c.process(b.data(), sr / 2, sr);
            double s = 0.0;
            for (size_t i = 2; i < b.size(); i += 2) {
                const double d = static_cast<double>(b[i] - b[i - 2]);
                s += d * d;
            }
            return s;
        };
        check(crushHf(600.0f) < crushHf(20000.0f) * 0.7,
              "bitcrusher post tone rolls off the crushed highs");
        audio::Bitcrusher dc;
        check(dc.tone() == 20000.0f, "bitcrusher tone defaults to open (20 kHz)");
    }

    // --- Phaser: sweeping all-pass notches change the signal over time -------
    {
        audio::Phaser ph;
        ph.setEnabled(true);
        ph.setMix(0.7f);
        std::vector<float> sig = sineStereo(sr, 600.0, 0.5, sr);
        const std::vector<float> ref = sig;
        ph.process(sig.data(), sr, sr);
        double changed = 0.0;
        for (size_t i = 0; i < sig.size(); ++i) {
            changed += std::fabs(static_cast<double>(sig[i] - ref[i]));
        }
        check(changed > 0.0, "phaser alters the signal");
        check(rms(sig) > 0.0, "phaser still passes signal");

        // Stage count: more all-pass stages carve a different notch pattern, so the output changes.
        auto phaseOut = [&](int stages) {
            audio::Phaser p;
            p.setEnabled(true);
            p.setMix(0.7f);
            p.setFeedback(0.0f); // isolate the stage count from feedback resonance
            p.setStages(stages);
            std::vector<float> b = sineStereo(sr / 2, 600.0, 0.5, sr);
            p.process(b.data(), sr / 2, sr);
            return b;
        };
        const std::vector<float> s2 = phaseOut(2);
        const std::vector<float> s12 = phaseOut(12);
        double stageDiff = 0.0;
        for (size_t i = 0; i < s2.size(); ++i) {
            stageDiff += std::fabs(static_cast<double>(s2[i] - s12[i]));
        }
        check(stageDiff > 1.0, "phaser stage count changes the sound (more stages = more notches)");
        audio::Phaser dp;
        check(dp.stages() == 4, "phaser stage count defaults to 4");
        dp.setStages(99);
        check(dp.stages() == 12, "phaser stage count clamps to the maximum");

        // Stereo: a mono input stays mono through the (default) mono phaser, but stereo mode offsets
        // the right channel's sweep by 90° so L and R decorrelate.
        auto lrDiff = [&](bool stereo) {
            audio::Phaser p;
            p.setEnabled(true);
            p.setMix(0.7f);
            p.setStereo(stereo);
            std::vector<float> b = sineStereo(sr / 2, 600.0, 0.5, sr); // mono input (L==R)
            p.process(b.data(), sr / 2, sr);
            double d = 0.0;
            for (size_t i = 0; i + 1 < b.size(); i += 2) {
                d += std::fabs(static_cast<double>(b[i] - b[i + 1]));
            }
            return d;
        };
        check(lrDiff(false) < 1e-6, "a mono phaser keeps L and R identical for a mono input");
        check(lrDiff(true) > 1.0, "stereo phaser decorrelates L and R (90°-offset sweep)");
        check(!audio::Phaser().stereo(), "phaser stereo defaults to off");
    }

    // --- Mixer: master gain scales; disabled chain is transparent ------------
    {
        audio::Mixer mixer; // all effects disabled by default
        mixer.setMasterGain(0.5f);
        std::vector<float> buf = sineStereo(1000, 440.0, 0.8, sr);
        const std::vector<float> ref = buf;
        mixer.process(buf.data(), 1000, sr);
        bool halved = true;
        for (size_t i = 0; i < buf.size(); ++i) {
            if (std::fabs(buf[i] - ref[i] * 0.5f) > 1e-5f) {
                halved = false;
                break;
            }
        }
        check(halved, "master gain scales the bus and effects are transparent when disabled");
    }

    // --- Mixer: master balance pans the final output ------------------------
    {
        auto chanRms = [](const std::vector<float>& b, int chan) {
            double s = 0.0;
            int c = 0;
            for (size_t i = static_cast<size_t>(chan); i < b.size(); i += 2) {
                s += static_cast<double>(b[i]) * b[i];
                ++c;
            }
            return c > 0 ? std::sqrt(s / c) : 0.0;
        };
        // Hard right: the left channel is silenced, the right passes.
        audio::Mixer right;
        right.setMasterGain(1.0f);
        right.setMasterBalance(1.0f);
        std::vector<float> rb = sineStereo(1000, 440.0, 0.5, sr);
        right.process(rb.data(), 1000, sr);
        check(chanRms(rb, 0) < 1e-6 && chanRms(rb, 1) > 0.2,
              "master balance hard-right silences the left channel");
        // Centre (default) is balanced: both channels equal.
        audio::Mixer centre;
        centre.setMasterGain(1.0f);
        std::vector<float> cb = sineStereo(1000, 440.0, 0.5, sr);
        centre.process(cb.data(), 1000, sr);
        check(std::fabs(chanRms(cb, 0) - chanRms(cb, 1)) < 1e-6,
              "master balance defaults to centre (channels equal)");
    }

    // --- Reverb wet-tail tone: low-cut / high-cut on the wet -----------------
    {
        auto reverbBright = [&](float lo, float hi) {
            audio::Reverb rv;
            rv.setEnabled(true);
            rv.setMix(1.0f); // fully wet, so we measure the tail's spectrum
            rv.setRoomSize(0.6f);
            rv.setDamping(0.1f);
            rv.setWetLowCut(lo);
            rv.setWetHighCut(hi);
            std::vector<float> b = sineStereo(sr / 2, 5000.0, 0.4, sr); // high tone
            std::vector<float> low = sineStereo(sr / 2, 120.0, 0.4, sr); // low tone
            for (size_t i = 0; i < b.size(); ++i) {
                b[i] += low[i];
            }
            rv.process(b.data(), sr / 2, sr);
            double h = 0.0, en = 0.0;
            for (int i = sr / 4 + 1; i < sr / 2; ++i) { // measure the settled tail
                const double d = static_cast<double>(b[static_cast<size_t>(i) * 2]) -
                                 b[static_cast<size_t>(i - 1) * 2];
                h += d * d;
                en += static_cast<double>(b[static_cast<size_t>(i) * 2]) * b[static_cast<size_t>(i) * 2];
            }
            return en > 0.0 ? h / en : 0.0;
        };
        const double open = reverbBright(0.0f, 20000.0f); // both filters off
        check(reverbBright(0.0f, 1500.0f) < open * 0.7,
              "reverb high-cut rolls off the wet tail's highs");
        check(reverbBright(1500.0f, 20000.0f) > open * 1.3,
              "reverb low-cut removes the wet tail's lows (brighter tail)");
        audio::Reverb dr;
        check(dr.wetLowCut() == 0.0f && dr.wetHighCut() == 20000.0f,
              "reverb wet tone defaults to off (0 Hz low-cut, 20 kHz high-cut)");
    }

    // --- Limiter ceiling: caps the master peak ------------------------------
    {
        auto peakOut = [&](float ceiling) {
            audio::Mixer mixer;
            mixer.setMasterGain(1.0f);
            mixer.setLimiterCeiling(ceiling);
            // A hot signal well above the ceiling.
            std::vector<float> buf(2000, 0.0f);
            for (size_t i = 0; i < buf.size(); i += 2) {
                buf[i] = 1.8f;
                buf[i + 1] = -1.8f;
            }
            mixer.process(buf.data(), 1000, sr);
            float pk = 0.0f;
            for (float v : buf) {
                pk = std::max(pk, std::fabs(v));
            }
            return pk;
        };
        check(peakOut(1.0f) <= 1.001f, "default ceiling keeps the master within ±1");
        const float low = peakOut(0.5f);
        check(low <= 0.51f && low > 0.4f, "a 0.5 ceiling caps the master near 0.5");
    }

    // --- Aux send/return buses: parallel reverb send ------------------------
    {
        // A single stereo impulse.
        std::vector<float> impulse(2000 * 2, 0.0f);
        impulse[0] = 1.0f;
        impulse[1] = 1.0f;
        auto tailEnergy = [](const std::vector<float>& b) {
            double e = 0.0;
            for (size_t i = 400 * 2; i < b.size(); ++i) {
                e += static_cast<double>(b[i]) * static_cast<double>(b[i]);
            }
            return e;
        };

        // Send at 0 → the return bus is silent, so no tail past the impulse.
        audio::Mixer mixer;
        mixer.setMasterGain(1.0f);
        std::vector<float> a = impulse;
        mixer.process(a.data(), 2000, sr);
        check(tailEnergy(a) < 1e-6, "reverb send at 0 adds no tail (transparent)");

        // Send > 0 → a reverb tail appears after the impulse on the parallel return.
        audio::Mixer wet;
        wet.setMasterGain(1.0f);
        wet.reverbReturn().setRoomSize(0.85f);
        wet.setReverbSend(0.9f);
        std::vector<float> b = impulse;
        wet.process(b.data(), 2000, sr);
        check(tailEnergy(b) > 1e-4, "reverb send bus produces a wet tail after the impulse");
    }

    // --- Tilt EQ: one knob pivots low vs. high ------------------------------
    {
        // Positive tilt brightens: a low tone loses level, a high tone gains it.
        auto level = [&](double hz, float tiltDb) {
            audio::TiltEQ t;
            t.setEnabled(true);
            t.setTilt(tiltDb);
            std::vector<float> b = sineStereo(sr, hz, 0.5, sr);
            const double in = rms(b);
            t.process(b.data(), sr, sr);
            return rms(b) / in; // gain ratio
        };
        check(level(80.0, 10.0f) < 0.85, "positive tilt cuts the low end");
        check(level(9000.0, 10.0f) > 1.15, "positive tilt boosts the high end");
        check(level(80.0, -10.0f) > 1.15, "negative tilt boosts the low end");

        // Pivot: a 1 kHz tone sits above a low (400 Hz) pivot — so a positive tilt boosts it — but
        // below a high (4 kHz) pivot, where the same tilt cuts it. Moving the pivot flips the region.
        auto levelP = [&](double hz, float tiltDb, float pivot) {
            audio::TiltEQ t;
            t.setEnabled(true);
            t.setTilt(tiltDb);
            t.setPivot(pivot);
            std::vector<float> b = sineStereo(sr, hz, 0.5, sr);
            const double in = rms(b);
            t.process(b.data(), sr, sr);
            return rms(b) / in;
        };
        check(levelP(1000.0, 10.0f, 400.0f) > levelP(1000.0, 10.0f, 4000.0f) * 1.2,
              "lowering the tilt pivot brings a mid tone into the boosted (high) region");
        audio::TiltEQ dp;
        check(std::fabs(dp.pivot() - 650.0f) < 1e-3f, "tilt pivot defaults to 650 Hz");

        // Disabled → transparent.
        audio::TiltEQ off;
        std::vector<float> sig = sineStereo(1000, 500.0, 0.5, sr);
        const std::vector<float> ref = sig;
        off.process(sig.data(), 1000, sr);
        bool same = true;
        for (size_t i = 0; i < sig.size(); ++i) {
            if (std::fabs(sig[i] - ref[i]) > 1e-6f) {
                same = false;
                break;
            }
        }
        check(same, "a disabled tilt EQ is transparent");
    }

    // --- Exciter: adds high-band harmonics, leaves the low body alone -------
    {
        // Gain ratio through the exciter for a pure tone at `hz`.
        auto ratio = [&](double hz) {
            audio::Exciter ex;
            ex.setEnabled(true);
            ex.setCrossover(4000.0f);
            ex.setAmount(0.5f);
            std::vector<float> b = sineStereo(sr, hz, 0.5, sr);
            const double in = rms(b);
            ex.process(b.data(), sr, sr);
            return rms(b) / in;
        };
        // A tone above the crossover sits in the excited band → harmonics add energy.
        check(ratio(6000.0) > 1.1, "exciter adds energy to a tone above the crossover");
        // A tone well below the crossover is barely touched (high band ≈ 0).
        check(ratio(200.0) < 1.02, "exciter leaves a low tone below the crossover almost untouched");

        // Disabled → bit-transparent.
        audio::Exciter off;
        std::vector<float> sig = sineStereo(1000, 6000.0, 0.5, sr);
        const std::vector<float> ref = sig;
        off.process(sig.data(), 1000, sr);
        bool same = true;
        for (size_t i = 0; i < sig.size(); ++i) {
            if (std::fabs(sig[i] - ref[i]) > 1e-6f) {
                same = false;
                break;
            }
        }
        check(same, "a disabled exciter is transparent");

        // Even/odd harmonic mode: a 3 kHz tone above the crossover. The odd (tanh) mode makes a 3rd
        // harmonic (9 kHz) and no 2nd; the even (squaring) mode makes a 2nd harmonic (6 kHz octave).
        auto harmPower = [&](bool even, double f) {
            audio::Exciter ex;
            ex.setEnabled(true);
            ex.setCrossover(2000.0f);
            ex.setAmount(0.8f);
            ex.setEvenHarmonics(even);
            std::vector<float> b = sineStereo(sr, 3000.0, 0.5, sr);
            ex.process(b.data(), sr, sr);
            const std::vector<float> tail(b.begin() + static_cast<std::ptrdiff_t>(b.size() / 2), b.end());
            const double w = 2.0 * 3.14159265358979 * f / sr;
            const double c = 2.0 * std::cos(w);
            double s1 = 0.0, s2 = 0.0;
            for (size_t i = 0; i < tail.size(); i += 2) {
                const double s0 = static_cast<double>(tail[i]) + c * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            return s1 * s1 + s2 * s2 - c * s1 * s2;
        };
        check(harmPower(true, 6000.0) > harmPower(false, 6000.0) * 5.0 + 1.0,
              "exciter even mode injects a 2nd harmonic the odd mode lacks");
        check(harmPower(false, 9000.0) > harmPower(true, 9000.0),
              "exciter odd mode makes more 3rd-harmonic than the even mode");
        check(!audio::Exciter().evenHarmonics(), "exciter defaults to odd harmonics");
    }

    // --- Transient shaper: reshape attack/sustain independent of level ------
    {
        // A percussive burst: a sharp onset then an exponential decay tail (mono → stereo).
        auto burst = [&]() {
            std::vector<float> b(static_cast<size_t>(sr) * 2, 0.0f); // 1 s stereo
            for (int i = 0; i < sr; ++i) {
                const double t = static_cast<double>(i) / sr;
                const double env = std::exp(-t * 12.0); // ~decays over the second
                const double s = env * std::sin(2.0 * 3.14159265358979 * 180.0 * t);
                b[static_cast<size_t>(2 * i)] = static_cast<float>(s);
                b[static_cast<size_t>(2 * i + 1)] = static_cast<float>(s);
            }
            return b;
        };
        // Crest factor (peak / RMS): a proxy for "punchiness" — attack boost raises it.
        auto crest = [&](const std::vector<float>& b) {
            double pk = 0.0, sum = 0.0;
            for (float v : b) {
                const double a = std::fabs(v);
                if (a > pk) pk = a;
                sum += static_cast<double>(v) * v;
            }
            const double r = std::sqrt(sum / static_cast<double>(b.size()));
            return r > 0.0 ? pk / r : 0.0;
        };

        const std::vector<float> dry = burst();
        const double dryCrest = crest(dry);
        const double dryRms = rms(dry);

        // Attack boost: sharpen the onset → higher crest factor than the dry burst.
        audio::TransientShaper punch;
        punch.setEnabled(true);
        punch.setAttack(1.0f);
        std::vector<float> pb = burst();
        punch.process(pb.data(), sr, sr);
        check(crest(pb) > dryCrest * 1.05, "attack boost increases the crest factor (punch)");

        // Sustain cut: tighten the body/tail → less total energy than the dry burst.
        audio::TransientShaper tighten;
        tighten.setEnabled(true);
        tighten.setSustain(-1.0f);
        std::vector<float> tb = burst();
        tighten.process(tb.data(), sr, sr);
        check(rms(tb) < dryRms * 0.95, "sustain cut reduces the tail energy");

        // Neutral (0/0) and disabled → bit-transparent.
        audio::TransientShaper neutral;
        neutral.setEnabled(true); // attack=0, sustain=0
        std::vector<float> nb = burst();
        neutral.process(nb.data(), sr, sr);
        bool sameNeutral = true;
        for (size_t i = 0; i < nb.size(); ++i) {
            if (std::fabs(nb[i] - dry[i]) > 1e-6f) {
                sameNeutral = false;
                break;
            }
        }
        check(sameNeutral, "a neutral (0/0) transient shaper is transparent");

        audio::TransientShaper off;
        off.setAttack(1.0f); // would shape, but it is disabled
        std::vector<float> ob = burst();
        off.process(ob.data(), sr, sr);
        bool sameOff = true;
        for (size_t i = 0; i < ob.size(); ++i) {
            if (std::fabs(ob[i] - dry[i]) > 1e-6f) {
                sameOff = false;
                break;
            }
        }
        check(sameOff, "a disabled transient shaper is transparent");
    }

    // --- Auto-wah: louder input opens the filter (brighter output) ----------
    {
        // A bright saw at `freq`, amplitude `amp`, as interleaved stereo.
        auto sawStereo = [&](double freq, double amp, int frames) {
            std::vector<float> b(static_cast<size_t>(frames) * 2, 0.0f);
            double ph = 0.0;
            const double inc = freq / sr;
            for (int i = 0; i < frames; ++i) {
                const float s = static_cast<float>(amp * (2.0 * ph - 1.0));
                b[static_cast<size_t>(2 * i)] = s;
                b[static_cast<size_t>(2 * i + 1)] = s;
                ph += inc;
                if (ph >= 1.0) ph -= 1.0;
            }
            return b;
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
        auto wahOut = [&](double amp) {
            audio::AutoWah w;
            w.setEnabled(true);
            w.setBaseHz(300.0f);
            w.setRangeHz(3000.0f);
            w.setSensitivity(1.0f);
            w.setResonance(3.0f);
            std::vector<float> b = sawStereo(300.0, amp, sr / 2); // 0.5 s
            w.process(b.data(), sr / 2, sr);
            // Measure the settled second half (after the envelope follower has tracked).
            return std::vector<float>(b.begin() + static_cast<std::ptrdiff_t>(b.size() / 2), b.end());
        };
        // A loud note pushes the cutoff up → more harmonics survive → brighter than a quiet note.
        check(brightness(wahOut(0.9)) > brightness(wahOut(0.05)) * 1.3,
              "auto-wah opens the filter for louder input");

        // Downward mode reverses it: the same loud input closes the filter, so it is darker than the
        // upward sweep.
        auto wahDir = [&](bool down) {
            audio::AutoWah w;
            w.setEnabled(true);
            w.setBaseHz(300.0f);
            w.setRangeHz(3000.0f);
            w.setSensitivity(1.0f);
            w.setResonance(3.0f);
            w.setDownward(down);
            std::vector<float> b = sawStereo(300.0, 0.9, sr / 2);
            w.process(b.data(), sr / 2, sr);
            return std::vector<float>(b.begin() + static_cast<std::ptrdiff_t>(b.size() / 2), b.end());
        };
        check(brightness(wahDir(true)) < brightness(wahDir(false)) * 0.7,
              "downward auto-wah closes the filter for louder input (darker)");
        audio::AutoWah dw;
        check(!dw.downward(), "auto-wah direction defaults to upward");

        // Dry/wet mix: at mix 0 the wah is transparent (bright dry saw); at mix 1 a low fixed cutoff
        // darkens it. So the dry-blend output is clearly brighter than the fully-wet one.
        auto wahMixOut = [&](float mixv) {
            audio::AutoWah w;
            w.setEnabled(true);
            w.setBaseHz(200.0f);
            w.setRangeHz(0.0f);     // fixed low cutoff
            w.setSensitivity(0.0f); // envelope doesn't move it
            w.setResonance(1.0f);
            w.setMix(mixv);
            std::vector<float> b = sawStereo(300.0, 0.5, sr / 2);
            w.process(b.data(), sr / 2, sr);
            return std::vector<float>(b.begin() + static_cast<std::ptrdiff_t>(b.size() / 2), b.end());
        };
        check(brightness(wahMixOut(0.0f)) > brightness(wahMixOut(1.0f)) * 1.5,
              "auto-wah mix 0 stays bright (dry) while mix 1 darkens (wet filter)");
        audio::AutoWah dm;
        check(std::fabs(dm.mix() - 1.0f) < 1e-6f, "auto-wah mix defaults to 1 (fully wet)");

        // Disabled → transparent.
        audio::AutoWah off;
        std::vector<float> sig = sawStereo(300.0, 0.5, 1000);
        const std::vector<float> ref = sig;
        off.process(sig.data(), 1000, sr);
        bool same = true;
        for (size_t i = 0; i < sig.size(); ++i) {
            if (std::fabs(sig[i] - ref[i]) > 1e-6f) {
                same = false;
                break;
            }
        }
        check(same, "a disabled auto-wah is transparent");
    }

    // --- Comb resonator: feedback echoes at the tuned delay -----------------
    {
        // A 100 Hz tuning at 48 kHz is an exactly-480-sample delay, so the impulse response has clean
        // echoes at 480, 960, … scaled by feedback^n — deterministic to check.
        audio::CombResonator comb;
        comb.setEnabled(true);
        comb.setFrequency(100.0f);  // delay = 48000/100 = 480 samples
        comb.setFeedback(0.8f);
        comb.setMix(1.0f);          // fully wet, so the output is the raw comb response
        std::vector<float> imp(3000 * 2, 0.0f);
        imp[0] = 1.0f; // unit impulse in both channels
        imp[1] = 1.0f;
        comb.process(imp.data(), 3000, sr);
        check(std::fabs(imp[0] - 1.0f) < 1e-4f, "comb passes the initial impulse");
        check(std::fabs(imp[2 * 480] - 0.8f) < 1e-3f, "comb echoes at the tuned delay, scaled by feedback");
        check(std::fabs(imp[2 * 960] - 0.64f) < 1e-3f, "comb's second echo is feedback squared");

        // Damping: a high-cut on the feedback smears the sharp echoes, so the impulse response
        // carries far less high-frequency (first-difference) energy than the undamped comb.
        auto combHf = [&](float damp) {
            audio::CombResonator c;
            c.setEnabled(true);
            c.setFrequency(100.0f);
            c.setFeedback(0.85f);
            c.setMix(1.0f);
            c.setDamping(damp);
            std::vector<float> b(4000 * 2, 0.0f);
            b[0] = 1.0f;
            b[1] = 1.0f;
            c.process(b.data(), 4000, sr);
            double hf = 0.0;
            for (size_t i = 2; i < b.size(); i += 2) {
                const double dd = static_cast<double>(b[i] - b[i - 2]);
                hf += dd * dd;
            }
            return hf;
        };
        check(combHf(0.7f) < combHf(0.0f) * 0.7, "comb damping darkens (smears) the resonant tail");
        audio::CombResonator dcmp;
        check(dcmp.damping() == 0.0f, "comb damping defaults to off");

        // Disabled → transparent.
        audio::CombResonator off;
        std::vector<float> sig = sineStereo(1000, 300.0, 0.5, sr);
        const std::vector<float> ref = sig;
        off.process(sig.data(), 1000, sr);
        bool same = true;
        for (size_t i = 0; i < sig.size(); ++i) {
            if (std::fabs(sig[i] - ref[i]) > 1e-6f) {
                same = false;
                break;
            }
        }
        check(same, "a disabled comb resonator is transparent");
    }

    // --- Chord resonator: rings at the chord's frequencies from noise --------
    {
        auto power = [](const std::vector<float>& b, double f, int srate) {
            const double w = 2.0 * 3.14159265358979 * f / srate;
            const double c = 2.0 * std::cos(w);
            double s1 = 0.0, s2 = 0.0;
            for (size_t i = 0; i < b.size(); i += 2) {
                const double s0 = static_cast<double>(b[i]) + c * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            return s1 * s1 + s2 * s2 - c * s1 * s2;
        };
        // Excite with 1 s of white noise, measure the resonant tail's spectrum (last 0.5 s).
        auto ring = [&](int root, audio::ChordResonator::Chord chord) {
            audio::ChordResonator cr;
            cr.setEnabled(true);
            cr.setRootNote(root);
            cr.setChord(chord);
            cr.setFeedback(0.95f);
            cr.setDamping(0.2f);
            cr.setMix(1.0f);
            std::vector<float> b(static_cast<size_t>(sr) * 2, 0.0f);
            uint32_t rng = 12345u;
            for (int i = 0; i < sr; ++i) {
                rng ^= rng << 13;
                rng ^= rng >> 17;
                rng ^= rng << 5;
                const float n = 0.2f * (static_cast<float>(rng) / 2147483648.0f - 1.0f);
                b[static_cast<size_t>(i) * 2] = n;
                b[static_cast<size_t>(i) * 2 + 1] = n;
            }
            cr.process(b.data(), sr, sr);
            return std::vector<float>(b.end() - static_cast<long>(sr), b.end());
        };
        // A-minor rooted at A3 (57 → 220 Hz): resonates at 220 (root) and 330 (fifth); little at 290.
        const std::vector<float> aMinor = ring(57, audio::ChordResonator::Chord::Minor);
        const double root = power(aMinor, 220.0, sr);
        const double fifth = power(aMinor, 330.0, sr);
        const double off = power(aMinor, 290.0, sr);
        check(root > off * 5.0, "chord resonator rings at the root frequency");
        check(fifth > off * 3.0, "chord resonator rings at a chord tone (the fifth)");
        // Moving the root shifts the resonant peak: root at A4 (69 → 440 Hz) now peaks at 440.
        const std::vector<float> aUp = ring(69, audio::ChordResonator::Chord::Minor);
        check(power(aUp, 440.0, sr) > power(aUp, 220.0, sr) * 3.0,
              "chord resonator's root note shifts the resonant frequency");
        // Disabled → transparent.
        audio::ChordResonator offx;
        std::vector<float> sig = sineStereo(1000, 300.0, 0.5, sr);
        const std::vector<float> ref = sig;
        offx.process(sig.data(), 1000, sr);
        check(sig == ref, "a disabled chord resonator is transparent");
        check(!audio::ChordResonator().enabled(), "chord resonator defaults to off");
    }

    // --- Tremolo / trance-gate: rhythmic amplitude modulation ---------------
    {
        // A square-shaped tremolo at full depth gates a steady tone on and off, so windowed levels
        // swing between ~full and ~silent.
        audio::Tremolo trem;
        trem.setEnabled(true);
        trem.setRate(8.0f);
        trem.setDepth(1.0f);
        trem.setShape(audio::Tremolo::Shape::Square);
        std::vector<float> b = sineStereo(sr, 300.0, 0.5, sr); // 1 s
        trem.process(b.data(), sr, sr);
        // Windowed RMS over 512-frame blocks: find the loudest and quietest window.
        double loud = 0.0, quiet = 1e9;
        const int win = 512;
        for (int start = 0; start + win <= sr; start += win) {
            double e = 0.0;
            for (int i = 0; i < win; ++i) {
                const float l = b[static_cast<size_t>((start + i) * 2)];
                e += static_cast<double>(l) * l;
            }
            const double r = std::sqrt(e / win);
            if (r > loud) loud = r;
            if (r < quiet) quiet = r;
        }
        check(loud > 0.2, "trance-gate passes the signal at the LFO peak");
        check(quiet < 0.02, "trance-gate silences the signal at the LFO trough");

        // Saw shape: the gain ramps linearly up across each cycle, so windowed level rises
        // monotonically over one cycle — unlike the sine, which rises then falls (symmetric).
        auto quarterRms = [&](audio::Tremolo::Shape shape, int q) {
            audio::Tremolo t;
            t.setEnabled(true);
            t.setRate(1.0f); // one cycle over the 1 s render
            t.setDepth(1.0f);
            t.setShape(shape);
            std::vector<float> bb = sineStereo(sr, 300.0, 0.5, sr);
            t.process(bb.data(), sr, sr);
            const int q0 = q * (sr / 4);
            double e = 0.0;
            for (int i = q0; i < q0 + sr / 4; ++i) {
                e += static_cast<double>(bb[static_cast<size_t>(i) * 2]) * bb[static_cast<size_t>(i) * 2];
            }
            return std::sqrt(e / (sr / 4));
        };
        const double sawQ0 = quarterRms(audio::Tremolo::Shape::Saw, 0);
        const double sawQ1 = quarterRms(audio::Tremolo::Shape::Saw, 1);
        const double sawQ2 = quarterRms(audio::Tremolo::Shape::Saw, 2);
        const double sawQ3 = quarterRms(audio::Tremolo::Shape::Saw, 3);
        check(sawQ0 < sawQ1 && sawQ1 < sawQ2 && sawQ2 < sawQ3,
              "saw tremolo ramps the level up monotonically across each cycle");
        check(quarterRms(audio::Tremolo::Shape::Sine, 3) < quarterRms(audio::Tremolo::Shape::Sine, 1),
              "sine tremolo is symmetric (its last quarter is quieter than its second — not a ramp)");

        // Depth 0 is unity gain (transparent) even while enabled.
        audio::Tremolo flat;
        flat.setEnabled(true);
        flat.setDepth(0.0f);
        std::vector<float> f = sineStereo(1000, 300.0, 0.5, sr);
        const std::vector<float> fref = f;
        flat.process(f.data(), 1000, sr);
        bool flatSame = true;
        for (size_t i = 0; i < f.size(); ++i) {
            if (std::fabs(f[i] - fref[i]) > 1e-6f) {
                flatSame = false;
                break;
            }
        }
        check(flatSame, "a zero-depth tremolo is transparent");

        // Disabled → transparent.
        audio::Tremolo off;
        off.setDepth(1.0f);
        std::vector<float> sig = sineStereo(1000, 300.0, 0.5, sr);
        const std::vector<float> ref = sig;
        off.process(sig.data(), 1000, sr);
        bool same = true;
        for (size_t i = 0; i < sig.size(); ++i) {
            if (std::fabs(sig[i] - ref[i]) > 1e-6f) {
                same = false;
                break;
            }
        }
        check(same, "a disabled tremolo is transparent");
    }

    // --- Step gate: a 16-step rhythmic volume pattern -----------------------
    {
        // All steps open (the default) → transparent (gain 1 everywhere).
        audio::StepGate open;
        open.setEnabled(true);
        open.setRate(2.0f);
        std::vector<float> b = sineStereo(sr, 300.0, 0.5, sr);
        const double dryRms = rms(sineStereo(sr, 300.0, 0.5, sr));
        open.process(b.data(), sr, sr);
        check(std::fabs(rms(b) - dryRms) < 1e-4, "an all-open step gate is transparent");

        // All steps closed → silence.
        audio::StepGate shut;
        shut.setEnabled(true);
        shut.setRate(2.0f);
        for (int s = 0; s < audio::StepGate::kSteps; ++s) shut.setStep(s, 0.0f);
        std::vector<float> z = sineStereo(sr, 300.0, 0.5, sr);
        shut.process(z.data(), sr, sr);
        // (A brief declick ramp from the gate's initially-open state leaks a few ms at the very start.)
        check(rms(z) < 0.03, "an all-closed step gate silences the signal");

        // An alternating on/off pattern gates the signal rhythmically: windowed levels swing between
        // ~full and ~silent, and the overall level drops versus the dry (roughly half the steps open).
        audio::StepGate alt;
        alt.setEnabled(true);
        alt.setRate(1.0f); // one 16-step pass per second → 62.5 ms steps
        for (int s = 0; s < audio::StepGate::kSteps; ++s) alt.setStep(s, (s % 2 == 0) ? 1.0f : 0.0f);
        std::vector<float> g = sineStereo(sr, 300.0, 0.5, sr);
        alt.process(g.data(), sr, sr);
        check(rms(g) < dryRms * 0.85 && rms(g) > dryRms * 0.4,
              "an alternating step gate cuts the overall level (rhythmic gating)");
        double loud = 0.0, quiet = 1e9;
        const int win = 512;
        for (int start = 0; start + win <= sr; start += win) {
            double e = 0.0;
            for (int i = 0; i < win; ++i) {
                const float l = g[static_cast<size_t>((start + i) * 2)];
                e += static_cast<double>(l) * l;
            }
            const double r = std::sqrt(e / win);
            if (r > loud) loud = r;
            if (r < quiet) quiet = r;
        }
        check(loud > 0.2 && quiet < 0.02, "step gate windows swing between open and closed");

        // Tempo sync: with 1/16-note steps the 16-step pattern spans one bar (0.5 Hz @120 BPM).
        audio::StepGate sg;
        sg.setSync(true);
        sg.setSyncDivision(5); // "1/16" per step in the shared modulation division set
        sg.updateTempo(120.0);
        check(std::fabs(sg.rate() - 0.5f) < 0.01f,
              "synced 1/16-step gate spans one bar @120 BPM (0.5 Hz pattern)");

        // Disabled → bit-identical passthrough; defaults are transparent (all steps open).
        audio::StepGate dis;
        dis.setStep(0, 0.0f);
        std::vector<float> d = sineStereo(1000, 300.0, 0.5, sr);
        const std::vector<float> dref = d;
        dis.process(d.data(), 1000, sr);
        bool same2 = true;
        for (size_t i = 0; i < d.size(); ++i) {
            if (d[i] != dref[i]) same2 = false;
        }
        check(same2, "a disabled step gate is a bit-identical passthrough");
        check(audio::StepGate().step(0) == 1.0f && !audio::StepGate().enabled(),
              "step gate defaults to all-open and off");
    }

    // --- Stereo delay: independent left/right echo times --------------------
    {
        // 10 ms left = 480 samples, 20 ms right = 960 samples at 48 kHz — exact for a clean check.
        audio::StereoDelay sd;
        sd.setEnabled(true);
        sd.setLeftMs(10.0f);
        sd.setRightMs(20.0f);
        sd.setFeedback(0.5f);
        sd.setMix(1.0f); // fully wet → the output is the echo train
        std::vector<float> imp(3000 * 2, 0.0f);
        imp[0] = 1.0f; // impulse on the left only
        imp[1] = 1.0f; // impulse on the right only
        sd.process(imp.data(), 3000, sr);
        // Left echoes at 480, 960 (×feedback each step); right echoes at 960, 1920.
        check(std::fabs(imp[2 * 480] - 1.0f) < 1e-3f, "left channel echoes at its own delay time");
        check(std::fabs(imp[2 * 960] - 0.5f) < 1e-3f, "left echo repeats scaled by feedback");
        check(std::fabs(imp[2 * 960 + 1] - 1.0f) < 1e-3f, "right channel echoes at its own (longer) time");
        // The right channel has NOT echoed yet at the left's first tap (times are independent).
        check(std::fabs(imp[2 * 480 + 1]) < 1e-3f, "right channel is silent at the left echo time");

        // Disabled → transparent.
        audio::StereoDelay off;
        std::vector<float> sig = sineStereo(1000, 300.0, 0.5, sr);
        const std::vector<float> ref = sig;
        off.process(sig.data(), 1000, sr);
        bool same = true;
        for (size_t i = 0; i < sig.size(); ++i) {
            if (std::fabs(sig[i] - ref[i]) > 1e-6f) {
                same = false;
                break;
            }
        }
        check(same, "a disabled stereo delay is transparent");
    }

    // --- Ping-pong stereo delay: feedback cross-routes L↔R -------------------
    {
        // A LEFT-only impulse: without ping-pong the right line never gets input, so the right output
        // stays silent; with ping-pong the left echo feeds the right line and the right channel sings.
        auto rightEnergy = [&](bool ping) {
            audio::StereoDelay d;
            d.setEnabled(true);
            d.setLeftMs(10.0f);
            d.setRightMs(20.0f);
            d.setFeedback(0.6f);
            d.setMix(1.0f); // fully wet
            d.setPingPong(ping);
            std::vector<float> imp(4000 * 2, 0.0f);
            imp[0] = 1.0f; // impulse on the LEFT channel only
            d.process(imp.data(), 4000, sr);
            double e = 0.0;
            for (size_t i = 1; i < imp.size(); i += 2) { // right samples only
                e += static_cast<double>(imp[i]) * imp[i];
            }
            return e;
        };
        check(rightEnergy(false) < 1e-9,
              "without ping-pong a left-only signal stays out of the right channel");
        check(rightEnergy(true) > 1e-3, "ping-pong bounces the left echo into the right channel");
        check(!audio::StereoDelay().pingPong(), "stereo delay ping-pong defaults to off");
    }

    // --- Stereo delay feedback tone: damping + low-cut -----------------------
    {
        // Damping high-cuts the feedback: a bright tone's echoes lose their highs.
        auto wetHF = [&](float damp) {
            audio::StereoDelay d;
            d.setEnabled(true);
            d.setLeftMs(50.0f);
            d.setRightMs(50.0f);
            d.setFeedback(0.7f);
            d.setMix(1.0f);
            d.setDamping(damp);
            std::vector<float> b = sineStereo(sr / 2, 3000.0, 0.5, sr);
            d.process(b.data(), sr / 2, sr);
            double s = 0.0;
            for (size_t i = 2; i < b.size(); i += 2) {
                const double diff = static_cast<double>(b[i] - b[i - 2]);
                s += diff * diff;
            }
            return s;
        };
        check(wetHF(0.7f) < wetHF(0.0f) * 0.7, "stereo delay damping darkens the echoes");

        // Low-cut high-passes the feedback: a low burst's late echoes carry less energy.
        auto lateEnergy = [&](float lowCut) {
            audio::StereoDelay d;
            d.setEnabled(true);
            d.setLeftMs(50.0f);
            d.setRightMs(50.0f);
            d.setFeedback(0.85f);
            d.setMix(1.0f);
            d.setFeedbackLowCut(lowCut);
            std::vector<float> b(static_cast<size_t>(sr / 2) * 2, 0.0f);
            const int burst = sr / 20;
            for (int i = 0; i < burst; ++i) {
                const float s = static_cast<float>(0.5 * std::sin(kTwoPi * 100.0 * i / sr));
                b[static_cast<size_t>(i) * 2] = s;
                b[static_cast<size_t>(i) * 2 + 1] = s;
            }
            d.process(b.data(), sr / 2, sr);
            double e = 0.0;
            for (int i = sr / 2 - sr / 7; i < sr / 2; ++i) {
                e += static_cast<double>(b[static_cast<size_t>(i) * 2]) * b[static_cast<size_t>(i) * 2];
            }
            return e;
        };
        check(lateEnergy(400.0f) < lateEnergy(0.0f) * 0.7,
              "stereo delay feedback low-cut thins the echoes' lows");
        audio::StereoDelay dd2;
        check(dd2.damping() == 0.0f && dd2.feedbackLowCut() == 0.0f,
              "stereo delay feedback tone defaults to off");
    }

    // --- Formant filter: passes a vowel's formant, rejects far-off tones -----
    {
        // Vowel A's first formant is ~800 Hz. A tone there survives the filter; a tone far above any
        // formant (5 kHz) is strongly attenuated.
        auto vowelPass = [&](double toneHz) {
            audio::FormantFilter f;
            f.setEnabled(true);
            f.setVowel(audio::FormantFilter::Vowel::A);
            f.setMix(1.0f); // fully wet
            std::vector<float> b = sineStereo(sr, toneHz, 0.5, sr);
            f.process(b.data(), sr, sr);
            return rms(b);
        };
        check(vowelPass(800.0) > vowelPass(5000.0) * 3.0,
              "formant filter passes a tone at the vowel's formant and rejects a far-off tone");

        // Vowel morph: an integer morph position reproduces that discrete vowel exactly; a fractional
        // position lands between (a continuous talkbox sweep).
        auto fmtRender = [&](bool morphOn, float pos, audio::FormantFilter::Vowel v) {
            audio::FormantFilter f;
            f.setEnabled(true);
            f.setMix(1.0f);
            f.setVowel(v);
            f.setMorphEnabled(morphOn);
            f.setMorph(pos);
            // A harmonic-rich saw (built inline) so the formants have overtones to shape.
            std::vector<float> b(static_cast<size_t>(sr / 4) * 2, 0.0f);
            double ph = 0.0;
            const double inc = 200.0 / sr;
            for (int i = 0; i < sr / 4; ++i) {
                const float s = static_cast<float>(2.0 * ph - 1.0) * 0.5f;
                b[static_cast<size_t>(i) * 2] = s;
                b[static_cast<size_t>(i) * 2 + 1] = s;
                ph += inc;
                if (ph >= 1.0) {
                    ph -= 1.0;
                }
            }
            f.process(b.data(), sr / 4, sr);
            return b;
        };
        const std::vector<float> morphI = fmtRender(true, 2.0f, audio::FormantFilter::Vowel::A);
        const std::vector<float> discreteI = fmtRender(false, 0.0f, audio::FormantFilter::Vowel::I);
        double dEq = 0.0;
        for (size_t i = 0; i < morphI.size(); ++i) {
            dEq += std::fabs(static_cast<double>(morphI[i] - discreteI[i]));
        }
        check(dEq < 1e-3, "formant morph at an integer position matches the discrete vowel (I)");
        const std::vector<float> morphMid = fmtRender(true, 0.5f, audio::FormantFilter::Vowel::A);
        const std::vector<float> discreteA = fmtRender(false, 0.0f, audio::FormantFilter::Vowel::A);
        double dMid = 0.0;
        for (size_t i = 0; i < morphMid.size(); ++i) {
            dMid += std::fabs(static_cast<double>(morphMid[i] - discreteA[i]));
        }
        check(dMid > 1.0, "a fractional morph position differs from the nearest discrete vowel");
        audio::FormantFilter dfm;
        check(!dfm.morphEnabled(), "formant vowel morph defaults to off");

        // Formant/gender shift: shifting the formants up an octave moves vowel A's ~800 Hz F1 up to
        // ~1600 Hz, so a 1600 Hz tone (rejected at the natural shift) now passes through the formant.
        auto shiftedPass = [&](float semis, double toneHz) {
            audio::FormantFilter f;
            f.setEnabled(true);
            f.setVowel(audio::FormantFilter::Vowel::A);
            f.setMix(1.0f);
            f.setFormantShift(semis);
            std::vector<float> b = sineStereo(sr, toneHz, 0.5, sr);
            f.process(b.data(), sr, sr);
            return rms(b);
        };
        check(shiftedPass(12.0f, 1600.0) > shiftedPass(0.0f, 1600.0) * 2.0,
              "formant shift moves the formants up (a higher tone now passes)");
        check(audio::FormantFilter().formantShift() == 0.0f, "formant shift defaults to natural (0)");

        // Disabled → transparent.
        audio::FormantFilter off;
        std::vector<float> sig = sineStereo(1000, 800.0, 0.5, sr);
        const std::vector<float> ref = sig;
        off.process(sig.data(), 1000, sr);
        bool same = true;
        for (size_t i = 0; i < sig.size(); ++i) {
            if (std::fabs(sig[i] - ref[i]) > 1e-6f) {
                same = false;
                break;
            }
        }
        check(same, "a disabled formant filter is transparent");
    }

    // --- Vocoder: the modulator's spectral envelope shapes the carrier -------
    {
        auto power = [](const std::vector<float>& b, double f, int srate) {
            const double w = 2.0 * 3.14159265358979 * f / srate;
            const double c = 2.0 * std::cos(w);
            double s1 = 0.0, s2 = 0.0;
            for (size_t i = 0; i < b.size(); i += 2) {
                const double s0 = static_cast<double>(b[i]) + c * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            return s1 * s1 + s2 * s2 - c * s1 * s2;
        };
        // The carrier is a fixed low saw; the modulator (input) is a tone whose band the output should
        // follow. A low-band modulator → low-band output; a high-band modulator → high-band output.
        auto voc = [&](double modFreq) {
            audio::Vocoder v;
            v.setEnabled(true);
            v.setCarrier(audio::Vocoder::Carrier::Saw);
            v.setCarrierHz(110.0f);
            v.setMix(1.0f);
            std::vector<float> b = sineStereo(sr, modFreq, 0.5, sr);
            v.process(b.data(), sr, sr);
            return std::vector<float>(b.end() - static_cast<long>(sr), b.end()); // settled 0.5 s
        };
        const std::vector<float> lo = voc(300.0);
        const std::vector<float> hi = voc(4000.0);
        check(power(lo, 300.0, sr) > power(lo, 4000.0, sr) * 3.0,
              "vocoder: a low modulator puts the carrier energy in the low band");
        check(power(hi, 4000.0, sr) > power(hi, 300.0, sr) * 3.0,
              "vocoder: a high modulator puts the carrier energy in the high band");
        // Disabled → transparent; defaults off.
        audio::Vocoder off;
        std::vector<float> sig = sineStereo(1000, 500.0, 0.5, sr);
        const std::vector<float> ref = sig;
        off.process(sig.data(), 1000, sr);
        check(sig == ref, "a disabled vocoder is transparent");
        check(!audio::Vocoder().enabled(), "vocoder defaults to off");
    }

    // --- Utility: gain trim, phase invert, mono-sum --------------------------
    {
        // Phase invert on the left channel negates it (right untouched).
        audio::Utility inv;
        inv.setEnabled(true);
        inv.setInvertL(true);
        std::vector<float> b(8, 0.0f);
        for (int i = 0; i < 4; ++i) {
            b[static_cast<size_t>(2 * i)] = 0.5f;      // L
            b[static_cast<size_t>(2 * i + 1)] = 0.3f;  // R
        }
        inv.process(b.data(), 4, sr);
        check(std::fabs(b[0] + 0.5f) < 1e-5f && std::fabs(b[1] - 0.3f) < 1e-5f,
              "utility phase-inverts the left channel only");

        // Mono-sum collapses L and R to their average on both channels.
        audio::Utility mono;
        mono.setEnabled(true);
        mono.setMono(true);
        std::vector<float> m = {0.8f, 0.2f, 0.8f, 0.2f};
        mono.process(m.data(), 2, sr);
        check(std::fabs(m[0] - 0.5f) < 1e-5f && std::fabs(m[1] - 0.5f) < 1e-5f,
              "utility mono-sum averages the channels");

        // Gain trim of +6 dB roughly doubles the level.
        audio::Utility gain;
        gain.setEnabled(true);
        gain.setGainDb(6.0f);
        std::vector<float> g = {0.4f, 0.4f};
        gain.process(g.data(), 1, sr);
        check(g[0] > 0.78f && g[0] < 0.82f, "utility +6 dB gain roughly doubles the level");

        // Width (M/S): a stereo signal's side (L−R) scales with the width; 0 = mono, 2 = wider.
        auto sideAfter = [&](float width) {
            audio::Utility u;
            u.setEnabled(true);
            u.setWidth(width);
            std::vector<float> ub = {0.6f, 0.2f, 0.6f, 0.2f}; // side = (L−R)/2 = 0.2 per frame
            u.process(ub.data(), 2, sr);
            return std::fabs(ub[0] - ub[1]) * 0.5f; // resulting side magnitude
        };
        check(std::fabs(sideAfter(0.0f)) < 1e-5f, "utility width 0 collapses to mono (no side)");
        check(std::fabs(sideAfter(2.0f) - 0.4f) < 1e-5f, "utility width 2 doubles the side component");
        check(std::fabs(sideAfter(1.0f) - 0.2f) < 1e-5f, "utility width 1 leaves the side unchanged");
        check(audio::Utility().width() == 1.0f, "utility width defaults to 1 (unchanged)");

        // Disabled → transparent.
        audio::Utility off;
        off.setInvertL(true);
        std::vector<float> sig = sineStereo(1000, 300.0, 0.5, sr);
        const std::vector<float> ref = sig;
        off.process(sig.data(), 1000, sr);
        bool same = true;
        for (size_t i = 0; i < sig.size(); ++i) {
            if (std::fabs(sig[i] - ref[i]) > 1e-6f) {
                same = false;
                break;
            }
        }
        check(same, "a disabled utility is transparent");
    }

    // --- HighPass: low frequencies attenuated, highs pass -------------------
    {
        audio::HighPass hp;
        hp.setEnabled(true);
        hp.setCutoff(500.0f);
        // A 60 Hz tone (well below cutoff) is strongly attenuated.
        std::vector<float> low = sineStereo(sr, 60.0, 0.8, sr);
        const double lowIn = rms(low);
        hp.process(low.data(), sr, sr);
        check(rms(low) < lowIn * 0.3, "high-pass attenuates a 60 Hz tone below the cutoff");

        // A 5 kHz tone (well above cutoff) passes ~unchanged.
        audio::HighPass hp2;
        hp2.setEnabled(true);
        hp2.setCutoff(500.0f);
        std::vector<float> high = sineStereo(sr, 5000.0, 0.8, sr);
        const double highIn = rms(high);
        hp2.process(high.data(), sr, sr);
        check(rms(high) > highIn * 0.8, "high-pass passes a 5 kHz tone above the cutoff");

        // Disabled → transparent.
        audio::HighPass off;
        std::vector<float> sig = sineStereo(1000, 100.0, 0.5, sr);
        const std::vector<float> ref = sig;
        off.process(sig.data(), 1000, sr);
        bool same = true;
        for (size_t i = 0; i < sig.size(); ++i) {
            if (std::fabs(sig[i] - ref[i]) > 1e-6f) {
                same = false;
                break;
            }
        }
        check(same, "a disabled high-pass is transparent");
    }

    // --- Flanger: swept comb with feedback ----------------------------------
    {
        // A static flanger (rate 0) is a short delay with feedback: an impulse produces a delayed
        // copy at ~ (1ms floor + depth*mod) and decaying feedback repeats after it.
        audio::Flanger fl;
        fl.setEnabled(true);
        fl.setRate(0.0f);      // hold the sweep still
        fl.setDepth(4.0f);     // + the ~1 ms floor → tap near (1 + 4*0.5)=3 ms at phase 0
        fl.setFeedback(0.7f);
        fl.setMix(1.0f);       // fully wet
        std::vector<float> buf(static_cast<size_t>(sr) / 5 * 2, 0.0f); // 0.2 s
        buf[0] = 1.0f;
        buf[1] = 1.0f;
        fl.process(buf.data(), sr / 5, sr);
        // The dry impulse is gone (fully wet), and there is delayed energy downstream.
        check(std::fabs(buf[0]) < 0.01f, "fully-wet flanger removes the dry impulse");
        double tail = 0.0;
        for (int i = 20; i < sr / 5; ++i) {
            tail += std::fabs(static_cast<double>(buf[static_cast<size_t>(i) * 2]));
        }
        check(tail > 0.1, "flanger produces delayed/feedback energy");

        // More feedback → a longer-ringing comb (more total tail energy).
        auto tailEnergy = [&](float fb) {
            audio::Flanger f2;
            f2.setEnabled(true);
            f2.setRate(0.0f);
            f2.setDepth(4.0f);
            f2.setFeedback(fb);
            f2.setMix(1.0f);
            std::vector<float> b(static_cast<size_t>(sr) / 5 * 2, 0.0f);
            b[0] = 1.0f;
            b[1] = 1.0f;
            f2.process(b.data(), sr / 5, sr);
            double e = 0.0;
            for (size_t i = 40; i < b.size(); ++i) {
                e += static_cast<double>(b[i]) * static_cast<double>(b[i]);
            }
            return e;
        };
        check(tailEnergy(0.85f) > tailEnergy(0.2f) * 1.5, "more feedback rings longer");

        // Invert: with the shortest delay the wet is nearly in phase with the dry, so flipping its
        // polarity cancels toward silence (hollow through-zero flange), whereas normal reinforces.
        auto flangeRms = [&](bool invert) {
            audio::Flanger f;
            f.setEnabled(true);
            f.setRate(0.0f);  // static, shortest tap (~1 ms floor)
            f.setDepth(0.1f);
            f.setFeedback(0.0f);
            f.setMix(0.5f);
            f.setInvert(invert);
            std::vector<float> b = sineStereo(sr / 2, 60.0, 0.5, sr); // low tone → tap ~in phase
            f.process(b.data(), sr / 2, sr);
            double e = 0.0;
            int n = 0;
            for (int i = 400; i < sr / 2; ++i) { // skip the delay-line fill
                e += static_cast<double>(b[static_cast<size_t>(i) * 2]) * b[static_cast<size_t>(i) * 2];
                ++n;
            }
            return std::sqrt(e / n);
        };
        check(flangeRms(true) < flangeRms(false) * 0.6,
              "inverting the flanger cancels a short-delay signal (hollow through-zero flange)");
        check(!audio::Flanger().invert(), "flanger invert defaults to off");

        // Disabled → transparent.
        audio::Flanger off;
        std::vector<float> sig = sineStereo(1000, 300.0, 0.5, sr);
        const std::vector<float> ref = sig;
        off.process(sig.data(), 1000, sr);
        bool same = true;
        for (size_t i = 0; i < sig.size(); ++i) {
            if (std::fabs(sig[i] - ref[i]) > 1e-6f) {
                same = false;
                break;
            }
        }
        check(same, "a disabled flanger is transparent");
    }

    // --- Tape saturation: adds harmonics, transparent when bypassed ---------
    {
        auto hf = [](const std::vector<float>& b) {
            double s = 0.0;
            for (size_t i = 1; i < b.size(); ++i) {
                const double d = static_cast<double>(b[i] - b[i - 1]);
                s += d * d;
            }
            return s;
        };
        // A pure sine driven hard should gain harmonic (high-frequency) energy.
        std::vector<float> clean = sineStereo(sr, 300.0, 0.8, sr);
        const double cleanHf = hf(clean);
        audio::TapeSaturation tape;
        tape.setEnabled(true);
        tape.setDrive(8.0f);
        tape.setWarmth(0.0f); // no high-cut, so harmonics survive for the measurement
        tape.setMix(1.0f);
        std::vector<float> driven = clean;
        tape.process(driven.data(), sr, sr);
        check(hf(driven) > cleanHf * 1.2, "tape saturation adds harmonics to a driven sine");
        check(rms(driven) > 0.0, "tape saturation still passes signal");

        // Warmth (high-cut) should reduce high-frequency energy vs. no warmth.
        audio::TapeSaturation warm;
        warm.setEnabled(true);
        warm.setDrive(8.0f);
        warm.setWarmth(1.0f);
        std::vector<float> warmed = clean;
        warm.process(warmed.data(), sr, sr);
        check(hf(warmed) < hf(driven), "warmth rolls off the high end");

        // Disabled → transparent.
        audio::TapeSaturation off;
        std::vector<float> sig = sineStereo(1000, 200.0, 0.5, sr);
        const std::vector<float> ref = sig;
        off.process(sig.data(), 1000, sr);
        bool same = true;
        for (size_t i = 0; i < sig.size(); ++i) {
            if (std::fabs(sig[i] - ref[i]) > 1e-6f) {
                same = false;
                break;
            }
        }
        check(same, "a disabled tape saturation is transparent");

        // Wow & flutter: the modulated delay wobbles the pitch, so the per-window pitch varies over
        // time; with it off the pitch is rock-steady.
        auto pitchSpread = [&](float wf) {
            audio::TapeSaturation t;
            t.setEnabled(true);
            t.setDrive(1.0f);
            t.setWarmth(0.0f);
            t.setMix(1.0f);
            t.setWowFlutter(wf);
            std::vector<float> b = sineStereo(sr, 440.0, 0.5, sr); // 1 s
            t.process(b.data(), sr, sr);
            int minC = 1 << 30, maxC = 0;
            const int win = 3000;
            for (int w = win; w + win <= sr; w += win) { // skip the first window (delay fill-in)
                int cx = 0;
                float prev = b[static_cast<size_t>(w) * 2];
                for (int i = w + 1; i < w + win; ++i) {
                    const float v = b[static_cast<size_t>(i) * 2];
                    if (prev <= 0.0f && v > 0.0f) {
                        ++cx;
                    }
                    prev = v;
                }
                minC = std::min(minC, cx);
                maxC = std::max(maxC, cx);
            }
            return maxC - minC;
        };
        check(pitchSpread(0.0f) <= 1, "with wow/flutter off the tape pitch is steady");
        check(pitchSpread(1.0f) >= 2, "wow/flutter wobbles the pitch over time");
        audio::TapeSaturation dwf;
        check(dwf.wowFlutter() == 0.0f, "tape wow/flutter defaults to 0 (off)");
    }

    // --- Mono bass: low band collapses to mono, highs stay stereo -----------
    {
        auto sideEnergy = [](const std::vector<float>& b) {
            double e = 0.0;
            for (size_t i = 0; i + 1 < b.size(); i += 2) {
                const double s = 0.5 * (static_cast<double>(b[i]) - static_cast<double>(b[i + 1]));
                e += s * s;
            }
            return e;
        };
        auto panned = [&](double hz) {
            // A tone only in the left channel → strong side energy until mono-maker acts on it.
            std::vector<float> b(static_cast<size_t>(sr) * 2, 0.0f);
            for (int i = 0; i < sr; ++i) {
                const double t = static_cast<double>(i) / sr;
                b[static_cast<size_t>(i) * 2] = static_cast<float>(0.6 * std::sin(2.0 * 3.14159265 * hz * t));
            }
            return b;
        };

        // A 60 Hz left-only tone (below the 120 Hz crossover) is summed to mono → side collapses.
        audio::MonoBass mb;
        mb.setEnabled(true);
        mb.setCrossover(120.0f);
        std::vector<float> low = panned(60.0);
        const double lowBefore = sideEnergy(low);
        mb.process(low.data(), sr, sr);
        check(sideEnergy(low) < lowBefore * 0.2, "mono-bass collapses the low band's stereo width");

        // A 4 kHz left-only tone (well above the crossover) keeps its stereo side energy.
        audio::MonoBass mb2;
        mb2.setEnabled(true);
        mb2.setCrossover(120.0f);
        std::vector<float> high = panned(4000.0);
        const double highBefore = sideEnergy(high);
        mb2.process(high.data(), sr, sr);
        check(sideEnergy(high) > highBefore * 0.7, "mono-bass leaves the high band stereo");

        // Disabled → transparent.
        audio::MonoBass off;
        std::vector<float> sig = panned(200.0);
        const std::vector<float> ref = sig;
        off.process(sig.data(), sr, sr);
        bool same = true;
        for (size_t i = 0; i < sig.size(); ++i) {
            if (std::fabs(sig[i] - ref[i]) > 1e-6f) {
                same = false;
                break;
            }
        }
        check(same, "a disabled mono-bass is transparent");
    }

    // --- Auto-pan: the LFO sweeps energy between L and R --------------------
    {
        // A steady mono tone through a 1 Hz full-depth auto-pan: over one second, the first quarter
        // leans one way and the third quarter the other (the LFO crosses zero at the halves).
        audio::AutoPan ap;
        ap.setEnabled(true);
        ap.setRate(1.0f);
        ap.setDepth(1.0f);
        std::vector<float> buf = sineStereo(sr, 220.0, 0.5, sr); // 1 s mono-in (L==R)
        ap.process(buf.data(), sr, sr);
        auto chanEnergy = [&](int start, int len, int ch) {
            double e = 0.0;
            for (int i = start; i < start + len; ++i) {
                const float v = buf[static_cast<size_t>(i) * 2 + static_cast<size_t>(ch)];
                e += static_cast<double>(v) * static_cast<double>(v);
            }
            return e;
        };
        // Quarter-second windows centered on the LFO peaks (t≈0.25 s → +1, t≈0.75 s → -1).
        const double q1R = chanEnergy(9000, 6000, 1);  // ~t=0.19-0.31 s: panned right
        const double q1L = chanEnergy(9000, 6000, 0);
        const double q3L = chanEnergy(33000, 6000, 0); // ~t=0.69-0.81 s: panned left
        const double q3R = chanEnergy(33000, 6000, 1);
        check(q1R > q1L * 4.0, "auto-pan leans right at the LFO's positive peak");
        check(q3L > q3R * 4.0, "auto-pan leans left at the LFO's negative peak");

        // Depth 0 → no movement (equal L/R throughout).
        audio::AutoPan flat;
        flat.setEnabled(true);
        flat.setDepth(0.0f);
        std::vector<float> b2 = sineStereo(sr / 2, 220.0, 0.5, sr);
        flat.process(b2.data(), sr / 2, sr);
        double eL = 0.0, eR = 0.0;
        for (size_t i = 0; i + 1 < b2.size(); i += 2) {
            eL += static_cast<double>(b2[i]) * static_cast<double>(b2[i]);
            eR += static_cast<double>(b2[i + 1]) * static_cast<double>(b2[i + 1]);
        }
        check(std::fabs(eL - eR) < eL * 1e-3 + 1e-9,
              "auto-pan at depth 0 leaves the balance centered");

        // Shape: a full-depth square pan hard-alternates L/R (one channel is silent at any instant),
        // whereas a sine pan sweeps through the centre where both channels carry signal. So the
        // "quieter channel" energy is near-zero for square but substantial for sine.
        auto minChanEnergy = [&](audio::AutoPan::Shape shape) {
            audio::AutoPan p;
            p.setEnabled(true);
            p.setRate(1.0f);
            p.setDepth(1.0f);
            p.setShape(shape);
            std::vector<float> b = sineStereo(sr, 220.0, 0.5, sr);
            p.process(b.data(), sr, sr);
            double e = 0.0;
            for (size_t i = 0; i + 1 < b.size(); i += 2) {
                const float mn = std::min(std::fabs(b[i]), std::fabs(b[i + 1]));
                e += static_cast<double>(mn) * static_cast<double>(mn);
            }
            return e;
        };
        const double sineMin = minChanEnergy(audio::AutoPan::Shape::Sine);
        const double sqMin = minChanEnergy(audio::AutoPan::Shape::Square);
        check(sqMin < sineMin * 0.1,
              "square auto-pan hard-alternates L/R (the quieter channel stays near silent)");
        check(audio::AutoPan().shape() == audio::AutoPan::Shape::Sine,
              "auto-pan shape defaults to sine");

        // Saw shape: the pan ramps one way across the cycle (−1→+1), so the left channel fades
        // monotonically from loud (start, hard-left) to quiet (end, hard-right).
        auto leftQuarter = [&](audio::AutoPan::Shape shape, int q) {
            audio::AutoPan p;
            p.setEnabled(true);
            p.setRate(1.0f);
            p.setDepth(1.0f);
            p.setShape(shape);
            std::vector<float> b = sineStereo(sr, 220.0, 0.5, sr);
            p.process(b.data(), sr, sr);
            const int q0 = q * (sr / 4);
            double e = 0.0;
            for (int i = q0; i < q0 + sr / 4; ++i) {
                e += static_cast<double>(b[static_cast<size_t>(i) * 2]) * b[static_cast<size_t>(i) * 2];
            }
            return e;
        };
        const double sawL0 = leftQuarter(audio::AutoPan::Shape::Saw, 0);
        const double sawL1 = leftQuarter(audio::AutoPan::Shape::Saw, 1);
        const double sawL2 = leftQuarter(audio::AutoPan::Shape::Saw, 2);
        const double sawL3 = leftQuarter(audio::AutoPan::Shape::Saw, 3);
        check(sawL0 > sawL1 && sawL1 > sawL2 && sawL2 > sawL3,
              "saw auto-pan ramps one way (left channel fades monotonically across the cycle)");
    }

    // --- Stereo widener: width controls L/R decorrelation -------------------
    {
        // Build a stereo signal with distinct L and R content (some side energy).
        auto makeStereo = [&](int frames) {
            std::vector<float> b(static_cast<size_t>(frames) * 2, 0.0f);
            for (int i = 0; i < frames; ++i) {
                const double t = static_cast<double>(i) / sr;
                b[static_cast<size_t>(i) * 2] = static_cast<float>(0.5 * std::sin(2.0 * 3.14159265 * 200.0 * t));
                b[static_cast<size_t>(i) * 2 + 1] = static_cast<float>(0.5 * std::sin(2.0 * 3.14159265 * 205.0 * t));
            }
            return b;
        };
        auto sideEnergy = [](const std::vector<float>& b) {
            double e = 0.0;
            for (size_t i = 0; i + 1 < b.size(); i += 2) {
                const double s = 0.5 * (static_cast<double>(b[i]) - static_cast<double>(b[i + 1]));
                e += s * s;
            }
            return e;
        };
        const std::vector<float> base = makeStereo(sr / 4);
        const double baseSide = sideEnergy(base);

        audio::StereoWidener mono;
        mono.setEnabled(true);
        mono.setWidth(0.0f);
        std::vector<float> m = base;
        mono.process(m.data(), sr / 4, sr);
        check(sideEnergy(m) < baseSide * 0.01, "width 0 collapses the image to mono (no side)");

        audio::StereoWidener wide;
        wide.setEnabled(true);
        wide.setWidth(2.0f);
        std::vector<float> w = base;
        wide.process(w.data(), sr / 4, sr);
        check(sideEnergy(w) > baseSide * 3.0, "width 2 doubles the side energy (wider image)");

        audio::StereoWidener unity;
        unity.setEnabled(true);
        unity.setWidth(1.0f);
        std::vector<float> u = base;
        unity.process(u.data(), sr / 4, sr);
        bool same = true;
        for (size_t i = 0; i < u.size(); ++i) {
            if (std::fabs(u[i] - base[i]) > 1e-5f) {
                same = false;
                break;
            }
        }
        check(same, "width 1 leaves the signal unchanged");
    }

    // --- Stereo widener bass mono: lows collapse to center, highs stay wide --
    {
        auto sideEnergy = [](const std::vector<float>& b) {
            double e = 0.0;
            for (size_t i = 0; i + 1 < b.size(); i += 2) {
                const double s = 0.5 * (static_cast<double>(b[i]) - static_cast<double>(b[i + 1]));
                e += s * s;
            }
            return e;
        };
        // A hard-left tone carries pure side energy (L != 0, R == 0).
        auto makeLeftTone = [&](double hz, int frames) {
            std::vector<float> b(static_cast<size_t>(frames) * 2, 0.0f);
            for (int i = 0; i < frames; ++i) {
                const double t = static_cast<double>(i) / sr;
                b[static_cast<size_t>(i) * 2] = static_cast<float>(0.5 * std::sin(2.0 * 3.14159265 * hz * t));
            }
            return b;
        };

        // A low, hard-left tone: bass mono should pull it toward the center (side drops).
        const std::vector<float> lowBase = makeLeftTone(60.0, sr / 2);
        const double lowSideBase = sideEnergy(lowBase);
        audio::StereoWidener bmLow;
        bmLow.setEnabled(true);
        bmLow.setWidth(1.0f);
        bmLow.setBassMonoHz(200.0f);
        std::vector<float> lo = lowBase;
        bmLow.process(lo.data(), sr / 2, sr);
        check(sideEnergy(lo) < lowSideBase * 0.25,
              "bass mono collapses the low-frequency side toward center");

        // A high, hard-left tone: bass mono leaves it wide (side largely preserved).
        const std::vector<float> hiBase = makeLeftTone(3000.0, sr / 2);
        const double hiSideBase = sideEnergy(hiBase);
        audio::StereoWidener bmHi;
        bmHi.setEnabled(true);
        bmHi.setWidth(1.0f);
        bmHi.setBassMonoHz(200.0f);
        std::vector<float> hi = hiBase;
        bmHi.process(hi.data(), sr / 2, sr);
        check(sideEnergy(hi) > hiSideBase * 0.8,
              "bass mono leaves the high-frequency side wide");

        // Default (0 Hz) is off: width-1 output is unchanged even on a low tone.
        audio::StereoWidener bmOff;
        bmOff.setEnabled(true);
        bmOff.setWidth(1.0f);
        check(bmOff.bassMonoHz() == 0.0f, "bass mono defaults to off");
        std::vector<float> off = lowBase;
        bmOff.process(off.data(), sr / 2, sr);
        check(sideEnergy(off) > lowSideBase * 0.99,
              "bass mono off leaves the low side untouched at width 1");
    }

    // --- Sub bass: generates a tone one octave below the input --------------
    {
        // Frequency of the left channel over [startFrame, endFrame), via zero crossings.
        auto freqOf = [](const std::vector<float>& b, int startFrame, int endFrame, int sampleRate) {
            int cross = 0;
            for (int i = startFrame + 1; i < endFrame; ++i) {
                const float a = b[static_cast<size_t>(2 * (i - 1))];
                const float c = b[static_cast<size_t>(2 * i)];
                if ((a <= 0.0f && c > 0.0f) || (a >= 0.0f && c < 0.0f)) {
                    ++cross;
                }
            }
            const double dur = static_cast<double>(endFrame - startFrame) / sampleRate;
            return static_cast<double>(cross) / (2.0 * dur);
        };

        // A 100 Hz input; the generated sub should land near 50 Hz (one octave down).
        const std::vector<float> in = sineStereo(sr, 100.0, 0.8, sr); // 1 s
        audio::SubBass sub;
        sub.setEnabled(true);
        sub.setAmount(1.0f);
        sub.setCutoff(150.0f);
        sub.setTone(220.0f);
        std::vector<float> wet = in;
        sub.process(wet.data(), sr, sr);

        // Isolate the added signal (wet − dry) and measure its frequency in the settled second half.
        std::vector<float> diff(wet.size(), 0.0f);
        for (size_t i = 0; i < wet.size(); ++i) {
            diff[i] = wet[i] - in[i];
        }
        const double subHz = freqOf(diff, sr / 2, sr, sr);
        check(subHz > 35.0 && subHz < 65.0, "sub bass generates a tone ~one octave below the input");
        check(rms(diff) > 0.02, "sub bass actually adds low-end energy");

        // Amount 0 (default) is transparent.
        audio::SubBass off;
        check(off.amount() == 0.0f, "sub bass defaults to off");
        off.setEnabled(true);
        std::vector<float> flat = in;
        off.process(flat.data(), sr, sr);
        bool same = true;
        for (size_t i = 0; i < flat.size(); ++i) {
            if (std::fabs(flat[i] - in[i]) > 1e-6f) {
                same = false;
                break;
            }
        }
        check(same, "sub bass at amount 0 leaves the signal unchanged");
    }

    // --- Gate: passes loud signal, attenuates quiet signal ------------------
    {
        // A loud tone (above threshold) passes ~unchanged.
        audio::Gate gate;
        gate.setEnabled(true);
        gate.setThresholdDb(-24.0f);
        gate.setRatio(6.0f);
        gate.setAttackMs(1.0f);
        gate.setReleaseMs(20.0f);
        std::vector<float> loud = sineStereo(sr, 220.0, 0.6, sr); // ~-4 dB, above -24 dB
        const double loudIn = rms(loud);
        gate.process(loud.data(), sr, sr);
        // Skip the first 50 ms (gate opening ramp) when measuring the passed level.
        std::vector<float> loudTail(loud.begin() + 2400 * 2, loud.end());
        check(rms(loudTail) > loudIn * 0.7, "gate passes signal above the threshold");

        // A quiet tone (below threshold) is pushed down toward the floor.
        audio::Gate gate2;
        gate2.setEnabled(true);
        gate2.setThresholdDb(-24.0f);
        gate2.setRatio(6.0f);
        gate2.setReleaseMs(20.0f);
        std::vector<float> quiet = sineStereo(sr, 220.0, 0.02, sr); // ~-34 dB, below -24 dB
        const double quietIn = rms(quiet);
        gate2.process(quiet.data(), sr, sr);
        std::vector<float> quietTail(quiet.begin() + 2400 * 2, quiet.end());
        check(rms(quietTail) < quietIn * 0.5, "gate attenuates signal below the threshold");

        // Disabled → transparent.
        audio::Gate off;
        std::vector<float> sig = sineStereo(1000, 100.0, 0.01, sr);
        const std::vector<float> ref = sig;
        off.process(sig.data(), 1000, sr);
        bool same = true;
        for (size_t i = 0; i < sig.size(); ++i) {
            if (std::fabs(sig[i] - ref[i]) > 1e-6f) {
                same = false;
                break;
            }
        }
        check(same, "a disabled gate is transparent");

        // Sidechain (key) high-pass: a loud LOW tone opens the gate normally, but with the key filter
        // set well above it the detector sees almost nothing, so the gate stays shut and the low tone
        // is attenuated toward the floor.
        auto lowTail = [&](float scHpf) {
            audio::Gate g;
            g.setEnabled(true);
            g.setThresholdDb(-24.0f);
            g.setRatio(6.0f);
            g.setReleaseMs(20.0f);
            g.setSidechainHpf(scHpf);
            std::vector<float> b = sineStereo(sr, 40.0, 0.6, sr); // loud low tone (~-4 dB)
            g.process(b.data(), sr, sr);
            std::vector<float> tail(b.begin() + 2400 * 2, b.end());
            return rms(tail);
        };
        const double keyOff = lowTail(0.0f);
        check(keyOff > 0.3, "without the key filter a loud low tone opens the gate");
        check(lowTail(2000.0f) < keyOff * 0.3,
              "the sidechain high-pass keeps the gate shut on a low tone the key can't see");
        audio::Gate dscg;
        check(dscg.sidechainHpf() == 0.0f, "gate sidechain HPF defaults to off");

        // Hold: after a loud burst drops to a quiet tail, hold keeps the gate open longer.
        auto tailLevel = [&](float holdMs) {
            audio::Gate g;
            g.setEnabled(true);
            g.setThresholdDb(-24.0f);
            g.setRatio(6.0f);
            g.setAttackMs(1.0f);
            g.setReleaseMs(10.0f);
            g.setHoldMs(holdMs);
            // 0.1 s loud (0.6, above threshold) then a quiet tail (0.03, below threshold).
            std::vector<float> b(static_cast<size_t>(sr) * 2, 0.0f);
            for (int i = 0; i < sr; ++i) {
                const double t = static_cast<double>(i) / sr;
                const float amp = i < sr / 10 ? 0.6f : 0.03f;
                const float v = amp * static_cast<float>(std::sin(2.0 * 3.14159265 * 220.0 * t));
                b[static_cast<size_t>(i) * 2] = v;
                b[static_cast<size_t>(i) * 2 + 1] = v;
            }
            g.process(b.data(), sr, sr);
            // Level in the tail window ~50–120 ms after the drop (frames 7000–10000).
            double e = 0.0;
            for (int i = 7000; i < 10000; ++i) {
                e += static_cast<double>(b[static_cast<size_t>(i) * 2]) *
                     static_cast<double>(b[static_cast<size_t>(i) * 2]);
            }
            return e;
        };
        check(tailLevel(150.0f) > tailLevel(0.0f) * 3.0,
              "gate hold keeps a quiet tail open after a loud burst");
    }

    // --- MixerTrack: per-bus insert strip -----------------------------------
    {
        audio::MixerTrack track; // all inserts off, unity gain
        check(!track.active(), "a fresh mixer track is transparent (inactive)");
        std::vector<float> sig = sineStereo(1000, 440.0, 0.5, sr);
        const std::vector<float> ref = sig;
        track.process(sig.data(), 1000, sr);
        bool unchanged = true;
        for (size_t i = 0; i < sig.size(); ++i) {
            if (std::fabs(sig[i] - ref[i]) > 1e-6f) {
                unchanged = false;
                break;
            }
        }
        check(unchanged, "an inactive track passes the signal through untouched");

        // Track gain scales the bus.
        audio::MixerTrack gained;
        gained.setGain(0.5f);
        check(gained.active(), "a non-unity gain makes the track active");
        std::vector<float> g = ref;
        gained.process(g.data(), 1000, sr);
        check(std::fabs(g[10] - ref[10] * 0.5f) < 1e-5f, "track gain scales the bus");

        // Mute silences the bus.
        audio::MixerTrack muted;
        muted.setMuted(true);
        std::vector<float> m = ref;
        muted.process(m.data(), 1000, sr);
        check(rms(m) == 0.0, "a muted track outputs silence");

        // An insert on the track alters the signal and reports active.
        audio::MixerTrack driven;
        driven.distortion().setEnabled(true);
        driven.distortion().setDrive(10.0f);
        check(driven.active(), "an enabled insert makes the track active");
        std::vector<float> d = ref;
        driven.process(d.data(), 1000, sr);
        double delta = 0.0;
        for (size_t i = 0; i < d.size(); ++i) {
            delta += std::fabs(static_cast<double>(d[i] - ref[i]));
        }
        check(delta > 0.0, "a track insert (distortion) alters the bus");

        // Per-bus high-pass: attenuates a low tone, leaves the track active.
        audio::MixerTrack hp;
        hp.highpass().setEnabled(true);
        hp.highpass().setCutoff(500.0f);
        check(hp.active(), "an enabled per-bus high-pass makes the track active");
        std::vector<float> low = sineStereo(sr, 60.0, 0.8, sr); // 60 Hz, well below cutoff
        const double lowIn = rms(low);
        hp.process(low.data(), sr, sr);
        check(rms(low) < lowIn * 0.3, "per-bus high-pass attenuates a low tone below its cutoff");

        // Per-bus pan/balance: hard left silences the right channel, leaves the left, stays active.
        audio::MixerTrack panned;
        panned.setPan(-1.0f);
        check(panned.active(), "a non-centre pan makes the track active");
        std::vector<float> pl = sineStereo(1000, 440.0, 0.5, sr);
        panned.process(pl.data(), 1000, sr);
        double lE = 0.0, rE = 0.0;
        for (int i = 0; i < 1000; ++i) {
            lE += static_cast<double>(pl[static_cast<size_t>(2 * i)]) * pl[static_cast<size_t>(2 * i)];
            rE += static_cast<double>(pl[static_cast<size_t>(2 * i + 1)]) *
                  pl[static_cast<size_t>(2 * i + 1)];
        }
        check(lE > 0.0 && rE < 1e-9, "hard-left bus pan keeps the left channel and silences the right");

        // Per-bus transient shaper: attack boost emphasizes a percussive onset.
        audio::MixerTrack trans;
        trans.transientShaper().setEnabled(true);
        trans.transientShaper().setAttack(1.0f);
        check(trans.active(), "an enabled per-bus transient shaper makes the track active");
        // A percussive burst: an exponentially-decaying 200 Hz tone (sharp onset at t=0).
        const int bn = 3000;
        std::vector<float> burst(static_cast<size_t>(bn) * 2, 0.0f);
        for (int i = 0; i < bn; ++i) {
            const float e = std::exp(-static_cast<float>(i) / 500.0f);
            const float s =
                e * static_cast<float>(std::sin(2.0 * 3.14159265 * 200.0 * i / sr));
            burst[static_cast<size_t>(2 * i)] = s;
            burst[static_cast<size_t>(2 * i) + 1] = s;
        }
        auto peakWindow = [](const std::vector<float>& b, int from, int to) {
            float p = 0.0f;
            for (int i = from; i < to; ++i) {
                p = std::max(p, std::fabs(b[static_cast<size_t>(2 * i)]));
            }
            return p;
        };
        std::vector<float> proc = burst;
        trans.process(proc.data(), bn, sr);
        check(peakWindow(proc, 0, 400) > peakWindow(burst, 0, 400) * 1.03f,
              "per-bus transient attack boost raises the onset peak");

        // Per-bus gate: a signal below the threshold is attenuated.
        audio::MixerTrack gated;
        gated.gate().setEnabled(true);
        gated.gate().setThresholdDb(-12.0f);
        gated.gate().setRatio(8.0f);
        gated.gate().setAttackMs(1.0f);
        gated.gate().setReleaseMs(20.0f);
        check(gated.active(), "an enabled per-bus gate makes the track active");
        std::vector<float> quiet = sineStereo(sr, 220.0, 0.05, sr); // ~-26 dB, below the threshold
        const double quietIn = rms(quiet);
        gated.process(quiet.data(), sr, sr);
        std::vector<float> qtail(quiet.begin() + static_cast<long>(sr), quiet.end()); // 2nd half
        check(rms(qtail) < quietIn * 0.5, "per-bus gate attenuates a signal below its threshold");
    }

    // --- Octaver: full-wave rectification injects a strong octave-up harmonic ------
    {
        // Goertzel power at a frequency on the left channel (stereo-interleaved buffer).
        auto power = [](const std::vector<float>& b, double f, int srate) {
            const double w = 2.0 * 3.14159265358979 * f / srate;
            const double c = 2.0 * std::cos(w);
            double s1 = 0.0, s2 = 0.0;
            for (size_t i = 0; i < b.size(); i += 2) {
                const double s0 = static_cast<double>(b[i]) + c * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            return s1 * s1 + s2 * s2 - c * s1 * s2;
        };
        // A 220 Hz tone has (essentially) no 440 Hz content; the octaver should create a strong one.
        std::vector<float> dry = sineStereo(sr, 220.0, 0.6, sr);
        std::vector<float> wet = dry; // copy, then process in place
        audio::Octaver oct;
        oct.setEnabled(true);
        oct.setAmount(1.0f);
        oct.setTone(6000.0f);
        oct.process(wet.data(), sr, sr);
        // Skip the DC-blocker/low-pass settling transient by measuring the second half.
        std::vector<float> wTail(wet.begin() + static_cast<long>(sr), wet.end());
        std::vector<float> dTail(dry.begin() + static_cast<long>(sr), dry.end());
        const double dry440 = power(dTail, 440.0, sr);
        const double wet440 = power(wTail, 440.0, sr);
        check(wet440 > dry440 * 50.0 + 1.0, "octaver injects a strong octave-up (440 Hz) component");
        check(power(wTail, 220.0, sr) > 0.0, "octaver keeps the dry fundamental");

        // amount 0 leaves the signal untouched (bit-for-bit), and it's off by default.
        std::vector<float> pass = sineStereo(sr / 4, 300.0, 0.5, sr);
        std::vector<float> ref = pass;
        audio::Octaver zero;
        zero.setEnabled(true);
        zero.setAmount(0.0f);
        zero.process(pass.data(), sr / 4, sr);
        bool identical = true;
        for (size_t i = 0; i < pass.size(); ++i) {
            if (pass[i] != ref[i]) {
                identical = false;
                break;
            }
        }
        check(identical, "octaver at amount 0 is bit-for-bit transparent");
        audio::Octaver def;
        check(!def.enabled() && def.amount() == 0.0f, "octaver is off by default");
    }

    // --- Convolver: an impulse produces an IR-length decaying tail; decay sets its length ----------
    {
        // Feed a unit impulse through a fully-wet convolver; the response is the IR (plus the dry
        // impulse at sample 0). Its energy should sit within the IR window and be ~silent afterwards.
        auto impulseResponse = [&](float decay, float* tailAfterIr, int* irLen) {
            audio::Convolver cv;
            cv.setEnabled(true);
            cv.setDecay(decay);
            cv.setTone(6000.0f);
            cv.setMix(1.0f); // fully wet → output is the IR
            std::vector<float> b(static_cast<size_t>(sr), 0.0f); // 0.5 s stereo
            b[0] = 1.0f;
            b[1] = 1.0f;
            cv.process(b.data(), sr / 2, sr);
            const int L = cv.impulseLength();
            *irLen = L;
            // Energy strictly after the IR window should be essentially zero (FIR has finite support).
            double e = 0.0;
            for (int i = L + 8; i < sr / 2; ++i) {
                e += static_cast<double>(b[static_cast<size_t>(i) * 2]) * b[static_cast<size_t>(i) * 2];
            }
            *tailAfterIr = static_cast<float>(e);
            return rms(b); // overall wet RMS
        };
        float afterShort = 0.0f, afterLong = 0.0f;
        int lenShort = 0, lenLong = 0;
        const double rmsShort = impulseResponse(0.1f, &afterShort, &lenShort);
        const double rmsLong = impulseResponse(0.4f, &afterLong, &lenLong);
        check(lenLong > lenShort * 2, "a longer decay builds a proportionally longer IR");
        check(lenShort > 0 && rmsShort > 0.0 && rmsLong > 0.0,
              "the convolver produces a non-trivial impulse response");
        check(afterShort < 1e-6f && afterLong < 1e-6f, "the IR is finite (silent past its length)");

        // A sustained tone through a wet convolver reverb-tails: it keeps ringing after the input stops.
        auto tailEnergy = [&]() {
            audio::Convolver cv;
            cv.setEnabled(true);
            cv.setDecay(0.3f);
            cv.setMix(1.0f);
            std::vector<float> b(static_cast<size_t>(sr), 0.0f); // 0.5 s stereo
            // 0.1 s of tone, then silence — the tail should fill part of the silence.
            for (int i = 0; i < sr / 10; ++i) {
                const float s = 0.5f * static_cast<float>(std::sin(2.0 * 3.14159265358979 * 300.0 * i / sr));
                b[static_cast<size_t>(i) * 2] = s;
                b[static_cast<size_t>(i) * 2 + 1] = s;
            }
            cv.process(b.data(), sr / 2, sr);
            double e = 0.0;
            for (int i = sr / 8; i < sr / 4; ++i) { // a window well after the input ended
                e += static_cast<double>(b[static_cast<size_t>(i) * 2]) * b[static_cast<size_t>(i) * 2];
            }
            return e;
        };
        check(tailEnergy() > 0.0, "the convolver rings out after the input stops (a reverb tail)");

        // mix 0 leaves the signal untouched (bit-for-bit), and it's off by default.
        std::vector<float> pass = sineStereo(sr / 4, 250.0, 0.5, sr);
        std::vector<float> ref = pass;
        audio::Convolver zero;
        zero.setEnabled(true);
        zero.setMix(0.0f);
        zero.process(pass.data(), sr / 4, sr);
        bool identical = true;
        for (size_t i = 0; i < pass.size(); ++i) {
            if (pass[i] != ref[i]) {
                identical = false;
                break;
            }
        }
        check(identical, "convolver at mix 0 is bit-for-bit transparent");
        audio::Convolver def;
        check(!def.enabled(), "convolver is off by default");
    }

    // --- BeatRepeat: each cell's captured sub-slice repeats to fill the cell ---
    {
        const int brSr = 48000;
        // A per-sample-distinct signal so a repeated sub-slice is unambiguous (a slow ramp per frame).
        auto ramp = [](int frames) {
            std::vector<float> b(static_cast<size_t>(frames) * 2, 0.0f);
            for (int i = 0; i < frames; ++i) {
                const float v = static_cast<float>(i) * 1e-4f;
                b[static_cast<size_t>(i) * 2] = v;
                b[static_cast<size_t>(i) * 2 + 1] = -v;
            }
            return b;
        };
        const int nframes = 480; // 10 ms cell @48k

        // Disabled and repeats==1 are both bit-for-bit passthroughs.
        {
            const std::vector<float> ref = ramp(nframes);
            std::vector<float> off = ref;
            audio::BeatRepeat br;
            br.setRepeats(4);
            br.setMix(1.0f); // still disabled → must be untouched
            br.process(off.data(), nframes, brSr);
            check(off == ref, "beat-repeat disabled is bit-for-bit passthrough");

            std::vector<float> one = ref;
            audio::BeatRepeat br1;
            br1.setEnabled(true);
            br1.setRepeats(1);
            br1.setMix(1.0f);
            br1.process(one.data(), nframes, brSr);
            check(one == ref, "beat-repeat at repeats=1 is bit-for-bit passthrough");
        }

        // repeats=2, mix=1, slice=10 ms → cell 480, sub 240: the 2nd sub-slice replays the 1st, which
        // (played live while capturing) equals the input. So out[240+k] == in[k] for k in [0,240).
        {
            const std::vector<float> in = ramp(nframes);
            std::vector<float> out = in;
            audio::BeatRepeat br;
            br.setEnabled(true);
            br.setSliceMs(10.0f);
            br.setRepeats(2);
            br.setMix(1.0f);
            br.process(out.data(), nframes, brSr);
            const int sub = 240;
            // First sub-slice is untouched (live capture).
            bool liveOk = true;
            for (int k = 0; k < sub && liveOk; ++k) {
                liveOk = out[static_cast<size_t>(k) * 2] == in[static_cast<size_t>(k) * 2] &&
                         out[static_cast<size_t>(k) * 2 + 1] == in[static_cast<size_t>(k) * 2 + 1];
            }
            check(liveOk, "beat-repeat plays the first sub-slice live");
            // Second sub-slice is a copy of the first (the stutter).
            bool repOk = true;
            for (int k = 0; k < sub && repOk; ++k) {
                repOk = out[static_cast<size_t>(sub + k) * 2] == in[static_cast<size_t>(k) * 2] &&
                        out[static_cast<size_t>(sub + k) * 2 + 1] == in[static_cast<size_t>(k) * 2 + 1];
            }
            check(repOk, "beat-repeat's later sub-slices replay the captured slice (the stutter)");
            // And it is not just the passthrough: the 2nd half now differs from the original input.
            check(out[static_cast<size_t>(sub) * 2] != in[static_cast<size_t>(sub) * 2],
                  "beat-repeat actually alters the repeated region");
        }

        audio::BeatRepeat brDef;
        check(!brDef.enabled() && brDef.repeats() == 1, "beat-repeat is off / passthrough by default");

        // Tempo sync: the cell length locks to the chosen note division. At 120 BPM a 1/4 cell is
        // 500 ms, 1/8 is 250 ms, 1/16 is 125 ms. With sync off, updateTempo leaves sliceMs alone.
        {
            audio::BeatRepeat brs;
            brs.setSliceMs(125.0f);
            brs.updateTempo(120.0); // sync off → no change
            check(std::fabs(brs.sliceMs() - 125.0f) < 1e-3f, "beat-repeat ignores tempo when sync off");
            brs.setSync(true);
            brs.setSyncDivision(2); // 1/4
            brs.updateTempo(120.0);
            check(std::fabs(brs.sliceMs() - 500.0f) < 1.0f, "beat-repeat 1/4 @120 BPM = 500 ms cell");
            brs.setSyncDivision(5); // 1/16
            brs.updateTempo(120.0);
            check(std::fabs(brs.sliceMs() - 125.0f) < 1.0f, "beat-repeat 1/16 @120 BPM = 125 ms cell");
        }
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
