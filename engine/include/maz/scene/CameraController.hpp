#pragma once

#include "maz/scene/Camera.hpp"
#include "maz/math/Math.hpp"

#include <cmath>

// Two camera rigs that drive a scene::Camera from intuitive parameters — OrbitCamera orbits a
// focus point at a distance via yaw/pitch (clamped to avoid the pole), zoom adjusts the distance;
// FlyCamera is a free-flight eye at a position looking along yaw/pitch (forward is -Z at zero,
// matching the camera convention), with moveLocal along its own axes. Both applyTo(Camera) set the
// camera's position/target. Angles in radians; the pitch clamp keeps right()/up well-defined.
// Composes maz::math + scene::Camera. NOT thread-safe. Smoothing/damping and roll are future
// refinements.

namespace maz::scene {

// Orbits a focus point at a distance, controlled by yaw/pitch (radians).
class OrbitCamera {
public:
    math::vec3 focus{0.0f, 0.0f, 0.0f};
    float yaw = 0.0f;         // radians, around +Y
    float pitch = 0.0f;       // radians, elevation
    float distance = 5.0f;
    float minPitch = -1.55334f; // ~-89°, avoids the straight-down gimbal degeneracy
    float maxPitch = 1.55334f;  // ~+89°, avoids the straight-up gimbal degeneracy
    float minDistance = 0.1f;
    float maxDistance = 1000.0f;

    // Spherical offset from focus. At yaw=0,pitch=0 the camera sits at focus + distance*(0,0,1)
    // (+Z of the focus), looking toward it. yaw rotates around +Y, pitch raises it.
    math::vec3 position() const {
        const float cp = std::cos(pitch), sp = std::sin(pitch);
        const float sy = std::sin(yaw), cy = std::cos(yaw);
        return focus + distance * math::vec3(cp * sy, sp, cp * cy);
    }

    // Yaw wraps freely; pitch is clamped to avoid flipping over the pole.
    void orbit(float deltaYaw, float deltaPitch) {
        yaw += deltaYaw;
        pitch = glm::clamp(pitch + deltaPitch, minPitch, maxPitch);
    }

    void zoom(float deltaDistance) {
        distance = glm::clamp(distance + deltaDistance, minDistance, maxDistance);
    }

    // The camera looks from the orbit position at the focus.
    void applyTo(Camera& cam) const {
        cam.setPosition(position());
        cam.setTarget(focus);
    }
};

// Free-flight camera at a position looking along yaw/pitch (radians).
class FlyCamera {
public:
    math::vec3 position{0.0f, 0.0f, 0.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
    float minPitch = -1.55334f; // ~-89°
    float maxPitch = 1.55334f;  // ~+89°

    // Look direction (-Z convention at yaw=0,pitch=0, matching the camera/Godot convention).
    // yaw=0,pitch=0 -> (0,0,-1); yaw=+90° -> (1,0,0); pitch=+90° -> (0,1,0).
    math::vec3 forward() const {
        const float cp = std::cos(pitch);
        return math::vec3(cp * std::sin(yaw), std::sin(pitch), -cp * std::cos(yaw));
    }

    // Horizontal (world-up based) right axis; unit. Undefined only when looking exactly along
    // +/-Y, which the pitch clamp excludes.
    math::vec3 right() const {
        return math::normalize(math::cross(forward(), math::vec3(0.0f, 1.0f, 0.0f)));
    }

    void look(float deltaYaw, float deltaPitch) {
        yaw += deltaYaw;
        pitch = glm::clamp(pitch + deltaPitch, minPitch, maxPitch);
    }

    // Move along the camera's own axes; up is world-up.
    void moveLocal(float forwardAmt, float rightAmt, float upAmt) {
        position += forward() * forwardAmt + right() * rightAmt + math::vec3(0.0f, 1.0f, 0.0f) * upAmt;
    }

    void applyTo(Camera& cam) const {
        cam.setPosition(position);
        cam.setTarget(position + forward());
    }
};

} // namespace maz::scene
