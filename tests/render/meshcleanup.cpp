// tests/render/meshcleanup.cpp — verifies the mesh cleanup pass (render::cleanupMesh): exact-duplicate vertex
// merge, unused-vertex removal, degenerate-triangle drop, and buffer compaction — all WITHOUT changing what the
// mesh draws (identical triangles, winding, and attributes). Pure CPU, headless.
#include "maz/render/MeshCleanup.hpp"

#include <array>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}

// The ordered sequence of per-triangle world positions — the ground truth for "what the mesh draws".
using TriPos = std::array<std::array<float, 3>, 3>;
static std::vector<TriPos> triangles(const shapes::MeshData& m) {
    std::vector<TriPos> out;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        TriPos tp{};
        for (int k = 0; k < 3; ++k) {
            const MeshVertex& v = m.vertices[m.indices[t + static_cast<std::size_t>(k)]];
            tp[static_cast<std::size_t>(k)] = {v.px, v.py, v.pz};
        }
        out.push_back(tp);
    }
    return out;
}

int main() {
    // --- 1. Unused vertices are removed; drawn geometry is byte-for-byte identical. ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(0,1,0),   // used by the triangle
                      vtx(9,9,9), vtx(8,8,8)};              // orphans, referenced by nothing
        m.indices = {0, 1, 2};
        MeshCleanupStats s;
        const shapes::MeshData c = cleanupMesh(m, &s);
        CHECK(s.verticesBefore == 5 && s.verticesAfter == 3, "two orphan vertices removed");
        CHECK(s.unusedRemoved == 2 && s.duplicatesMerged == 0, "counted as unused, none merged");
        CHECK(s.trianglesAfter == 1, "the triangle survives");
        CHECK(triangles(c) == triangles(m), "drawn triangle positions are unchanged");
    }

    // --- 2. Exact-duplicate vertices (face-by-face quad) merge from 6 to 4; same two triangles. ---
    {
        shapes::MeshData m;
        // Two triangles authored with their own copies of the shared corners -> 6 vertices, 2 are duplicates.
        m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,1,0),    // tri 0
                      vtx(0,0,0), vtx(1,1,0), vtx(0,1,0)};   // tri 1 (verts 3,4 duplicate 0,2)
        m.indices = {0,1,2, 3,4,5};
        MeshCleanupStats s;
        const shapes::MeshData c = cleanupMesh(m, &s);
        CHECK(s.verticesAfter == 4, "quad compacts to four unique vertices");
        CHECK(s.duplicatesMerged == 2, "two exact duplicates merged");
        CHECK(s.unusedRemoved == 0, "nothing unused");
        CHECK(s.trianglesAfter == 2 && s.degenerateRemoved == 0, "both triangles kept");
        CHECK(triangles(c) == triangles(m), "drawn geometry identical after dedup");
    }

    // --- 3. Degenerate triangles (repeated corner) are dropped. ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(0,1,0)};
        m.indices = {0,1,2,  0,1,1,  2,2,2}; // one real + two degenerate
        MeshCleanupStats s;
        const shapes::MeshData c = cleanupMesh(m, &s);
        CHECK(s.trianglesBefore == 3 && s.trianglesAfter == 1, "only the non-degenerate triangle survives");
        CHECK(s.degenerateRemoved == 2, "two degenerate triangles removed");
    }

    // --- 4. A mesh with duplicates behind a degenerate triangle: dedup can itself expose a degenerate. ---
    {
        shapes::MeshData m;
        // Vertices 0 and 2 are identical; the triangle (0,1,2) becomes (0,1,0) after dedup -> degenerate.
        m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(0,0,0)};
        m.indices = {0,1,2};
        MeshCleanupStats s;
        const shapes::MeshData c = cleanupMesh(m, &s);
        CHECK(s.duplicatesMerged == 1, "the identical vertex is merged");
        CHECK(s.degenerateRemoved == 1 && s.trianglesAfter == 0, "the collapsed triangle is dropped");
        CHECK(c.vertices.empty(), "no triangle survives -> no vertices emitted");
    }

    // --- 5. An already-clean mesh is left unchanged, and cleanup is idempotent. ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,1,0), vtx(0,1,0)};
        m.indices = {0,1,2, 0,2,3};
        MeshCleanupStats s;
        const shapes::MeshData c = cleanupMesh(m, &s);
        CHECK(s.verticesAfter == 4 && s.trianglesAfter == 2, "clean mesh keeps its counts");
        CHECK(s.duplicatesMerged == 0 && s.unusedRemoved == 0 && s.degenerateRemoved == 0, "nothing to do");
        const shapes::MeshData c2 = cleanupMesh(c);
        CHECK(triangles(c2) == triangles(c) && c2.vertices.size() == c.vertices.size(), "cleanup is idempotent");
    }

    // --- 6. Empty mesh is safe. ---
    {
        MeshCleanupStats s;
        const shapes::MeshData c = cleanupMesh(shapes::MeshData{}, &s);
        CHECK(c.vertices.empty() && c.indices.empty(), "empty mesh -> empty mesh");
        CHECK(s.verticesAfter == 0 && s.trianglesAfter == 0, "empty stats");
    }

    if (g_fail == 0) {
        std::printf("meshcleanup: OK — orphans removed, exact dupes merged 6->4, degenerates dropped, idempotent.\n");
        return 0;
    }
    std::printf("meshcleanup: %d failure(s).\n", g_fail);
    return 1;
}
