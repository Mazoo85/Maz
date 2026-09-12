// tests/game/flowaccumulation.cpp — verifies D8 flow accumulation (game FlowAccumulation.hpp).
// Ground truths, deterministic (no <random>, no clock — seeded LCG for terrain):
//   * every cell accumulates at least its own unit (>= 1);
//   * flow is monotone: a cell's downstream receiver always carries at least as much as the cell;
//   * CONSERVATION: the accumulations at all outlet cells (no downstream) sum to the cell count — every unit
//     of water reaches an outlet, none created or lost;
//   * on a tilted plane the analytic answer holds: cell (x,y) accumulates x+1, and the low edge carries the
//     whole row;
//   * determinism.
#include "maz/game/FlowAccumulation.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

using maz::game::flowAccumulation;
using maz::game::FlowResult;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
    float unit() { return static_cast<float>(next()) / 4294967296.0f; }
};

int main() {
    // --- 1. Random terrain: >=1, monotone, conservation. ---
    {
        const int w = 48, h = 48;
        const std::size_t n = static_cast<std::size_t>(w * h);
        std::vector<float> hgt(n);
        Lcg rng{0xF10Eu};
        for (auto& v : hgt) v = rng.unit() * 100.0f;

        const FlowResult r = flowAccumulation(hgt, w, h);
        CHECK(r.accum.size() == n && r.downstream.size() == n, "result sized to the grid");

        bool geOne = true, monotone = true;
        std::uint64_t outletSum = 0;
        for (std::size_t c = 0; c < n; ++c) {
            if (r.accum[c] < 1u) geOne = false;
            const int d = r.downstream[c];
            if (d >= 0) {
                if (r.accum[static_cast<std::size_t>(d)] < r.accum[c]) monotone = false;
            } else {
                outletSum += r.accum[c];
            }
        }
        CHECK(geOne, "every cell accumulates at least 1");
        CHECK(monotone, "downstream receiver carries at least as much as the cell");
        CHECK(outletSum == n, "outlet accumulations sum to the cell count (water conserved)");
    }

    // --- 2. Tilted plane: analytic accumulation. ---
    {
        const int w = 10, h = 6;
        std::vector<float> hgt(static_cast<std::size_t>(w * h));
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                hgt[static_cast<std::size_t>(y * w + x)] = static_cast<float>(w - x); // drops to the east

        const FlowResult r = flowAccumulation(hgt, w, h);
        bool ok = true;
        for (int y = 0; y < h && ok; ++y)
            for (int x = 0; x < w; ++x)
                if (r.accum[static_cast<std::size_t>(y * w + x)] != static_cast<std::uint32_t>(x + 1)) {
                    ok = false; break;
                }
        CHECK(ok, "on an east-facing slope, cell (x,y) accumulates x+1");
        // The low (east) edge cells are outlets carrying their whole row.
        bool edge = true;
        for (int y = 0; y < h; ++y)
            if (r.accum[static_cast<std::size_t>(y * w + (w - 1))] != static_cast<std::uint32_t>(w)) edge = false;
        CHECK(edge, "the low edge carries the entire row (w)");
    }

    // --- 3. Determinism. ---
    {
        const int w = 20, h = 20;
        std::vector<float> hgt(static_cast<std::size_t>(w * h));
        Lcg rng{0x5EED17u};
        for (auto& v : hgt) v = rng.unit() * 30.0f;
        const FlowResult a = flowAccumulation(hgt, w, h);
        const FlowResult b = flowAccumulation(hgt, w, h);
        CHECK(a.accum == b.accum && a.downstream == b.downstream, "flow accumulation is deterministic");
    }

    if (g_fail == 0) {
        std::printf("flowaccumulation: OK — >=1, monotone, conservation, tilted-plane analytic, determinism.\n");
        return 0;
    }
    std::printf("flowaccumulation: %d failure(s).\n", g_fail);
    return 1;
}
