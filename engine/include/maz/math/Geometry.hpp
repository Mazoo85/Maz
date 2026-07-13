#pragma once

// Geometry primitives (AABB, Ray, Plane) and intersection helpers, built on the
// maz::math GLM aliases. Include this rather than GLM directly (see Math.hpp).
//
// Conventions:
//   - Plane: dot(normal, x) + d == 0. signedDistance is positive on the side the
//     normal points toward. fromPointNormal builds a plane through a point.
//   - Ray: dir need NOT be normalized. Intersection parameters (t) are expressed
//     in dir-length units, i.e. the hit point is origin + t*dir.

#include "maz/math/Math.hpp"

#include <cmath>
#include <limits>

namespace maz::math {

// Axis-aligned bounding box. An "invalid" box has min > max on every axis and is
// the identity for expand/merge (see invalid()).
struct Aabb {
    vec3 min;
    vec3 max;

    static Aabb invalid() {
        const float inf = std::numeric_limits<float>::infinity();
        return Aabb{vec3(inf), vec3(-inf)};
    }

    bool isValid() const {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }

    // Closed/inclusive containment.
    bool contains(vec3 p) const {
        return p.x >= min.x && p.x <= max.x &&
               p.y >= min.y && p.y <= max.y &&
               p.z >= min.z && p.z <= max.z;
    }

    vec3 center() const {
        return (min + max) * 0.5f;
    }

    vec3 size() const {
        return max - min;
    }

    vec3 extents() const {
        return (max - min) * 0.5f;
    }

    // Grow to include p (mutating).
    void expand(vec3 p) {
        min = glm::min(min, p);
        max = glm::max(max, p);
    }

    Aabb expanded(vec3 p) const {
        Aabb out = *this;
        out.expand(p);
        return out;
    }

    void merge(const Aabb& o) {
        min = glm::min(min, o.min);
        max = glm::max(max, o.max);
    }

    Aabb merged(const Aabb& o) const {
        Aabb out = *this;
        out.merge(o);
        return out;
    }

    // Inclusive overlap on all three axes.
    bool intersects(const Aabb& o) const {
        return min.x <= o.max.x && max.x >= o.min.x &&
               min.y <= o.max.y && max.y >= o.min.y &&
               min.z <= o.max.z && max.z >= o.min.z;
    }
};

struct Ray {
    vec3 origin;
    vec3 dir;

    vec3 at(float t) const {
        return origin + dir * t;
    }
};

// Plane: dot(normal, x) + d == 0.
struct Plane {
    vec3 normal;
    float d;

    float signedDistance(vec3 p) const {
        return dot(normal, p) + d;
    }

    // -1 behind, 0 on (within epsilon band), +1 in front.
    int side(vec3 p) const {
        const float sd = signedDistance(p);
        const float eps = 1e-6f;
        if (sd > eps) {
            return 1;
        }
        if (sd < -eps) {
            return -1;
        }
        return 0;
    }

    static Plane fromPointNormal(vec3 point, vec3 n) {
        return Plane{n, -dot(n, point)};
    }
};

// Ray vs plane. Returns false when the ray is parallel to the plane or the hit is
// behind the origin (t < 0). On a true return, tOut is the hit parameter.
inline bool rayPlane(const Ray& r, const Plane& pl, float& tOut) {
    const float denom = dot(pl.normal, r.dir);
    const float eps = 1e-6f;
    if (std::fabs(denom) < eps) {
        return false;
    }
    const float t = -(dot(pl.normal, r.origin) + pl.d) / denom;
    if (t < 0.0f) {
        return false;
    }
    tOut = t;
    return true;
}

// Ray vs AABB via the slab method, with an explicit per-axis guard so a zero
// direction component never divides. On a true return, tminOut is the entry
// parameter (clamped to 0 when the ray starts inside the box).
inline bool rayAabb(const Ray& r, const Aabb& a, float& tminOut) {
    const float eps = 1e-6f;
    float tmin = -std::numeric_limits<float>::infinity();
    float tmax = std::numeric_limits<float>::infinity();

    for (int k = 0; k < 3; ++k) {
        const float o = r.origin[k];
        const float dk = r.dir[k];
        const float lo = a.min[k];
        const float hi = a.max[k];

        if (std::fabs(dk) < eps) {
            // Ray parallel to this slab: must already be within it.
            if (o < lo || o > hi) {
                return false;
            }
        } else {
            float t1 = (lo - o) / dk;
            float t2 = (hi - o) / dk;
            if (t1 > t2) {
                const float tmp = t1;
                t1 = t2;
                t2 = tmp;
            }
            if (t1 > tmin) {
                tmin = t1;
            }
            if (t2 < tmax) {
                tmax = t2;
            }
            if (tmin > tmax) {
                return false;
            }
        }
    }

    // Box entirely behind the ray origin.
    if (tmax < 0.0f) {
        return false;
    }

    // Ray starts inside: report entry at 0 rather than a negative tmin.
    tminOut = tmin < 0.0f ? 0.0f : tmin;
    return true;
}

inline bool aabbAabb(const Aabb& a, const Aabb& b) {
    return a.intersects(b);
}

inline bool pointInAabb(vec3 p, const Aabb& a) {
    return a.contains(p);
}

} // namespace maz::math
