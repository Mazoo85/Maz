// tests/render/meshwatertight.cpp — verifies the watertightness/hole report (render::analyzeWatertight).
// Ground truths: a closed cube is watertight with no holes; removing one face opens a single 4-edge hole of
// perimeter 4; removing two opposite faces opens two holes; an open plane has one boundary loop (its rim).
// Pure CPU, headless.
#include "maz/render/MeshWatertight.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}
static bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

// Unit cube; the first two triangles are the z=0 face (corners 0,1,2,3), the next two the z=1 face.
static shapes::MeshData cube() {
    shapes::MeshData m;
    m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,1,0), vtx(0,1,0),
                  vtx(0,0,1), vtx(1,0,1), vtx(1,1,1), vtx(0,1,1)};
    m.indices = {0,2,1, 0,3,2,  4,5,6, 4,6,7,  0,1,5, 0,5,4,
                 3,7,6, 3,6,2,  0,4,7, 0,7,3,  1,2,6, 1,6,5};
    return m;
}

int main() {
    // --- 1. Closed cube: watertight, no holes. ---
    {
        const WatertightReport r = analyzeWatertight(cube());
        CHECK(r.watertight, "a closed cube is watertight");
        CHECK(r.boundaryEdgeCount == 0 && r.nonManifoldEdgeCount == 0, "no open or non-manifold edges");
        CHECK(r.holeCount == 0 && r.holes.empty(), "a closed cube has no holes");
    }

    // --- 2. Cube with the z=0 face removed: one 4-edge hole of perimeter 4. ---
    {
        shapes::MeshData m = cube();
        m.indices.erase(m.indices.begin(), m.indices.begin() + 6); // drop the first two triangles (z=0 face)
        const WatertightReport r = analyzeWatertight(m);
        CHECK(!r.watertight, "removing a face breaks watertightness");
        CHECK(r.holeCount == 1, "exactly one hole opens");
        CHECK(r.boundaryEdgeCount == 4, "the hole exposes four boundary edges");
        CHECK(r.holes[0].edgeCount == 4, "the hole rim is a four-vertex loop");
        CHECK(near(r.holes[0].perimeter, 4.0, 1e-5), "the unit-face rim has perimeter 4");
        CHECK(near(r.largestHolePerimeter, 4.0, 1e-5), "largest-hole perimeter is reported");
    }

    // --- 3. Cube with z=0 and z=1 faces removed: two holes. ---
    {
        shapes::MeshData m = cube();
        m.indices.erase(m.indices.begin(), m.indices.begin() + 12); // drop the first four triangles (both z faces)
        const WatertightReport r = analyzeWatertight(m);
        CHECK(!r.watertight, "two open faces are not watertight");
        CHECK(r.holeCount == 2, "two separate holes are found");
        CHECK(r.boundaryEdgeCount == 8, "eight boundary edges total (four per hole)");
    }

    // --- 4. An open plane (single quad) has one boundary loop (its rim). ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(0,0,0), vtx(2,0,0), vtx(2,0,3), vtx(0,0,3)};
        m.indices = {0,1,2, 0,2,3};
        const WatertightReport r = analyzeWatertight(m);
        CHECK(!r.watertight, "a flat quad is an open surface");
        CHECK(r.holeCount == 1, "the quad has a single boundary loop");
        CHECK(near(r.holes[0].perimeter, 2.0 + 3.0 + 2.0 + 3.0, 1e-5), "rim perimeter matches the 2x3 rectangle");
    }

    // --- 5. Empty mesh is safe (vacuously watertight, no holes). ---
    {
        const WatertightReport r = analyzeWatertight(shapes::MeshData{});
        CHECK(r.watertight && r.holeCount == 0, "empty mesh -> watertight, no holes");
    }

    if (g_fail == 0) {
        std::printf("meshwatertight: OK — closed cube sealed, one/two faces removed -> 1/2 holes, quad rim perimeter.\n");
        return 0;
    }
    std::printf("meshwatertight: %d failure(s).\n", g_fail);
    return 1;
}
