// tests/render/trail.cpp — verifies the motion-trail ribbon builder (render Trail.hpp).
// Ground truths, deterministic (no <random>, no clock):
//   * a ribbon has 2 vertices per history point and 2*(n-1) triangles;
//   * each cross-section is symmetric about its trail point (midpoint of the two edge vertices == the point),
//     and the edge direction is perpendicular to both the trail tangent and the view direction;
//   * width TAPERS by age: the newest (head) cross-section is widest and it shrinks toward the oldest (tail);
//   * update() expires points older than the lifetime; maxPoints caps the history;
//   * fewer than two points yields an empty ribbon.
#include "maz/render/Trail.hpp"

#include <cmath>
#include <cstdio>

using maz::math::vec3;
using maz::render::RibbonMesh;
using maz::render::Trail;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float dot(const vec3& a, const vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static float len(const vec3& a) { return std::sqrt(dot(a, a)); }

int main() {
    // --- 1. Counts, symmetry, perpendicularity on a straight fresh trail. ---
    {
        Trail t(2.0f, 1.0f);
        for (int i = 0; i < 10; ++i) t.push(vec3{static_cast<float>(i), 0.0f, 0.0f}); // along +X
        const vec3 view{0.0f, 0.0f, 1.0f};
        const RibbonMesh m = t.buildRibbon(view);
        CHECK(m.vertices.size() == 20, "2 vertices per history point");
        CHECK(m.indices.size() == static_cast<std::size_t>(2 * (10 - 1) * 3), "2*(n-1) triangles");

        bool ok = true;
        for (int i = 0; i < 10; ++i) {
            const vec3 L = m.vertices[static_cast<std::size_t>(2 * i)];
            const vec3 R = m.vertices[static_cast<std::size_t>(2 * i + 1)];
            const vec3 mid{(L.x + R.x) * 0.5f, (L.y + R.y) * 0.5f, (L.z + R.z) * 0.5f};
            if (std::fabs(mid.x - static_cast<float>(i)) > 1e-4f || std::fabs(mid.y) > 1e-4f ||
                std::fabs(mid.z) > 1e-4f) ok = false;
            const vec3 edge{L.x - R.x, L.y - R.y, L.z - R.z};
            const vec3 tangent{1.0f, 0.0f, 0.0f};
            if (std::fabs(dot(edge, tangent)) > 1e-4f || std::fabs(dot(edge, view)) > 1e-4f) ok = false;
        }
        CHECK(ok, "cross-sections centred on the point and perpendicular to tangent + view");
        // All points fresh -> uniform full width (1.0).
        const vec3 L0 = m.vertices[0], R0 = m.vertices[1];
        CHECK(std::fabs(len(vec3{L0.x - R0.x, L0.y - R0.y, L0.z - R0.z}) - 1.0f) < 1e-4f,
              "fresh trail is full width");
    }

    // --- 2. Width tapers with age. ---
    {
        Trail t(2.0f, 1.0f);
        // Push + age so the oldest point (front) is oldest, newest (back) is freshest.
        for (int i = 0; i < 10; ++i) {
            t.push(vec3{static_cast<float>(i), 0.0f, 0.0f});
            t.update(0.1f);
        }
        const RibbonMesh m = t.buildRibbon(vec3{0.0f, 0.0f, 1.0f});
        const std::size_t n = t.pointCount();
        // Width at cross-section i (front=oldest -> back=newest) should be non-decreasing.
        bool increasing = true;
        float prev = -1.0f;
        for (std::size_t i = 0; i < n; ++i) {
            const vec3 L = m.vertices[2 * i], R = m.vertices[2 * i + 1];
            const float wdt = len(vec3{L.x - R.x, L.y - R.y, L.z - R.z});
            if (wdt + 1e-5f < prev) increasing = false;
            prev = wdt;
        }
        CHECK(increasing, "width grows from the old tail toward the fresh head");
        // Head (newest) clearly wider than tail (oldest).
        const vec3 Lh = m.vertices[2 * (n - 1)], Rh = m.vertices[2 * (n - 1) + 1];
        const vec3 Lt = m.vertices[0], Rt = m.vertices[1];
        CHECK(len(vec3{Lh.x - Rh.x, Lh.y - Rh.y, Lh.z - Rh.z}) >
                  len(vec3{Lt.x - Rt.x, Lt.y - Rt.y, Lt.z - Rt.z}) + 0.1f,
              "the head is clearly wider than the tail");
    }

    // --- 3. Lifetime expiry + maxPoints cap. ---
    {
        Trail t(1.0f, 1.0f, 4);
        for (int i = 0; i < 10; ++i) t.push(vec3{static_cast<float>(i), 0.0f, 0.0f});
        CHECK(t.pointCount() == 4, "maxPoints caps the history");
        t.update(2.0f); // older than lifetime 1.0 -> all expire
        CHECK(t.pointCount() == 0, "points older than the lifetime expire");
        CHECK(t.buildRibbon(vec3{0.0f, 0.0f, 1.0f}).vertices.empty(), "empty trail -> empty ribbon");
    }

    // --- 4. Fewer than two points. ---
    {
        Trail t(2.0f, 1.0f);
        t.push(vec3{0.0f, 0.0f, 0.0f});
        CHECK(t.buildRibbon(vec3{0.0f, 0.0f, 1.0f}).vertices.empty(), "single point -> empty ribbon");
    }

    if (g_fail == 0) {
        std::printf("trail: OK — counts, symmetry/perpendicularity, taper, expiry/cap, degenerate.\n");
        return 0;
    }
    std::printf("trail: %d failure(s).\n", g_fail);
    return 1;
}
