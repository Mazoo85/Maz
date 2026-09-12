// tests/render/meshspherify.cpp — verifies the cast-to-sphere deformer (render::spherifyMesh). Ground truths:
// t=1 puts every vertex exactly on the sphere; t=0 is the identity; at a partial t each vertex's distance from the
// centre is lerp(originalDist, radius, t); a vertex at the centre stays put; the direction (ray) is preserved.
// Pure CPU, headless.
#include "maz/render/MeshSpherify.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }
static float dist(const MeshVertex& v, maz::math::vec3 c) {
    const float dx = v.px - c.x, dy = v.py - c.y, dz = v.pz - c.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// A cube from -1..+1 (8 corners), each at distance sqrt(3) from the origin.
static shapes::MeshData cube() {
    shapes::MeshData m;
    auto v = [](float x, float y, float z) { MeshVertex q{}; q.px = x; q.py = y; q.pz = z; q.r = q.g = q.b = 1; return q; };
    m.vertices = {v(-1,-1,-1), v(1,-1,-1), v(1,1,-1), v(-1,1,-1), v(-1,-1,1), v(1,-1,1), v(1,1,1), v(-1,1,1)};
    m.indices = {0, 1, 2, 4, 5, 6};
    return m;
}

int main() {
    const maz::math::vec3 O(0, 0, 0);
    const shapes::MeshData c = cube();
    const float root3 = std::sqrt(3.0f);

    // --- 1. t=1 puts every vertex exactly on the sphere of the given radius. ---
    {
        const shapes::MeshData s = spherifyMesh(c, /*radius=*/2.0f, /*t=*/1.0f);
        bool onSphere = true;
        for (const MeshVertex& v : s.vertices)
            if (!near(dist(v, O), 2.0f, 1e-4f)) onSphere = false;
        CHECK(onSphere, "t=1 casts every vertex onto the sphere (radius 2)");
    }

    // --- 2. t=0 is the identity. ---
    {
        const shapes::MeshData s = spherifyMesh(c, 2.0f, 0.0f);
        bool same = true;
        for (std::size_t i = 0; i < c.vertices.size(); ++i)
            if (!near(s.vertices[i].px, c.vertices[i].px, 1e-6f) ||
                !near(s.vertices[i].py, c.vertices[i].py, 1e-6f) ||
                !near(s.vertices[i].pz, c.vertices[i].pz, 1e-6f)) same = false;
        CHECK(same, "t=0 leaves the mesh unchanged");
    }

    // --- 3. At a partial t, each vertex's radius is lerp(originalDist, radius, t). ---
    {
        const float R = 1.0f, t = 0.5f;
        const shapes::MeshData s = spherifyMesh(c, R, t);
        const float expected = root3 + (R - root3) * t; // same for every cube corner
        bool ok = true;
        for (const MeshVertex& v : s.vertices)
            if (!near(dist(v, O), expected, 1e-4f)) ok = false;
        CHECK(ok, "at t=0.5 each corner's distance is the lerp between sqrt(3) and R");
    }

    // --- 4. The cast preserves each vertex's direction (ray from the centre). ---
    {
        const shapes::MeshData s = spherifyMesh(c, 5.0f, 1.0f);
        // Corner 6 is (1,1,1); on the sphere it should be (1,1,1)/sqrt(3)*5 — all components equal & positive.
        const MeshVertex& v = s.vertices[6];
        CHECK(v.px > 0 && near(v.px, v.py, 1e-4f) && near(v.py, v.pz, 1e-4f), "direction (equal +x,+y,+z) preserved");
    }

    // --- 5. A vertex at the centre has no cast direction and stays put. ---
    {
        shapes::MeshData m;
        MeshVertex q{}; q.px = q.py = q.pz = 0.0f; q.r = q.g = q.b = 1.0f;
        m.vertices = {q};
        const shapes::MeshData s = spherifyMesh(m, 3.0f, 1.0f);
        CHECK(near(s.vertices[0].px, 0.0f, 1e-6f) && near(s.vertices[0].py, 0.0f, 1e-6f), "centre vertex stays put");
    }

    // --- 6. An offset centre casts about that point. ---
    {
        const maz::math::vec3 C(10, 0, 0);
        shapes::MeshData m;
        auto v = [](float x, float y, float z) { MeshVertex q{}; q.px = x; q.py = y; q.pz = z; return q; };
        m.vertices = {v(12, 0, 0), v(10, 3, 0)}; // distances 2 and 3 from C
        const shapes::MeshData s = spherifyMesh(m, 1.0f, 1.0f, C);
        CHECK(near(dist(s.vertices[0], C), 1.0f, 1e-5f) && near(dist(s.vertices[1], C), 1.0f, 1e-5f),
              "vertices cast onto the sphere about the offset centre");
    }

    if (g_fail == 0) {
        std::printf("meshspherify: OK — t=1 lands on the sphere, t=0 identity, partial t lerps the radius.\n");
        return 0;
    }
    std::printf("meshspherify: %d failure(s).\n", g_fail);
    return 1;
}
