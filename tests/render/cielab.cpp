// tests/render/cielab.cpp — verifies CIELAB colour space + Delta-E (render CieLab.hpp).
// Ground truths, deterministic:
//   * reference white (linear RGB 1,1,1) maps to L*=100, a*=b*=0;
//   * black maps to L*=0;
//   * Color -> Lab -> Color round-trips for a spread of colours;
//   * L* is monotonic in luminance (mid grey sits between black and white);
//   * a* is +red / -green, b* is +yellow / -blue (channel signs);
//   * deltaE76 is symmetric and zero for identical colours;
//   * deltaE2000 matches the published Sharma-Wu-Dalal reference pairs (the standard test vectors).
#include "maz/render/CieLab.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::render::Color;
using maz::render::deltaE2000;
using maz::render::deltaE76;
using maz::render::fromLab;
using maz::render::Lab;
using maz::render::toLab;

static bool near(float a, float b, float e = 1e-3f) { return std::fabs(a - b) < e; }

int main() {
    // --- 1. Reference white and black. ---
    {
        const Lab w = toLab(Color{1, 1, 1, 1});
        CHECK(near(w.L, 100.0f, 1e-2f), "white -> L*=100");
        CHECK(near(w.a, 0.0f, 1e-2f) && near(w.b, 0.0f, 1e-2f), "white is neutral (a*=b*=0)");
        const Lab k = toLab(Color{0, 0, 0, 1});
        CHECK(near(k.L, 0.0f, 1e-2f), "black -> L*=0");
    }

    // --- 2. Round trip Color -> Lab -> Color. ---
    {
        const Color samples[] = {
            {0.8f, 0.2f, 0.3f, 1}, {0.1f, 0.6f, 0.9f, 0.5f}, {0.5f, 0.5f, 0.5f, 1},
            {0.05f, 0.9f, 0.2f, 1}, {0.02f, 0.02f, 0.02f, 1},
        };
        for (const Color& c : samples) {
            const Color r = fromLab(toLab(c), c.a);
            CHECK(near(r.r, c.r) && near(r.g, c.g) && near(r.b, c.b), "Lab round trip preserves RGB");
            CHECK(near(r.a, c.a), "Lab round trip preserves alpha");
        }
    }

    // --- 3. L* monotonic in luminance. ---
    {
        const float lo = toLab(Color{0.1f, 0.1f, 0.1f, 1}).L;
        const float mid = toLab(Color{0.5f, 0.5f, 0.5f, 1}).L;
        const float hi = toLab(Color{0.9f, 0.9f, 0.9f, 1}).L;
        CHECK(lo < mid && mid < hi, "L* increases with lightness");
    }

    // --- 4. Channel signs: a* red/green, b* yellow/blue. ---
    {
        CHECK(toLab(Color{0.8f, 0.1f, 0.1f, 1}).a > 0.0f, "red is +a*");
        CHECK(toLab(Color{0.1f, 0.8f, 0.1f, 1}).a < 0.0f, "green is -a*");
        CHECK(toLab(Color{0.8f, 0.8f, 0.1f, 1}).b > 0.0f, "yellow is +b*");
        CHECK(toLab(Color{0.1f, 0.1f, 0.8f, 1}).b < 0.0f, "blue is -b*");
    }

    // --- 5. deltaE76 basics. ---
    {
        const Lab p = toLab(Color{0.8f, 0.2f, 0.3f, 1});
        const Lab q = toLab(Color{0.1f, 0.6f, 0.9f, 1});
        CHECK(near(deltaE76(p, p), 0.0f), "deltaE76 of a colour with itself is 0");
        CHECK(near(deltaE76(p, q), deltaE76(q, p)), "deltaE76 is symmetric");
        CHECK(deltaE76(p, q) > 0.0f, "distinct colours have positive deltaE76");
    }

    // --- 6. CIEDE2000 vs the published reference pairs (Sharma, Wu, Dalal 2005, Table 1). ---
    {
        struct Pair { Lab a, b; float expected; };
        const Pair refs[] = {
            {{50.0000f, 2.6772f, -79.7751f}, {50.0000f, 0.0000f, -82.7485f}, 2.0425f},
            {{50.0000f, 3.1571f, -77.2803f}, {50.0000f, 0.0000f, -82.7485f}, 2.8615f},
            {{50.0000f, 2.8361f, -74.0200f}, {50.0000f, 0.0000f, -82.7485f}, 3.4412f},
            {{50.0000f, -1.3802f, -84.2814f}, {50.0000f, 0.0000f, -82.7485f}, 1.0000f},
            {{50.0000f, -1.1848f, -84.8006f}, {50.0000f, 0.0000f, -82.7485f}, 1.0000f},
            {{50.0000f, 0.0000f, 0.0000f}, {50.0000f, -1.0000f, 2.0000f}, 2.3669f},
            {{50.0000f, 2.5000f, 0.0000f}, {73.0000f, 25.0000f, -18.0000f}, 27.1492f},
            {{50.0000f, 2.5000f, 0.0000f}, {50.0000f, 3.1736f, 0.5854f}, 1.0000f},
            {{50.0000f, 2.5000f, 0.0000f}, {50.0000f, 3.2972f, 0.0000f}, 1.0000f},
            {{60.2574f, -34.0099f, 36.2677f}, {60.4626f, -34.1751f, 39.4387f}, 1.2644f},
            {{63.0109f, -31.0961f, -5.8663f}, {62.8187f, -29.7946f, -4.0864f}, 1.2630f},
            {{22.7233f, 20.0904f, -46.6940f}, {23.0331f, 14.9730f, -42.5619f}, 2.0373f},
            {{2.0776f, 0.0795f, -1.1350f}, {0.9033f, -0.0636f, -0.5514f}, 0.9082f},
        };
        for (const Pair& r : refs) {
            const float got = deltaE2000(r.a, r.b);
            if (!near(got, r.expected, 1e-3f)) {
                std::printf("  CIEDE2000 mismatch: got %.4f expected %.4f\n",
                            static_cast<double>(got), static_cast<double>(r.expected));
                ++g_fail;
            }
        }
        // Symmetry and identity.
        CHECK(near(deltaE2000(refs[0].a, refs[0].a), 0.0f), "CIEDE2000 of a colour with itself is 0");
        CHECK(near(deltaE2000(refs[6].a, refs[6].b), deltaE2000(refs[6].b, refs[6].a), 1e-3f),
              "CIEDE2000 is symmetric");
    }

    if (g_fail == 0) {
        std::printf("cielab: OK — white/black, round trip, L* monotonic, channel signs, deltaE76, "
                    "CIEDE2000 reference pairs.\n");
        return 0;
    }
    std::printf("cielab: %d failure(s).\n", g_fail);
    return 1;
}
