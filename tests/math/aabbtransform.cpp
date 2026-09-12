// tests/math/aabbtransform.cpp — verifies transforming an AABB by a matrix into a tight enclosing AABB
// (math::Aabb3::transformed, Arvo's method). Ground truths: identity leaves the box unchanged; translation
// shifts it; non-uniform scale scales the extents; a 90-degree rotation swaps the corresponding extents
// exactly; a 45-degree rotation of a unit cube grows its in-plane extents to sqrt(2); a rotation about the
// box centre keeps the centre fixed. Checked against hand-computed boxes. Pure CPU, deterministic.
#include "maz/math/Geometry3D.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::vec3;
using maz::math::vec4;
using maz::math::mat4;
using maz::math::Aabb3;

static bool nearv(const vec3& a, const vec3& b, float e = 1e-4f) {
    return std::fabs(a.x - b.x) <= e && std::fabs(a.y - b.y) <= e && std::fabs(a.z - b.z) <= e;
}

int main() {
    const Aabb3 box(vec3(-1, -2, -3), vec3(1, 2, 3)); // centre 0, half-extents (1,2,3)

    // --- 1. Identity leaves the box unchanged. ---
    {
        const Aabb3 r = box.transformed(mat4(1.0f));
        CHECK(nearv(r.min, box.min) && nearv(r.max, box.max), "identity unchanged");
    }

    // --- 2. Pure translation shifts min and max. ---
    {
        mat4 m(1.0f);
        m[3] = vec4(10.0f, -5.0f, 2.0f, 1.0f); // translation column
        const Aabb3 r = box.transformed(m);
        CHECK(nearv(r.min, vec3(9, -7, -1)) && nearv(r.max, vec3(11, -3, 5)), "translation shifts box");
    }

    // --- 3. Non-uniform scale scales the half-extents. ---
    {
        mat4 m(1.0f);
        m[0][0] = 2.0f; m[1][1] = 3.0f; m[2][2] = 4.0f;
        const Aabb3 r = box.transformed(m);
        // half-extents (1,2,3) -> (2,6,12), centre still 0.
        CHECK(nearv(r.min, vec3(-2, -6, -12)) && nearv(r.max, vec3(2, 6, 12)), "scale scales extents");
    }

    // --- 4. 90-degree rotation about z swaps the x/y extents exactly. ---
    {
        const float a = 1.5707963267948966f; // pi/2
        const float c = std::cos(a), s = std::sin(a);
        mat4 m(1.0f);
        m[0][0] = c; m[0][1] = s;   // column 0 (image of local x) = (0,1,0)
        m[1][0] = -s; m[1][1] = c;  // column 1 (image of local y) = (-1,0,0)
        const Aabb3 r = box.transformed(m);
        // extents (1,2,3) -> (2,1,3); centre stays 0.
        CHECK(nearv(r.min, vec3(-2, -1, -3)) && nearv(r.max, vec3(2, 1, 3)), "90deg z-rotation swaps x/y extents");
    }

    // --- 5. 45-degree rotation of a unit cube grows the in-plane extents to sqrt(2). ---
    {
        const Aabb3 cube(vec3(-1, -1, -1), vec3(1, 1, 1));
        const float a = 0.7853981633974483f; // pi/4
        const float c = std::cos(a), s = std::sin(a);
        mat4 m(1.0f);
        m[0][0] = c; m[0][1] = s;
        m[1][0] = -s; m[1][1] = c;
        const Aabb3 r = cube.transformed(m);
        const float root2 = std::sqrt(2.0f);
        CHECK(nearv(r.min, vec3(-root2, -root2, -1)) && nearv(r.max, vec3(root2, root2, 1)),
              "45deg rotation grows in-plane extents to sqrt(2)");
    }

    // --- 6. Rotation about the box's own centre keeps the centre fixed. ---
    {
        const Aabb3 off(vec3(4, 4, 0), vec3(6, 8, 0)); // centre (5,6,0)
        const float a = 0.6f;
        const float c = std::cos(a), s = std::sin(a);
        // Rotate about the origin then translate so (5,6) maps back to itself.
        mat4 m(1.0f);
        m[0][0] = c; m[0][1] = s;
        m[1][0] = -s; m[1][1] = c;
        const vec3 centre(5, 6, 0);
        const vec3 rotated(m * vec4(centre, 1.0f));
        m[3] = vec4(centre.x - rotated.x, centre.y - rotated.y, 0.0f, 1.0f);
        const Aabb3 r = off.transformed(m);
        const vec3 rc = (r.min + r.max) * 0.5f;
        CHECK(nearv(rc, centre), "rotation about the box centre keeps the centre fixed");
    }

    if (g_fail == 0) {
        std::printf("aabb transform: all tests passed\n");
    }
    return g_fail == 0 ? 0 : 1;
}
