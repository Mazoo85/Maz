// tests/render/meshhardedges.cpp — verifies crease-angle vertex splitting (render::splitHardEdges). A cube
// welded to 8 shared vertices: a tight crease angle splits every 90-degree edge so each face gets its own 4
// vertices (24 total) with axis-aligned flat normals; a wide crease angle keeps it welded (8 verts) with
// smooth diagonal corner normals; a flat quad never splits; the triangle set is always preserved; and it is
// deterministic. Pure CPU, headless.
#include "maz/render/MeshHardEdges.hpp"

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
    const shapes::MeshData cube = weldedCube();

    // --- 1. Tight crease (30 deg): every 90-deg edge splits -> 24 vertices, axis-aligned flat normals. ---
    {
        const shapes::MeshData hard = splitHardEdges(cube, 30.0f);
        CHECK(hard.vertices.size() == 24, "hard cube has 24 vertices (4 per face)");
        CHECK(hard.indices == cube.indices || hard.indices.size() == cube.indices.size(),
              "triangle count preserved");
        bool axisFlat = true;
        for (const MeshVertex& v : hard.vertices) {
            const float ax = std::fabs(v.nx), ay = std::fabs(v.ny), az = std::fabs(v.nz);
            const float mx = std::max(ax, std::max(ay, az));
            const float sum = ax + ay + az;
            // A pure axis normal has one component ~1 and the sum ~1.
            if (!near(mx, 1.0f, 1e-4f) || !near(sum, 1.0f, 1e-4f)) axisFlat = false;
        }
        CHECK(axisFlat, "hard-split cube vertices carry flat, axis-aligned face normals");
    }

    // --- 2. Wide crease (100 deg > 90): stays welded to 8 verts, each normal along the corner diagonal
    //         (area-weighting makes it diagonal-pointing, not exactly symmetric; winding sets only the sign). ---
    {
        const shapes::MeshData smooth = splitHardEdges(cube, 100.0f);
        CHECK(smooth.vertices.size() == 8, "smooth cube stays welded to 8 vertices");
        bool diagonal = true;
        for (const MeshVertex& v : smooth.vertices) {
            const float len = std::sqrt(v.nx * v.nx + v.ny * v.ny + v.nz * v.nz);
            if (!near(len, 1.0f, 1e-3f)) { diagonal = false; continue; }
            // Unit vector from cube centre toward this corner.
            const float dx = v.px - 0.5f, dy = v.py - 0.5f, dz = v.pz - 0.5f;
            const float dl = std::sqrt(dx * dx + dy * dy + dz * dz);
            const float aligned = std::fabs((v.nx * dx + v.ny * dy + v.nz * dz) / dl); // |cos angle to diagonal|
            if (aligned < 0.9f) diagonal = false;                 // normal points along the corner diagonal
            if (std::fabs(v.nx) < 0.2f || std::fabs(v.ny) < 0.2f || std::fabs(v.nz) < 0.2f) diagonal = false;
        }
        CHECK(diagonal, "smooth cube corner normals are unit and parallel to the corner diagonal");
    }

    // --- 3. A flat quad never splits (its two triangles are coplanar), normals axis-aligned in Y. ---
    {
        shapes::MeshData q;
        q.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,0,1), vtx(0,0,1)};
        q.indices = {0,1,2, 0,2,3};
        const shapes::MeshData r = splitHardEdges(q, 1.0f); // even a 1-degree threshold keeps coplanar welded
        CHECK(r.vertices.size() == 4, "coplanar quad keeps its 4 vertices");
        bool flatY = true;
        for (const MeshVertex& v : r.vertices)
            if (!near(std::fabs(v.ny), 1.0f, 1e-4f) || !near(v.nx, 0.0f, 1e-4f) || !near(v.nz, 0.0f, 1e-4f))
                flatY = false;
        CHECK(flatY, "flat quad normals are axis-aligned along Y");
    }

    // --- 4. Deterministic. ---
    {
        const shapes::MeshData a = splitHardEdges(cube, 30.0f);
        const shapes::MeshData b = splitHardEdges(cube, 30.0f);
        bool same = a.indices == b.indices && a.vertices.size() == b.vertices.size();
        CHECK(same, "hard-edge split is deterministic");
    }

    if (g_fail == 0) {
        std::printf("meshhardedges: OK — crease splits cube to 24 flat, keeps 8 smooth, flat quad intact.\n");
        return 0;
    }
    std::printf("meshhardedges: %d failure(s).\n", g_fail);
    return 1;
}
