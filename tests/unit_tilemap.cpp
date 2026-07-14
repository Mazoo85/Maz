// Unit tests for maz::scene::TileMap — the 2D multi-layer tile grid. Exercises
// construction + dimensions + all-empty state, set/at roundtrip, inBounds + safe
// out-of-bounds reads, per-layer independence, whole-layer fill, inclusive+clamped
// (and reversed-coord-normalizing) fillRegion, clear across all layers, and the flat
// index math via the four corners on multiple layers. Pure C++, no GPU/display.

#include "maz/scene/TileMap.hpp"

#include <cstdio>

using namespace maz::scene;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

} // namespace

int main() {
    // --- CONSTRUCT + DIMS + ALL-EMPTY ----------------------------------------
    {
        TileMap m(4, 3, 2);
        check(m.width() == 4, "width() == 4");
        check(m.height() == 3, "height() == 3");
        check(m.layerCount() == 2, "layerCount() == 2");
        check(m.at(0, 0) == 0, "fresh cell reads empty (0)");
        check(m.countNonEmpty(0) == 0, "fresh layer 0 has no non-empty cells");
        check(m.empty(0) == true, "fresh layer 0 is empty");
    }

    // --- SET / AT ROUNDTRIP ---------------------------------------------------
    {
        TileMap m(4, 3, 2);
        m.set(1, 2, 42);
        check(m.at(1, 2) == 42, "set(1,2,42) then at(1,2) == 42");
        check(m.at(0, 0) == 0, "other cells unaffected by set");
        check(m.countNonEmpty(0) == 1, "countNonEmpty(0) == 1 after one set");
        check(m.empty(0) == false, "layer 0 no longer empty after a set");
    }

    // --- INBOUNDS + OOB READ --------------------------------------------------
    {
        TileMap m(4, 3, 1);
        check(m.inBounds(0, 0) == true, "inBounds(0,0) true");
        check(m.inBounds(4, 0) == false, "inBounds(4,0) false (x == width)");
        check(m.inBounds(-1, 0) == false, "inBounds(-1,0) false (negative x)");
        check(m.inBounds(0, 3) == false, "inBounds(0,3) false (y == height)");
        check(m.at(-1, 0) == 0, "OOB read at(-1,0) returns empty");
        check(m.at(4, 0) == 0, "OOB read at(4,0) returns empty");
        check(m.at(100, 100) == 0, "far-OOB read at(100,100) returns empty, no crash");
    }

    // --- LAYER INDEPENDENCE ---------------------------------------------------
    {
        TileMap m(4, 3, 2);
        m.set(1, 1, 7, 0);
        m.set(1, 1, 9, 1);
        check(m.at(1, 1, 0) == 7, "layer 0 holds 7 at (1,1)");
        check(m.at(1, 1, 1) == 9, "layer 1 holds 9 at (1,1)");
        check(m.countNonEmpty(0) == 1, "layer 0 has exactly one non-empty cell");
        check(m.countNonEmpty(1) == 1, "layer 1 has exactly one non-empty cell");
    }

    // --- FILL -----------------------------------------------------------------
    {
        TileMap m(4, 3, 2);
        m.fill(5, 0);
        check(m.countNonEmpty(0) == 12, "fill(5,0) sets all 4*3 == 12 cells");
        check(m.at(0, 0, 0) == 5 && m.at(3, 2, 0) == 5 && m.at(2, 1, 0) == 5,
              "every sampled in-bounds cell of layer 0 == 5");
        check(m.countNonEmpty(1) == 0, "fill on layer 0 leaves layer 1 untouched");
    }

    // --- FILLREGION INCLUSIVE + CLAMP -----------------------------------------
    {
        TileMap m(5, 5, 1);
        m.fillRegion(1, 1, 2, 3, 8, 0);  // x in [1,2], y in [1,3] -> 2*3 == 6 cells
        check(m.countNonEmpty(0) == 6, "fillRegion(1,1,2,3) fills 2*3 == 6 cells (inclusive)");
        check(m.at(1, 1) == 8, "fillRegion corner (1,1) set to 8");
        check(m.at(2, 3) == 8, "fillRegion corner (2,3) set to 8");
        check(m.at(0, 0) == 0, "cell (0,0) outside region stays empty");
        check(m.at(3, 3) == 0, "cell (3,3) outside region stays empty");

        // Partly-off-map region clamps to x[3,4], y[3,4] -> 4 new cells.
        m.fillRegion(3, 3, 100, 100, 4, 0);
        check(m.countNonEmpty(0) == 10, "partly-off-map fillRegion adds 4 clamped cells (6+4)");
        check(m.at(3, 3) == 4 && m.at(4, 4) == 4 && m.at(4, 3) == 4 && m.at(3, 4) == 4,
              "clamped region corners set, no crash");

        // Reversed coords normalize -> fills x[0,2], y[0,2].
        TileMap n(5, 5, 1);
        n.fillRegion(2, 2, 0, 0, 1, 0);
        check(n.countNonEmpty(0) == 9, "reversed-coord fillRegion normalizes to 3*3 == 9 cells");
        check(n.at(0, 0) == 1 && n.at(2, 2) == 1 && n.at(1, 1) == 1,
              "normalized region covers x[0,2] y[0,2]");
    }

    // --- CLEAR ----------------------------------------------------------------
    {
        TileMap m(4, 3, 2);
        m.fill(5, 0);
        m.set(2, 2, 9, 1);
        m.fillRegion(0, 0, 1, 1, 3, 1);
        m.clear();
        check(m.countNonEmpty(0) == 0, "clear() empties layer 0");
        check(m.countNonEmpty(1) == 0, "clear() empties layer 1");
        check(m.at(0, 0, 0) == 0 && m.at(3, 2, 0) == 0 && m.at(2, 2, 1) == 0,
              "all sampled cells read empty after clear");
        check(m.empty(0) && m.empty(1), "both layers empty after clear");
    }

    // --- INDEX CORNERS --------------------------------------------------------
    {
        TileMap m(4, 3, 2);
        const int w = m.width();
        const int h = m.height();
        m.set(0, 0, 11, 0);
        m.set(w - 1, 0, 22, 0);
        m.set(0, h - 1, 33, 0);
        m.set(w - 1, h - 1, 44, 0);
        m.set(w - 1, h - 1, 55, 1);  // same corner, different layer
        check(m.at(0, 0, 0) == 11, "corner (0,0) layer 0 == 11");
        check(m.at(w - 1, 0, 0) == 22, "corner (w-1,0) layer 0 == 22");
        check(m.at(0, h - 1, 0) == 33, "corner (0,h-1) layer 0 == 33");
        check(m.at(w - 1, h - 1, 0) == 44, "corner (w-1,h-1) layer 0 == 44");
        check(m.at(w - 1, h - 1, 1) == 55, "corner (w-1,h-1) layer 1 == 55 (no aliasing)");
        check(m.at(0, 0, 1) == 0, "corner (0,0) layer 1 still empty (no cross-layer aliasing)");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
