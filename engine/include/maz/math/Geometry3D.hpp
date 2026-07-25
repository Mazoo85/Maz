#pragma once

#include "maz/math/Math.hpp"
#include "maz/math/VectorOps.hpp" // isEqualApprox(vec3), isFinite(vec3)

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

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

    // True when `p` lies on the plane within `tolerance` — Godot's Plane.has_point.
    bool hasPoint(const vec3& p, float tolerance = 1e-5f) const {
        return std::fabs(distanceTo(p)) <= tolerance;
    }

    // The point on the plane closest to the origin (normal * d) — Godot's Plane.get_center.
    vec3 center() const { return normal * d; }

    // A copy with a unit normal, rescaling `d` to keep the same plane — Godot's Plane.normalized.
    Plane normalized() const {
        const float len = length(normal);
        if (len < 1e-9f) {
            return *this;
        }
        return Plane(normal / len, d / len);
    }

    // Component-wise approximate equality of normal AND offset — Godot's Plane.is_equal_approx.
    bool isEqualApprox(const Plane& o) const {
        return maz::math::isEqualApprox(normal, o.normal) && maz::math::isEqualApproxf(d, o.d);
    }
    // True when the normal and offset are all finite — Godot's Plane.is_finite.
    bool isFinite() const { return maz::math::isFinite(normal) && maz::math::isFinitef(d); }

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

    // Component-wise approximate equality of position (min) AND size — Godot's AABB.is_equal_approx.
    bool isEqualApprox(const Aabb3& o) const {
        return maz::math::isEqualApprox(min, o.min) && maz::math::isEqualApprox(size(), o.size());
    }
    // True when every component of min and max is finite — Godot's AABB.is_finite.
    bool isFinite() const { return maz::math::isFinite(min) && maz::math::isFinite(max); }

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

    // True when the box straddles `plane` — Godot's AABB.intersects_plane. Godot classifies each
    // corner as "over" (signed distance > 0) or "under" (<= 0) and returns over && under, so a box
    // that merely TOUCHES the plane counts as intersecting only from the positive side (a corner at
    // distance 0 is "under"). Only the two support corners along ±normal bound every corner's
    // distance, so checking them reproduces the 8-corner scan exactly.
    bool intersectsPlane(const Plane& plane) const {
        const float dNear = plane.distanceTo(support(-plane.normal)); // smallest signed distance
        const float dFar = plane.distanceTo(support(plane.normal));   // largest signed distance
        const bool over = dFar > 0.0f;    // at least one corner strictly on the normal side
        const bool under = dNear <= 0.0f; // at least one corner on/behind the plane
        return over && under;
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

    // ---- Godot AABB method completeness (M272) ----
    // True when this box fully contains `o` (Godot's AABB.encloses).
    bool encloses(const Aabb3& o) const {
        return min.x <= o.min.x && min.y <= o.min.y && min.z <= o.min.z && max.x >= o.max.x &&
               max.y >= o.max.y && max.z >= o.max.z;
    }
    // Overlap box of the two AABBs, clamped to non-negative size (Godot's AABB.intersection). When the
    // boxes are disjoint the result is degenerate (zero size) at the clamped corner.
    Aabb3 intersection(const Aabb3& o) const {
        const vec3 lo = glm::max(min, o.min);
        const vec3 hi = glm::min(max, o.max);
        return Aabb3(lo, glm::max(lo, hi));
    }
    // Uniformly expand (positive) or shrink (negative) by `margin` on every side (Godot's AABB.grow).
    Aabb3 grow(float margin) const { return Aabb3(min - vec3(margin), max + vec3(margin)); }
    // Copy grown to include `p` (the const form of enclosePoint; Godot's AABB.expand).
    Aabb3 expand(const vec3& p) const { return Aabb3(glm::min(min, p), glm::max(max, p)); }
    // Normalise a box that may have min > max on some axis (Godot's AABB.abs).
    Aabb3 abs() const { return Aabb3(glm::min(min, max), glm::max(min, max)); }

    int longestAxisIndex() const {
        const vec3 s = size();
        int ax = 0;
        float m = s.x;
        if (s.y > m) { m = s.y; ax = 1; }
        if (s.z > m) { ax = 2; }
        return ax;
    }
    float longestAxisSize() const {
        const vec3 s = size();
        return std::max(s.x, std::max(s.y, s.z));
    }
    int shortestAxisIndex() const {
        const vec3 s = size();
        int ax = 0;
        float m = s.x;
        if (s.y < m) { m = s.y; ax = 1; }
        if (s.z < m) { ax = 2; }
        return ax;
    }
    float shortestAxisSize() const {
        const vec3 s = size();
        return std::min(s.x, std::min(s.y, s.z));
    }

    // Unit axis (Vector3) of the longest / shortest side — Godot's AABB.get_longest_axis /
    // get_shortest_axis. Ties resolve to the earliest axis (x, then y), matching Godot.
    vec3 longestAxis() const {
        const vec3 s = size();
        vec3 ax(1.0f, 0.0f, 0.0f);
        float m = s.x;
        if (s.y > m) { ax = vec3(0.0f, 1.0f, 0.0f); m = s.y; }
        if (s.z > m) { ax = vec3(0.0f, 0.0f, 1.0f); }
        return ax;
    }
    vec3 shortestAxis() const {
        const vec3 s = size();
        vec3 ax(1.0f, 0.0f, 0.0f);
        float m = s.x;
        if (s.y < m) { ax = vec3(0.0f, 1.0f, 0.0f); m = s.y; }
        if (s.z < m) { ax = vec3(0.0f, 0.0f, 1.0f); }
        return ax;
    }

    // The idx-th of the 8 corners (idx in [0,7]; bit 2 = +x, bit 1 = +y, bit 0 = +z) — Godot's
    // AABB.get_endpoint.
    vec3 endpoint(int idx) const {
        const vec3 s = size();
        return vec3(min.x + ((idx & 4) ? s.x : 0.0f), min.y + ((idx & 2) ? s.y : 0.0f),
                    min.z + ((idx & 1) ? s.z : 0.0f));
    }

    // Segment-vs-box overlap test (slab clip over the segment's [0,1] parameter). Godot's
    // AABB.intersects_segment (boolean form).
    bool intersectsSegment(const vec3& a, const vec3& b) const {
        float tmin = 0.0f, tmax = 1.0f;
        const vec3 d = b - a;
        for (int i = 0; i < 3; ++i) {
            if (std::fabs(d[i]) < 1e-9f) {
                if (a[i] < min[i] || a[i] > max[i]) {
                    return false;
                }
            } else {
                const float inv = 1.0f / d[i];
                float t1 = (min[i] - a[i]) * inv, t2 = (max[i] - a[i]) * inv;
                if (t1 > t2) {
                    std::swap(t1, t2);
                }
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
                if (tmin > tmax) {
                    return false;
                }
            }
        }
        return true;
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

    // Slab ray/box test in the box's own frame: transform the ray into local coordinates (where the OBB is an
    // axis-aligned box [-half, half] at the origin) and run the standard slab test. Returns the entry distance t
    // in [0, tMax] (world units, since the axes are orthonormal) if the ray enters the box. This is the exact
    // pick/raycast test for a rotated collider — the oriented counterpart of Aabb3::intersectRay.
    std::optional<float> intersectRay(const vec3& origin, const vec3& dir,
                                      float tMax = std::numeric_limits<float>::infinity()) const {
        const vec3 d = origin - center;
        // Ray in local space: component along each box axis.
        const vec3 lo(dot(d, axis(0)), dot(d, axis(1)), dot(d, axis(2)));
        const vec3 ld(dot(dir, axis(0)), dot(dir, axis(1)), dot(dir, axis(2)));
        float tmin = 0.0f, tmax = tMax;
        for (int a = 0; a < 3; ++a) {
            if (std::fabs(ld[a]) < 1e-9f) {
                if (lo[a] < -half[a] || lo[a] > half[a])
                    return std::nullopt; // parallel to slab and outside it
            } else {
                const float inv = 1.0f / ld[a];
                float t1 = (-half[a] - lo[a]) * inv, t2 = (half[a] - lo[a]) * inv;
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

// ---- Free helpers mirroring Godot's Geometry3D static methods. ---------------------------------
// These are the closed-form segment / triangle / sphere primitives level queries, picking, and
// LOS/ballistics checks lean on, kept as free functions (like Godot's Geometry3D.*).

// Closest point on segment [a,b] to point p.
inline vec3 closestPointToSegment(const vec3& p, const vec3& a, const vec3& b) {
    const vec3 ab = b - a;
    const float len2 = dot(ab, ab);
    if (len2 < 1e-12f) {
        return a;
    }
    float t = dot(p - a, ab) / len2;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return a + ab * t;
}

// Closest point on the INFINITE line through a,b to point p (projection, not clamped to the
// segment) — Godot's Geometry3D.get_closest_point_to_segment_uncapped.
inline vec3 closestPointToSegmentUncapped(const vec3& p, const vec3& a, const vec3& b) {
    const vec3 ab = b - a;
    const float len2 = dot(ab, ab);
    if (len2 < 1e-12f) {
        return a;
    }
    return a + ab * (dot(p - a, ab) / len2);
}

// The pair of closest points between segments [p1,p2] and [q1,q2] (out c1 on the first, c2 on the
// second). Follows the standard clamped-parametric solution (Ericson, Real-Time Collision Detection).
inline void closestPointsBetweenSegments(const vec3& p1, const vec3& p2, const vec3& q1,
                                         const vec3& q2, vec3& c1, vec3& c2) {
    const vec3 d1 = p2 - p1; // direction of segment 1
    const vec3 d2 = q2 - q1; // direction of segment 2
    const vec3 r = p1 - q1;
    const float a = dot(d1, d1);
    const float e = dot(d2, d2);
    const float f = dot(d2, r);
    float s = 0.0f, t = 0.0f;
    const float eps = 1e-12f;
    if (a <= eps && e <= eps) {
        c1 = p1;
        c2 = q1;
        return;
    }
    if (a <= eps) {
        s = 0.0f;
        t = std::clamp(f / e, 0.0f, 1.0f);
    } else {
        const float c = dot(d1, r);
        if (e <= eps) {
            t = 0.0f;
            s = std::clamp(-c / a, 0.0f, 1.0f);
        } else {
            const float b = dot(d1, d2);
            const float denom = a * e - b * b;
            s = denom > eps ? std::clamp((b * f - c * e) / denom, 0.0f, 1.0f) : 0.0f;
            t = (b * s + f) / e;
            if (t < 0.0f) {
                t = 0.0f;
                s = std::clamp(-c / a, 0.0f, 1.0f);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = std::clamp((b - c) / a, 0.0f, 1.0f);
            }
        }
    }
    c1 = p1 + d1 * s;
    c2 = q1 + d2 * t;
}

// Möller-Trumbore ray/triangle intersection: ray origin + t*dir (t >= 0). Returns the hit point.
inline std::optional<vec3> rayIntersectsTriangle(const vec3& from, const vec3& dir, const vec3& a,
                                                 const vec3& b, const vec3& c) {
    const vec3 e1 = b - a;
    const vec3 e2 = c - a;
    const vec3 h = cross(dir, e2);
    const float det = dot(e1, h);
    if (det > -1e-9f && det < 1e-9f) {
        return std::nullopt; // parallel
    }
    const float inv = 1.0f / det;
    const vec3 s = from - a;
    const float u = dot(s, h) * inv;
    if (u < 0.0f || u > 1.0f) {
        return std::nullopt;
    }
    const vec3 q = cross(s, e1);
    const float v = dot(dir, q) * inv;
    if (v < 0.0f || u + v > 1.0f) {
        return std::nullopt;
    }
    const float t = dot(e2, q) * inv;
    if (t < 0.0f) {
        return std::nullopt;
    }
    return from + dir * t;
}

// Segment/triangle intersection: [from,to] crossing triangle (a,b,c).
inline std::optional<vec3> segmentIntersectsTriangle(const vec3& from, const vec3& to, const vec3& a,
                                                     const vec3& b, const vec3& c) {
    const vec3 dir = to - from;
    const float len = length(dir);
    if (len < 1e-9f) {
        return std::nullopt;
    }
    const vec3 nd = dir / len;
    auto hit = rayIntersectsTriangle(from, nd, a, b, c);
    if (!hit) {
        return std::nullopt;
    }
    // Reject hits past the segment end.
    if (dot(*hit - from, nd) > len + 1e-4f) {
        return std::nullopt;
    }
    return hit;
}

// Segment/sphere intersection: returns the first entry point of [from,to] into the sphere.
inline std::optional<vec3> segmentIntersectsSphere(const vec3& from, const vec3& to,
                                                   const vec3& center, float radius) {
    const vec3 d = to - from;
    const float len2 = dot(d, d);
    if (len2 < 1e-12f) {
        return (length(from - center) <= radius) ? std::optional<vec3>(from) : std::nullopt;
    }
    const vec3 m = from - center;
    const float aa = len2;
    const float bb = 2.0f * dot(m, d);
    const float cc = dot(m, m) - radius * radius;
    const float disc = bb * bb - 4.0f * aa * cc;
    if (disc < 0.0f) {
        return std::nullopt;
    }
    const float sq = std::sqrt(disc);
    float t = (-bb - sq) / (2.0f * aa); // nearest root
    if (t < 0.0f) {
        t = (-bb + sq) / (2.0f * aa); // segment may start inside the sphere
    }
    if (t < 0.0f || t > 1.0f) {
        return std::nullopt;
    }
    return from + d * t;
}

// First surface point where segment [from,to] meets a finite cylinder — Godot's
// Geometry3D.segment_intersects_cylinder. The cylinder is centred at the origin, aligned to the Y
// axis, spans y in [-height/2, +height/2] and has the given radius. Returns the entry crossing;
// a segment that starts inside reports its forward exit crossing, mirroring segmentIntersectsSphere.
// nullopt when the segment never reaches the cylinder's surface within [from,to].
inline std::optional<vec3> segmentIntersectsCylinder(const vec3& from, const vec3& to, float height,
                                                     float radius) {
    const vec3 d = to - from;
    const float inf = std::numeric_limits<float>::infinity();
    // Radial slab: points with x^2 + z^2 <= radius^2 (infinite cylinder about Y).
    float tr0 = -inf, tr1 = inf;
    const float a = d.x * d.x + d.z * d.z;
    const float c = from.x * from.x + from.z * from.z - radius * radius;
    if (a < 1e-12f) {
        if (c > 0.0f) {
            return std::nullopt; // parallel to axis and outside the radius
        }
    } else {
        const float b = 2.0f * (from.x * d.x + from.z * d.z);
        const float disc = b * b - 4.0f * a * c;
        if (disc < 0.0f) {
            return std::nullopt; // never crosses the infinite cylinder
        }
        const float sq = std::sqrt(disc);
        tr0 = (-b - sq) / (2.0f * a);
        tr1 = (-b + sq) / (2.0f * a);
    }
    // Cap slab: points with y in [-height/2, +height/2].
    const float halfH = height * 0.5f;
    float ty0 = -inf, ty1 = inf;
    if (std::fabs(d.y) < 1e-9f) {
        if (from.y < -halfH || from.y > halfH) {
            return std::nullopt; // parallel to caps and outside the height band
        }
    } else {
        ty0 = (-halfH - from.y) / d.y;
        ty1 = (halfH - from.y) / d.y;
        if (ty0 > ty1) {
            std::swap(ty0, ty1);
        }
    }
    const float tEnter = std::max(tr0, ty0);
    const float tExit = std::min(tr1, ty1);
    if (tEnter > tExit) {
        return std::nullopt; // radial and cap intervals do not overlap
    }
    float t = tEnter;
    if (t < 0.0f) {
        t = tExit; // segment starts inside -> forward exit crossing
    }
    if (t < 0.0f || t > 1.0f) {
        return std::nullopt;
    }
    return from + d * t;
}

// Build the six outward-facing planes of an axis-aligned box with the given half-extents, centred at
// `center` — Godot's Geometry3D.build_box_planes. A point is INSIDE the box when it is on the negative
// side of every plane (normal·p - d <= 0). Order: +X, -X, +Y, -Y, +Z, -Z.
inline std::vector<Plane> buildBoxPlanes(const vec3& extents, const vec3& center = vec3(0.0f)) {
    std::vector<Plane> planes;
    planes.reserve(6);
    const vec3 axes[3] = {vec3(1, 0, 0), vec3(0, 1, 0), vec3(0, 0, 1)};
    const float ext[3] = {extents.x, extents.y, extents.z};
    for (int i = 0; i < 3; ++i) {
        // +axis: normal·p <= ext + normal·center ; -axis mirrors it.
        planes.emplace_back(axes[i], ext[i] + dot(axes[i], center));
        planes.emplace_back(-axes[i], ext[i] + dot(-axes[i], center));
    }
    return planes;
}

// Build the bounding planes of a cylinder (a `sides`-faceted prism) of the given radius and height,
// aligned to `axis` (0=X, 1=Y, 2=Z; default Z to match Godot) and centred at the origin — Godot's
// Geometry3D.build_cylinder_planes. Returns `sides` radial side planes (each at distance `radius`)
// followed by the two cap planes at +/- height/2. A point is INSIDE when it is on the negative side
// of every plane (normal·p - d <= 0), the same convention as buildBoxPlanes / segmentIntersectsConvex.
inline std::vector<Plane> buildCylinderPlanes(float radius, float height, int sides, int axis = 2) {
    std::vector<Plane> planes;
    if (axis < 0 || axis > 2) {
        axis = 2;
    }
    const int a1 = (axis + 1) % 3;
    const int a2 = (axis + 2) % 3;
    constexpr float twoPi = 6.28318530717958647692f;
    for (int i = 0; i < sides; ++i) {
        const float angle = static_cast<float>(i) * twoPi / static_cast<float>(sides);
        vec3 n(0.0f);
        n[a1] = std::cos(angle);
        n[a2] = std::sin(angle);
        planes.emplace_back(n, radius);
    }
    vec3 ax(0.0f);
    ax[axis] = 1.0f;
    planes.emplace_back(ax, height * 0.5f);
    planes.emplace_back(-ax, height * 0.5f);
    return planes;
}

// Build bounding planes of a capsule — a cylinder of `radius`/`height` capped by two hemispheres,
// aligned to `axis` (0=X/1=Y/2=Z, default Z), centred at the origin. The capsule companion to
// buildBoxPlanes / buildCylinderPlanes (toward Godot Geometry3D.build_capsule_planes): `sides`
// radial facets bound the cylindrical middle and each hemisphere is bounded by `rings` latitude
// rings of tangent planes. `height` is the distance between the two hemisphere CENTRES, so the
// capsule spans +/-(height/2 + radius) along the axis. Every plane is tangent to (or outside) the
// true capsule, so the intersection of their half-spaces CONTAINS the capsule — a conservative bound
// for culling / segmentIntersectsConvex clipping, NOT a byte-exact match of Godot's facet layout.
// Returns sides*(1 + 2*rings) planes; a point is INSIDE when on the negative side of every plane.
inline std::vector<Plane> buildCapsulePlanes(float radius, float height, int sides, int rings,
                                             int axis = 2) {
    std::vector<Plane> planes;
    if (axis < 0 || axis > 2) {
        axis = 2;
    }
    const int a1 = (axis + 1) % 3;
    const int a2 = (axis + 2) % 3;
    constexpr float twoPi = 6.28318530717958647692f;
    constexpr float halfPi = 1.57079632679489661923f;
    vec3 ax(0.0f);
    ax[axis] = 1.0f;
    const vec3 capTop = ax * (height * 0.5f);
    const vec3 capBot = ax * (-height * 0.5f);
    for (int i = 0; i < sides; ++i) {
        const float ang = static_cast<float>(i) * twoPi / static_cast<float>(sides);
        vec3 radial(0.0f);
        radial[a1] = std::cos(ang);
        radial[a2] = std::sin(ang);
        planes.emplace_back(radial, radius); // cylindrical side facet
        for (int j = 1; j <= rings; ++j) {
            const float theta = static_cast<float>(j) * halfPi / static_cast<float>(rings);
            const float ct = std::cos(theta), st = std::sin(theta);
            const vec3 nTop = radial * ct + ax * st; // tangent plane of the top cap sphere
            planes.emplace_back(nTop, dot(nTop, capTop) + radius);
            const vec3 nBot = radial * ct - ax * st; // tangent plane of the bottom cap sphere
            planes.emplace_back(nBot, dot(nBot, capBot) + radius);
        }
    }
    return planes;
}

// The point on triangle (a,b,c) closest to `p` — Ericson's Voronoi-region method (Real-Time
// Collision Detection). Handles all seven regions (three vertices, three edges, the interior face)
// in closed form, so it works for a point above/below the face or off to any side. The bedrock of
// sphere-vs-mesh collision, decal projection, and "snap to surface" queries.
inline vec3 closestPointOnTriangle(const vec3& p, const vec3& a, const vec3& b, const vec3& c) {
    const vec3 ab = b - a;
    const vec3 ac = c - a;
    const vec3 ap = p - a;
    const float d1 = dot(ab, ap);
    const float d2 = dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) {
        return a; // vertex region A
    }
    const vec3 bp = p - b;
    const float d3 = dot(ab, bp);
    const float d4 = dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) {
        return b; // vertex region B
    }
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        const float v = d1 / (d1 - d3);
        return a + v * ab; // edge AB
    }
    const vec3 cp = p - c;
    const float d5 = dot(ab, cp);
    const float d6 = dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) {
        return c; // vertex region C
    }
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        const float w = d2 / (d2 - d6);
        return a + w * ac; // edge AC
    }
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + w * (c - b); // edge BC
    }
    // Inside the face: barycentric projection.
    const float denom = 1.0f / (va + vb + vc);
    const float v = vb * denom;
    const float w = vc * denom;
    return a + ab * v + ac * w;
}

// Barycentric coordinates (u,v,w) of `p` with respect to triangle (a,b,c): p projects to u*a+v*b+w*c
// and u+v+w == 1. u is the weight of a, v of b, w of c. For a point off the triangle's plane these
// are the coordinates of its orthogonal projection (Ericson's area/Cramer method). The standard tool
// for interpolating a per-vertex attribute (colour, UV, normal) at an arbitrary point on a triangle,
// e.g. shading a ray hit. A degenerate (zero-area) triangle returns (1,0,0).
inline vec3 barycentric(const vec3& p, const vec3& a, const vec3& b, const vec3& c) {
    const vec3 v0 = b - a, v1 = c - a, v2 = p - a;
    const float d00 = dot(v0, v0);
    const float d01 = dot(v0, v1);
    const float d11 = dot(v1, v1);
    const float d20 = dot(v2, v0);
    const float d21 = dot(v2, v1);
    const float denom = d00 * d11 - d01 * d01;
    if (std::fabs(denom) < 1e-20f) {
        return vec3(1.0f, 0.0f, 0.0f); // degenerate triangle
    }
    const float v = (d11 * d20 - d01 * d21) / denom;
    const float w = (d00 * d21 - d01 * d20) / denom;
    return vec3(1.0f - v - w, v, w);
}

// Intersect a segment [from,to] with the convex volume that is the intersection of the half-spaces
// (normal·p - d <= 0) of `planes` — Godot's Geometry3D.segment_intersects_convex. Returns the point
// where the segment first ENTERS the volume through one of its faces (nullopt if it never does).
// Matching Godot, a segment that starts already inside the volume reports no hit (there is no entry
// crossing). `outNormal`, when non-null, receives the normal of the entry plane.
inline std::optional<vec3> segmentIntersectsConvex(const vec3& from, const vec3& to,
                                                   const Plane* planes, std::size_t planeCount,
                                                   vec3* outNormal = nullptr) {
    const vec3 rel = to - from;
    const float relLen = length(rel);
    if (relLen < 1e-9f || planeCount == 0) {
        return std::nullopt;
    }
    const vec3 dir = rel / relLen;
    float tMin = -std::numeric_limits<float>::infinity();
    float tMax = std::numeric_limits<float>::infinity();
    std::size_t entryPlane = planeCount; // sentinel = "none"
    for (std::size_t i = 0; i < planeCount; ++i) {
        const Plane& p = planes[i];
        const float den = dot(p.normal, dir);
        if (std::fabs(den) <= 1e-9f) {
            // Parallel to this plane: if `from` is strictly outside it, the segment can never enter.
            if (p.distanceTo(from) > 0.0f) {
                return std::nullopt;
            }
            continue;
        }
        const float dist = -p.distanceTo(from) / den;
        if (den < 0.0f) {
            // Entering half-space: raises the lower bound.
            if (dist > tMin) {
                tMin = dist;
                entryPlane = i;
            }
        } else {
            // Leaving half-space: lowers the upper bound.
            if (dist < tMax) {
                tMax = dist;
            }
        }
    }
    // Godot's guards: the entering bound must precede the leaving bound, lie within the segment, and
    // come from a real face (tMin < 0 means the segment starts inside -> no entry crossing).
    if (tMax <= tMin || tMin < 0.0f || tMin > relLen || entryPlane == planeCount) {
        return std::nullopt;
    }
    if (outNormal) {
        *outNormal = planes[entryPlane].normal;
    }
    return from + dir * tMin;
}

inline std::optional<vec3> segmentIntersectsConvex(const vec3& from, const vec3& to,
                                                   const std::vector<Plane>& planes,
                                                   vec3* outNormal = nullptr) {
    return segmentIntersectsConvex(from, to, planes.data(), planes.size(), outNormal);
}

// Given a set of planes bounding a convex volume (INSIDE = distanceTo(p) <= 0 for every plane, the
// convention buildBoxPlanes / buildCylinderPlanes / buildCapsulePlanes and segmentIntersectsConvex
// all share), return the corner vertices of that convex polytope — Godot's
// Geometry3D.compute_convex_mesh_points. This is the INVERSE of the plane-builder family: a box /
// cylinder / capsule bound (or any half-space set) becomes the drawable set of hull corners you can
// feed to a convex-hull builder, a debug renderer, or a bounding-box computation. Every corner is a
// point where three of the planes meet (via Plane::intersect3); it is a real vertex only when it also
// lies on or inside all the other planes. Corners where more than three planes coincide (a box corner
// meets 3, but a pyramid apex meets 4+) are merged within `eps`. Returns an empty list for fewer than
// four planes, or when the half-space set is empty / unbounded (no finite corner survives the test).
inline std::vector<vec3> computeConvexMeshPoints(const Plane* planes, std::size_t planeCount,
                                                 float eps = 1e-5f) {
    std::vector<vec3> points;
    if (planeCount < 4) {
        return points;
    }
    for (std::size_t i = 0; i < planeCount; ++i) {
        for (std::size_t j = i + 1; j < planeCount; ++j) {
            for (std::size_t k = j + 1; k < planeCount; ++k) {
                const std::optional<vec3> corner =
                    Plane::intersect3(planes[i], planes[j], planes[k]);
                if (!corner) {
                    continue; // two of the three planes are parallel / share a line
                }
                const vec3 p = *corner;
                bool inside = true;
                for (std::size_t m = 0; m < planeCount; ++m) {
                    if (planes[m].distanceTo(p) > eps) {
                        inside = false;
                        break; // this triple's meeting point pokes outside another face
                    }
                }
                if (!inside) {
                    continue;
                }
                bool duplicate = false;
                for (const vec3& q : points) {
                    if (length(p - q) <= eps) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate) {
                    points.push_back(p);
                }
            }
        }
    }
    return points;
}

inline std::vector<vec3> computeConvexMeshPoints(const std::vector<Plane>& planes,
                                                 float eps = 1e-5f) {
    return computeConvexMeshPoints(planes.data(), planes.size(), eps);
}

// Clip a 3D polygon (an ordered ring of points) against a plane, keeping the part on the plane's
// NEGATIVE side (distanceTo(p) < 0) — Godot's Geometry3D.clip_polygon. Single-plane Sutherland-
// Hodgman: vertices strictly inside are kept in order, and every edge that crosses the plane
// contributes the exact intersection point, so the ring comes back trimmed to the half-space. This is
// the same "inside = negative side" convention as buildBoxPlanes / segmentIntersectsConvex /
// computeConvexMeshPoints, so a face can be clipped plane-by-plane against a convex bound (the classic
// way to build the polygon faces that go with computeConvexMeshPoints' corners). Returns the polygon
// unchanged when it lies wholly inside (or on the plane), and an empty list when wholly outside.
inline std::vector<vec3> clipPolygon(const std::vector<vec3>& polygon, const Plane& plane,
                                     float eps = 1e-6f) {
    if (polygon.empty()) {
        return polygon;
    }
    enum Loc { Inside = 1, Boundary = 0, Outside = -1 };
    std::vector<int> loc(polygon.size());
    std::size_t insideCount = 0;
    std::size_t outsideCount = 0;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const float dist = plane.distanceTo(polygon[i]);
        if (dist < -eps) {
            loc[i] = Inside;
            ++insideCount;
        } else if (dist > eps) {
            loc[i] = Outside;
            ++outsideCount;
        } else {
            loc[i] = Boundary;
        }
    }
    if (outsideCount == 0) {
        return polygon; // wholly inside / on the plane -> unchanged
    }
    if (insideCount == 0) {
        return {}; // wholly outside -> nothing survives
    }
    std::vector<vec3> clipped;
    std::size_t previous = polygon.size() - 1;
    for (std::size_t index = 0; index < polygon.size(); ++index) {
        const int cur = loc[index];
        if (cur == Outside) {
            if (loc[previous] == Inside) {
                const vec3& v1 = polygon[previous];
                const vec3& v2 = polygon[index];
                const vec3 segment = v1 - v2;
                const float t = -plane.distanceTo(v1) / dot(plane.normal, segment);
                clipped.push_back(v1 + segment * t);
            }
        } else {
            const vec3& v1 = polygon[index];
            if (cur == Inside && loc[previous] == Outside) {
                const vec3& v2 = polygon[previous];
                const vec3 segment = v1 - v2;
                const float t = -plane.distanceTo(v1) / dot(plane.normal, segment);
                clipped.push_back(v1 + segment * t);
            }
            clipped.push_back(v1);
        }
        previous = index;
    }
    return clipped;
}

// Build the polygon faces of the convex volume bounded by a set of planes (INSIDE = distanceTo <= 0
// for every plane) — the planes->faces companion that closes the loop with computeConvexMeshPoints
// (planes->corner vertices) and clipPolygon (trim a face against a plane). For each plane it starts
// with a large quad lying on that plane, wound so its outward normal matches the plane normal, then
// clips that quad against every other plane; whatever survives with >= 3 vertices is that plane's
// face. The result is a watertight, correctly-wound set of convex polygon faces ready for a debug
// renderer, a collision proxy, or triangulation. This is the standard "planes to mesh" construction
// (the same idea Godot uses internally to turn its build_*_planes output into a MeshData); it is a
// composing utility over the verified M402/M403 primitives, not a distinct Godot public API. Returns
// an empty list when the half-space set is unbounded or empty (fewer than 4 finite corners).
inline std::vector<std::vector<vec3>> buildConvexMeshFaces(const std::vector<Plane>& planes,
                                                           float eps = 1e-5f) {
    std::vector<std::vector<vec3>> faces;
    const std::vector<vec3> corners = computeConvexMeshPoints(planes, eps);
    if (corners.size() < 4) {
        return faces; // unbounded / empty volume -> no finite faces
    }
    // Size the working quad to comfortably enclose the volume.
    vec3 centroid(0.0f);
    for (const vec3& c : corners) {
        centroid += c;
    }
    centroid /= static_cast<float>(corners.size());
    float radius = 0.0f;
    for (const vec3& c : corners) {
        radius = std::max(radius, length(c - centroid));
    }
    const float big = radius * 4.0f + 1.0f;

    for (std::size_t i = 0; i < planes.size(); ++i) {
        const Plane& pi = planes[i];
        // Tangent basis on the plane: u, v with v = cross(normal, u) so (u, v, normal) is right-handed.
        const vec3 helper =
            (std::fabs(pi.normal.x) > 0.9f) ? vec3(0.0f, 1.0f, 0.0f) : vec3(1.0f, 0.0f, 0.0f);
        const vec3 u = normalize(cross(pi.normal, helper));
        const vec3 v = cross(pi.normal, u);
        const vec3 c = pi.center(); // normal * d, the point on the plane nearest the origin
        // Quad wound CCW around the plane normal (outward face): edges u,v,-u,-v => normal = +pi.normal.
        std::vector<vec3> face = {c - u * big - v * big, c + u * big - v * big,
                                  c + u * big + v * big, c - u * big + v * big};
        for (std::size_t j = 0; j < planes.size(); ++j) {
            if (j == i) {
                continue; // the quad already lies on plane i
            }
            face = clipPolygon(face, planes[j], eps);
            if (face.size() < 3) {
                break; // fully clipped away -> plane i contributes no face
            }
        }
        if (face.size() >= 3) {
            faces.push_back(std::move(face));
        }
    }
    return faces;
}

// An indexed triangle mesh: a shared vertex list plus 3 indices per triangle, wound CCW (outward,
// matching the source faces). The GPU-/collider-ready form.
struct ConvexMesh3 {
    std::vector<vec3> vertices;
    std::vector<std::uint32_t> indices;
    std::size_t triangleCount() const { return indices.size() / 3; }
};

// Triangulate a set of convex polygon faces (e.g. from buildConvexMeshFaces) into an indexed triangle
// mesh — the final step of the planes -> corners -> faces -> mesh pipeline. Each convex face is
// fan-triangulated from its first vertex (valid precisely because the faces are convex), and vertices
// shared between faces are merged within `eps`, so the result is a compact indexed mesh ready to hand
// to a renderer or a collision system. Winding is preserved from the input faces (CCW / outward).
inline ConvexMesh3 triangulateConvexFaces(const std::vector<std::vector<vec3>>& faces,
                                          float eps = 1e-5f) {
    ConvexMesh3 mesh;
    auto indexOf = [&](const vec3& p) -> std::uint32_t {
        for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
            if (length(mesh.vertices[i] - p) <= eps) {
                return static_cast<std::uint32_t>(i);
            }
        }
        mesh.vertices.push_back(p);
        return static_cast<std::uint32_t>(mesh.vertices.size() - 1);
    };
    for (const std::vector<vec3>& face : faces) {
        if (face.size() < 3) {
            continue;
        }
        const std::uint32_t i0 = indexOf(face[0]);
        for (std::size_t k = 1; k + 1 < face.size(); ++k) {
            const std::uint32_t i1 = indexOf(face[k]);
            const std::uint32_t i2 = indexOf(face[k + 1]);
            mesh.indices.push_back(i0);
            mesh.indices.push_back(i1);
            mesh.indices.push_back(i2);
        }
    }
    return mesh;
}

// Convenience: bounding planes straight to an indexed triangle mesh (buildConvexMeshFaces then
// triangulateConvexFaces) — one call turns a box/cylinder/capsule bound into a drawable/collidable
// convex mesh. Empty for an unbounded / empty half-space set.
inline ConvexMesh3 triangulateConvexPlanes(const std::vector<Plane>& planes, float eps = 1e-5f) {
    return triangulateConvexFaces(buildConvexMeshFaces(planes, eps), eps);
}

} // namespace maz::math
