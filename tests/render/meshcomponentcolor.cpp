// tests/render/meshcomponentcolor.cpp — verifies per-component tinting (render::tintComponents). Ground truths:
// vertices in the same connected component all get the identical colour; different components get different
// colours; the reported component count is right; a single-piece mesh is one uniform colour; it's deterministic;
// positions are untouched. Pure CPU, headless.
#include "maz/render/MeshComponentColor.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }
static bool sameColor(const MeshVertex& a, const MeshVertex& b) {
    return near(a.r, b.r, 1e-6f) && near(a.g, b.g, 1e-6f) && near(a.b, b.b, 1e-6f);
}

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 0.5f; return v;
}

// Two disjoint triangles (vertices 0,1,2 and 3,4,5 share no index) -> two components.
static shapes::MeshData twoTriangles() {
    shapes::MeshData m;
    m.vertices = {vtx(0, 0, 0), vtx(1, 0, 0), vtx(0, 1, 0),        // component A
                  vtx(10, 0, 0), vtx(11, 0, 0), vtx(10, 1, 0)};    // component B (far away)
    m.indices = {0, 1, 2, 3, 4, 5};
    return m;
}

int main() {
    // --- 1. Two disjoint triangles -> 2 components; each is internally one colour, and the two differ. ---
    {
        std::uint32_t count = 0;
        const shapes::MeshData t = tintComponents(twoTriangles(), 0.7f, 0.9f, &count);
        CHECK(count == 2, "two disjoint triangles report two components");
        CHECK(sameColor(t.vertices[0], t.vertices[1]) && sameColor(t.vertices[1], t.vertices[2]),
              "component A's three vertices share one colour");
        CHECK(sameColor(t.vertices[3], t.vertices[4]) && sameColor(t.vertices[4], t.vertices[5]),
              "component B's three vertices share one colour");
        CHECK(!sameColor(t.vertices[0], t.vertices[3]), "the two components get different colours");
    }

    // --- 2. Positions are untouched. ---
    {
        const shapes::MeshData src = twoTriangles();
        const shapes::MeshData t = tintComponents(src);
        bool posSame = true;
        for (std::size_t i = 0; i < src.vertices.size(); ++i)
            if (!near(t.vertices[i].px, src.vertices[i].px, 1e-6f) ||
                !near(t.vertices[i].py, src.vertices[i].py, 1e-6f)) posSame = false;
        CHECK(posSame, "tinting only changes colour, not geometry");
    }

    // --- 3. A single connected piece is one uniform colour (1 component). ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(0, 0, 0), vtx(1, 0, 0), vtx(0, 1, 0), vtx(1, 1, 0)};
        m.indices = {0, 1, 2, 1, 3, 2}; // a shared-vertex quad -> one component
        std::uint32_t count = 0;
        const shapes::MeshData t = tintComponents(m, 0.7f, 0.9f, &count);
        CHECK(count == 1, "a connected quad is one component");
        bool uniform = true;
        for (const MeshVertex& v : t.vertices)
            if (!sameColor(v, t.vertices[0])) uniform = false;
        CHECK(uniform, "one component -> one uniform colour");
    }

    // --- 4. Deterministic: two runs give identical colours. ---
    {
        const shapes::MeshData a = tintComponents(twoTriangles());
        const shapes::MeshData b = tintComponents(twoTriangles());
        bool same = true;
        for (std::size_t i = 0; i < a.vertices.size(); ++i)
            if (!sameColor(a.vertices[i], b.vertices[i])) same = false;
        CHECK(same, "the tint is deterministic");
    }

    // --- 5. Empty mesh is safe. ---
    {
        std::uint32_t count = 99;
        const shapes::MeshData t = tintComponents(shapes::MeshData{}, 0.7f, 0.9f, &count);
        CHECK(t.vertices.empty() && count == 0, "empty -> empty, zero components");
    }

    if (g_fail == 0) {
        std::printf("meshcomponentcolor: OK — each island one colour, distinct across islands, deterministic.\n");
        return 0;
    }
    std::printf("meshcomponentcolor: %d failure(s).\n", g_fail);
    return 1;
}
