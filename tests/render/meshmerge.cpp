// tests/render/meshmerge.cpp — verifies mesh concatenation (render::mergeMeshes). Ground truths: merging two
// meshes appends vertices and re-bases the second mesh's indices by the first's vertex count; attributes are
// preserved; merging a list sums everything; the merge of two disjoint parts splits back into two components;
// empty inputs are skipped. Pure CPU, headless.
#include "maz/render/MeshMerge.hpp"
#include "maz/render/MeshComponents.hpp" // splitConnectedComponents — round-trip check

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}

static shapes::MeshData triAt(float ox) { // a single triangle offset in x (disjoint from others)
    shapes::MeshData m;
    m.vertices = {vtx(ox, 0, 0), vtx(ox + 1, 0, 0), vtx(ox, 1, 0)};
    m.indices = {0, 1, 2};
    return m;
}

int main() {
    // --- 1. Merge two triangles: 6 vertices, 6 indices, second triangle re-based by +3. ---
    {
        const shapes::MeshData m = mergeMeshes(triAt(0.0f), triAt(10.0f));
        CHECK(m.vertices.size() == 6, "vertices are appended (3 + 3)");
        CHECK(m.indices.size() == 6, "indices are appended (3 + 3)");
        CHECK(m.indices[0] == 0 && m.indices[1] == 1 && m.indices[2] == 2, "first triangle keeps its indices");
        CHECK(m.indices[3] == 3 && m.indices[4] == 4 && m.indices[5] == 5, "second triangle is re-based by +3");
        CHECK(m.vertices[3].px > 9.0f, "the second triangle's vertices came along at their x offset");
    }

    // --- 2. Merge a list of three: everything sums. ---
    {
        const std::vector<shapes::MeshData> parts = {triAt(0), triAt(5), triAt(10)};
        const shapes::MeshData m = mergeMeshes(parts);
        CHECK(m.vertices.size() == 9 && m.indices.size() == 9, "three triangles -> 9 verts, 9 indices");
        // The last triangle's indices must point at vertices 6,7,8.
        CHECK(m.indices[6] == 6 && m.indices[7] == 7 && m.indices[8] == 8, "third triangle re-based by +6");
    }

    // --- 3. Round-trip: merging two disjoint parts splits back into two components. ---
    {
        const shapes::MeshData merged = mergeMeshes(triAt(0.0f), triAt(100.0f));
        std::uint32_t count = 0;
        connectedComponentLabels(merged, count);
        CHECK(count == 2, "the merged mesh is two disjoint islands again");
        const std::vector<shapes::MeshData> back = splitConnectedComponents(merged);
        CHECK(back.size() == 2, "splitting recovers the two parts");
    }

    // --- 4. Empty parts are skipped; merging with an empty mesh is a no-op copy. ---
    {
        const std::vector<shapes::MeshData> parts = {shapes::MeshData{}, triAt(0.0f), shapes::MeshData{}};
        const shapes::MeshData m = mergeMeshes(parts);
        CHECK(m.vertices.size() == 3 && m.indices.size() == 3, "empty meshes contribute nothing");
    }

    // --- 5. Merging nothing yields an empty mesh. ---
    {
        const shapes::MeshData m = mergeMeshes(std::vector<shapes::MeshData>{});
        CHECK(m.vertices.empty() && m.indices.empty(), "merging an empty list -> empty mesh");
    }

    if (g_fail == 0) {
        std::printf("meshmerge: OK — two/three triangles appended + re-based, round-trip splits back, empties skipped.\n");
        return 0;
    }
    std::printf("meshmerge: %d failure(s).\n", g_fail);
    return 1;
}
