// tests/core/fixedtimestep.cpp — verifies the fixed-timestep game-loop driver (core::FixedTimestep): a
// variable frame delta is accumulated and decomposed into a whole number of fixed steps plus an
// interpolation alpha, with a spiral-of-death cap. Ground truths are hand-computed against step=0.1: a
// partial delta yields 0 steps and a matching alpha; deltas that cross the step boundary yield the right
// count and leftover; accumulation carries across frames; a huge delta is capped to maxSteps; alpha stays
// in [0,1); non-positive dt is a no-op. Deterministic CPU.
#include "maz/core/FixedTimestep.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::core::FixedTimestep;

static bool near1(double a, double b, double e = 1e-9) { return std::fabs(a - b) <= e; }

int main() {
    // --- 1. A partial delta runs no step and reports the fractional alpha. ---
    {
        FixedTimestep ts(0.1, 5);
        CHECK(ts.advance(0.05) == 0, "sub-step delta runs 0 steps");
        CHECK(near1(ts.alpha(), 0.5), "alpha is accumulator/step");
        CHECK(near1(ts.accumulated(), 0.05), "leftover accumulated");
    }

    // --- 2. Crossing the boundary runs exactly one step; accumulation carries over. ---
    {
        FixedTimestep ts(0.1, 5);
        ts.advance(0.05);
        CHECK(ts.advance(0.05) == 1, "reaching the step boundary runs 1 step");
        CHECK(near1(ts.alpha(), 0.0) && near1(ts.accumulated(), 0.0), "no leftover after an exact step");
    }

    // --- 3. A larger delta runs multiple steps and keeps the remainder. ---
    {
        FixedTimestep ts(0.1, 5);
        CHECK(ts.advance(0.25) == 2, "0.25 over a 0.1 step runs 2 steps");
        CHECK(near1(ts.alpha(), 0.5), "0.05 remainder -> alpha 0.5"); // 0.25 - 2*0.1
    }

    // --- 4. Exactly one step at a time (the common 1:1 case). ---
    {
        FixedTimestep ts(0.1, 5);
        for (int i = 0; i < 4; ++i) CHECK(ts.advance(0.1) == 1, "one step per matching frame");
        CHECK(near1(ts.alpha(), 0.0), "alpha stays 0 in the 1:1 case");
    }

    // --- 5. Accumulation across several small frames eventually fires a step. ---
    {
        FixedTimestep ts(0.1, 5);
        CHECK(ts.advance(0.03) == 0 && ts.advance(0.03) == 0 && ts.advance(0.03) == 0, "0.09 < step: no step yet");
        CHECK(ts.advance(0.03) == 1, "crossing 0.1 fires the step"); // 0.12 total
        CHECK(near1(ts.alpha(), 0.2), "0.02 remainder -> alpha 0.2");
    }

    // --- 6. Spiral-of-death cap: a huge delta runs at most maxSteps, not hundreds. ---
    {
        FixedTimestep ts(0.1, 5);
        CHECK(ts.advance(10.0) == 5, "huge delta capped to maxSteps");
        CHECK(near1(ts.alpha(), 0.0), "capped frame leaves no runaway backlog");
        // The very next normal frame behaves normally (no accumulated debt).
        CHECK(ts.advance(0.1) == 1, "no backlog carried past the cap");
    }

    // --- 7. Non-positive dt is a no-op. ---
    {
        FixedTimestep ts(0.1, 5);
        ts.advance(0.05);
        CHECK(ts.advance(0.0) == 0 && ts.advance(-1.0) == 0, "non-positive dt runs no steps");
        CHECK(near1(ts.accumulated(), 0.05), "non-positive dt does not change the accumulator");
    }

    // --- 8. alpha is always in [0,1) after advance. ---
    {
        FixedTimestep ts(0.1, 8);
        const double deltas[] = {0.017, 0.033, 0.1, 0.05, 0.2, 0.007, 0.5};
        for (double d : deltas) {
            ts.advance(d);
            CHECK(ts.alpha() >= 0.0 && ts.alpha() < 1.0, "alpha stays in [0,1)");
        }
    }

    // --- 9. reset clears the accumulator; setStep changes the rate and clears leftover. ---
    {
        FixedTimestep ts(0.1, 5);
        ts.advance(0.07);
        ts.reset();
        CHECK(near1(ts.accumulated(), 0.0) && ts.alpha() == 0.0, "reset clears accumulator");
        ts.advance(0.07);
        ts.setStep(0.02);
        CHECK(near1(ts.step(), 0.02) && near1(ts.accumulated(), 0.0), "setStep changes rate and clears leftover");
    }

    // --- 10. Constructor clamps a nonsensical step / maxSteps. ---
    {
        FixedTimestep ts(-1.0, 0);
        CHECK(ts.step() > 0.0 && ts.maxSteps() >= 1, "constructor clamps bad step/maxSteps");
    }

    if (g_fail == 0) std::printf("fixed timestep: all tests passed\n");
    return g_fail == 0 ? 0 : 1;
}
