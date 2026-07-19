#pragma once

// maz::math bounding sphere — the smallest sphere enclosing a cloud of 3D points, the 3D companion to
// Geometry2D's minEnclosingCircle (Welzl in 2D). A tight bounding sphere is the cheapest possible proxy
// for an object's extent: it is the standard primitive for frustum culling (one sphere-vs-plane test
// per object), broad-phase overlap and LOD selection (distance to the sphere vs a threshold), and
// trigger/aggro radii fitted to an actual mesh. Computed with Welzl's incremental minimal-enclosing-
// sphere algorithm (the exact 3D analog of the 2D minidisk), so the result is the true smallest sphere,
// not an approximation; a final growth pass guarantees every point is enclosed even under floating-point
// round-off. Godot computes AABBs but exposes no bounding-sphere fit to gameplay code, so this is a
// beyond-Godot geometry utility. Header-only, std-only, deterministic.
#include <cmath>
#include <cstddef>
#include <vector>

#include "maz/math/Math.hpp"

namespace maz::math {

// A 3D sphere (centre + radius).
struct Sphere {
    vec3 center{0.0f, 0.0f, 0.0f};
    float radius = 0.0f;
    bool contains(const vec3& p, float eps = 1e-4f) const {
        const vec3 d = p - center;
        return glm::dot(d, d) <= (radius + eps) * (radius + eps);
    }
};

namespace detail {

// Solve a 3x3 linear system m*x = b (Cramer's rule). Returns false if (numerically) singular.
inline bool solve3(const double m[3][3], const double b[3], double x[3]) {
    const double det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
                     - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
                     + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    if (det < 1e-18 && det > -1e-18) return false;
    const double inv = 1.0 / det;
    for (int col = 0; col < 3; ++col) {
        double mc[3][3];
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c) mc[r][c] = (c == col) ? b[r] : m[r][c];
        const double dc = mc[0][0] * (mc[1][1] * mc[2][2] - mc[1][2] * mc[2][1])
                        - mc[0][1] * (mc[1][0] * mc[2][2] - mc[1][2] * mc[2][0])
                        + mc[0][2] * (mc[1][0] * mc[2][1] - mc[1][1] * mc[2][0]);
        x[col] = dc * inv;
    }
    return true;
}

struct DSphere {
    double cx = 0.0, cy = 0.0, cz = 0.0, r = 0.0;
    bool contains(double x, double y, double z) const {
        const double dx = x - cx, dy = y - cy, dz = z - cz;
        const double slack = r * 1e-6 + 1e-9; // tolerate boundary points from the exact fits
        return dx * dx + dy * dy + dz * dz <= (r + slack) * (r + slack);
    }
};

inline DSphere sphere1(const vec3& a) { return {a.x, a.y, a.z, 0.0}; }

inline DSphere sphere2(const vec3& a, const vec3& b) {
    DSphere s;
    s.cx = 0.5 * (static_cast<double>(a.x) + b.x);
    s.cy = 0.5 * (static_cast<double>(a.y) + b.y);
    s.cz = 0.5 * (static_cast<double>(a.z) + b.z);
    const double dx = static_cast<double>(a.x) - b.x, dy = static_cast<double>(a.y) - b.y,
                 dz = static_cast<double>(a.z) - b.z;
    s.r = 0.5 * std::sqrt(dx * dx + dy * dy + dz * dz);
    return s;
}

// Smallest sphere through 3 points (circumscribed circle of the triangle, in its plane).
inline DSphere sphere3(const vec3& a, const vec3& b, const vec3& c) {
    const double ax = a.x, ay = a.y, az = a.z;
    const double abx = static_cast<double>(b.x) - ax, aby = static_cast<double>(b.y) - ay, abz = static_cast<double>(b.z) - az;
    const double acx = static_cast<double>(c.x) - ax, acy = static_cast<double>(c.y) - ay, acz = static_cast<double>(c.z) - az;
    const double nx = aby * acz - abz * acy;
    const double ny = abz * acx - abx * acz;
    const double nz = abx * acy - aby * acx;
    const double m[3][3] = {{2 * abx, 2 * aby, 2 * abz}, {2 * acx, 2 * acy, 2 * acz}, {nx, ny, nz}};
    const double bb = (static_cast<double>(b.x) * b.x + static_cast<double>(b.y) * b.y + static_cast<double>(b.z) * b.z)
                    - (ax * ax + ay * ay + az * az);
    const double cc = (static_cast<double>(c.x) * c.x + static_cast<double>(c.y) * c.y + static_cast<double>(c.z) * c.z)
                    - (ax * ax + ay * ay + az * az);
    const double rhs[3] = {bb, cc, nx * ax + ny * ay + nz * az};
    double x[3];
    if (!solve3(m, rhs, x)) return sphere2(a, glm::length(b - a) >= glm::length(c - a) ? b : c);
    DSphere s;
    s.cx = x[0]; s.cy = x[1]; s.cz = x[2];
    const double dx = x[0] - ax, dy = x[1] - ay, dz = x[2] - az;
    s.r = std::sqrt(dx * dx + dy * dy + dz * dz);
    return s;
}

// Sphere through 4 points (solve the linear system of equal squared distances).
inline DSphere sphere4(const vec3& a, const vec3& b, const vec3& c, const vec3& d) {
    const double ax = a.x, ay = a.y, az = a.z;
    const double a2 = ax * ax + ay * ay + az * az;
    const double m[3][3] = {
        {2 * (static_cast<double>(b.x) - ax), 2 * (static_cast<double>(b.y) - ay), 2 * (static_cast<double>(b.z) - az)},
        {2 * (static_cast<double>(c.x) - ax), 2 * (static_cast<double>(c.y) - ay), 2 * (static_cast<double>(c.z) - az)},
        {2 * (static_cast<double>(d.x) - ax), 2 * (static_cast<double>(d.y) - ay), 2 * (static_cast<double>(d.z) - az)}};
    const double rhs[3] = {
        (static_cast<double>(b.x) * b.x + static_cast<double>(b.y) * b.y + static_cast<double>(b.z) * b.z) - a2,
        (static_cast<double>(c.x) * c.x + static_cast<double>(c.y) * c.y + static_cast<double>(c.z) * c.z) - a2,
        (static_cast<double>(d.x) * d.x + static_cast<double>(d.y) * d.y + static_cast<double>(d.z) * d.z) - a2};
    double x[3];
    if (!solve3(m, rhs, x)) return sphere3(a, b, c); // coplanar -> fall back
    DSphere s;
    s.cx = x[0]; s.cy = x[1]; s.cz = x[2];
    const double dx = x[0] - ax, dy = x[1] - ay, dz = x[2] - az;
    s.r = std::sqrt(dx * dx + dy * dy + dz * dz);
    return s;
}

} // namespace detail

// Smallest enclosing sphere of `points` via Welzl's incremental algorithm. Empty input -> a zero sphere
// at the origin; a single point -> that point with radius 0. Every input point is guaranteed enclosed.
inline Sphere boundingSphere(const std::vector<vec3>& points) {
    Sphere out;
    if (points.empty()) return out;

    using detail::DSphere;
    auto inside = [](const DSphere& s, const vec3& p) { return s.contains(p.x, p.y, p.z); };

    DSphere s = detail::sphere1(points[0]);
    for (std::size_t i = 1; i < points.size(); ++i) {
        if (inside(s, points[i])) continue;
        s = detail::sphere1(points[i]);
        for (std::size_t j = 0; j < i; ++j) {
            if (inside(s, points[j])) continue;
            s = detail::sphere2(points[i], points[j]);
            for (std::size_t k = 0; k < j; ++k) {
                if (inside(s, points[k])) continue;
                s = detail::sphere3(points[i], points[j], points[k]);
                for (std::size_t l = 0; l < k; ++l) {
                    if (inside(s, points[l])) continue;
                    s = detail::sphere4(points[i], points[j], points[k], points[l]);
                }
            }
        }
    }

    out.center = vec3(static_cast<float>(s.cx), static_cast<float>(s.cy), static_cast<float>(s.cz));
    out.radius = static_cast<float>(s.r);

    // Numerical guarantee: grow minimally over any point that round-off left just outside.
    for (const vec3& p : points) {
        const vec3 d = p - out.center;
        const float dist2 = glm::dot(d, d);
        if (dist2 > out.radius * out.radius) {
            const float dist = std::sqrt(dist2);
            const float newRadius = 0.5f * (out.radius + dist);
            out.center += d * ((dist - out.radius) / (2.0f * dist));
            out.radius = newRadius;
        }
    }
    return out;
}

} // namespace maz::math
