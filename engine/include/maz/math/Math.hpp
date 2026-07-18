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

// 2D pixel-space orthographic projection with a top-left origin: pixel (0,0) is the top-left
// corner, +X right, +Y down. Depth is Vulkan's 0..1.
//
// The engine renders with a positive-height viewport and bakes the Vulkan Y-flip into the
// projection (see perspective() above). For an ortho that means using the OpenGL-style
// bottom-origin form `glm::ortho(0, w, 0, h)`: on Vulkan's Y-down NDC + positive viewport that
// lands pixel y=0 at the top of the framebuffer, i.e. a top-left origin — the natural convention
// for 2D sprites, tilemaps and HUDs.
inline mat4 ortho2D(float width, float height) {
    return glm::ortho(0.0f, width, 0.0f, height, -1.0f, 1.0f);
}

} // namespace maz::math
