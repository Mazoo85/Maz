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
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
