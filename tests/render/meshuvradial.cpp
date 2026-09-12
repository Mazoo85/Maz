// tests/render/meshuvradial.cpp — verifies spherical & cylindrical UV projection (render::sphericalUv /
// render::cylindricalUv). Ground truths of the lat-long layout: +X maps to u=0.5,v=0.5; the +Y pole to v=0 and
// the −Y pole to v=1; +Z to u=0.75 on the equator. Cylindrical about Y: angle 0 (+X) maps to u=0.5 and height to
// v. Topology is preserved. Pure CPU, headless.
#include "maz/render/MeshUvRadial.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}
static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

int main() {
    // --- 1. Spherical: cardinal directions land at the expected lat-long coordinates. ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(1,0,0), vtx(0,1,0), vtx(0,-1,0), vtx(0,0,1), vtx(-1,0,0)};
        m.indices = {0,1,2}; // topology irrelevant to per-vertex projection
        const shapes::MeshData r = sphericalUv(m);
        CHECK(r.vertices.size() == m.vertices.size() && r.indices == m.indices, "spherical keeps topology");
        CHECK(near(r.vertices[0].u, 0.5f, 1e-5f) && near(r.vertices[0].v, 0.5f, 1e-5f), "+X -> (0.5, 0.5)");
        CHECK(near(r.vertices[1].v, 0.0f, 1e-5f), "+Y pole -> v=0");
        CHECK(near(r.vertices[2].v, 1.0f, 1e-5f), "-Y pole -> v=1");
        CHECK(near(r.vertices[3].u, 0.75f, 1e-5f) && near(r.vertices[3].v, 0.5f, 1e-5f), "+Z equator -> (0.75, 0.5)");
        CHECK(near(r.vertices[4].u, 1.0f, 1e-5f) && near(r.vertices[4].v, 0.5f, 1e-5f), "-X -> (1.0, 0.5)");
    }

    // --- 2. Spherical is scale-invariant (direction only): a radius-5 point matches its unit direction. ---
    {
        shapes::MeshData a;
        a.vertices = {vtx(0,0,1)};
        a.indices = {};
        shapes::MeshData b;
        b.vertices = {vtx(0,0,5)};
        b.indices = {};
        const shapes::MeshData ra = sphericalUv(a);
        const shapes::MeshData rb = sphericalUv(b);
        CHECK(near(ra.vertices[0].u, rb.vertices[0].u, 1e-5f) && near(ra.vertices[0].v, rb.vertices[0].v, 1e-5f),
              "spherical depends on direction, not distance");
    }

    // --- 3. Cylindrical about Y: angle 0 (+X) -> u=0.5, height -> v; +Z -> u=0.75. ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(1,2,0), vtx(0,5,1), vtx(-1,0,0)};
        m.indices = {};
        const shapes::MeshData r = cylindricalUv(m, 1, maz::math::vec3(0,0,0), 1.0f, 1.0f);
        CHECK(near(r.vertices[0].u, 0.5f, 1e-5f) && near(r.vertices[0].v, 2.0f, 1e-5f), "+X at height 2 -> (0.5, 2)");
        CHECK(near(r.vertices[1].u, 0.75f, 1e-5f) && near(r.vertices[1].v, 5.0f, 1e-5f), "+Z at height 5 -> (0.75, 5)");
        CHECK(near(r.vertices[2].u, 1.0f, 1e-5f), "-X -> u=1.0");
    }

    // --- 4. Cylindrical vScale/vOffset scale the height axis. ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(1,4,0)};
        m.indices = {};
        const shapes::MeshData r = cylindricalUv(m, 1, maz::math::vec3(0,0,0), 1.0f, 0.25f, 0.1f);
        CHECK(near(r.vertices[0].v, 4.0f * 0.25f + 0.1f, 1e-5f), "v = height*0.25 + 0.1");
    }

    // --- 5. Empty mesh is safe. ---
    {
        const shapes::MeshData s = sphericalUv(shapes::MeshData{});
        const shapes::MeshData c = cylindricalUv(shapes::MeshData{});
        CHECK(s.vertices.empty() && c.vertices.empty(), "empty mesh -> empty");
    }

    if (g_fail == 0) {
        std::printf("meshuvradial: OK — spherical cardinals + poles, scale-invariant, cylindrical angle/height.\n");
        return 0;
    }
    std::printf("meshuvradial: %d failure(s).\n", g_fail);
    return 1;
}
