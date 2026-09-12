#pragma once

#include "maz/math/LinearSolve.hpp" // solveLinearSystem (per-cell QEF)
#include "maz/math/Math.hpp"        // vec2

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math 2D dual contouring — extract a contour line from a signed distance field (SDF) that PRESERVES
// SHARP CORNERS, unlike marching squares which bevels every corner into a chamfer. Marching squares can only
// put contour points on the midpoints of grid edges, so a square or a hard crease comes out rounded; dual
// contouring instead places ONE vertex inside each boundary cell at the point that best satisfies the
// surface NORMALS of that cell's edge crossings (a tiny per-cell least-squares "QEF" solve), so two edges
// meeting at 90 degrees produce a vertex sitting exactly on the corner. That is why it needs the field's
// gradient (Hermite data), which marching squares ignores — and why it can reproduce features MS can't. Turn
// an SDF (procedural shapes, boolean CSG, destructible terrain, brush masks) into a clean polygon outline
// with crisp features for collision, decals, or rendering. Reuses the dense linear solver for the QEF. Godot
// ships only rounded marching-squares-style meshing. Header-only, std-only, deterministic. `sdf` is any
// callable taking (float x, float y) and returning the field value (negative inside).
namespace maz::math {

struct DcSegment {
    vec2 a{0.0f, 0.0f};
    vec2 b{0.0f, 0.0f};
};

// Dual-contour the iso-line of `sdf` over the integer grid [0,nx) x [0,ny). Returns the contour as line
// segments in grid coordinates. A cell gets a vertex when the iso-line crosses any of its four edges; the
// vertex is the least-squares intersection of the crossing tangent planes (sharp features preserved).
template <class F>
inline std::vector<DcSegment> dualContour2D(F&& sdf, int nx, int ny, float iso = 0.0f) {
    std::vector<DcSegment> out;
    if (nx < 2 || ny < 2) {
        return out;
    }
    // Sample corners once for topology.
    std::vector<float> f(static_cast<std::size_t>(nx) * static_cast<std::size_t>(ny));
    for (int y = 0; y < ny; ++y) {
        for (int x = 0; x < nx; ++x) {
            f[static_cast<std::size_t>(y) * static_cast<std::size_t>(nx) + static_cast<std::size_t>(x)] =
                static_cast<float>(sdf(static_cast<float>(x), static_cast<float>(y))) - iso;
        }
    }
    auto at = [&](int x, int y) { return f[static_cast<std::size_t>(y) * static_cast<std::size_t>(nx) + static_cast<std::size_t>(x)]; };
    // Exact surface normal from the true SDF gradient (central differences).
    auto normalAt = [&](float px, float py) {
        const float e = 0.01f;
        vec2 g(static_cast<float>(sdf(px + e, py) - sdf(px - e, py)),
               static_cast<float>(sdf(px, py + e) - sdf(px, py - e)));
        const float l = std::sqrt(g.x * g.x + g.y * g.y);
        return l > 1e-9f ? vec2(g.x / l, g.y / l) : vec2(1.0f, 0.0f);
    };

    const int cnx = nx - 1, cny = ny - 1;
    std::vector<vec2> vert(static_cast<std::size_t>(cnx) * static_cast<std::size_t>(cny), vec2(0.0f, 0.0f));
    std::vector<std::uint8_t> active(static_cast<std::size_t>(cnx) * static_cast<std::size_t>(cny), 0);

    for (int cy = 0; cy < cny; ++cy) {
        for (int cx = 0; cx < cnx; ++cx) {
            const float f00 = at(cx, cy), f10 = at(cx + 1, cy), f11 = at(cx + 1, cy + 1), f01 = at(cx, cy + 1);
            std::vector<vec2> pts, nrm;
            auto edge = [&](float fa, float fb, vec2 pa, vec2 pb) {
                if ((fa < 0.0f) != (fb < 0.0f)) {
                    const float t = fa / (fa - fb);
                    const vec2 c(pa.x + t * (pb.x - pa.x), pa.y + t * (pb.y - pa.y));
                    pts.push_back(c);
                    nrm.push_back(normalAt(c.x, c.y));
                }
            };
            const vec2 c00(static_cast<float>(cx), static_cast<float>(cy));
            const vec2 c10(static_cast<float>(cx + 1), static_cast<float>(cy));
            const vec2 c11(static_cast<float>(cx + 1), static_cast<float>(cy + 1));
            const vec2 c01(static_cast<float>(cx), static_cast<float>(cy + 1));
            edge(f00, f10, c00, c10); // bottom
            edge(f10, f11, c10, c11); // right
            edge(f01, f11, c01, c11); // top
            edge(f00, f01, c00, c01); // left
            if (pts.empty()) {
                continue;
            }
            vec2 mass(0.0f, 0.0f);
            for (const vec2& p : pts) {
                mass.x += p.x;
                mass.y += p.y;
            }
            mass.x /= static_cast<float>(pts.size());
            mass.y /= static_cast<float>(pts.size());
            // QEF: minimise sum (n_i . (x - p_i))^2 with a small pull toward the mass point for stability.
            const double lambda = 0.02;
            double A[4] = {lambda, 0.0, 0.0, lambda};
            double b[2] = {lambda * mass.x, lambda * mass.y};
            for (std::size_t i = 0; i < pts.size(); ++i) {
                const double n0 = nrm[i].x, n1 = nrm[i].y;
                const double d = n0 * pts[i].x + n1 * pts[i].y;
                A[0] += n0 * n0;
                A[1] += n0 * n1;
                A[2] += n1 * n0;
                A[3] += n1 * n1;
                b[0] += n0 * d;
                b[1] += n1 * d;
            }
            std::vector<double> x;
            vec2 v = mass;
            if (solveLinearSystem({A[0], A[1], A[2], A[3]}, {b[0], b[1]}, 2, x)) {
                v = vec2(static_cast<float>(x[0]), static_cast<float>(x[1]));
            }
            v.x = v.x < c00.x ? c00.x : (v.x > c10.x ? c10.x : v.x);
            v.y = v.y < c00.y ? c00.y : (v.y > c01.y ? c01.y : v.y);
            const std::size_t ci = static_cast<std::size_t>(cy) * static_cast<std::size_t>(cnx) + static_cast<std::size_t>(cx);
            vert[ci] = v;
            active[ci] = 1;
        }
    }

    auto cellIdx = [&](int cx, int cy) { return static_cast<std::size_t>(cy) * static_cast<std::size_t>(cnx) + static_cast<std::size_t>(cx); };
    for (int y = 0; y < ny; ++y) {
        for (int x = 0; x < nx; ++x) {
            if (x + 1 < nx && y >= 1 && y < ny - 1) { // horizontal grid edge -> cells above/below
                if ((at(x, y) < 0.0f) != (at(x + 1, y) < 0.0f)) {
                    const std::size_t below = cellIdx(x, y - 1), above = cellIdx(x, y);
                    if (active[below] && active[above]) {
                        out.push_back(DcSegment{vert[below], vert[above]});
                    }
                }
            }
            if (y + 1 < ny && x >= 1 && x < nx - 1) { // vertical grid edge -> cells left/right
                if ((at(x, y) < 0.0f) != (at(x, y + 1) < 0.0f)) {
                    const std::size_t left = cellIdx(x - 1, y), right = cellIdx(x, y);
                    if (active[left] && active[right]) {
                        out.push_back(DcSegment{vert[left], vert[right]});
                    }
                }
            }
        }
    }
    return out;
}

} // namespace maz::math
