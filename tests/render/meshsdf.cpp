// tests/render/meshsdf.cpp — verifies the signed-distance-field bake (render::bakeMeshSdf / sampleMeshSdf)
// against a unit cube's closed-form distance: the centre is deepest inside (-0.5), a point 0.5 outside the +X
// face is +0.5, a point on a face is ~0, the sign flips across the surface, and the field's most-negative
// value is about -0.5 (the inradius). Pure CPU, headless.
#include "maz/render/MeshSdf.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}

static shapes::MeshData weldedCube() {
    shapes::MeshData m;
    m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,1,0), vtx(0,1,0),
                  vtx(0,0,1), vtx(1,0,1), vtx(1,1,1), vtx(0,1,1)};
    m.indices = {
        0,1,2, 2,3,0,   1,5,6, 6,2,1,   5,4,7, 7,6,5,
        4,0,3, 3,7,4,   3,2,6, 6,7,3,   4,5,1, 1,0,4,
    };
    return m;
}

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

int main() {
    const MeshSdf sdf = bakeMeshSdf(weldedCube(), 33, 0.75f); // fine grid, enough padding to hold the probes
    CHECK(!sdf.dist.empty(), "sdf baked");
    const float tol = sdf.cellSize * 1.5f; // trilinear sampling accuracy ~ a cell

    // --- 1. Centre of the unit cube: deepest interior, distance -0.5 to the nearest face. ---
    CHECK(near(sampleMeshSdf(sdf, maz::math::vec3(0.5f, 0.5f, 0.5f)), -0.5f, tol),
          "cube centre signed distance is -0.5");

    // --- 2. A point 0.5 outside the +X face: +0.5. ---
    CHECK(near(sampleMeshSdf(sdf, maz::math::vec3(1.5f, 0.5f, 0.5f)), 0.5f, tol),
          "0.5 outside the +X face is +0.5");

    // --- 3. A point on a face samples ~0. ---
    CHECK(std::fabs(sampleMeshSdf(sdf, maz::math::vec3(1.0f, 0.5f, 0.5f))) <= tol, "on the +X face is ~0");

    // --- 4. Sign flips across the surface: inside negative, outside positive. ---
    CHECK(sampleMeshSdf(sdf, maz::math::vec3(0.5f, 0.5f, 0.5f)) < 0.0f, "interior is negative");
    CHECK(sampleMeshSdf(sdf, maz::math::vec3(1.4f, 0.5f, 0.5f)) > 0.0f, "exterior is positive");
    CHECK(sampleMeshSdf(sdf, maz::math::vec3(0.5f, 1.4f, 0.5f)) > 0.0f, "exterior (+Y) is positive");

    // --- 5. The most-negative grid value is about -0.5 (the cube's inradius). ---
    {
        float mn = 1e30f;
        for (float d : sdf.dist) mn = std::min(mn, d);
        CHECK(near(mn, -0.5f, sdf.cellSize * 1.5f), "deepest interior distance is about -0.5");
    }

    // --- 6. Degenerate input is safe. ---
    CHECK(bakeMeshSdf(shapes::MeshData{}, 16, 0.5f).dist.empty(), "empty mesh -> empty field");

    if (g_fail == 0) {
        std::printf("meshsdf: OK — cube SDF matches closed form: centre -0.5, outside +, surface ~0.\n");
        return 0;
    }
    std::printf("meshsdf: %d failure(s).\n", g_fail);
    return 1;
}
