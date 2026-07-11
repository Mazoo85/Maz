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

// 2D pixel-space orthographic projection, origin top-left, y increasing downward.
// Vulkan clip-space y points down (opposite of OpenGL), so top maps to the near-clip edge:
// glm::ortho(l, r, bottom, top) sends `bottom` -> -1; we pass 0 as bottom so y=0 is the top.
inline mat4 ortho2D(float width, float height) {
    return glm::ortho(0.0f, width, 0.0f, height, -1.0f, 1.0f);
}

} // namespace maz::math
