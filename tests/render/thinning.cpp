// tests/render/thinning.cpp — verifies Zhang-Suen morphological thinning (render Thinning.hpp).
// Ground truths, deterministic (fixed shapes, no <random>, no clock):
//   * SUBSET: the skeleton only ever removes pixels (output ⊆ input);
//   * THINNESS: no 2x2 block of foreground remains (the result is one pixel wide);
//   * CONNECTIVITY preserved: a connected shape thins to a shape with the same number of components;
//   * a solid rectangle skeletonizes to far fewer pixels; idempotence; empty stays empty.
#include "maz/render/Thinning.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

// Count 8-connected foreground components via flood fill.
static int components(int w, int h, const std::vector<std::uint8_t>& g) {
    std::vector<std::uint8_t> seen(g.size(), 0);
    int comps = 0;
    std::vector<int> stack;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const std::size_t idx = static_cast<std::size_t>(y * w + x);
            if (!g[idx] || seen[idx]) continue;
            ++comps;
            stack.clear();
            stack.push_back(y * w + x);
            seen[idx] = 1;
            while (!stack.empty()) {
                const int cur = stack.back();
                stack.pop_back();
                const int cx = cur % w, cy = cur / w;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        const int nx = cx + dx, ny = cy + dy;
                        if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                        const std::size_t ni = static_cast<std::size_t>(ny * w + nx);
                        if (g[ni] && !seen[ni]) { seen[ni] = 1; stack.push_back(ny * w + nx); }
                    }
            }
        }
    return comps;
}

static bool has2x2(int w, int h, const std::vector<std::uint8_t>& g) {
    for (int y = 0; y + 1 < h; ++y)
        for (int x = 0; x + 1 < w; ++x) {
            const std::size_t i = static_cast<std::size_t>(y * w + x);
            if (g[i] && g[i + 1] && g[static_cast<std::size_t>((y + 1) * w + x)] &&
                g[static_cast<std::size_t>((y + 1) * w + x + 1)])
                return true;
        }
    return false;
}

int main() {
    const int w = 32, h = 32;

    // A solid rectangle in the middle.
    std::vector<std::uint8_t> rect(static_cast<std::size_t>(w * h), 0);
    for (int y = 6; y < 26; ++y)
        for (int x = 4; x < 28; ++x) rect[static_cast<std::size_t>(y * w + x)] = 1;

    // --- 1. Subset + thinness + connectivity + reduction. ---
    {
        const std::vector<std::uint8_t> sk = maz::render::thinZhangSuen(w, h, rect);
        bool subset = true;
        int inCount = 0, outCount = 0;
        for (std::size_t i = 0; i < rect.size(); ++i) {
            if (rect[i]) ++inCount;
            if (sk[i]) ++outCount;
            if (sk[i] && !rect[i]) subset = false;
        }
        CHECK(subset, "skeleton only removes pixels (output is a subset of input)");
        CHECK(!has2x2(w, h, sk), "no 2x2 foreground block remains (result is one pixel wide)");
        CHECK(components(w, h, rect) == 1 && components(w, h, sk) == 1,
              "connectivity preserved (still one component)");
        CHECK(outCount < inCount / 4, "the skeleton has far fewer pixels than the solid shape");
        CHECK(outCount > 0, "a non-empty shape yields a non-empty skeleton");
    }

    // --- 2. Idempotence. ---
    {
        const std::vector<std::uint8_t> sk = maz::render::thinZhangSuen(w, h, rect);
        const std::vector<std::uint8_t> sk2 = maz::render::thinZhangSuen(w, h, sk);
        CHECK(sk == sk2, "thinning an already-thin skeleton changes nothing");
    }

    // --- 3. Two separate blobs stay two components. ---
    {
        std::vector<std::uint8_t> two(static_cast<std::size_t>(w * h), 0);
        for (int y = 4; y < 12; ++y) for (int x = 4; x < 12; ++x) two[static_cast<std::size_t>(y * w + x)] = 1;
        for (int y = 18; y < 28; ++y) for (int x = 18; x < 28; ++x) two[static_cast<std::size_t>(y * w + x)] = 1;
        const std::vector<std::uint8_t> sk = maz::render::thinZhangSuen(w, h, two);
        CHECK(components(w, h, two) == 2 && components(w, h, sk) == 2, "two blobs remain two skeletons");
    }

    // --- 4. Empty + determinism. ---
    {
        std::vector<std::uint8_t> empty(static_cast<std::size_t>(w * h), 0);
        const std::vector<std::uint8_t> se = maz::render::thinZhangSuen(w, h, empty);
        bool allZero = true;
        for (std::uint8_t v : se) if (v) allZero = false;
        CHECK(allZero, "empty image stays empty");
        CHECK(maz::render::thinZhangSuen(w, h, rect) == maz::render::thinZhangSuen(w, h, rect), "deterministic");
    }

    if (g_fail == 0) {
        std::printf("thinning: OK — subset, thinness, connectivity, reduction, idempotence, two-blob, empty.\n");
        return 0;
    }
    std::printf("thinning: %d failure(s).\n", g_fail);
    return 1;
}
