#pragma once

#include "maz/math/Poisson.hpp"    // poissonSolve
#include "maz/render/Image.hpp"    // Image, Color

#include <cstdint>
#include <vector>

// maz::render::seamlessClone — Poisson image editing (Pérez et al. 2003): paste a patch of one image into
// another so the seam DISAPPEARS. Naively copying pixels leaves a hard edge whenever the patch's lighting or
// colour differs from its new surroundings. Seamless cloning instead copies the patch's GRADIENTS (its
// internal detail) while forcing its border to match the destination exactly, then solves a Poisson equation
// to fill the interior — so the patch keeps its texture and shapes but its overall tone slides to blend
// perfectly with the background. This is how "healing brush" / seamless compositing works, and it is handy
// for decals, damage overlays, terrain-splat blending, and stitching texture tiles without visible joins.
// Solves one Poisson problem per colour channel with the destination as the boundary condition. Godot has no
// gradient-domain compositing. Header-only, deterministic; built on math::poissonSolve.
namespace maz::render {

// Clone `source` into a copy of `dest` over the region where `mask` is bright (red > 0.5), placing source
// pixel (sx,sy) at destination pixel (sx+offX, sy+offY). Returns the blended image (same size as dest).
// `iterations`/`tol` control the Poisson relaxation.
inline Image seamlessClone(const Image& dest, const Image& source, const Image& mask, int offX, int offY,
                           int iterations = 4000, float tol = 1e-3f) {
    Image out = dest;
    const int w = dest.width(), h = dest.height();
    if (w < 3 || h < 3) {
        return out;
    }
    const std::size_t n = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);

    // A destination cell is FREE (solved) if the mask marks it, it is not on the image border, and the whole
    // source stencil it needs is in range; everything else is FIXED to the destination.
    std::vector<std::uint8_t> fixed(n, 1);
    auto srcIn = [&](int sx, int sy) { return sx >= 0 && sy >= 0 && sx < source.width() && sy < source.height(); };
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            if (mask.getPixel(x, y).r <= 0.5f) {
                continue;
            }
            const int sx = x - offX, sy = y - offY;
            if (srcIn(sx - 1, sy) && srcIn(sx + 1, sy) && srcIn(sx, sy - 1) && srcIn(sx, sy + 1)) {
                fixed[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] = 0;
            }
        }
    }

    // Solve each channel: guidance rhs = discrete Laplacian of the source; boundary = destination.
    for (int ch = 0; ch < 3; ++ch) {
        std::vector<float> u(n, 0.0f), rhs(n, 0.0f);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const std::size_t idx =
                    static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x);
                const Color dc = dest.getPixel(x, y);
                u[idx] = ch == 0 ? dc.r : (ch == 1 ? dc.g : dc.b);
                if (!fixed[idx]) {
                    const int sx = x - offX, sy = y - offY;
                    auto sch = [&](int ax, int ay) {
                        const Color c = source.getPixel(ax, ay);
                        return ch == 0 ? c.r : (ch == 1 ? c.g : c.b);
                    };
                    rhs[idx] = sch(sx - 1, sy) + sch(sx + 1, sy) + sch(sx, sy - 1) + sch(sx, sy + 1) -
                               4.0f * sch(sx, sy);
                }
            }
        }
        maz::math::poissonSolve(w, h, u, rhs, fixed, iterations, tol);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const std::size_t idx =
                    static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x);
                if (fixed[idx]) {
                    continue;
                }
                float v = u[idx];
                v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
                Color c = out.getPixel(x, y);
                if (ch == 0) c.r = v; else if (ch == 1) c.g = v; else c.b = v;
                out.setPixel(x, y, c);
            }
        }
    }
    return out;
}

} // namespace maz::render
