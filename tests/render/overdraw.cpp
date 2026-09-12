// tests/render/overdraw.cpp — verifies overdraw optimization (render::optimizeOverdraw /
// render::simulateOverdraw). A correct front-to-back reorder: is a pure index PERMUTATION (same triangle
// set, positions untouched), covers the same pixels, and — measured by the software-rasterized overdraw
// simulator — SHADES FEWER fragments than the original back-to-front order, approaching 1 shade per covered
// pixel. Pure CPU, headless.
#include "maz/render/OverdrawOptimize.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

// N unit quads (2 triangles each) stacked along +Z at increasing depth, all covering the same XY footprint.
// Inserted in BACK-TO-FRONT order (farthest first) — the worst case for early-Z, so every layer shades.
static shapes::MeshData stackedQuads(int n) {
    shapes::MeshData m;
    for (int i = 0; i < n; ++i) {
        const float z = static_cast<float>(n - 1 - i); // first quad is farthest (largest z)
        const std::uint32_t base = static_cast<std::uint32_t>(m.vertices.size());
        const float xy[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        for (const auto& c : xy) {
            MeshVertex v{};
            v.px = c[0]; v.py = c[1]; v.pz = z;
            v.nz = -1.0f; v.r = v.g = v.b = 1.0f;
            m.vertices.push_back(v);
        }
        m.indices.insert(m.indices.end(),
                         {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    return m;
}

// Canonicalize the triangle set: each triangle as its three indices sorted, then the whole list sorted.
static std::vector<std::array<std::uint32_t, 3>> triSet(const shapes::MeshData& m) {
    std::vector<std::array<std::uint32_t, 3>> tris;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        std::array<std::uint32_t, 3> tri{m.indices[t], m.indices[t + 1], m.indices[t + 2]};
        std::sort(tri.begin(), tri.end());
        tris.push_back(tri);
    }
    std::sort(tris.begin(), tris.end());
    return tris;
}

int main() {
    const int n = 8;
    const shapes::MeshData src = stackedQuads(n);
    const maz::math::vec3 view(0, 0, 1); // looking along +Z; nearest = smallest z

    // --- 1. The original back-to-front order overdraws heavily. ---
    const OverdrawStats before = simulateOverdraw(src, view, 32);
    CHECK(before.coveredPixels > 0, "simulator covers pixels");
    CHECK(before.overdraw > 3.0f, "back-to-front order overdraws (many shaded fragments per pixel)");

    // --- 2. optimizeOverdraw reorders front-to-back and cuts overdraw toward 1. ---
    const shapes::MeshData opt = optimizeOverdraw(src, view);
    const OverdrawStats after = simulateOverdraw(opt, view, 32);
    CHECK(after.shadedFragments < before.shadedFragments, "optimized shades strictly fewer fragments");
    CHECK(after.overdraw < before.overdraw, "optimized overdraw is lower");
    CHECK(after.overdraw <= 1.01f, "optimized overdraw approaches the ideal of 1 shade per pixel");
    CHECK(after.coveredPixels == before.coveredPixels, "same pixels covered (geometry unchanged)");

    // --- 3. It is a pure permutation: identical vertices and identical triangle set. ---
    CHECK(opt.vertices.size() == src.vertices.size(), "vertex list length unchanged");
    bool vertsSame = true;
    for (std::size_t i = 0; i < opt.vertices.size(); ++i)
        if (opt.vertices[i].px != src.vertices[i].px || opt.vertices[i].py != src.vertices[i].py ||
            opt.vertices[i].pz != src.vertices[i].pz) { vertsSame = false; break; }
    CHECK(vertsSame, "vertex positions untouched");
    CHECK(opt.indices.size() == src.indices.size(), "index count unchanged");
    CHECK(triSet(opt) == triSet(src), "triangle set preserved exactly (permutation only)");

    // --- 4. Degenerate view direction is a safe no-op copy. ---
    const shapes::MeshData none = optimizeOverdraw(src, maz::math::vec3(0, 0, 0));
    CHECK(none.indices == src.indices, "zero view direction returns the mesh unchanged");

    if (g_fail == 0) {
        std::printf("overdraw: OK — front-to-back reorder cuts shaded fragments, permutation preserved.\n");
        return 0;
    }
    std::printf("overdraw: %d failure(s).\n", g_fail);
    return 1;
}
