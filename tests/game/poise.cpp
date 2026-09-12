// tests/game/poise.cpp — verifies the poise/posture super-armor mechanic (game::Poise): a second resource
// separate from health that decides whether a hit interrupts you. Ground truths are hand-computed: a hit
// smaller than poise drains it without breaking; a hit that empties it breaks (staggered) exactly once;
// the stagger lasts its configured time then recovers to full; poise regenerates only after a post-hit
// lull, at the configured rate, clamped to max; sustained hits reset the lull; hits while broken are
// absorbed. Deterministic CPU.
#include "maz/game/Poise.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::game::Poise;
using maz::game::PoiseParams;

static bool near1(float a, float b, float e = 1e-3f) { return std::fabs(a - b) <= e; }

int main() {
    PoiseParams p; // max 100, stagger 1.5s, regenDelay 2.0s, regenRate 40/s

    // --- 1. Fresh poise is full and unbroken. ---
    {
        Poise q(p);
        CHECK(q.poise() == 100.0f && !q.isBroken() && q.isFull(), "starts full, unbroken");
    }

    // --- 2. A sub-lethal hit drains poise without breaking. ---
    {
        Poise q(p);
        CHECK(!q.takeStagger(30.0f), "hit under threshold does not break");
        CHECK(near1(q.poise(), 70.0f) && !q.isBroken(), "poise drained by the hit amount");
    }

    // --- 3. A hit that empties poise breaks exactly once. ---
    {
        Poise q(p);
        q.takeStagger(60.0f); // 40 left
        CHECK(q.takeStagger(50.0f), "the emptying hit returns true (newly broken)");
        CHECK(q.isBroken() && q.poise() == 0.0f, "broken at zero poise");
        CHECK(!q.takeStagger(20.0f), "further hits while broken do not re-break");
    }

    // --- 4. Stagger lasts its configured time, then recovers to full. ---
    {
        Poise q(p);
        q.takeStagger(150.0f); // break immediately
        q.update(1.0f);        // 1.0 of 1.5s
        CHECK(q.isBroken() && near1(q.staggerRemaining(), 0.5f), "still staggered mid-timer");
        q.update(0.5f);        // stagger elapses
        CHECK(!q.isBroken() && q.isFull(), "recovers to full poise after stagger");
    }

    // --- 5. Poise regenerates only after the post-hit lull, at the configured rate, clamped to max. ---
    {
        Poise q(p);
        q.takeStagger(40.0f); // 60 left, regen lull armed to 2.0s
        q.update(1.0f);       // still within lull
        CHECK(near1(q.poise(), 60.0f), "no regen during the lull");
        q.update(1.0f);       // lull exactly elapses this frame; no regen yet
        CHECK(near1(q.poise(), 60.0f), "no regen on the frame the lull ends");
        q.update(0.5f);       // now regenerates: 60 + 40*0.5 = 80
        CHECK(near1(q.poise(), 80.0f), "regenerates at regenRate after the lull");
        q.update(10.0f);      // clamps at max
        CHECK(q.isFull(), "regen clamps to max");
    }

    // --- 6. Sustained pressure resets the lull so poise never recovers. ---
    {
        Poise q(p);
        q.takeStagger(20.0f); // 80, lull 2.0
        q.update(1.9f);       // almost through the lull
        q.takeStagger(10.0f); // 70, lull reset to 2.0
        q.update(1.0f);
        CHECK(near1(q.poise(), 70.0f), "a fresh hit re-arms the lull, blocking regen");
    }

    // --- 7. reset restores full unbroken poise. ---
    {
        Poise q(p);
        q.takeStagger(150.0f);
        q.reset();
        CHECK(q.isFull() && !q.isBroken(), "reset restores full poise");
    }

    // --- 8. Non-positive dt and non-positive stagger are no-ops (but a hit still arms the lull). ---
    {
        Poise q(p);
        q.update(0.0f);
        q.update(-1.0f);
        CHECK(q.isFull(), "non-positive dt does nothing");
        CHECK(!q.takeStagger(0.0f) && q.isFull(), "zero stagger does not drain poise");
    }

    // --- 9. A zero-max params object is clamped so fraction stays well-defined. ---
    {
        PoiseParams bad;
        bad.maxPoise = 0.0f;
        Poise q(bad);
        CHECK(q.max() > 0.0f && std::isfinite(q.fraction()), "zero max clamped, fraction finite");
    }

    if (g_fail == 0) std::printf("poise: all tests passed\n");
    return g_fail == 0 ? 0 : 1;
}
