#pragma once

#include "maz/math/Math.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

// maz::math::Geometry3D — the first-class 3D primitive types every engine leans on for spatial
// math: a Plane, a Ray3, an axis-aligned box (Aabb3), and an oriented box (Obb), with the exact,
// closed-form intersection tests that culling, picking, physics broadphase, and level queries are
// built from. These mirror Godot's `Plane` / `AABB` / geometry helpers (and add an OBB with the
// separating-axis test Godot only has internally), so gameplay code has a portable,
// dependency-light vocabulary for "where is this, and does it touch that?".
//
// Conventions match the rest of Maz math: right-handed, GLM vectors, world units. Everything is a
// pure value type — no allocation, no GPU — so it is trivially unit-testable and deterministic.
namespace maz::math {

// ---- Plane: normal·x + d = 0, with a UNIT normal (Godot Plane). "Over" the plane means the side
// the normal points to. ------------------------------------------------------------------------
struct Plane {
    vec3 normal{0.0f, 1.0f, 0.0f};
    float d = 0.0f; // signed so that dot(normal, p) - d is the signed distance from p to the plane

    Plane() = default;
    Plane(const vec3& n, float dist) : normal(n), d(dist) {}
    // Plane through `point` with the given normal.
    Plane(const vec3& n, const vec3& point) : normal(normalize(n)) { d = dot(normal, point); }

    // Plane through three points (CCW winding gives the outward normal).
    static Plane fromPoints(const vec3& a, const vec3& b, const vec3& c) {
        const vec3 n = normalize(cross(b - a, c - a));
        return Plane(n, dot(n, a));
    }

    // Signed distance: positive when `p` is on the normal's side.
    float distanceTo(const vec3& p) const { return dot(normal, p) - d; }
    bool isPointOver(const vec3& p) const { return distanceTo(p) > 0.0f; }

    // Closest point on the plane to `p`.
    vec3 project(const vec3& p) const { return p - normal * distanceTo(p); }

    // Ray/plane intersection: origin + t*dir, t >= 0. Returns the distance t if it hits the front
    // or back face within tMax (parallel rays miss).
    std::optional<float> intersectRay(const vec3& origin, const vec3& dir,
                                      float tMax = std::numeric_limits<float>::infinity()) const {
        const float denom = dot(normal, dir);
        if (std::fabs(denom) < 1e-9f) {
            return std::nullopt; // parallel
        }
        const float t = (d - dot(normal, origin)) / denom;
        if (t < 0.0f || t > tMax) {
            return std::nullopt;
        }
        return t;
    }

    // Segment intersection: returns the hit point if [a,b] crosses the plane.
    std::optional<vec3> intersectSegment(const vec3& a, const vec3& b) const {
        const vec3 dir = b - a;
        const float denom = dot(normal, dir);
        if (std::fabs(denom) < 1e-9f) {
            return std::nullopt;
        }
        const float t = (d - dot(normal, a)) / denom;
        if (t < 0.0f || t > 1.0f) {
            return std::nullopt;
        }
        return a + dir * t;
    }

    // The single point common to three planes (nullopt if any pair is parallel / they share a
    // line).
    static std::optional<vec3> intersect3(const Plane& a, const Plane& b, const Plane& c) {
        const vec3 bc = cross(b.normal, c.normal);
        const float denom = dot(a.normal, bc);
        if (std::fabs(denom) < 1e-9f) {
            return std::nullopt;
        }
        const vec3 p =
            (bc * a.d + cross(c.normal, a.normal) * b.d + cross(a.normal, b.normal) * c.d) / denom;
        return p;
    }
};

// ---- Ray3: an origin and a (not necessarily unit) direction. -----------------------------------
struct Ray3 {
    vec3 origin{0.0f};
    vec3 dir{0.0f, 0.0f, -1.0f};

    vec3 at(float t) const { return origin + dir * t; }
};

// ---- Aabb3: an axis-aligned box stored as min/max corners. -------------------------------------
struct Aabb3 {
    vec3 min{0.0f};
    vec3 max{0.0f};

    Aabb3() = default;
    Aabb3(const vec3& lo, const vec3& hi) : min(lo), max(hi) {}

    static Aabb3 fromCenterHalf(const vec3& center, const vec3& half) {
        return Aabb3(center - half, center + half);
    }
    // An "empty" box that any point grows to enclose (min = +inf, max = -inf).
    static Aabb3 empty() {
        const float inf = std::numeric_limits<float>::infinity();
        return Aabb3(vec3(inf), vec3(-inf));
    }

    vec3 center() const { return (min + max) * 0.5f; }
    vec3 size() const { return max - min; }
    vec3 half() const { return (max - min) * 0.5f; }
    float volume() const {
        const vec3 s = size();
        return s.x * s.y * s.z;
    }

    bool contains(const vec3& p) const {
        return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y && p.z >= min.z &&
               p.z <= max.z;
    }
    bool intersects(const Aabb3& o) const {
        return min.x <= o.max.x && max.x >= o.min.x && min.y <= o.max.y && max.y >= o.min.y &&
               min.z <= o.max.z && max.z >= o.min.z;
    }

    void enclosePoint(const vec3& p) {
        min = glm::min(min, p);
        max = glm::max(max, p);
    }
    Aabb3 merge(const Aabb3& o) const { return Aabb3(glm::min(min, o.min), glm::max(max, o.max)); }

    // Corner farthest along +dir (support mapping — the building block of GJK/SAT).
    vec3 support(const vec3& dir) const {
        return vec3(dir.x >= 0 ? max.x : min.x, dir.y >= 0 ? max.y : min.y,
                    dir.z >= 0 ? max.z : min.z);
    }

    // Slab ray/box test: entry distance t in [0, tMax] if the ray enters the box.
    std::optional<float> intersectRay(const vec3& origin, const vec3& dir,
                                      float tMax = std::numeric_limits<float>::infinity()) const {
        float tmin = 0.0f, tmax = tMax;
        for (int a = 0; a < 3; ++a) {
            const float o = origin[a], dd = dir[a];
            if (std::fabs(dd) < 1e-9f) {
                if (o < min[a] || o > max[a])
                    return std::nullopt;
            } else {
                const float inv = 1.0f / dd;
                float t1 = (min[a] - o) * inv, t2 = (max[a] - o) * inv;
                if (t1 > t2)
                    std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
                if (tmin > tmax)
                    return std::nullopt;
            }
        }
        return tmin;
    }
};

// ---- Obb: an oriented box — center, half-extents, and an orthonormal rotation (columns are the
// box axes). Intersection uses the 15-axis separating-axis theorem (SAT), the standard exact
// box-box test.
// ------------------------------------------------------------------------------------------
struct Obb {
    vec3 center{0.0f};
    vec3 half{0.5f};
    mat3 axes{1.0f}; // columns = local x/y/z axes in world space (orthonormal)

    vec3 axis(int i) const { return vec3(axes[i][0], axes[i][1], axes[i][2]); }

    bool contains(const vec3& p) const {
        const vec3 d = p - center;
        for (int i = 0; i < 3; ++i) {
            const float dist = dot(d, axis(i));
            if (dist < -half[i] || dist > half[i])
                return false;
        }
        return true;
    }

    Aabb3 aabb() const {
        // Extent of the box projected onto each world axis.
        const vec3 e(half.x * std::fabs(axes[0][0]) + half.y * std::fabs(axes[1][0]) +
                         half.z * std::fabs(axes[2][0]),
                     half.x * std::fabs(axes[0][1]) + half.y * std::fabs(axes[1][1]) +
                         half.z * std::fabs(axes[2][1]),
                     half.x * std::fabs(axes[0][2]) + half.y * std::fabs(axes[1][2]) +
                         half.z * std::fabs(axes[2][2]));
        return Aabb3(center - e, center + e);
    }

    // SAT box-vs-box (Gottschalk). Returns true if the two oriented boxes overlap.
    bool intersects(const Obb& o) const {
        const float EPS = 1e-6f;
        float R[3][3], AbsR[3][3];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                R[i][j] = dot(axis(i), o.axis(j));
                AbsR[i][j] = std::fabs(R[i][j]) + EPS;
            }
        const vec3 tWorld = o.center - center;
        const vec3 t(dot(tWorld, axis(0)), dot(tWorld, axis(1)), dot(tWorld, axis(2)));
        const float a[3] = {half.x, half.y, half.z};
        const float b[3] = {o.half.x, o.half.y, o.half.z};

        // Axes of A.
        for (int i = 0; i < 3; ++i) {
            const float ra = a[i];
            const float rb = b[0] * AbsR[i][0] + b[1] * AbsR[i][1] + b[2] * AbsR[i][2];
            if (std::fabs(t[i]) > ra + rb)
                return false;
        }
        // Axes of B.
        for (int j = 0; j < 3; ++j) {
            const float ra = a[0] * AbsR[0][j] + a[1] * AbsR[1][j] + a[2] * AbsR[2][j];
            const float rb = b[j];
            const float tj = t[0] * R[0][j] + t[1] * R[1][j] + t[2] * R[2][j];
            if (std::fabs(tj) > ra + rb)
                return false;
        }
        // Cross-product axes A_i × B_j.
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                const int i1 = (i + 1) % 3, i2 = (i + 2) % 3;
                const int j1 = (j + 1) % 3, j2 = (j + 2) % 3;
                const float ra = a[i1] * AbsR[i2][j] + a[i2] * AbsR[i1][j];
                const float rb = b[j1] * AbsR[i][j2] + b[j2] * AbsR[i][j1];
                const float tv = t[i2] * R[i1][j] - t[i1] * R[i2][j];
                if (std::fabs(tv) > ra + rb)
                    return false;
            }
        }
        return true; // no separating axis found
    }
};

} // namespace maz::math
