// tests/game/firespread.cpp — verifies deterministic grid fire/contagion spread (game::FireGrid).
// Ground truths: with spreadRate=threshold=1 and abundant fuel, a single lit source spreads by Manhattan
// distance (4-connected) — distance-k cells are burning after exactly k steps; a zero-fuel cell is a
// firebreak the fire never crosses; a burning cell consumes its fuel and turns Burnt after fuel/burnRate
// ticks; a source that burns out before delivering enough heat leaves its neighbour unignited; wind biases
// the front so the downwind cell ignites while the upwind one does not; the sim is deterministic and
// out-of-bounds / degenerate inputs are safe. Pure CPU, no RNG.
#include "maz/game/FireSpread.hpp"

#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::game::FireGrid;
using maz::game::FireParams;
using maz::game::BurnState;

int main() {
    // --- 1. Manhattan-rate spread on an infinite-fuel field. ---
    {
        FireGrid g(7, 7);
        for (int y = 0; y < 7; ++y)
            for (int x = 0; x < 7; ++x) g.setFuel(x, y, 100.0f); // won't burn out during the test
        g.ignite(3, 3);
        FireParams p; // burnRate=spreadRate=threshold=1, no wind, 4-connected
        g.step(1.0f, p); // step 1
        CHECK(g.isBurning(4, 3) && g.isBurning(2, 3) && g.isBurning(3, 4) && g.isBurning(3, 2),
              "step1: dist-1 orthogonal cells burning");
        CHECK(!g.isBurning(5, 3), "step1: dist-2 not yet burning");
        g.step(1.0f, p); // step 2
        CHECK(g.isBurning(5, 3), "step2: dist-2 burning");
        CHECK(!g.isBurning(6, 3), "step2: dist-3 not yet burning");
        g.step(1.0f, p); // step 3
        CHECK(g.isBurning(6, 3), "step3: dist-3 burning");
    }

    // --- 2. A zero-fuel cell is a firebreak. ---
    {
        FireGrid g(7, 1);
        for (int x = 0; x < 7; ++x) g.setFuel(x, 0, 100.0f);
        g.setFuel(3, 0, 0.0f); // firebreak
        g.ignite(0, 0);
        FireParams p;
        for (int s = 0; s < 12; ++s) g.step(1.0f, p);
        CHECK(g.isBurning(2, 0), "fire reaches up to the firebreak");
        CHECK(g.at(3, 0).state == BurnState::Unburnt, "firebreak never ignites");
        CHECK(g.at(4, 0).state == BurnState::Unburnt, "beyond the firebreak stays unburnt");
        CHECK(g.at(6, 0).state == BurnState::Unburnt, "far side stays unburnt");
    }

    // --- 3. A burning cell consumes fuel and burns out. ---
    {
        FireGrid g(1, 1);
        g.setFuel(0, 0, 2.0f);
        g.ignite(0, 0);
        FireParams p; // burnRate 1
        g.step(1.0f, p);
        CHECK(g.at(0, 0).state == BurnState::Burning, "fuel 2 -> still burning after 1 tick");
        g.step(1.0f, p);
        CHECK(g.at(0, 0).state == BurnState::Burnt, "fuel 2 -> burnt after 2 ticks");
    }

    // --- 4. A source that burns out first leaves a high-threshold neighbour unignited. ---
    {
        FireGrid g(2, 1);
        g.setFuel(0, 0, 2.0f);   // source burns for only 2 ticks
        g.setFuel(1, 0, 100.0f);
        g.ignite(0, 0);
        FireParams p;
        p.ignitionThreshold = 5.0f; // needs 5 heat; source only delivers 2 before dying
        for (int s = 0; s < 6; ++s) g.step(1.0f, p);
        CHECK(g.at(0, 0).state == BurnState::Burnt, "source burnt out");
        CHECK(g.at(1, 0).state == BurnState::Unburnt, "neighbour never reached ignition threshold");
    }

    // --- 5. Wind biases the front: downwind ignites, upwind does not (same single step). ---
    {
        FireGrid g(5, 5);
        for (int y = 0; y < 5; ++y)
            for (int x = 0; x < 5; ++x) g.setFuel(x, y, 100.0f);
        g.ignite(2, 2);
        FireParams p;
        p.ignitionThreshold = 1.5f; // one unbiased neighbour (heat 1) is not enough
        p.wind = maz::math::vec2(1.0f, 0.0f);
        p.windStrength = 1.0f; // downwind bias doubles the heat -> 2 >= 1.5
        g.step(1.0f, p);
        CHECK(g.isBurning(3, 2), "downwind (east) cell ignites under wind");
        CHECK(!g.isBurning(1, 2), "upwind (west) cell does not ignite");
        CHECK(!g.isBurning(2, 3) && !g.isBurning(2, 1), "cross-wind cells do not ignite");
    }

    // --- 6. Determinism: identical runs match cell-for-cell. ---
    {
        auto run = [] {
            FireGrid g(9, 9);
            for (int y = 0; y < 9; ++y)
                for (int x = 0; x < 9; ++x) g.setFuel(x, y, (x + y) % 3 == 0 ? 0.0f : 3.0f);
            g.ignite(4, 4);
            FireParams p;
            p.diagonal = true;
            p.wind = maz::math::vec2(0.3f, -0.7f);
            p.windStrength = 0.8f;
            for (int s = 0; s < 10; ++s) g.step(0.5f, p);
            return g;
        };
        FireGrid a = run(), b = run();
        bool same = true;
        for (int y = 0; y < 9 && same; ++y)
            for (int x = 0; x < 9 && same; ++x) {
                same = a.at(x, y).state == b.at(x, y).state && a.at(x, y).fuel == b.at(x, y).fuel;
            }
        CHECK(same, "deterministic across identical runs");
    }

    // --- 7. Degenerate inputs are safe. ---
    {
        FireGrid empty(0, 5);
        FireParams p;
        CHECK(empty.step(1.0f, p) == 0, "empty grid step returns 0");
        FireGrid g(3, 3); // all fuel 0
        CHECK(!g.ignite(1, 1), "cannot ignite an inert cell");
        CHECK(!g.ignite(99, 99), "out-of-bounds ignite is safe");
        CHECK(g.step(1.0f, p) == 0, "no fuel -> nothing burns");
    }

    if (g_fail == 0) {
        std::printf("fire spread: all tests passed\n");
    }
    return g_fail == 0 ? 0 : 1;
}
