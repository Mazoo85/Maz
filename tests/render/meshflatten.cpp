// tests/render/meshflatten.cpp — verifies flatten/project-to-plane (render::projectToPlane). Ground truths: t=1
// puts every vertex on the plane (signed distance 0); t=0 is the identity; a partial t removes that fraction of
// the distance along the normal; the in-plane coordinates don't drift; an offset plane and a tilted normal both
// work; a zero-length normal is a no-op. Pure CPU, headless.
#include "maz/render/MeshFlatten.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

static MeshVertex at(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}
static float planeDist(const MeshVertex& v, maz::math::vec3 pt, maz::math::vec3 n) {
    return (v.px - pt.x) * n.x + (v.py - pt.y) * n.y + (v.pz - pt.z) * n.z;
}

int main() {
    const maz::math::vec3 up(0, 1, 0), origin(0, 0, 0);

    shapes::MeshData m;
    m.vertices = {at(1, 3, 2), at(-2, -1, 0), at(0, 5, -4), at(3, 0, 1)};
    m.indices = {0, 1, 2};

    // --- 1. t=1 onto the ground plane (y=0): every vertex has y=0. ---
    {
        const shapes::MeshData f = projectToPlane(m, origin, up, 1.0f);
        bool flat = true;
        for (const MeshVertex& v : f.vertices)
            if (!near(v.py, 0.0f, 1e-5f)) flat = false;
        CHECK(flat, "t=1 lands every vertex on the y=0 plane");
        // in-plane coords (x,z) unchanged.
        CHECK(near(f.vertices[0].px, 1.0f, 1e-6f) && near(f.vertices[0].pz, 2.0f, 1e-6f), "x,z do not drift");
    }

    // --- 2. t=0 is the identity. ---
    {
        const shapes::MeshData f = projectToPlane(m, origin, up, 0.0f);
        bool same = true;
        for (std::size_t i = 0; i < m.vertices.size(); ++i)
            if (!near(f.vertices[i].py, m.vertices[i].py, 1e-6f)) same = false;
        CHECK(same, "t=0 leaves the mesh unchanged");
    }

    // --- 3. t=0.5 removes half the distance: (1,3,2) -> y=1.5. ---
    {
        const shapes::MeshData f = projectToPlane(m, origin, up, 0.5f);
        CHECK(near(f.vertices[0].py, 1.5f, 1e-5f), "half flatten halves the height");
        CHECK(near(f.vertices[1].py, -0.5f, 1e-5f), "a vertex below the plane also moves halfway toward it");
    }

    // --- 4. An offset plane (y=5): a vertex projects onto y=5. ---
    {
        const maz::math::vec3 hi(0, 5, 0);
        const shapes::MeshData f = projectToPlane(m, hi, up, 1.0f);
        for (const MeshVertex& v : f.vertices)
            CHECK(near(planeDist(v, hi, up), 0.0f, 1e-4f), "vertices land on the offset plane y=5");
    }

    // --- 5. A tilted plane (normal +Z, non-unit) flattens Z. ---
    {
        const shapes::MeshData f = projectToPlane(m, origin, maz::math::vec3(0, 0, 3), 1.0f); // non-unit normal
        bool flatZ = true;
        for (const MeshVertex& v : f.vertices)
            if (!near(v.pz, 0.0f, 1e-4f)) flatZ = false;
        CHECK(flatZ, "a +Z plane (non-unit normal) flattens onto z=0");
    }

    // --- 6. A zero-length normal is a no-op; empty is safe. ---
    {
        const shapes::MeshData f = projectToPlane(m, origin, maz::math::vec3(0, 0, 0), 1.0f);
        CHECK(near(f.vertices[0].py, 3.0f, 1e-6f), "a degenerate plane leaves the mesh alone");
        CHECK(projectToPlane(shapes::MeshData{}, origin, up, 1.0f).vertices.empty(), "empty -> empty");
    }

    if (g_fail == 0) {
        std::printf("meshflatten: OK — t=1 coplanar, t=0 identity, partial t removes the fraction along the normal.\n");
        return 0;
    }
    std::printf("meshflatten: %d failure(s).\n", g_fail);
    return 1;
}
