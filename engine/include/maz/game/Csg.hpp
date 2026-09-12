#pragma once

#include "maz/math/Math.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

// maz::game constructive solid geometry — Godot's CSG nodes (CSGCombiner3D union / intersection /
// subtraction of CSGBox/Sphere/etc). This implements the boolean *math* with signed distance fields:
// each solid is a function giving the signed distance from a point to its surface (negative inside,
// positive outside, zero on it), and the booleans are exact closed-form combinations of those
// distances — union = min, intersection = max, subtraction = max(a, -b) — plus rounded ("smooth")
// variants for filleted joins. From an SDF you can test containment, estimate the surface normal, and
// (via a mesher such as marching cubes) extract a triangle mesh. Pure math, header-only, no GPU —
// exactly unit-testable by sampling distances. Triangle-mesh boolean output (Godot's actual CSG mesh)
// is a separate mesher on top; this is the boolean core it would sample.
namespace maz::game {

// A signed distance field: distance from a query point to the solid's surface (<0 inside, >0 outside).
using Sdf = std::function<float(const math::vec3&)>;

// --- primitives -----------------------------------------------------------------------------------

// Sphere of radius r centred at c.
inline Sdf sdSphere(const math::vec3& c, float r) {
    return [c, r](const math::vec3& p) { return std::sqrt(glm::dot(p - c, p - c)) - r; };
}

// Axis-aligned box centred at c with half-extents he. Exact distance both inside and outside.
inline Sdf sdBox(const math::vec3& c, const math::vec3& he) {
    return [c, he](const math::vec3& p) {
        const math::vec3 q = glm::abs(p - c) - he;
        const math::vec3 qp = glm::max(q, math::vec3(0.0f));
        const float outside = std::sqrt(glm::dot(qp, qp));
        const float inside = std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f);
        return outside + inside;
    };
}

// Box with rounded corners of radius r.
inline Sdf sdRoundBox(const math::vec3& c, const math::vec3& he, float r) {
    Sdf box = sdBox(c, he - math::vec3(r));
    return [box, r](const math::vec3& p) { return box(p) - r; };
}

// Half-space solid: the region on the back side of the plane {x : dot(n,x) = d}. n need not be unit;
// it is normalized so the field is a true distance. Inside is dot(n,p) < d.
inline Sdf sdPlane(const math::vec3& n, float d) {
    const float len = std::sqrt(glm::dot(n, n));
    const math::vec3 nn = len > 1e-9f ? n / len : math::vec3(0, 1, 0);
    const float dd = len > 1e-9f ? d / len : d;
    return [nn, dd](const math::vec3& p) { return glm::dot(p, nn) - dd; };
}

// Infinite cylinder of radius r about the axis through `base` in unit direction `axis`.
inline Sdf sdCylinder(const math::vec3& base, const math::vec3& axis, float r) {
    const float len = std::sqrt(glm::dot(axis, axis));
    const math::vec3 a = len > 1e-9f ? axis / len : math::vec3(0, 1, 0);
    return [base, a, r](const math::vec3& p) {
        const math::vec3 d = p - base;
        const math::vec3 radial = d - a * glm::dot(d, a);
        return std::sqrt(glm::dot(radial, radial)) - r;
    };
}

// --- boolean operations ---------------------------------------------------------------------------

inline Sdf opUnion(Sdf a, Sdf b) {
    return [a, b](const math::vec3& p) { return std::min(a(p), b(p)); };
}
inline Sdf opIntersect(Sdf a, Sdf b) {
    return [a, b](const math::vec3& p) { return std::max(a(p), b(p)); };
}
// a with b carved out of it.
inline Sdf opSubtract(Sdf a, Sdf b) {
    return [a, b](const math::vec3& p) { return std::max(a(p), -b(p)); };
}

// Rounded ("smooth") variants — k is the blend radius; k->0 recovers the sharp op. Polynomial smooth
// min (Inigo Quilez), which rounds the seam between the two surfaces.
inline Sdf opSmoothUnion(Sdf a, Sdf b, float k) {
    return [a, b, k](const math::vec3& p) {
        const float da = a(p);
        const float db = b(p);
        if (k <= 0.0f) {
            return std::min(da, db);
        }
        const float h = std::clamp(0.5f + 0.5f * (db - da) / k, 0.0f, 1.0f);
        return db * (1.0f - h) + da * h - k * h * (1.0f - h);
    };
}
inline Sdf opSmoothIntersect(Sdf a, Sdf b, float k) {
    return [a, b, k](const math::vec3& p) {
        const float da = a(p);
        const float db = b(p);
        if (k <= 0.0f) {
            return std::max(da, db);
        }
        const float h = std::clamp(0.5f - 0.5f * (db - da) / k, 0.0f, 1.0f);
        return db * (1.0f - h) + da * h + k * h * (1.0f - h);
    };
}
inline Sdf opSmoothSubtract(Sdf a, Sdf b, float k) {
    return [a, b, k](const math::vec3& p) {
        const float da = a(p);
        const float db = -b(p);
        if (k <= 0.0f) {
            return std::max(da, db);
        }
        const float h = std::clamp(0.5f - 0.5f * (da - db) / k, 0.0f, 1.0f);
        return da * (1.0f - h) + db * h + k * h * (1.0f - h);
    };
}

// --- transforms -----------------------------------------------------------------------------------

// Translate a field by `t`.
inline Sdf opTranslate(Sdf s, const math::vec3& t) {
    return [s, t](const math::vec3& p) { return s(p - t); };
}
// Uniform scale by factor `f` (>0) about the origin. Distances scale with the shape.
inline Sdf opScale(Sdf s, float f) {
    const float g = f > 1e-6f ? f : 1e-6f;
    return [s, g](const math::vec3& p) { return s(p / g) * g; };
}

// --- queries --------------------------------------------------------------------------------------

inline bool inside(const Sdf& s, const math::vec3& p) { return s(p) < 0.0f; }

// Surface normal at p by central differences on the field gradient (points outward).
inline math::vec3 sdfNormal(const Sdf& s, const math::vec3& p, float eps = 1e-3f) {
    const math::vec3 dx(eps, 0, 0);
    const math::vec3 dy(0, eps, 0);
    const math::vec3 dz(0, 0, eps);
    math::vec3 g(s(p + dx) - s(p - dx), s(p + dy) - s(p - dy), s(p + dz) - s(p - dz));
    const float len = std::sqrt(glm::dot(g, g));
    return len > 1e-12f ? g / len : math::vec3(0, 1, 0);
}

} // namespace maz::game
