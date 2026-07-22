// tests/render/meshreindex.cpp — verifies bit-exact full-attribute index dedup (render::reindexMesh). Ground
// truths: a triangle soup that shares corners collapses to one vertex per unique (position+normal+colour+UV)
// tuple with a rebuilt index buffer; vertices at the same POSITION but different normal/UV are KEPT separate
// (creases/seams survive); degenerate triangles are dropped; the compacted mesh describes the same triangles.
// Pure CPU, headless.
#include "maz/render/MeshReindex.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

// Full vertex with explicit attributes so we can prove seam/crease preservation.
static MeshVertex full(float x, float y, float z, float nx, float ny, float nz, float u, float v) {
    MeshVertex vv{}; vv.px = x; vv.py = y; vv.pz = z; vv.nx = nx; vv.ny = ny; vv.nz = nz;
    vv.r = vv.g = vv.b = 1.0f; vv.u = u; vv.v = v; return vv;
}
static MeshVertex pos(float x, float y, float z) { return full(x, y, z, 0, 1, 0, 0, 0); }

int main() {
    // --- 1. Triangle soup of two triangles sharing an edge -> 4 unique verts, 2 tris kept. ---
    {
        // Quad as two triangles, each carrying its own corners: 6 vertices, corners (a,b,c) and (a,c,d).
        const MeshVertex a = pos(0, 0, 0), b = pos(1, 0, 0), c = pos(1, 1, 0), d = pos(0, 1, 0);
        shapes::MeshData m;
        m.vertices = {a, b, c, a, c, d}; // soup — a and c each appear twice, bit-identical
        // indices left empty -> implicit soup 0..5
        const ReindexReport r = reindexMesh(m);
        CHECK(r.uniqueVertices == 4, "6 soup corners collapse to 4 unique vertices");
        CHECK(r.mergedVertices == 2, "the two duplicate corners (a, c) merged");
        CHECK(r.mesh.indices.size() == 6, "still two triangles (6 indices)");
        CHECK(r.removedTriangles == 0, "no triangle was degenerate");
        // Every index must be in range of the compacted vertex list.
        bool inRange = true;
        for (std::uint32_t idx : r.mesh.indices)
            if (idx >= r.uniqueVertices) inRange = false;
        CHECK(inRange, "all indices point inside the compacted vertex buffer");
    }

    // --- 2. Same POSITION, different UV -> a texture seam: must stay TWO vertices. ---
    {
        const MeshVertex p0 = full(0, 0, 0, 0, 1, 0, /*uv=*/0.0f, 0.0f);
        const MeshVertex p0seam = full(0, 0, 0, 0, 1, 0, /*uv=*/1.0f, 0.0f); // same xyz, different U
        const MeshVertex q = pos(1, 0, 0), s = pos(0, 1, 0);
        shapes::MeshData m;
        m.vertices = {p0, q, s, p0seam, q, s}; // p0 and p0seam share a point but differ in UV
        const ReindexReport r = reindexMesh(m);
        // q and s each duplicate (merge), p0/p0seam do NOT -> unique = p0,p0seam,q,s = 4.
        CHECK(r.uniqueVertices == 4, "a UV seam is preserved — the shared point stays two vertices");
        CHECK(r.mergedVertices == 2, "only the genuinely identical q and s merged");
    }

    // --- 3. Same POSITION+UV, different NORMAL -> a hard crease: must stay TWO vertices. ---
    {
        const MeshVertex up = full(0, 0, 0, 0, 1, 0, 0, 0);
        const MeshVertex side = full(0, 0, 0, 1, 0, 0, 0, 0); // same xyz+uv, different normal
        CHECK(!(detail::keyOf(up) == detail::keyOf(side)), "a differing normal yields a different key");
        shapes::MeshData m;
        m.vertices = {up, pos(1, 0, 0), pos(0, 1, 0), side, pos(1, 0, 0), pos(0, 1, 0)};
        const ReindexReport r = reindexMesh(m);
        CHECK(r.uniqueVertices == 4, "a normal crease is preserved — two vertices at the shared point");
    }

    // --- 4. A degenerate triangle (two corners the same vertex) is dropped. ---
    {
        shapes::MeshData m;
        m.vertices = {pos(0, 0, 0), pos(1, 0, 0), pos(0, 0, 0)}; // corner 0 and 2 are bit-identical
        m.indices = {0, 1, 2};
        const ReindexReport r = reindexMesh(m);
        CHECK(r.uniqueVertices == 2, "the two identical corners collapse to one vertex");
        CHECK(r.removedTriangles == 1 && r.mesh.indices.empty(), "the collapsed triangle is dropped");
    }

    // --- 5. An already-indexed clean mesh is returned unchanged in shape (nothing to merge). ---
    {
        shapes::MeshData m;
        m.vertices = {pos(0, 0, 0), pos(1, 0, 0), pos(0, 1, 0)};
        m.indices = {0, 1, 2};
        const ReindexReport r = reindexMesh(m);
        CHECK(r.uniqueVertices == 3 && r.mergedVertices == 0, "a clean indexed triangle is untouched");
        CHECK(r.mesh.indices.size() == 3 && r.removedTriangles == 0, "its one triangle survives");
    }

    // --- 6. Empty mesh is safe. ---
    {
        const ReindexReport r = reindexMesh(shapes::MeshData{});
        CHECK(r.uniqueVertices == 0 && r.mesh.vertices.empty() && r.mesh.indices.empty(), "empty -> empty");
    }

    if (g_fail == 0) {
        std::printf("meshreindex: OK — soup indexed to unique verts, seams/creases preserved, degenerates dropped.\n");
        return 0;
    }
    std::printf("meshreindex: %d failure(s).\n", g_fail);
    return 1;
}
