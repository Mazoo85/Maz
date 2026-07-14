// Unit tests for maz::anim::Tween — a single-value float eased tween.
// Exercises linear + eased interpolation, Once end-clamping, zero-duration
// guarding, Loop/PingPong fmod phase wrap, onComplete fire-once semantics, and
// reset/restart. All asserted floats are exact binary fractions (0/0.25/0.5/
// 1/5/10/...) so equality checks are exact. Pure C++, no GPU/display.

#include "maz/anim/Tween.hpp"

#include <cstdio>

using namespace maz::anim;
using maz::math::Easing;

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
    // --- LINEAR BASIC --------------------------------------------------------
    {
        Tween t(0.0f, 10.0f, 1.0f, Easing::Linear);
        check(t.value() == 0.0f, "linear starts at from");
        check(t.update(0.5f) == 5.0f, "linear halfway == 5");
        check(t.update(0.5f) == 10.0f && t.finished(), "linear end == 10 and finished");
        check(t.progress() == 1.0f, "linear progress 1.0 when done");
    }

    // --- EASING (InQuad) -----------------------------------------------------
    {
        // easeInQuad(0.5) = 0.5*0.5 = 0.25 (exact) -> 0 + 4*0.25 = 1.0.
        Tween t(0.0f, 4.0f, 1.0f, Easing::InQuad);
        check(t.update(0.5f) == 1.0f, "InQuad at t=0.5 -> value 1.0");
    }

    // --- CLAMP PAST END (Once) ----------------------------------------------
    {
        Tween t(0.0f, 10.0f, 1.0f);
        check(t.update(2.0f) == 10.0f && t.finished(), "overshoot clamps to 10 and finishes");
        check(t.update(1.0f) == 10.0f && t.finished(), "further update stays at 10, still finished");
    }

    // --- ZERO DURATION -------------------------------------------------------
    {
        Tween t(0.0f, 10.0f, 0.0f);
        check(t.value() == 10.0f, "zero-duration snaps to 'to' immediately");
        check(t.update(0.1f) == 10.0f && t.finished(), "zero-duration finished, no NaN/inf");
    }

    // --- LOOP ----------------------------------------------------------------
    {
        Tween t(0.0f, 10.0f, 1.0f, Easing::Linear, LoopMode::Loop);
        check(t.update(1.5f) == 5.0f && !t.finished(), "loop fmod(1.5,1)=0.5 -> 5, never finished");
        // progress() and value() must agree: both derive from phase() t=0.5.
        check(t.progress() == 0.5f, "loop progress mid-cycle == 0.5 (matches value 5)");
        check(t.update(1.0f) == 5.0f && !t.finished(), "loop elapsed 2.5 -> fmod 0.5 -> 5");
    }

    // --- PINGPONG ------------------------------------------------------------
    {
        Tween t(0.0f, 10.0f, 1.0f, Easing::Linear, LoopMode::PingPong);
        check(t.update(0.5f) == 5.0f && !t.finished(), "pingpong forward 0.5 -> 5");
        check(t.update(1.0f) == 5.0f && !t.finished(), "pingpong elapsed 1.5, returning -> 5");
        // Return-leg t=0.5: progress() and value() agree (same phase() t).
        check(t.progress() == 0.5f, "pingpong return-leg progress == 0.5 (matches value 5)");
        check(t.update(0.5f) == 0.0f && !t.finished(), "pingpong elapsed 2.0 wraps to 0 -> 0");
    }

    // --- onComplete ONCE -----------------------------------------------------
    {
        Tween t(0.0f, 10.0f, 1.0f);
        int fires = 0;
        t.onComplete([&]{ ++fires; });
        t.update(1.0f);
        check(fires == 1, "onComplete fires once on finish");
        t.update(1.0f);
        check(fires == 1, "onComplete does not re-fire");

        Tween loopT(0.0f, 10.0f, 1.0f, Easing::Linear, LoopMode::Loop);
        int loopFires = 0;
        loopT.onComplete([&]{ ++loopFires; });
        loopT.update(1.0f);
        loopT.update(1.0f);
        loopT.update(1.0f);
        check(loopFires == 0, "loop onComplete never fires");
    }

    // --- RESET / RESTART -----------------------------------------------------
    {
        Tween t(0.0f, 10.0f, 1.0f);
        t.update(2.0f);
        check(t.finished(), "run to finished before reset");
        t.reset();
        check(!t.finished() && t.value() == 0.0f && t.progress() == 0.0f, "reset returns to from, not finished");
        t.restart(5.0f, 15.0f, 2.0f);
        check(t.value() == 5.0f && !t.finished(), "restart sets value to new from");
        check(t.update(1.0f) == 10.0f, "restart midpoint t=0.5 -> 10");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
