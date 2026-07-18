#pragma once

#include "maz/math/Math.hpp"

#include <cmath>

namespace maz::math {

// Transform2D — Godot's Transform2D: the 2x3 affine matrix behind every Node2D. It stores two basis columns
// (`x`, `y`) plus an `origin` translation; a point maps as `x*p.x + y*p.y + origin`. This is the value type
// the engine's 2D world runs on — placing/parenting sprites, converting between local and world/screen space
// (xform / xformInv), composing a parent's transform with a child's (operator*), and reading back a node's
// rotation / scale / skew. It complements `scene::TransformGraph` (a hierarchy of decomposed TRS nodes): this
// is the flat matrix those nodes ultimately bake to. Pure math, header-only, deterministic — it unit-tests
// exactly and drives a golden (a shape drawn under several transforms).

struct Transform2D {
    vec2 x{1.0f, 0.0f};      // basis column 0 (local +X in world space)
    vec2 y{0.0f, 1.0f};      // basis column 1 (local +Y in world space)
    vec2 origin{0.0f, 0.0f}; // translation

    Transform2D() = default;
    Transform2D(vec2 xCol, vec2 yCol, vec2 o) : x(xCol), y(yCol), origin(o) {}

    // --- builders ---
    static Transform2D identity() { return Transform2D{}; }

    static Transform2D rotation(float radians) {
        const float c = std::cos(radians);
        const float s = std::sin(radians);
        return Transform2D{vec2(c, s), vec2(-s, c), vec2(0.0f, 0.0f)};
    }

    static Transform2D scaling(vec2 s) {
        return Transform2D{vec2(s.x, 0.0f), vec2(0.0f, s.y), vec2(0.0f, 0.0f)};
    }

    static Transform2D translation(vec2 t) {
        return Transform2D{vec2(1.0f, 0.0f), vec2(0.0f, 1.0f), t};
    }

    // Godot's Transform2D(rotation, scale, skew, position) constructor: the canonical way to build a node
    // transform from its editable properties.
    static Transform2D compose(float rotationRad, vec2 scale, vec2 position, float skewRad = 0.0f) {
        const float cr = std::cos(rotationRad);
        const float sr = std::sin(rotationRad);
        const float cs = std::cos(rotationRad + skewRad);
        const float ss = std::sin(rotationRad + skewRad);
        return Transform2D{vec2(cr, sr) * scale.x, vec2(-ss, cs) * scale.y, position};
    }

    // --- application ---
    // Transform a POINT (includes translation).
    vec2 xform(vec2 p) const { return x * p.x + y * p.y + origin; }
    // Transform a VECTOR / direction (basis only, no translation).
    vec2 basisXform(vec2 v) const { return x * v.x + y * v.y; }
    // Transform a direction by the TRANSPOSED basis (dot with each basis column) — Godot's
    // Transform2D.basis_xform_inv. For an orthonormal basis this exactly undoes basisXform.
    vec2 basisXformInv(vec2 v) const { return vec2(x.x * v.x + x.y * v.y, y.x * v.x + y.y * v.y); }
    // Inverse-transform a point (world -> local). Uses the full affine inverse, so it is correct even when
    // the transform is scaled or skewed.
    vec2 xformInv(vec2 p) const { return affineInverse().xform(p); }

    float determinant() const { return x.x * y.y - x.y * y.x; }

    // Full affine inverse.
    Transform2D affineInverse() const {
        const float det = determinant();
        const float invDet = (std::abs(det) > 1e-12f) ? 1.0f / det : 0.0f;
        // Inverse basis (2x2).
        const vec2 ix(y.y * invDet, -x.y * invDet);
        const vec2 iy(-y.x * invDet, x.x * invDet);
        Transform2D r{ix, iy, vec2(0.0f, 0.0f)};
        r.origin = -r.basisXform(origin);
        return r;
    }

    // Composition: (a * b).xform(p) == a.xform(b.xform(p)) — parent * child, like Godot's operator*.
    Transform2D operator*(const Transform2D& o) const {
        Transform2D r;
        r.x = basisXform(o.x);
        r.y = basisXform(o.y);
        r.origin = xform(o.origin);
        return r;
    }

    // --- decomposition (Godot get_rotation / get_scale / get_skew) ---
    float getRotation() const { return std::atan2(x.y, x.x); }

    vec2 getScale() const {
        const float det = determinant();
        const float sx = std::sqrt(x.x * x.x + x.y * x.y);
        const float syLen = std::sqrt(y.x * y.x + y.y * y.y);
        const float sy = (det < 0.0f) ? -syLen : syLen; // negative when the basis is mirrored
        return vec2(sx, sy);
    }

    float getSkew() const {
        const float det = determinant();
        const vec2 xn = normalize(x);
        vec2 yn = normalize(y);
        if (det < 0.0f) {
            yn = -yn;
        }
        return std::acos(clampF(dot(xn, yn), -1.0f, 1.0f)) - 1.5707963267948966f;
    }

    // Gram-Schmidt: keep origin + rotation, drop scale/skew (unit, perpendicular basis).
    Transform2D orthonormalized() const {
        vec2 nx = normalize(x);
        vec2 ny = y - nx * dot(nx, y);
        ny = normalize(ny);
        return Transform2D{nx, ny, origin};
    }

    // Decompose both, lerp position + scale and shortest-arc the rotation, recompose (Godot interpolate_with).
    Transform2D interpolateWith(const Transform2D& to, float t) const {
        const vec2 p1 = origin, p2 = to.origin;
        const float r1 = getRotation(), r2 = to.getRotation();
        const vec2 s1 = getScale(), s2 = to.getScale();
        const vec2 p = p1 + (p2 - p1) * t;
        const float r = lerpAngle(r1, r2, t);
        const vec2 s = s1 + (s2 - s1) * t;
        return compose(r, s, p);
    }

    // --- apply relative (global vs _local), mirroring Godot's Transform2D (M278) ---
    // Global translate: shift the origin in world space.
    Transform2D translated(vec2 offset) const { return Transform2D{x, y, origin + offset}; }
    // Local translate: shift along the transform's own axes.
    Transform2D translatedLocal(vec2 offset) const {
        return Transform2D{x, y, origin + basisXform(offset)};
    }
    // Global rotate: left-multiply (rotates the whole frame, origin included, about the world origin).
    Transform2D rotated(float radians) const { return rotation(radians) * *this; }
    // Local rotate: right-multiply (spin in place, origin unchanged).
    Transform2D rotatedLocal(float radians) const { return *this * rotation(radians); }
    // Global scale: scale every column component-wise (basis rows + origin) — Godot's scaled.
    Transform2D scaled(vec2 s) const {
        return Transform2D{vec2(x.x * s.x, x.y * s.y), vec2(y.x * s.x, y.y * s.y),
                           vec2(origin.x * s.x, origin.y * s.y)};
    }
    // Local scale: scale the basis columns, origin unchanged — Godot's scaled_local.
    Transform2D scaledLocal(vec2 s) const { return Transform2D{x * s.x, y * s.y, origin}; }

private:
    static float clampF(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    // Shortest-arc angle interpolation.
    static float lerpAngle(float a, float b, float t) {
        const float twoPi = 6.283185307179586f;
        float diff = std::fmod(b - a, twoPi);
        diff = std::fmod(2.0f * diff, twoPi) - diff; // shortest signed difference
        return a + diff * t;
    }
};

} // namespace maz::math
