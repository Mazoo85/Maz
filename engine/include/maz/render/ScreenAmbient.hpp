#pragma once

#include "maz/render/DepthOfField.hpp" // viewDistance — depth back into metres
#include "maz/render/Image.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

// maz::render CONTACT SHADING from the depth buffer.
//
// Where two surfaces meet, light gets trapped and it goes dark. Nothing else does as much to make an
// object REST on a floor rather than sit pasted in front of one, and a renderer without it produces
// rooms where every piece of furniture hovers very slightly.
//
// WHY NOT THE BAKER. The engine already has `bakeVertexAO`, which is the textbook answer, and it was
// tried first. It does not work on these rooms, for a reason worth keeping: the sets are built from
// large boxes, so a wall has four corners and no vertices anywhere in between. Per-vertex occlusion
// can only darken those corners and interpolate across the whole wall, which is a soft gradient
// rather than a shadow in a corner. Inside a closed room it is worse than that — nearly every vertex
// is occluded by the room itself, so the bake mostly just turns the lights down. Rendered and looked
// at, it was worse than nothing. It would become the right tool the day the sets are built from more
// than a few hundred triangles.
//
// This works in screen space instead, from the depth buffer already read back for the lens. It does
// not care how anything is tessellated, it costs the same on a box as on a carving, and it works on
// the PEOPLE — who are rebuilt from scratch every frame and could never have been baked.
namespace maz::render {

// Darken what is closed in on.
//
//   img         the frame, in linear light, before the print curve
//   y0, y1      the rows the picture occupies
//   depth       one depth reading per pixel of that band, row-major, width `img.width()`
//   nearPlane   the projection's own, so depths become metres
//   farPlane
//   radius      how far, in metres, a surface has to be to stop counting as closing in. A room's
//               corner wants about half a metre; a figure's feet want less.
//   strength    0 off, 1 takes the light out of a fully enclosed corner
inline void screenAmbientOcclusion(Image& img, int y0, int y1, const std::vector<float>& depth,
                                   float nearPlane, float farPlane, float radius, float strength) {
    const int w = img.width();
    const int rows = y1 - y0;
    if (strength <= 0.0001f || radius <= 0.0f || rows < 3 || w < 3) {
        return;
    }
    if (depth.size() < static_cast<std::size_t>(w) * static_cast<std::size_t>(rows)) {
        return;
    }

    const float fw = static_cast<float>(w);
    // How much world one pixel spans at one metre. Taken from a typical film lens rather than passed
    // in: it only sets the scale at which the radius is interpreted, and being ten per cent out moves
    // the shading by less than the eye can find.
    const float perPixel = 1.0f / fw;

    // Worked out at HALF resolution, and stretched back over the frame at the end.
    //
    // Occlusion in a corner is a soft, wide thing — there is no detail in it finer than a few pixels —
    // so computing it per pixel spends four times the samples on an answer that is then almost the
    // same for every one of those four. At full resolution this pass cost 10.2 ms of a 38 ms frame,
    // which the budget does not have; at half it is a quarter of that for a picture nobody can tell
    // apart. The nearest of the four depths is kept rather than the average, so an edge stays an edge
    // instead of being blurred into the surface behind it.
    const int hw = w / 2 < 2 ? 2 : w / 2;
    const int hh = rows / 2 < 2 ? 2 : rows / 2;
    std::vector<float> away(static_cast<std::size_t>(hw) * static_cast<std::size_t>(hh), 0.0f);
    for (int y = 0; y < hh; ++y) {
        for (int x = 0; x < hw; ++x) {
            float nearest = 1e30f;
            for (int dy = 0; dy < 2; ++dy) {
                for (int dx = 0; dx < 2; ++dx) {
                    const int sx = x * 2 + dx < w ? x * 2 + dx : w - 1;
                    const int sy = y * 2 + dy < rows ? y * 2 + dy : rows - 1;
                    const float d = depth[static_cast<std::size_t>(sy) * static_cast<std::size_t>(w) +
                                          static_cast<std::size_t>(sx)];
                    const float metres = viewDistance(d, nearPlane, farPlane);
                    nearest = metres < nearest ? metres : nearest;
                }
            }
            away[static_cast<std::size_t>(y) * static_cast<std::size_t>(hw) +
                 static_cast<std::size_t>(x)] = nearest;
        }
    }

    // Eight neighbours on a ring, at two radii. A ring rather than a random spray because the pattern
    // is fixed and the same every frame — a spray that changed frame to frame would crawl.
    const int kRing = 8;
    static const float kDx[8] = {1.0f, 0.707f, 0.0f, -0.707f, -1.0f, -0.707f, 0.0f, 0.707f};
    static const float kDy[8] = {0.0f, 0.707f, 1.0f, 0.707f, 0.0f, -0.707f, -1.0f, -0.707f};

    std::vector<float> shade(static_cast<std::size_t>(hw) * static_cast<std::size_t>(hh), 0.0f);

    for (int y = 0; y < hh; ++y) {
        for (int x = 0; x < hw; ++x) {
            const std::size_t p =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(hw) + static_cast<std::size_t>(x);
            const float here = away[p];
            // The sky, skipped. Not for correctness — the fade below would reject everything a sky
            // pixel could find anyway — but because outdoors the sky is half the frame, and this
            // skips a ring of samples for every pixel of it.
            if (here >= farPlane * 0.9f) {
                continue;
            }
            // How many pixels half a metre spans at this distance. Near things get a wide ring and far
            // things a tight one, which is what keeps the effect the same SIZE IN THE WORLD rather
            // than the same size on screen.
            float span = radius / (here * perPixel) * 0.5f;   // half resolution, half the pixels
            // And capped hard. A contact shadow is a few pixels wide; a ring that reaches a
            // twentieth of the frame is not finding corners, it is comparing one wall with another
            // wall across the room, which came back as long dark smears down every surface that ran
            // away from the camera. Near geometry is where the cap bites, and near geometry is
            // exactly where an over-wide reach does the most damage.
            if (span > fw * 0.009f) {
                span = fw * 0.009f;
            }
            // The floor is applied AFTER the cap, so it wins. Applied before, the cap could shrink the
            // ring below a pixel on a small frame and the eight samples all landed on the same pixel.
            if (span < 2.0f) {
                span = 2.0f;
            }

            // Which way this surface is sloping, in metres of distance per pixel.
            //
            // This is what makes the difference between contact shading and a renderer that darkens
            // every floor along its whole length. Asking "is my neighbour nearer than me?" is not the
            // question: on a floor running away from the camera, EVERY pixel has a nearer neighbour,
            // and the answer is yes everywhere. The question is whether the neighbour is nearer than
            // the surface I am standing on would have put it — whether it rises off my own plane.
            //
            // Of the two differences either side, the SMALLER is used. At the edge of an object one
            // side steps off a cliff and the other stays on the surface; taking the gentler of the two
            // keeps the estimate on the surface the pixel actually belongs to.
            // Measured over the same distance it is about to be used over, not over one pixel.
            //
            // A one-pixel measurement extrapolated fifty pixels is the second thing that went wrong
            // here, and it is visible: a wall seen at a grazing angle changes distance so fast per
            // pixel that a one-pixel slope, carried out to the edge of the ring, is metres out — and
            // the frame came back with long dark smears running down every wall that ran away from
            // the camera. Sampling the slope at the ring's own radius makes the estimate answer the
            // question actually being asked.
            const int reach = static_cast<int>(span) < 1 ? 1 : static_cast<int>(span);
            float slopeX = 0.0f, slopeY = 0.0f;
            {
                const int lx = x - reach < 0 ? 0 : x - reach;
                const int rx = x + reach >= hw ? hw - 1 : x + reach;
                const float a = lx < x ? (here - away[p - static_cast<std::size_t>(x - lx)]) /
                                             static_cast<float>(x - lx)
                                       : 0.0f;
                const float b = rx > x ? (away[p + static_cast<std::size_t>(rx - x)] - here) /
                                             static_cast<float>(rx - x)
                                       : 0.0f;
                // The gentler of the two sides. At the edge of an object one side steps off a cliff
                // and the other stays on the surface; the gentler one is the surface this pixel
                // actually belongs to.
                slopeX = std::fabs(a) < std::fabs(b) ? a : b;

                const int uy = y - reach < 0 ? 0 : y - reach;
                const int dy = y + reach >= hh ? hh - 1 : y + reach;
                const float c =
                    uy < y ? (here - away[p - static_cast<std::size_t>(y - uy) *
                                                  static_cast<std::size_t>(hw)]) /
                                 static_cast<float>(y - uy)
                           : 0.0f;
                const float d =
                    dy > y ? (away[p + static_cast<std::size_t>(dy - y) *
                                        static_cast<std::size_t>(hw)] -
                              here) /
                                 static_cast<float>(dy - y)
                           : 0.0f;
                slopeY = std::fabs(c) < std::fabs(d) ? c : d;
            }

            float occluded = 0.0f;
            int taken = 0;
            for (int pass = 0; pass < 2; ++pass) {
                const float at = pass == 0 ? span * 0.45f : span;
                for (int i = 0; i < kRing; ++i) {
                    const int sx = static_cast<int>(static_cast<float>(x) + kDx[i] * at + 0.5f);
                    const int sy = static_cast<int>(static_cast<float>(y) + kDy[i] * at + 0.5f);
                    if (sx < 0 || sy < 0 || sx >= hw || sy >= hh) {
                        continue;
                    }
                    // The offset to the pixel ACTUALLY sampled, not the one asked for. Sampling is on
                    // whole pixels, so predicting the surface from the unrounded offset leaves up to
                    // half a pixel of disagreement — which on a steeply receding floor is several
                    // centimetres, comfortably more than the threshold below, and darkens the entire
                    // floor. The rounding has to happen before the prediction, not after it.
                    const float ox = static_cast<float>(sx - x);
                    const float oy = static_cast<float>(sy - y);
                    const float there =
                        away[static_cast<std::size_t>(sy) * static_cast<std::size_t>(hw) +
                             static_cast<std::size_t>(sx)];
                    ++taken;
                    // Where this surface, carried on, would have put that neighbour.
                    const float expected = here + slopeX * ox + slopeY * oy;
                    // How far it stands in FRONT of that — how much it rises off my own plane.
                    const float above = expected - there;
                    if (above <= radius * 0.06f) {
                        continue;   // on my surface, or behind it: not closing in on anything
                    }
                    // There were two more guards here — an absolute cap on how far in front a sample
                    // could be, and a relative one — both aimed at the halo a silhouette would
                    // otherwise draw around every figure. Mutation testing could not kill either of
                    // them, and instrumenting the case showed why: the fade below already falls to
                    // zero well before a silhouette's distance, so both were dead code. Deleted
                    // rather than kept as reassurance, because a guard that cannot fail is a guard
                    // nobody can reason about.
                    // Strongest right at the join and fading out to the edge of the reach — and
                    // never below zero.
                    //
                    // Without the clamp this term goes NEGATIVE for anything further in front than
                    // the reach, and a negative contribution does not reject a sample, it CANCELS
                    // the others: a pixel with one genuine corner beside it and one distant object
                    // in the ring came out unshaded, the corner silently subtracted away. It also
                    // made the guards above impossible to test, because the negative was quietly
                    // doing their job for them and doing it wrong.
                    const float fade = 1.0f - (above - radius * 0.06f) / (radius * 2.2f);
                    occluded += fade > 0.0f ? fade : 0.0f;
                }
            }
            if (taken > 0) {
                shade[p] = occluded / static_cast<float>(taken);
            }
        }
    }

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < w; ++x) {
            // Read back with bilinear interpolation, so the half-resolution grid does not show up as
            // a staircase along every edge it darkens.
            const float fx = (static_cast<float>(x) - 0.5f) * 0.5f;
            const float fy = (static_cast<float>(y) - 0.5f) * 0.5f;
            int x0 = static_cast<int>(fx < 0.0f ? 0.0f : fx);
            int y0s = static_cast<int>(fy < 0.0f ? 0.0f : fy);
            if (x0 > hw - 2) x0 = hw - 2;
            if (y0s > hh - 2) y0s = hh - 2;
            const float tx = fx - static_cast<float>(x0) < 0.0f
                                 ? 0.0f
                                 : (fx - static_cast<float>(x0) > 1.0f ? 1.0f : fx - static_cast<float>(x0));
            const float ty = fy - static_cast<float>(y0s) < 0.0f
                                 ? 0.0f
                                 : (fy - static_cast<float>(y0s) > 1.0f ? 1.0f
                                                                        : fy - static_cast<float>(y0s));
            const std::size_t row0 = static_cast<std::size_t>(y0s) * static_cast<std::size_t>(hw);
            const std::size_t row1 = row0 + static_cast<std::size_t>(hw);
            const float topLeft = shade[row0 + static_cast<std::size_t>(x0)];
            const float topRight = shade[row0 + static_cast<std::size_t>(x0) + 1u];
            const float lowLeft = shade[row1 + static_cast<std::size_t>(x0)];
            const float lowRight = shade[row1 + static_cast<std::size_t>(x0) + 1u];
            const float top = topLeft + (topRight - topLeft) * tx;
            const float low = lowLeft + (lowRight - lowLeft) * tx;
            const float here = top + (low - top) * ty;
            if (here <= 0.0f) {
                continue;
            }
            float k = 1.0f - here * strength;
            if (k < 0.0f) {
                k = 0.0f;
            }
            const Color c = img.getPixel(x, y0 + y);
            img.setPixel(x, y0 + y, Color{c.r * k, c.g * k, c.b * k, c.a});
        }
    }
}

} // namespace maz::render
