// tests/render/meshlodbuilder.cpp — verifies the LOD-ladder builder (render::buildMeshLods), the one-call
// import step that turns one authored mesh into a full level-of-detail set (Godot's
// ImporterMesh.generate_lods). A correct build: level 0 is the source untouched, triangle counts decrease
// monotonically down the ladder, every level is a valid mesh, the returned LodChain has one descending
// threshold per level (index-aligned so a selected index is a direct index into meshes), selection picks the
// finest level for a big on-screen object and a coarser one as it shrinks, maxLevels is respected, it is
// deterministic, and degenerate inputs (maxLevels<=1, empty mesh) yield a single-level set. Source is a
// curved height-field so the quadric decimator has real curvature to work with. Pure CPU, headless.
#include "maz/render/MeshLodBuilder.hpp"

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

// A (w+1)x(h+1) height-field grid over [0,w]x[0,h] with a smooth central bump, two triangles per quad.
static shapes::MeshData bumpGrid(int w, int h, float amp) {
    shapes::MeshData m;
    const float pi = 3.14159265358979323846f;
    for (int y = 0; y <= h; ++y) {
        for (int x = 0; x <= w; ++x) {
            MeshVertex v{};
            v.px = static_cast<float>(x);
            v.pz = static_cast<float>(y);
            v.py = amp * std::sin(static_cast<float>(x) / static_cast<float>(w) * pi)
                       * std::sin(static_cast<float>(y) / static_cast<float>(h) * pi);
            v.r = v.g = v.b = 1.0f;
            m.vertices.push_back(v);
        }
    }
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::uint32_t a = static_cast<std::uint32_t>(y * (w + 1) + x);
            const std::uint32_t b = a + 1;
            const std::uint32_t c = a + static_cast<std::uint32_t>(w + 1);
            const std::uint32_t d = c + 1;
            m.indices.insert(m.indices.end(), {a, b, c, b, d, c});
        }
    }
    return m;
}

static bool meshValid(const shapes::MeshData& m) {
    if (m.indices.size() % 3 != 0) return false;
    const std::uint32_t n = static_cast<std::uint32_t>(m.vertices.size());
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const std::uint32_t a = m.indices[t], b = m.indices[t + 1], c = m.indices[t + 2];
        if (a == b || b == c || a == c) return false;   // degenerate
        if (a >= n || b >= n || c >= n) return false;    // out of range
    }
    return true;
}

static std::size_t tris(const shapes::MeshData& m) { return m.indices.size() / 3; }

int main() {
    const shapes::MeshData src = bumpGrid(40, 40, 6.0f); // 3200 triangles
    const std::size_t srcTris = tris(src);
    CHECK(srcTris == 3200, "source has 3200 triangles");

    // --- basic ladder shape ---
    const MeshLodSet set = buildMeshLods(src, 0.5f, 32, 6, 200.0f);
    CHECK(set.count() >= 2, "a dense mesh yields at least 2 LOD levels");
    CHECK(set.count() <= 6, "maxLevels is respected");
    CHECK(set.chain.levelCount() == set.count(), "chain has one threshold per mesh level");
    CHECK(tris(set.meshes[0]) == srcTris, "level 0 is the source mesh untouched");

    // meshes[0] must be an exact copy of the source (verbatim vertices/indices).
    CHECK(set.meshes[0].vertices.size() == src.vertices.size(), "level 0 keeps every source vertex");
    CHECK(set.meshes[0].indices == src.indices, "level 0 keeps the source index buffer verbatim");

    // --- monotonic decrease + validity ---
    bool monotonic = true;
    for (std::size_t i = 0; i < set.count(); ++i) {
        CHECK(meshValid(set.meshes[i]), "every LOD level is a valid mesh");
        if (i > 0 && tris(set.meshes[i]) >= tris(set.meshes[i - 1])) monotonic = false;
    }
    CHECK(monotonic, "triangle counts strictly decrease down the ladder");
    CHECK(tris(set.meshes[set.count() - 1]) < srcTris, "the coarsest level is cheaper than the source");

    // --- selection wiring: big -> finest, tiny -> coarsest ---
    CHECK(set.chain.select(1.0e6f) == 0, "a huge on-screen size selects the finest level");
    CHECK(set.chain.select(1.0e-3f) == static_cast<int>(set.count() - 1),
          "a tiny on-screen size selects the coarsest level");

    // Camera-driven: a big near object is finer than the same object far away.
    const int near = set.chain.selectForCamera(2.0f, 3.0f, 1.0f, 1080.0f);
    const int far = set.chain.selectForCamera(2.0f, 300.0f, 1.0f, 1080.0f);
    CHECK(near >= 0 && far >= 0, "camera selection returns valid indices");
    CHECK(near <= far, "a nearer object uses an equal-or-finer LOD than a distant one");

    // --- determinism ---
    const MeshLodSet again = buildMeshLods(src, 0.5f, 32, 6, 200.0f);
    bool deterministic = again.count() == set.count();
    for (std::size_t i = 0; deterministic && i < set.count(); ++i) {
        if (tris(again.meshes[i]) != tris(set.meshes[i])) deterministic = false;
    }
    CHECK(deterministic, "same input yields the same ladder (deterministic)");

    // --- degenerate inputs ---
    const MeshLodSet one = buildMeshLods(src, 0.5f, 32, 1, 200.0f);
    CHECK(one.count() == 1, "maxLevels<=1 yields a single level");
    CHECK(one.chain.levelCount() == 1, "single-level set has a single threshold");

    const MeshLodSet empty = buildMeshLods(shapes::MeshData{}, 0.5f, 32, 6, 200.0f);
    CHECK(empty.count() == 1, "an empty mesh yields a single (empty) level");
    CHECK(tris(empty.meshes[0]) == 0, "the single empty level has zero triangles");

    // A high floor should stop the ladder early (fewer levels than the low-floor build).
    const MeshLodSet highFloor = buildMeshLods(src, 0.5f, 1600, 6, 200.0f);
    CHECK(highFloor.count() <= 2, "a high minTriangles floor stops the ladder early");

    if (g_fail == 0) std::printf("meshlodbuilder: all checks passed\n");
    return g_fail == 0 ? 0 : 1;
}
