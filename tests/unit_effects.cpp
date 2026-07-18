// Unit tests for the A3 mixer + effects — pure DSP, no audio device. Each effect is exercised on a
// known stereo signal and checked for the behaviour that defines it (echo tap, gain reduction,
// high-cut, reverb tail) plus the mixer's master gain and transparent-by-default passthrough.

#include "maz/audio/Effects.hpp"
#include "maz/audio/Mixer.hpp"

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

        audio::Limiter dl;
        check(!dl.enabled() && std::fabs(dl.ceilingDb() + 0.3f) < 1e-4f,
              "limiter defaults to off with a −0.3 dB ceiling");
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
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
