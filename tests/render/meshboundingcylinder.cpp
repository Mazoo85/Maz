// tests/render/meshboundingcylinder.cpp — verifies the PCA bounding-cylinder fit (render::fitBoundingCylinder).
// Ground truths: for a bar the length axis aligns with the bar's long direction, height matches its extent along
// that axis, radius is the farthest perpendicular distance (contains every vertex), and the centre sits at the
// bar's middle; the fit works whichever world axis the bar runs along; too-few-vertices returns invalid.
// Pure CPU, headless.
#include "maz/render/MeshBoundingCylinder.hpp"

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

// A bar: 8 corners of a box that is long along `axis` (half-length 4) and thin (half-width 0.5) on the other two.
static shapes::MeshData bar(int axis) {
    shapes::MeshData m;
    const float L = 4.0f, W = 0.5f;
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2)
            for (int sz = -1; sz <= 1; sz += 2) {
                float x = (axis == 0 ? L : W) * static_cast<float>(sx);
                float y = (axis == 1 ? L : W) * static_cast<float>(sy);
                float z = (axis == 2 ? L : W) * static_cast<float>(sz);
                m.vertices.push_back(at(x, y, z));
            }
    m.indices = {0, 1, 2};
    return m;
}

int main() {
    // --- 1. A Y-long bar: axis ~ (0,±1,0), height 8, radius sqrt(0.5), centre at origin. ---
    {
        const BoundingCylinder c = fitBoundingCylinder(bar(1));
        CHECK(c.valid, "the fit succeeds");
        CHECK(std::fabs(c.axis.y) > 0.99f && near(c.axis.x, 0.0f, 1e-3f) && near(c.axis.z, 0.0f, 1e-3f),
              "the length axis aligns with +/-Y");
        CHECK(near(c.height, 8.0f, 1e-3f), "height matches the bar's Y extent (8)");
        CHECK(near(c.radius, std::sqrt(0.5f), 1e-3f), "radius is the farthest corner's perpendicular distance");
        CHECK(near(c.centre.x, 0.0f, 1e-3f) && near(c.centre.y, 0.0f, 1e-3f) && near(c.centre.z, 0.0f, 1e-3f),
              "the centre sits at the bar's middle (origin)");
    }

    // --- 2. The radius contains every vertex (no vertex is farther from the axis than radius). ---
    {
        const shapes::MeshData m = bar(1);
        const BoundingCylinder c = fitBoundingCylinder(m);
        bool contained = true;
        for (const MeshVertex& v : m.vertices) {
            const maz::math::vec3 d(v.px - c.centre.x, v.py - c.centre.y, v.pz - c.centre.z);
            const float t = d.x * c.axis.x + d.y * c.axis.y + d.z * c.axis.z;
            const maz::math::vec3 perp(d.x - c.axis.x * t, d.y - c.axis.y * t, d.z - c.axis.z * t);
            const float r = std::sqrt(perp.x * perp.x + perp.y * perp.y + perp.z * perp.z);
            if (r > c.radius + 1e-4f) contained = false;
            if (std::fabs(t) > c.height * 0.5f + 1e-4f) contained = false;
        }
        CHECK(contained, "every vertex fits inside the cylinder (radius + half-height)");
    }

    // --- 3. Works for a bar along X too. ---
    {
        const BoundingCylinder c = fitBoundingCylinder(bar(0));
        CHECK(std::fabs(c.axis.x) > 0.99f, "an X-long bar gives an X axis");
        CHECK(near(c.height, 8.0f, 1e-3f), "height still 8");
    }

    // --- 4. And along Z. ---
    {
        const BoundingCylinder c = fitBoundingCylinder(bar(2));
        CHECK(std::fabs(c.axis.z) > 0.99f, "a Z-long bar gives a Z axis");
    }

    // --- 5. Fewer than two vertices -> invalid. ---
    {
        shapes::MeshData one; one.vertices = {at(0, 0, 0)};
        CHECK(!fitBoundingCylinder(one).valid, "a single vertex cannot form a cylinder");
        CHECK(!fitBoundingCylinder(shapes::MeshData{}).valid, "empty -> invalid");
    }

    if (g_fail == 0) {
        std::printf("meshboundingcylinder: OK — PCA axis, height/radius contain all verts, centre at the middle.\n");
        return 0;
    }
    std::printf("meshboundingcylinder: %d failure(s).\n", g_fail);
    return 1;
}
