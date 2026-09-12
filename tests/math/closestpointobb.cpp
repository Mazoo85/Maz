// tests/math/closestpointobb.cpp — verifies closest point on an oriented box (math ClosestPointObb.hpp).
// Ground truths, deterministic (fixed + seeded-LCG points, no <random>, no clock):
//   * BRUTE-FORCE ORACLE: for random points and a rotated box, the returned closest point matches — in both
//     location and distance — the nearest of thousands of points densely sampled over the box's six faces;
//   * INSIDE: a point inside the box returns itself (distance 0); the result always lies on/in the box;
//   * ANALYTIC: for an axis-aligned box, a point pushed straight out along +x clamps to (halfx, y, z);
//   * IDEMPOTENCE: closest(closest(p)) == closest(p); sphere-vs-OBB agrees with the distance; determinism.
#include "maz/math/ClosestPointObb.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::math::vec3;
using maz::math::Obb;
using maz::math::dot;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float dist(const vec3& a, const vec3& b) { const vec3 v = a - b; return std::sqrt(dot(v, v)); }

// A box rotated about the Z then Y axes (columns of `axes` are the local axes in world space).
static Obb rotatedBox() {
    Obb b;
    b.center = vec3(2.0f, -1.0f, 0.5f);
    b.half = vec3(1.5f, 0.8f, 1.1f);
    const float cz = std::cos(0.6f), sz = std::sin(0.6f);
    const float cy = std::cos(-0.4f), sy = std::sin(-0.4f);
    // Rz then Ry applied to the identity axes; store columns as local x/y/z.
    const vec3 x0(cz, sz, 0.0f), y0(-sz, cz, 0.0f), z0(0.0f, 0.0f, 1.0f);
    auto roty = [&](const vec3& v) { return vec3(cy * v.x + sy * v.z, v.y, -sy * v.x + cy * v.z); };
    const vec3 x = roty(x0), y = roty(y0), z = roty(z0);
    b.axes = maz::math::mat3(x.x, x.y, x.z, y.x, y.y, y.z, z.x, z.y, z.z);
    return b;
}

// Brute-force nearest point over the six faces of the OBB (independent oracle).
static float bruteNearest(const vec3& p, const Obb& box, vec3& outClosest) {
    float best = 1e30f;
    const int N = 60;
    for (int face = 0; face < 6; ++face) {
        const int axis = face / 2;
        const float sgn = (face & 1) ? 1.0f : -1.0f;
        const int u = (axis + 1) % 3, v = (axis + 2) % 3;
        for (int i = 0; i <= N; ++i) {
            for (int j = 0; j <= N; ++j) {
                const float fu = (static_cast<float>(i) / N * 2.0f - 1.0f) * box.half[u];
                const float fv = (static_cast<float>(j) / N * 2.0f - 1.0f) * box.half[v];
                const vec3 q = box.center + box.axis(axis) * (sgn * box.half[axis]) +
                               box.axis(u) * fu + box.axis(v) * fv;
                const float dd = dist(p, q);
                if (dd < best) { best = dd; outClosest = q; }
            }
        }
    }
    return best;
}

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 33); }
    float sym() { return static_cast<float>(next()) / 4294967296.0f * 12.0f - 6.0f; }
};

int main() {
    const Obb box = rotatedBox();

    // --- 1. Brute-force oracle over random points. ---
    {
        Lcg rng{0x0BB0u};
        float worstDistErr = 0.0f, worstPtErr = 0.0f;
        int outside = 0;
        for (int t = 0; t < 4000; ++t) {
            const vec3 p(rng.sym() + 2.0f, rng.sym() - 1.0f, rng.sym() + 0.5f);
            if (box.contains(p)) continue;
            ++outside;
            const vec3 got = maz::math::closestPointOnObb(p, box);
            vec3 bruteC(0.0f);
            const float bruteD = bruteNearest(p, box, bruteC);
            const float gotD = dist(p, got);
            // Our analytic distance must be <= brute-force sampled distance (finer), and very close.
            worstDistErr = std::max(worstDistErr, gotD - bruteD); // should be <= tiny (we can only be better)
            worstPtErr = std::max(worstPtErr, dist(got, bruteC));
        }
        CHECK(outside > 1000, "the random set contains many points outside the box");
        CHECK(worstDistErr < 1e-3f, "analytic closest distance never exceeds the brute-force sampled distance");
        CHECK(worstPtErr < 0.15f, "analytic closest point matches the brute-force nearest (within sampling)");
    }

    // --- 2. Inside points return themselves; result is always on/in the box. ---
    {
        Lcg rng{0xC0DEu};
        bool insideOk = true, onBoxOk = true;
        for (int t = 0; t < 2000; ++t) {
            const vec3 p(rng.sym() + 2.0f, rng.sym() - 1.0f, rng.sym() + 0.5f);
            const vec3 c = maz::math::closestPointOnObb(p, box);
            if (box.contains(p) && dist(p, c) > 1e-4f) insideOk = false;
            // c must be inside/on the box (allow a small epsilon).
            const vec3 d = c - box.center;
            for (int i = 0; i < 3; ++i) {
                if (std::fabs(dot(d, box.axis(i))) > box.half[i] + 1e-3f) onBoxOk = false;
            }
        }
        CHECK(insideOk, "a point inside the box is its own closest point (distance 0)");
        CHECK(onBoxOk, "the closest point always lies on or inside the box");
    }

    // --- 3. Analytic: axis-aligned box. ---
    {
        Obb ab;
        ab.center = vec3(0, 0, 0);
        ab.half = vec3(1, 2, 3);
        ab.axes = maz::math::mat3(1.0f); // identity
        const vec3 c = maz::math::closestPointOnObb(vec3(5.0f, 1.0f, -10.0f), ab);
        CHECK(dist(c, vec3(1.0f, 1.0f, -3.0f)) < 1e-5f, "axis-aligned clamp gives (halfx, y, -halfz)");
        CHECK(std::fabs(maz::math::distanceToObb(vec3(5, 0, 0), ab) - 4.0f) < 1e-5f, "distance along +x is 4");
    }

    // --- 4. Idempotence + sphere-vs-OBB + determinism. ---
    {
        const vec3 p(7.0f, 3.0f, -2.0f);
        const vec3 c = maz::math::closestPointOnObb(p, box);
        CHECK(dist(maz::math::closestPointOnObb(c, box), c) < 1e-4f, "closest(closest(p)) == closest(p)");
        vec3 contact;
        const float d = maz::math::distanceToObb(p, box);
        CHECK(maz::math::sphereIntersectsObb(p, d + 0.01f, box, contact), "a sphere just larger than the gap overlaps");
        CHECK(!maz::math::sphereIntersectsObb(p, d - 0.01f, box, contact), "a sphere just smaller than the gap misses");
        CHECK(dist(maz::math::closestPointOnObb(p, box), maz::math::closestPointOnObb(p, box)) == 0.0f,
              "identical inputs produce identical output");
    }

    if (g_fail == 0) {
        std::printf("closestpointobb: OK — brute-force oracle, inside, analytic, idempotence, sphere, "
                    "determinism.\n");
        return 0;
    }
    std::printf("closestpointobb: %d failure(s).\n", g_fail);
    return 1;
}
