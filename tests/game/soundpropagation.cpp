// tests/game/soundpropagation.cpp — verifies weighted, obstacle-aware sound propagation
// (game::propagateSound / SoundField). Ground truths: the source hears the full loudness; on a uniform
// open grid the heard loudness falls off by Manhattan distance (4-connected) times the per-tile
// attenuation, matching a hand-computed field; a wall of high attenuation forces sound to ROUTE AROUND it
// so a cell behind the wall is quieter than the straight-line-through-wall value would predict (the
// discriminating case a naive radial-falloff model fails); cells that drop to/under the audible floor are
// silent; diagonals cost sqrt(2); results are deterministic; degenerate inputs are safe. Pure CPU.
#include "maz/game/SoundPropagation.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::game;

static bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

int main() {
    // --- 1. Source hears the full loudness; open uniform grid falls off by Manhattan distance. ---
    {
        // attenuation 1 per open tile, 4-connected, floor 0. Loudness at (x,y) = 100 - manhattan(src).
        auto attn = [](const SoundCell&) { return 1.0f; };
        const SoundField f = propagateSound(9, 9, SoundCell(4, 4), 100.0f, attn, false, 0.0f);
        CHECK(f.width == 9 && f.height == 9, "field dims");
        CHECK(near(f.at(4, 4), 100.0f), "source hears full loudness");
        CHECK(near(f.at(5, 4), 99.0f), "one step east = 99");
        CHECK(near(f.at(4, 6), 98.0f), "two steps south = 98");
        // A diagonal cell must be reached by two orthogonal steps (4-connected) => 100 - 2.
        CHECK(near(f.at(5, 5), 98.0f), "diagonal via two steps = 98 (4-connected)");
        // Far corner: manhattan (4,4)->(0,0) = 8 => 92.
        CHECK(near(f.at(0, 0), 92.0f), "corner = 92");
    }

    // --- 2. Audible floor silences distant cells and everything past them. ---
    {
        auto attn = [](const SoundCell&) { return 1.0f; };
        // Loudness 5, floor 0: cells at manhattan distance >= 5 are inaudible.
        const SoundField f = propagateSound(20, 1, SoundCell(0, 0), 5.0f, attn, false, 0.0f);
        CHECK(near(f.at(0, 0), 5.0f), "src = 5");
        CHECK(near(f.at(4, 0), 1.0f), "dist 4 = 1");
        CHECK(f.at(5, 0) == 0.0f && !f.audible(5, 0), "dist 5 = inaudible (floor)");
        CHECK(f.at(9, 0) == 0.0f, "beyond floor stays silent");
    }

    // --- 3. Discriminating case: sound routes AROUND a wall (a naive radial model would fail). ---
    {
        // 7x3 grid. A full-height... actually a wall column at x=3 for y=0 and y=1, leaving a gap at y=2.
        // Source at (0,1). Target at (6,1) sits directly across the wall from the source.
        // Straight-line Manhattan (ignoring the wall) would be 6 => 100-6 = 94.
        // But the wall tile (x=3) has attenuation 1000, so the cheap path detours down to y=2, across, back
        // up: (0,1)->...(3,2)... The go-around adds vertical steps, so the heard loudness is < 94 and, with
        // a wall this loud, the through-wall path is never taken.
        const float wallAtt = 1000.0f;
        auto attn = [&](const SoundCell& c) -> float {
            if (c.x == 3 && (c.y == 0 || c.y == 1)) return wallAtt; // wall with a gap at y=2
            return 1.0f;
        };
        const SoundField f = propagateSound(7, 3, SoundCell(0, 1), 100.0f, attn, false, 0.0f);

        // The wall tiles themselves are (nearly) silent — sound barely penetrates them.
        CHECK(f.at(3, 1) < 1.0f, "wall tile is muffled");

        // Cheapest path to (6,1): (0,1)->(1,1)->(2,1)->(2,2)->(3,2)->(4,2)->(4,1)->(5,1)->(6,1) = 8 steps.
        // Heard = 100 - 8 = 92, strictly less than the 94 a wall-ignorant straight line would give.
        CHECK(near(f.at(6, 1), 92.0f), "target reached via go-around path = 92");
        CHECK(f.at(6, 1) < 94.0f, "go-around is quieter than through-wall straight line");

        // The gap tile is the through-route and is louder than the walled cell above it.
        CHECK(f.at(3, 2) > f.at(3, 1), "gap tile louder than sealed wall tile");
    }

    // --- 4. Diagonals cost sqrt(2) when enabled and open a cheaper path than orthogonal. ---
    {
        auto attn = [](const SoundCell&) { return 1.0f; };
        const SoundField f = propagateSound(5, 5, SoundCell(0, 0), 100.0f, attn, true, 0.0f);
        // (1,1) via one diagonal step: cost sqrt(2) => 100 - sqrt(2).
        CHECK(near(f.at(1, 1), 100.0f - 1.41421356237309515f), "diagonal step cost sqrt(2)");
        // Diagonal beats two orthogonals (which would be 98): sqrt(2) < 2.
        CHECK(f.at(1, 1) > 98.0f, "diagonal cheaper than two orthogonal steps");
    }

    // --- 5. Determinism: same inputs -> identical field. ---
    {
        auto attn = [](const SoundCell& c) { return (c.x == 2) ? 3.0f : 1.0f; };
        const SoundField a = propagateSound(8, 8, SoundCell(1, 1), 50.0f, attn, true, 0.0f);
        const SoundField b = propagateSound(8, 8, SoundCell(1, 1), 50.0f, attn, true, 0.0f);
        bool same = a.loudness.size() == b.loudness.size();
        for (std::size_t i = 0; same && i < a.loudness.size(); ++i) {
            same = (a.loudness[i] == b.loudness[i]);
        }
        CHECK(same, "deterministic");
    }

    // --- 6. Degenerate inputs are safe. ---
    {
        auto attn = [](const SoundCell&) { return 1.0f; };
        CHECK(propagateSound(0, 5, SoundCell(0, 0), 10.0f, attn).loudness.empty(), "zero width -> empty");
        const SoundField oob = propagateSound(5, 5, SoundCell(9, 9), 10.0f, attn);
        bool allZero = true;
        for (float v : oob.loudness) allZero = allZero && (v == 0.0f);
        CHECK(allZero, "source out of bounds -> silent field");
        const SoundField quiet = propagateSound(5, 5, SoundCell(2, 2), 0.0f, attn);
        bool q = true;
        for (float v : quiet.loudness) q = q && (v == 0.0f);
        CHECK(q, "zero source loudness -> silent field");
    }

    if (g_fail == 0) {
        std::printf("sound propagation: all tests passed\n");
    }
    return g_fail == 0 ? 0 : 1;
}
