// tests/render/meshflip.cpp — verifies deliberate mesh flipping (render::flipWinding/flipNormals/flipMesh).
// Ground truths: flipWinding swaps corners 2 and 3 of every triangle (reversing its facing) without touching
// normals; flipNormals negates stored normals without touching indices; flipMesh does both; flipping twice
// restores the original. Pure CPU, headless.
#include "maz/render/MeshFlip.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex nvtx(float x, float y, float z, float nx, float ny, float nz) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.nx = nx; v.ny = ny; v.nz = nz; v.r = v.g = v.b = 1.0f; return v;
}
static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

// Two-triangle quad in the XZ plane with upward (+Y) normals; face normal via cross((b-a),(c-a)) is +Y.
static shapes::MeshData quad() {
    shapes::MeshData m;
    m.vertices = {nvtx(0,0,0, 0,1,0), nvtx(1,0,0, 0,1,0), nvtx(1,0,1, 0,1,0), nvtx(0,0,1, 0,1,0)};
    m.indices = {0,2,1, 0,3,2}; // wound so the geometric face normal is +Y
    return m;
}
// Geometric normal of triangle t (cross of its edges), y-component sign tells which way it faces.
static float faceNy(const shapes::MeshData& m, std::size_t t) {
    const MeshVertex& a = m.vertices[m.indices[t * 3 + 0]];
    const MeshVertex& b = m.vertices[m.indices[t * 3 + 1]];
    const MeshVertex& c = m.vertices[m.indices[t * 3 + 2]];
    const float ex1 = b.px - a.px, ez1 = b.pz - a.pz;
    const float ex2 = c.px - a.px, ez2 = c.pz - a.pz;
    return ez1 * ex2 - ex1 * ez2; // y-component of cross((b-a),(c-a)) for points in the XZ plane
}

int main() {
    const shapes::MeshData base = quad();
    const float baseNy = faceNy(base, 0);
    CHECK(baseNy > 0.0f, "the base quad faces +Y");

    // --- 1. flipWinding: swaps corners 2 and 3, reverses the geometric facing, leaves normals. ---
    {
        const shapes::MeshData f = flipWinding(base);
        CHECK(f.indices[1] == base.indices[2] && f.indices[2] == base.indices[1], "corners 2 and 3 swapped");
        CHECK(faceNy(f, 0) < 0.0f, "the geometric face now points the other way");
        CHECK(near(f.vertices[0].ny, 1.0f, 1e-6f), "stored normals are untouched by flipWinding");
    }

    // --- 2. flipNormals: negates stored normals, leaves indices. ---
    {
        const shapes::MeshData f = flipNormals(base);
        CHECK(near(f.vertices[0].ny, -1.0f, 1e-6f), "stored normals are negated");
        CHECK(f.indices == base.indices, "indices are untouched by flipNormals");
    }

    // --- 3. flipMesh: both — winding reversed AND normals negated. ---
    {
        const shapes::MeshData f = flipMesh(base);
        CHECK(faceNy(f, 0) < 0.0f, "geometric facing reversed");
        CHECK(near(f.vertices[0].ny, -1.0f, 1e-6f), "stored normals negated");
    }

    // --- 4. flipMesh twice restores the original winding and normals. ---
    {
        const shapes::MeshData f2 = flipMesh(flipMesh(base));
        CHECK(f2.indices == base.indices, "double flip restores winding");
        CHECK(near(f2.vertices[0].ny, 1.0f, 1e-6f), "double flip restores normals");
        CHECK(faceNy(f2, 0) > 0.0f, "and the facing is back to +Y");
    }

    // --- 5. Empty mesh is safe. ---
    {
        CHECK(flipMesh(shapes::MeshData{}).vertices.empty(), "empty mesh -> empty result");
    }

    if (g_fail == 0) {
        std::printf("meshflip: OK — winding reverses facing, normals negate, flipMesh does both, double flip restores.\n");
        return 0;
    }
    std::printf("meshflip: %d failure(s).\n", g_fail);
    return 1;
}
