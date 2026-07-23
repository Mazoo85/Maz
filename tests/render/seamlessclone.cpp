// tests/render/seamlessclone.cpp — verifies Poisson seamless cloning (render SeamlessClone.hpp).
// Ground truths, deterministic (fixed images, no <random>, no clock):
//   * the defining property: at every SOLVED cell the result's discrete Laplacian equals the SOURCE's
//     (the guidance field), and every cell outside the region equals the DESTINATION exactly;
//   * identity: cloning a region from an image identical to the destination changes nothing;
//   * constant-offset absorption: a source that differs from the destination by a flat brightness offset
//     blends away completely (the seam vanishes) — result equals the destination;
//   * determinism.
#include "maz/render/SeamlessClone.hpp"

#include <cmath>
#include <cstdio>

using maz::render::Color;
using maz::render::Image;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

// A smooth destination background.
static Image makeDest(int w, int h) {
    Image img(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float fx = static_cast<float>(x) / static_cast<float>(w);
            const float fy = static_cast<float>(y) / static_cast<float>(h);
            img.setPixel(x, y, Color{0.2f + 0.5f * fx, 0.3f + 0.4f * fy, 0.5f, 1.0f});
        }
    return img;
}

int main() {
    const int W = 32, H = 28;

    // A source with internal detail (a bright bump), and a rectangular mask region.
    Image source(W, H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const float d = std::sqrt(static_cast<float>((x - 16) * (x - 16) + (y - 14) * (y - 14)));
            const float bump = std::exp(-d * d / 40.0f);
            source.setPixel(x, y, Color{0.6f + 0.3f * bump, 0.4f, 0.7f - 0.2f * bump, 1.0f});
        }
    Image mask(W, H, Color{0, 0, 0, 1});
    for (int y = 8; y < 20; ++y)
        for (int x = 10; x < 22; ++x) mask.setPixel(x, y, Color{1, 1, 1, 1});

    // --- 1. Guidance + boundary property. ---
    {
        const Image dest = makeDest(W, H);
        const Image out = maz::render::seamlessClone(dest, source, mask, 0, 0, 8000, 1e-4f);
        auto rch = [](const Image& im, int x, int y) { return im.getPixel(x, y).r; };
        bool guidance = true, boundary = true;
        for (int y = 1; y < H - 1; ++y)
            for (int x = 1; x < W - 1; ++x) {
                const bool inRegion = mask.getPixel(x, y).r > 0.5f;
                if (inRegion) {
                    const float lapOut = rch(out, x - 1, y) + rch(out, x + 1, y) + rch(out, x, y - 1) +
                                         rch(out, x, y + 1) - 4.0f * rch(out, x, y);
                    const float lapSrc = rch(source, x - 1, y) + rch(source, x + 1, y) +
                                         rch(source, x, y - 1) + rch(source, x, y + 1) -
                                         4.0f * rch(source, x, y);
                    // The Laplacian sums 5 pixels each read back through 8-bit storage (~1/255 rounding),
                    // so ~0.02 of the gap is pure quantization, not solver error.
                    if (std::fabs(lapOut - lapSrc) > 3e-2f) guidance = false;
                } else {
                    if (std::fabs(rch(out, x, y) - rch(dest, x, y)) > 1e-4f) boundary = false;
                }
            }
        CHECK(guidance, "solved interior reproduces the source's gradients (guidance field)");
        CHECK(boundary, "cells outside the region keep the destination exactly (seamless boundary)");
    }

    // --- 2. Identity: source == dest -> unchanged. ---
    {
        const Image dest = makeDest(W, H);
        const Image out = maz::render::seamlessClone(dest, dest, mask, 0, 0, 8000, 1e-5f);
        bool same = true;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                const Color a = out.getPixel(x, y), b = dest.getPixel(x, y);
                if (std::fabs(a.r - b.r) > 6e-3f || std::fabs(a.g - b.g) > 6e-3f || std::fabs(a.b - b.b) > 6e-3f)
                    same = false; // ~1/255 quantization tolerance
            }
        CHECK(same, "cloning from an identical image is a no-op");
    }

    // --- 3. Constant brightness offset is absorbed -> result equals destination. ---
    {
        const Image dest = makeDest(W, H);
        Image shifted(W, H);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                const Color d = dest.getPixel(x, y);
                shifted.setPixel(x, y, Color{d.r * 0.0f + d.r + 0.2f, d.g + 0.2f, d.b - 0.15f, 1.0f});
            }
        const Image out = maz::render::seamlessClone(dest, shifted, mask, 0, 0, 8000, 1e-5f);
        bool absorbed = true;
        for (int y = 9; y < 19; ++y)
            for (int x = 11; x < 21; ++x) {
                const Color a = out.getPixel(x, y), b = dest.getPixel(x, y);
                if (std::fabs(a.r - b.r) > 8e-3f || std::fabs(a.g - b.g) > 8e-3f || std::fabs(a.b - b.b) > 8e-3f)
                    absorbed = false;
            }
        CHECK(absorbed, "a flat brightness offset blends away (seam vanishes)");
    }

    // --- 4. Determinism. ---
    {
        const Image dest = makeDest(W, H);
        const Image a = maz::render::seamlessClone(dest, source, mask, 0, 0, 2000, 1e-3f);
        const Image b = maz::render::seamlessClone(dest, source, mask, 0, 0, 2000, 1e-3f);
        CHECK(a.data() == b.data(), "identical inputs produce identical output");
    }

    if (g_fail == 0) {
        std::printf("seamlessclone: OK — guidance+boundary, identity, constant-offset absorption, determinism.\n");
        return 0;
    }
    std::printf("seamlessclone: %d failure(s).\n", g_fail);
    return 1;
}
