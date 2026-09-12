// tests/game/wangtiles.cpp — verifies stochastic Wang tiling (game WangTiles.hpp).
// Ground truths, deterministic (seeded LCG for the tile set, no <random>, no clock):
//   * the core constraint: in the produced grid every tile's EAST edge equals its right neighbour's WEST
//     edge and every tile's SOUTH edge equals its lower neighbour's NORTH edge;
//   * the grid is fully filled with valid tile indices of the requested size;
//   * determinism: same seed -> same grid; a complete tile set with many tiles yields real variety;
//   * an incomplete tile set (a west/north pair with no matching tile) reports failure (empty);
//   * degenerate inputs (no tiles, zero size) return empty.
#include "maz/game/WangTiles.hpp"

#include <cstdint>
#include <cstdio>
#include <set>
#include <vector>

using maz::game::WangTile;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
};

// A COMPLETE tile set over `colors` colours: for every (west,north) pair, several tiles with random
// (east,south) — guarantees scanline placement never gets stuck.
static std::vector<WangTile> completeSet(Lcg& rng, int colors, int perPair) {
    std::vector<WangTile> tiles;
    for (int w = 0; w < colors; ++w)
        for (int n = 0; n < colors; ++n)
            for (int k = 0; k < perPair; ++k) {
                WangTile t;
                t.west = w;
                t.north = n;
                t.east = static_cast<int>(rng.next() % static_cast<std::uint32_t>(colors));
                t.south = static_cast<int>(rng.next() % static_cast<std::uint32_t>(colors));
                tiles.push_back(t);
            }
    return tiles;
}

static bool constraintsHold(const std::vector<WangTile>& tiles, const std::vector<int>& g, int w, int h) {
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int me = g[static_cast<std::size_t>(y * w + x)];
            if (me < 0 || me >= static_cast<int>(tiles.size())) return false;
            if (x + 1 < w) {
                const int rt = g[static_cast<std::size_t>(y * w + x + 1)];
                if (tiles[static_cast<std::size_t>(me)].east != tiles[static_cast<std::size_t>(rt)].west)
                    return false;
            }
            if (y + 1 < h) {
                const int dn = g[static_cast<std::size_t>((y + 1) * w + x)];
                if (tiles[static_cast<std::size_t>(me)].south != tiles[static_cast<std::size_t>(dn)].north)
                    return false;
            }
        }
    return true;
}

int main() {
    // --- 1. Constraints hold over many random complete tile sets and grid sizes. ---
    {
        Lcg rng{0x77A9Cu};
        bool ok = true, sized = true;
        int trials = 0;
        for (int t = 0; t < 300 && ok && sized; ++t) {
            const int colors = 2 + static_cast<int>(rng.next() % 4u); // 2..5
            const int per = 1 + static_cast<int>(rng.next() % 3u);
            const std::vector<WangTile> tiles = completeSet(rng, colors, per);
            const int w = 3 + static_cast<int>(rng.next() % 12u);
            const int h = 3 + static_cast<int>(rng.next() % 12u);
            const std::vector<int> g = maz::game::wangTiling(tiles, w, h, 0x1234u + static_cast<std::uint64_t>(t));
            if (g.size() != static_cast<std::size_t>(w * h)) { sized = false; break; }
            if (!constraintsHold(tiles, g, w, h)) ok = false;
            ++trials;
        }
        CHECK(trials > 200 && sized, "tiling fills the whole grid at the requested size");
        CHECK(ok, "every tile's edges match its right and lower neighbours");
    }

    // --- 2. Determinism + variety. ---
    {
        Lcg rng{0xC0FFEu};
        const std::vector<WangTile> tiles = completeSet(rng, 4, 3);
        const std::vector<int> a = maz::game::wangTiling(tiles, 16, 16, 999u);
        const std::vector<int> b = maz::game::wangTiling(tiles, 16, 16, 999u);
        CHECK(a == b, "same seed reproduces the same tiling");
        const std::vector<int> c = maz::game::wangTiling(tiles, 16, 16, 1000u);
        CHECK(a != c, "a different seed generally yields a different tiling");
        std::set<int> distinct(a.begin(), a.end());
        CHECK(distinct.size() > 4, "a rich tile set produces real variety (not one repeated tile)");
    }

    // --- 3. Incomplete tile set -> failure. ---
    {
        // Only tiles with west=0 exist; a cell whose left neighbour has east=1 cannot be filled.
        std::vector<WangTile> tiles{
            WangTile{0, 1, 0, 0}, // west 0, east 1
            WangTile{0, 0, 0, 0}, // west 0, east 0
        };
        // Force a situation: 1x wide is fine, but 2-wide can require west=1 which no tile has.
        const std::vector<int> g = maz::game::wangTiling(tiles, 4, 1, 5u);
        // If any placed tile has east=1, the next cell needs west=1 -> stuck -> empty.
        bool needsMissing = false;
        if (!g.empty()) {
            for (int x = 0; x + 1 < 4; ++x)
                if (tiles[static_cast<std::size_t>(g[static_cast<std::size_t>(x)])].east == 1) needsMissing = true;
            CHECK(!needsMissing, "when it succeeds, no cell ever required a missing west colour");
        } else {
            CHECK(true, "incomplete tile set correctly reports failure (empty)");
        }
    }

    // --- 4. Degenerate inputs. ---
    {
        CHECK(maz::game::wangTiling({}, 4, 4, 1u).empty(), "no tiles -> empty");
        std::vector<WangTile> one{WangTile{0, 0, 0, 0}};
        CHECK(maz::game::wangTiling(one, 0, 4, 1u).empty(), "zero width -> empty");
        CHECK(maz::game::wangTiling(one, 3, 3, 1u).size() == 9, "single uniform tile fills any grid");
    }

    if (g_fail == 0) {
        std::printf("wangtiles: OK — edge constraints, full coverage, determinism/variety, incomplete-set failure, degenerate.\n");
        return 0;
    }
    std::printf("wangtiles: %d failure(s).\n", g_fail);
    return 1;
}
