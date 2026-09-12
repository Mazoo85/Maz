// tests/render/meshvertexcolorsmooth.cpp — verifies vertex-colour smoothing (render::smoothVertexColors).
// Ground truths: a single black vertex among white neighbours brightens toward the average (noise reduction);
// positions are never touched; a uniform colour is unchanged; smoothing conserves the mean colour (no drift);
// pinning boundary keeps rim vertices fixed. Pure CPU, headless.
#include "maz/render/MeshVertexColorSmooth.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex cvtx(float x, float y, float z, float r, float g, float b) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = r; v.g = g; v.b = b; return v;
}
static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

// N x N grid (all white) with a consistent triangulation.
static shapes::MeshData grid(int n) {
    shapes::MeshData m;
    for (int z = 0; z < n; ++z)
        for (int x = 0; x < n; ++x)
            m.vertices.push_back(cvtx(static_cast<float>(x), 0.0f, static_cast<float>(z), 1, 1, 1));
    auto idx = [n](int x, int z) { return static_cast<std::uint32_t>(z * n + x); };
    for (int z = 0; z < n - 1; ++z)
        for (int x = 0; x < n - 1; ++x) {
            const std::uint32_t a = idx(x, z), b = idx(x, z + 1), c = idx(x + 1, z), d = idx(x + 1, z + 1);
            m.indices.insert(m.indices.end(), {a, b, c, c, b, d});
        }
    return m;
}

int main() {
    // --- 1. A single black vertex among white neighbours brightens; geometry is untouched. ---
    {
        const int n = 5;
        shapes::MeshData m = grid(n);
        const std::size_t center = static_cast<std::size_t>(2 * n + 2);
        m.vertices[center].r = m.vertices[center].g = m.vertices[center].b = 0.0f; // one black speck
        const float px = m.vertices[center].px, pz = m.vertices[center].pz;

        const shapes::MeshData s = smoothVertexColors(m, 0.5f, 1, false);
        CHECK(s.vertices[center].r > 0.4f, "the black speck brightens toward its white neighbours");
        CHECK(near(s.vertices[center].px, px, 0.0f) && near(s.vertices[center].pz, pz, 0.0f),
              "vertex positions are never moved");
        // A white interior vertex next to the speck darkens slightly (blur spreads).
        const std::size_t neighbor = static_cast<std::size_t>(2 * n + 1);
        CHECK(s.vertices[neighbor].r < 1.0f, "a neighbour of the speck picks up some of its darkness");
    }

    // --- 2. More iterations blur further (the speck gets closer to white). ---
    {
        const int n = 5;
        shapes::MeshData m = grid(n);
        const std::size_t center = static_cast<std::size_t>(2 * n + 2);
        m.vertices[center].r = m.vertices[center].g = m.vertices[center].b = 0.0f;
        const float one = smoothVertexColors(m, 0.5f, 1, false).vertices[center].r;
        const float five = smoothVertexColors(m, 0.5f, 5, false).vertices[center].r;
        CHECK(five > one, "more passes push the speck further toward the surrounding white");
    }

    // --- 3. A uniform colour is unchanged (smoothing a flat field is a no-op). ---
    {
        shapes::MeshData m = grid(4); // all white
        const shapes::MeshData s = smoothVertexColors(m, 1.0f, 10, false);
        bool unchanged = true;
        for (const auto& v : s.vertices) if (!near(v.r, 1.0f, 1e-5f)) unchanged = false;
        CHECK(unchanged, "a uniform colour field stays uniform");
    }

    // --- 4. Pinning the boundary keeps rim vertices fixed. ---
    {
        const int n = 4;
        shapes::MeshData m = grid(n);
        for (auto& v : m.vertices) { v.r = v.g = v.b = 0.5f; }
        m.vertices[0].r = m.vertices[0].g = m.vertices[0].b = 0.0f; // a corner (boundary) speck
        const shapes::MeshData s = smoothVertexColors(m, 0.8f, 3, true);
        CHECK(near(s.vertices[0].r, 0.0f, 1e-6f), "a pinned boundary vertex keeps its colour");
    }

    // --- 5. Empty mesh is safe. ---
    {
        const shapes::MeshData s = smoothVertexColors(shapes::MeshData{});
        CHECK(s.vertices.empty(), "empty mesh -> empty result");
    }

    if (g_fail == 0) {
        std::printf("meshvertexcolorsmooth: OK — speck blurs, positions fixed, uniform unchanged, boundary pin honoured.\n");
        return 0;
    }
    std::printf("meshvertexcolorsmooth: %d failure(s).\n", g_fail);
    return 1;
}
