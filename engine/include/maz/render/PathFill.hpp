#pragma once

#include "maz/render/Image.hpp"
#include "maz/render/LineAA.hpp" // detail::blendCoverage
#include "maz/render/Path.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

// maz::render::fillPath — anti-aliased scanline fill of an arbitrary vector path, the primitive the
// engine's CPU raster was missing. ImageDraw.hpp stops at fillTriangle and the GPU renderer's polygon
// fill is convex-only, so nothing could fill a shape with a curve, a hole, or a concave corner in it.
//
// How it works: every contour becomes a list of non-horizontal EDGES. For each pixel row the path
// touches, the row is sampled at several evenly spaced sub-scanlines; each sub-scanline is intersected
// with the edges, the crossings are sorted, and the spans that the fill rule says are INSIDE are
// accumulated into a per-pixel coverage buffer. Coverage is exact horizontally (a span contributes the
// exact fraction of each pixel it covers) and sampled vertically, which is what makes near-horizontal
// edges smooth without any filtering pass. The row is then composited once through blendCoverage, so a
// pixel is written at most once per fill and self-overlapping shapes do not darken where they overlap.
//
// Fill rules: NONZERO sums the winding directions of the crossings to the left (contours wound opposite
// to each other cut holes) and EVEN-ODD just counts them (every other contour is a hole). Both are
// needed: a figure whose arm crosses its torso wants nonzero, a ring wants either.
//
// Deterministic, header-only, no GPU and no window.
namespace maz::render {

enum class FillRule {
    NonZero, // inside where the summed winding of crossings to the left is not zero
    EvenOdd, // inside where the count of crossings to the left is odd
};

namespace detail {

// One non-horizontal edge, always stored top-to-bottom with the original direction kept separately, so
// that a contour and the same contour traversed backwards produce bit-identical crossings.
struct FillEdge {
    float ytop;
    float ybot;
    float xtop;
    float xbot;
    int dir; // +1 if the contour ran down the screen here, -1 if it ran up
};

// Add the horizontal span [xa,xb) to the row accumulator, weighted, splitting the two end pixels by the
// exact fraction covered. Tracks the touched range so the row can be composited and cleared cheaply.
inline void addSpanCoverage(std::vector<float>& acc, float xa, float xb, float weight, int width,
                            int& lo, int& hi) {
    if (!(xb > xa) || width <= 0) {
        return;
    }
    if (xa < 0.0f) {
        xa = 0.0f;
    }
    if (xb > static_cast<float>(width)) {
        xb = static_cast<float>(width);
    }
    if (!(xb > xa)) {
        return;
    }
    int ia = static_cast<int>(std::floor(xa));
    int ib = static_cast<int>(std::floor(xb));
    if (ia < 0) {
        ia = 0;
    }
    if (ib >= width) {
        ib = width - 1;
    }
    if (ib < ia) {
        return;
    }
    if (ia < lo) {
        lo = ia;
    }
    if (ib > hi) {
        hi = ib;
    }
    if (ia == ib) {
        acc[static_cast<std::size_t>(ia)] += (xb - xa) * weight;
        return;
    }
    acc[static_cast<std::size_t>(ia)] += (static_cast<float>(ia + 1) - xa) * weight;
    for (int i = ia + 1; i < ib; ++i) {
        acc[static_cast<std::size_t>(i)] += weight;
    }
    acc[static_cast<std::size_t>(ib)] += (xb - static_cast<float>(ib)) * weight;
}

} // namespace detail

// Fill `path` into `img`, taking the colour of each pixel from `shade(x, y)`.
//
// A shader rather than a colour because a set is not all flat fills: a sky is a vertical gradient, a
// lighthouse beam is a linear one, a lamp is a radial one. Generalising the rasterizer is the
// alternative to a second rasterizer that would drift out of step with this one -- and a shader that
// ignores its arguments and returns a constant compiles to the same work as a plain colour fill.
// Returning a colour with zero alpha draws nothing, which is also how a clip is done.
//
// `samples` is the number of sub-scanlines per pixel row: more is smoother on near-horizontal edges
// and costs proportionally more; 16 is visually clean.
template <class Shade>
inline void fillPathShaded(Image& img, const Path& path, Shade&& shade,
                           FillRule rule = FillRule::NonZero, int samples = 16) {
    if (img.empty() || samples < 1 || path.empty()) {
        return;
    }

    // --- edges ---
    std::vector<detail::FillEdge> edges;
    for (const auto& contour : path.contours()) {
        const std::size_t n = contour.size();
        if (n < 3) {
            continue; // fewer than three points encloses no area
        }
        for (std::size_t i = 0; i < n; ++i) {
            const math::vec2& a = contour[i];
            const math::vec2& b = contour[i + 1 == n ? 0 : i + 1];
            if (a.y == b.y) {
                continue; // horizontal edges never cross a scanline
            }
            detail::FillEdge e{};
            if (a.y < b.y) {
                e.ytop = a.y; e.xtop = a.x; e.ybot = b.y; e.xbot = b.x; e.dir = 1;
            } else {
                e.ytop = b.y; e.xtop = b.x; e.ybot = a.y; e.xbot = a.x; e.dir = -1;
            }
            edges.push_back(e);
        }
    }
    if (edges.empty()) {
        return;
    }

    // --- rows the path can touch ---
    float minY = edges[0].ytop;
    float maxY = edges[0].ybot;
    for (const auto& e : edges) {
        minY = std::fmin(minY, e.ytop);
        maxY = std::fmax(maxY, e.ybot);
    }
    int rowFirst = static_cast<int>(std::floor(minY));
    int rowLast = static_cast<int>(std::ceil(maxY));
    if (rowFirst < 0) {
        rowFirst = 0;
    }
    if (rowLast > img.height() - 1) {
        rowLast = img.height() - 1;
    }
    if (rowLast < rowFirst) {
        return;
    }

    const int width = img.width();
    std::vector<float> acc(static_cast<std::size_t>(width), 0.0f);
    std::vector<std::pair<float, int>> crossings;
    const float weight = 1.0f / static_cast<float>(samples);

    // An ACTIVE EDGE TABLE, because the obvious loop is quadratic where it hurts most. Testing every
    // edge against every sub-scanline is fine for a triangle and ruinous for a line of text, which is
    // one path of several thousand edges spread over forty rows: that is millions of tests per caption
    // for the handful that actually cross each row. Edges are visited in top-to-bottom order and kept
    // in a list only while the sweep is inside them, so each row looks at its own edges and no others.
    std::vector<std::size_t> byTop(edges.size());
    for (std::size_t i = 0; i < byTop.size(); ++i) {
        byTop[i] = i;
    }
    std::sort(byTop.begin(), byTop.end(),
              [&edges](std::size_t a, std::size_t b) { return edges[a].ytop < edges[b].ytop; });
    std::size_t pending = 0;
    std::vector<std::size_t> active;

    for (int y = rowFirst; y <= rowLast; ++y) {
        const float rowTop = static_cast<float>(y);
        const float rowBottom = rowTop + 1.0f;
        while (pending < byTop.size() && edges[byTop[pending]].ytop < rowBottom) {
            active.push_back(byTop[pending]);
            ++pending;
        }
        active.erase(std::remove_if(active.begin(), active.end(),
                                    [&edges, rowTop](std::size_t i) {
                                        return edges[i].ybot <= rowTop;
                                    }),
                     active.end());
        if (active.empty()) {
            continue;
        }

        int lo = width;
        int hi = -1;
        for (int s = 0; s < samples; ++s) {
            const float sy =
                static_cast<float>(y) + (static_cast<float>(s) + 0.5f) * weight;
            crossings.clear();
            for (const std::size_t ei : active) {
                const detail::FillEdge& e = edges[ei];
                // Half-open in y: an edge owns its top endpoint and not its bottom, so two edges
                // meeting at a vertex contribute exactly one crossing, not two or none.
                if (sy < e.ytop || sy >= e.ybot) {
                    continue;
                }
                const float t = (sy - e.ytop) / (e.ybot - e.ytop);
                crossings.emplace_back(e.xtop + t * (e.xbot - e.xtop), e.dir);
            }
            if (crossings.size() < 2) {
                continue;
            }
            std::sort(crossings.begin(), crossings.end(),
                      [](const std::pair<float, int>& a, const std::pair<float, int>& b) {
                          return a.first < b.first;
                      });
            int winding = 0;
            for (std::size_t i = 0; i + 1 < crossings.size(); ++i) {
                winding += rule == FillRule::NonZero ? crossings[i].second : 1;
                const bool inside = rule == FillRule::NonZero ? winding != 0 : (winding & 1) != 0;
                if (inside) {
                    detail::addSpanCoverage(acc, crossings[i].first, crossings[i + 1].first, weight,
                                            width, lo, hi);
                }
            }
        }
        for (int x = lo; x <= hi; ++x) {
            const std::size_t k = static_cast<std::size_t>(x);
            if (acc[k] > 0.0f) {
                detail::blendCoverage(img, x, y, shade(x, y), acc[k]);
            }
            acc[k] = 0.0f;
        }
    }
}

// Fill `path` into `img` in one flat `color`.
inline void fillPath(Image& img, const Path& path, const Color& color,
                     FillRule rule = FillRule::NonZero, int samples = 16) {
    fillPathShaded(img, path, [&color](int, int) -> const Color& { return color; }, rule, samples);
}

} // namespace maz::render
