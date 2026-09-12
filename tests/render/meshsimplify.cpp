// tests/render/meshsimplify.cpp — verifies mesh simplification by vertex clustering
// (render::simplifyClustering). Invariants of a correct decimation: the output has FEWER vertices and
// triangles, every output triangle is non-degenerate (3 distinct indices) and in range, the bounding box is
// preserved within about one cell, and a cell size below the vertex spacing is a no-op. Pure CPU, headless.
#include "maz/render/MeshSimplify.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

// A WxH grid of unit-spaced vertices in the XZ plane, two triangles per quad.
static shapes::MeshData gridMesh(int w, int h) {
    shapes::MeshData m;
    for (int y = 0; y <= h; ++y) {
        for (int x = 0; x <= w; ++x) {
            MeshVertex v{};
            v.px = static_cast<float>(x);
            v.py = 0.0f;
            v.pz = static_cast<float>(y);
            v.ny = 1.0f;
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

static void bbox(const shapes::MeshData& m, float& mnx, float& mnz, float& mxx, float& mxz) {
    mnx = mnz = 1e9f; mxx = mxz = -1e9f;
    for (const MeshVertex& v : m.vertices) {
        mnx = std::min(mnx, v.px); mxx = std::max(mxx, v.px);
        mnz = std::min(mnz, v.pz); mxz = std::max(mxz, v.pz);
    }
}

int main() {
    const int w = 16, h = 16;
    const shapes::MeshData src = gridMesh(w, h); // 17*17=289 verts, 512 tris

    // --- 1. Clustering at cell size 4 (units) reduces vertices and triangles. ---
    {
        const shapes::MeshData out = simplifyClustering(src, 4.0f);
        CHECK(out.vertices.size() < src.vertices.size(), "vertex count reduced");
        CHECK(out.indices.size() < src.indices.size(), "triangle count reduced");
        CHECK(out.indices.size() % 3 == 0, "still a triangle list");

        // Every triangle non-degenerate + in range.
        bool ok = true;
        for (std::size_t t = 0; t + 2 < out.indices.size(); t += 3) {
            const std::uint32_t a = out.indices[t], b = out.indices[t + 1], c = out.indices[t + 2];
            if (a == b || b == c || a == c) ok = false;
            if (a >= out.vertices.size() || b >= out.vertices.size() || c >= out.vertices.size()) ok = false;
        }
        CHECK(ok, "all output triangles are non-degenerate and in range");

        // Bounding box preserved within ~one cell.
        float smnx, smnz, smxx, smxz, omnx, omnz, omxx, omxz;
        bbox(src, smnx, smnz, smxx, smxz);
        bbox(out, omnx, omnz, omxx, omxz);
        CHECK(std::fabs(omxx - smxx) <= 4.0f && std::fabs(omxz - smxz) <= 4.0f, "bbox max within one cell");
        CHECK(std::fabs(omnx - smnx) <= 4.0f && std::fabs(omnz - smnz) <= 4.0f, "bbox min within one cell");
    }

    // --- 2. Coarser cells reduce more aggressively (monotone in cell size). ---
    {
        const std::size_t v2 = simplifyClustering(src, 2.0f).vertices.size();
        const std::size_t v8 = simplifyClustering(src, 8.0f).vertices.size();
        CHECK(v8 < v2, "a larger cell yields fewer vertices");
        CHECK(v8 >= 4, "even the coarsest grid keeps enough verts to form the quad's corners");
    }

    // --- 3. A cell finer than the vertex spacing keeps every vertex (no merges). ---
    {
        const shapes::MeshData out = simplifyClustering(src, 0.5f);
        CHECK(out.vertices.size() == src.vertices.size(), "sub-spacing cell merges nothing");
        CHECK(out.indices.size() == src.indices.size(), "no triangles dropped when nothing merges");
    }

    if (g_fail == 0) {
        std::printf("meshsimplify: OK — reduces verts/tris, no degenerates, bbox + no-op preserved.\n");
        return 0;
    }
    std::printf("meshsimplify: %d failure(s).\n", g_fail);
    return 1;
}
