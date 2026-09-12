// tests/core/framepacer.cpp — verifies core::FramePacer, the frame-rate cap / power-save pacer that keeps a
// mobile game from cooking the battery by running faster than the display. Pure deterministic policy (no real
// clock or sleep): sleep = budget - work at the target fps, zero when uncapped or over budget, drift
// correction that pays back heavy frames, an idle cap for menus/pause, expected-fps, and config sanitizing.
#include "maz/core/FramePacer.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::core;

static bool approx(double a, double b) { return std::fabs(a - b) < 1e-6; }

int main() {
    const double p60 = 1.0 / 60.0; // 16.67 ms

    // --- 1. Under budget -> sleep fills the rest of the frame to the target period. ---
    {
        FramePacer fp; // active 60fps, drift on
        double sleep = fp.sleepFor(0.004); // 4ms of work at a 16.67ms budget
        CHECK(approx(sleep, p60 - 0.004), "sleep = period - work when under budget");
        CHECK(approx(fp.targetPeriod(), p60), "target period is 1/60");
        CHECK(approx(fp.targetFps(), 60.0), "target fps is 60");
    }

    // --- 2. Uncapped (activeFps <= 0) never sleeps. ---
    {
        FramePacerConfig cfg;
        cfg.activeFps = 0.0;
        FramePacer fp(cfg);
        CHECK(approx(fp.sleepFor(0.001), 0.0), "uncapped never sleeps");
        CHECK(approx(fp.targetPeriod(), 0.0), "uncapped period is 0");
    }

    // --- 3. Over budget -> zero sleep. ---
    {
        FramePacerConfig cfg;
        cfg.driftCorrection = false;
        FramePacer fp(cfg);
        CHECK(approx(fp.sleepFor(0.030), 0.0), "a frame slower than the budget sleeps zero");
    }

    // --- 4. Drift correction: a heavy frame's overrun is paid back by a shorter next sleep. ---
    {
        FramePacer fp; // drift on
        // Frame A overruns by ~3.33ms (20ms work vs 16.67 budget) -> zero sleep, carries the deficit.
        double a = fp.sleepFor(0.020);
        CHECK(approx(a, 0.0), "over-budget frame sleeps zero");
        // Frame B does 4ms of work: normally it'd sleep p60-0.004, but the carried deficit shortens it.
        double b = fp.sleepFor(0.004);
        CHECK(approx(b, p60 - 0.004 - (0.020 - p60)), "next sleep is shortened by the carried overrun");
        CHECK(b < p60 - 0.004, "drift correction makes the recovery sleep shorter");
    }

    // --- 4b. Without drift correction, an identical heavy frame is NOT paid back. ---
    {
        FramePacerConfig cfg;
        cfg.driftCorrection = false;
        FramePacer fp(cfg);
        fp.sleepFor(0.020);                 // over budget, no carry
        double b = fp.sleepFor(0.004);
        CHECK(approx(b, p60 - 0.004), "no drift correction -> next sleep is the plain budget-minus-work");
    }

    // --- 4c. The carried deficit is bounded to one period (a long stall can't cause a long sprint). ---
    {
        FramePacer fp;
        fp.sleepFor(5.0);                   // a giant stall (clamped to maxFrameSeconds internally)
        // Even so, the debt is at most one period, so a following idle frame still sleeps >= 0 and the
        // pacer recovers within a single frame rather than skipping many.
        double next = fp.sleepFor(0.0);
        CHECK(next >= 0.0 && next <= p60, "post-stall sleep stays within [0, period] (bounded debt)");
    }

    // --- 5. Idle mode swaps to the lower cap (battery saver on menu/pause). ---
    {
        FramePacerConfig cfg;
        cfg.activeFps = 60.0;
        cfg.idleFps = 10.0;
        FramePacer fp(cfg);
        CHECK(approx(fp.targetFps(), 60.0), "active cap in effect by default");
        fp.setIdle(true);
        CHECK(fp.idle() && approx(fp.targetFps(), 10.0), "idle mode uses the idle cap");
        CHECK(approx(fp.sleepFor(0.004), 1.0 / 10.0 - 0.004), "idle sleep fills a 100ms frame");
        fp.setIdle(false);
        CHECK(approx(fp.targetFps(), 60.0), "leaving idle restores the active cap");
    }

    // --- 5b. Idle mode with idleFps <= 0 is disabled: stays on the active cap. ---
    {
        FramePacerConfig cfg;
        cfg.activeFps = 60.0;
        cfg.idleFps = 0.0; // disabled
        FramePacer fp(cfg);
        fp.setIdle(true);
        CHECK(approx(fp.targetFps(), 60.0), "idle with no idle cap keeps the active cap");
    }

    // --- 6. expectedFps: capped at target, but lower when work alone exceeds the budget. ---
    {
        FramePacer fp; // 60fps
        CHECK(approx(fp.expectedFps(0.004), 60.0), "light work is capped at the target fps");
        CHECK(approx(fp.expectedFps(0.040), 25.0), "heavy work (40ms) yields 25fps, below the cap");
    }

    // --- 7. Config sanitize: negative fps -> uncapped/disabled; zero maxFrame -> restored default. ---
    {
        FramePacerConfig cfg;
        cfg.activeFps = -5.0;
        cfg.idleFps = -1.0;
        cfg.maxFrameSeconds = 0.0;
        FramePacer fp(cfg);
        const FramePacerConfig& c = fp.config();
        CHECK(approx(c.activeFps, 0.0), "negative active fps normalized to 0 (uncapped)");
        CHECK(approx(c.idleFps, 0.0), "negative idle fps normalized to 0 (disabled)");
        CHECK(c.maxFrameSeconds > 0.0, "zero maxFrameSeconds restored to a positive default");
    }

    // --- 8. runtime cap change + reset clears drift. ---
    {
        FramePacer fp;
        fp.setActiveFps(30.0);
        CHECK(approx(fp.targetPeriod(), 1.0 / 30.0), "setActiveFps changes the cap at runtime");
        fp.sleepFor(0.050);   // over the 33ms budget -> carries a deficit
        fp.reset();
        double s = fp.sleepFor(0.004); // after reset, no carry: plain budget - work
        CHECK(approx(s, 1.0 / 30.0 - 0.004), "reset clears the drift accumulator");
    }

    if (g_fail == 0) {
        std::printf("frame_pacer: OK — cap sleep, uncapped, over-budget, drift correction, idle cap, "
                    "expected fps, sanitize, reset.\n");
        return 0;
    }
    std::printf("frame_pacer: %d failure(s).\n", g_fail);
    return 1;
}
