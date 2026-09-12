// tests/render/meshfacet.cpp — verifies flat-shading facet split (render::facetMesh). Ground truths: the output
// has exactly 3 vertices per triangle (nothing shared), each vertex carries its own triangle's unit face normal,
// the drawn triangle positions are unchanged, and a cube's 12 triangles yield the 6 axis-aligned face normals.
// Pure CPU, headless.
#include "maz/render/MeshFacet.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.nx = 9; v.ny = 9; v.nz = 9; v.r = v.g = v.b = 1.0f; return v;
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

static std::vector<std::array<std::array<float,3>,3>> triPos(const shapes::MeshData& m) {
    std::vector<std::array<std::array<float,3>,3>> out;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        std::array<std::array<float,3>,3> tp{};
        for (int k = 0; k < 3; ++k) {
            const MeshVertex& v = m.vertices[m.indices[t + static_cast<std::size_t>(k)]];
            tp[static_cast<std::size_t>(k)] = {v.px, v.py, v.pz};
        }
        out.push_back(tp);
    }
    return out;
}

int main() {
    // --- 1. Single triangle in z=0 plane: 3 verts, all normals +Z, positions preserved. ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(0,1,0)};
        m.indices = {0,1,2};
        const shapes::MeshData f = facetMesh(m);
        CHECK(f.vertices.size() == 3 && f.indices.size() == 3, "one triangle -> 3 own vertices");
        for (const auto& v : f.vertices)
            CHECK(near(v.nx, 0.0f, 1e-6f) && near(v.ny, 0.0f, 1e-6f) && near(v.nz, 1.0f, 1e-6f),
                  "every vertex carries the +Z face normal");
        CHECK(triPos(f) == triPos(m), "drawn triangle positions are unchanged");
    }

    // --- 2. Vertex count is exactly 3 * triangleCount (nothing shared) and geometry is preserved. ---
    {
        const shapes::MeshData c = cube();
        const shapes::MeshData f = facetMesh(c);
        CHECK(f.vertices.size() == (c.indices.size() / 3) * 3, "faceted cube has 3 verts per triangle");
        CHECK(f.vertices.size() == 36, "cube -> 12 triangles * 3 = 36 vertices");
        CHECK(triPos(f) == triPos(c), "cube triangle positions unchanged");
    }

    // --- 3. Cube face normals are the six unit axes, and every normal is unit length. ---
    {
        const shapes::MeshData f = facetMesh(cube());
        bool px=false, nx=false, py=false, ny=false, pz=false, nz=false;
        bool allUnit = true;
        for (const auto& v : f.vertices) {
            const float len = std::sqrt(v.nx*v.nx + v.ny*v.ny + v.nz*v.nz);
            if (!near(len, 1.0f, 1e-5f)) allUnit = false;
            if (near(v.nx, 1.0f, 1e-4f)) px = true;
            if (near(v.nx, -1.0f, 1e-4f)) nx = true;
            if (near(v.ny, 1.0f, 1e-4f)) py = true;
            if (near(v.ny, -1.0f, 1e-4f)) ny = true;
            if (near(v.nz, 1.0f, 1e-4f)) pz = true;
            if (near(v.nz, -1.0f, 1e-4f)) nz = true;
        }
        CHECK(allUnit, "all face normals are unit length");
        CHECK(px && nx && py && ny && pz && nz, "all six axis-aligned face normals are present");
    }

    // --- 4. Empty mesh is safe. ---
    {
        const shapes::MeshData f = facetMesh(shapes::MeshData{});
        CHECK(f.vertices.empty() && f.indices.empty(), "empty mesh -> empty mesh");
    }

    if (g_fail == 0) {
        std::printf("meshfacet: OK — 3 verts/triangle, per-face normals, cube->36 verts + six axis normals.\n");
        return 0;
    }
    std::printf("meshfacet: %d failure(s).\n", g_fail);
    return 1;
}
