// tests/render/meshboundaryloops.cpp — verifies boundary/hole edge-loop extraction
// (render::extractBoundaryLoops). A watertight cube has no boundary loops; a cube with one face removed has a
// single 4-vertex loop ringing the hole (exactly the removed face's corners); a flat quad has one 4-vertex
// perimeter loop; two disjoint quads give two loops; and every loop is closed (its chain returns to the
// start). Built on MeshTopology. Pure CPU, headless.
#include "maz/render/MeshBoundaryLoops.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}

// The 8-vertex welded cube. Face list is split so a single face can be omitted.
static shapes::MeshData cubeWithout(bool topFace) {
    shapes::MeshData m;
    m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,1,0), vtx(0,1,0),
                  vtx(0,0,1), vtx(1,0,1), vtx(1,1,1), vtx(0,1,1)};
    m.indices = {
        0,1,2, 2,3,0,   // front  z=0
        1,5,6, 6,2,1,   // right   x=1
        5,4,7, 7,6,5,   // back    z=1
        4,0,3, 3,7,4,   // left    x=0
        4,5,1, 1,0,4,   // bottom  y=0
    };
    if (!topFace) m.indices.insert(m.indices.end(), {3,2,6, 6,7,3}); // top y=1 (verts 2,3,6,7)
    return m;
}

int main() {
    // --- 1. Watertight cube: no boundary loops. ---
    {
        const auto loops = extractBoundaryLoops(cubeWithout(false));
        CHECK(loops.empty(), "closed cube has no boundary loops");
    }

    // --- 2. Cube missing its top face: exactly one 4-vertex loop around the hole = verts {2,3,6,7}. ---
    {
        const auto loops = extractBoundaryLoops(cubeWithout(true));
        CHECK(loops.size() == 1, "open cube has exactly one hole loop");
        if (loops.size() == 1) {
            CHECK(loops[0].size() == 4, "the hole loop has four vertices");
            std::vector<std::uint32_t> got = loops[0];
            std::sort(got.begin(), got.end());
            const std::vector<std::uint32_t> want = {2, 3, 6, 7};
            CHECK(got == want, "the loop is exactly the removed face's four corners");
        }
    }

    // --- 3. A flat quad: one 4-vertex perimeter loop. ---
    {
        shapes::MeshData q;
        q.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,0,1), vtx(0,0,1)};
        q.indices = {0,1,2, 0,2,3};
        const auto loops = extractBoundaryLoops(q);
        CHECK(loops.size() == 1 && loops[0].size() == 4, "flat quad has one 4-vertex perimeter loop");
    }

    // --- 4. Two disjoint quads: two separate loops. ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,0,1), vtx(0,0,1),
                      vtx(5,0,0), vtx(6,0,0), vtx(6,0,1), vtx(5,0,1)};
        m.indices = {0,1,2, 0,2,3,  4,5,6, 4,6,7};
        const auto loops = extractBoundaryLoops(m);
        CHECK(loops.size() == 2, "two disjoint quads give two loops");
        for (const auto& L : loops) CHECK(L.size() == 4, "each loop has four vertices");
    }

    // --- 5. Every extracted loop closes: consecutive vertices (cyclically) are all distinct. ---
    {
        const auto loops = extractBoundaryLoops(cubeWithout(true));
        bool distinct = true;
        for (const auto& L : loops)
            for (std::size_t i = 0; i < L.size(); ++i)
                if (L[i] == L[(i + 1) % L.size()]) distinct = false;
        CHECK(distinct, "loop vertices are all distinct (a proper cycle)");
    }

    if (g_fail == 0) {
        std::printf("meshboundaryloops: OK — closed=0, open cube hole=1 loop of 4, quad + two-quad cases.\n");
        return 0;
    }
    std::printf("meshboundaryloops: %d failure(s).\n", g_fail);
    return 1;
}
