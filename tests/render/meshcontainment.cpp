// tests/render/meshcontainment.cpp — verifies point-in-mesh containment (render::containsPoint / containsPoints).
// Ground truth: a point is inside a closed cube iff all coordinates fall within its extent; inside a sphere iff
// it is nearer the centre than the radius. The ray-parity test must agree with those closed forms, reject
// points outside the bounding box, and handle the batch path. Pure CPU, headless.
#include "maz/render/MeshContainment.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}

// Cube spanning [0,1]^3, welded, outward-wound.
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

int main() {
    // --- 1. Cube: centre inside, corners/outside points outside. ---
    {
        const shapes::MeshData c = unitCube();
        CHECK(containsPoint(c, maz::math::vec3(0.5f, 0.5f, 0.5f)), "cube centre is inside");
        CHECK(containsPoint(c, maz::math::vec3(0.1f, 0.9f, 0.5f)), "a near-corner interior point is inside");
        CHECK(!containsPoint(c, maz::math::vec3(1.5f, 0.5f, 0.5f)), "a point beyond +X is outside");
        CHECK(!containsPoint(c, maz::math::vec3(-0.2f, 0.5f, 0.5f)), "a point beyond -X is outside");
        CHECK(!containsPoint(c, maz::math::vec3(0.5f, 0.5f, 5.0f)), "a far point is outside");
    }

    // --- 2. Batch matches the AABB predicate for a lattice of points inside/around the cube. ---
    {
        const shapes::MeshData c = unitCube();
        std::vector<maz::math::vec3> pts;
        for (int i = -1; i <= 11; ++i)
            for (int j = -1; j <= 11; ++j)
                pts.push_back(maz::math::vec3(static_cast<float>(i) * 0.1f, static_cast<float>(j) * 0.1f, 0.5f));
        const std::vector<std::uint8_t> in = containsPoints(c, pts);
        bool ok = true;
        for (std::size_t k = 0; k < pts.size(); ++k) {
            const maz::math::vec3& p = pts[k];
            // Skip points sitting on a face — exactly-on-boundary containment is ambiguous for ray parity.
            const bool nearFace = std::fabs(p.x) < 0.02f || std::fabs(p.x - 1.0f) < 0.02f
                                  || std::fabs(p.y) < 0.02f || std::fabs(p.y - 1.0f) < 0.02f;
            if (nearFace) continue;
            const bool truth = p.x > 0.0f && p.x < 1.0f && p.y > 0.0f && p.y < 1.0f;
            if (static_cast<bool>(in[k]) != truth) ok = false;
        }
        CHECK(ok, "batch containment matches the interior predicate across the lattice");
    }

    // --- 3. Sphere: agrees with the analytic radius test. ---
    {
        const shapes::MeshData s = sphere(1.0f, 24, 24);
        CHECK(containsPoint(s, maz::math::vec3(0, 0, 0)), "sphere centre is inside");
        CHECK(containsPoint(s, maz::math::vec3(0.7f, 0.0f, 0.0f)), "a point well within the radius is inside");
        CHECK(!containsPoint(s, maz::math::vec3(0.99f, 0.99f, 0.0f)), "a point outside the radius (r~1.4) is outside");
        CHECK(!containsPoint(s, maz::math::vec3(2.0f, 0.0f, 0.0f)), "a far point is outside");
        // Batch agreement with |p| < 0.95 (kept clear of the faceted surface).
        std::vector<maz::math::vec3> pts;
        for (int i = 0; i < 40; ++i) {
            const float t = static_cast<float>(i) / 40.0f * 1.6f;
            pts.push_back(maz::math::vec3(t, 0.0f, 0.0f));
        }
        const std::vector<std::uint8_t> in = containsPoints(s, pts);
        bool ok = true;
        for (std::size_t k = 0; k < pts.size(); ++k) {
            const float r = pts[k].x;
            if (r < 0.9f && !in[k]) ok = false;   // clearly inside must read inside
            if (r > 1.05f && in[k]) ok = false;   // clearly outside must read outside
        }
        CHECK(ok, "sphere batch agrees with the radius test away from the surface");
    }

    // --- 4. Empty mesh / empty points are safe. ---
    {
        CHECK(!containsPoint(shapes::MeshData{}, maz::math::vec3(0, 0, 0)), "empty mesh contains nothing");
        const std::vector<std::uint8_t> in = containsPoints(unitCube(), {});
        CHECK(in.empty(), "no points -> no results");
    }

    if (g_fail == 0) {
        std::printf("meshcontainment: OK — cube in/out + lattice batch, sphere radius agreement, empty safe.\n");
        return 0;
    }
    std::printf("meshcontainment: %d failure(s).\n", g_fail);
    return 1;
}
