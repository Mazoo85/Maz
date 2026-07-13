// Unit tests for maz::math::Frustum: plane extraction from a view-projection
// matrix and containment/intersection tests. Pure math, no GPU required.

#include "maz/math/Frustum.hpp"
#include "maz/scene/Camera.hpp"

#include <cmath>
#include <cstdio>

using namespace maz;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

bool approx(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) < eps;
}

} // namespace

int main() {
    using math::Aabb;
    using math::Frustum;
    using math::vec3;

    // Camera at (0,0,5) looking at the origin, +Y up, 60-degree fov, square aspect.
    scene::Camera cam;
    cam.setPosition(vec3(0.0f, 0.0f, 5.0f));
    cam.setTarget(vec3(0.0f, 0.0f, 0.0f));
    cam.setUp(vec3(0.0f, 1.0f, 0.0f));
    cam.setPerspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);

    Frustum f = Frustum::fromViewProj(cam.viewProjection());

    // --- plane normals are unit length -------------------------------------
    for (int k = 0; k < 6; ++k) {
        check(approx(glm::length(f.planes[k].normal), 1.0f), "frustum plane normal is unit length");
    }

    // --- center point / small AABB -----------------------------------------
    check(f.contains(vec3(0.0f, 0.0f, 0.0f)), "center point is contained");
    Aabb centerBox{vec3(-0.5f, -0.5f, -0.5f), vec3(0.5f, 0.5f, 0.5f)};
    check(f.intersects(centerBox), "small AABB at center intersects");

    // --- a point outside each of the six planes ----------------------------
    check(!f.contains(vec3(-100.0f, 0.0f, 0.0f)), "point far left is not contained");
    check(!f.contains(vec3(100.0f, 0.0f, 0.0f)), "point far right is not contained");
    check(!f.contains(vec3(0.0f, -100.0f, 0.0f)), "point far below is not contained");
    check(!f.contains(vec3(0.0f, 100.0f, 0.0f)), "point far above is not contained");
    // In front of the near plane (camera at z=5, near at z=4.9): validates [0,1] depth.
    // z=4.92 sits in the gap between the real near plane (z=4.90) and where the WRONG
    // OpenGL-style r3+r2 near plane would land (~z=4.95), so this assert fails iff the
    // near-plane extraction uses the wrong depth convention. See review.
    check(!f.contains(vec3(0.0f, 0.0f, 4.92f)), "point in front of near is not contained");
    check(!f.contains(vec3(0.0f, 0.0f, -200.0f)), "point beyond far is not contained");

    // --- AABB straddling the left plane ------------------------------------
    Aabb straddleLeft{vec3(-100.0f, -1.0f, -1.0f), vec3(0.0f, 1.0f, 1.0f)};
    check(f.intersects(straddleLeft), "AABB straddling left plane intersects");

    // --- AABB fully to the left --------------------------------------------
    Aabb fullyLeft{vec3(-200.0f, -1.0f, -1.0f), vec3(-150.0f, 1.0f, 1.0f)};
    check(!f.intersects(fullyLeft), "AABB fully left does not intersect");

    // --- spheres -----------------------------------------------------------
    check(f.intersectsSphere(vec3(0.0f, 0.0f, 0.0f), 1.0f), "sphere at center intersects");
    check(!f.intersectsSphere(vec3(1000.0f, 0.0f, 0.0f), 1.0f), "distant sphere does not intersect");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
