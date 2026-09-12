// tests/render/meshstats.cpp — verifies mesh statistics (render::analyzeMesh). Ground truths: a unit cube has
// bounds [0,1]^3, surface area 6, centroid at its centre, 18 distinct edges (12 of length 1, 6 diagonals of
// length sqrt(2)); a single triangle has a known area and centroid; translating/scaling shifts the bounds and
// centroid accordingly. Pure CPU, headless.
#include "maz/render/MeshStats.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}
static bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

static shapes::MeshData cube(float o, float s) { // origin-corner o, edge length s
    shapes::MeshData m;
    m.vertices = {vtx(o,o,o), vtx(o+s,o,o), vtx(o+s,o+s,o), vtx(o,o+s,o),
                  vtx(o,o,o+s), vtx(o+s,o,o+s), vtx(o+s,o+s,o+s), vtx(o,o+s,o+s)};
    m.indices = {0,2,1, 0,3,2,  4,5,6, 4,6,7,  0,1,5, 0,5,4,
                 3,7,6, 3,6,2,  0,4,7, 0,7,3,  1,2,6, 1,6,5};
    return m;
}

int main() {
    // --- 1. Unit cube: bounds, area, centroid, edge distribution. ---
    {
        const MeshStats s = analyzeMesh(cube(0.0f, 1.0f));
        CHECK(s.vertexCount == 8 && s.triangleCount == 12 && s.edgeCount == 18, "cube V/F/E = 8/12/18");
        CHECK(near(s.boundsMin.x, 0.0, 1e-6) && near(s.boundsMax.x, 1.0, 1e-6), "cube bounds are [0,1]");
        CHECK(near(s.size().x, 1.0, 1e-6) && near(s.size().y, 1.0, 1e-6) && near(s.size().z, 1.0, 1e-6),
              "cube size is 1x1x1");
        CHECK(near(s.surfaceArea, 6.0, 1e-5), "unit cube surface area is 6");
        CHECK(near(s.centroid.x, 0.5, 1e-5) && near(s.centroid.y, 0.5, 1e-5) && near(s.centroid.z, 0.5, 1e-5),
              "cube centroid is its centre");
        CHECK(near(s.minEdgeLength, 1.0, 1e-5), "shortest edge is a length-1 rim");
        CHECK(near(s.maxEdgeLength, std::sqrt(2.0), 1e-5), "longest edge is a sqrt(2) face diagonal");
        // 12 rims of 1 + 6 diagonals of sqrt(2), averaged over 18 edges.
        const double expectedMean = (12.0 * 1.0 + 6.0 * std::sqrt(2.0)) / 18.0;
        CHECK(near(s.meanEdgeLength, expectedMean, 1e-5), "mean edge length matches the 12x1 + 6xsqrt2 mix");
    }

    // --- 2. Scaled + translated cube: bounds and centroid shift and scale. ---
    {
        const MeshStats s = analyzeMesh(cube(2.0f, 3.0f)); // corner at 2, edge 3 -> [2,5]^3
        CHECK(near(s.boundsMin.x, 2.0, 1e-6) && near(s.boundsMax.x, 5.0, 1e-6), "translated bounds are [2,5]");
        CHECK(near(s.centroid.x, 3.5, 1e-5), "centroid tracks the translation (3.5)");
        CHECK(near(s.surfaceArea, 6.0 * 9.0, 1e-4), "area scales with edge^2 (6 * 3^2 = 54)");
        CHECK(near(s.minEdgeLength, 3.0, 1e-5), "shortest edge scaled to 3");
    }

    // --- 3. Single triangle: exact area and centroid. ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(0,1,0)};
        m.indices = {0,1,2};
        const MeshStats s = analyzeMesh(m);
        CHECK(near(s.surfaceArea, 0.5, 1e-6), "right triangle area is 0.5");
        CHECK(near(s.centroid.x, 1.0/3.0, 1e-5) && near(s.centroid.y, 1.0/3.0, 1e-5), "triangle centroid at (1/3,1/3)");
        CHECK(s.edgeCount == 3, "a triangle has three edges");
        CHECK(near(s.minEdgeLength, 1.0, 1e-5) && near(s.maxEdgeLength, std::sqrt(2.0), 1e-5),
              "triangle edges are 1,1,sqrt(2)");
    }

    // --- 4. Empty mesh is safe. ---
    {
        const MeshStats s = analyzeMesh(shapes::MeshData{});
        CHECK(s.vertexCount == 0 && s.triangleCount == 0 && s.surfaceArea == 0.0, "empty mesh -> zeroed stats");
    }

    if (g_fail == 0) {
        std::printf("meshstats: OK — cube area 6 / 18 edges / centre centroid, scale+translate, triangle area 0.5.\n");
        return 0;
    }
    std::printf("meshstats: %d failure(s).\n", g_fail);
    return 1;
}
