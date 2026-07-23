#pragma once

#include "maz/math/Math.hpp" // vec3

#include <cstddef>
#include <vector>

// maz::math bilinearly-blended Coons patch — a smooth surface that fills in the interior given only its FOUR
// boundary curves. You describe the edges (the two u-edges P(·,0) and P(·,1), the two v-edges P(0,·) and
// P(1,·)) and the patch interpolates a natural surface between them, reproducing each boundary curve exactly.
// It is the standard way to build procedural surfaces from edge curves: lofted track/road ribbons, terrain
// patches stitched to their neighbours' edges, cloth/sail panels, tent canopies and swept shapes. Godot has
// no surface-from-curves primitive. The boundary curves are passed as callables `f(t) -> vec3` on [0,1] that
// must agree at the shared corners. Header-only, std-only, deterministic.
namespace maz::math {

// A point on the Coons patch at parameters (u, v) in [0,1]^2. `c0`,`c1` are the u-direction boundary curves
// at v=0 and v=1; `d0`,`d1` are the v-direction boundary curves at u=0 and u=1. Callables return vec3.
template <class C0, class C1, class D0, class D1>
inline vec3 coonsPatchPoint(C0&& c0, C1&& c1, D0&& d0, D1&& d1, float u, float v) {
    // Shared corners (taken from the u-edges; the v-edges must agree there).
    const vec3 p00 = c0(0.0f);
    const vec3 p10 = c0(1.0f);
    const vec3 p01 = c1(0.0f);
    const vec3 p11 = c1(1.0f);
    // Ruled surfaces in each direction, minus the bilinear correction of the corners.
    const vec3 ruledU = c0(u) * (1.0f - v) + c1(u) * v;
    const vec3 ruledV = d0(v) * (1.0f - u) + d1(v) * u;
    const vec3 bilinear = p00 * ((1.0f - u) * (1.0f - v)) + p10 * (u * (1.0f - v)) +
                          p01 * ((1.0f - u) * v) + p11 * (u * v);
    return ruledU + ruledV - bilinear;
}

// Sample the patch on an (nu+1) x (nv+1) grid of points, row-major with u varying fastest within a row
// (index = row_v * (nu+1) + col_u). `nu`,`nv` are clamped to at least 1.
template <class C0, class C1, class D0, class D1>
inline std::vector<vec3> coonsPatchGrid(C0&& c0, C1&& c1, D0&& d0, D1&& d1, int nu, int nv) {
    if (nu < 1) {
        nu = 1;
    }
    if (nv < 1) {
        nv = 1;
    }
    std::vector<vec3> out;
    out.reserve(static_cast<std::size_t>((nu + 1) * (nv + 1)));
    for (int j = 0; j <= nv; ++j) {
        const float v = static_cast<float>(j) / static_cast<float>(nv);
        for (int i = 0; i <= nu; ++i) {
            const float u = static_cast<float>(i) / static_cast<float>(nu);
            out.push_back(coonsPatchPoint(c0, c1, d0, d1, u, v));
        }
    }
    return out;
}

} // namespace maz::math
