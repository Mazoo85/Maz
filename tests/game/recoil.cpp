// tests/game/recoil.cpp — verifies the FPS spray-pattern recoil (game::RecoilPattern).
// Ground truths: each fire() adds the next pattern kick and accumulates; firing past the pattern end repeats
// the last kick (sustained climb); shotIndex tracks the burst length; update(dt) recovers the offset toward
// centre by exp(-rate*dt) without overshooting the sign; release() restarts the pattern but leaves the offset
// to keep recovering; reset() clears both; an empty pattern is a safe no-op; recoveryRate 0 means no recovery.
// Values checked against the accumulation + exponential-decay definitions. Pure CPU, deterministic.
#include "maz/game/Recoil.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::game::RecoilPattern;
using maz::math::vec2;

static bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

int main() {
    // A climbing pattern: up, up-right, up-left.
    const std::vector<vec2> pattern = {vec2(0, 1), vec2(1, 1), vec2(-1, 1)};

    // --- 1. Each shot accumulates the next kick; shotIndex tracks the burst. ---
    {
        RecoilPattern r(pattern, 8.0f);
        CHECK(near(r.offset().x, 0) && near(r.offset().y, 0), "starts centred");
        vec2 o = r.fire();
        CHECK(near(o.x, 0) && near(o.y, 1) && r.shotIndex() == 1, "shot 1 -> (0,1)");
        o = r.fire();
        CHECK(near(o.x, 1) && near(o.y, 2) && r.shotIndex() == 2, "shot 2 accumulates -> (1,2)");
        o = r.fire();
        CHECK(near(o.x, 0) && near(o.y, 3) && r.shotIndex() == 3, "shot 3 -> (0,3)");
    }

    // --- 2. Firing past the pattern repeats the last kick. ---
    {
        RecoilPattern r(pattern, 8.0f);
        r.fire(); r.fire(); r.fire(); // offset (0,3)
        const vec2 o = r.fire();      // repeats last kick (-1,1) -> (-1,4)
        CHECK(near(o.x, -1) && near(o.y, 4) && r.shotIndex() == 4, "beyond pattern repeats last kick");
    }

    // --- 3. update recovers toward centre by exp(-rate*dt); no overshoot. ---
    {
        RecoilPattern r(pattern, 1.0f); // rate 1 for an easy closed form
        r.fire(); r.fire(); r.fire();   // offset (0,3)
        r.update(1.0f);                 // multiply by exp(-1)
        const float e = std::exp(-1.0f);
        CHECK(near(r.offset().y, 3.0f * e) && near(r.offset().x, 0), "offset decays by exp(-1) after 1s");
        // Long recovery drives it near zero, and the sign never flips.
        for (int i = 0; i < 50; ++i) r.update(0.5f);
        CHECK(std::fabs(r.offset().y) < 1e-3f && r.offset().y >= 0.0f, "offset eases to ~0 without overshoot");
    }

    // --- 4. release restarts the pattern but keeps the recovering offset. ---
    {
        RecoilPattern r(pattern, 4.0f);
        r.fire(); r.fire();          // index 2, offset (1,2)
        const vec2 held = r.offset();
        r.release();
        CHECK(r.shotIndex() == 0, "release restarts the pattern index");
        CHECK(near(r.offset().x, held.x) && near(r.offset().y, held.y), "release keeps the current offset");
        const vec2 o = r.fire();     // next burst starts at pattern[0] = (0,1), added to held offset
        CHECK(near(o.x, held.x + 0.0f) && near(o.y, held.y + 1.0f), "post-release fire uses pattern[0]");
    }

    // --- 5. reset clears offset + index; empty pattern & rate 0 are safe. ---
    {
        RecoilPattern r(pattern, 8.0f);
        r.fire(); r.fire();
        r.reset();
        CHECK(near(r.offset().x, 0) && near(r.offset().y, 0) && r.shotIndex() == 0, "reset clears everything");

        RecoilPattern empty;
        CHECK(empty.empty(), "default pattern is empty");
        const vec2 o = empty.fire();
        CHECK(near(o.x, 0) && near(o.y, 0) && empty.shotIndex() == 0, "empty fire is a safe no-op");

        RecoilPattern noRecover(pattern, 0.0f);
        noRecover.fire();
        const vec2 before = noRecover.offset();
        noRecover.update(5.0f);
        CHECK(near(noRecover.offset().y, before.y), "recoveryRate 0 -> no recovery");
    }

    if (g_fail == 0) {
        std::printf("recoil: OK — accumulation, pattern repeat, exp recovery, release/reset, empty/no-recover.\n");
        return 0;
    }
    std::printf("recoil: %d failure(s).\n", g_fail);
    return 1;
}
