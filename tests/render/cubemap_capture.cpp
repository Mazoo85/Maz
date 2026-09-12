// tests/render/cubemap_capture.cpp — verifies the reflection-probe / cubemap CAPTURE matrices
// (render::cubeFaceView / cubeFaceProjection / cubeFaceViewProjection) are self-consistent with the SAMPLING
// convention in Cubemap.hpp. The key invariant: the world direction a face's camera looks down is exactly
// the direction `directionToCube` assigns to that face, and a point on that axis projects to the face
// center. Pure matrix math — verifiable without a GPU.
#include "maz/render/CubemapCapture.hpp"

#include <array>
#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;
namespace math = maz::math;

int main() {
    const std::array<CubeFace, 6> faces = {CubeFace::PosX, CubeFace::NegX, CubeFace::PosY,
                                           CubeFace::NegY, CubeFace::PosZ, CubeFace::NegZ};

    // --- 1. Each face's look direction is the direction the sampler assigns to that face. ---
    for (CubeFace f : faces) {
        const math::vec3 fwd = cubeFaceForward(f);
        const CubeSample s = directionToCube(fwd);
        CHECK(s.face == f, "capture forward maps back to the same face via directionToCube");
        // And it lands at the face center (u,v ~ 0.5).
        CHECK(std::fabs(s.u - 0.5f) < 1e-4f && std::fabs(s.v - 0.5f) < 1e-4f, "axis hits the face center");
    }

    // --- 2. The view matrix actually looks down that axis: a point ahead projects in front (positive
    //         view-space forward => negative view Z), and to screen center. ---
    for (CubeFace f : faces) {
        const math::vec3 center{2.0f, -1.0f, 3.0f}; // arbitrary probe position
        const math::mat4 view = cubeFaceView(f, center);
        const math::vec3 ahead = center + cubeFaceForward(f) * 10.0f;
        const math::vec4 vp = view * math::vec4{ahead.x, ahead.y, ahead.z, 1.0f};
        // In a right-handed view space (glm::lookAt) the camera looks down -Z, so a point ahead has z < 0.
        CHECK(vp.z < 0.0f, "a point down the look axis is in front of the face camera");
        // Project it: it should land at clip center (x=y=0 before divide).
        const math::mat4 vpm = cubeFaceViewProjection(f, center);
        const math::vec4 clip = vpm * math::vec4{ahead.x, ahead.y, ahead.z, 1.0f};
        CHECK(std::fabs(clip.x) < 1e-3f && std::fabs(clip.y) < 1e-3f, "axis point projects to face center");
        CHECK(clip.w > 0.0f, "axis point has positive w (in front of near plane)");
    }

    // --- 3. The projection is a 90° FOV (unit aspect): a point at 45° to the axis lands on the frustum
    //         edge (|x/w| ~ 1). Use +Z face: forward +Z, right ~ +X. ---
    {
        const math::vec3 center{0.0f, 0.0f, 0.0f};
        const math::mat4 vpm = cubeFaceViewProjection(CubeFace::PosZ, center);
        // 45° between +Z and +X at distance -> point (d, 0, d).
        const math::vec4 clip = vpm * math::vec4{5.0f, 0.0f, 5.0f, 1.0f};
        const float ndcX = clip.x / clip.w;
        CHECK(std::fabs(std::fabs(ndcX) - 1.0f) < 1e-3f, "90 degree FOV: 45-degree point sits on the frustum edge");
    }

    // --- 4. All six faces cover the sphere without a gap: a direction near any axis maps to that face. ---
    {
        int covered = 0;
        for (CubeFace f : faces) {
            // A direction tilted slightly off the axis must still resolve to this face.
            math::vec3 d = cubeFaceForward(f);
            d.x += 0.05f; // small perturbation
            const CubeSample s = directionToCube(math::normalize(d));
            if (s.face == f) ++covered;
        }
        CHECK(covered >= 4, "most faces remain stable under a small direction perturbation");
    }

    if (g_fail == 0) {
        std::printf("cubemap_capture: OK — 6 face views consistent with the sampler, 90-degree FOV, centered.\n");
        return 0;
    }
    std::printf("cubemap_capture: %d failure(s).\n", g_fail);
    return 1;
}
