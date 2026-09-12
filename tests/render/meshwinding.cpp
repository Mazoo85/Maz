// tests/render/meshwinding.cpp — verifies triangle winding-consistency detection (render::analyzeWinding /
// makeWindingConsistent). Ground truths: a uniformly wound grid and a proper cube report zero inconsistent
// edges; reversing one triangle's winding is detected as the single minority flip and its shared edges become
// inconsistent; makeWindingConsistent repairs it back to zero inconsistent edges. Pure CPU, headless.
#include "maz/render/MeshWinding.hpp"

#include <cstdio>
#include <utility>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}

static shapes::MeshData grid(int n) {
    shapes::MeshData m;
    for (int z = 0; z < n; ++z)
        for (int x = 0; x < n; ++x)
            m.vertices.push_back(vtx(static_cast<float>(x), 0.0f, static_cast<float>(z)));
    auto idx = [n](int x, int z) { return static_cast<std::uint32_t>(z * n + x); };
    for (int z = 0; z < n - 1; ++z)
        for (int x = 0; x < n - 1; ++x) {
            const std::uint32_t a = idx(x, z), b = idx(x, z + 1), c = idx(x + 1, z), d = idx(x + 1, z + 1);
            m.indices.insert(m.indices.end(), {a, b, c, c, b, d});
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
    // --- 1. A uniformly wound grid is already consistent. ---
    {
        const WindingReport r = analyzeWinding(grid(4));
        CHECK(r.consistent && r.inconsistentEdges == 0, "a uniformly wound grid has no inconsistent edges");
        CHECK(r.flippedCount == 0, "nothing needs flipping");
        CHECK(r.componentsChecked == 1, "the grid is one connected component");
    }

    // --- 2. A proper cube is consistently wound. ---
    {
        const WindingReport r = analyzeWinding(cube());
        CHECK(r.consistent && r.inconsistentEdges == 0, "a proper cube is uniformly wound");
        CHECK(r.flippedCount == 0, "no cube triangle needs flipping");
    }

    // --- 3. Reverse one grid triangle: detected as one minority flip; its interior edges go inconsistent. ---
    {
        shapes::MeshData m = grid(4);
        // Reverse triangle 10 (swap corners 1 and 2 of that triangle).
        const std::size_t t = 10;
        std::swap(m.indices[t * 3 + 1], m.indices[t * 3 + 2]);
        const WindingReport r = analyzeWinding(m);
        CHECK(!r.consistent, "the reversed triangle makes the mesh inconsistent");
        CHECK(r.inconsistentEdges >= 1, "at least one shared edge now winds the same direction");
        CHECK(r.flippedCount == 1, "exactly one triangle is the minority to flip");
        CHECK(r.flipped[t] == 1, "the reversed triangle is the one flagged");
    }

    // --- 4. makeWindingConsistent repairs the reversed triangle. ---
    {
        shapes::MeshData m = grid(4);
        const std::size_t t = 10;
        std::swap(m.indices[t * 3 + 1], m.indices[t * 3 + 2]);
        const shapes::MeshData fixed = makeWindingConsistent(m);
        const WindingReport r = analyzeWinding(fixed);
        CHECK(r.consistent && r.inconsistentEdges == 0, "the repaired mesh is uniformly wound");
        CHECK(r.flippedCount == 0, "nothing remains to flip after repair");
    }

    // --- 5. Reverse one cube face's two triangles: both detected. ---
    {
        shapes::MeshData m = cube();
        // Face triangles 0 and 1 (the z=0 face). Reverse both.
        std::swap(m.indices[0 * 3 + 1], m.indices[0 * 3 + 2]);
        std::swap(m.indices[1 * 3 + 1], m.indices[1 * 3 + 2]);
        const WindingReport r = analyzeWinding(m);
        CHECK(!r.consistent, "a flipped cube face is inconsistent");
        CHECK(r.flippedCount == 2, "the two reversed face triangles are the minority to flip");
        const shapes::MeshData fixed = makeWindingConsistent(m);
        CHECK(analyzeWinding(fixed).consistent, "repair restores full consistency");
    }

    // --- 6. Empty mesh is safe (and vacuously consistent). ---
    {
        const WindingReport r = analyzeWinding(shapes::MeshData{});
        CHECK(r.consistent && r.flipped.empty(), "empty mesh -> consistent, empty report");
    }

    if (g_fail == 0) {
        std::printf("meshwinding: OK — grid/cube consistent, one flip detected+repaired, cube face flip detected.\n");
        return 0;
    }
    std::printf("meshwinding: %d failure(s).\n", g_fail);
    return 1;
}
