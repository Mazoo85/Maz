#pragma once

#include "maz/render/Image.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

// maz::render DEPTH OF FIELD — the one thing a lens does that a pinhole does not.
//
// Everything a rasteriser draws is in focus, because a rasteriser is a pinhole camera: every point in
// the world maps to exactly one pixel however far away it is. Real glass cannot do that. A lens focuses
// at ONE distance and everything else lands on the film as a small disc rather than a point, and the
// size of that disc is most of what separates a photograph from a diagram.
//
// It is also the cheapest way to say "look here". A close-up with the room sharp behind it is a
// snapshot; the same close-up with the room fallen away is a close-up. Nothing else available at this
// price does as much to the picture.
//
// HOW IT IS DONE, and why not the obvious way. The obvious way is to gather a disc of samples per
// pixel with a radius that varies per pixel, which is correct and costs a hundred taps at every pixel
// of every frame. Instead two blurred copies of the frame are made at fixed radii — each one two
// separable box passes, so a handful of adds per pixel — and the result is mixed between sharp, soft
// and softer by how far out of focus that pixel is. It is the standard trick, it is wrong at the edges
// of a sharp object in front of a blurred one, and at the size a film like this plays at nobody has
// ever noticed.
namespace maz::render {

// How far away a pixel actually is, in metres, from what the depth buffer stored.
//
// A depth buffer does not store distance. It stores z/w after the projection, which is crammed up
// against the far plane — half the range is spent on the first few metres. Blurring by THAT is a
// camera that focuses on everything past ten metres equally, which is not what a lens does, so it has
// to be turned back into a distance first.
inline float viewDistance(float depth, float nearPlane, float farPlane) {
    const float d = depth < 0.0f ? 0.0f : (depth > 0.9999f ? 0.9999f : depth);
    const float denom = farPlane - d * (farPlane - nearPlane);
    return denom > 1e-6f ? nearPlane * farPlane / denom : farPlane;
}

// How out of focus a point at this distance is: 0 at the focus distance, rising to 1 well away from
// it. The shape is the thin-lens one — the difference of the reciprocals — which is why the
// foreground goes soft so much faster than the background, and why a face a metre in front of a wall
// is separated from it while two trees fifty metres away are not separated from each other.
inline float blurAmount(float distance, float focus, float aperture) {
    if (distance <= 1e-4f || focus <= 1e-4f || aperture <= 0.0f) {
        return 0.0f;
    }
    const float coc = std::fabs(1.0f / focus - 1.0f / distance) * aperture;
    return coc > 1.0f ? 1.0f : coc;
}

namespace dofdetail {

// One separable box blur pass, in place, over a band of rows. Two of these in a row is close enough to
// a Gaussian that nothing at this size can tell, and it costs the same however wide the radius is.
inline void boxBlur(std::vector<float>& rgb, std::vector<float>& tmp, int w, int y0, int y1,
                    int radius) {
    if (radius < 1) {
        return;
    }
    // Resized, not cleared: the across pass writes every element of the band before the down pass
    // reads any of it, so zeroing first is a third of a megabyte of memset for nothing.
    if (tmp.size() < rgb.size()) {
        tmp.resize(rgb.size());
    }
    const float inv = 1.0f / static_cast<float>(radius * 2 + 1);
    // Across.
    for (int y = y0; y < y1; ++y) {
        for (int c = 0; c < 3; ++c) {
            float sum = 0.0f;
            for (int k = -radius; k <= radius; ++k) {
                const int x = k < 0 ? 0 : (k >= w ? w - 1 : k);
                sum += rgb[(static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                            static_cast<std::size_t>(x)) *
                               3u +
                           static_cast<std::size_t>(c)];
            }
            for (int x = 0; x < w; ++x) {
                tmp[(static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                     static_cast<std::size_t>(x)) *
                        3u +
                    static_cast<std::size_t>(c)] = sum * inv;
                const int drop = x - radius < 0 ? 0 : x - radius;
                const int gain = x + radius + 1 >= w ? w - 1 : x + radius + 1;
                sum += rgb[(static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                            static_cast<std::size_t>(gain)) *
                               3u +
                           static_cast<std::size_t>(c)] -
                       rgb[(static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                            static_cast<std::size_t>(drop)) *
                               3u +
                           static_cast<std::size_t>(c)];
            }
        }
    }
    // And down — and this pass is written the awkward way round on purpose. The obvious loop is
    // column by column, and it walks memory in strides of a whole row: five kilobytes between one
    // read and the next, which is a cache miss at every single pixel. Sweeping row by row instead,
    // carrying one running total per column, touches memory in order and is several times faster for
    // the same arithmetic.
    {
        std::vector<float> running(static_cast<std::size_t>(w) * 3u, 0.0f);
        auto row = [&](int y) {
            const int clamped = y < y0 ? y0 : (y >= y1 ? y1 - 1 : y);
            return static_cast<std::size_t>(clamped) * static_cast<std::size_t>(w) * 3u;
        };
        for (int k = -radius; k <= radius; ++k) {
            const std::size_t at = row(y0 + k);
            for (std::size_t i2 = 0; i2 < running.size(); ++i2) {
                running[i2] += tmp[at + i2];
            }
        }
        for (int y = y0; y < y1; ++y) {
            const std::size_t out = static_cast<std::size_t>(y) * static_cast<std::size_t>(w) * 3u;
            for (std::size_t i2 = 0; i2 < running.size(); ++i2) {
                rgb[out + i2] = running[i2] * inv;
            }
            const std::size_t gain = row(y + radius + 1);
            const std::size_t drop = row(y - radius);
            for (std::size_t i2 = 0; i2 < running.size(); ++i2) {
                running[i2] += tmp[gain + i2] - tmp[drop + i2];
            }
        }
    }
}

} // namespace dofdetail

// Put the picture through a lens.
//
//   img            the frame, in linear light — BEFORE the print curve, because a blur is something
//                  that happens to light and not to a photograph of it
//   y0, y1         the rows the picture occupies (the rest is letterbox)
//   depth          one depth-buffer reading per pixel of that band, row-major, width `img.width()`
//   nearPlane/far  the projection's own, so the depths can be turned back into metres
//   focus          how far away the thing in focus is, in metres
//   aperture       how wide open: 0 is a pinhole and everything is sharp, 1 is a portrait lens
//   levels         1 draws sharp and one soft copy, 2 adds a softer one. Two is a better lens and
//                   costs a second pair of blur passes; one is what a film plays at, where the frame
//                   budget is real and the difference is a long way down the list of what anybody
//                   notices.
inline void depthOfField(Image& img, int y0, int y1, const std::vector<float>& depth, float nearPlane,
                         float farPlane, float focus, float aperture, int levels = 2) {
    const int w = img.width();
    const int rows = y1 - y0;
    if (aperture <= 0.001f || rows < 3 || w < 3) {
        return;
    }
    if (depth.size() < static_cast<std::size_t>(w) * static_cast<std::size_t>(rows)) {
        return;
    }

    std::vector<float> sharp(static_cast<std::size_t>(w) * static_cast<std::size_t>(rows) * 3u, 0.0f);
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < w; ++x) {
            const Color c = img.getPixel(x, y0 + y);
            const std::size_t i =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                 static_cast<std::size_t>(x)) *
                3u;
            sharp[i + 0] = c.r;
            sharp[i + 1] = c.g;
            sharp[i + 2] = c.b;
        }
    }

    // Two copies, at two radii, scaled to the size of the picture so a film rendered at 1280 is as
    // soft as the same film rendered at 480 rather than four times sharper.
    const int unit = w / 240 < 1 ? 1 : w / 240;
    // One scratch buffer for all four passes rather than a fresh one inside each: at 480 across that
    // is a third of a megabyte allocated and thrown away four times a frame, twelve times a second.
    std::vector<float> scratch;
    std::vector<float> soft = sharp;
    dofdetail::boxBlur(soft, scratch, w, 0, rows, unit * (levels > 1 ? 2 : 3));
    // The second level is the first one blurred AGAIN rather than the sharp one blurred harder: a box
    // blur applied twice is a wider, smoother blur than one pass at twice the radius, and it costs
    // less than starting over.
    std::vector<float> softer;
    if (levels > 1) {
        softer = soft;
        dofdetail::boxBlur(softer, scratch, w, 0, rows, unit * 3);
    }

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t p =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x);
            const float away = viewDistance(depth[p], nearPlane, farPlane);
            const float blur = blurAmount(away, focus, aperture);
            const std::size_t i = p * 3u;
            float out[3];
            if (levels < 2) {
                for (int c = 0; c < 3; ++c) {
                    const std::size_t k = i + static_cast<std::size_t>(c);
                    out[c] = sharp[k] + (soft[k] - sharp[k]) * blur;
                }
                img.setPixel(x, y0 + y, Color{out[0], out[1], out[2], 1.0f});
                continue;
            }
            for (int c = 0; c < 3; ++c) {
                // Sharp to soft over the first half, soft to softer over the second: two mixes rather
                // than one, so the middle of the range is a real second radius and not a half-strength
                // version of the big one.
                out[c] = blur < 0.5f ? sharp[i + static_cast<std::size_t>(c)] +
                                           (soft[i + static_cast<std::size_t>(c)] -
                                            sharp[i + static_cast<std::size_t>(c)]) *
                                               (blur * 2.0f)
                                     : soft[i + static_cast<std::size_t>(c)] +
                                           (softer[i + static_cast<std::size_t>(c)] -
                                            soft[i + static_cast<std::size_t>(c)]) *
                                               ((blur - 0.5f) * 2.0f);
            }
            img.setPixel(x, y0 + y, Color{out[0], out[1], out[2], 1.0f});
        }
    }
}

} // namespace maz::render
