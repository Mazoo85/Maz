// tests/render/meshslice.cpp — verifies mesh plane-slice / cross-section (render::sliceMesh). Ground truths:
// a unit cube sliced through its middle gives one CLOSED square loop of perimeter 4 lying in the plane; a cube
// sliced above itself gives nothing; a unit sphere sliced off-centre gives one closed circular loop whose
// points sit at the right radius and whose perimeter approaches 2*pi*r. Pure CPU, headless.
#include "maz/render/MeshSlice.hpp"

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

static shapes::MeshData sphere(float r, int rings, int sectors) {
    shapes::MeshData m;
    const double pi = 3.14159265358979323846;
    m.vertices.push_back(vtx(0, r, 0));
    for (int i = 1; i < rings; ++i) {
        const double th = pi * i / rings, y = std::cos(th) * r, rr = std::sin(th) * r;
        for (int j = 0; j < sectors; ++j) {
            const double ph = 2.0 * pi * j / sectors;
            m.vertices.push_back(vtx(static_cast<float>(std::cos(ph) * rr), static_cast<float>(y),
                                     static_cast<float>(std::sin(ph) * rr)));
        }
    }
    const std::uint32_t bottom = static_cast<std::uint32_t>(m.vertices.size());
    m.vertices.push_back(vtx(0, -r, 0));
    auto rv = [&](int ring, int j) { return static_cast<std::uint32_t>(1 + (ring - 1) * sectors + (j % sectors)); };
    for (int j = 0; j < sectors; ++j) m.indices.insert(m.indices.end(), {0u, rv(1, j), rv(1, j + 1)});
    for (int i = 1; i < rings - 1; ++i)
        for (int j = 0; j < sectors; ++j)
            m.indices.insert(m.indices.end(), {rv(i, j), rv(i + 1, j), rv(i, j + 1),
                                               rv(i, j + 1), rv(i + 1, j), rv(i + 1, j + 1)});
    for (int j = 0; j < sectors; ++j) m.indices.insert(m.indices.end(), {bottom, rv(rings - 1, j + 1), rv(rings - 1, j)});
    return m;
}

// Total contour length across all loops (closed loops include their wrap-around edge).
static float perimeter(const SliceContour& c) {
    float total = 0.0f;
    auto d = [&](std::uint32_t a, std::uint32_t b) {
        const maz::math::vec3 pa = c.points[a], pb = c.points[b];
        const float dx = pa.x - pb.x, dy = pa.y - pb.y, dz = pa.z - pb.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    };
    for (std::size_t li = 0; li < c.loops.size(); ++li) {
        const auto& L = c.loops[li];
        for (std::size_t i = 0; i + 1 < L.size(); ++i) total += d(L[i], L[i + 1]);
        if (li < c.loopClosed.size() && c.loopClosed[li] && L.size() >= 2) total += d(L.back(), L.front());
    }
    return total;
}

int main() {
    // --- 1. Cube sliced at y=0.5: one closed square loop, perimeter 4, all points on the plane. ---
    {
        const SliceContour c = sliceMesh(unitCube(), maz::math::vec3(0, 1, 0), 0.5f);
        CHECK(c.closedLoopCount() == 1, "cube mid-slice is a single closed loop");
        CHECK(c.loops.size() == 1, "exactly one loop");
        CHECK(near(perimeter(c), 4.0f, 1e-3f), "the square cross-section has perimeter 4");
        bool onPlane = true;
        for (const auto& p : c.points) if (!near(p.y, 0.5f, 1e-5f)) onPlane = false;
        CHECK(onPlane, "every contour point lies on the y=0.5 plane");
    }

    // --- 2. Cube sliced above itself: nothing crosses. ---
    {
        const SliceContour c = sliceMesh(unitCube(), maz::math::vec3(0, 1, 0), 2.0f);
        CHECK(c.segmentCount == 0 && c.loops.empty(), "a plane clear of the cube produces no contour");
    }

    // --- 3. Cube sliced by a diagonal plane still yields one closed loop. ---
    {
        const SliceContour c = sliceMesh(unitCube(), maz::math::vec3(1, 1, 0), 1.0f);
        CHECK(c.closedLoopCount() == 1, "diagonal cut is a single closed loop");
        bool onPlane = true;
        for (const auto& p : c.points) if (!near(p.x + p.y, 1.0f, 1e-4f)) onPlane = false;
        CHECK(onPlane, "diagonal contour points satisfy x+y=1");
    }

    // --- 4. Sphere sliced off-centre: one closed loop, correct radius, perimeter ~ 2*pi*r_slice. ---
    {
        const float pi = 3.14159265358979323846f;
        const float y = 0.3f;
        const float rSlice = std::sqrt(1.0f - y * y); // circle radius at height y on the unit sphere
        const SliceContour c = sliceMesh(sphere(1.0f, 24, 24), maz::math::vec3(0, 1, 0), y);
        CHECK(c.closedLoopCount() == 1, "sphere slice is a single closed ring");
        bool onCircle = true;
        for (const auto& p : c.points) {
            if (!near(p.y, y, 1e-4f)) onCircle = false;
            if (!near(std::sqrt(p.x * p.x + p.z * p.z), rSlice, 0.02f)) onCircle = false;
        }
        CHECK(onCircle, "all points sit at the slice height and radius");
        CHECK(near(perimeter(c), 2.0f * pi * rSlice, 0.1f), "ring perimeter approaches the circle circumference");
    }

    // --- 5. Empty mesh is safe. ---
    {
        const SliceContour c = sliceMesh(shapes::MeshData{}, maz::math::vec3(0, 1, 0), 0.0f);
        CHECK(c.points.empty() && c.loops.empty() && c.segmentCount == 0, "empty mesh -> empty contour");
    }

    if (g_fail == 0) {
        std::printf("meshslice: OK — cube->square loop (perim 4), diagonal loop, sphere->circle, clear plane empty.\n");
        return 0;
    }
    std::printf("meshslice: %d failure(s).\n", g_fail);
    return 1;
}
