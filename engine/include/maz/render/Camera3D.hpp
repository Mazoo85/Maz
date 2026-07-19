#pragma once

#include "maz/math/Math.hpp"

#include <cmath>

namespace maz::render {

// Camera3D — the projection helper behind Godot's Camera3D: it turns a view + projection matrix (built from
// a look-at + perspective, or supplied directly) plus a viewport size into the screen↔world queries games
// lean on every frame:
//   * worldToScreen   — project a 3D world point to 2D screen pixels        (Godot unproject_position)
//   * screenToRay     — unproject a screen pixel to a world-space pick ray  (Godot project_ray_origin/normal)
//   * screenToWorld   — a world point a given distance down that ray        (Godot project_position)
//   * frustum / isPointVisible / isSphereVisible — the six view planes + containment tests
//                                                                            (Godot is_position_in_frustum)
// These drive mouse picking, world-space UI labels / health bars over 3D units, off-screen culling, and
// look-at aiming. It uses the engine's Vulkan clip convention (`math::perspective`: y-down NDC, depth 0..1),
// so screen coordinates have a top-left origin and match what the renderer draws. Pure matrix math,
// header-only, deterministic — it unit-tests exactly and drives a golden via a 2D projection of a 3D scene.

struct Ray3 {
    math::vec3 origin{0.0f, 0.0f, 0.0f};
    math::vec3 direction{0.0f, 0.0f, -1.0f}; // normalized
};

// Result of projecting a world point. `screen` is in pixels (origin top-left); `depth` is the 0..1 clip
// depth; `inFront` is false when the point is at or behind the camera plane (screen is then meaningless).
struct Projected {
    math::vec2 screen{0.0f, 0.0f};
    float depth = 0.0f;
    bool inFront = false;
};

// Six world-space frustum planes with INWARD normals: a point is inside when dot(plane.xyz, p) + plane.w >= 0
// for all six. Order: left, right, bottom, top, near, far.
struct FrustumPlanes {
    math::vec4 planes[6];
};

// Frustum vs axis-aligned box (given by its min/max corners) — the "positive-vertex" culling test.
// Returns true when the box is at least partially inside the frustum; false only when the box lies
// wholly outside one plane. Conservative (a box tucked in a frustum-corner gap may test visible), the
// standard fast reject used for scene culling. The AABB companion to Camera3D::isPointVisible /
// isSphereVisible — extract the planes once (Camera3D::frustum) and test many boxes against them.
inline bool frustumIntersectsAabb(const FrustumPlanes& f, math::vec3 boxMin, math::vec3 boxMax) {
    for (const math::vec4& pl : f.planes) {
        // The box corner farthest along this inward-facing plane normal.
        const math::vec3 pv(pl.x >= 0.0f ? boxMax.x : boxMin.x, pl.y >= 0.0f ? boxMax.y : boxMin.y,
                            pl.z >= 0.0f ? boxMax.z : boxMin.z);
        if (math::dot(math::vec3(pl), pv) + pl.w < 0.0f) {
            return false; // wholly outside this plane
        }
    }
    return true;
}

class Camera3D {
public:
    float viewportWidth = 1280.0f;
    float viewportHeight = 720.0f;

    // --- setup ---

    void lookAt(math::vec3 eye, math::vec3 target, math::vec3 up) {
        m_view = glm::lookAt(eye, target, up);
        m_eye = eye;
        recompute();
    }

    void perspective(float fovYRadians, float aspect, float zNear, float zFar) {
        m_proj = math::perspective(fovYRadians, aspect, zNear, zFar);
        recompute();
    }

    void setView(const math::mat4& v) {
        m_view = v;
        m_eye = math::vec3(glm::inverse(v)[3]);
        recompute();
    }
    void setProjection(const math::mat4& p) {
        m_proj = p;
        recompute();
    }

    const math::mat4& view() const { return m_view; }
    const math::mat4& projection() const { return m_proj; }
    const math::mat4& viewProjection() const { return m_viewProj; }
    math::vec3 eye() const { return m_eye; }

    // --- projection ---

    // World point -> screen pixels (origin top-left). `inFront` is false for points behind the camera.
    Projected worldToScreen(math::vec3 world) const {
        const math::vec4 clip = m_viewProj * math::vec4(world, 1.0f);
        Projected out;
        if (std::abs(clip.w) < 1e-9f) {
            return out;
        }
        const math::vec3 ndc = math::vec3(clip) / clip.w;
        out.screen.x = (ndc.x * 0.5f + 0.5f) * viewportWidth;
        out.screen.y = (ndc.y * 0.5f + 0.5f) * viewportHeight;
        out.depth = ndc.z;
        out.inFront = clip.w > 0.0f;
        return out;
    }

    // Screen pixel -> world-space pick ray (origin at the camera, direction into the scene).
    Ray3 screenToRay(math::vec2 screen) const {
        const math::vec3 nearW = unprojectNdc(screen, 0.0f);
        const math::vec3 farW = unprojectNdc(screen, 1.0f);
        Ray3 r;
        r.origin = m_eye;
        r.direction = math::normalize(farW - nearW);
        return r;
    }

    // Screen pixel at `distance` world units down the pick ray -> world point.
    math::vec3 screenToWorld(math::vec2 screen, float distance) const {
        const Ray3 r = screenToRay(screen);
        return r.origin + r.direction * distance;
    }

    // --- frustum ---

    // Extract the six world-space planes (Gribb-Hartmann; near uses row2 for Vulkan's 0..1 clip depth).
    FrustumPlanes frustum() const {
        // Rows of the view-projection matrix (glm stores column-major: row i = (m[0][i]..m[3][i])).
        const math::vec4 r0(m_viewProj[0][0], m_viewProj[1][0], m_viewProj[2][0], m_viewProj[3][0]);
        const math::vec4 r1(m_viewProj[0][1], m_viewProj[1][1], m_viewProj[2][1], m_viewProj[3][1]);
        const math::vec4 r2(m_viewProj[0][2], m_viewProj[1][2], m_viewProj[2][2], m_viewProj[3][2]);
        const math::vec4 r3(m_viewProj[0][3], m_viewProj[1][3], m_viewProj[2][3], m_viewProj[3][3]);
        FrustumPlanes f;
        f.planes[0] = normalizePlane(r3 + r0); // left
        f.planes[1] = normalizePlane(r3 - r0); // right
        f.planes[2] = normalizePlane(r3 + r1); // bottom
        f.planes[3] = normalizePlane(r3 - r1); // top
        f.planes[4] = normalizePlane(r2);      // near
        f.planes[5] = normalizePlane(r3 - r2); // far
        return f;
    }

    bool isPointVisible(math::vec3 world) const {
        const FrustumPlanes f = frustum();
        for (const math::vec4& p : f.planes) {
            if (math::dot(math::vec3(p), world) + p.w < 0.0f) {
                return false;
            }
        }
        return true;
    }

    // A sphere is visible unless it lies entirely outside any single plane.
    bool isSphereVisible(math::vec3 center, float radius) const {
        const FrustumPlanes f = frustum();
        for (const math::vec4& p : f.planes) {
            if (math::dot(math::vec3(p), center) + p.w < -radius) {
                return false;
            }
        }
        return true;
    }

private:
    math::vec3 unprojectNdc(math::vec2 screen, float ndcZ) const {
        const float nx = (screen.x / viewportWidth) * 2.0f - 1.0f;
        const float ny = (screen.y / viewportHeight) * 2.0f - 1.0f;
        math::vec4 world = m_invViewProj * math::vec4(nx, ny, ndcZ, 1.0f);
        if (std::abs(world.w) > 1e-9f) {
            world /= world.w;
        }
        return math::vec3(world);
    }

    static math::vec4 normalizePlane(math::vec4 p) {
        const float len = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
        return len > 1e-9f ? p / len : p;
    }

    void recompute() {
        m_viewProj = m_proj * m_view;
        m_invViewProj = glm::inverse(m_viewProj);
    }

    math::mat4 m_view{1.0f};
    math::mat4 m_proj{1.0f};
    math::mat4 m_viewProj{1.0f};
    math::mat4 m_invViewProj{1.0f};
    math::vec3 m_eye{0.0f, 0.0f, 0.0f};
};

} // namespace maz::render
