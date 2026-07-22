// tests/render/meshextrude.cpp — verifies per-face extrude (render::extrudeFaces). Ground truths: each triangle
// becomes 6 vertices and 7 triangles (a raised top + 3 side-wall quads); the base ring stays put while the raised
// ring moves out along the face normal by `distance`; the top face keeps the original facing; the bbox grows by
// `distance` along the normal. Pure CPU, headless.
#include "maz/render/MeshExtrude.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

static MeshVertex at(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}
// y-component of a triangle's geometric normal (cross of its XZ-plane edges).
static float faceNy(const shapes::MeshData& m, std::size_t t) {
    const MeshVertex& a = m.vertices[m.indices[t * 3 + 0]];
    const MeshVertex& b = m.vertices[m.indices[t * 3 + 1]];
    const MeshVertex& c = m.vertices[m.indices[t * 3 + 2]];
    const float ex1 = b.px - a.px, ez1 = b.pz - a.pz;
    const float ex2 = c.px - a.px, ez2 = c.pz - a.pz;
    return ez1 * ex2 - ex1 * ez2;
}

// One triangle in the XZ plane facing +Y: (0,0,0),(0,0,1),(1,0,0).
static shapes::MeshData tri() {
    shapes::MeshData m;
    m.vertices = {at(0, 0, 0), at(0, 0, 1), at(1, 0, 0)};
    m.indices = {0, 1, 2};
    return m;
}

int main() {
    const shapes::MeshData base = tri();
    CHECK(faceNy(base, 0) > 0.0f, "the base triangle faces +Y");

    const shapes::MeshData e = extrudeFaces(base, 0.5f);

    // --- 1. One triangle -> 6 vertices, 7 triangles. ---
    {
        CHECK(e.vertices.size() == 6, "each triangle makes 6 vertices (base ring + raised ring)");
        CHECK(e.indices.size() == 7 * 3, "each triangle makes 7 triangles (1 top + 6 side)");
    }

    // --- 2. The base ring stays at y=0; the raised ring is pushed to y=0.5 along +Y. ---
    {
        CHECK(near(e.vertices[0].py, 0.0f, 1e-6f) && near(e.vertices[1].py, 0.0f, 1e-6f), "base ring stays at y=0");
        CHECK(near(e.vertices[3].py, 0.5f, 1e-5f) && near(e.vertices[4].py, 0.5f, 1e-5f), "raised ring moves to y=0.5");
        // The raised ring keeps the base X/Z (moved only along the normal).
        CHECK(near(e.vertices[3].px, e.vertices[0].px, 1e-6f) && near(e.vertices[3].pz, e.vertices[0].pz, 1e-6f),
              "raised corners sit directly above their base corners");
    }

    // --- 3. The top face (triangle 0 of the output) still faces +Y. ---
    {
        CHECK(faceNy(e, 0) > 0.0f, "the raised top face keeps the +Y facing");
    }

    // --- 4. The bbox grows by `distance` along the normal (max y goes 0 -> 0.5). ---
    {
        float maxY = e.vertices[0].py;
        for (const MeshVertex& v : e.vertices) if (v.py > maxY) maxY = v.py;
        CHECK(near(maxY, 0.5f, 1e-5f), "the extruded prism reaches y=0.5");
    }

    // --- 5. A negative distance presses inward (raised ring goes to y=-0.5). ---
    {
        const shapes::MeshData ei = extrudeFaces(base, -0.5f);
        CHECK(near(ei.vertices[3].py, -0.5f, 1e-5f), "negative distance extrudes inward");
    }

    // --- 6. Empty mesh is safe. ---
    {
        CHECK(extrudeFaces(shapes::MeshData{}, 0.5f).vertices.empty(), "empty -> empty");
    }

    if (g_fail == 0) {
        std::printf("meshextrude: OK — 1 tri -> 6 verts / 7 tris, base fixed, top raised along the normal.\n");
        return 0;
    }
    std::printf("meshextrude: %d failure(s).\n", g_fail);
    return 1;
}
