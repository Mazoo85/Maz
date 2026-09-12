// tests/render/meshsharpedges.cpp — verifies dihedral-angle / sharp-edge detection (render::detectSharpEdges).
// Ground truths: two coplanar triangles meet at a 180-degree (flat) dihedral and are never sharp; two triangles
// folded at a right angle meet at 90 degrees; a cube's twelve rim edges are all 90-degree creases while its six
// face-diagonal edges stay flat at 180; and the sharp threshold gates the count. Pure CPU, headless.
#include "maz/render/MeshSharpEdges.hpp"

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

static shapes::MeshData cube() {
    shapes::MeshData m;
    m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,1,0), vtx(0,1,0),
                  vtx(0,0,1), vtx(1,0,1), vtx(1,1,1), vtx(0,1,1)};
    m.indices = {0,2,1, 0,3,2,  4,5,6, 4,6,7,  0,1,5, 0,5,4,
                 3,7,6, 3,6,2,  0,4,7, 0,7,3,  1,2,6, 1,6,5};
    return m;
}

int main() {
    // --- 1. Two coplanar triangles: one flat interior edge, never sharp. ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,1,0), vtx(0,1,0)};
        m.indices = {0,1,2, 0,2,3};
        const SharpEdgeResult r = detectSharpEdges(m, 30.0f);
        CHECK(r.interiorEdgeCount() == 1, "the quad has one interior (shared) edge");
        CHECK(near(r.edges[0].dihedralDegrees, 180.0f, 1e-2f), "coplanar faces meet at 180 degrees (flat)");
        CHECK(r.sharpCount() == 0, "a flat edge is not sharp");
    }

    // --- 2. Right-angle fold: the shared edge reads 90 degrees and is flagged sharp. ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(0,1,0), vtx(0,0,1)};
        m.indices = {0,1,2,  1,0,3}; // tri A in z=0 plane, tri B in y=0 plane, sharing edge 0-1
        const SharpEdgeResult r = detectSharpEdges(m, 30.0f);
        CHECK(r.interiorEdgeCount() == 1, "the fold has one shared edge");
        CHECK(near(r.edges[0].dihedralDegrees, 90.0f, 1e-2f), "a right-angle fold measures 90 degrees");
        CHECK(near(r.edges[0].sharpnessDegrees, 90.0f, 1e-2f), "sharpness is 180 - dihedral = 90");
        CHECK(r.sharpCount() == 1, "the fold is flagged sharp at a 30-degree threshold");
    }

    // --- 3. Cube: 12 rim creases at 90 degrees, 6 flat face diagonals at 180. ---
    {
        const SharpEdgeResult r = detectSharpEdges(cube(), 30.0f);
        CHECK(r.interiorEdgeCount() == 18, "the cube has 18 interior edges (12 rims + 6 diagonals)");
        CHECK(r.sharpCount() == 12, "the twelve rim edges are sharp");
        CHECK(near(r.minDihedral, 90.0f, 1e-2f), "the sharpest cube edge is a 90-degree rim");
        int flat = 0, rims = 0;
        for (const auto& e : r.edges) {
            if (near(e.dihedralDegrees, 180.0f, 1e-2f)) ++flat;
            else if (near(e.dihedralDegrees, 90.0f, 1e-2f)) ++rims;
        }
        CHECK(flat == 6 && rims == 12, "six diagonals stay flat, twelve rims fold 90 degrees");
    }

    // --- 4. Threshold gates the sharp count. ---
    {
        const shapes::MeshData c = cube();
        CHECK(detectSharpEdges(c, 45.0f).sharpCount() == 12, "90-degree rims are sharp at a 45-degree threshold");
        CHECK(detectSharpEdges(c, 100.0f).sharpCount() == 0, "no edge is sharp when the threshold exceeds 90");
    }

    // --- 5. Empty mesh is safe. ---
    {
        const SharpEdgeResult r = detectSharpEdges(shapes::MeshData{}, 30.0f);
        CHECK(r.edges.empty() && r.sharp.empty() && near(r.minDihedral, 180.0f, 1e-6f), "empty mesh -> nothing");
    }

    if (g_fail == 0) {
        std::printf("meshsharpedges: OK — flat=180, right fold=90, cube 12 rims sharp + 6 flat diagonals, threshold gates.\n");
        return 0;
    }
    std::printf("meshsharpedges: %d failure(s).\n", g_fail);
    return 1;
}
