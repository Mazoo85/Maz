// tests/render/vertexcache.cpp — verifies vertex-cache optimization (render::optimizeVertexCache). Two
// invariants that pin down correctness: (1) the reordered index list is a PERMUTATION OF THE SAME TRIANGLES
// (same geometry, nothing added/dropped/altered), and (2) the simulated-cache ACMR drops versus the naive
// row-major grid ordering. Pure integer work, headless.
#include "maz/render/VertexCacheOptimize.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <set>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

// Build a WxH quad grid, emitted row by row (the classic cache-hostile order).
static std::vector<std::uint32_t> gridIndices(int w, int h) {
    std::vector<std::uint32_t> idx;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::uint32_t a = static_cast<std::uint32_t>(y * (w + 1) + x);
            const std::uint32_t b = a + 1;
            const std::uint32_t c = a + static_cast<std::uint32_t>(w + 1);
            const std::uint32_t d = c + 1;
            idx.insert(idx.end(), {a, b, c, b, d, c});
        }
    }
    return idx;
}

// A canonical key for a triangle: its three vertices sorted (order-independent set membership).
static std::array<std::uint32_t, 3> triKey(const std::vector<std::uint32_t>& idx, std::size_t t) {
    std::array<std::uint32_t, 3> k = {idx[t * 3], idx[t * 3 + 1], idx[t * 3 + 2]};
    std::sort(k.begin(), k.end());
    return k;
}

int main() {
    const int w = 24, h = 24;
    const std::size_t vertexCount = static_cast<std::size_t>((w + 1) * (h + 1));
    const std::vector<std::uint32_t> before = gridIndices(w, h);

    const std::vector<std::uint32_t> after = optimizeVertexCache(before, vertexCount);

    // --- 1. Same number of indices/triangles. ---
    CHECK(after.size() == before.size(), "index count unchanged");
    CHECK(after.size() % 3 == 0, "still a triangle list");

    // --- 2. Exactly the same multiset of triangles (nothing added, dropped, or altered). ---
    {
        std::multiset<std::array<std::uint32_t, 3>> sb, sa;
        for (std::size_t t = 0; t < before.size() / 3; ++t) sb.insert(triKey(before, t));
        for (std::size_t t = 0; t < after.size() / 3; ++t) sa.insert(triKey(after, t));
        CHECK(sb == sa, "reordering preserves the exact triangle set (lossless permutation)");
    }

    // --- 3. ACMR improves versus the naive row-major order. ---
    {
        const float acmrBefore = simulateAcmr(before, 32);
        const float acmrAfter = simulateAcmr(after, 32);
        std::printf("  ACMR before=%.3f after=%.3f\n", static_cast<double>(acmrBefore),
                    static_cast<double>(acmrAfter));
        CHECK(acmrAfter < acmrBefore, "optimized order has a lower ACMR (better cache reuse)");
        CHECK(acmrAfter < 1.0f, "optimized ACMR is well under 1 (strong vertex reuse)");
    }

    // --- 4. Degenerate inputs are handled. ---
    {
        CHECK(optimizeVertexCache({}, 0).empty(), "empty mesh -> empty");
        const std::vector<std::uint32_t> oneTri = {0, 1, 2};
        const std::vector<std::uint32_t> optTri = optimizeVertexCache(oneTri, 3);
        CHECK(optTri.size() == 3, "single triangle preserved");
    }

    if (g_fail == 0) {
        std::printf("vertexcache: OK — lossless permutation + ACMR improvement.\n");
        return 0;
    }
    std::printf("vertexcache: %d failure(s).\n", g_fail);
    return 1;
}
