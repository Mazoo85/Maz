// tests/render/meshrecenter.cpp — verifies pivot-snap / recentering (render::recenterMesh). Ground truths:
// bounding-box-center recentres a cube's centre to the origin; base mode puts the bottom face on y=0; centre of
// mass of a pyramid sits at 1/4 height (below the bbox centre); vertex-average matches the corner mean; an open
// mesh falls back from centre-of-mass to the average. Pure CPU, headless.
#include "maz/render/MeshRecenter.hpp"
#include "maz/render/MeshWinding.hpp" // makeWindingConsistent — prepare the closed pyramid

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
static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

static shapes::MeshData cube() { // unit cube [0,1]^3, closed
    shapes::MeshData m;
    m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,1,0), vtx(0,1,0),
                  vtx(0,0,1), vtx(1,0,1), vtx(1,1,1), vtx(0,1,1)};
    m.indices = {0,2,1, 0,3,2,  4,5,6, 4,6,7,  0,1,5, 0,5,4,
                 3,7,6, 3,6,2,  0,4,7, 0,7,3,  1,2,6, 1,6,5};
    return m;
}

// Bounding-box extents of a mesh.
static void bounds(const shapes::MeshData& m, maz::math::vec3& lo, maz::math::vec3& hi) {
    lo = maz::math::vec3(m.vertices[0].px, m.vertices[0].py, m.vertices[0].pz);
    hi = lo;
    for (const auto& v : m.vertices) {
        lo = maz::math::vec3(std::min(lo.x, v.px), std::min(lo.y, v.py), std::min(lo.z, v.pz));
        hi = maz::math::vec3(std::max(hi.x, v.px), std::max(hi.y, v.py), std::max(hi.z, v.pz));
    }
}

int main() {
    // --- 1. BBoxCenter: the cube's centre moves to the origin; offset == -pivot. ---
    {
        const RecenterResult r = recenterMesh(cube(), PivotMode::BBoxCenter);
        CHECK(near(r.pivot.x, 0.5f, 1e-6f) && near(r.pivot.y, 0.5f, 1e-6f) && near(r.pivot.z, 0.5f, 1e-6f),
              "the pivot is the cube's centre (0.5,0.5,0.5)");
        CHECK(near(r.offset.x, -0.5f, 1e-6f) && near(r.offset.y, -0.5f, 1e-6f), "offset is -pivot");
        maz::math::vec3 lo, hi;
        bounds(r.mesh, lo, hi);
        CHECK(near(lo.x, -0.5f, 1e-6f) && near(hi.x, 0.5f, 1e-6f), "recentred cube spans [-0.5,0.5]");
    }

    // --- 2. Base (up +Y): the bottom face lands on y=0, centred in x/z. ---
    {
        const RecenterResult r = recenterMesh(cube(), PivotMode::Base);
        CHECK(near(r.pivot.x, 0.5f, 1e-6f) && near(r.pivot.y, 0.0f, 1e-6f) && near(r.pivot.z, 0.5f, 1e-6f),
              "the base pivot is the bottom-centre (0.5,0,0.5)");
        maz::math::vec3 lo, hi;
        bounds(r.mesh, lo, hi);
        CHECK(near(lo.y, 0.0f, 1e-6f) && near(hi.y, 1.0f, 1e-6f), "the cube now stands on y=0");
        CHECK(near(lo.x, -0.5f, 1e-6f) && near(hi.x, 0.5f, 1e-6f), "and is centred in x");
    }

    // --- 3. Centre of mass of a square pyramid sits at 1/4 height (below the bbox centre at 1/2). ---
    {
        shapes::MeshData p;
        p.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,0,1), vtx(0,0,1), vtx(0.5f,1,0.5f)}; // base + apex
        p.indices = {0,1,2, 0,2,3,          // base
                     0,1,4, 1,2,4, 2,3,4, 3,0,4}; // sides
        const shapes::MeshData pyr = makeWindingConsistent(p);
        const RecenterResult com = recenterMesh(pyr, PivotMode::CenterOfMass);
        CHECK(!com.fellBack, "a closed pyramid has a real centre of mass");
        CHECK(near(com.pivot.y, 0.25f, 1e-3f), "the pyramid's centre of mass is at 1/4 height");
        const RecenterResult bb = recenterMesh(pyr, PivotMode::BBoxCenter);
        CHECK(near(bb.pivot.y, 0.5f, 1e-6f), "its bbox centre is at 1/2 height");
        CHECK(com.pivot.y < bb.pivot.y, "centre of mass is lower than the bbox centre (mass near the base)");
    }

    // --- 4. VertexAverage of the cube corners is (0.5,0.5,0.5). ---
    {
        const RecenterResult r = recenterMesh(cube(), PivotMode::VertexAverage);
        CHECK(near(r.pivot.x, 0.5f, 1e-6f) && near(r.pivot.y, 0.5f, 1e-6f), "corner average is the cube centre");
    }

    // --- 5. Centre of mass on an open mesh falls back to the vertex average. ---
    {
        shapes::MeshData open;
        open.vertices = {vtx(0,0,0), vtx(2,0,0), vtx(0,0,2)}; // a single triangle, no volume
        open.indices = {0,1,2};
        const RecenterResult r = recenterMesh(open, PivotMode::CenterOfMass);
        CHECK(r.fellBack, "an open mesh can't have a centre of mass -> fell back");
        CHECK(near(r.pivot.x, 2.0f / 3.0f, 1e-5f), "the fallback pivot is the vertex average");
    }

    // --- 6. Empty mesh is safe. ---
    {
        const RecenterResult r = recenterMesh(shapes::MeshData{});
        CHECK(r.mesh.vertices.empty(), "empty mesh -> empty result");
    }

    if (g_fail == 0) {
        std::printf("meshrecenter: OK — bbox centre to origin, base on y=0, pyramid CoM at 1/4 h, open-mesh fallback.\n");
        return 0;
    }
    std::printf("meshrecenter: %d failure(s).\n", g_fail);
    return 1;
}
