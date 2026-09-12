// tests/render/meshtopology.cpp — verifies mesh connectivity (render::buildTopology / MeshTopology). A
// correct topology: on a welded closed cube it is watertight (no boundary/non-manifold edges), has the right
// edge count and Euler characteristic 2, every triangle has three neighbours, and twins are symmetric; on an
// open grid it reports exactly the perimeter as boundary edges (with those vertices flagged and the interior
// vertex not) and Euler characteristic 1; and an edge shared by three triangles is flagged non-manifold.
// Pure CPU, headless.
#include "maz/render/MeshTopology.hpp"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}

// A welded unit cube: 8 shared vertices, 12 triangles, consistent winding.
static shapes::MeshData weldedCube() {
    shapes::MeshData m;
    m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,1,0), vtx(0,1,0),
                  vtx(0,0,1), vtx(1,0,1), vtx(1,1,1), vtx(0,1,1)};
    m.indices = {
        0,1,2, 2,3,0,   // front  z=0
        1,5,6, 6,2,1,   // right   x=1
        5,4,7, 7,6,5,   // back    z=1
        4,0,3, 3,7,4,   // left    x=0
        3,2,6, 6,7,3,   // top     y=1
        4,5,1, 1,0,4,   // bottom  y=0
    };
    return m;
}

// A WxH grid of unit quads (welded, shared vertices), two triangles per quad — an open surface.
static shapes::MeshData grid(int w, int h) {
    shapes::MeshData m;
    for (int y = 0; y <= h; ++y)
        for (int x = 0; x <= w; ++x)
            m.vertices.push_back(vtx(static_cast<float>(x), 0.0f, static_cast<float>(y)));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const std::uint32_t a = static_cast<std::uint32_t>(y * (w + 1) + x);
            const std::uint32_t b = a + 1;
            const std::uint32_t c = a + static_cast<std::uint32_t>(w + 1);
            const std::uint32_t d = c + 1;
            m.indices.insert(m.indices.end(), {a, b, c, b, d, c});
        }
    return m;
}

int main() {
    // --- 1. Welded cube: watertight, correct edge count + Euler, full neighbours, symmetric twins. ---
    {
        const shapes::MeshData cube = weldedCube();
        const MeshTopology t = buildTopology(cube);
        CHECK(t.vertexCount == 8 && t.triangleCount == 12, "cube counts");
        CHECK(t.edgeCount == 18, "cube has 18 undirected edges");           // V-E+F=2 => 8-18+12=2
        CHECK(t.boundaryEdgeCount == 0, "cube has no boundary edges");
        CHECK(t.nonManifoldEdgeCount == 0, "cube has no non-manifold edges");
        CHECK(t.watertight(), "cube is watertight");
        CHECK(t.eulerCharacteristic() == 2, "cube Euler characteristic is 2");

        bool allNeighbors = true, twinSym = true;
        for (std::uint32_t tri = 0; tri < t.triangleCount; ++tri)
            for (int e = 0; e < 3; ++e)
                if (t.triangleNeighbor(tri, e) == MeshTopology::kNone) allNeighbors = false;
        for (std::uint32_t he = 0; he < t.opposite.size(); ++he) {
            const std::uint32_t o = t.opposite[he];
            if (o == MeshTopology::kNone || t.opposite[o] != he) twinSym = false;
        }
        CHECK(allNeighbors, "every cube triangle edge has a neighbour");
        CHECK(twinSym, "cube twins are symmetric (opposite[opposite[he]] == he)");
        bool noBoundaryVerts = true;
        for (std::uint8_t bv : t.boundaryVertex) if (bv) noBoundaryVerts = false;
        CHECK(noBoundaryVerts, "no cube vertex is on a boundary");
    }

    // --- 2. Open 2x2 grid: perimeter is boundary, interior vertex is not, Euler 1. ---
    {
        const shapes::MeshData g = grid(2, 2); // 9 verts, 8 tris
        const MeshTopology t = buildTopology(g);
        CHECK(t.vertexCount == 9 && t.triangleCount == 8, "grid counts");
        CHECK(!t.watertight(), "open grid is not watertight");
        CHECK(t.boundaryEdgeCount == 8, "2x2 grid has 8 perimeter boundary edges");
        CHECK(t.nonManifoldEdgeCount == 0, "grid is manifold");
        CHECK(t.eulerCharacteristic() == 1, "disk Euler characteristic is 1"); // 9-16+8=1
        // The center vertex (index 4 in a 3x3 grid) is interior; the 8 others are on the rim.
        CHECK(t.boundaryVertex[4] == 0, "center vertex is interior");
        int rim = 0;
        for (std::uint8_t bv : t.boundaryVertex) rim += bv;
        CHECK(rim == 8, "the 8 rim vertices are flagged boundary");
    }

    // --- 3. Non-manifold: an edge shared by three triangles is flagged. ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(0,1,0), vtx(1,1,0), vtx(0,0,1)};
        // Three triangles all sharing the edge (0,1).
        m.indices = {0,1,2, 0,1,3, 0,1,4};
        const MeshTopology t = buildTopology(m);
        CHECK(t.nonManifoldEdgeCount >= 1, "edge shared by 3 triangles is non-manifold");
        CHECK(!t.watertight(), "non-manifold mesh is not watertight");
    }

    // --- 4. Empty / malformed is handled. ---
    {
        const MeshTopology t = buildTopology(0, {});
        CHECK(t.triangleCount == 0 && t.edgeCount == 0 && t.watertight(), "empty mesh: trivially closed");
    }

    if (g_fail == 0) {
        std::printf("meshtopology: OK — watertight cube, open-grid boundary, non-manifold flag, twins.\n");
        return 0;
    }
    std::printf("meshtopology: %d failure(s).\n", g_fail);
    return 1;
}
