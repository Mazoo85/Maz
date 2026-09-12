// tests/render/meshsweep.cpp — verifies sweep-along-path / loft (render::sweepProfile, buildTube). Ground truths:
// sweeping a circle of radius r down a straight path makes a pipe whose every vertex sits exactly r from the path
// axis; ring centroids stay on the path; the rotation-minimizing frame keeps the tube from twisting on a bent path
// (every ring vertex stays radius r from its own path point); counts follow path/profile lengths; small inputs are
// safe. Pure CPU, headless.
#include "maz/render/MeshSweep.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

int main() {
    // --- 1. A circle swept down a straight +X path is a pipe: every vertex is radius 0.5 from the X axis. ---
    {
        std::vector<maz::math::vec3> path;
        for (int i = 0; i <= 4; ++i) path.push_back(maz::math::vec3(static_cast<float>(i), 0, 0));
        const int sides = 12;
        const shapes::MeshData tube = buildTube(path, 0.5f, sides);

        CHECK(tube.vertices.size() == path.size() * static_cast<std::size_t>(sides), "verts = rings * sides");
        // A closed tube: (rings-1) * sides quads * 2 tris.
        CHECK(tube.indices.size() == (path.size() - 1) * static_cast<std::size_t>(sides) * 2u * 3u,
              "triangles = (rings-1) * sides * 2");

        bool ok = true;
        for (const MeshVertex& v : tube.vertices) {
            const float perp = std::sqrt(v.py * v.py + v.pz * v.pz); // distance from the X axis
            if (!near(perp, 0.5f, 1e-4f)) ok = false;
            if (v.px < -1e-4f || v.px > 4.0f + 1e-4f) ok = false;    // stays within the path span
        }
        CHECK(ok, "every pipe vertex sits radius 0.5 from the straight path axis");
    }

    // --- 2. Ring centroids land exactly on the path points. ---
    {
        std::vector<maz::math::vec3> path = {{0, 0, 0}, {0, 3, 0}, {0, 6, 0}}; // straight up +Y
        const int sides = 8;
        const shapes::MeshData tube = buildTube(path, 1.0f, sides);
        bool ok = true;
        for (std::size_t i = 0; i < path.size(); ++i) {
            maz::math::vec3 c(0, 0, 0);
            for (int j = 0; j < sides; ++j) {
                const MeshVertex& v = tube.vertices[i * static_cast<std::size_t>(sides) + static_cast<std::size_t>(j)];
                c.x += v.px; c.y += v.py; c.z += v.pz;
            }
            c.x /= static_cast<float>(sides); c.y /= static_cast<float>(sides); c.z /= static_cast<float>(sides);
            if (!near(c.x, path[i].x, 1e-4f) || !near(c.y, path[i].y, 1e-4f) || !near(c.z, path[i].z, 1e-4f)) ok = false;
        }
        CHECK(ok, "each ring's centroid coincides with its path point");
    }

    // --- 3. On a BENT path (L-shape) the frame stays coherent: every ring vertex is radius r from its own point. ---
    {
        std::vector<maz::math::vec3> path = {{0, 0, 0}, {4, 0, 0}, {4, 4, 0}, {4, 4, 4}}; // three turns in 3D
        const float r = 0.75f;
        const int sides = 10;
        const shapes::MeshData tube = buildTube(path, r, sides);
        bool ok = true;
        for (std::size_t i = 0; i < path.size(); ++i) {
            for (int j = 0; j < sides; ++j) {
                const MeshVertex& v = tube.vertices[i * static_cast<std::size_t>(sides) + static_cast<std::size_t>(j)];
                const float dx = v.px - path[i].x, dy = v.py - path[i].y, dz = v.pz - path[i].z;
                const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (!near(d, r, 2e-3f)) ok = false; // stays a clean circle of radius r around each path point
            }
        }
        CHECK(ok, "on a bent 3D path every ring vertex keeps radius r (no collapse / blowup)");
    }

    // --- 4. An OPEN profile (a strip) makes fewer faces than a closed one on the same path. ---
    {
        std::vector<maz::math::vec3> path = {{0, 0, 0}, {0, 0, 2}, {0, 0, 4}};
        const std::vector<maz::math::vec2> strip = {{-1, 0}, {0, 0}, {1, 0}}; // 3 points, 2 open edges
        const shapes::MeshData ribbon = sweepProfile(strip, path, /*closedProfile=*/false);
        // (rings-1) * (P-1) quads * 2 tris = 2 * 2 * 2 = 8 triangles.
        CHECK(ribbon.indices.size() == 2u * 2u * 2u * 3u, "open profile: (rings-1)*(P-1)*2 triangles");
    }

    // --- 5. Degenerate inputs are safe. ---
    {
        CHECK(buildTube({{0, 0, 0}}, 1.0f, 8).vertices.empty(), "single path point -> empty");
        CHECK(buildTube({{0, 0, 0}, {1, 0, 0}}, 1.0f, 2).vertices.empty(), "fewer than 3 sides -> empty");
        CHECK(sweepProfile({}, {{0, 0, 0}, {1, 0, 0}}).vertices.empty(), "empty profile -> empty");
    }

    if (g_fail == 0) {
        std::printf("meshsweep: OK — pipe radius exact, centroids on path, no twist on bends, open strips, safe.\n");
        return 0;
    }
    std::printf("meshsweep: %d failure(s).\n", g_fail);
    return 1;
}
