#pragma once

#include "maz/math/Geometry3D.hpp"

#include <cmath>

// maz::render cubemap direction mapping — the sampling math shared by reflection probes, skyboxes, and
// image-based lighting. A cubemap stores a 360° environment across six square faces; to look one up you
// convert a 3D direction into "which face + where on it (u,v)", and to bake or debug it you go the other
// way. This is the standard OpenGL/Vulkan cube mapping (major-axis selection with the conventional per-face
// s/t axes), so a Maz cubemap matches what artists author elsewhere. Pure math — unit-tested headlessly; a
// GPU samples the actual texels, and reflection-probe capture is a separate render pass.
//
// Scope note (honest): the direction<->face/uv convention only. It does not allocate or sample real cubemap
// textures, do seamless edge filtering, or prefilter a mip chain (roughness) — those are the renderer's job.
namespace maz::render {

// Cube faces in the conventional order (matches GL_TEXTURE_CUBE_MAP_POSITIVE_X + i).
enum class CubeFace { PosX = 0, NegX = 1, PosY = 2, NegY = 3, PosZ = 4, NegZ = 5 };

struct CubeSample {
    CubeFace face;
    float u; // [0,1]
    float v; // [0,1]
};

// Map a direction (need not be normalized) to the cube face and face UV it samples.
inline CubeSample directionToCube(const math::vec3& dir) {
    const float ax = std::fabs(dir.x), ay = std::fabs(dir.y), az = std::fabs(dir.z);
    CubeFace face;
    float sc, tc, ma; // s, t, and the major-axis magnitude (GL convention)

    if (ax >= ay && ax >= az) {
        ma = ax;
        if (dir.x >= 0.0f) { face = CubeFace::PosX; sc = -dir.z; tc = -dir.y; }
        else               { face = CubeFace::NegX; sc =  dir.z; tc = -dir.y; }
    } else if (ay >= az) {
        ma = ay;
        if (dir.y >= 0.0f) { face = CubeFace::PosY; sc =  dir.x; tc =  dir.z; }
        else               { face = CubeFace::NegY; sc =  dir.x; tc = -dir.z; }
    } else {
        ma = az;
        if (dir.z >= 0.0f) { face = CubeFace::PosZ; sc =  dir.x; tc = -dir.y; }
        else               { face = CubeFace::NegZ; sc = -dir.x; tc = -dir.y; }
    }

    const float inv = ma > 0.0f ? 0.5f / ma : 0.0f;
    return CubeSample{face, sc * inv + 0.5f, tc * inv + 0.5f};
}

// Inverse: the (normalized) direction pointing at face UV (u,v).
inline math::vec3 cubeToDirection(CubeFace face, float u, float v) {
    const float sc = 2.0f * u - 1.0f;
    const float tc = 2.0f * v - 1.0f;
    math::vec3 d{0.0f, 0.0f, 0.0f};
    switch (face) {
        case CubeFace::PosX: d = math::vec3{1.0f, -tc, -sc}; break;
        case CubeFace::NegX: d = math::vec3{-1.0f, -tc, sc}; break;
        case CubeFace::PosY: d = math::vec3{sc, 1.0f, tc}; break;
        case CubeFace::NegY: d = math::vec3{sc, -1.0f, -tc}; break;
        case CubeFace::PosZ: d = math::vec3{sc, -tc, 1.0f}; break;
        case CubeFace::NegZ: d = math::vec3{-sc, -tc, -1.0f}; break;
    }
    const float len = std::sqrt(glm::dot(d, d));
    return len > 0.0f ? d / len : d;
}

} // namespace maz::render
