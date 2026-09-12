// tests/render/uvdensity.cpp — verifies UV / texel-density analysis (render::analyzeUvDensity). A unit quad
// (two triangles) mapped to the unit UV square has uniform density 1 everywhere; scaling one triangle's UVs
// by 2x quadruples that triangle's density (area scales with the square) and leaves the other at 1; the world
// and UV area totals are exact; a degenerate (zero-3D-area) triangle is counted and excluded from the stats.
// Pure CPU, headless.
#include "maz/render/UvDensity.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::render::analyzeUvDensity;
using maz::render::UvDensityStats;
using maz::render::MeshVertex;
namespace shapes = maz::render::shapes;

static MeshVertex v(float x, float z, float u, float uv) {
    MeshVertex m{}; m.px = x; m.py = 0.0f; m.pz = z; m.u = u; m.v = uv; m.r = m.g = m.b = 1.0f; return m;
}
static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

int main() {
    // A unit quad in the XZ plane (world area 1), UVs = the unit square. Two triangles.
    // Triangle A = (0,1,2), triangle B = (0,2,3).
    {
        shapes::MeshData m;
        m.vertices = {v(0,0, 0,0), v(1,0, 1,0), v(1,1, 1,1), v(0,1, 0,1)};
        m.indices = {0,1,2, 0,2,3};
        const UvDensityStats s = analyzeUvDensity(m);
        CHECK(s.triangleCount == 2, "two triangles analyzed");
        CHECK(near(s.totalWorldArea, 1.0f, 1e-5f), "unit quad world area is 1");
        CHECK(near(s.totalUvArea, 1.0f, 1e-5f), "unit-square UVs cover area 1");
        CHECK(near(s.minDensity, 1.0f, 1e-5f) && near(s.maxDensity, 1.0f, 1e-5f), "uniform density 1");
        CHECK(near(s.avgDensity, 1.0f, 1e-5f), "average density 1");
        CHECK(near(s.uniformityRatio(), 1.0f, 1e-5f), "perfectly uniform (ratio 1)");
        CHECK(s.degenerateCount == 0, "no degenerate triangles");
    }

    // Same world quad, but triangle A's UVs scaled 2x about the origin -> its UV area is 4x -> density 4.
    {
        shapes::MeshData m;
        // A uses doubled UVs; B keeps unit UVs. Shared vertices 0 and 2 must match per triangle, so give A its
        // own vertices to carry the scaled UVs.
        m.vertices = {
            v(0,0, 0,0), v(1,0, 2,0), v(1,1, 2,2),   // triangle A (scaled UVs)
            v(0,0, 0,0), v(1,1, 1,1), v(0,1, 0,1),   // triangle B (unit UVs)
        };
        m.indices = {0,1,2, 3,4,5};
        const UvDensityStats s = analyzeUvDensity(m);
        // Each triangle has world area 0.5. A: uvArea 2 -> density 4. B: uvArea 0.5 -> density 1.
        CHECK(near(s.minDensity, 1.0f, 1e-4f), "unscaled triangle density 1");
        CHECK(near(s.maxDensity, 4.0f, 1e-4f), "2x-UV triangle density 4 (area scales as the square)");
        CHECK(near(s.uniformityRatio(), 4.0f, 1e-4f), "uniformity ratio is 4 (stretch flagged)");
        CHECK(near(s.totalWorldArea, 1.0f, 1e-5f), "total world area still 1");
        CHECK(near(s.totalUvArea, 2.5f, 1e-4f), "total UV area is 2.0 + 0.5");
        // World-area-weighted average of {4,1} at equal weights = 2.5.
        CHECK(near(s.avgDensity, 2.5f, 1e-4f), "area-weighted average density is 2.5");
    }

    // A degenerate triangle (all three verts collinear -> zero world area) is counted, excluded from stats.
    {
        shapes::MeshData m;
        m.vertices = {v(0,0, 0,0), v(1,0, 1,0), v(1,1, 1,1),   // good triangle
                      v(0,0, 0,0), v(1,0, 1,0), v(2,0, 2,0)};  // collinear in XZ -> zero area
        m.indices = {0,1,2, 3,4,5};
        const UvDensityStats s = analyzeUvDensity(m);
        CHECK(s.degenerateCount == 1, "one degenerate triangle counted");
        CHECK(near(s.triDensity[1], 0.0f, 1e-6f), "degenerate triangle has 0 recorded density");
        CHECK(near(s.minDensity, 1.0f, 1e-4f) && near(s.maxDensity, 1.0f, 1e-4f),
              "stats come only from the valid triangle");
    }

    // Empty mesh is safe.
    {
        const UvDensityStats s = analyzeUvDensity(shapes::MeshData{});
        CHECK(s.triangleCount == 0 && s.triDensity.empty(), "empty mesh -> empty stats");
    }

    if (g_fail == 0) {
        std::printf("uvdensity: OK — uniform density 1, 2x UVs give 4x density, areas + degenerates correct.\n");
        return 0;
    }
    std::printf("uvdensity: %d failure(s).\n", g_fail);
    return 1;
}
