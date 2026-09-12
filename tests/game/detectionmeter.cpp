// tests/game/detectionmeter.cpp — verifies the stealth awareness system (game::DetectionMeter): a
// per-frame exposure signal integrated over time into a 0..1 meter classified Unaware -> Suspicious ->
// Alerted with fill / linger / decay and hysteresis. Ground truths are hand-computed from the model
// (meter += fillRate*exposure*dt while seen; holds for lingerTime then decays at decayRate; hysteretic
// thresholds), and the full spot-then-lose-then-forget lifecycle is walked end to end. Deterministic CPU.
#include "maz/game/DetectionMeter.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::game::DetectionMeter;
using maz::game::DetectionParams;
using maz::game::Awareness;

static bool near1(float a, float b, float e = 1e-4f) { return std::fabs(a - b) <= e; }

int main() {
    DetectionParams p; // defaults: fill 0.6, decay 0.35, linger 1.0, suspiciousAt 0.25, alertAt 1.0, relaxAt 0.5

    // --- 1. Fresh meter is Unaware at zero. ---
    {
        DetectionMeter d(p);
        CHECK(d.meter() == 0.0f && d.state() == Awareness::Unaware, "starts Unaware at 0");
    }

    // --- 2. Full exposure fills at fillRate; crosses suspiciousAt then reaches Alerted (clamped at 1). ---
    {
        DetectionMeter d(p);
        d.update(1.0f, 0.1f); // meter = 0.06, below suspiciousAt
        CHECK(near1(d.meter(), 0.06f) && d.state() == Awareness::Unaware, "small glimpse stays Unaware");
        // Advance to 0.5s total -> meter 0.30 -> Suspicious.
        for (int i = 0; i < 4; ++i) d.update(1.0f, 0.1f);
        CHECK(near1(d.meter(), 0.30f) && d.state() == Awareness::Suspicious, "sustained view -> Suspicious");
        // Keep looking; meter reaches 1.0 and clamps -> Alerted (needs >= 1/0.6 ~ 1.667s total).
        for (int i = 0; i < 20; ++i) d.update(1.0f, 0.1f);
        CHECK(near1(d.meter(), 1.0f) && d.isAlerted(), "full exposure eventually alerts and clamps at 1");
    }

    // --- 3. Fill rate scales with exposure: half exposure fills at half the rate. ---
    {
        DetectionMeter a(p), b(p);
        for (int i = 0; i < 5; ++i) { a.update(1.0f, 0.1f); b.update(0.5f, 0.1f); }
        CHECK(near1(a.meter(), 0.30f) && near1(b.meter(), 0.15f), "exposure scales the fill rate");
    }

    // --- 4. Linger: after losing sight the meter HOLDS for lingerTime, then decays. ---
    {
        DetectionMeter d(p);
        for (int i = 0; i < 5; ++i) d.update(1.0f, 0.1f); // meter 0.30, linger refreshed to 1.0
        const float held = d.meter();
        d.update(0.0f, 0.5f); // within linger (1.0s) -> frozen
        CHECK(near1(d.meter(), held), "meter holds during linger window");
        CHECK(near1(d.lingerRemaining(), 0.5f), "linger counts down");
        d.update(0.0f, 0.5f); // linger now exactly exhausted, still no decay this step
        CHECK(near1(d.meter(), held), "meter still held on the frame linger hits zero");
        d.update(0.0f, 0.2f); // now decays: 0.30 - 0.35*0.2 = 0.23
        CHECK(near1(d.meter(), 0.23f), "meter decays after linger elapses");
    }

    // --- 5. Full spot -> lose sight -> forget lifecycle with hysteresis. ---
    {
        DetectionMeter d(p);
        for (int i = 0; i < 20; ++i) d.update(1.0f, 0.1f); // fully alerted, meter 1.0
        CHECK(d.isAlerted(), "lifecycle: alerted");
        // Break line of sight; burn the linger hold.
        d.update(0.0f, 1.0f);
        CHECK(d.isAlerted() && near1(d.meter(), 1.0f), "still alerted through linger");
        // Decay from 1.0. Stays Alerted until meter < relaxAt (0.5). 1.0 -> below 0.5 needs >~1.43s.
        d.update(0.0f, 1.0f); // 1.0 - 0.35 = 0.65, still >= relaxAt -> Alerted (hysteresis)
        CHECK(near1(d.meter(), 0.65f) && d.isAlerted(), "hysteresis: holds Alerted above relaxAt");
        d.update(0.0f, 1.0f); // 0.65 - 0.35 = 0.30, now < relaxAt and >= 0 -> Suspicious
        CHECK(near1(d.meter(), 0.30f) && d.state() == Awareness::Suspicious, "drops to Suspicious below relaxAt");
        d.update(0.0f, 1.0f); // 0.30 - 0.35 -> clamps 0 -> Unaware
        CHECK(d.meter() == 0.0f && d.state() == Awareness::Unaware, "forgets fully -> Unaware");
    }

    // --- 6. Clamps: cannot exceed 1 or drop below 0; exposure is clamped to [0,1]. ---
    {
        DetectionMeter d(p);
        for (int i = 0; i < 100; ++i) d.update(5.0f, 0.1f); // huge exposure clamped to 1
        CHECK(d.meter() == 1.0f, "meter clamps at 1 under over-exposure");
        DetectionMeter e(p);
        e.update(-3.0f, 0.5f); // negative exposure clamped to 0 -> no fill, no linger, no decay from 0
        CHECK(e.meter() == 0.0f, "negative exposure clamped, meter stays 0");
    }

    // --- 7. forceAlert and reset. ---
    {
        DetectionMeter d(p);
        d.forceAlert();
        CHECK(d.isAlerted() && d.meter() == 1.0f, "forceAlert slams to full detection");
        d.reset();
        CHECK(d.state() == Awareness::Unaware && d.meter() == 0.0f, "reset returns to Unaware");
    }

    // --- 8. dt <= 0 is a no-op. ---
    {
        DetectionMeter d(p);
        d.update(1.0f, 0.5f);
        const float m = d.meter();
        d.update(1.0f, 0.0f);
        d.update(1.0f, -0.2f);
        CHECK(near1(d.meter(), m), "non-positive dt is a no-op");
    }

    // --- 9. A configured "instant spot" (high fill) alerts within a couple frames. ---
    {
        DetectionParams fast = p;
        fast.fillRate = 10.0f;
        DetectionMeter d(fast);
        d.update(1.0f, 0.1f); // meter 1.0 immediately -> Alerted
        CHECK(d.isAlerted(), "high fill rate gives near-instant detection");
    }

    if (g_fail == 0) std::printf("detection meter: all tests passed\n");
    return g_fail == 0 ? 0 : 1;
}
