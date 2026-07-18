// Unit tests for the A4 automation system — the LFO shapes/rate and the Automation bank driving a
// live AudioEngine parameter over time. Pure DSP/logic, no audio device.

#include "maz/audio/AudioEngine.hpp"
#include "maz/audio/Automation.hpp"
#include "maz/audio/LFO.hpp"

#include <cmath>
#include <cstdio>

using namespace maz;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

} // namespace

int main() {
    // --- LFO ------------------------------------------------------------------
    audio::LFO sine;
    sine.shape = audio::Waveform::Sine;
    sine.rateHz = 1.0f;
    check(std::fabs(sine.valueBipolar(0.0)) < 1e-4, "sine LFO starts at 0");
    check(sine.valueBipolar(0.25) > 0.99f, "sine LFO peaks at a quarter period");
    check(std::fabs(sine.valueBipolar(1.0)) < 1e-4, "sine LFO completes one cycle in 1 s at 1 Hz");

    audio::LFO saw;
    saw.shape = audio::Waveform::Saw;
    saw.rateHz = 2.0f;
    // At 2 Hz the phase wraps every 0.5 s; a saw ramps -1→1 across each period.
    check(saw.valueBipolar(0.0) < -0.99f, "saw LFO starts low");
    check(saw.valueUnipolar(0.0) >= 0.0f && saw.valueUnipolar(0.0) <= 0.01f,
          "unipolar maps the low point to ~0");

    // --- Automation bank ------------------------------------------------------
    audio::Automation automation;
    check(!automation.anyEnabled(), "no lanes enabled by default");

    audio::AutoLane& lane = automation.lane(audio::AutoTarget::FilterCutoff);
    lane.enabled = true;
    lane.lfo.shape = audio::Waveform::Sine;
    lane.lfo.rateHz = 1.0f;
    lane.lo = 500.0f;
    lane.hi = 5000.0f;
    check(automation.anyEnabled(), "enabling a lane is observable");

    // Applying at different times must move the target between its bounds.
    audio::AudioEngine engine;
    engine.initOffline();
    automation.apply(engine, 0.0); // sine=0 → unipolar 0.5 → midpoint
    const float cutoffMid = engine.mixer().eq().cutoff();
    automation.apply(engine, 0.25); // sine peak → unipolar 1 → hi bound
    const float cutoffHi = engine.mixer().eq().cutoff();
    automation.apply(engine, 0.75); // sine trough → unipolar 0 → lo bound
    const float cutoffLo = engine.mixer().eq().cutoff();

    check(std::fabs(cutoffMid - 2750.0f) < 50.0f, "midpoint sweep lands between the bounds");
    check(cutoffHi > 4900.0f, "peak sweep reaches the high bound");
    check(cutoffLo < 600.0f, "trough sweep reaches the low bound");
    check(engine.mixer().eq().enabled(), "an active cutoff lane switches the EQ on");

    // --- Automation clips (breakpoint envelopes) ------------------------------
    {
        // A clip ramps 0 → 1 over 0 → 2 s, then holds. It must interpolate linearly and take
        // priority over the LFO on the same lane.
        audio::AutoLane clipLane;
        clipLane.lfo.shape = audio::Waveform::Sine; // would give 0.5 at t=0 if it were used
        clipLane.clip = {{0.0, 0.0f}, {2.0, 1.0f}};
        clipLane.clipLength = 0.0; // hold past the last point

        check(std::fabs(clipLane.sourceUnipolar(0.0) - 0.0f) < 1e-4f, "clip holds first value at t=0");
        check(std::fabs(clipLane.sourceUnipolar(1.0) - 0.5f) < 1e-4f, "clip interpolates to midpoint");
        check(std::fabs(clipLane.sourceUnipolar(2.0) - 1.0f) < 1e-4f, "clip reaches last value");
        check(std::fabs(clipLane.sourceUnipolar(5.0) - 1.0f) < 1e-4f,
              "clip holds the last value past its end");

        // With a loop length, time wraps: at t = clipLength it is back to the start value.
        audio::AutoLane looped;
        looped.clip = {{0.0, 0.2f}, {1.0, 0.8f}};
        looped.clipLength = 2.0;
        check(std::fabs(looped.sourceUnipolar(0.0) - 0.2f) < 1e-4f, "looped clip starts at 0.2");
        check(std::fabs(looped.sourceUnipolar(2.0) - 0.2f) < 1e-4f,
              "looped clip wraps back to the start after clipLength");
        check(std::fabs(looped.sourceUnipolar(0.5) - 0.5f) < 1e-4f, "looped clip midpoint interpolates");
    }

    // --- Extended targets: delay mix + distortion drive ----------------------
    {
        audio::Automation autom;
        audio::AutoLane& dl = autom.lane(audio::AutoTarget::DelayMix);
        dl.enabled = true;
        dl.lfo.shape = audio::Waveform::Sine;
        dl.lfo.rateHz = 1.0f;
        dl.lo = 0.0f;
        dl.hi = 0.5f;
        audio::AutoLane& dd = autom.lane(audio::AutoTarget::DistDrive);
        dd.enabled = true;
        dd.lfo.shape = audio::Waveform::Sine;
        dd.lfo.rateHz = 1.0f;
        dd.lo = 2.0f;
        dd.hi = 8.0f;

        audio::AudioEngine eng;
        eng.initOffline();
        autom.apply(eng, 0.25); // sine peak → unipolar 1 → hi bounds
        check(eng.mixer().delay().enabled() && eng.mixer().delay().mix() > 0.45f,
              "automating delay mix drives (and enables) the delay");
        check(eng.mixer().distortion().enabled() && eng.mixer().distortion().drive() > 7.5f,
              "automating distortion drive drives (and enables) the distortion");
        autom.apply(eng, 0.75); // trough → lo bounds
        check(eng.mixer().delay().mix() < 0.05f, "delay-mix automation reaches its low bound");
    }

    // --- Stereo-width target -------------------------------------------------
    {
        audio::Automation autom;
        audio::AutoLane& sw = autom.lane(audio::AutoTarget::StereoWidth);
        sw.enabled = true;
        sw.lfo.shape = audio::Waveform::Sine;
        sw.lfo.rateHz = 1.0f;
        sw.lo = 0.0f;
        sw.hi = 2.0f;

        audio::AudioEngine eng;
        eng.initOffline();
        autom.apply(eng, 0.25); // sine peak → unipolar 1 → hi bound (2.0)
        check(eng.mixer().widener().enabled() && eng.mixer().widener().width() > 1.9f,
              "automating stereo width drives (and enables) the widener");
        autom.apply(eng, 0.75); // trough → lo bound (0 = mono)
        check(eng.mixer().widener().width() < 0.1f, "stereo-width automation reaches its low bound");
    }

    // A disabled lane leaves its target untouched.
    audio::Automation idle;
    audio::AudioEngine engine2;
    engine2.initOffline();
    const float before = engine2.mixer().masterGain();
    idle.apply(engine2, 0.3);
    check(std::fabs(engine2.mixer().masterGain() - before) < 1e-6f,
          "disabled lanes do not touch their targets");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
