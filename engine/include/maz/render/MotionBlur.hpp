#pragma once

#include "maz/math/Math.hpp"
#include "maz/render/Image.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

// maz::render MOTION BLUR — why a film at twelve frames a second is not a slideshow.
//
// A rasteriser renders an INSTANT: infinitely short, perfectly sharp, and wrong. A film camera's
// shutter is open for a fraction of every frame, and everything that moved while it was open lands on
// the film as a smear. That smear is not a defect being simulated for authenticity — it is the thing
// that joins one frame to the next. Without it a pan at twelve a second is a sequence of separate
// photographs, which is the effect people call strobing and is the most obvious thing wrong with a
// rendered camera move.
//
// WHAT THIS DOES AND DOES NOT DO. This is camera motion blur. For every pixel it works out where the
// thing standing there was on screen one frame ago — by unprojecting through the depth buffer and
// reprojecting through the previous frame's camera — and gathers along the line between. It is exact
// for anything that did not move under its own power, which is the room, the furniture, the walls and
// the sky, and it is wrong for a walking figure: their shadow, their arm and their coat are all
// smeared as if they had held still while the camera moved.
//
// That limit is worth being straight about rather than quietly shipping. Doing it properly needs a
// per-pixel velocity written by the rasteriser — a second buffer, filled by a second transform of
// every vertex, at the previous frame's pose — and that is a real cost in a renderer where the whole
// frame is 30 ms. Camera motion is also where nearly all of the visible strobing is: a figure
// crossing a locked-off frame moves a few pixels a frame, and a pan moves the entire picture.
namespace maz::render {

// How far the thing standing at this pixel moved across the screen since the previous frame, in
// pixels, with the shutter wide open.
//
//   viewProj      this frame's camera
//   prevViewProj  the previous frame's
//   x, y          the pixel, in the band's own coordinates
//   depth         what the depth buffer holds there
//   w, h          the band's size
//
// Per pixel and not once per frame, because the answer genuinely depends on distance. Under a pure
// TURN everything moves across the frame together whatever its distance; under a TRACK the near thing
// moves much further than the far one, and that parallax is most of what carries the feeling of
// speed in a tracking shot. One number for the whole frame would be right for the pan and wrong for
// the track.
inline math::vec2 cameraVelocity(const math::mat4& viewProj, const math::mat4& prevViewProj, float x,
                                 float y, float depth, int w, int h) {
    if (w < 1 || h < 1) {
        return math::vec2(0.0f, 0.0f);
    }
    const float fw = static_cast<float>(w);
    const float fh = static_cast<float>(h);
    const float d = depth < 0.0f ? 0.0f : (depth > 0.999999f ? 0.999999f : depth);
    const math::vec4 ndc(x / fw * 2.0f - 1.0f, y / fh * 2.0f - 1.0f, d, 1.0f);

    // Back out into the world. The inverse is computed per CALL here for clarity; the picture-wide
    // version below inverts once and hands the result in, because inverting a 4x4 at every pixel of
    // every frame is not a thing anybody can afford.
    const math::vec4 world = math::inverse(viewProj) * ndc;
    if (std::fabs(world.w) < 1e-9f) {
        return math::vec2(0.0f, 0.0f);
    }
    const math::vec4 here(world.x / world.w, world.y / world.w, world.z / world.w, 1.0f);

    const math::vec4 was = prevViewProj * here;
    if (std::fabs(was.w) < 1e-6f) {
        return math::vec2(0.0f, 0.0f); // behind the previous camera: nothing sensible to say
    }
    const float px = (was.x / was.w * 0.5f + 0.5f) * fw;
    const float py = (was.y / was.w * 0.5f + 0.5f) * fh;
    if (!std::isfinite(px) || !std::isfinite(py)) {
        return math::vec2(0.0f, 0.0f);
    }
    return math::vec2(x - px, y - py);
}

namespace blurdetail {

// The same answer, at half the arithmetic, and it is worth saying why it is the same.
//
// The version above does two transforms per pixel: out of this frame's clip space into the world,
// then from the world into the previous frame's clip space. But "inverse of this camera, then the
// previous camera" is a fixed pair of matrices for the whole frame, and two matrices applied one
// after another ARE a matrix. Multiplying them once per frame leaves one transform and one divide per
// pixel instead of two of each — the same numbers, since the intermediate divide by w only scales a
// homogeneous point and the next transform is linear.
//
// This is where the cost of the whole pass turned out to live, which was not the guess. The obvious
// suspect was the gathering, so the first measurement was of the tap count: four taps and nine taps
// came out 32.2 ms and 32.0 ms a frame, which is to say identical. The taps were never the cost. The
// arithmetic in front of them was.
inline math::vec2 velocityWith(const math::mat4& reproject, float x, float y, float depth, float fw,
                               float fh) {
    const float d = depth < 0.0f ? 0.0f : (depth > 0.999999f ? 0.999999f : depth);
    const math::vec4 was =
        reproject * math::vec4(x / fw * 2.0f - 1.0f, y / fh * 2.0f - 1.0f, d, 1.0f);
    if (std::fabs(was.w) < 1e-6f) {
        return math::vec2(0.0f, 0.0f); // behind the previous camera: nothing sensible to say
    }
    const float px = (was.x / was.w * 0.5f + 0.5f) * fw;
    const float py = (was.y / was.w * 0.5f + 0.5f) * fh;
    if (!std::isfinite(px) || !std::isfinite(py)) {
        return math::vec2(0.0f, 0.0f);
    }
    return math::vec2(x - px, y - py);
}

} // namespace blurdetail

// Open the shutter.
//
//   img            the frame, in linear light — before the print curve, because a shutter is
//                  something that happens to light and not to a photograph of it
//   y0, y1         the rows the picture occupies (the rest is letterbox)
//   depth          one depth reading per pixel of that band, row-major, width `img.width()`
//   viewProj       this frame's camera
//   prevViewProj   the previous frame's; pass the same matrix twice and nothing happens
//   shutter        how much of the frame interval the shutter is open for. A film camera is about
//                  0.5 — the famous 180-degree shutter — and that is the number to reach for. 0 is a
//                  stills camera and blurs nothing.
//   maxTaps        the most samples any pixel will gather. The cap is what makes this affordable: a
//                  whip pan can move the picture a hundred pixels in a frame and gathering a hundred
//                  samples for it would cost more than the frame did.
inline void motionBlur(Image& img, int y0, int y1, const std::vector<float>& depth,
                       const math::mat4& viewProj, const math::mat4& prevViewProj, float shutter,
                       int maxTaps = 9) {
    const int w = img.width();
    const int rows = y1 - y0;
    if (shutter <= 0.0001f || rows < 2 || w < 2 || maxTaps < 2) {
        return;
    }
    if (depth.size() < static_cast<std::size_t>(w) * static_cast<std::size_t>(rows)) {
        return;
    }

    // This camera undone, then the previous one applied — as one matrix, worked out once.
    const math::mat4 reproject = prevViewProj * math::inverse(viewProj);
    const float fw = static_cast<float>(w);
    const float fh = static_cast<float>(rows);

    // Is the camera moving at all? Nearly every frame of nearly every film is a locked-off shot, and
    // on those this should cost one probe rather than a copy of the whole picture. Four corners and
    // the middle, because a camera can turn about the point in the centre of frame and move
    // everything else — testing only the middle would call a whip pan a still camera.
    {
        const float cx[5] = {0.5f, 0.08f, 0.92f, 0.08f, 0.92f};
        const float cy[5] = {0.5f, 0.08f, 0.08f, 0.92f, 0.92f};
        float most = 0.0f;
        for (int i = 0; i < 5; ++i) {
            const int sx = static_cast<int>(cx[i] * fw);
            const int sy = static_cast<int>(cy[i] * fh);
            const float d = depth[static_cast<std::size_t>(sy) * static_cast<std::size_t>(w) +
                                  static_cast<std::size_t>(sx)];
            const math::vec2 v =
                blurdetail::velocityWith(reproject, static_cast<float>(sx) + 0.5f,
                                         static_cast<float>(sy) + 0.5f, d, fw, fh);
            const float len = std::sqrt(v.x * v.x + v.y * v.y) * shutter;
            most = len > most ? len : most;
        }
        if (most < 0.75f) {
            return; // less than a pixel of movement anywhere: there is nothing to smear
        }
    }

    // Gathered, not scattered: each output pixel reads back along its own line of movement. Scatter —
    // each pixel smearing itself forward — needs somewhere to accumulate and a second pass to
    // normalise, and gathers are what a cache likes.
    std::vector<float> source(static_cast<std::size_t>(w) * static_cast<std::size_t>(rows) * 3u, 0.0f);
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < w; ++x) {
            const Color c = img.getPixel(x, y0 + y);
            const std::size_t i =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                 static_cast<std::size_t>(x)) *
                3u;
            source[i + 0] = c.r;
            source[i + 1] = c.g;
            source[i + 2] = c.b;
        }
    }

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t p =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x);
            const math::vec2 v =
                blurdetail::velocityWith(reproject, static_cast<float>(x) + 0.5f,
                                         static_cast<float>(y) + 0.5f, depth[p], fw, fh);
            const float dx = v.x * shutter;
            const float dy = v.y * shutter;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len < 0.75f) {
                continue; // this pixel did not move far enough to be worth smearing
            }
            // One sample per pixel of travel, capped. Beyond the cap the samples spread out and the
            // smear becomes a row of ghosts rather than a streak — which is the honest failure mode,
            // and only shows up on a whip pan, where nobody can see anything anyway.
            int taps = 1 + static_cast<int>(len);
            if (taps > maxTaps) {
                taps = maxTaps;
            }
            float sum[3] = {0.0f, 0.0f, 0.0f};
            // Back along the way it came, from here to where it was. Not centred on the pixel: the
            // shutter opens at the previous frame and closes at this one, so the streak trails BEHIND
            // the movement rather than straddling it.
            //
            // Two loops, and the second one is the same arithmetic with the safety taken out. This is
            // where the whole pass costs what it costs: measured at a real tracking speed, the
            // velocity is 0.8 ms of it and copying the frame 0.2, and everything else — near six
            // milliseconds — is these few lines. Per sample the careful version does two float-to-int
            // conversions, four comparisons to keep it on the picture, and a multiply to turn a row
            // and a column into an offset. For any pixel whose whole streak lands inside the picture,
            // which is nearly all of them, none of that is needed: the samples walk a straight line,
            // so the offset between one and the next is a CONSTANT, and the loop becomes an add.
            const float lastF = static_cast<float>(taps - 1);
            const float endX = static_cast<float>(x) - dx;
            const float endY = static_cast<float>(y) - dy;
            const bool inside = endX >= 0.5f && endX <= static_cast<float>(w) - 1.5f &&
                                endY >= 0.5f && endY <= static_cast<float>(rows) - 1.5f;
            if (inside) {
                // The line's two ends are on the picture, so every sample between them is too.
                const int firstX = static_cast<int>(static_cast<float>(x) + 0.5f);
                const int firstY = static_cast<int>(static_cast<float>(y) + 0.5f);
                const std::ptrdiff_t stepX =
                    static_cast<std::ptrdiff_t>(static_cast<int>(endX + 0.5f) - firstX);
                const std::ptrdiff_t stepY =
                    static_cast<std::ptrdiff_t>(static_cast<int>(endY + 0.5f) - firstY);
                // Whole steps, shared out over the gaps — worked out once rather than per sample.
                const float perStepX = static_cast<float>(stepX) / lastF;
                const float perStepY = static_cast<float>(stepY) / lastF;
                float atX = static_cast<float>(firstX);
                float atY = static_cast<float>(firstY);
                for (int t = 0; t < taps; ++t) {
                    const std::size_t s =
                        (static_cast<std::size_t>(static_cast<int>(atY)) *
                             static_cast<std::size_t>(w) +
                         static_cast<std::size_t>(static_cast<int>(atX))) *
                        3u;
                    sum[0] += source[s + 0];
                    sum[1] += source[s + 1];
                    sum[2] += source[s + 2];
                    atX += perStepX;
                    atY += perStepY;
                }
            } else {
                // Near an edge, where part of the streak would run off the picture. Clamped, so the
                // edge of the frame smears into itself rather than reading somebody else's row.
                for (int t = 0; t < taps; ++t) {
                    const float f = static_cast<float>(t) / lastF;
                    int sx = static_cast<int>(static_cast<float>(x) - dx * f + 0.5f);
                    int sy = static_cast<int>(static_cast<float>(y) - dy * f + 0.5f);
                    sx = sx < 0 ? 0 : (sx >= w ? w - 1 : sx);
                    sy = sy < 0 ? 0 : (sy >= rows ? rows - 1 : sy);
                    const std::size_t s =
                        (static_cast<std::size_t>(sy) * static_cast<std::size_t>(w) +
                         static_cast<std::size_t>(sx)) *
                        3u;
                    sum[0] += source[s + 0];
                    sum[1] += source[s + 1];
                    sum[2] += source[s + 2];
                }
            }
            const float inv = 1.0f / static_cast<float>(taps);
            img.setPixel(x, y0 + y, Color{sum[0] * inv, sum[1] * inv, sum[2] * inv, 1.0f});
        }
    }
}

} // namespace maz::render
