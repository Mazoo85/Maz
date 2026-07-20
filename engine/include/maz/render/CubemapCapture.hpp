#pragma once

#include "maz/render/Cubemap.hpp" // CubeFace, directionToCube, cubeToDirection
#include "maz/math/Math.hpp"      // mat4, perspective, glm::lookAt

#include <glm/gtc/matrix_transform.hpp> // glm::lookAt

// maz::render reflection-probe / cubemap CAPTURE math — the render-side counterpart to the SAMPLING helpers
// in Cubemap.hpp (`directionToCube` / `cubeToDirection`). To build a reflection probe or a dynamic
// environment map, the engine renders the scene SIX times from the probe's position, once down each cube
// axis (+X,-X,+Y,-Y,+Z,-Z), into the six faces of a cubemap. That needs, per face, a camera VIEW matrix
// (look direction + up vector) and a 90° field-of-view PROJECTION so the six frustums tile the whole sphere
// with no gaps or overlap. This header provides exactly those — `cubeFaceView(face, center)` and
// `cubeFaceProjection(near, far)` — using the same axis convention as the sampler, so a direction that
// `directionToCube` says belongs to face F really is the face captured by `cubeFaceView(F, ...)`. That
// consistency is the thing cubemap capture most often gets wrong, and it is exactly what the unit test pins
// down. Pure matrix math — no GPU — so it verifies headlessly; the actual six render passes run on the GPU.
//
// Honest tag: the view/projection MATRICES are CPU-verified here; issuing the six render passes into a
// cubemap render target and prefiltering the result for roughness are the GPU steps this feeds.
namespace maz::render {

// The 90°-FOV, unit-aspect perspective projection every cube face uses (Vulkan clip space). `zNear`/`zFar`
// bound the captured depth range around the probe.
inline math::mat4 cubeFaceProjection(float zNear = 0.05f, float zFar = 1000.0f) {
    return math::perspective(1.5707963267948966f /* 90° */, 1.0f, zNear, zFar);
}

// The view matrix for one cube face: a camera at `center` looking down that face's axis, with the up vector
// that matches Cubemap.hpp's sampling convention (the standard Vulkan/OpenGL cube layout — world Y-down as
// the face "up" for the ±X/±Z faces, ±Z for the ±Y faces).
inline math::mat4 cubeFaceView(CubeFace face, const math::vec3& center) {
    math::vec3 forward{0.0f, 0.0f, 0.0f};
    math::vec3 up{0.0f, 0.0f, 0.0f};
    switch (face) {
        case CubeFace::PosX: forward = {1.0f, 0.0f, 0.0f};  up = {0.0f, -1.0f, 0.0f}; break;
        case CubeFace::NegX: forward = {-1.0f, 0.0f, 0.0f}; up = {0.0f, -1.0f, 0.0f}; break;
        case CubeFace::PosY: forward = {0.0f, 1.0f, 0.0f};  up = {0.0f, 0.0f, 1.0f};  break;
        case CubeFace::NegY: forward = {0.0f, -1.0f, 0.0f}; up = {0.0f, 0.0f, -1.0f}; break;
        case CubeFace::PosZ: forward = {0.0f, 0.0f, 1.0f};  up = {0.0f, -1.0f, 0.0f}; break;
        case CubeFace::NegZ: forward = {0.0f, 0.0f, -1.0f}; up = {0.0f, -1.0f, 0.0f}; break;
    }
    return glm::lookAt(center, center + forward, up);
}

// Combined view-projection for one face — what a capture pass binds as its camera matrix.
inline math::mat4 cubeFaceViewProjection(CubeFace face, const math::vec3& center, float zNear = 0.05f,
                                         float zFar = 1000.0f) {
    return cubeFaceProjection(zNear, zFar) * cubeFaceView(face, center);
}

// The world-space forward (look) direction of a face's capture camera — the axis it points down. Handy for
// culling which objects fall in a face's frustum, and the value the round-trip test checks against the
// sampler.
inline math::vec3 cubeFaceForward(CubeFace face) {
    switch (face) {
        case CubeFace::PosX: return {1.0f, 0.0f, 0.0f};
        case CubeFace::NegX: return {-1.0f, 0.0f, 0.0f};
        case CubeFace::PosY: return {0.0f, 1.0f, 0.0f};
        case CubeFace::NegY: return {0.0f, -1.0f, 0.0f};
        case CubeFace::PosZ: return {0.0f, 0.0f, 1.0f};
        case CubeFace::NegZ: return {0.0f, 0.0f, -1.0f};
    }
    return {0.0f, 0.0f, 1.0f};
}

} // namespace maz::render
