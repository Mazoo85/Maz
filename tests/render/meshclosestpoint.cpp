// tests/render/meshclosestpoint.cpp — verifies closest-point queries (render::closestPointOnMesh). Ground truths:
// for a point directly above a flat quad the nearest surface point is straight below it with distance = height; a
// point beyond the quad's edge clamps to the nearest edge/corner; for a cube the nearest point on the near face is
// directly out along that face and the reported distance matches; the returned normal is the hit face's facing;
// an empty mesh is invalid. Pure CPU, headless.
#include "maz/render/MeshClosestPoint.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

// A flat quad on the XZ plane (y=0) spanning [-2,2] in x and z, two triangles.
static shapes::MeshData quad() {
    shapes::MeshData m;
    auto v = [](float x, float z) { MeshVertex p{}; p.px = x; p.py = 0; p.pz = z; p.r = p.g = p.b = 1; return p; };
    m.vertices = {v(-2, -2), v(2, -2), v(2, 2), v(-2, 2)};
    m.indices = {0, 1, 2, 0, 2, 3};
    return m;
}

// Watertight cube of half-extent h.
static shapes::MeshData cube(float h) {
    shapes::MeshData m;
    auto v = [](float x, float y, float z) { MeshVertex p{}; p.px = x; p.py = y; p.pz = z; p.r = p.g = p.b = 1; return p; };
    m.vertices = {v(-h,-h,-h), v(h,-h,-h), v(h,-h,h), v(-h,-h,h), v(-h,h,-h), v(h,h,-h), v(h,h,h), v(-h,h,h)};
    m.indices = {0,1,2, 0,2,3,  4,6,5, 4,7,6,  3,2,6, 3,6,7,  1,0,4, 1,4,5,  2,1,5, 2,5,6,  0,3,7, 0,7,4};
    return m;
}

int main() {
    // --- 1. A point straight above the quad: nearest point is directly below, distance = height. ---
    {
        const shapes::MeshData q = quad();
        const ClosestPointResult r = closestPointOnMesh(q, maz::math::vec3(0.5f, 3.0f, -1.0f));
        CHECK(r.valid, "query returns a valid result");
        CHECK(near(r.point.x, 0.5f, 1e-4f) && near(r.point.y, 0.0f, 1e-4f) && near(r.point.z, -1.0f, 1e-4f),
              "nearest point on the quad is directly below the query");
        CHECK(near(r.distance, 3.0f, 1e-4f), "distance equals the height above the quad");
        CHECK(near(std::fabs(r.normal.y), 1.0f, 1e-4f), "hit-face normal is vertical (the quad faces +/-Y)");
    }

    // --- 2. A point beyond the quad's edge clamps to the boundary (x,z clamped into [-2,2]). ---
    {
        const shapes::MeshData q = quad();
        const ClosestPointResult r = closestPointOnMesh(q, maz::math::vec3(5.0f, 0.0f, 0.0f));
        CHECK(near(r.point.x, 2.0f, 1e-4f) && near(r.point.z, 0.0f, 1e-4f) && near(r.point.y, 0.0f, 1e-4f),
              "off-edge query snaps to the nearest quad boundary point");
        CHECK(near(r.distance, 3.0f, 1e-4f), "distance to the clamped edge is correct");
    }

    // --- 3. A point outside a cube's +X face maps onto that face; distance = gap to the face. ---
    {
        const shapes::MeshData c = cube(1.0f);
        const ClosestPointResult r = closestPointOnMesh(c, maz::math::vec3(4.0f, 0.2f, -0.3f));
        CHECK(near(r.point.x, 1.0f, 1e-4f), "nearest point sits on the +X face (x=1)");
        CHECK(near(r.point.y, 0.2f, 1e-4f) && near(r.point.z, -0.3f, 1e-4f), "y,z carry through onto the face");
        CHECK(near(r.distance, 3.0f, 1e-4f), "distance is the gap from x=4 to the x=1 face");
    }

    // --- 4. A point INSIDE the cube still returns the nearest surface (unsigned): the closest face. ---
    {
        const shapes::MeshData c = cube(1.0f);
        // Near the +X face from inside: closest surface is x=1, distance 0.1.
        const ClosestPointResult r = closestPointOnMesh(c, maz::math::vec3(0.9f, 0.0f, 0.0f));
        CHECK(near(r.distance, 0.1f, 1e-4f) && near(r.point.x, 1.0f, 1e-4f), "inside query returns nearest face (unsigned)");
    }

    // --- 5. Empty mesh -> invalid. ---
    {
        CHECK(!closestPointOnMesh(shapes::MeshData{}, maz::math::vec3(0, 0, 0)).valid, "empty mesh -> invalid");
    }

    if (g_fail == 0) {
        std::printf("meshclosestpoint: OK — above/off-edge quad, cube face, inside unsigned, safe.\n");
        return 0;
    }
    std::printf("meshclosestpoint: %d failure(s).\n", g_fail);
    return 1;
}
