// tests/render/meshvoxelize.cpp — verifies solid mesh voxelization (render::voxelizeSolid). Ground truth of a
// solid fill: the number of inside cells times the cell volume must converge to the mesh's real volume. A unit
// cube fills to ~1.0; a radius-1 sphere fills to ~(4/3)*pi ~ 4.18867; a point at the centre is inside and a
// point out in the padding shell is outside. Built on the same ray-parity inside test as MeshSdf. Headless.
#include "maz/render/MeshVoxelize.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}

// Axis-aligned cube [0,1]^3, welded, outward-wound.
static shapes::MeshData unitCube() {
    shapes::MeshData m;
    m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,1,0), vtx(0,1,0),
                  vtx(0,0,1), vtx(1,0,1), vtx(1,1,1), vtx(0,1,1)};
    m.indices = {
        0,2,1, 0,3,2,   // front  z=0
        4,5,6, 4,6,7,   // back    z=1
        0,1,5, 0,5,4,   // bottom  y=0
        3,7,6, 3,6,2,   // top     y=1
        0,4,7, 0,7,3,   // left    x=0
        1,2,6, 1,6,5,   // right   x=1
    };
    return m;
}

// Welded UV sphere of radius r (single poles, no seam duplication) — a closed manifold solid.
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

static bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

int main() {
    // --- 1. Unit cube, grid-aligned (padding 0): faces land on cell edges so the fill is exact -> vol == 1. ---
    {
        const VoxelGrid g = voxelizeSolid(unitCube(), 32, 0.0f);
        CHECK(g.nx > 0 && g.ny > 0 && g.nz > 0, "cube grid is non-empty");
        CHECK(g.solidCount() > 0, "cube has solid cells");
        CHECK(near(g.estimatedVolume(), 1.0, 1e-3), "aligned cube voxel volume is exactly 1.0");
        // A cell whose centre is near the cube middle must be solid.
        const int cx = static_cast<int>(0.5f / g.voxelSize);
        CHECK(g.at(cx, cx, cx) == 1, "cube centre cell is solid");
    }

    // --- 2. Resolution up -> the aligned estimate stays exact; a padded grid leaves an empty shell. ---
    {
        const VoxelGrid lo = voxelizeSolid(unitCube(), 16, 0.0f);
        const VoxelGrid hi = voxelizeSolid(unitCube(), 40, 0.0f);
        CHECK(near(lo.estimatedVolume(), 1.0, 1e-3), "coarse aligned grid volume is 1.0");
        CHECK(near(hi.estimatedVolume(), 1.0, 1e-3), "fine aligned grid volume is 1.0");
        CHECK(hi.solidCount() > lo.solidCount(), "finer grid has more solid cells");
        // Padded grid: the mesh is inset, so the outer corner cell is empty and the volume is still ~1.
        const VoxelGrid padded = voxelizeSolid(unitCube(), 32, 0.1f);
        CHECK(padded.at(0, 0, 0) == 0, "padding-corner cell is empty");
        CHECK(near(padded.estimatedVolume(), 1.0, 0.1), "padded cube volume is within 10% of 1.0");
    }

    // --- 3. Sphere: solid volume ~ (4/3)*pi*r^3. ---
    {
        const double pi = 3.14159265358979323846;
        const VoxelGrid g = voxelizeSolid(sphere(1.0f, 32, 32), 36, 0.15f);
        const double truth = 4.0 / 3.0 * pi; // r=1
        CHECK(near(g.estimatedVolume(), truth, 0.25), "unit sphere voxel volume ~ 4.19");
        // Radius-2 sphere scales as r^3 = 8x.
        const VoxelGrid g2 = voxelizeSolid(sphere(2.0f, 32, 32), 36, 0.3f);
        CHECK(near(g2.estimatedVolume(), 8.0 * truth, 2.0), "radius-2 sphere volume ~ 8x the unit sphere");
    }

    // --- 4. Empty mesh is safe. ---
    {
        const VoxelGrid g = voxelizeSolid(shapes::MeshData{}, 16, 0.0f);
        CHECK(g.nx == 0 && g.solid.empty() && g.solidCount() == 0, "empty mesh -> empty grid");
    }

    if (g_fail == 0) {
        std::printf("meshvoxelize: OK — cube vol~1, sphere vol~4.19 (8x at r=2), centre solid, shell empty.\n");
        return 0;
    }
    std::printf("meshvoxelize: %d failure(s).\n", g_fail);
    return 1;
}
