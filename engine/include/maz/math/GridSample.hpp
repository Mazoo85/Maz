#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math grid sampling — read a value out of a 2D grid at FRACTIONAL coordinates, smoothly interpolating
// between cells. The everyday need behind sampling a heightfield between vertices, a flow-field or vector map
// between cells, a downsampled lightmap / SDF / noise texture, or any coarse data grid a game wants to read
// at a continuous world position. Integer coordinates land on cell centres; in between you choose nearest
// (blocky), bilinear (smooth, the default workhorse), or bicubic Catmull-Rom (smoother, passes through the
// grid values). Out-of-bounds reads are handled by the edge mode: Clamp (repeat the border) or Wrap (tile).
// Templated on the cell type, so it samples a float grid, a vec2 flow field, or an RGB grid alike — any type
// that supports `T + T` and `T * float`. The engine had bilinear baked into HeightField / Image / noise
// individually; this is the one reusable primitive. Header-only, pure, deterministic.
namespace maz::math {

enum class GridEdge { Clamp, Wrap };

// Fold an index into [0, n) per the edge mode. Assumes n >= 1.
inline int gridWrapIndex(int i, int n, GridEdge edge) {
    if (edge == GridEdge::Wrap) {
        int m = i % n;
        if (m < 0) m += n;
        return m;
    }
    return i < 0 ? 0 : (i >= n ? n - 1 : i); // Clamp
}

namespace detail {
template <typename T>
inline T gridAt(const std::vector<T>& data, int w, int h, int x, int y, GridEdge edge) {
    const int cx = gridWrapIndex(x, w, edge);
    const int cy = gridWrapIndex(y, h, edge);
    return data[static_cast<std::size_t>(cy) * static_cast<std::size_t>(w) + static_cast<std::size_t>(cx)];
}
template <typename T>
inline T lerpT(const T& a, const T& b, float t) {
    return a * (1.0f - t) + b * t;
}
} // namespace detail

// Nearest-neighbour: round to the closest cell centre.
template <typename T>
inline T gridNearest(const std::vector<T>& data, int w, int h, float x, float y, GridEdge edge = GridEdge::Clamp) {
    const int ix = static_cast<int>(std::floor(x + 0.5f));
    const int iy = static_cast<int>(std::floor(y + 0.5f));
    return detail::gridAt(data, w, h, ix, iy, edge);
}

// Bilinear: blend the four surrounding cells. Integer coords return the exact grid value.
template <typename T>
inline T gridBilinear(const std::vector<T>& data, int w, int h, float x, float y, GridEdge edge = GridEdge::Clamp) {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);
    const T v00 = detail::gridAt(data, w, h, x0, y0, edge);
    const T v10 = detail::gridAt(data, w, h, x0 + 1, y0, edge);
    const T v01 = detail::gridAt(data, w, h, x0, y0 + 1, edge);
    const T v11 = detail::gridAt(data, w, h, x0 + 1, y0 + 1, edge);
    return detail::lerpT(detail::lerpT(v00, v10, fx), detail::lerpT(v01, v11, fx), fy);
}

namespace detail {
// Catmull-Rom cubic through p1 (t=0) and p2 (t=1).
template <typename T>
inline T catmullRom(const T& p0, const T& p1, const T& p2, const T& p3, float t) {
    const float t2 = t * t;
    const float t3 = t2 * t;
    return (p1 * 2.0f + (p2 - p0) * t + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 +
            (p1 * 3.0f - p0 - p2 * 3.0f + p3) * t3) *
           0.5f;
}
} // namespace detail

// Bicubic (Catmull-Rom) over the 4x4 neighbourhood: smoother than bilinear and still passes through the grid
// values at integer coordinates. Best for heightfields where you want C1 continuity.
template <typename T>
inline T gridBicubic(const std::vector<T>& data, int w, int h, float x, float y, GridEdge edge = GridEdge::Clamp) {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);
    T col[4];
    for (int j = 0; j < 4; ++j) {
        const T p0 = detail::gridAt(data, w, h, x0 - 1, y0 - 1 + j, edge);
        const T p1 = detail::gridAt(data, w, h, x0 + 0, y0 - 1 + j, edge);
        const T p2 = detail::gridAt(data, w, h, x0 + 1, y0 - 1 + j, edge);
        const T p3 = detail::gridAt(data, w, h, x0 + 2, y0 - 1 + j, edge);
        col[j] = detail::catmullRom(p0, p1, p2, p3, fx);
    }
    return detail::catmullRom(col[0], col[1], col[2], col[3], fy);
}

} // namespace maz::math
