#pragma once

#include "maz/math/GridSample.hpp" // GridEdge, gridWrapIndex, detail::lerpT

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math 3D grid sampling — read a value out of a VOLUME grid at fractional coordinates, the 3D companion
// to GridSample.hpp (which handles 2D). This is the primitive under sampling a density / fog / SDF / 3D-noise
// volume, a baked GI light-probe grid, or a 3D LUT at a continuous world position: integer coordinates land
// on cell centres and in between you pick nearest (blocky) or trilinear (smooth — the workhorse). Trilinear
// blends the 8 surrounding cells and reproduces any affine or multilinear field EXACTLY. Out-of-bounds reads
// use the edge mode (Clamp repeats the border, Wrap tiles). Templated on the cell type, so it samples a float
// density grid, a vec3 vector field, or an RGB volume alike — any type with `T + T` and `T * float`. The
// engine had trilinear baked into MeshSdf / noise individually; this is the one reusable primitive.
// Header-only, pure, deterministic.
namespace maz::math {

namespace detail {
template <typename T>
inline T gridAt3D(const std::vector<T>& data, int w, int h, int d, int x, int y, int z, GridEdge edge) {
    const int cx = gridWrapIndex(x, w, edge);
    const int cy = gridWrapIndex(y, h, edge);
    const int cz = gridWrapIndex(z, d, edge);
    const std::size_t idx = (static_cast<std::size_t>(cz) * static_cast<std::size_t>(h) +
                             static_cast<std::size_t>(cy)) * static_cast<std::size_t>(w) +
                            static_cast<std::size_t>(cx);
    return data[idx];
}
} // namespace detail

// Nearest-neighbour: round to the closest cell centre. Grid is indexed (z*h + y)*w + x.
template <typename T>
inline T grid3DNearest(const std::vector<T>& data, int w, int h, int d, float x, float y, float z,
                       GridEdge edge = GridEdge::Clamp) {
    const int ix = static_cast<int>(std::floor(x + 0.5f));
    const int iy = static_cast<int>(std::floor(y + 0.5f));
    const int iz = static_cast<int>(std::floor(z + 0.5f));
    return detail::gridAt3D(data, w, h, d, ix, iy, iz, edge);
}

// Trilinear: blend the eight surrounding cells. Integer coords return the exact grid value; affine and
// multilinear fields are reproduced exactly.
template <typename T>
inline T grid3DTrilinear(const std::vector<T>& data, int w, int h, int d, float x, float y, float z,
                         GridEdge edge = GridEdge::Clamp) {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int z0 = static_cast<int>(std::floor(z));
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);
    const float fz = z - static_cast<float>(z0);
    const T c000 = detail::gridAt3D(data, w, h, d, x0, y0, z0, edge);
    const T c100 = detail::gridAt3D(data, w, h, d, x0 + 1, y0, z0, edge);
    const T c010 = detail::gridAt3D(data, w, h, d, x0, y0 + 1, z0, edge);
    const T c110 = detail::gridAt3D(data, w, h, d, x0 + 1, y0 + 1, z0, edge);
    const T c001 = detail::gridAt3D(data, w, h, d, x0, y0, z0 + 1, edge);
    const T c101 = detail::gridAt3D(data, w, h, d, x0 + 1, y0, z0 + 1, edge);
    const T c011 = detail::gridAt3D(data, w, h, d, x0, y0 + 1, z0 + 1, edge);
    const T c111 = detail::gridAt3D(data, w, h, d, x0 + 1, y0 + 1, z0 + 1, edge);
    // Interpolate along x, then y, then z.
    const T c00 = detail::lerpT(c000, c100, fx);
    const T c10 = detail::lerpT(c010, c110, fx);
    const T c01 = detail::lerpT(c001, c101, fx);
    const T c11 = detail::lerpT(c011, c111, fx);
    const T c0 = detail::lerpT(c00, c10, fy);
    const T c1 = detail::lerpT(c01, c11, fy);
    return detail::lerpT(c0, c1, fz);
}

} // namespace maz::math
