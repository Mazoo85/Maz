// tests/math/bezierintersect.cpp — verifies cubic Bézier curve–curve intersection (math BezierIntersect.hpp).
// Ground truths, deterministic (fixed curves, no <random>, no clock):
//   * INDEPENDENT ORACLE: for many curve pairs, the reported crossings match a brute-force polyline
//     intersection — each curve is finely sampled into 600 segments and every segment/segment crossing is
//     found and clustered; the subdivision result must agree on COUNT and LOCATION with that oracle;
//   * ON BOTH CURVES: every reported point lies on curve A and on curve B (min distance from the point to a
//     dense sampling of each curve is ~0);
//   * an analytic X-cross of two near-straight curves is found at the known centre;
//   * two clearly-separated curves report no intersection;
//   * determinism.
#include "maz/math/BezierIntersect.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using maz::math::vec2;
using maz::math::CubicBezier2;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float dist(const vec2& a, const vec2& b) {
    const float dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// Minimum distance from p to a dense sampling of a cubic Bézier (independent "is it on the curve" check).
static float minDistToCurve(const CubicBezier2& c, const vec2& p) {
    float best = 1e30f;
    for (int i = 0; i <= 2000; ++i) {
        const vec2 q = maz::math::cubicBezierEval(c, static_cast<float>(i) / 2000.0f);
        best = std::min(best, dist(p, q));
    }
    return best;
}

// Brute-force oracle: sample both curves into polylines and intersect every segment pair, clustering hits.
static std::vector<vec2> bruteForce(const CubicBezier2& a, const CubicBezier2& b) {
    const int N = 600;
    std::vector<vec2> A(static_cast<std::size_t>(N + 1)), B(static_cast<std::size_t>(N + 1));
    for (int i = 0; i <= N; ++i) {
        A[static_cast<std::size_t>(i)] = maz::math::cubicBezierEval(a, static_cast<float>(i) / N);
        B[static_cast<std::size_t>(i)] = maz::math::cubicBezierEval(b, static_cast<float>(i) / N);
    }
    std::vector<vec2> hits;
    auto segInt = [](vec2 p, vec2 p2, vec2 q, vec2 q2, vec2& out) -> bool {
        const vec2 r = p2 - p, s = q2 - q;
        const float denom = r.x * s.y - r.y * s.x;
        if (std::fabs(denom) < 1e-12f) return false;
        const vec2 qp = q - p;
        const float t = (qp.x * s.y - qp.y * s.x) / denom;
        const float u = (qp.x * r.y - qp.y * r.x) / denom;
        if (t < 0.0f || t > 1.0f || u < 0.0f || u > 1.0f) return false;
        out = vec2(p.x + t * r.x, p.y + t * r.y);
        return true;
    };
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            vec2 h;
            if (segInt(A[static_cast<std::size_t>(i)], A[static_cast<std::size_t>(i + 1)],
                       B[static_cast<std::size_t>(j)], B[static_cast<std::size_t>(j + 1)], h)) {
                bool merged = false;
                for (vec2& e : hits) {
                    if (dist(e, h) < 0.05f) { merged = true; break; }
                }
                if (!merged) hits.push_back(h);
            }
        }
    }
    return hits;
}

// Does every point in `got` have a match in `want` and vice versa (within tol)?
static bool sameSet(const std::vector<vec2>& got, const std::vector<vec2>& want, float tol) {
    if (got.size() != want.size()) return false;
    for (const vec2& w : want) {
        bool found = false;
        for (const vec2& g : got) if (dist(g, w) < tol) { found = true; break; }
        if (!found) return false;
    }
    return true;
}

int main() {
    // A set of curve pairs producing 0..3 crossings.
    std::vector<std::pair<CubicBezier2, CubicBezier2>> cases = {
        // Single X-cross of two near-straight curves through the origin region.
        {{{-3, -3}, {-1, -1}, {1, 1}, {3, 3}}, {{-3, 3}, {-1, 1}, {1, -1}, {3, -3}}},
        // An arch crossing a rising line twice.
        {{{-3, 0}, {-1, 6}, {1, 6}, {3, 0}}, {{-3, 1}, {-1, 2}, {1, 3}, {3, 4}}},
        // A wavy S curve crossing a horizontal-ish curve (multiple crossings).
        {{{-3, 0}, {-1, 4}, {1, -4}, {3, 0}}, {{-3, 0}, {-1, 0.2f}, {1, -0.2f}, {3, 0}}},
        // Clearly separated: no crossing.
        {{{-3, 5}, {-1, 6}, {1, 6}, {3, 5}}, {{-3, -5}, {-1, -6}, {1, -6}, {3, -5}}},
    };

    for (std::size_t k = 0; k < cases.size(); ++k) {
        const CubicBezier2& A = cases[k].first;
        const CubicBezier2& B = cases[k].second;
        const auto got = maz::math::bezierIntersections(A, B, 1e-3f);
        const auto want = bruteForce(A, B);
        CHECK(sameSet(got, want, 0.05f), "subdivision crossings match the brute-force polyline oracle");
        for (const vec2& p : got) {
            CHECK(minDistToCurve(A, p) < 0.02f, "each reported crossing lies on curve A");
            CHECK(minDistToCurve(B, p) < 0.02f, "each reported crossing lies on curve B");
        }
    }

    // Analytic single cross: the two straight diagonals meet at the origin.
    {
        const CubicBezier2 A{{-3, -3}, {-1, -1}, {1, 1}, {3, 3}};
        const CubicBezier2 B{{-3, 3}, {-1, 1}, {1, -1}, {3, -3}};
        const auto got = maz::math::bezierIntersections(A, B, 1e-3f);
        CHECK(got.size() == 1, "the two diagonals cross exactly once");
        if (got.size() == 1) {
            CHECK(dist(got[0], vec2(0, 0)) < 0.02f, "the diagonals cross at the origin");
        }
    }

    // Separated curves: none.
    {
        const CubicBezier2 A{{-3, 5}, {-1, 6}, {1, 6}, {3, 5}};
        const CubicBezier2 B{{-3, -5}, {-1, -6}, {1, -6}, {3, -5}};
        CHECK(maz::math::bezierIntersections(A, B).empty(), "separated curves report no crossings");
    }

    // Determinism.
    {
        const CubicBezier2 A{{-3, 0}, {-1, 4}, {1, -4}, {3, 0}};
        const CubicBezier2 B{{-3, 0}, {-1, 1}, {1, -1}, {3, 0}};
        const auto a = maz::math::bezierIntersections(A, B);
        const auto b = maz::math::bezierIntersections(A, B);
        bool same = a.size() == b.size();
        for (std::size_t i = 0; same && i < a.size(); ++i) {
            same = a[i].x == b[i].x && a[i].y == b[i].y;
        }
        CHECK(same, "identical inputs produce identical results");
    }

    if (g_fail == 0) {
        std::printf("bezierintersect: OK — brute-force agreement, on-curve, analytic cross, separation, "
                    "determinism.\n");
        return 0;
    }
    std::printf("bezierintersect: %d failure(s).\n", g_fail);
    return 1;
}
