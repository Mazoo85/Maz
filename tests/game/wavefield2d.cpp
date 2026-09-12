// tests/game/wavefield2d.cpp — verifies the 2D wave/ripple simulation (WaveField2D.hpp).
// Ground truths, deterministic:
//   * a still surface (no disturbance) stays flat forever;
//   * a drop at the exact centre of a square grid stays 4-fold symmetric as it spreads;
//   * the ripple propagates outward — cells next to the drop become non-zero after stepping;
//   * damping makes the total amplitude decay toward zero over many steps;
//   * the border is held at rest (a fixed shore).
#include "maz/game/WaveField2D.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::game::WaveField2D;

static bool near(float a, float b, float e = 1e-5f) { return std::fabs(a - b) < e; }

int main() {
    // --- 1. Still surface stays flat. ---
    {
        WaveField2D f(9, 9);
        for (int s = 0; s < 20; ++s) f.step();
        bool flat = true;
        for (int y = 0; y < 9; ++y)
            for (int x = 0; x < 9; ++x)
                if (!near(f.at(x, y), 0.0f)) flat = false;
        CHECK(flat, "an undisturbed surface stays perfectly flat");
    }

    // --- 2. Centred drop -> 4-fold symmetry. ---
    {
        const int n = 21;
        WaveField2D f(n, n, 0.01f);
        const int c = n / 2;
        f.disturb(c, c, 1.0f);
        for (int s = 0; s < 15; ++s) f.step();
        bool sym = true;
        for (int dy = 0; dy <= c; ++dy) {
            for (int dx = 0; dx <= c; ++dx) {
                const float a = f.at(c + dx, c + dy);
                if (!near(a, f.at(c - dx, c + dy)) || !near(a, f.at(c + dx, c - dy)) ||
                    !near(a, f.at(c - dx, c - dy)) || !near(a, f.at(c + dy, c + dx))) {
                    sym = false;
                }
            }
        }
        CHECK(sym, "a centred drop stays 4-fold (and diagonally) symmetric");
    }

    // --- 3. Outward propagation. ---
    {
        const int n = 21;
        WaveField2D f(n, n, 0.0f);
        const int c = n / 2;
        f.disturb(c, c, 1.0f);
        CHECK(near(f.at(c + 1, c), 0.0f), "before stepping, the neighbour is still at rest");
        f.step();
        CHECK(!near(f.at(c + 1, c), 0.0f), "after one step the ripple has reached the neighbouring cell");
    }

    // --- 4. Damping decays the total amplitude. ---
    {
        const int n = 25;
        WaveField2D f(n, n, 0.05f); // noticeable damping
        f.disturb(n / 2, n / 2, 5.0f);
        for (int s = 0; s < 10; ++s) f.step();
        const float early = f.totalAmplitude();
        for (int s = 0; s < 400; ++s) f.step();
        const float late = f.totalAmplitude();
        CHECK(late < early, "damping reduces the total wave amplitude over time");
        CHECK(late < 0.5f, "with damping the surface settles back toward rest");
    }

    // --- 5. Border held at rest. ---
    {
        const int n = 15;
        WaveField2D f(n, n, 0.0f);
        f.disturb(n / 2, n / 2, 3.0f);
        for (int s = 0; s < 30; ++s) f.step();
        bool borderRest = true;
        for (int x = 0; x < n; ++x) {
            if (!near(f.at(x, 0), 0.0f) || !near(f.at(x, n - 1), 0.0f)) borderRest = false;
        }
        for (int y = 0; y < n; ++y) {
            if (!near(f.at(0, y), 0.0f) || !near(f.at(n - 1, y), 0.0f)) borderRest = false;
        }
        CHECK(borderRest, "the border stays at rest (fixed shore)");
    }

    if (g_fail == 0) {
        std::printf("wavefield2d: OK — still flat, centred symmetry, propagation, damping decay, "
                    "fixed border.\n");
        return 0;
    }
    std::printf("wavefield2d: %d failure(s).\n", g_fail);
    return 1;
}
