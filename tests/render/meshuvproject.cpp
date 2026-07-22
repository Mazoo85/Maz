// tests/render/meshuvproject.cpp — verifies projection UV unwrap (render::planarUv / render::boxUv). Ground
// truths: a unit XZ plane projected top-down maps to the unit square [0,1]x[0,1]; box projection gives each cube
// face its own unit-square UVs, splits into 3 verts per triangle, and picks the face's dominant axis. Headless.
#include "maz/render/MeshUvProject.hpp"

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

static shapes::MeshData unitCube() {
    shapes::MeshData m;
    m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,1,0), vtx(0,1,0),
                  vtx(0,0,1), vtx(1,0,1), vtx(1,1,1), vtx(0,1,1)};
    m.indices = {0,2,1, 0,3,2,  4,5,6, 4,6,7,  0,1,5, 0,5,4,
                 3,7,6, 3,6,2,  0,4,7, 0,7,3,  1,2,6, 1,6,5};
    return m;
}

int main() {
    // --- 1. Planar top-down (Y axis) on a unit XZ quad maps to the unit square, topology unchanged. ---
    {
        shapes::MeshData q;
        q.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,0,1), vtx(0,0,1)};
        q.indices = {0,1,2, 0,2,3};
        const shapes::MeshData r = planarUv(q, 1, 1.0f); // axis Y: u=x, v=z
        CHECK(r.vertices.size() == 4 && r.indices == q.indices, "planar keeps the mesh topology");
        CHECK(near(r.vertices[0].u, 0.0f, 1e-6f) && near(r.vertices[0].v, 0.0f, 1e-6f), "corner (0,0,0) -> UV (0,0)");
        CHECK(near(r.vertices[1].u, 1.0f, 1e-6f) && near(r.vertices[1].v, 0.0f, 1e-6f), "corner (1,0,0) -> UV (1,0)");
        CHECK(near(r.vertices[2].u, 1.0f, 1e-6f) && near(r.vertices[2].v, 1.0f, 1e-6f), "corner (1,0,1) -> UV (1,1)");
        CHECK(near(r.vertices[3].u, 0.0f, 1e-6f) && near(r.vertices[3].v, 1.0f, 1e-6f), "corner (0,0,1) -> UV (0,1)");
    }

    // --- 2. Planar scale/offset: doubling the scale doubles the UV span. ---
    {
        shapes::MeshData q;
        q.vertices = {vtx(0,0,0), vtx(2,0,0), vtx(2,0,2), vtx(0,0,2)};
        q.indices = {0,1,2, 0,2,3};
        const shapes::MeshData r = planarUv(q, 1, 0.5f, 0.25f, 0.25f); // scale 0.5, offset 0.25
        CHECK(near(r.vertices[1].u, 1.25f, 1e-6f), "u = x*0.5 + 0.25 at x=2 is 1.25");
        CHECK(near(r.vertices[0].u, 0.25f, 1e-6f), "u offset applies at the origin");
    }

    // --- 3. Box projection: 3 verts per triangle, and each cube face's UVs fill the unit square. ---
    {
        const shapes::MeshData c = unitCube();
        const shapes::MeshData r = boxUv(c, 1.0f);
        CHECK(r.vertices.size() == (c.indices.size() / 3) * 3, "box projection facet-splits to 3 verts/triangle");
        CHECK(r.vertices.size() == 36, "cube -> 36 vertices");
        // Every UV lies within [0,1] (a unit cube with unit scale), and the full square is used.
        float umin = 1e30f, umax = -1e30f, vmin = 1e30f, vmax = -1e30f;
        for (const auto& v : r.vertices) {
            umin = std::min(umin, v.u);
            umax = std::max(umax, v.u);
            vmin = std::min(vmin, v.v);
            vmax = std::max(vmax, v.v);
        }
        CHECK(near(umin, 0.0f, 1e-5f) && near(umax, 1.0f, 1e-5f), "box U spans [0,1]");
        CHECK(near(vmin, 0.0f, 1e-5f) && near(vmax, 1.0f, 1e-5f), "box V spans [0,1]");
    }

    // --- 4. Box projection picks the dominant axis: the top face (normal ±Y) projects to the XZ plane. ---
    {
        shapes::MeshData top; // a single quad at y=1 facing up, positions spread in x and z
        top.vertices = {vtx(0,1,0), vtx(1,1,0), vtx(1,1,1), vtx(0,1,1)};
        top.indices = {0,1,2, 0,2,3};
        const shapes::MeshData r = boxUv(top, 1.0f);
        // With axis=Y, u = z (a2), v = x (a0=... ) — regardless of exact channel assignment, the projected
        // coordinates must vary across BOTH triangle spans (a face parallel to the projection plane isn't
        // collapsed). Check the UV bounding box is a full unit square, not a line.
        float uspan = 0.0f, vspan = 0.0f, u0 = r.vertices[0].u, v0 = r.vertices[0].v;
        for (const auto& v : r.vertices) {
            uspan = std::max(uspan, std::fabs(v.u - u0));
            vspan = std::max(vspan, std::fabs(v.v - v0));
        }
        CHECK(near(uspan, 1.0f, 1e-5f) && near(vspan, 1.0f, 1e-5f), "top face projects to a full unit square (not collapsed)");
    }

    // --- 5. Empty mesh is safe. ---
    {
        const shapes::MeshData p = planarUv(shapes::MeshData{}, 0, 1.0f);
        const shapes::MeshData b = boxUv(shapes::MeshData{}, 1.0f);
        CHECK(p.vertices.empty() && b.vertices.empty(), "empty mesh -> empty");
    }

    if (g_fail == 0) {
        std::printf("meshuvproject: OK — planar quad->unit square, scale/offset, box cube->36 verts + [0,1] UVs.\n");
        return 0;
    }
    std::printf("meshuvproject: %d failure(s).\n", g_fail);
    return 1;
}
