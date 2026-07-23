// tests/math/polygonboolean.cpp — verifies general polygon boolean ops (math PolygonBoolean.hpp).
// Ground truths, deterministic (fixed polygons + seeded LCG Monte-Carlo, no <random>, no clock):
//   * MEMBERSHIP ORACLE (independent of the tracing code): a point lies in the RESULT region — measured by
//     an even-odd ray cast over ALL result edges — exactly when it satisfies the boolean predicate against
//     the two inputs (inside A AND B for intersection; A OR B for union; A AND NOT B for difference). This
//     is checked over thousands of seeded random points and is what actually validates correctness: it does
//     not depend on the output being pretty, only on it enclosing the right region.
//   * INCLUSION–EXCLUSION: |A∩B| + |A∪B| == |A| + |B| for a partial overlap (an analytic area identity).
//   * CONTAINMENT: for B nested in A, A∩B has area |B| and A∪B has area |A|; DISJOINT inputs give an empty
//     intersection and a union whose area is |A|+|B|.
//   * CONCAVE inputs (an L-shape) are handled, not just convex.
//   * determinism.
#include "maz/math/PolygonBoolean.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::math::vec2;

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

// Even-odd point-in-region over every edge of every result contour (independent of the boolean tracer).
static bool inResult(const vec2& p, const std::vector<std::vector<vec2>>& contours) {
    bool inside = false;
    for (const auto& ring : contours) {
        const std::size_t n = ring.size();
        for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
            const vec2& a = ring[i];
            const vec2& b = ring[j];
            if (((a.y > p.y) != (b.y > p.y)) &&
                (p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x)) {
                inside = !inside;
            }
        }
    }
    return inside;
}

static bool inRing(const vec2& p, const std::vector<vec2>& ring) {
    return inResult(p, {ring});
}

static float ringArea(const std::vector<vec2>& poly) {
    float a = 0.0f;
    const std::size_t n = poly.size();
    for (std::size_t i = 0; i < n; ++i) {
        const vec2& p = poly[i];
        const vec2& q = poly[(i + 1) % n];
        a += p.x * q.y - q.x * p.y;
    }
    return 0.5f * a;
}

// Net (signed) area of a multi-contour result — holes (opposite winding) subtract.
static float netArea(const std::vector<std::vector<vec2>>& contours) {
    float a = 0.0f;
    for (const auto& r : contours) a += std::fabs(ringArea(r));
    return a;
}

// Monte-Carlo agreement between the result region and a reference predicate over [lo,hi]^2.
template <class Pred>
static double agreement(const std::vector<std::vector<vec2>>& res, Pred pred, float lo, float hi,
                        std::uint64_t seed) {
    Lcg rng{seed};
    int agree = 0, total = 0;
    for (int i = 0; i < 6000; ++i) {
        const vec2 p{lo + (hi - lo) * rng.unit(), lo + (hi - lo) * rng.unit()};
        if (inResult(p, res) == pred(p)) ++agree;
        ++total;
    }
    return static_cast<double>(agree) / static_cast<double>(total);
}

int main() {
    // Two overlapping squares (partial overlap): A=[0,6]^2, B=[3,9]^2.
    std::vector<vec2> A{{0, 0}, {6, 0}, {6, 6}, {0, 6}};
    std::vector<vec2> B{{3, 3}, {9, 3}, {9, 9}, {3, 9}};
    auto inA = [&](const vec2& p) { return inRing(p, A); };
    auto inB = [&](const vec2& p) { return inRing(p, B); };

    // --- 1. Intersection: membership + exact area (overlap is the 3x3 square = 9). ---
    {
        const auto r = maz::math::polygonIntersection(A, B);
        CHECK(agreement(r, [&](const vec2& p) { return inA(p) && inB(p); }, -2, 11, 0x11u) > 0.995,
              "intersection region matches (inside A AND inside B) over random points");
        CHECK(std::fabs(netArea(r) - 9.0f) < 1e-2f, "intersection area equals the 3x3 overlap (=9)");
    }

    // --- 2. Union: membership + inclusion-exclusion. ---
    {
        const auto ri = maz::math::polygonIntersection(A, B);
        const auto ru = maz::math::polygonUnion(A, B);
        CHECK(agreement(ru, [&](const vec2& p) { return inA(p) || inB(p); }, -2, 11, 0x22u) > 0.995,
              "union region matches (inside A OR inside B) over random points");
        const float areaA = std::fabs(ringArea(A)), areaB = std::fabs(ringArea(B));
        CHECK(std::fabs((netArea(ri) + netArea(ru)) - (areaA + areaB)) < 1e-2f,
              "inclusion-exclusion: |A B| + |A B| == |A| + |B|");
    }

    // --- 3. Difference A - B: membership + area = |A| - |A B|. ---
    {
        const auto r = maz::math::polygonDifference(A, B);
        CHECK(agreement(r, [&](const vec2& p) { return inA(p) && !inB(p); }, -2, 11, 0x33u) > 0.995,
              "difference region matches (inside A AND NOT inside B) over random points");
        CHECK(std::fabs(netArea(r) - (36.0f - 9.0f)) < 1e-2f, "difference area equals |A| - overlap (=27)");
    }

    // --- 4. Concave (L-shape) intersected with a square. ---
    {
        // L-shape occupying [0,6]x[0,6] minus the top-right [3,6]x[3,6] quadrant (area 27).
        std::vector<vec2> L{{0, 0}, {6, 0}, {6, 3}, {3, 3}, {3, 6}, {0, 6}};
        std::vector<vec2> S{{2, 2}, {8, 2}, {8, 8}, {2, 8}};
        auto inL = [&](const vec2& p) { return inRing(p, L); };
        auto inS = [&](const vec2& p) { return inRing(p, S); };
        const auto r = maz::math::polygonIntersection(L, S);
        CHECK(agreement(r, [&](const vec2& p) { return inL(p) && inS(p); }, -2, 11, 0x44u) > 0.99,
              "concave L S: result matches (inside L AND inside S)");
        CHECK(netArea(r) > 0.5f, "concave intersection is non-empty");
    }

    // --- 5. Containment: B fully inside A2. ---
    {
        std::vector<vec2> A2{{0, 0}, {10, 0}, {10, 10}, {0, 10}};
        std::vector<vec2> inner{{3, 3}, {6, 3}, {6, 6}, {3, 6}};
        const auto ri = maz::math::polygonIntersection(A2, inner);
        const auto ru = maz::math::polygonUnion(A2, inner);
        CHECK(std::fabs(netArea(ri) - std::fabs(ringArea(inner))) < 1e-2f,
              "A n (inner in A) has the inner polygon's area");
        CHECK(std::fabs(netArea(ru) - std::fabs(ringArea(A2))) < 1e-2f,
              "A u (inner in A) has the outer polygon's area");
    }

    // --- 6. Disjoint: empty intersection, union area = sum. ---
    {
        std::vector<vec2> P{{0, 0}, {2, 0}, {2, 2}, {0, 2}};
        std::vector<vec2> Q{{5, 5}, {7, 5}, {7, 7}, {5, 7}};
        const auto ri = maz::math::polygonIntersection(P, Q);
        const auto ru = maz::math::polygonUnion(P, Q);
        CHECK(netArea(ri) < 1e-3f, "disjoint polygons have empty intersection");
        CHECK(std::fabs(netArea(ru) - 8.0f) < 1e-2f, "disjoint union area is the sum of both (=8)");
    }

    // --- 7. Determinism. ---
    {
        const auto a = maz::math::polygonIntersection(A, B);
        const auto b = maz::math::polygonIntersection(A, B);
        bool same = a.size() == b.size();
        for (std::size_t i = 0; same && i < a.size(); ++i) {
            same = a[i].size() == b[i].size();
            for (std::size_t k = 0; same && k < a[i].size(); ++k)
                same = a[i][k].x == b[i][k].x && a[i][k].y == b[i][k].y;
        }
        CHECK(same, "identical inputs produce identical results");
    }

    if (g_fail == 0) {
        std::printf("polygonboolean: OK — membership oracle, inclusion-exclusion, concave, containment, "
                    "disjoint, determinism.\n");
        return 0;
    }
    std::printf("polygonboolean: %d failure(s).\n", g_fail);
    return 1;
}
