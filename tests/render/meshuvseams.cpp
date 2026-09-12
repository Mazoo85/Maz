// tests/render/meshuvseams.cpp — verifies UV-seam edge detection (render::detectUvSeams). Ground truths: a cube
// whose six faces are each their own UV island has a seam along all twelve rim edges (adjacent faces disagree on
// the corner UV) but not along the six in-face diagonals; a continuous shared-UV grid has zero seams; splitting a
// grid's UVs along a column produces seams exactly there. Pure CPU, headless.
#include "maz/render/MeshUvSeams.hpp"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

#include <cmath>
static MeshVertex vtx(float x, float y, float z, float u, float v) {
    MeshVertex mv{}; mv.px = x; mv.py = y; mv.pz = z; mv.u = u; mv.v = v; mv.r = mv.g = mv.b = 1.0f; return mv;
}
static bool near_x(const MeshVertex& v, float x) { return std::fabs(v.px - x) < 1e-4f; }

// A cube whose six faces are separate UV islands: each face has its own four vertices with UVs spanning [0,1].
static shapes::MeshData uvCube() {
    shapes::MeshData m;
    // Each face is packed into its own column of a UV atlas (u offset = face index), so no two faces ever share
    // a UV value at a shared rim — every rim is therefore a genuine UV discontinuity.
    float off = 0.0f;
    auto face = [&](float ax, float ay, float az, float bx, float by, float bz,
                    float cx, float cy, float cz, float dx, float dy, float dz) {
        const std::uint32_t base = static_cast<std::uint32_t>(m.vertices.size());
        m.vertices.push_back(vtx(ax, ay, az, off + 0, 0));
        m.vertices.push_back(vtx(bx, by, bz, off + 1, 0));
        m.vertices.push_back(vtx(cx, cy, cz, off + 1, 1));
        m.vertices.push_back(vtx(dx, dy, dz, off + 0, 1));
        m.indices.insert(m.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        off += 10.0f; // push each face far apart in UV space
    };
    face(0,0,0, 1,0,0, 1,1,0, 0,1,0); // front  z=0
    face(0,0,1, 1,0,1, 1,1,1, 0,1,1); // back   z=1
    face(0,0,0, 0,0,1, 0,1,1, 0,1,0); // left   x=0
    face(1,0,0, 1,0,1, 1,1,1, 1,1,0); // right  x=1
    face(0,0,0, 1,0,0, 1,0,1, 0,0,1); // bottom y=0
    face(0,1,0, 1,1,0, 1,1,1, 0,1,1); // top    y=1
    return m;
}

int main() {
    // --- 1. UV-island cube: 18 interior edges, the 12 rims are seams, the 6 diagonals are not. ---
    {
        const UvSeamResult r = detectUvSeams(uvCube());
        CHECK(r.interiorEdgeCount == 18, "the cube has 18 interior edges (12 rims + 6 face diagonals)");
        CHECK(r.boundaryEdgeCount == 0, "the cube is closed (no boundary edges)");
        CHECK(r.seamCount() == 12, "all twelve rim edges are UV seams");
    }

    // --- 2. Continuous shared-UV grid: no seams. ---
    {
        shapes::MeshData g;
        const int N = 4;
        for (int z = 0; z <= N; ++z)
            for (int x = 0; x <= N; ++x)
                g.vertices.push_back(vtx(static_cast<float>(x), 0.0f, static_cast<float>(z),
                                         static_cast<float>(x) / N, static_cast<float>(z) / N));
        auto id = [&](int x, int z) { return static_cast<std::uint32_t>(z * (N + 1) + x); };
        for (int z = 0; z < N; ++z)
            for (int x = 0; x < N; ++x)
                g.indices.insert(g.indices.end(), {id(x,z), id(x+1,z), id(x+1,z+1),
                                                   id(x,z), id(x+1,z+1), id(x,z+1)});
        const UvSeamResult r = detectUvSeams(g);
        CHECK(r.interiorEdgeCount > 0, "the grid has interior edges");
        CHECK(r.seamCount() == 0, "a continuous shared-UV grid has no seams");
    }

    // --- 3. Split a strip's UVs down the middle column: seams appear exactly along that column. ---
    {
        // Two 1x1 quads side by side sharing the middle column of positions, but authored with separate
        // vertices whose UVs jump across the shared column (left island u in [0,0.5], right in [0.5,1] offset).
        shapes::MeshData m;
        // Left quad: positions x in [0,1]; middle column x=1.
        m.vertices.push_back(vtx(0,0,0, 0.0f, 0)); // 0
        m.vertices.push_back(vtx(1,0,0, 0.5f, 0)); // 1  (middle, left island UV)
        m.vertices.push_back(vtx(1,0,1, 0.5f, 1)); // 2
        m.vertices.push_back(vtx(0,0,1, 0.0f, 1)); // 3
        // Right quad: positions x in [1,2]; middle column x=1 duplicated with a DIFFERENT UV.
        m.vertices.push_back(vtx(1,0,0, 0.9f, 0)); // 4  (middle, right island UV -> discontinuous vs vertex 1)
        m.vertices.push_back(vtx(2,0,0, 1.0f, 0)); // 5
        m.vertices.push_back(vtx(2,0,1, 1.0f, 1)); // 6
        m.vertices.push_back(vtx(1,0,1, 0.9f, 1)); // 7  (middle, discontinuous vs vertex 2)
        m.indices = {0,1,2, 0,2,3,  4,5,6, 4,6,7};
        const UvSeamResult r = detectUvSeams(m);
        CHECK(r.seamCount() == 1, "the shared middle column is exactly one UV seam edge");
        if (r.seamCount() == 1) {
            // The seam runs along the x=1 column: endpoints are the (1,0,0) and (1,0,1) positions.
            const MeshVertex& a = m.vertices[r.seamEdges[0].first];
            const MeshVertex& b = m.vertices[r.seamEdges[0].second];
            const bool along = near_x(a, 1.0f) && near_x(b, 1.0f);
            CHECK(along, "the seam edge lies on the shared x=1 column");
        }
    }

    // --- 4. Empty mesh is safe. ---
    {
        const UvSeamResult r = detectUvSeams(shapes::MeshData{});
        CHECK(r.seamCount() == 0 && r.interiorEdgeCount == 0, "empty mesh -> no seams");
    }

    if (g_fail == 0) {
        std::printf("meshuvseams: OK — UV-island cube 12 rim seams, continuous grid 0, split column 1 seam.\n");
        return 0;
    }
    std::printf("meshuvseams: %d failure(s).\n", g_fail);
    return 1;
}
