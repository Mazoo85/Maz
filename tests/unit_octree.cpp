// Unit tests for maz::spatial::Octree<T> — the hierarchical adaptive point octree.
// Exercises insert/size/empty, exact-set queryAabb and querySphere (with hand-verified
// boundaries), forced subdivision + redistribution (no drop/dup), split-plane point
// placement (octantOf/childBounds agreement), coincident points beyond capacity at
// max depth (bounded subdivision, no lost point), empty/no-match queries, the
// append-not-clear query contract, and clear(). Pure C++, no GPU/display.

#include "maz/spatial/Octree.hpp"

#include <cstdio>
#include <vector>
#include <algorithm>  // std::sort

using namespace maz::spatial;
using maz::math::Aabb;
using maz::math::vec3;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

// Exact SET comparison of an int payload list (robust to traversal order): sort a
// copy of the actual results and compare to a sorted expected list.
bool sameSet(std::vector<int> actual, std::vector<int> expected) {
    std::sort(actual.begin(), actual.end());
    std::sort(expected.begin(), expected.end());
    return actual == expected;
}

} // namespace

int main() {
    const Aabb whole{vec3(0, 0, 0), vec3(8, 8, 8)};

    // --- INSERT + SIZE -------------------------------------------------------
    {
        Octree<int> t(whole);
        check(t.empty() && t.size() == 0, "fresh tree is empty, size 0");
        t.insert(vec3(1, 1, 1), 10);
        t.insert(vec3(2, 3, 4), 20);
        t.insert(vec3(5, 6, 7), 30);
        check(t.size() == 3, "size == 3 after three inserts");
        check(!t.empty(), "tree not empty after inserts");
    }

    // --- QUERY AABB EXACT SET ------------------------------------------------
    {
        Octree<int> t(whole);
        t.insert(vec3(1, 1, 1), 10);
        t.insert(vec3(7, 7, 7), 20);
        t.insert(vec3(1, 7, 1), 30);
        t.insert(vec3(7, 1, 7), 40);
        t.insert(vec3(4, 4, 4), 50);

        std::vector<int> out;
        t.queryAabb(Aabb{vec3(0, 0, 0), vec3(3, 3, 3)}, out);
        check(sameSet(out, {10}), "queryAabb corner {0,0,0}-{3,3,3} -> {10}");

        out.clear();
        t.queryAabb(whole, out);
        check(sameSet(out, {10, 20, 30, 40, 50}), "queryAabb whole -> all 5");

        out.clear();
        t.queryAabb(Aabb{vec3(6, 6, 6), vec3(8, 8, 8)}, out);
        check(sameSet(out, {20}), "queryAabb corner {6,6,6}-{8,8,8} -> {20}");
    }

    // --- QUERY SPHERE EXACT --------------------------------------------------
    {
        Octree<int> t(whole);
        t.insert(vec3(1, 1, 1), 10);
        t.insert(vec3(7, 7, 7), 20);
        t.insert(vec3(1, 7, 1), 30);
        t.insert(vec3(7, 1, 7), 40);
        t.insert(vec3(4, 4, 4), 50);

        std::vector<int> out;
        t.querySphere(vec3(1, 1, 1), 0.5f, out);  // only (1,1,1) itself
        check(sameSet(out, {10}), "querySphere (1,1,1) r=0.5 -> {10}");

        out.clear();
        t.querySphere(vec3(4, 4, 4), 100.0f, out);  // everything
        check(sameSet(out, {10, 20, 30, 40, 50}), "querySphere (4,4,4) r=100 -> all 5");

        out.clear();
        t.querySphere(vec3(7, 7, 7), 1.0f, out);  // only (7,7,7)
        check(sameSet(out, {20}), "querySphere (7,7,7) r=1 -> {20}");
    }

    // --- FORCE SUBDIVISION ---------------------------------------------------
    {
        Octree<int> t(whole, /*maxPerNode*/ 2, /*maxDepth*/ 8);
        t.insert(vec3(1, 1, 1), 1);  // octant 000
        t.insert(vec3(7, 1, 1), 2);  // octant 001
        t.insert(vec3(1, 7, 1), 3);  // octant 010
        t.insert(vec3(7, 7, 1), 4);  // octant 011
        t.insert(vec3(1, 1, 7), 5);  // octant 100
        t.insert(vec3(7, 1, 7), 6);  // octant 101
        t.insert(vec3(3, 5, 4), 7);  // mid
        t.insert(vec3(5, 3, 4), 8);  // mid
        check(t.size() == 8, "size == 8 after forced subdivision");

        std::vector<int> out;
        t.queryAabb(whole, out);
        check(sameSet(out, {1, 2, 3, 4, 5, 6, 7, 8}), "queryAabb whole after subdivision -> all 8");

        out.clear();
        t.queryAabb(Aabb{vec3(6, 0, 0), vec3(8, 2, 2)}, out);
        check(sameSet(out, {2}), "targeted corner query -> just {2}");
    }

    // --- BOUNDARY / SPLIT-PLANE POINT ----------------------------------------
    {
        Octree<int> t(whole, /*maxPerNode*/ 1, /*maxDepth*/ 4);
        t.insert(vec3(4, 4, 4), 100);  // exactly on the center split plane
        t.insert(vec3(1, 1, 1), 200);  // forces subdivision so the plane point relocates
        t.insert(vec3(7, 7, 7), 300);

        std::vector<int> out;
        t.queryAabb(whole, out);
        check(sameSet(out, {100, 200, 300}), "split-plane point survives subdivision (whole -> all 3)");

        out.clear();
        t.queryAabb(Aabb{vec3(4, 4, 4), vec3(5, 5, 5)}, out);
        check(sameSet(out, {100}), "region touching (4,4,4) returns it");
    }

    // --- COINCIDENT POINTS BEYOND CAPACITY AT MAX DEPTH ----------------------
    {
        Octree<int> t(whole, /*maxPerNode*/ 1, /*maxDepth*/ 2);
        t.insert(vec3(2, 2, 2), 1);
        t.insert(vec3(2, 2, 2), 2);
        t.insert(vec3(2, 2, 2), 3);
        check(t.size() == 3, "coincident points: size == 3");

        std::vector<int> out;
        t.queryAabb(whole, out);
        check(sameSet(out, {1, 2, 3}), "coincident points all found (whole -> {1,2,3})");
    }

    // --- EMPTY QUERY / NO MATCH ----------------------------------------------
    {
        Octree<int> t(whole);
        t.insert(vec3(1, 1, 1), 10);
        t.insert(vec3(7, 7, 7), 20);

        std::vector<int> out;
        t.queryAabb(Aabb{vec3(3, 3, 3), vec3(4, 4, 4)}, out);
        check(out.empty(), "queryAabb region with no points -> empty");

        out.clear();
        t.querySphere(vec3(4, 4, 4), 0.1f, out);
        check(out.empty(), "querySphere tiny radius far from all -> empty");
    }

    // --- QUERY APPENDS (NO CLEAR) --------------------------------------------
    {
        Octree<int> t(whole);
        t.insert(vec3(1, 1, 1), 10);

        std::vector<int> out;
        out.push_back(-999);  // sentinel
        t.queryAabb(whole, out);
        check(sameSet(out, {-999, 10}), "queryAabb appends (sentinel preserved + result)");
    }

    // --- CLEAR ---------------------------------------------------------------
    {
        Octree<int> t(whole);
        t.insert(vec3(1, 1, 1), 10);
        t.insert(vec3(7, 7, 7), 20);
        t.clear();
        check(t.size() == 0 && t.empty(), "clear -> size 0, empty");

        std::vector<int> out;
        t.queryAabb(whole, out);
        check(out.empty(), "queryAabb after clear -> empty");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
