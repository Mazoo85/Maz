// tests/render/meshheightfield.cpp — verifies terrain-mesh build (render::buildHeightfield). Ground truths: a
// cols*rows grid makes cols*rows vertices and (cols-1)*(rows-1)*2 triangles; each vertex is lifted to its grid
// height*heightScale; a flat grid has all-up (+Y) normals; the grid is centred on the origin with cellSize spacing;
// mismatched / too-small inputs are safe. Pure CPU, headless.
#include "maz/render/MeshHeightfield.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

int main() {
    // --- 1. A 3x3 flat grid: counts, centring, y=0, and all normals point straight up. ---
    {
        const std::vector<float> flat(9, 0.0f);
        const shapes::MeshData m = buildHeightfield(flat, 3, 3, 2.0f, 1.0f);

        CHECK(m.vertices.size() == 9, "3x3 grid -> 9 vertices");
        CHECK(m.indices.size() == 2u * 2u * 2u * 3u, "(cols-1)*(rows-1)*2 triangles");

        bool up = true, flatY = true;
        float minX = 1e9f, maxX = -1e9f, minZ = 1e9f, maxZ = -1e9f;
        for (const MeshVertex& v : m.vertices) {
            if (v.ny < 0.99f) up = false;                 // flat terrain -> normals straight up
            if (!near(v.py, 0.0f, 1e-6f)) flatY = false;
            minX = std::fmin(minX, v.px); maxX = std::fmax(maxX, v.px);
            minZ = std::fmin(minZ, v.pz); maxZ = std::fmax(maxZ, v.pz);
        }
        CHECK(up, "a flat grid has all +Y normals");
        CHECK(flatY, "a flat grid sits at y=0");
        // centred: 3 points, spacing 2 -> spans -2..+2 on both axes.
        CHECK(near(minX, -2.0f, 1e-6f) && near(maxX, 2.0f, 1e-6f), "grid centred on X with cellSize spacing");
        CHECK(near(minZ, -2.0f, 1e-6f) && near(maxZ, 2.0f, 1e-6f), "grid centred on Z with cellSize spacing");
    }

    // --- 2. Heights map through: vertex (i,j) has py == heights[j*cols+i] * heightScale. ---
    {
        // 2x2 grid with distinct heights, scaled by 10.
        const std::vector<float> h = {0.0f, 1.0f, 2.0f, 3.0f}; // (0,0)=0 (1,0)=1 (0,1)=2 (1,1)=3
        const shapes::MeshData m = buildHeightfield(h, 2, 2, 1.0f, 10.0f);
        CHECK(m.vertices.size() == 4, "2x2 -> 4 verts");
        bool ok = true;
        for (int j = 0; j < 2; ++j)
            for (int i = 0; i < 2; ++i) {
                const float expected = h[static_cast<std::size_t>(j * 2 + i)] * 10.0f;
                if (!near(m.vertices[static_cast<std::size_t>(j * 2 + i)].py, expected, 1e-5f)) ok = false;
            }
        CHECK(ok, "each vertex is lifted to its height * heightScale");
    }

    // --- 3. A tilted ramp (height rises with i) has normals leaning off +Y but still upward. ---
    {
        // 3x3, height = i (a ramp climbing in +X). Slope means normals tilt toward -X but keep ny>0.
        std::vector<float> ramp(9);
        for (int j = 0; j < 3; ++j) for (int i = 0; i < 3; ++i) ramp[static_cast<std::size_t>(j * 3 + i)] = static_cast<float>(i);
        const shapes::MeshData m = buildHeightfield(ramp, 3, 3, 1.0f, 1.0f);
        bool tilted = false, allUp = true;
        for (const MeshVertex& v : m.vertices) {
            if (v.nx < -0.1f) tilted = true; // normal leans in -X against the climb
            if (v.ny <= 0.0f) allUp = false;
        }
        CHECK(tilted, "a ramp tilts normals off vertical");
        CHECK(allUp, "ramp normals still point generally upward");
    }

    // --- 4. Degenerate inputs are safe. ---
    {
        CHECK(buildHeightfield(std::vector<float>(1, 0.0f), 1, 1).vertices.empty(), "1x1 grid -> empty");
        CHECK(buildHeightfield({0, 0, 0}, 3, 3).vertices.empty(), "size mismatch (3 != 9) -> empty");
        CHECK(buildHeightfield({}, 4, 4).vertices.empty(), "empty heights -> empty");
    }

    if (g_fail == 0) {
        std::printf("meshheightfield: OK — grid counts, heights mapped, flat=up normals, centred, safe.\n");
        return 0;
    }
    std::printf("meshheightfield: %d failure(s).\n", g_fail);
    return 1;
}
