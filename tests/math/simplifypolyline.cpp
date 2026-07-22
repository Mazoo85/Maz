// tests/math/simplifypolyline.cpp — verifies Ramer-Douglas-Peucker simplification (math SimplifyPolyline.hpp).
// Ground truths, deterministic (seeded LCG for polylines, no <random>, no clock):
//   * retained points are a SUBSEQUENCE of the input (no invented vertices) and always include both ends;
//   * the core guarantee: EVERY original point lies within epsilon of the simplified polyline (independent
//     point-to-polyline distance check) over thousands of random polylines and tolerances;
//   * monotonicity: a larger epsilon never keeps more points;
//   * a straight (collinear) run collapses to its two ends; a clear corner is preserved at small epsilon;
//   * degenerate inputs (0/1/2 points) pass through unchanged.
#include "maz/math/SimplifyPolyline.hpp"

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
    float range(float lo, float hi) { return lo + (hi - lo) * (static_cast<float>(next() % 100000u) / 99999.0f); }
};

static float ptSeg(vec2 p, vec2 a, vec2 b) {
    const vec2 ab{b.x - a.x, b.y - a.y};
    const float len2 = ab.x * ab.x + ab.y * ab.y;
    float t = 0.0f;
    if (len2 > 1e-20f) {
        t = ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / len2;
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    }
    const vec2 f{a.x + ab.x * t, a.y + ab.y * t};
    return std::sqrt((p.x - f.x) * (p.x - f.x) + (p.y - f.y) * (p.y - f.y));
}

// Min distance from p to the whole polyline (nearest of its segments).
static float distToPolyline(vec2 p, const std::vector<vec2>& poly) {
    float best = 1e30f;
    for (std::size_t i = 0; i + 1 < poly.size(); ++i) {
        best = std::fmin(best, ptSeg(p, poly[i], poly[i + 1]));
    }
    return best;
}

int main() {
    // --- 1. Collinear run collapses; a corner survives. ---
    {
        std::vector<vec2> line;
        for (int i = 0; i < 10; ++i) line.push_back(vec2{static_cast<float>(i), 0.0f});
        const std::vector<vec2> s = maz::math::simplifyPolyline(line, 0.01f);
        CHECK(s.size() == 2, "a straight run collapses to its two endpoints");

        std::vector<vec2> corner{{0.0f, 0.0f}, {1.0f, 0.0f}, {2.0f, 2.0f}, {3.0f, 4.0f}}; // bend at index 1
        const std::vector<vec2> sc = maz::math::simplifyPolyline(corner, 0.1f);
        CHECK(sc.size() == 3, "a real corner is retained");
        CHECK(sc.front().x == 0.0f && sc.back().x == 3.0f, "endpoints preserved");
    }

    // --- 2. Randomized: subsequence + endpoints + deviation guarantee + monotonicity. ---
    {
        Lcg rng{0x5117Fu};
        bool subseqOk = true, endsOk = true, devOk = true, monoOk = true;
        for (int trial = 0; trial < 3000 && subseqOk && endsOk && devOk && monoOk; ++trial) {
            const int n = 2 + static_cast<int>(rng.next() % 40u);
            std::vector<vec2> pts;
            float x = 0.0f;
            for (int i = 0; i < n; ++i) {
                x += rng.range(0.05f, 1.0f); // strictly increasing x -> a genuine open polyline
                pts.push_back(vec2{x, rng.range(-3.0f, 3.0f)});
            }
            const float eps = rng.range(0.0f, 2.0f);
            const std::vector<std::size_t> idx = maz::math::simplifyPolylineIndices(pts, eps);
            const std::vector<vec2> s = maz::math::simplifyPolyline(pts, eps);

            // Subsequence: strictly increasing indices, values match originals.
            for (std::size_t k = 0; k < idx.size(); ++k) {
                if (k > 0 && idx[k] <= idx[k - 1]) subseqOk = false;
                if (idx[k] >= pts.size()) { subseqOk = false; break; }
                const vec2 a = pts[idx[k]], b = s[k];
                if (a.x != b.x || a.y != b.y) subseqOk = false;
            }
            if (idx.empty() || idx.front() != 0 || idx.back() != pts.size() - 1) endsOk = false;

            // Deviation guarantee: every original point within eps of the simplified polyline.
            for (const vec2& p : pts) {
                if (distToPolyline(p, s) > eps + 1e-4f) { devOk = false; break; }
            }

            // Monotonicity: a bigger tolerance keeps no more points.
            const std::size_t c1 = maz::math::simplifyPolylineIndices(pts, eps).size();
            const std::size_t c2 = maz::math::simplifyPolylineIndices(pts, eps + 0.5f).size();
            if (c2 > c1) monoOk = false;
        }
        CHECK(subseqOk, "retained points are an in-order subsequence of the input");
        CHECK(endsOk, "both endpoints are always retained");
        CHECK(devOk, "every original point is within epsilon of the simplified polyline");
        CHECK(monoOk, "a larger epsilon never keeps more points");
    }

    // --- 3. epsilon large enough drops everything but the ends. ---
    {
        std::vector<vec2> zig{{0.0f, 0.0f}, {1.0f, 0.2f}, {2.0f, -0.15f}, {3.0f, 0.1f}, {4.0f, 0.0f}};
        CHECK(maz::math::simplifyPolyline(zig, 10.0f).size() == 2, "huge epsilon leaves only the endpoints");
    }

    // --- 4. Degenerate inputs. ---
    {
        std::vector<vec2> empty;
        std::vector<vec2> one{{1.0f, 1.0f}};
        std::vector<vec2> two{{0.0f, 0.0f}, {5.0f, 5.0f}};
        CHECK(maz::math::simplifyPolyline(empty, 0.5f).empty(), "empty stays empty");
        CHECK(maz::math::simplifyPolyline(one, 0.5f).size() == 1, "single point passes through");
        CHECK(maz::math::simplifyPolyline(two, 0.5f).size() == 2, "two points pass through");
    }

    if (g_fail == 0) {
        std::printf("simplifypolyline: OK — collapse/corner, subsequence, deviation guarantee, monotonicity, degenerate.\n");
        return 0;
    }
    std::printf("simplifypolyline: %d failure(s).\n", g_fail);
    return 1;
}
