// tests/render/harriscorners.cpp — verifies Harris corner detection (render HarrisCorners.hpp).
// Ground truths, deterministic (fixed images, no <random>, no clock):
//   * a solid square yields a corner near EACH of its four true corners, and NONE in the flat interior or
//     along the straight edges (the defining "two-direction change" property);
//   * a flat image yields no corners;
//   * a single straight edge yields no interior corners;
//   * determinism.
#include "maz/render/HarrisCorners.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

using maz::render::Color;
using maz::render::Corner;
using maz::render::Image;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static bool near(const std::vector<Corner>& cs, int tx, int ty, int radius) {
    for (const Corner& c : cs)
        if (std::abs(c.x - tx) <= radius && std::abs(c.y - ty) <= radius) return true;
    return false;
}

int main() {
    const int w = 40, h = 40;

    // --- 1. Solid square: four corners, nothing in flat/edge regions. ---
    {
        Image img(w, h, Color{0.0f, 0.0f, 0.0f, 1.0f});
        for (int y = 10; y < 30; ++y)
            for (int x = 10; x < 30; ++x) img.setPixel(x, y, Color{1.0f, 1.0f, 1.0f, 1.0f});
        const std::vector<Corner> cs = maz::render::harrisCorners(img, 0.04f, 0.02f, 3);

        CHECK(near(cs, 10, 10, 2) && near(cs, 29, 10, 2) && near(cs, 10, 29, 2) && near(cs, 29, 29, 2),
              "a corner is found near each of the square's four corners");
        // No corner in the flat interior.
        CHECK(!near(cs, 20, 20, 4), "no corner in the flat interior");
        // No corner in the middle of an edge.
        CHECK(!near(cs, 20, 10, 3) && !near(cs, 10, 20, 3),
              "no corner along the straight edges (only at the true corners)");
        CHECK(cs.size() >= 4 && cs.size() <= 12, "roughly four corner clusters, not a flood");
    }

    // --- 2. Flat image: no corners. ---
    {
        Image flat(w, h, Color{0.5f, 0.5f, 0.5f, 1.0f});
        CHECK(maz::render::harrisCorners(flat, 0.04f, 0.02f, 3).empty(), "a flat image has no corners");
    }

    // --- 3. Straight horizontal edge: no interior corners. ---
    {
        Image edge(w, h);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                edge.setPixel(x, y, Color{y < h / 2 ? 0.1f : 0.9f, y < h / 2 ? 0.1f : 0.9f, y < h / 2 ? 0.1f : 0.9f, 1.0f});
        const std::vector<Corner> cs = maz::render::harrisCorners(edge, 0.04f, 0.05f, 3);
        bool interiorCorner = false;
        for (const Corner& c : cs)
            if (c.x > 3 && c.x < w - 3) interiorCorner = true; // ignore image-border artefacts
        CHECK(!interiorCorner, "a straight edge produces no interior corners");
    }

    // --- 4. Determinism. ---
    {
        Image img(w, h, Color{0.0f, 0.0f, 0.0f, 1.0f});
        for (int y = 12; y < 28; ++y)
            for (int x = 12; x < 28; ++x) img.setPixel(x, y, Color{0.9f, 0.3f, 0.6f, 1.0f});
        const std::vector<Corner> a = maz::render::harrisCorners(img);
        const std::vector<Corner> b = maz::render::harrisCorners(img);
        bool same = a.size() == b.size();
        for (std::size_t i = 0; same && i < a.size(); ++i)
            if (a[i].x != b[i].x || a[i].y != b[i].y) same = false;
        CHECK(same, "identical inputs produce identical corners");
    }

    if (g_fail == 0) {
        std::printf("harriscorners: OK — square corners, no flat/edge false positives, flat empty, edge, determinism.\n");
        return 0;
    }
    std::printf("harriscorners: %d failure(s).\n", g_fail);
    return 1;
}
