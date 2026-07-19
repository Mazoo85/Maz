#pragma once

#include "maz/math/Math.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math marching squares — extract the iso-contour (a set of line segments) of a 2D scalar field
// at a given threshold. This is the 2D sibling of marching cubes and the standard tool behind
// metaball outlines, fluid/lava surfaces, terrain contour lines, and fog-of-war edges: sample any
// scalar function on a grid, ask "where does it cross value T?", and get back the polyline pieces of
// that boundary. Each grid cell is classified by which of its four corners are >= the threshold
// (16 cases), edge crossings are placed by linear interpolation for a smooth contour, and the two
// ambiguous saddle cases are resolved with the cell-centre average. Pure CPU math, deterministic,
// header-only. Godot has no direct equivalent (its CSG uses marching cubes internally), so this is a
// genuinely-useful beyond-Godot utility with an exact, testable output.
namespace maz::math {

// One line segment of the extracted contour, in the same coordinate space as `origin`/`cellSize`.
struct ContourSegment {
    vec2 a{0.0f, 0.0f};
    vec2 b{0.0f, 0.0f};
};

// Extract the iso-contour at `threshold` from a `width` x `height` scalar field (row-major:
// field[y*width + x]). Corners with value >= threshold are "inside". Contour points are emitted in
// world space as origin + (gridX*cellSize.x, gridY*cellSize.y). Returns an empty list when the field
// is smaller than 2x2 or size mismatches. Segment endpoints are placed by linear interpolation of the
// crossing along each cell edge; the two saddle cases (opposite corners inside) are resolved by the
// cell-centre average so the contour is topologically consistent.
inline std::vector<ContourSegment> marchingSquares(const std::vector<float>& field, int width,
                                                   int height, float threshold,
                                                   const vec2& origin = vec2(0.0f, 0.0f),
                                                   const vec2& cellSize = vec2(1.0f, 1.0f)) {
    std::vector<ContourSegment> out;
    if (width < 2 || height < 2 ||
        field.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
        return out;
    }
    auto at = [&](int x, int y) -> float {
        return field[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                     static_cast<std::size_t>(x)];
    };
    auto world = [&](float gx, float gy) -> vec2 {
        return vec2(origin.x + gx * cellSize.x, origin.y + gy * cellSize.y);
    };
    // Linear crossing between grid points p0(value v0) and p1(value v1).
    auto lerpCross = [&](vec2 p0, float v0, vec2 p1, float v1) -> vec2 {
        const float denom = v1 - v0;
        const float t = (std::fabs(denom) < 1e-12f) ? 0.5f : (threshold - v0) / denom;
        return vec2(p0.x + t * (p1.x - p0.x), p0.y + t * (p1.y - p0.y));
    };

    for (int y = 0; y < height - 1; ++y) {
        for (int x = 0; x < width - 1; ++x) {
            const float va = at(x, y);         // a: (x,   y)
            const float vb = at(x + 1, y);     // b: (x+1, y)
            const float vc = at(x + 1, y + 1); // c: (x+1, y+1)
            const float vd = at(x, y + 1);     // d: (x,   y+1)

            int caseIndex = 0;
            if (va >= threshold) caseIndex |= 1;
            if (vb >= threshold) caseIndex |= 2;
            if (vc >= threshold) caseIndex |= 4;
            if (vd >= threshold) caseIndex |= 8;
            if (caseIndex == 0 || caseIndex == 15) {
                continue; // wholly outside or wholly inside -> no contour here
            }

            const vec2 pa = world(static_cast<float>(x), static_cast<float>(y));
            const vec2 pb = world(static_cast<float>(x + 1), static_cast<float>(y));
            const vec2 pc = world(static_cast<float>(x + 1), static_cast<float>(y + 1));
            const vec2 pd = world(static_cast<float>(x), static_cast<float>(y + 1));

            // Edge crossings (only the ones each case needs are computed lazily below).
            auto e0 = [&]() { return lerpCross(pa, va, pb, vb); }; // top    a-b
            auto e1 = [&]() { return lerpCross(pb, vb, pc, vc); }; // right  b-c
            auto e2 = [&]() { return lerpCross(pc, vc, pd, vd); }; // bottom c-d
            auto e3 = [&]() { return lerpCross(pd, vd, pa, va); }; // left   d-a

            switch (caseIndex) {
            case 1: out.push_back({e3(), e0()}); break;
            case 2: out.push_back({e0(), e1()}); break;
            case 3: out.push_back({e3(), e1()}); break;
            case 4: out.push_back({e1(), e2()}); break;
            case 6: out.push_back({e0(), e2()}); break;
            case 7: out.push_back({e3(), e2()}); break;
            case 8: out.push_back({e2(), e3()}); break;
            case 9: out.push_back({e2(), e0()}); break;
            case 11: out.push_back({e2(), e1()}); break;
            case 12: out.push_back({e1(), e3()}); break;
            case 13: out.push_back({e1(), e0()}); break;
            case 14: out.push_back({e0(), e3()}); break;
            case 5: { // a,c inside (b,d outside) — saddle, resolve by centre
                const float centre = (va + vb + vc + vd) * 0.25f;
                if (centre >= threshold) { // inside corners connected
                    out.push_back({e0(), e1()});
                    out.push_back({e2(), e3()});
                } else {
                    out.push_back({e3(), e0()});
                    out.push_back({e1(), e2()});
                }
                break;
            }
            case 10: { // b,d inside (a,c outside) — saddle, resolve by centre
                const float centre = (va + vb + vc + vd) * 0.25f;
                if (centre >= threshold) {
                    out.push_back({e3(), e0()});
                    out.push_back({e1(), e2()});
                } else {
                    out.push_back({e0(), e1()});
                    out.push_back({e2(), e3()});
                }
                break;
            }
            default: break;
            }
        }
    }
    return out;
}

} // namespace maz::math
