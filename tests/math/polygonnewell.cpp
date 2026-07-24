// tests/math/polygonnewell.cpp — verifies Newell's polygon normal/area/centroid (math PolygonNewell.hpp).
// Ground truths, deterministic (fixed + seeded-LCG polygons, no <random>, no clock):
//   * PLANAR NORMAL (airtight): for a polygon built in a known plane, the Newell normal equals that plane's
//     normal (unit), regardless of vertex count;
//   * AREA vs INDEPENDENT SHOELACE (airtight): the area matches a shoelace computed in the polygon's own
//     in-plane 2D frame — a different formula from Newell's edge sum;
//   * REGULAR POLYGON (closed form): a regular n-gon of circumradius R has area 0.5*n*R^2*sin(2pi/n);
//   * CENTROID: a symmetric polygon's centroid is its centre; the unit square gives area 1 + centre (0.5,0.5);
//   * INVARIANCE: area is translation/rotation invariant; reversing the winding flips the normal.
#include "maz/math/PolygonNewell.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::math::vec3;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len(const vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
static float dot3(const vec3& a, const vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 33); }
    float unit() { return static_cast<float>(next()) / 4294967296.0f; }
};

// Independent in-plane shoelace: build an orthonormal basis (u,v) spanning the polygon's plane from its
// normal, project each vertex to 2D, and run the standard 2D shoelace. Different arithmetic from Newell.
static float shoelaceInPlane(const std::vector<vec3>& poly, const vec3& normal, const vec3& origin) {
    vec3 ref(1, 0, 0);
    if (std::fabs(dot3(ref, normal)) > 0.9f) {
        ref = vec3(0, 1, 0);
    }
    vec3 u = ref - normal * dot3(ref, normal);
    u = u * (1.0f / len(u));
    const vec3 v(normal.y * u.z - normal.z * u.y, normal.z * u.x - normal.x * u.z, normal.x * u.y - normal.y * u.x);
    float area2 = 0.0f;
    const std::size_t m = poly.size();
    for (std::size_t i = 0; i < m; ++i) {
        const vec3 a = poly[i] - origin, b = poly[(i + 1) % m] - origin;
        const float ax = dot3(a, u), ay = dot3(a, v);
        const float bx = dot3(b, u), by = dot3(b, v);
        area2 += ax * by - bx * ay;
    }
    return std::fabs(area2) * 0.5f;
}

int main() {
    using namespace maz::math;

    // --- 1. Planar polygons in a tilted plane: Newell normal matches, area matches independent shoelace. ---
    {
        // A tilted orthonormal frame (origin o, in-plane axes u,v, normal n).
        const vec3 o(0.5f, -1.0f, 2.0f);
        vec3 n = vec3(0.3f, 0.8f, 0.5f);
        n = n * (1.0f / len(n));
        vec3 u(1, 0, 0);
        u = u - n * dot3(u, n);
        u = u * (1.0f / len(u));
        const vec3 v(n.y * u.z - n.z * u.y, n.z * u.x - n.x * u.z, n.x * u.y - n.y * u.x);

        Lcg rng{0x9E11u};
        float worstN = 0.0f, worstA = 0.0f;
        int tested = 0;
        for (int t = 0; t < 400; ++t) {
            const int k = 3 + static_cast<int>(rng.unit() * 7.0f); // 3..9 vertices
            // Evenly-spaced base angles with bounded jitter: strictly increasing AND spanning the full
            // circle, so the origin o is always inside and the winding is reliably CCW (normal = +n).
            std::vector<float> angs;
            for (int i = 0; i < k; ++i) {
                const float base = 6.2831853f * static_cast<float>(i) / static_cast<float>(k);
                angs.push_back(base + (rng.unit() * 0.8f) * (6.2831853f / static_cast<float>(k)));
            }
            std::vector<vec3> poly;
            for (int i = 0; i < k; ++i) {
                const float r = 0.5f + rng.unit() * 1.5f;
                poly.push_back(o + u * (r * std::cos(angs[static_cast<std::size_t>(i)])) +
                               v * (r * std::sin(angs[static_cast<std::size_t>(i)])));
            }
            const PolygonInfo info = polygonInfo3D(poly);
            if (!info.valid) {
                continue;
            }
            worstN = std::max(worstN, len(info.normal - n)); // CCW in (u,v) => +n
            worstA = std::max(worstA, std::fabs(info.area - shoelaceInPlane(poly, n, o)));
            ++tested;
        }
        CHECK(tested > 300, "many planar polygons were tested");
        CHECK(worstN < 2e-3f, "the Newell normal equals the plane normal for CCW polygons");
        CHECK(worstA < 2e-3f, "the Newell area matches an independent in-plane shoelace");
    }

    // --- 2. Regular n-gon closed-form area. ---
    {
        for (int k : {3, 4, 5, 6, 8, 12}) {
            const float R = 2.0f;
            std::vector<vec3> poly;
            for (int i = 0; i < k; ++i) {
                const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(k);
                poly.push_back(vec3(R * std::cos(a), R * std::sin(a), 3.0f)); // z=3 plane
            }
            const float want = 0.5f * static_cast<float>(k) * R * R * std::sin(6.2831853f / static_cast<float>(k));
            CHECK(std::fabs(polygonArea3D(poly) - want) < 1e-3f, "regular n-gon area matches the closed form");
        }
    }

    // --- 3. Unit square: area 1, normal +Z, centroid centre. ---
    {
        const std::vector<vec3> sq = {vec3(0, 0, 0), vec3(1, 0, 0), vec3(1, 1, 0), vec3(0, 1, 0)};
        const PolygonInfo info = polygonInfo3D(sq);
        CHECK(info.valid, "square is valid");
        CHECK(std::fabs(info.area - 1.0f) < 1e-5f, "unit square area is 1");
        CHECK(len(info.normal - vec3(0, 0, 1)) < 1e-5f, "CCW square normal is +Z");
        CHECK(len(info.centroid - vec3(0.5f, 0.5f, 0.0f)) < 1e-5f, "square centroid is its centre");
    }

    // --- 4. Concave polygon centroid via independent triangulation weighting. ---
    {
        // An L-shaped (concave) polygon in the z=0 plane.
        const std::vector<vec3> L = {vec3(0, 0, 0), vec3(2, 0, 0), vec3(2, 1, 0),
                                     vec3(1, 1, 0), vec3(1, 2, 0), vec3(0, 2, 0)};
        const PolygonInfo info = polygonInfo3D(L);
        CHECK(info.valid, "L polygon valid");
        CHECK(std::fabs(info.area - 3.0f) < 1e-4f, "L-shape area is 3");
        // Known centroid of this L (two unit-scaled rectangles): area-weighted centre.
        // Rect A: [0,2]x[0,1] area2 centroid(1,0.5); Rect B: [0,1]x[1,2] area1 centroid(0.5,1.5).
        const float cx = (2.0f * 1.0f + 1.0f * 0.5f) / 3.0f;
        const float cy = (2.0f * 0.5f + 1.0f * 1.5f) / 3.0f;
        CHECK(len(info.centroid - vec3(cx, cy, 0.0f)) < 1e-4f, "L-shape centroid matches rectangle decomposition");
    }

    // --- 5. Invariance: translation keeps area; reversing winding flips the normal. ---
    {
        const std::vector<vec3> poly = {vec3(0, 0, 1), vec3(3, 0, 1), vec3(3, 2, 1), vec3(0, 2, 1)};
        std::vector<vec3> shifted, reversed;
        for (const vec3& p : poly) {
            shifted.push_back(p + vec3(10, -5, 4));
        }
        for (std::size_t i = poly.size(); i-- > 0;) {
            reversed.push_back(poly[i]);
        }
        CHECK(std::fabs(polygonArea3D(poly) - polygonArea3D(shifted)) < 1e-4f, "area is translation invariant");
        CHECK(len(polygonNormal3D(poly) + polygonNormal3D(reversed)) < 1e-4f, "reversing winding flips the normal");
    }

    if (g_fail == 0) {
        std::printf("polygonnewell: OK — planar normal, shoelace area, regular n-gon, centroid, invariance.\n");
        return 0;
    }
    std::printf("polygonnewell: %d failure(s).\n", g_fail);
    return 1;
}
