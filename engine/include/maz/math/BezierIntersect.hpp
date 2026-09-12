#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math cubic Bézier curve–curve intersection — find the points where two cubic Bézier curves cross.
// The engine has cubic Bézier paths (Curve2D) and easing, but no way to ask "where do these two curves
// meet?" — the query you need for path/obstacle collision, self-intersection and trim/clip checks in a
// vector tool, spline-vs-spline hit testing, and gesture/stroke analysis. Godot's Curve2D exposes no such
// query. This uses robust recursive DE CASTELJAU SUBDIVISION with convex-hull (control-point bounding-box)
// culling: two curve pieces can only cross where their control hulls overlap, so the pair is recursively
// split until each piece is smaller than a tolerance, and the surviving overlaps are reported as crossing
// points (de-duplicated). It finds all TRANSVERSAL crossings; curves that overlap along a shared arc are a
// degenerate case left out of scope. Pure vec2 math, deterministic, header-only.
namespace maz::math {

// A cubic Bézier curve given by its four control points.
struct CubicBezier2 {
    vec2 p0{0.0f, 0.0f};
    vec2 p1{0.0f, 0.0f};
    vec2 p2{0.0f, 0.0f};
    vec2 p3{0.0f, 0.0f};
};

// Evaluate a cubic Bézier at parameter u in [0,1] (de Casteljau).
inline vec2 cubicBezierEval(const CubicBezier2& c, float u) {
    const float v = 1.0f - u;
    const float b0 = v * v * v;
    const float b1 = 3.0f * v * v * u;
    const float b2 = 3.0f * v * u * u;
    const float b3 = u * u * u;
    return vec2(b0 * c.p0.x + b1 * c.p1.x + b2 * c.p2.x + b3 * c.p3.x,
                b0 * c.p0.y + b1 * c.p1.y + b2 * c.p2.y + b3 * c.p3.y);
}

namespace detail {

// Axis-aligned bounding box of a cubic's four control points (contains the whole curve — convex hull).
inline void bezBounds(const CubicBezier2& c, vec2& lo, vec2& hi) {
    lo = c.p0;
    hi = c.p0;
    const vec2 pts[3] = {c.p1, c.p2, c.p3};
    for (const vec2& p : pts) {
        lo.x = p.x < lo.x ? p.x : lo.x;
        lo.y = p.y < lo.y ? p.y : lo.y;
        hi.x = p.x > hi.x ? p.x : hi.x;
        hi.y = p.y > hi.y ? p.y : hi.y;
    }
}

inline bool boxesOverlap(const vec2& lo0, const vec2& hi0, const vec2& lo1, const vec2& hi1, float eps) {
    return !(hi0.x + eps < lo1.x || hi1.x + eps < lo0.x || hi0.y + eps < lo1.y || hi1.y + eps < lo0.y);
}

// Split a cubic at u = 0.5 into its left and right halves (de Casteljau).
inline void bezSplit(const CubicBezier2& c, CubicBezier2& left, CubicBezier2& right) {
    const vec2 p01 = (c.p0 + c.p1) * 0.5f;
    const vec2 p12 = (c.p1 + c.p2) * 0.5f;
    const vec2 p23 = (c.p2 + c.p3) * 0.5f;
    const vec2 p012 = (p01 + p12) * 0.5f;
    const vec2 p123 = (p12 + p23) * 0.5f;
    const vec2 p0123 = (p012 + p123) * 0.5f;
    left = CubicBezier2{c.p0, p01, p012, p0123};
    right = CubicBezier2{p0123, p123, p23, c.p3};
}

inline float bezBoxDiag(const CubicBezier2& c) {
    vec2 lo, hi;
    bezBounds(c, lo, hi);
    const vec2 d = hi - lo;
    return std::sqrt(d.x * d.x + d.y * d.y);
}

inline void bezRecurse(const CubicBezier2& a, const CubicBezier2& b, float tol, int depth,
                       std::vector<vec2>& out) {
    vec2 la, ha, lb, hb;
    bezBounds(a, la, ha);
    bezBounds(b, lb, hb);
    if (!boxesOverlap(la, ha, lb, hb, 0.0f)) {
        return;
    }
    const bool aSmall = bezBoxDiag(a) < tol;
    const bool bSmall = bezBoxDiag(b) < tol;
    if ((aSmall && bSmall) || depth <= 0) {
        // Report the centre of the overlap region.
        const vec2 lo(std::fmax(la.x, lb.x), std::fmax(la.y, lb.y));
        const vec2 hi(std::fmin(ha.x, hb.x), std::fmin(ha.y, hb.y));
        out.push_back((lo + hi) * 0.5f);
        return;
    }
    // Subdivide the larger piece (keeps recursion balanced and shallow).
    if (bezBoxDiag(a) > bezBoxDiag(b)) {
        CubicBezier2 al, ar;
        bezSplit(a, al, ar);
        bezRecurse(al, b, tol, depth - 1, out);
        bezRecurse(ar, b, tol, depth - 1, out);
    } else {
        CubicBezier2 bl, br;
        bezSplit(b, bl, br);
        bezRecurse(a, bl, tol, depth - 1, out);
        bezRecurse(a, br, tol, depth - 1, out);
    }
}

} // namespace detail

// All transversal intersection points of two cubic Bézier curves. `tol` sets both the sub-piece size at
// which a crossing is recorded and the radius within which nearby hits are merged into one point.
inline std::vector<vec2> bezierIntersections(const CubicBezier2& a, const CubicBezier2& b,
                                             float tol = 1e-3f) {
    std::vector<vec2> raw;
    detail::bezRecurse(a, b, tol, 40, raw);

    // Merge candidate points that lie within a small radius (the recursion clusters hits near a crossing).
    const float mergeR = tol * 8.0f;
    const float mergeR2 = mergeR * mergeR;
    std::vector<vec2> merged;
    for (const vec2& p : raw) {
        bool found = false;
        for (vec2& m : merged) {
            const vec2 d = p - m;
            if (d.x * d.x + d.y * d.y < mergeR2) {
                m = (m + p) * 0.5f; // average toward the cluster centre
                found = true;
                break;
            }
        }
        if (!found) {
            merged.push_back(p);
        }
    }
    return merged;
}

} // namespace maz::math
