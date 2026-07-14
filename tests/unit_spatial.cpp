// Unit tests for maz::spatial::SpatialHashGrid — the sparse uniform spatial hash
// grid broadphase. Exercises the empty grid, insert/query broad+narrow phase,
// narrow-phase rejection of same-cell non-overlap, multi-cell dedup, negative
// coordinates (std::floor cell gate), remove/update, overlapping sets, clear, and
// a lattice consistency sweep. INTEGER/BOOL asserts only — no float compares.

#include "maz/spatial/SpatialHashGrid.hpp"

#include <cstdio>
#include <vector>
#include <algorithm>

using maz::spatial::SpatialHashGrid;
using maz::math::Aabb;
using maz::math::vec3;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    if (!cond) {
        ++g_failures;
        std::printf("[FAIL] %s\n", msg);
    } else {
        std::printf("[PASS] %s\n", msg);
    }
}

bool sameSet(std::vector<std::uint32_t> got, std::vector<std::uint32_t> expected) {
    std::sort(got.begin(), got.end());
    std::sort(expected.begin(), expected.end());
    return got == expected;
}

} // namespace

int main() {
    // --- 1: EMPTY GRID -------------------------------------------------------
    {
        SpatialHashGrid g(1.0f);
        check(g.size() == 0, "empty: size() == 0");
        check(g.cellCount() == 0, "empty: cellCount() == 0");
        check(g.queryRegion(Aabb{vec3(0, 0, 0), vec3(10, 10, 10)}).empty(), "empty: queryRegion empty");
        check(g.queryPoint(vec3(0, 0, 0)).empty(), "empty: queryPoint empty");
    }

    // --- 2: SINGLE INSERT + QUERY --------------------------------------------
    {
        SpatialHashGrid g(1.0f);
        g.insert(7, Aabb{vec3(0, 0, 0), vec3(1, 1, 1)});
        check(g.size() == 1, "single: size() == 1");
        check(sameSet(g.queryRegion(Aabb{vec3(0, 0, 0), vec3(2, 2, 2)}), {7}), "single: overlapping region -> {7}");
        check(g.queryRegion(Aabb{vec3(50, 50, 50), vec3(60, 60, 60)}).empty(), "single: far region -> {}");
        check(sameSet(g.queryPoint(vec3(0, 0, 0)), {7}), "single: queryPoint inside -> {7}");
        check(g.queryPoint(vec3(5, 5, 5)).empty(), "single: queryPoint outside -> {}");
    }

    // --- 3: NARROW-PHASE REJECTS SAME-CELL NON-OVERLAP -----------------------
    {
        SpatialHashGrid g(10.0f);  // both small boxes land in cell 0
        g.insert(1, Aabb{vec3(0, 0, 0), vec3(1, 1, 1)});
        g.insert(2, Aabb{vec3(5, 5, 5), vec3(6, 6, 6)});
        check(g.cellCount() == 1, "narrow: both items share one cell");
        check(sameSet(g.queryRegion(Aabb{vec3(0, 0, 0), vec3(2, 2, 2)}), {1}), "narrow: region overlaps only box A -> {1}");
    }

    // --- 4: MULTI-CELL ITEM + DEDUP ------------------------------------------
    {
        SpatialHashGrid g(1.0f);
        g.insert(3, Aabb{vec3(0, 0, 0), vec3(3, 3, 3)});  // spans cells 0..3 per axis
        check(g.cellCount() == 64, "multi-cell: item occupies 4x4x4 cells");
        std::vector<std::uint32_t> res = g.queryRegion(Aabb{vec3(0, 0, 0), vec3(3, 3, 3)});
        check(res.size() == 1, "multi-cell: multi-cell item returned exactly once");
        check(sameSet(res, {3}), "multi-cell: the returned id is 3");
    }

    // --- 5: NEGATIVE COORDINATES ---------------------------------------------
    {
        SpatialHashGrid g(1.0f);
        g.insert(11, Aabb{vec3(-3, -3, -3), vec3(-1, -1, -1)});
        check(sameSet(g.queryPoint(vec3(-2, -2, -2)), {11}), "negative: queryPoint(-2,-2,-2) finds it");
        check(sameSet(g.queryRegion(Aabb{vec3(-4, -4, -4), vec3(-2, -2, -2)}), {11}), "negative: overlapping region finds it");
    }

    // --- 6: REMOVE -----------------------------------------------------------
    {
        SpatialHashGrid g(1.0f);
        g.insert(1, Aabb{vec3(0, 0, 0), vec3(1, 1, 1)});
        g.insert(2, Aabb{vec3(0, 0, 0), vec3(1, 1, 1)});
        check(g.remove(1), "remove: existing returns true");
        check(g.size() == 1, "remove: size decremented");
        check(sameSet(g.queryRegion(Aabb{vec3(0, 0, 0), vec3(1, 1, 1)}), {2}), "remove: query returns only b");
        check(!g.remove(9999), "remove: absent id returns false");
        check(g.remove(2), "remove: last item returns true");
        check(g.cellCount() == 0, "remove: cells pruned when empty");
    }

    // --- 7: UPDATE -----------------------------------------------------------
    {
        SpatialHashGrid g(1.0f);
        g.insert(5, Aabb{vec3(0, 0, 0), vec3(1, 1, 1)});
        g.update(5, Aabb{vec3(50, 50, 50), vec3(51, 51, 51)});
        check(g.queryRegion(Aabb{vec3(0, 0, 0), vec3(2, 2, 2)}).empty(), "update: old region -> empty");
        check(sameSet(g.queryRegion(Aabb{vec3(49, 49, 49), vec3(52, 52, 52)}), {5}), "update: new region -> {5}");
        check(g.size() == 1, "update: size unchanged");
    }

    // --- 8: MULTIPLE OVERLAPPING ITEMS ---------------------------------------
    {
        SpatialHashGrid g(1.0f);
        g.insert(1, Aabb{vec3(0, 0, 0), vec3(2, 2, 2)});
        g.insert(2, Aabb{vec3(1, 1, 1), vec3(3, 3, 3)});
        g.insert(3, Aabb{vec3(2, 2, 2), vec3(4, 4, 4)});
        g.insert(4, Aabb{vec3(20, 20, 20), vec3(21, 21, 21)});  // far away
        check(sameSet(g.queryRegion(Aabb{vec3(1, 1, 1), vec3(2, 2, 2)}), {1, 2, 3}), "overlap: region returns the full set");
    }

    // --- 9: CLEAR ------------------------------------------------------------
    {
        SpatialHashGrid g(1.0f);
        g.insert(1, Aabb{vec3(0, 0, 0), vec3(1, 1, 1)});
        g.insert(2, Aabb{vec3(5, 5, 5), vec3(6, 6, 6)});
        g.clear();
        check(g.size() == 0, "clear: size() == 0");
        check(g.cellCount() == 0, "clear: cellCount() == 0");
        check(g.queryRegion(Aabb{vec3(0, 0, 0), vec3(10, 10, 10)}).empty(), "clear: queryRegion empty");
        check(g.queryPoint(vec3(0, 0, 0)).empty(), "clear: queryPoint empty");
    }

    // --- 10: LATTICE CONSISTENCY ---------------------------------------------
    {
        SpatialHashGrid g(1.0f);
        // id i at box [i,0,0]-[i+1,1,1] along the x axis (0..99).
        for (std::uint32_t i = 0; i < 100; ++i) {
            g.insert(i, Aabb{vec3(static_cast<float>(i), 0, 0),
                             vec3(static_cast<float>(i) + 1, 1, 1)});
        }
        check(g.size() == 100, "lattice: 100 items inserted");
        // Remove even ids.
        for (std::uint32_t i = 0; i < 100; i += 2) {
            check(g.remove(i), "lattice: even id removed");
        }
        check(g.size() == 50, "lattice: 50 items remain after removing evens");
        // Query the sub-region x in [10,20]; expect the odd ids whose box overlaps.
        // Box i spans [i, i+1], so it overlaps [10,20] for i in 9..20 -> odd: 9,11,13,15,17,19.
        std::vector<std::uint32_t> res = g.queryRegion(Aabb{vec3(10, 0, 0), vec3(20, 1, 1)});
        check(sameSet(res, {9, 11, 13, 15, 17, 19}), "lattice: sub-region returns exactly the surviving odd ids");
    }

    // --- 11: STRADDLING BOX PINS std::floor CELL DISTRIBUTION ----------------
    // This is the REAL floor guard. The negative-coords test (block 5) uses only
    // exact-integer coords where std::floor(x) == (int)x, and both insert and query
    // route through the same cellCoord AND queries re-filter in narrow phase — so no
    // query-RESULT assertion can distinguish std::floor from a truncating cast (the
    // whole suite still reports "ALL PASS" under truncation). The distinction is only
    // observable in cell DISTRIBUTION: a box straddling the origin. With cellSize 1.0,
    // min=-0.5 -> floor(-0.5)=-1 and max=0.5 -> floor(0.5)=0, so each axis spans cells
    // {-1,0}; the inclusive insert triple-loop rasterizes 2x2x2 = 8 cells. A truncating
    // cellCoord would round both -0.5 and 0.5 toward zero into cell 0, collapsing the
    // box to a single cell (0,0,0) and reporting cellCount()==1 — which this check
    // catches but the narrow-phased query-result tests cannot.
    {
        SpatialHashGrid g(1.0f);
        g.insert(1, maz::math::Aabb{ maz::math::vec3(-0.5f, -0.5f, -0.5f),
                                     maz::math::vec3(0.5f, 0.5f, 0.5f) });
        check(g.cellCount() == 8, "straddling box spans 8 cells (std::floor, not truncation)");
    }

    // --- 12: INVALID-BOX UPSERT IS A NO-OP (does not wipe existing entry) -----
    // insert(existingId, invalidBox) must leave the prior placement untouched: the
    // !isValid() guard runs before the upsert remove(), so an invalid box never
    // deletes an existing entry.
    {
        SpatialHashGrid g(1.0f);
        g.insert(7, Aabb{vec3(0, 0, 0), vec3(1, 1, 1)});
        g.insert(7, Aabb::invalid());  // invalid upsert -> total no-op
        check(g.size() == 1, "invalid-upsert: existing entry still present");
        check(sameSet(g.queryRegion(Aabb{vec3(0, 0, 0), vec3(1, 1, 1)}), {7}),
              "invalid-upsert: queryRegion over original box still returns {7}");
        check(sameSet(g.queryPoint(vec3(0, 0, 0)), {7}),
              "invalid-upsert: queryPoint inside original box still returns {7}");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
