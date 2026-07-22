// tests/render/meshcomponents.cpp — verifies connected-component / island splitting
// (render::splitConnectedComponents / connectedComponentLabels). A correct split: partitions the triangles
// exactly (every triangle in one island, none lost or duplicated), separates edge-disjoint pieces (two cubes
// -> two watertight islands) while keeping an edge-connected mesh whole, and treats a vertex-only touch as
// two islands. Built on MeshTopology. Pure CPU, headless.
#include "maz/render/MeshComponents.hpp"
#include "maz/render/MeshTopology.hpp"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}

// Append a welded unit cube (8 verts, 12 tris) translated by (ox,oy,oz) into `m`.
static void addCube(shapes::MeshData& m, float ox, float oy, float oz) {
    const std::uint32_t b = static_cast<std::uint32_t>(m.vertices.size());
    const float c[8][3] = {{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};
    for (const auto& p : c) m.vertices.push_back(vtx(p[0]+ox, p[1]+oy, p[2]+oz));
    const std::uint32_t f[36] = {
        0,1,2, 2,3,0,  1,5,6, 6,2,1,  5,4,7, 7,6,5,
        4,0,3, 3,7,4,  3,2,6, 6,7,3,  4,5,1, 1,0,4,
    };
    for (std::uint32_t i : f) m.indices.push_back(b + i);
}

int main() {
    // --- 1. Two edge-disjoint cubes split into two watertight islands. ---
    {
        shapes::MeshData m;
        addCube(m, 0, 0, 0);
        addCube(m, 10, 0, 0); // far apart, no shared verts
        const std::vector<shapes::MeshData> parts = splitConnectedComponents(m);
        CHECK(parts.size() == 2, "two cubes -> two components");
        std::size_t totalTris = 0;
        bool eachWatertight = true;
        for (const shapes::MeshData& p : parts) {
            CHECK(p.vertices.size() == 8, "each island has its own 8 vertices");
            CHECK(p.indices.size() == 36, "each island has 12 triangles");
            totalTris += p.indices.size() / 3;
            if (!buildTopology(p).watertight()) eachWatertight = false;
        }
        CHECK(totalTris == 24, "triangle count is partitioned exactly (12+12)");
        CHECK(eachWatertight, "each separated cube is still a watertight solid");
    }

    // --- 2. A single connected cube stays one island. ---
    {
        shapes::MeshData m;
        addCube(m, 0, 0, 0);
        const std::vector<shapes::MeshData> parts = splitConnectedComponents(m);
        CHECK(parts.size() == 1, "one cube -> one component");
        CHECK(parts[0].indices.size() == 36, "the whole cube is kept together");
    }

    // --- 3. Labels partition every triangle and are deterministic. ---
    {
        shapes::MeshData m;
        addCube(m, 0, 0, 0);
        addCube(m, 10, 0, 0);
        addCube(m, 20, 0, 0);
        std::uint32_t n1 = 0, n2 = 0;
        const std::vector<std::uint32_t> a = connectedComponentLabels(m, n1);
        const std::vector<std::uint32_t> b = connectedComponentLabels(m, n2);
        CHECK(n1 == 3 && n2 == 3, "three cubes -> three components");
        CHECK(a == b, "labelling is deterministic");
        bool allLabeled = true;
        for (std::uint32_t l : a) if (l >= n1) allLabeled = false;
        CHECK(a.size() == 36 && allLabeled, "every triangle gets a valid component label");
    }

    // --- 4. Two cubes touching at a single shared vertex are still two islands (edge, not vertex, connects). ---
    {
        shapes::MeshData m;
        addCube(m, 0, 0, 0);          // occupies [0,1]^3, corner vertex 6 = (1,1,1)
        addCube(m, 1, 1, 1);          // occupies [1,2]^3, its corner (1,1,1) coincides but is a SEPARATE vertex
        const std::vector<shapes::MeshData> parts = splitConnectedComponents(m);
        CHECK(parts.size() == 2, "vertex-only touch is not an edge connection -> two islands");
    }

    // --- 5. Empty mesh yields no components. ---
    {
        const std::vector<shapes::MeshData> parts = splitConnectedComponents(shapes::MeshData{});
        CHECK(parts.empty(), "empty mesh -> no components");
    }

    if (g_fail == 0) {
        std::printf("meshcomponents: OK — islands split, partition exact, watertight, vertex-touch separate.\n");
        return 0;
    }
    std::printf("meshcomponents: %d failure(s).\n", g_fail);
    return 1;
}
