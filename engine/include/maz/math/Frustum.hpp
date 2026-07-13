#pragma once

// View frustum: the six clip-space planes extracted from a view-projection matrix,
// with containment/intersection tests for culling. Built on maz::math Geometry
// primitives (Plane, Aabb) and the engine's Vulkan conventions.
//
// Conventions:
//   - Depth range 0..1 (Vulkan clip space): the near plane is row 2, NOT row3 + row2.
//   - Planes store inward-pointing normals, so signedDistance(p) >= 0 means "inside".

#include "maz/math/Geometry.hpp"

namespace maz::math {

// Extract row i of a column-major GLM matrix (row i = component i of each column).
inline vec4 row(const mat4& m, int i) {
    return vec4(m[0][i], m[1][i], m[2][i], m[3][i]);
}

struct Frustum {
    Plane planes[6];
    enum { Left, Right, Bottom, Top, Near, Far };

    static Frustum fromViewProj(const mat4& m) {
        const vec4 r0 = row(m, 0);
        const vec4 r1 = row(m, 1);
        const vec4 r2 = row(m, 2);
        const vec4 r3 = row(m, 3);

        vec4 rows[6];
        rows[Left] = r3 + r0;
        rows[Right] = r3 - r0;
        rows[Bottom] = r3 + r1;
        rows[Top] = r3 - r1;
        rows[Near] = r2; // Vulkan [0,1] depth: near = row 2, not r3 + r2.
        rows[Far] = r3 - r2;

        Frustum out;
        for (int k = 0; k < 6; ++k) {
            const vec4 pv = rows[k];
            vec3 nrm = vec3(pv.x, pv.y, pv.z);
            float dist = pv.w;
            const float len = glm::length(nrm);
            if (len > 0.0f) {
                nrm /= len;
                dist /= len;
            }
            out.planes[k] = Plane{nrm, dist};
        }
        return out;
    }

    // Point is inside when it lies on the inward side of all six planes.
    bool contains(vec3 p) const {
        for (int k = 0; k < 6; ++k) {
            if (planes[k].signedDistance(p) < 0.0f) {
                return false;
            }
        }
        return true;
    }

    // AABB vs frustum via the positive-vertex test: for each plane, pick the box
    // corner furthest along the plane normal; if that corner is outside, the whole
    // box is outside. May report false positives for boxes near a corner, never a
    // false negative — correct for conservative culling.
    bool intersects(const Aabb& box) const {
        for (int i = 0; i < 6; ++i) {
            vec3 pv;
            for (int k = 0; k < 3; ++k) {
                pv[k] = (planes[i].normal[k] >= 0.0f) ? box.max[k] : box.min[k];
            }
            if (planes[i].signedDistance(pv) < 0.0f) {
                return false;
            }
        }
        return true;
    }

    // Sphere vs frustum: outside if the center is further than r behind any plane.
    bool intersectsSphere(vec3 c, float r) const {
        for (int i = 0; i < 6; ++i) {
            if (planes[i].signedDistance(c) < -r) {
                return false;
            }
        }
        return true;
    }
};

} // namespace maz::math
