// tests/game/hexringrange.cpp — verifies hex ring / range / spiral (game::hexRing / hexRange / hexSpiral).
// Ground truths: a ring of radius N holds exactly 6N hexes all at distance N (radius 0 is the centre alone);
// ring 1 is precisely the six neighbours; a range of radius N holds 1+3N(N+1) distinct hexes all within N and
// includes the centre; a spiral is the same SET as the range but ordered centre-first with non-decreasing
// distance and starts at the centre; everything works around a non-origin centre; negative radii are empty.
// Checked against the axial hex-coordinate definitions. Pure integer math, deterministic.
#include "maz/game/HexGrid.hpp"

#include <algorithm>
#include <cstdio>
#include <set>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::game;

struct HexLess {
    bool operator()(const Hex& a, const Hex& b) const { return a.q != b.q ? a.q < b.q : a.r < b.r; }
};

int main() {
    const Hex origin(0, 0);
    const Hex off(3, -2); // a non-origin centre

    // --- 1. Ring sizes, distances, and the radius-0/negative edge cases. ---
    {
        CHECK(hexRing(origin, 0).size() == 1 && hexRing(origin, 0)[0] == origin, "ring 0 is the centre alone");
        CHECK(hexRing(origin, -1).empty(), "negative ring radius is empty");
        for (int n = 1; n <= 5; ++n) {
            const auto ring = hexRing(off, n);
            CHECK(static_cast<int>(ring.size()) == 6 * n, "ring N has exactly 6N hexes");
            bool allAtN = true;
            for (const Hex& h : ring)
                if (hexDistance(h, off) != n) allAtN = false;
            CHECK(allAtN, "every ring hex is at exactly distance N");
        }
    }

    // --- 2. Ring 1 is exactly the six neighbours. ---
    {
        const auto ring = hexRing(off, 1);
        const auto nbrs = hexNeighbors(off);
        std::set<Hex, HexLess> a(ring.begin(), ring.end());
        std::set<Hex, HexLess> b(nbrs.begin(), nbrs.end());
        CHECK(a == b, "ring 1 equals the neighbour set");
    }

    // --- 3. Range: count 1+3N(N+1), all within N, distinct, includes the centre. ---
    {
        for (int n = 0; n <= 5; ++n) {
            const auto rng = hexRange(off, n);
            const int expect = 1 + 3 * n * (n + 1);
            CHECK(static_cast<int>(rng.size()) == expect, "range N has 1+3N(N+1) hexes");
            std::set<Hex, HexLess> uniq(rng.begin(), rng.end());
            CHECK(static_cast<int>(uniq.size()) == expect, "range hexes are distinct");
            bool allWithin = true, hasCenter = false;
            for (const Hex& h : rng) {
                if (hexDistance(h, off) > n) allWithin = false;
                if (h == off) hasCenter = true;
            }
            CHECK(allWithin, "every range hex is within N");
            CHECK(hasCenter, "range includes the centre");
        }
        CHECK(hexRange(origin, -1).empty(), "negative range is empty");
    }

    // --- 4. Spiral: same set as range, centre-first, non-decreasing distance. ---
    {
        const int n = 4;
        const auto spiral = hexSpiral(off, n);
        const auto rng = hexRange(off, n);
        CHECK(spiral.size() == rng.size(), "spiral and range have the same count");
        std::set<Hex, HexLess> a(spiral.begin(), spiral.end());
        std::set<Hex, HexLess> b(rng.begin(), rng.end());
        CHECK(a == b, "spiral and range cover the same set");
        CHECK(!spiral.empty() && spiral[0] == off, "spiral starts at the centre");
        bool nonDecreasing = true;
        for (std::size_t i = 1; i < spiral.size(); ++i)
            if (hexDistance(spiral[i], off) < hexDistance(spiral[i - 1], off)) nonDecreasing = false;
        CHECK(nonDecreasing, "spiral distance is non-decreasing (ring by ring)");
    }

    if (g_fail == 0) {
        std::printf("hexringrange: OK — ring 6N at distance N, ring1==neighbours, range count/within, spiral order.\n");
        return 0;
    }
    std::printf("hexringrange: %d failure(s).\n", g_fail);
    return 1;
}
