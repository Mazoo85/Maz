// tests/render/meshsurfacesample.cpp — verifies area-weighted surface sampling (render::sampleSurfacePoints).
// Ground truths: every point lies on the mesh (inside its reported triangle), the sample count is exact, a
// big triangle collects proportionally more points than a small one, sampling is deterministic per seed, and
// the reported normal is the face normal. Pure CPU, headless.
#include "maz/render/MeshSurfaceSample.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}
static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

int main() {
    // --- 1. Single triangle in z=0: exact count, points inside the triangle, +Z face normal. ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(0,1,0)}; // CCW -> normal +Z
        m.indices = {0,1,2};
        const std::vector<SurfacePoint> pts = sampleSurfacePoints(m, 2000, 12345);
        CHECK(pts.size() == 2000, "returns exactly the requested count");
        bool inside = true;
        bool normalOk = true;
        for (const auto& p : pts) {
            if (!near(p.position.z, 0.0f, 1e-5f)) inside = false;                // on the plane
            if (p.position.x < -1e-4f || p.position.y < -1e-4f) inside = false;  // non-negative bary
            if (p.position.x + p.position.y > 1.0f + 1e-4f) inside = false;      // within the triangle
            if (!(near(p.normal.z, 1.0f, 1e-5f) && near(p.normal.x, 0.0f, 1e-5f))) normalOk = false;
        }
        CHECK(inside, "every sample lies inside the triangle");
        CHECK(normalOk, "every sample carries the +Z face normal");
    }

    // --- 2. Area weighting: a triangle 100x bigger collects ~100x more points. ---
    {
        shapes::MeshData m;
        // Small triangle near origin (area 0.005) + big triangle (area 0.5), well separated.
        m.vertices = {vtx(0,0,0), vtx(0.1f,0,0), vtx(0,0.1f,0),          // tri 0: area 0.005
                      vtx(10,0,0), vtx(11,0,0), vtx(10,1,0)};            // tri 1: area 0.5
        m.indices = {0,1,2, 3,4,5};
        const std::vector<SurfacePoint> pts = sampleSurfacePoints(m, 20000, 7);
        std::size_t small = 0, big = 0;
        for (const auto& p : pts) {
            if (p.triangle == 0) ++small;
            else ++big;
        }
        CHECK(big > small * 20, "the 100x-larger triangle gets far more points");
        CHECK(small > 0, "the small triangle still gets some points");
    }

    // --- 3. Determinism: same seed -> identical, different seed -> different. ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(0,1,0)};
        m.indices = {0,1,2};
        const std::vector<SurfacePoint> a = sampleSurfacePoints(m, 100, 42);
        const std::vector<SurfacePoint> b = sampleSurfacePoints(m, 100, 42);
        const std::vector<SurfacePoint> c = sampleSurfacePoints(m, 100, 43);
        bool same = a.size() == b.size();
        for (std::size_t i = 0; i < a.size() && same; ++i)
            if (!near(a[i].position.x, b[i].position.x, 0.0f) || !near(a[i].position.y, b[i].position.y, 0.0f))
                same = false;
        CHECK(same, "same seed reproduces the exact same points");
        bool differ = false;
        for (std::size_t i = 0; i < a.size() && i < c.size(); ++i)
            if (!near(a[i].position.x, c[i].position.x, 1e-6f)) differ = true;
        CHECK(differ, "a different seed produces different points");
    }

    // --- 4. A unit-square quad: points spread across BOTH triangles and stay within [0,1]^2. ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,1,0), vtx(0,1,0)};
        m.indices = {0,1,2, 0,2,3};
        const std::vector<SurfacePoint> pts = sampleSurfacePoints(m, 4000, 99);
        bool inBounds = true;
        std::size_t t0 = 0, t1 = 0;
        for (const auto& p : pts) {
            if (p.position.x < -1e-4f || p.position.x > 1.0f + 1e-4f) inBounds = false;
            if (p.position.y < -1e-4f || p.position.y > 1.0f + 1e-4f) inBounds = false;
            if (p.triangle == 0) ++t0;
            else ++t1;
        }
        CHECK(inBounds, "all quad samples stay within the unit square");
        CHECK(t0 > 500 && t1 > 500, "both equal-area triangles are sampled");
    }

    // --- 5. Empty / zero-count are safe. ---
    {
        CHECK(sampleSurfacePoints(shapes::MeshData{}, 100).empty(), "empty mesh -> no points");
        shapes::MeshData m;
        m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(0,1,0)};
        m.indices = {0,1,2};
        CHECK(sampleSurfacePoints(m, 0).empty(), "zero count -> no points");
    }

    if (g_fail == 0) {
        std::printf("meshsurfacesample: OK — on-surface, area-weighted, deterministic, quad both tris, empty safe.\n");
        return 0;
    }
    std::printf("meshsurfacesample: %d failure(s).\n", g_fail);
    return 1;
}
