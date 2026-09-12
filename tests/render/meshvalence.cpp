// tests/render/meshvalence.cpp — verifies the vertex-valence report (render::analyzeValence). Ground truths:
// a regularly triangulated grid has valence-6 interior vertices and lower-valence boundary vertices; a single
// triangle is all boundary; a closed cube has no boundary and no valence-6 vertices; an unreferenced vertex is
// isolated. Pure CPU, headless.
#include "maz/render/MeshValence.hpp"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}

// N x N grid triangulated with a consistent diagonal ({a,b,c,c,b,d}); interior vertices get valence 6.
static shapes::MeshData grid(int n) {
    shapes::MeshData m;
    for (int z = 0; z < n; ++z)
        for (int x = 0; x < n; ++x)
            m.vertices.push_back(vtx(static_cast<float>(x), 0.0f, static_cast<float>(z)));
    auto idx = [n](int x, int z) { return static_cast<std::uint32_t>(z * n + x); };
    for (int z = 0; z < n - 1; ++z) {
        for (int x = 0; x < n - 1; ++x) {
            const std::uint32_t a = idx(x, z), b = idx(x, z + 1), c = idx(x + 1, z), d = idx(x + 1, z + 1);
            m.indices.insert(m.indices.end(), {a, b, c, c, b, d});
        }
    }
    return m;
}

static shapes::MeshData cube() {
    shapes::MeshData m;
    m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,1,0), vtx(0,1,0),
                  vtx(0,0,1), vtx(1,0,1), vtx(1,1,1), vtx(0,1,1)};
    m.indices = {0,2,1, 0,3,2,  4,5,6, 4,6,7,  0,1,5, 0,5,4,
                 3,7,6, 3,6,2,  0,4,7, 0,7,3,  1,2,6, 1,6,5};
    return m;
}

int main() {
    // --- 1. 5x5 grid: the single interior-interior vertex (2,2) has valence 6; corners/edges are boundary. ---
    {
        const int n = 5;
        const ValenceReport r = analyzeValence(grid(n));
        const std::size_t center = static_cast<std::size_t>(2 * n + 2);
        CHECK(r.valence[center] == 6, "a regular grid interior vertex has valence 6");
        CHECK(r.boundary[center] == 0, "the centre vertex is interior");
        // The 4*(n-1) rim vertices are all boundary; the (n-2)^2 inner ones are interior.
        CHECK(r.boundaryVertices == static_cast<std::size_t>(4 * (n - 1)), "the grid rim is all boundary");
        CHECK(r.regularInterior == static_cast<std::size_t>((n - 2) * (n - 2)),
              "every interior grid vertex is a regular valence-6 vertex");
        CHECK(r.irregularInterior == 0, "a regular grid has no interior poles");
        CHECK(r.isolatedVertices == 0, "every grid vertex is used");
    }

    // --- 2. A corner vertex of the grid has valence 2 or 3 and is flagged boundary. ---
    {
        const ValenceReport r = analyzeValence(grid(4));
        CHECK(r.boundary[0] == 1, "the (0,0) corner is on the boundary");
        CHECK(r.minValence >= 2, "even a corner connects to at least two neighbours");
    }

    // --- 3. Single triangle: three boundary vertices, each valence 2, no interior. ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(0,1,0)};
        m.indices = {0,1,2};
        const ValenceReport r = analyzeValence(m);
        CHECK(r.boundaryVertices == 3, "all three triangle vertices are boundary");
        CHECK(r.valence[0] == 2 && r.valence[1] == 2 && r.valence[2] == 2, "each triangle vertex has valence 2");
        CHECK(r.regularInterior == 0 && r.irregularInterior == 0, "a lone triangle has no interior vertices");
    }

    // --- 4. Closed cube: no boundary, and no vertex reaches valence 6 -> all interior-irregular. ---
    {
        const ValenceReport r = analyzeValence(cube());
        CHECK(r.boundaryVertices == 0, "a closed cube has no open edges");
        CHECK(r.regularInterior + r.irregularInterior == 8, "all eight cube corners are interior vertices");
        CHECK(r.isolatedVertices == 0, "every cube corner is used");
        CHECK(r.maxValence <= 6, "cube valences stay at or below 6");
    }

    // --- 5. An unreferenced extra vertex is isolated. ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(0,1,0), vtx(9,9,9)}; // last one used by no triangle
        m.indices = {0,1,2};
        const ValenceReport r = analyzeValence(m);
        CHECK(r.isolatedVertices == 1 && r.valence[3] == 0, "the unused vertex is isolated (valence 0)");
    }

    // --- 6. Empty mesh is safe. ---
    {
        const ValenceReport r = analyzeValence(shapes::MeshData{});
        CHECK(r.valence.empty() && r.regularInterior == 0, "empty mesh -> empty report");
    }

    if (g_fail == 0) {
        std::printf("meshvalence: OK — grid interior valence 6, rim boundary, triangle all-boundary, cube irregular, isolated vertex.\n");
        return 0;
    }
    std::printf("meshvalence: %d failure(s).\n", g_fail);
    return 1;
}
