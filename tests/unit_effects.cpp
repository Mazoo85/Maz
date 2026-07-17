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

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
