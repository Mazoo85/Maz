#pragma once

#include "maz/math/Math.hpp"      // vec3
#include "maz/math/VectorOps.hpp" // dot

#include <cmath>

// maz::math ray vs finite (capped) right circular cone — the hitscan / picking test against a cone given by
// its apex, axis direction, half-angle and height, returning the hit distance, world point AND outward
// surface normal. Cones show up as spotlight/flashlight volumes, particle-emitter cones, funnels, horns,
// wizard hats, drill tips and AI vision volumes; picking or shooting at them (or clipping against a spotlight
// gizmo in an editor) needs exactly this query, which Godot does not expose. It solves the quadratic for the
// single cone nappe (clamped to the height so the infinite mirror-cone behind the apex is excluded) and also
// tests the circular base cap, returning the nearest forward hit. Pure vec3 math, deterministic, header-only.
namespace maz::math {

struct ConeHit {
    bool hit = false;
    float t = 0.0f;    // ray parameter of the hit (from + t*dir), t >= 0
    vec3 point{0.0f};  // world hit point
    vec3 normal{0.0f}; // unit outward surface normal at the hit
};

// Intersect the ray `from + t*dir` (t >= 0) with the finite cone whose tip is `apex`, opening along the unit
// vector `axis`, with the given `halfAngle` (radians, apex half-angle) and `height` (along the axis; the base
// disk sits at apex + axis*height with radius height*tan(halfAngle)). `dir` and `axis` are treated as unit.
inline ConeHit rayIntersectsCone(const vec3& from, const vec3& dir, const vec3& apex, const vec3& axis,
                                 float halfAngle, float height) {
    ConeHit best;
    best.t = 1e30f;
    const float cosA = std::cos(halfAngle);
    const float ca = cosA * cosA;

    auto consider = [&](float t, const vec3& p, const vec3& n) {
        if (t >= 0.0f && t < best.t) {
            best.hit = true;
            best.t = t;
            best.point = p;
            best.normal = n;
        }
    };

    // --- Cone side: points where dot(P-apex, axis)^2 = cos^2(angle) * |P-apex|^2, single nappe (m in [0,h]). ---
    const vec3 co = from - apex;
    const float dA = dot(dir, axis);
    const float cA = dot(co, axis);
    const float dd = dot(dir, dir);
    const float dc = dot(dir, co);
    const float cc = dot(co, co);
    const float a = dA * dA - ca * dd;
    const float b = 2.0f * (dA * cA - ca * dc);
    const float c = cA * cA - ca * cc;

    auto sideHit = [&](float t) {
        if (t < 0.0f) {
            return;
        }
        const vec3 p = from + dir * t;
        const vec3 rel = p - apex;
        const float m = dot(rel, axis); // axial coordinate; must be on the forward nappe within height
        if (m < 0.0f || m > height) {
            return;
        }
        // Outward normal ∝ ca*(P-apex) - m*axis (points radially out and back toward the apex).
        vec3 n = rel * ca - axis * m;
        const float nl = std::sqrt(dot(n, n));
        const vec3 nn = nl > 1e-9f ? n * (1.0f / nl) : axis;
        consider(t, p, nn);
    };

    if (std::fabs(a) > 1e-12f) {
        const float disc = b * b - 4.0f * a * c;
        if (disc >= 0.0f) {
            const float sq = std::sqrt(disc);
            sideHit((-b - sq) / (2.0f * a));
            sideHit((-b + sq) / (2.0f * a));
        }
    } else if (std::fabs(b) > 1e-12f) {
        sideHit(-c / b); // ray parallel to a generator: single linear root
    }

    // --- Base cap: disk at m = height, radius R = height*tan(halfAngle), normal +axis. ---
    if (std::fabs(dA) > 1e-12f) {
        const float t = (height - cA) / dA; // plane dot(P-apex, axis) = height
        if (t >= 0.0f) {
            const vec3 p = from + dir * t;
            const vec3 rel = p - apex;
            const vec3 radial = rel - axis * height;
            const float R = height * std::tan(halfAngle);
            if (dot(radial, radial) <= R * R) {
                consider(t, p, axis);
            }
        }
    }

    if (!best.hit) {
        best.t = 0.0f;
    }
    return best;
}

} // namespace maz::math
