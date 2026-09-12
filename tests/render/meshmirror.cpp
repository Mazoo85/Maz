// tests/render/meshmirror.cpp — verifies mesh mirror / symmetrize (render::mirrorMesh). Ground truths of a
// mirror modifier: reflecting a half across a plane doubles the geometry into a symmetric whole; vertices
// exactly on the mirror plane are shared (not duplicated) so the seam is watertight; the reflected triangles
// carry REVERSED winding (and negated normals) so their faces still point outward; and every mirrored vertex
// sits at the reflected position of an original. Pure CPU, headless.
#include "maz/render/MeshMirror.hpp"

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

// Face normal of triangle t in a mesh (via its winding).
static void faceNormal(const shapes::MeshData& m, std::size_t t, float& nx, float& ny, float& nz) {
    const MeshVertex& A = m.vertices[m.indices[t * 3 + 0]];
    const MeshVertex& B = m.vertices[m.indices[t * 3 + 1]];
    const MeshVertex& C = m.vertices[m.indices[t * 3 + 2]];
    const float ux = B.px - A.px, uy = B.py - A.py, uz = B.pz - A.pz;
    const float vx = C.px - A.px, vy = C.py - A.py, vz = C.pz - A.pz;
    nx = uy * vz - uz * vy; ny = uz * vx - ux * vz; nz = ux * vy - uy * vx;
}

int main() {
    // --- 1. Triangle with one vertex on the mirror plane: seam shared, counts and bbox symmetric. ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,1,0)}; // vertex 0 sits on x=0
        m.indices = {0,1,2};
        const shapes::MeshData r = mirrorMesh(m, 0, 0.0f);
        CHECK(r.vertices.size() == 5, "one seam vertex shared -> 3 + 2 = 5 vertices");
        CHECK(r.indices.size() == 6, "two triangles after mirroring");
        float minx = 1e30f, maxx = -1e30f;
        for (const auto& v : r.vertices) { minx = std::min(minx, v.px); maxx = std::max(maxx, v.px); }
        CHECK(near(minx, -1.0f, 1e-5f) && near(maxx, 1.0f, 1e-5f), "bounding box is symmetric about x=0");
    }

    // --- 2. Winding reversal: a +X-facing triangle mirrors to a -X-facing one (both point outward). ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(1,0,0), vtx(1,1,0), vtx(1,0,1)}; // wound so its normal is +X
        m.indices = {0,1,2};
        float nx, ny, nz; faceNormal(m, 0, nx, ny, nz);
        CHECK(nx > 0.0f, "source triangle faces +X");
        const shapes::MeshData r = mirrorMesh(m, 0, 0.0f);
        CHECK(r.indices.size() == 6, "original + mirrored triangle");
        float mx, my, mz; faceNormal(r, 1, mx, my, mz); // triangle 1 is the mirrored one
        CHECK(mx < 0.0f, "mirrored triangle faces -X (winding reversed -> outward)");
    }

    // --- 3. No seam (all vertices off the plane): vertex and triangle counts exactly double. ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(1,0,0), vtx(2,0,0), vtx(1,1,0)};
        m.indices = {0,1,2};
        const shapes::MeshData r = mirrorMesh(m, 0, 0.0f);
        CHECK(r.vertices.size() == 6, "no seam -> vertices double to 6");
        CHECK(r.indices.size() == 6, "triangles double to 2");
    }

    // --- 4. Every mirrored vertex is the exact reflection of an original (across x=0). ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(1,2,3), vtx(2,0,1), vtx(1,1,0)};
        m.indices = {0,1,2};
        const shapes::MeshData r = mirrorMesh(m, 0, 0.0f);
        bool allReflected = true;
        for (const auto& v : r.vertices) {
            // Its mirror (-x, y, z) must also be present in the result.
            bool found = false;
            for (const auto& w : r.vertices)
                if (near(w.px, -v.px, 1e-5f) && near(w.py, v.py, 1e-5f) && near(w.pz, v.pz, 1e-5f)) found = true;
            if (!found) allReflected = false;
        }
        CHECK(allReflected, "the vertex set is symmetric across x=0");
    }

    // --- 5. Mirroring across Y works too (position and normal reflect on the Y axis). ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(0,1,0), vtx(1,1,0), vtx(0,1,1)};
        m.indices = {0,1,2};
        const shapes::MeshData r = mirrorMesh(m, 1, 0.0f);
        float miny = 1e30f, maxy = -1e30f;
        for (const auto& v : r.vertices) { miny = std::min(miny, v.py); maxy = std::max(maxy, v.py); }
        CHECK(near(miny, -1.0f, 1e-5f) && near(maxy, 1.0f, 1e-5f), "Y mirror gives symmetric Y bounds");
    }

    // --- 6. Empty mesh is safe. ---
    {
        const shapes::MeshData r = mirrorMesh(shapes::MeshData{}, 0, 0.0f);
        CHECK(r.vertices.empty() && r.indices.empty(), "empty mesh -> empty mesh");
    }

    if (g_fail == 0) {
        std::printf("meshmirror: OK — seam shared, winding reversed (+X->-X), counts double, symmetric across plane.\n");
        return 0;
    }
    std::printf("meshmirror: %d failure(s).\n", g_fail);
    return 1;
}
