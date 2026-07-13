#pragma once

// Maz math is GLM, re-exported under maz::math with the conventions the engine relies on:
//   - Depth range 0..1 (Vulkan clip space), not OpenGL's -1..1.
//   - Right-handed coordinates.
// Include this header rather than GLM directly so these defines are always set consistently.

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace maz::math {

using glm::mat3;
using glm::mat4;
using glm::quat;
using glm::vec2;
using glm::vec3;
using glm::vec4;

using glm::cross;
using glm::dot;
using glm::normalize;

// Vulkan-correct perspective (clip-space Y points down vs OpenGL, so flip [1][1]).
inline mat4 perspective(float fovYRadians, float aspect, float zNear, float zFar) {
    mat4 proj = glm::perspective(fovYRadians, aspect, zNear, zFar);
    proj[1][1] *= -1.0f;
    return proj;
}

// Vulkan-correct 3D orthographic projection (parallel projection — no perspective divide, so equal-size
// objects at every depth: the isometric / CAD / 2.5D-strategy camera). Clip-space Y points down vs OpenGL
// (flip [1][1]) and depth is 0..1, matching `perspective`. Bounds are in eye-space units.
inline mat4 orthographic(float left, float right, float bottom, float top, float zNear, float zFar) {
    mat4 proj = glm::ortho(left, right, bottom, top, zNear, zFar);
    proj[1][1] *= -1.0f;
    return proj;
}

// Convenience ortho sized by its VERTICAL extent + aspect ratio, centred — Godot's Camera3D `size` in
// Orthogonal mode. `verticalSize` world units fill the viewport height; width follows the aspect.
inline mat4 orthographicSize(float verticalSize, float aspect, float zNear, float zFar) {
    const float halfH = verticalSize * 0.5f;
    const float halfW = halfH * aspect;
    return orthographic(-halfW, halfW, -halfH, halfH, zNear, zFar);
}

// 2D pixel-space orthographic projection, origin top-left, y increasing downward.
// Vulkan clip-space y points down (opposite of OpenGL), so top maps to the near-clip edge:
// glm::ortho(l, r, bottom, top) sends `bottom` -> -1; we pass 0 as bottom so y=0 is the top.
inline mat4 ortho2D(float width, float height) {
    return glm::ortho(0.0f, width, 0.0f, height, -1.0f, 1.0f);
}

} // namespace maz::math
