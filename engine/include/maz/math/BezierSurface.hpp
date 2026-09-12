#pragma once

#include "maz/math/Math.hpp" // vec3

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

// maz::math bicubic Bézier surface patch — the tensor-product cubic Bézier, the smooth free-form surface a
// 4×4 grid of control points sculpts (the primitive the Utah teapot, car bodies, and font/vector surfaces are
// built from). Unlike the Coons patch (CoonsPatch.hpp, which fills in FOUR boundary curves), a Bézier patch
// is shaped by an interior control net that pushes/pulls the surface like clay — the standard way to author
// organic hulls, terrain sculpts, cloth rest shapes and deformation targets. The 16 control points are
// row-major, index = i*4 + j, so B_i(u)·B_j(v)·P[i*4+j]; the patch interpolates its four CORNER control points
// and its four boundary curves are the cubic Béziers of the edge control points. Godot has no Bézier-surface
// primitive. Includes the analytic surface normal (from the u/v tangents). Header-only, std-only,
// deterministic.
namespace maz::math {

namespace bez {
inline std::array<float, 4> basis(float t) {
    const float mt = 1.0f - t;
    return {mt * mt * mt, 3.0f * t * mt * mt, 3.0f * t * t * mt, t * t * t};
}
inline std::array<float, 4> dbasis(float t) {
    const float mt = 1.0f - t;
    return {-3.0f * mt * mt, 3.0f * mt * (1.0f - 3.0f * t), 3.0f * t * (2.0f - 3.0f * t), 3.0f * t * t};
}
} // namespace bez

// A point on the bicubic Bézier patch at (u, v) in [0,1]^2. `cp` is 16 control points, row-major (i*4 + j).
inline vec3 bezierSurfacePoint(const std::array<vec3, 16>& cp, float u, float v) {
    const std::array<float, 4> bu = bez::basis(u);
    const std::array<float, 4> bv = bez::basis(v);
    vec3 p(0.0f);
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            p = p + cp[static_cast<std::size_t>(i * 4 + j)] * (bu[static_cast<std::size_t>(i)] *
                                                               bv[static_cast<std::size_t>(j)]);
        }
    }
    return p;
}

// The two surface tangents (∂P/∂u, ∂P/∂v) at (u, v). Handy for building a TBN frame or the normal.
inline void bezierSurfaceTangents(const std::array<vec3, 16>& cp, float u, float v, vec3& du, vec3& dv) {
    const std::array<float, 4> bu = bez::basis(u);
    const std::array<float, 4> bv = bez::basis(v);
    const std::array<float, 4> dbu = bez::dbasis(u);
    const std::array<float, 4> dbv = bez::dbasis(v);
    du = vec3(0.0f);
    dv = vec3(0.0f);
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            const vec3& P = cp[static_cast<std::size_t>(i * 4 + j)];
            du = du + P * (dbu[static_cast<std::size_t>(i)] * bv[static_cast<std::size_t>(j)]);
            dv = dv + P * (bu[static_cast<std::size_t>(i)] * dbv[static_cast<std::size_t>(j)]);
        }
    }
}

// The unit surface normal at (u, v), = normalize(∂P/∂u × ∂P/∂v). Returns +Z if the patch is degenerate there.
inline vec3 bezierSurfaceNormal(const std::array<vec3, 16>& cp, float u, float v) {
    vec3 du, dv;
    bezierSurfaceTangents(cp, u, v, du, dv);
    const vec3 n(du.y * dv.z - du.z * dv.y, du.z * dv.x - du.x * dv.z, du.x * dv.y - du.y * dv.x);
    const float l = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
    return l > 1e-9f ? n * (1.0f / l) : vec3(0.0f, 0.0f, 1.0f);
}

// Sample the patch on an (nu+1) x (nv+1) grid of points, row-major with u varying fastest within a row
// (index = row_v * (nu+1) + col_u). `nu`,`nv` are clamped to at least 1.
inline std::vector<vec3> bezierSurfaceGrid(const std::array<vec3, 16>& cp, int nu, int nv) {
    if (nu < 1) {
        nu = 1;
    }
    if (nv < 1) {
        nv = 1;
    }
    std::vector<vec3> out;
    out.reserve(static_cast<std::size_t>((nu + 1) * (nv + 1)));
    for (int b = 0; b <= nv; ++b) {
        const float v = static_cast<float>(b) / static_cast<float>(nv);
        for (int a = 0; a <= nu; ++a) {
            const float u = static_cast<float>(a) / static_cast<float>(nu);
            out.push_back(bezierSurfacePoint(cp, u, v));
        }
    }
    return out;
}

} // namespace maz::math
