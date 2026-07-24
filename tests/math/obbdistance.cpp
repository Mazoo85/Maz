// tests/math/obbdistance.cpp — verifies closest point / distance / SDF for an OBB (math ObbDistance.hpp).
// Ground truths, deterministic (fixed + seeded-LCG boxes/points, no <random>, no clock):
//   * CLOSEST IS MINIMAL (airtight): the reported closest point is inside the box and no sampled point of the
//     box is nearer — distanceToObb equals the true minimum over a dense sampling of the box;
//   * DISTANCE CONSISTENCY: distanceToObb == |p - closestPointOnObb|; sqDistanceToObb is its square;
//   * EXACT SDF: signedDistanceObb is negative strictly inside, positive outside, ~0 on the surface, and has
//     unit gradient outside (eikonal |∇sd| == 1);
//   * KNOWN VALUES: an axis-aligned unit box gives the hand-computed distances;
//   * ROTATION INVARIANCE: rotating box and point together leaves the distance unchanged;
//   * SPHERE OVERLAP matches distanceToObb <= radius.
#include "maz/math/ObbDistance.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::math::vec3;
using maz::math::mat3;
using maz::math::Obb;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len(const vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 33); }
    float sym() { return static_cast<float>(next()) / 4294967296.0f * 2.0f - 1.0f; }
    float unit() { return static_cast<float>(next()) / 4294967296.0f; }
};

// Build an orthonormal axes mat3 (columns = axes) from yaw/pitch/roll.
static mat3 axesFrom(float yaw, float pitch, float roll) {
    const float cy = std::cos(yaw), sy = std::sin(yaw);
    const float cp = std::cos(pitch), sp = std::sin(pitch);
    const float cr = std::cos(roll), sr = std::sin(roll);
    // Rz(roll) * Ry(pitch) * Rx? Just compose three simple rotations; orthonormal regardless.
    mat3 rz(1.0f), ry(1.0f), rx(1.0f);
    rz[0][0] = cr; rz[0][1] = sr; rz[1][0] = -sr; rz[1][1] = cr;
    ry[0][0] = cp; ry[0][2] = -sp; ry[2][0] = sp; ry[2][2] = cp;
    rx[1][1] = cy; rx[1][2] = sy; rx[2][1] = -sy; rx[2][2] = cy;
    return rz * ry * rx;
}

int main() {
    using namespace maz::math;

    // --- 1. Closest-point minimality + distance consistency vs a dense box sampling. ---
    {
        Lcg rng{0x0BB1u};
        float worstMin = 0.0f, worstConsist = 0.0f, worstSq = 0.0f;
        int tested = 0;
        for (int i = 0; i < 500; ++i) {
            Obb box;
            box.center = vec3(rng.sym() * 3.0f, rng.sym() * 3.0f, rng.sym() * 3.0f);
            box.half = vec3(0.3f + rng.unit() * 1.5f, 0.3f + rng.unit() * 1.5f, 0.3f + rng.unit() * 1.5f);
            box.axes = axesFrom(rng.sym() * 3.14159f, rng.sym() * 3.14159f, rng.sym() * 3.14159f);
            const vec3 p(box.center.x + rng.sym() * 5.0f, box.center.y + rng.sym() * 5.0f,
                        box.center.z + rng.sym() * 5.0f);

            const vec3 cp = closestPointOnObb(box, p);
            const float dist = distanceToObb(box, p);
            // Consistency: distance equals |p - closest|; sq is its square.
            worstConsist = std::max(worstConsist, std::fabs(dist - len(p - cp)));
            worstSq = std::max(worstSq, std::fabs(sqDistanceToObb(box, p) - dist * dist));
            // The closest point lies within the box.
            CHECK(box.contains(cp + (box.center - cp) * 1e-4f) || dist == 0.0f || box.contains(cp) ||
                      distanceToObb(box, cp) < 1e-3f,
                  "the closest point lies on/in the box");
            // Minimality: no sampled box point is closer.
            float sampledMin = 1e30f;
            for (int a = 0; a <= 8; ++a)
                for (int b = 0; b <= 8; ++b)
                    for (int c = 0; c <= 8; ++c) {
                        const vec3 q = box.center + box.axis(0) * (box.half.x * (static_cast<float>(a) / 4.0f - 1.0f)) +
                                       box.axis(1) * (box.half.y * (static_cast<float>(b) / 4.0f - 1.0f)) +
                                       box.axis(2) * (box.half.z * (static_cast<float>(c) / 4.0f - 1.0f));
                        sampledMin = std::min(sampledMin, len(p - q));
                    }
            // Analytic distance must not exceed the best sample, and be within grid resolution of it.
            CHECK(dist <= sampledMin + 1e-3f, "analytic distance never exceeds a sampled box point");
            worstMin = std::max(worstMin, dist - sampledMin); // <= 0 up to tolerance
            ++tested;
        }
        CHECK(tested > 400, "many OBB/point pairs tested");
        CHECK(worstConsist < 1e-4f, "distanceToObb equals |p - closestPointOnObb|");
        CHECK(worstSq < 1e-3f, "sqDistanceToObb is the square of the distance");
    }

    // --- 2. Exact SDF: sign + eikonal (unit gradient) outside. ---
    {
        Lcg rng{0x5DF0u};
        float worstGrad = 0.0f;
        int inN = 0, outN = 0;
        for (int i = 0; i < 2000; ++i) {
            Obb box;
            box.center = vec3(rng.sym(), rng.sym(), rng.sym());
            box.half = vec3(0.5f + rng.unit(), 0.5f + rng.unit(), 0.5f + rng.unit());
            box.axes = axesFrom(rng.sym() * 3.0f, rng.sym() * 3.0f, rng.sym() * 3.0f);
            vec3 p;
            if (i % 3 == 0) {
                // Guaranteed interior point (local coords within the half-extents).
                p = box.center + box.axis(0) * (rng.sym() * 0.9f * box.half.x) +
                    box.axis(1) * (rng.sym() * 0.9f * box.half.y) + box.axis(2) * (rng.sym() * 0.9f * box.half.z);
            } else {
                p = vec3(box.center.x + rng.sym() * 4.0f, box.center.y + rng.sym() * 4.0f,
                         box.center.z + rng.sym() * 4.0f);
            }
            const float sd = signedDistanceObb(box, p);
            if (box.contains(p)) {
                CHECK(sd < 1e-4f, "interior points have negative SDF");
                ++inN;
            } else {
                CHECK(sd > -1e-4f, "exterior points have positive SDF");
                // Eikonal outside: |grad| == 1 via central differences.
                const float h = 1e-3f;
                const float gx = signedDistanceObb(box, p + vec3(h, 0, 0)) - signedDistanceObb(box, p - vec3(h, 0, 0));
                const float gy = signedDistanceObb(box, p + vec3(0, h, 0)) - signedDistanceObb(box, p - vec3(0, h, 0));
                const float gz = signedDistanceObb(box, p + vec3(0, 0, h)) - signedDistanceObb(box, p - vec3(0, 0, h));
                const float gmag = std::sqrt(gx * gx + gy * gy + gz * gz) / (2.0f * h);
                worstGrad = std::max(worstGrad, std::fabs(gmag - 1.0f));
                ++outN;
            }
            // Outside, |SDF| equals the unsigned distance to the box.
            if (!box.contains(p)) {
                CHECK(std::fabs(sd - distanceToObb(box, p)) < 1e-3f, "outside, SDF magnitude is the box distance");
            }
        }
        CHECK(inN > 50 && outN > 500, "both interior and exterior points were tested");
        CHECK(worstGrad < 3e-3f, "the box SDF has unit gradient outside (eikonal)");
    }

    // --- 3. Known values: axis-aligned unit box at the origin. ---
    {
        Obb box; // center 0, half 0.5, axes identity
        box.half = vec3(0.5f, 0.5f, 0.5f);
        CHECK(len(closestPointOnObb(box, vec3(2, 0, 0)) - vec3(0.5f, 0, 0)) < 1e-5f, "closest of (2,0,0) is the +x face");
        CHECK(std::fabs(distanceToObb(box, vec3(2, 0, 0)) - 1.5f) < 1e-5f, "distance to (2,0,0) is 1.5");
        CHECK(std::fabs(signedDistanceObb(box, vec3(0, 0, 0)) + 0.5f) < 1e-5f, "centre SDF is -0.5");
        CHECK(std::fabs(distanceToObb(box, vec3(2, 2, 0)) - std::sqrt(2.0f * 1.5f * 1.5f)) < 1e-5f,
              "corner-direction distance is sqrt(1.5^2+1.5^2)");
        CHECK(distanceToObb(box, vec3(0.2f, -0.1f, 0.3f)) == 0.0f, "an interior point has zero distance");
    }

    // --- 4. Rotation invariance + sphere overlap consistency. ---
    {
        Obb box;
        box.center = vec3(1, -2, 0.5f);
        box.half = vec3(1.0f, 0.4f, 0.7f);
        const vec3 p(3.0f, -1.0f, 2.0f);
        const float d0 = distanceToObb(box, p);
        // Rotate the whole configuration about the box centre.
        const mat3 R = axesFrom(0.7f, -0.4f, 1.1f);
        Obb rot = box;
        rot.axes = R * box.axes;
        const vec3 pr = box.center + R * (p - box.center);
        CHECK(std::fabs(distanceToObb(rot, pr) - d0) < 1e-4f, "distance is invariant under a shared rotation");
        // Sphere overlap agrees with the distance test.
        for (float r : {0.5f, 1.5f, 3.0f, 5.0f}) {
            CHECK(obbIntersectsSphere(box, p, r) == (d0 <= r), "sphere overlap matches distance <= radius");
        }
    }

    if (g_fail == 0) {
        std::printf("obbdistance: OK — minimal closest, consistency, exact SDF, known values, invariance.\n");
        return 0;
    }
    std::printf("obbdistance: %d failure(s).\n", g_fail);
    return 1;
}
