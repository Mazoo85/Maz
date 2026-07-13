#pragma once

// Affine transform: a 3x3 linear basis plus a translation origin, i.e. the map
// x -> basis * x + origin. Column-major throughout (matching GLM/Math.hpp): the
// basis columns are the transformed axes and the translation lives in column 3 of
// the equivalent mat4. This representation is closed under compose (operator*) and
// inverse, including non-uniform scale (the basis is a general invertible 3x3, so
// composing or inverting a scaled+rotated transform stays exact rather than being
// forced back onto a rotation).

#include "maz/math/Math.hpp"

namespace maz::math {

struct Transform {
    mat3 basis;
    vec3 origin;

    static Transform identity() { return Transform{mat3(1.0f), vec3(0.0f)}; }

    static Transform fromTRS(vec3 t, quat r, vec3 s) {
        mat3 b = glm::mat3_cast(r);
        b[0] *= s.x; b[1] *= s.y; b[2] *= s.z; // column-major: scaling columns = R*S (NOT rows/S*R)
        return Transform{b, t};
    }

    static Transform fromMatrix(const mat4& m) { return Transform{mat3(m), vec3(m[3])}; }

    vec3 xform(vec3 p) const { return basis * p + origin; }
    vec3 xformBasis(vec3 v) const { return basis * v; }

    Transform operator*(const Transform& other) const {
        return Transform{basis * other.basis, basis * other.origin + origin};
    }

    // Precondition: basis is invertible (no zero-scale axis); otherwise the
    // result contains inf/NaN from the singular matrix inverse.
    Transform inverse() const {
        mat3 ib = glm::inverse(basis);
        return Transform{ib, -(ib * origin)};
    }

    mat4 toMatrix() const {
        mat4 m(basis);
        m[3] = vec4(origin, 1.0f);
        return m;
    }
};

} // namespace maz::math
