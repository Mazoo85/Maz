// tests/game/timecontrol.cpp — verifies the gameplay time-step authority (game::TimeControl): a smoothly
// ramped global time scale plus transient hit-stop freezes, combined into the scaled delta returned each
// frame. Ground truths are hand-computed: normal scale passes the delta through; a fixed scale multiplies
// it; a ramp moves toward the target at a constant real-time rate without overshoot; a hit-stop freezes the
// whole frame (returns 0), lasts a real-time duration regardless of scale, extends-but-never-shortens, and
// pauses the ramp. Deterministic CPU.
#include "maz/game/TimeControl.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::game::TimeControl;

static bool near1(float a, float b, float e = 1e-5f) { return std::fabs(a - b) <= e; }

int main() {
    // --- 1. Default scale 1 passes the real delta straight through. ---
    {
        TimeControl t;
        CHECK(near1(t.advance(0.016f), 0.016f) && near1(t.scale(), 1.0f), "default scale passes dt through");
    }

    // --- 2. A fixed scale multiplies the delta. ---
    {
        TimeControl t;
        t.setScale(0.5f);
        CHECK(near1(t.advance(0.02f), 0.01f), "half scale halves the delta");
        t.setScale(2.0f);
        CHECK(near1(t.advance(0.02f), 0.04f), "double scale doubles the delta");
        t.setScale(0.0f);
        CHECK(near1(t.advance(0.02f), 0.0f), "zero scale (soft pause) yields no gameplay time");
    }

    // --- 3. setScale clamps negatives to zero. ---
    {
        TimeControl t;
        t.setScale(-1.0f);
        CHECK(near1(t.scale(), 0.0f), "negative scale clamps to 0");
    }

    // --- 4. Ramp eases toward the target at a constant real-time rate, without overshoot. ---
    {
        TimeControl t;                 // scale 1
        t.rampTo(0.0f, 2.0f);          // head to 0 at 2.0/sec
        // First frame: 1 - 2*0.1 = 0.8, delta = 0.1*0.8 = 0.08.
        CHECK(near1(t.advance(0.1f), 0.08f) && near1(t.scale(), 0.8f), "ramp steps toward target");
        CHECK(t.isRamping(), "still ramping mid-way");
        // Drive it well past the target; it must clamp exactly to 0, never overshoot below.
        for (int i = 0; i < 20; ++i) t.advance(0.1f);
        CHECK(near1(t.scale(), 0.0f) && !t.isRamping(), "ramp clamps to target, no overshoot");
    }

    // --- 5. rampTo with rate 0 snaps immediately. ---
    {
        TimeControl t;
        t.rampTo(0.25f, 0.0f);
        CHECK(near1(t.scale(), 0.25f) && !t.isRamping(), "zero-rate ramp snaps");
    }

    // --- 6. Hit-stop freezes the whole frame and counts down in real time. ---
    {
        TimeControl t;                 // scale 1
        t.hitStop(0.1f);
        CHECK(t.isFrozen() && near1(t.hitStopRemaining(), 0.1f), "hit-stop armed");
        CHECK(near1(t.advance(0.03f), 0.0f), "frozen frame returns 0");
        CHECK(near1(t.hitStopRemaining(), 0.07f), "hit-stop counts down by real dt");
        t.advance(0.03f); t.advance(0.03f); // 0.07 -> 0.04 -> 0.01
        CHECK(t.isFrozen() && near1(t.hitStopRemaining(), 0.01f), "still frozen with time left");
        t.advance(0.03f); // consumes the last 0.01, clamps to 0
        CHECK(!t.isFrozen(), "freeze ends after its real-time duration");
        CHECK(near1(t.advance(0.02f), 0.02f), "gameplay resumes at normal speed");
    }

    // --- 7. hitStop extends but never shortens an active freeze. ---
    {
        TimeControl t;
        t.hitStop(0.1f);
        t.hitStop(0.05f); // shorter -> ignored
        CHECK(near1(t.hitStopRemaining(), 0.1f), "shorter hit-stop request ignored");
        t.hitStop(0.2f);  // longer -> extends
        CHECK(near1(t.hitStopRemaining(), 0.2f), "longer hit-stop request extends");
    }

    // --- 8. A freeze pauses the ramp; it resumes exactly where it left off. ---
    {
        TimeControl t;                 // scale 1
        t.rampTo(0.0f, 1.0f);
        t.advance(0.2f);               // scale 1 - 0.2 = 0.8
        CHECK(near1(t.scale(), 0.8f), "ramped to 0.8 before freeze");
        t.hitStop(0.1f);
        t.advance(0.05f);              // frozen: scale must not change
        CHECK(near1(t.scale(), 0.8f) && t.isFrozen(), "ramp paused during freeze");
        t.advance(0.05f);              // freeze ends this frame (0.1 consumed), scale still 0.8
        CHECK(near1(t.scale(), 0.8f), "scale unchanged across the freeze");
        t.advance(0.2f);              // ramp resumes: 0.8 - 0.2 = 0.6
        CHECK(near1(t.scale(), 0.6f), "ramp resumes after freeze");
    }

    // --- 9. Hit-stop lasts the same real time regardless of slow-mo scale. ---
    {
        TimeControl t;
        t.setScale(0.2f);              // heavy slow-mo
        t.hitStop(0.1f);
        t.advance(0.05f);
        CHECK(near1(t.hitStopRemaining(), 0.05f), "freeze uses real time, not scaled time");
    }

    // --- 10. Non-positive real dt is a no-op returning 0. ---
    {
        TimeControl t;
        t.rampTo(0.0f, 1.0f);
        CHECK(near1(t.advance(0.0f), 0.0f) && near1(t.scale(), 1.0f), "zero dt: no time, no ramp");
        CHECK(near1(t.advance(-0.5f), 0.0f) && near1(t.scale(), 1.0f), "negative dt: no-op");
    }

    if (g_fail == 0) std::printf("time control: all tests passed\n");
    return g_fail == 0 ? 0 : 1;
}
