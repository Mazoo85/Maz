// tests/math/epa.cpp — verifies EPA penetration depth + separating normal (math Epa.hpp).
// Ground truths, deterministic (seeded LCG for shapes, no <random>, no clock):
//   * a hand-computable axis-aligned box overlap gives the exact depth and normal;
//   * over many random overlapping convex polygons, EPA's penetration depth matches an INDEPENDENT SAT
//     minimum-translation-vector computation, and the normal is parallel to the SAT axis;
//   * the separating property holds: translating A by (depth+margin)*normal removes the overlap
//     (checked with an independent SAT overlap test);
//   * clearly separated shapes report intersecting=false.
#include "maz/math/Epa.hpp"

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

// --- Independent SAT: minimum-translation-vector between two convex polygons (CCW). ---
struct Sat {
    bool overlap = false;
    float depth = 0.0f;
    vec2 axis{0.0f, 0.0f}; // points to push A away from B
};

static void projectOnto(const std::vector<vec2>& poly, vec2 ax, float& mn, float& mx) {
    mn = 1e30f;
    mx = -1e30f;
    for (const vec2& p : poly) {
        const float d = p.x * ax.x + p.y * ax.y;
        mn = d < mn ? d : mn;
        mx = d > mx ? d : mx;
    }
}

static Sat satMtv(const std::vector<vec2>& a, const std::vector<vec2>& b) {
    Sat r;
    r.overlap = true;
    float best = 1e30f;
    vec2 bestAxis{0.0f, 0.0f};
    for (int shape = 0; shape < 2; ++shape) {
        const std::vector<vec2>& poly = shape == 0 ? a : b;
        const std::size_t n = poly.size();
        for (std::size_t i = 0; i < n; ++i) {
            const vec2 e = poly[(i + 1) % n] - poly[i];
            vec2 ax{-e.y, e.x};
            const float len = std::sqrt(ax.x * ax.x + ax.y * ax.y);
            if (len < 1e-9f) continue;
            ax = vec2{ax.x / len, ax.y / len};
            float amn, amx, bmn, bmx;
            projectOnto(a, ax, amn, amx);
            projectOnto(b, ax, bmn, bmx);
            if (amx < bmn || bmx < amn) { r.overlap = false; return r; }
            // Penetration to push A off B along this axis. NOTE: this is min(amx-bmn, bmx-amn), the distance
            // to slide one interval entirely past the other -- NOT the intersection length, which
            // under-reports when one projection is fully contained in the other.
            const float o = std::fmin(amx - bmn, bmx - amn);
            if (o < best) { best = o; bestAxis = ax; }
        }
    }
    // Orient the axis to push A away from B (from B's centroid toward A's centroid).
    vec2 ca{0.0f, 0.0f}, cb{0.0f, 0.0f};
    for (const vec2& p : a) ca = ca + p;
    for (const vec2& p : b) cb = cb + p;
    ca = vec2{ca.x / static_cast<float>(a.size()), ca.y / static_cast<float>(a.size())};
    cb = vec2{cb.x / static_cast<float>(b.size()), cb.y / static_cast<float>(b.size())};
    const vec2 ab = ca - cb;
    if (ab.x * bestAxis.x + ab.y * bestAxis.y < 0.0f) bestAxis = vec2{-bestAxis.x, -bestAxis.y};
    r.depth = best;
    r.axis = bestAxis;
    return r;
}

// A random CCW convex polygon: the convex hull of a handful of random points in a disk. Taking the hull
// guarantees convexity (an ellipse sweep can wrap past 2*pi and self-cross; a hull never can).
static std::vector<vec2> randomConvex(Lcg& rng, vec2 center, float rmin, float rmax) {
    std::vector<vec2> pts;
    for (int i = 0; i < 8; ++i) {
        const float ang = rng.range(0.0f, 6.2831853f);
        const float r = rng.range(rmin, rmax);
        pts.push_back(vec2{center.x + std::cos(ang) * r, center.y + std::sin(ang) * r});
    }
    return maz::math::detail::convexHullCcw(std::move(pts));
}

int main() {
    // --- 1. Hand-computable axis-aligned boxes. ---
    {
        std::vector<vec2> A{{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}, {-0.5f, 0.5f}};
        std::vector<vec2> B{{0.0f, -0.5f}, {1.0f, -0.5f}, {1.0f, 0.5f}, {0.0f, 0.5f}}; // shifted +0.5 in x
        const maz::math::EpaResult e = maz::math::epaPenetration(A, B);
        CHECK(e.intersecting, "overlapping boxes report intersection");
        CHECK(std::fabs(e.depth - 0.5f) < 1e-3f, "box overlap depth is 0.5");
        CHECK(std::fabs(e.normal.x + 1.0f) < 1e-3f && std::fabs(e.normal.y) < 1e-3f,
              "box separating normal points -x (push A away from B)");
    }

    // --- 2. Random overlapping convex polygons vs SAT + separation property. ---
    {
        Lcg rng{0xEA9C137u};
        bool depthOk = true, normalOk = true, sepOk = true, detectOk = true;
        int overlaps = 0;
        for (int trial = 0; trial < 4000 && depthOk && normalOk && sepOk && detectOk; ++trial) {
            const vec2 ca{rng.range(-1.0f, 1.0f), rng.range(-1.0f, 1.0f)};
            const vec2 cb{rng.range(-1.0f, 1.0f), rng.range(-1.0f, 1.0f)};
            const std::vector<vec2> A = randomConvex(rng, ca, 0.5f, 1.2f);
            const std::vector<vec2> B = randomConvex(rng, cb, 0.5f, 1.2f);
            const Sat sat = satMtv(A, B);
            const maz::math::EpaResult e = maz::math::epaPenetration(A, B);
            if (e.intersecting != sat.overlap) { detectOk = false; break; }
            if (!sat.overlap) continue;
            ++overlaps;
            if (std::fabs(e.depth - sat.depth) > 1e-2f * (1.0f + sat.depth)) { depthOk = false; break; }
            // Normal PARALLEL to the SAT axis (either sign — SAT's centroid-based orientation heuristic is
            // not always reliable; EPA's orientation is validated by the separation property below).
            const float align = e.normal.x * sat.axis.x + e.normal.y * sat.axis.y;
            if (std::fabs(align) < 0.98f) { normalOk = false; break; }
            // Separation: translate A by (depth+margin)*normal -> no longer overlapping (independent SAT).
            std::vector<vec2> Amoved = A;
            const vec2 shift{e.normal.x * (e.depth + 0.05f), e.normal.y * (e.depth + 0.05f)};
            for (vec2& p : Amoved) p = p + shift;
            if (satMtv(Amoved, B).overlap) { sepOk = false; break; }
        }
        CHECK(overlaps > 200, "the random battery actually produced many overlaps");
        CHECK(detectOk, "EPA overlap detection agrees with SAT");
        CHECK(depthOk, "EPA penetration depth matches SAT minimum translation");
        CHECK(normalOk, "EPA normal is parallel to the SAT MTV axis");
        CHECK(sepOk, "translating A by (depth+margin)*normal separates the shapes");
    }

    // --- 3. Clearly separated shapes. ---
    {
        std::vector<vec2> A{{0.0f, 0.0f}, {1.0f, 0.0f}, {0.5f, 1.0f}};
        std::vector<vec2> B{{5.0f, 5.0f}, {6.0f, 5.0f}, {5.5f, 6.0f}};
        CHECK(!maz::math::epaPenetration(A, B).intersecting, "far-apart shapes report no intersection");
    }

    if (g_fail == 0) {
        std::printf("epa: OK — box depth/normal, SAT parity (depth+normal), separation property, disjoint.\n");
        return 0;
    }
    std::printf("epa: %d failure(s).\n", g_fail);
    return 1;
}
