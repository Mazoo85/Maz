#pragma once

#include "maz/math/Math.hpp" // vec3, dot, cross, normalize

#include <cmath>
#include <vector>

// maz::math rotation-minimizing frames (RMF) along a curve — a stable "which way is up" at every point.
//
// To extrude a tube or ribbon along a path, sweep a cross-section, sway a camera down a spline, or place
// rungs on a twisting ladder, you need an orthonormal frame (tangent + two perpendicular axes) at each
// point. The textbook Frenet frame is unusable in practice: it is undefined on straight sections and
// FLIPS 180° at inflection points, so a tube built on it kinks and turns inside-out. A rotation-
// minimizing frame instead carries the previous frame forward with the LEAST possible twist about the
// tangent, giving a smooth, non-flipping sweep. This uses Wang et al.'s (2008) double-reflection method:
// two reflections transport the reference axis from one sample to the next, exactly and cheaply. Pure
// vec3 math, header-only, deterministic — unit-tested for orthonormality, no rotation on a straight line,
// a constant bi-normal on a planar curve, and stability where a Frenet frame would flip.
namespace maz::math {

struct Frame {
    vec3 tangent{0.0f, 0.0f, 1.0f};
    vec3 normal{1.0f, 0.0f, 0.0f};   // reference axis, perpendicular to tangent
    vec3 binormal{0.0f, 1.0f, 0.0f}; // tangent x normal
};

namespace detail {
inline float rmfDot(const vec3& a, const vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline vec3 rmfReflect(const vec3& x, const vec3& axis, float c) {
    const float k = 2.0f / c * rmfDot(axis, x);
    return vec3(x.x - k * axis.x, x.y - k * axis.y, x.z - k * axis.z);
}
} // namespace detail

// Advance a frame from point `x0` (frame `f`) to point `x1` with unit tangent `t1`, minimising twist.
inline Frame advanceRMF(const Frame& f, const vec3& x0, const vec3& x1, const vec3& t1) {
    const vec3 v1(x1.x - x0.x, x1.y - x0.y, x1.z - x0.z);
    const float c1 = detail::rmfDot(v1, v1);
    Frame out;
    out.tangent = t1;
    if (c1 < 1e-12f) {
        // Coincident samples: keep the previous reference, re-orthonormalise against t1.
        vec3 r = f.normal;
        const float d = detail::rmfDot(r, t1);
        r = vec3(r.x - d * t1.x, r.y - d * t1.y, r.z - d * t1.z);
        out.normal = normalize(r);
        out.binormal = normalize(cross(t1, out.normal));
        return out;
    }
    // First reflection: send the plane's normal to v1, reflecting r and t.
    const vec3 rL = detail::rmfReflect(f.normal, v1, c1);
    const vec3 tL = detail::rmfReflect(f.tangent, v1, c1);
    // Second reflection: align tL with t1.
    const vec3 v2(t1.x - tL.x, t1.y - tL.y, t1.z - tL.z);
    const float c2 = detail::rmfDot(v2, v2);
    vec3 r1 = c2 < 1e-12f ? rL : detail::rmfReflect(rL, v2, c2);
    // Re-orthonormalise defensively.
    const float d = detail::rmfDot(r1, t1);
    r1 = vec3(r1.x - d * t1.x, r1.y - d * t1.y, r1.z - d * t1.z);
    out.normal = normalize(r1);
    out.binormal = normalize(cross(t1, out.normal));
    return out;
}

// Build a rotation-minimizing frame at every sample of a poly-curve. `points` and `tangents` (unit) must
// be the same length (>= 1). `initialNormal` seeds the first frame; it is projected perpendicular to the
// first tangent. Returns one Frame per sample.
inline std::vector<Frame> rotationMinimizingFrames(const std::vector<vec3>& points,
                                                   const std::vector<vec3>& tangents,
                                                   const vec3& initialNormal) {
    std::vector<Frame> frames;
    const std::size_t n = points.size();
    if (n == 0 || tangents.size() != n) return frames;
    frames.resize(n);

    Frame f0;
    f0.tangent = tangents[0];
    // Project the requested normal perpendicular to t0; fall back to any perpendicular if degenerate.
    vec3 r = initialNormal;
    float d = detail::rmfDot(r, f0.tangent);
    r = vec3(r.x - d * f0.tangent.x, r.y - d * f0.tangent.y, r.z - d * f0.tangent.z);
    if (detail::rmfDot(r, r) < 1e-10f) {
        // initialNormal was parallel to the tangent: pick an arbitrary perpendicular.
        const vec3 a = std::fabs(f0.tangent.x) < 0.9f ? vec3(1, 0, 0) : vec3(0, 1, 0);
        d = detail::rmfDot(a, f0.tangent);
        r = vec3(a.x - d * f0.tangent.x, a.y - d * f0.tangent.y, a.z - d * f0.tangent.z);
    }
    f0.normal = normalize(r);
    f0.binormal = normalize(cross(f0.tangent, f0.normal));
    frames[0] = f0;

    for (std::size_t i = 1; i < n; ++i) {
        frames[i] = advanceRMF(frames[i - 1], points[i - 1], points[i], tangents[i]);
    }
    return frames;
}

} // namespace maz::math
