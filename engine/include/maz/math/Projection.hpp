#pragma once

#include "maz/math/Math.hpp" // mat4, vec3, vec4

#include <cmath>

// maz::math Projection — Godot's Projection: a 4x4 matrix specialized for camera projections, with the
// projection-specific constructors (perspective / orthographic / frustum) and the queries you actually
// ask of one (near/far plane, vertical FOV, aspect, is-it-orthographic) that a raw mat4 does not spell.
// Conventions match the rest of maz::math: right-handed, clip-space depth 0..1 (Vulkan). NOTE: unlike
// `math::perspective` in Math.hpp — which flips [1][1] for the swapchain's inverted Y and is what the
// renderer feeds the GPU — Projection is the pure math type (no Y flip), so its queries invert cleanly.
// Header-only, deterministic — unit-tested by round-tripping the constructor inputs back out.
namespace maz::math {

struct Projection {
    mat4 m{1.0f}; // identity by default

    Projection() = default;
    explicit Projection(const mat4& matrix) : m(matrix) {}

    // Perspective projection from vertical FOV (radians), aspect (width/height) and clip planes.
    static Projection perspective(float fovYRadians, float aspect, float zNear, float zFar) {
        return Projection(glm::perspectiveRH_ZO(fovYRadians, aspect, zNear, zFar));
    }
    // Off-axis / symmetric orthographic box.
    static Projection orthographic(float left, float right, float bottom, float top, float zNear,
                                   float zFar) {
        return Projection(glm::orthoRH_ZO(left, right, bottom, top, zNear, zFar));
    }
    // General (possibly off-axis) perspective frustum.
    static Projection frustum(float left, float right, float bottom, float top, float zNear,
                              float zFar) {
        return Projection(glm::frustumRH_ZO(left, right, bottom, top, zNear, zFar));
    }

    // Transform a homogeneous point (no perspective divide) — Godot's Projection.xform(Vector4).
    vec4 xform(const vec4& v) const { return m * v; }

    // Project a 3D point and apply the perspective divide (clip -> NDC).
    vec3 project(const vec3& p) const {
        const vec4 r = m * vec4(p, 1.0f);
        const float w = r.w != 0.0f ? r.w : 1.0f;
        return vec3(r.x / w, r.y / w, r.z / w);
    }

    Projection operator*(const Projection& o) const { return Projection(m * o.m); }
    Projection inverse() const { return Projection(glm::inverse(m)); }
    float determinant() const { return glm::determinant(m); }

    // Orthographic projections keep w == 1 (m[2][3] == 0); perspective ones set m[2][3] == -1.
    bool isOrthogonal() const { return std::abs(m[2][3]) < 1e-6f; }

    // width/height ratio recovered from the matrix — Godot's Projection.get_aspect.
    float getAspect() const { return m[1][1] / m[0][0]; }

    // Vertical field of view in radians (perspective only) — Godot's Projection.get_fov (which returns
    // degrees; convert with glm::degrees if you need Godot's exact units).
    float getFovYRadians() const { return 2.0f * std::atan(1.0f / m[1][1]); }

    // Near / far clip distances recovered from the depth row (RH, ZO). Near solves NDC z' = 0 for both
    // perspective and orthographic layouts.
    float getZNear() const { return m[3][2] / m[2][2]; }
    float getZFar() const {
        const float a = m[2][2];
        if (isOrthogonal()) {
            return (m[3][2] - 1.0f) / m[2][2]; // ortho: z'=1 at far
        }
        const float nearP = m[3][2] / a;
        return a * nearP / (1.0f + a);
    }
};

} // namespace maz::math
