// tests/render/frustumcull.cpp — verifies the free-function frustum-culling primitives on FrustumPlanes
// (render::frustumIntersectsSphere / frustumContainsPoint), the sphere/point companions to the existing
// frustumIntersectsAabb. Camera: eye (0,0,5) looking at the origin, 60° vertical FOV, 16:9, near 0.1, far 100.
//   * a point dead-centre in front is inside; one behind the camera or far to the side is not;
//   * the three plane tests agree with each other at a point (point == zero-radius sphere == tiny AABB);
//   * a sphere fully in front is visible; one far behind is not;
//   * a sphere centred behind the camera becomes visible once its radius grows to reach the frustum (monotone);
//   * a huge sphere enclosing the camera is visible;
//   * the free functions match Camera3D::isPointVisible / isSphereVisible.
#include "maz/render/Camera3D.hpp"

#include <cstdio>
#include <initializer_list>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;
namespace math = maz::math;

int main() {
    Camera3D cam;
    cam.viewportWidth = 1280.0f;
    cam.viewportHeight = 720.0f;
    cam.lookAt(math::vec3(0, 0, 5), math::vec3(0, 0, 0), math::vec3(0, 1, 0));
    cam.perspective(60.0f * 3.14159265358979323846f / 180.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    const FrustumPlanes f = cam.frustum();

    // 1. Point containment: centre in front, behind camera, far to the side.
    CHECK(frustumContainsPoint(f, math::vec3(0, 0, 0)), "a point dead-centre in front is inside the frustum");
    CHECK(!frustumContainsPoint(f, math::vec3(0, 0, 10)), "a point behind the camera is outside");
    CHECK(!frustumContainsPoint(f, math::vec3(50, 0, 0)), "a point far to the side is outside");

    // 2. The three plane tests agree at a point: containsPoint == zero-radius sphere == tiny AABB, for points
    // that are not sitting exactly on a plane.
    {
        const math::vec3 pts[4] = {math::vec3(0, 0, 0), math::vec3(0, 0, 10), math::vec3(50, 0, 0),
                                   math::vec3(1, 0.5f, -3)};
        bool agree = true;
        for (const math::vec3& p : pts) {
            const bool cp = frustumContainsPoint(f, p);
            const bool sp = frustumIntersectsSphere(f, p, 0.0f);
            const bool bx = frustumIntersectsAabb(f, p - math::vec3(1e-4f), p + math::vec3(1e-4f));
            if (cp != sp || cp != bx) agree = false;
        }
        CHECK(agree, "point / zero-radius-sphere / tiny-AABB plane tests agree");
    }

    // 3. Sphere fully in front is visible; one far behind is not.
    CHECK(frustumIntersectsSphere(f, math::vec3(0, 0, 0), 1.0f), "a sphere in front is visible");
    CHECK(!frustumIntersectsSphere(f, math::vec3(0, 0, 60), 1.0f), "a sphere far behind the camera is culled");

    // 4. Monotonic in radius: a sphere centred behind the camera is culled when small but visible once large
    // enough to reach the frustum.
    CHECK(!frustumIntersectsSphere(f, math::vec3(0, 0, 10), 0.1f), "small sphere behind the camera is culled");
    CHECK(frustumIntersectsSphere(f, math::vec3(0, 0, 10), 20.0f), "large sphere behind reaches the frustum");

    // 5. A huge sphere enclosing the whole camera is visible.
    CHECK(frustumIntersectsSphere(f, math::vec3(0, 0, 0), 1000.0f), "a huge enclosing sphere is visible");

    // 6. The free functions match the Camera3D member methods across a spread of inputs.
    {
        const math::vec3 samples[6] = {math::vec3(0, 0, 0),  math::vec3(0, 0, 10), math::vec3(3, 2, -4),
                                       math::vec3(-2, 1, -8), math::vec3(50, 0, 0), math::vec3(0, -30, -5)};
        bool ptOk = true, spOk = true;
        for (const math::vec3& p : samples) {
            if (frustumContainsPoint(f, p) != cam.isPointVisible(p)) ptOk = false;
            for (float r : {0.0f, 0.5f, 5.0f}) {
                if (frustumIntersectsSphere(f, p, r) != cam.isSphereVisible(p, r)) spOk = false;
            }
        }
        CHECK(ptOk, "frustumContainsPoint matches Camera3D::isPointVisible");
        CHECK(spOk, "frustumIntersectsSphere matches Camera3D::isSphereVisible");
    }

    if (g_fail == 0) {
        std::printf("frustumcull: OK — sphere/point frustum culling correct and consistent with AABB + camera.\n");
        return 0;
    }
    std::printf("frustumcull: %d failure(s).\n", g_fail);
    return 1;
}
