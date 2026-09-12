// tests/render/meshwireframe.cpp — verifies unique-edge / wireframe extraction (render::meshEdges, meshWireframe).
// Ground truths: a single triangle has 3 edges; two triangles sharing an edge (a quad) have 5 unique edges (the
// shared one collapsed); a watertight triangulated cube has 18 edges (E = 3F/2); every edge is unique (no pair
// twice); meshWireframe returns two positions per edge; an empty mesh yields nothing. Pure CPU, headless.
#include "maz/render/MeshWireframe.hpp"

#include <cstdio>
#include <unordered_set>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::render::MeshVertex;
using maz::render::meshEdges;
using maz::render::meshWireframe;
namespace shapes = maz::render::shapes;

static MeshVertex V(float x, float y, float z) { MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1; return v; }

int main() {
    // --- 1. A single triangle -> 3 edges. ---
    {
        shapes::MeshData m;
        m.vertices = {V(0, 0, 0), V(1, 0, 0), V(0, 1, 0)};
        m.indices = {0, 1, 2};
        CHECK(meshEdges(m).size() == 3, "a triangle has 3 unique edges");
        CHECK(meshWireframe(m).size() == 6, "wireframe has 2 positions per edge (3 edges -> 6)");
    }

    // --- 2. Two triangles sharing an edge (a quad) -> 5 unique edges (shared diagonal collapsed). ---
    {
        shapes::MeshData m;
        m.vertices = {V(0, 0, 0), V(1, 0, 0), V(1, 1, 0), V(0, 1, 0)};
        m.indices = {0, 1, 2, 0, 2, 3}; // shared edge 0-2
        CHECK(meshEdges(m).size() == 5, "a two-triangle quad has 5 unique edges (shared 0-2 counted once)");
    }

    // --- 3. A watertight triangulated cube -> 18 unique edges (12 box edges + 6 face diagonals). ---
    {
        shapes::MeshData m;
        const float h = 1.0f;
        m.vertices = {V(-h,-h,-h), V(h,-h,-h), V(h,-h,h), V(-h,-h,h), V(-h,h,-h), V(h,h,-h), V(h,h,h), V(-h,h,h)};
        m.indices = {0,1,2, 0,2,3,  4,6,5, 4,7,6,  3,2,6, 3,6,7,  1,0,4, 1,4,5,  2,1,5, 2,5,6,  0,3,7, 0,7,4};
        const auto e = meshEdges(m);
        CHECK(e.size() == 18, "a triangulated cube has 18 unique edges (E = 3F/2)");
        // Every edge is unique (build a set and confirm no collisions).
        std::unordered_set<std::uint64_t> keys;
        for (const auto& ed : e) keys.insert((static_cast<std::uint64_t>(ed.first) << 32) | ed.second);
        CHECK(keys.size() == e.size(), "no edge is listed twice");
        // Every edge references valid, distinct vertices.
        bool ok = true;
        for (const auto& ed : e) if (ed.first == ed.second || ed.first >= 8 || ed.second >= 8) ok = false;
        CHECK(ok, "edges reference distinct, in-range vertices");
    }

    // --- 4. Empty mesh -> no edges. ---
    {
        CHECK(meshEdges(shapes::MeshData{}).empty(), "empty mesh -> no edges");
        CHECK(meshWireframe(shapes::MeshData{}).empty(), "empty mesh -> no wireframe segments");
    }

    if (g_fail == 0) {
        std::printf("meshwireframe: OK — triangle 3, quad 5, cube 18, unique edges, line-list, safe.\n");
        return 0;
    }
    std::printf("meshwireframe: %d failure(s).\n", g_fail);
    return 1;
}
