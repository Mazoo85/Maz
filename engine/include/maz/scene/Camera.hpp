#pragma once

#include "maz/math/Math.hpp"

// A perspective camera — the "eye" the 3D scene is drawn from. It produces the view and
// projection matrices a mesh renderer multiplies each vertex by (model → view → clip).
//
// Matrices follow the engine's Vulkan conventions: right-handed world space, clip depth
// 0..1, and the projection Y-flip handled by maz::math::perspective (Vulkan clip-space Y
// points down). Matrices are computed on demand — cheap, and keeps the type trivially
// testable with no GPU.

namespace maz::scene {

class Camera {
public:
    // --- Placement (world space) --------------------------------------------------
    void setPosition(const math::vec3& p) { position_ = p; }
    void setTarget(const math::vec3& t) { target_ = t; }
    void setUp(const math::vec3& up) { up_ = up; }

    // --- Lens ---------------------------------------------------------------------
    void setPerspective(float fovYRadians, float aspect, float zNear, float zFar);
    void setAspect(float aspect) { aspect_ = aspect; } // call on window resize

    // --- Queries ------------------------------------------------------------------
    const math::vec3& position() const { return position_; }
    const math::vec3& target() const { return target_; }
    float fovY() const { return fovY_; }
    float aspect() const { return aspect_; }
    float nearPlane() const { return near_; }
    float farPlane() const { return far_; }

    // Forward direction (unit vector from position toward target).
    math::vec3 forward() const { return math::normalize(target_ - position_); }

    // --- Matrices -----------------------------------------------------------------
    math::mat4 view() const;
    math::mat4 projection() const;
    math::mat4 viewProjection() const { return projection() * view(); }

private:
    math::vec3 position_{0.0f, 0.0f, 3.0f};
    math::vec3 target_{0.0f, 0.0f, 0.0f};
    math::vec3 up_{0.0f, 1.0f, 0.0f};

    float fovY_ = glm::radians(60.0f);
    float aspect_ = 16.0f / 9.0f;
    float near_ = 0.1f;
    float far_ = 100.0f;
};

} // namespace maz::scene
