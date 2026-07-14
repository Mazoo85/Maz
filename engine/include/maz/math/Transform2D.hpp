#pragma once

// 2D affine transform: a 2x2 linear basis plus a translation origin, i.e. the map
// x -> basis * x + origin. Column-major throughout (matching GLM/Math.hpp): the
// basis columns are the transformed axes and the translation lives in origin. This
// representation is closed under compose (operator*) and inverse, including
// non-uniform scale and rotation (the basis is a general invertible 2x2, so
// composing or inverting a scaled+rotated transform stays exact rather than being
// forced back onto a rotation). The Godot Transform2D analog; complements iter5's
// 3D Transform. Factories: identity/translation/rotation (CCW radians)/scale.
// xform maps a point (with translation); xformBasis maps a direction (no
// translation).

#include "maz/math/Math.hpp"
#include "maz/core/Assert.hpp"

#include <cmath>

namespace maz::math {

using glm::mat2;

struct Transform2D {
    mat2 basis{1.0f};
    vec2 origin{0.0f, 0.0f};

    static Transform2D identity() { return Transform2D{}; }

    static Transform2D translation(vec2 t) { return Transform2D{mat2(1.0f), t}; }

    // CCW rotation: glm::mat2 is column-major, so mat2(c, s, -s, c) has col0=(c,s),
    // col1=(-s,c); xform((1,0)) = col0 = (c,s), so +X rotates toward +Y (CCW).
    static Transform2D rotation(float radians) {
        const float c = std::cos(radians), s = std::sin(radians);
        return Transform2D{mat2(c, s, -s, c), vec2(0.0f)};
    }

    static Transform2D scale(vec2 s) { return Transform2D{mat2(s.x, 0.0f, 0.0f, s.y), vec2(0.0f)}; }

    vec2 xform(vec2 p) const { return basis * p + origin; }
    vec2 xformBasis(vec2 v) const { return basis * v; }

    Transform2D operator*(const Transform2D& other) const {
        return Transform2D{basis * other.basis, basis * other.origin + origin};
    }

    // Precondition: basis is invertible (no zero-scale axis); otherwise the
    // result contains inf/NaN from the singular matrix inverse.
    Transform2D inverse() const {
        const mat2 ib = glm::inverse(basis);
        return Transform2D{ib, -(ib * origin)};
    }
};

} // namespace maz::math
