#pragma once

#include "maz/math/Math.hpp"       // vec2
#include "maz/math/VectorOps.hpp"  // dot, length, lengthSquared

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math GJK distance — the Gilbert-Johnson-Keerthi algorithm for the MINIMUM DISTANCE between two convex
// polygons (2D), with the closest pair of witness points. This is the proximity query the engine's SAT
// collider (game::ConvexShape2D) cannot answer: SAT reports only a boolean overlap (+ penetration when
// touching), whereas GJK returns the exact gap between two shapes that are APART, and the two closest
// points, one on each. That is what "how close is the projectile to the wall?", speculative/predictive
// contacts, proximity triggers, and AI standoff distances need. GJK works on the Minkowski difference via a
// support function (the farthest vertex in a direction), evolving a 1-3 point simplex toward the origin; if
// the origin is enclosed the shapes intersect (distance 0). Pure vec2 math, deterministic, header-only.
// Godot exposes no GJK distance query. (Convex inputs assumed; any vertex order works.)
namespace maz::math {

struct GjkResult {
    bool intersecting = false; // true when the shapes overlap (distance 0)
    float distance = 0.0f;     // minimum separation (0 when intersecting)
    vec2 pointA{0.0f, 0.0f};   // closest point on shape A
    vec2 pointB{0.0f, 0.0f};   // closest point on shape B
};

namespace detail {

// Farthest vertex of a convex point set along direction d.
inline vec2 gjkSupport(const std::vector<vec2>& hull, vec2 d) {
    vec2 best = hull[0];
    float bestDot = dot(hull[0], d);
    for (std::size_t i = 1; i < hull.size(); ++i) {
        const float dd = dot(hull[i], d);
        if (dd > bestDot) {
            bestDot = dd;
            best = hull[i];
        }
    }
    return best;
}

} // namespace detail

// Minimum distance between convex polygons A and B, with witness points. Empty inputs return a zero result.
inline GjkResult gjkDistance(const std::vector<vec2>& A, const std::vector<vec2>& B) {
    GjkResult res;
    if (A.empty() || B.empty()) {
        return res;
    }

    // Minkowski-difference support in direction d, also returning the underlying support point on A.
    auto mink = [&](vec2 d, vec2& outPa) -> vec2 {
        const vec2 sa = detail::gjkSupport(A, d);
        const vec2 sb = detail::gjkSupport(B, vec2(-d.x, -d.y));
        outPa = sa;
        return sa - sb;
    };

    // Simplex storage: Minkowski points, their support-on-A partners, and barycentric weights.
    vec2 mp[3];
    vec2 pa[3];
    float w[3] = {1.0f, 0.0f, 0.0f};
    int n = 1;

    vec2 dir(1.0f, 0.0f);
    mp[0] = mink(dir, pa[0]);
    vec2 closest = mp[0];

    for (int iter = 0; iter < 40; ++iter) {
        dir = vec2(-closest.x, -closest.y);
        if (lengthSquared(dir) < 1e-14f) {
            // Origin sits on the simplex — the shapes are just touching.
            res.intersecting = true;
            res.distance = 0.0f;
            res.pointA = closest; // ~origin; witnesses coincide
            res.pointB = closest;
            return res;
        }

        vec2 npa;
        const vec2 sup = mink(dir, npa);
        // Termination: the new support cannot get meaningfully closer to the origin along dir.
        const float progress = dot(sup, dir) - dot(closest, dir);
        if (progress < 1e-8f * (std::fabs(dot(sup, dir)) + 1.0f)) {
            break;
        }

        mp[n] = sup;
        pa[n] = npa;
        ++n;

        // Reduce the simplex to the feature closest to the origin, recording weights.
        if (n == 2) {
            const vec2 a = mp[0];
            const vec2 b = mp[1];
            const vec2 ab = b - a;
            float t = dot(vec2(-a.x, -a.y), ab) / dot(ab, ab);
            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
            closest = a + ab * t;
            if (t <= 0.0f) {
                n = 1;
                w[0] = 1.0f;
            } else if (t >= 1.0f) {
                mp[0] = mp[1];
                pa[0] = pa[1];
                n = 1;
                w[0] = 1.0f;
            } else {
                w[0] = 1.0f - t;
                w[1] = t;
            }
        } else { // n == 3: closest point on triangle to the origin (Ericson), with containment test.
            const vec2 a = mp[0];
            const vec2 b = mp[1];
            const vec2 c = mp[2];
            const vec2 ab = b - a;
            const vec2 ac = c - a;
            const vec2 ap = vec2(-a.x, -a.y);
            const float d1 = dot(ab, ap);
            const float d2 = dot(ac, ap);
            const vec2 bp = vec2(-b.x, -b.y);
            const float d3 = dot(ab, bp);
            const float d4 = dot(ac, bp);
            const vec2 cp = vec2(-c.x, -c.y);
            const float d5 = dot(ab, cp);
            const float d6 = dot(ac, cp);

            int keep = 0;          // 1 = vertex, 2 = edge, 3 = interior (intersecting)
            int idx[3] = {0, 0, 0};
            float bw[3] = {0, 0, 0};

            if (d1 <= 0.0f && d2 <= 0.0f) {
                keep = 1; idx[0] = 0; bw[0] = 1.0f;
            } else if (d3 >= 0.0f && d4 <= d3) {
                keep = 1; idx[0] = 1; bw[0] = 1.0f;
            } else if (d6 >= 0.0f && d5 <= d6) {
                keep = 1; idx[0] = 2; bw[0] = 1.0f;
            } else {
                const float vc = d1 * d4 - d3 * d2;
                const float vb = d5 * d2 - d1 * d6;
                const float va = d3 * d6 - d5 * d4;
                if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
                    const float t = d1 / (d1 - d3);
                    keep = 2; idx[0] = 0; idx[1] = 1; bw[0] = 1.0f - t; bw[1] = t;
                } else if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
                    const float t = d2 / (d2 - d6);
                    keep = 2; idx[0] = 0; idx[1] = 2; bw[0] = 1.0f - t; bw[1] = t;
                } else if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
                    const float t = (d4 - d3) / ((d4 - d3) + (d5 - d6));
                    keep = 2; idx[0] = 1; idx[1] = 2; bw[0] = 1.0f - t; bw[1] = t;
                } else {
                    keep = 3; // origin inside the triangle
                }
            }

            if (keep == 3) {
                res.intersecting = true;
                res.distance = 0.0f;
                // Witnesses coincide at the contact region; report the triangle's A-support centroid.
                res.pointA = (pa[0] + pa[1] + pa[2]) * (1.0f / 3.0f);
                res.pointB = res.pointA;
                return res;
            }

            // Rebuild the simplex from the kept feature.
            vec2 nmp[3];
            vec2 npaArr[3];
            float nw[3] = {0, 0, 0};
            for (int i = 0; i < keep; ++i) {
                nmp[i] = mp[idx[i]];
                npaArr[i] = pa[idx[i]];
                nw[i] = bw[i];
            }
            vec2 cl(0.0f, 0.0f);
            for (int i = 0; i < keep; ++i) {
                cl = cl + nmp[i] * nw[i];
                mp[i] = nmp[i];
                pa[i] = npaArr[i];
                w[i] = nw[i];
            }
            n = keep;
            closest = cl;
        }
    }

    // Converged: reconstruct witness points from the reduced simplex weights.
    vec2 wa(0.0f, 0.0f);
    for (int i = 0; i < n; ++i) {
        wa = wa + pa[i] * w[i];
    }
    res.pointA = wa;
    res.pointB = wa - closest; // minkowski = A - B, closest = witnessA - witnessB
    res.distance = length(closest);
    res.intersecting = res.distance < 1e-6f;
    return res;
}

} // namespace maz::math
