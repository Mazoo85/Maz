// tests/render/meshprojectedarea.cpp — verifies frontal/projected area (render::projectedArea). Ground truths: a
// cube of side s viewed along a face axis projects to s*s; the value is symmetric in the view direction (closed
// mesh); viewed corner-on the cube projects to its hexagonal silhouette (s^2*sqrt(3)); a sphere projects to ~pi*r^2;
// direction length does not matter; an empty mesh gives 0. Pure CPU, headless.
#include "maz/render/MeshProjectedArea.hpp"
#include "maz/render/MeshIcosphere.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool rel(float a, float b, float tolFrac) { return std::fabs(a - b) <= tolFrac * std::fabs(b) + 1e-4f; }

// Watertight cube of half-extent h (side 2h).
static shapes::MeshData cube(float h) {
    shapes::MeshData m;
    auto v = [](float x, float y, float z) { MeshVertex p{}; p.px = x; p.py = y; p.pz = z; p.r = p.g = p.b = 1; return p; };
    m.vertices = {v(-h,-h,-h), v(h,-h,-h), v(h,-h,h), v(-h,-h,h), v(-h,h,-h), v(h,h,-h), v(h,h,h), v(-h,h,h)};
    m.indices = {0,1,2, 0,2,3,  4,6,5, 4,7,6,  3,2,6, 3,6,7,  1,0,4, 1,4,5,  2,1,5, 2,5,6,  0,3,7, 0,7,4};
    return m;
}

int main() {
    const float h = 1.5f;          // side s = 3
    const float s = 2.0f * h;
    const shapes::MeshData c = cube(h);

    // --- 1. Face-on: projected area is one face, s*s. ---
    {
        CHECK(rel(projectedArea(c, maz::math::vec3(0, 0, 1)), s * s, 1e-4f), "cube face-on projects to s*s (+Z)");
        CHECK(rel(projectedArea(c, maz::math::vec3(1, 0, 0)), s * s, 1e-4f), "cube face-on projects to s*s (+X)");
        CHECK(rel(projectedArea(c, maz::math::vec3(0, 1, 0)), s * s, 1e-4f), "cube face-on projects to s*s (+Y)");
    }

    // --- 2. Symmetric in the view direction (closed mesh). ---
    {
        CHECK(rel(projectedArea(c, maz::math::vec3(0, 0, 1)), projectedArea(c, maz::math::vec3(0, 0, -1)), 1e-5f),
              "frontal area is the same from front and back");
    }

    // --- 3. Direction length is irrelevant (it is normalised). ---
    {
        CHECK(rel(projectedArea(c, maz::math::vec3(0, 0, 5)), s * s, 1e-4f), "a non-unit direction gives the same area");
    }

    // --- 4. Corner-on: the cube's hexagonal silhouette has area s^2 * sqrt(3). ---
    {
        const float expected = s * s * std::sqrt(3.0f);
        CHECK(rel(projectedArea(c, maz::math::vec3(1, 1, 1)), expected, 1e-3f), "cube corner-on projects to s^2*sqrt(3)");
    }

    // --- 5. A sphere projects to ~pi*r^2 regardless of direction. ---
    {
        const float r = 2.0f;
        const shapes::MeshData sph = makeIcosphere(r, 4); // fine enough to approximate the disc area
        const float disc = 3.14159265f * r * r;
        CHECK(rel(projectedArea(sph, maz::math::vec3(0, 0, 1)), disc, 0.02f), "sphere projects to ~pi*r^2 (+Z)");
        CHECK(rel(projectedArea(sph, maz::math::vec3(1, 2, 3)), disc, 0.02f), "sphere projection is direction-independent");
    }

    // --- 6. Empty mesh / zero direction -> 0. ---
    {
        CHECK(projectedArea(shapes::MeshData{}, maz::math::vec3(0, 0, 1)) == 0.0f, "empty mesh -> 0");
        CHECK(projectedArea(c, maz::math::vec3(0, 0, 0)) == 0.0f, "zero direction -> 0");
    }

    if (g_fail == 0) {
        std::printf("meshprojectedarea: OK — cube face s^2, symmetric, corner hexagon, sphere pi*r^2, safe.\n");
        return 0;
    }
    std::printf("meshprojectedarea: %d failure(s).\n", g_fail);
    return 1;
}
