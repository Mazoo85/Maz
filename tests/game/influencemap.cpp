// tests/game/influencemap.cpp — verifies the tactical influence map (game InfluenceMap.hpp).
// Ground truths, deterministic:
//   * a single positive source diffuses to a symmetric field that decreases with distance from it;
//   * decay < 1 shrinks the total magnitude every step; spread = 0 only decays in place (no bleed);
//   * a friendly (+) and hostile (-) source placed symmetrically leave the midline at ~0 (the front line);
//   * the gradient points toward a positive source (advance/flee direction);
//   * peak() / trough() find the friendly and hostile hot-spots.
#include "maz/game/InfluenceMap.hpp"

#include <cmath>
#include <cstdio>

using maz::game::InfluenceMap;
using maz::math::vec2;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

int main() {
    // --- 1. Single source diffuses symmetrically and falls off with distance. ---
    {
        InfluenceMap im(21, 21);
        im.addSource(10, 10, 100.0f);
        for (int i = 0; i < 12; ++i) im.propagate(1.0f, 0.5f); // no decay, pure spread
        // Symmetry about the centre.
        CHECK(std::fabs(im.at(10 + 3, 10) - im.at(10 - 3, 10)) < 1e-3f, "horizontally symmetric");
        CHECK(std::fabs(im.at(10, 10 + 3) - im.at(10, 10 - 3)) < 1e-3f, "vertically symmetric");
        CHECK(std::fabs(im.at(10 + 2, 10) - im.at(10, 10 + 2)) < 1e-3f, "radially symmetric (4-fold)");
        // Monotone falloff along a ray from the centre.
        bool falloff = true;
        for (int d = 0; d < 8; ++d)
            if (im.at(10 + d + 1, 10) > im.at(10 + d, 10) + 1e-4f) falloff = false;
        CHECK(falloff, "influence decreases with distance from the source");
        CHECK(im.at(10, 10) > 0.0f, "the source cell stays the strongest");
    }

    // --- 2. Decay shrinks the total; spread=0 keeps influence local. ---
    {
        InfluenceMap im(11, 11);
        im.addSource(5, 5, 64.0f);
        float before = im.at(5, 5);
        im.propagate(0.5f, 0.0f); // spread 0 -> only decay, no bleed
        CHECK(std::fabs(im.at(5, 5) - before * 0.5f) < 1e-3f, "spread=0 just halves in place");
        CHECK(im.at(6, 5) == 0.0f, "spread=0 does not leak to neighbours");

        InfluenceMap im2(11, 11);
        im2.addSource(5, 5, 100.0f);
        auto total = [](const InfluenceMap& m) {
            float s = 0.0f;
            for (int y = 0; y < m.height(); ++y)
                for (int x = 0; x < m.width(); ++x) s += std::fabs(m.at(x, y));
            return s;
        };
        const float t0 = total(im2);
        im2.propagate(0.9f, 0.5f);
        CHECK(total(im2) < t0, "decay<1 reduces the total magnitude");
    }

    // --- 3. Friendly + hostile -> zero front line on the midline. ---
    {
        InfluenceMap im(21, 11);
        im.addSource(5, 5, 100.0f);   // friendly, left
        im.addSource(15, 5, -100.0f); // hostile, right (mirror about x=10)
        for (int i = 0; i < 15; ++i) im.propagate(0.98f, 0.5f);
        bool midZero = true;
        for (int y = 0; y < 11; ++y)
            if (std::fabs(im.at(10, y)) > 1e-2f) midZero = false;
        CHECK(midZero, "the midline between equal-and-opposite sources is ~0 (the front line)");
        CHECK(im.at(5, 5) > 0.0f && im.at(15, 5) < 0.0f, "friendly side positive, hostile side negative");
    }

    // --- 4. Gradient points toward a positive source. ---
    {
        InfluenceMap im(21, 21);
        im.addSource(10, 10, 100.0f);
        for (int i = 0; i < 12; ++i) im.propagate(1.0f, 0.5f);
        // At a cell to the LEFT of the source, ascent points RIGHT (+x) toward it.
        const vec2 gL = im.gradient(6, 10);
        CHECK(gL.x > 0.0f && std::fabs(gL.y) < 1e-3f, "left of source, gradient points right toward it");
        // Below the source, ascent points UP (+y) toward it.
        const vec2 gB = im.gradient(10, 6);
        CHECK(gB.y > 0.0f && std::fabs(gB.x) < 1e-3f, "below source, gradient points up toward it");
    }

    // --- 5. peak / trough. ---
    {
        InfluenceMap im(15, 15);
        im.addSource(3, 4, 50.0f);
        im.addSource(11, 9, -70.0f);
        int px, py, tx, ty;
        const float pv = im.peak(px, py);
        const float tv = im.trough(tx, ty);
        CHECK(px == 3 && py == 4 && pv > 0.0f, "peak is the friendly source");
        CHECK(tx == 11 && ty == 9 && tv < 0.0f, "trough is the hostile source");
    }

    if (g_fail == 0) {
        std::printf("influencemap: OK — diffusion falloff, decay/spread, front line, gradient, peak.\n");
        return 0;
    }
    std::printf("influencemap: %d failure(s).\n", g_fail);
    return 1;
}
