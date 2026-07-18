#pragma once

#include "maz/math/Math.hpp"

#include <algorithm>
#include <cmath>
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

} // namespace maz::math
