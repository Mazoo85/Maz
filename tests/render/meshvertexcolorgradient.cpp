// tests/render/meshvertexcolorgradient.cpp — verifies vertex-colour gradient paint (render::paintAxisGradient /
// paintRadialGradient). Ground truths: an axis gradient makes the RGB run smoothly from `low` at the axis min to
// `high` at the axis max with auto-fit to the bbox; a radial gradient makes the RGB run from `innerColor` at the
// centre to `outerColor` past `outer`, clamped both ends; positions/normals are never touched. Pure CPU, headless.
#include "maz/render/MeshVertexColorGradient.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex at(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.nx = 0; v.ny = 1; v.nz = 0; v.r = v.g = v.b = 0.5f; return v;
}
static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

int main() {
    const Color black{0, 0, 0, 1};
    const Color white{1, 1, 1, 1};

    // --- 1. Axis gradient along Y, auto-fit: bottom -> black, top -> white, middle -> grey. ---
    {
        shapes::MeshData m;
        m.vertices = {at(0, 0, 0), at(0, 5, 0), at(0, 10, 0)}; // y = 0, 5, 10
        m.indices = {0, 1, 2};
        const shapes::MeshData p = paintAxisGradient(m, /*axis=*/1, black, white); // auto range [0,10]
        CHECK(near(p.vertices[0].r, 0.0f, 1e-5f), "bottom vertex gets `low` (black)");
        CHECK(near(p.vertices[2].r, 1.0f, 1e-5f), "top vertex gets `high` (white)");
        CHECK(near(p.vertices[1].r, 0.5f, 1e-5f), "the middle vertex is half-way -> mid grey");
        CHECK(near(p.vertices[1].g, 0.5f, 1e-5f) && near(p.vertices[1].b, 0.5f, 1e-5f), "all channels ramp together");
        // Geometry is untouched.
        CHECK(near(p.vertices[1].py, 5.0f, 1e-6f) && near(p.vertices[1].ny, 1.0f, 1e-6f), "positions/normals untouched");
    }

    // --- 2. Explicit axis range clamps outside [min,max]. ---
    {
        shapes::MeshData m;
        m.vertices = {at(-100, 0, 0), at(0, 0, 0), at(100, 0, 0)}; // x well outside [0,1]
        m.indices = {0, 1, 2};
        const shapes::MeshData p = paintAxisGradient(m, /*axis=*/0, black, white, /*min=*/0.0f, /*max=*/1.0f);
        CHECK(near(p.vertices[0].r, 0.0f, 1e-5f), "x below min clamps to `low`");
        CHECK(near(p.vertices[2].r, 1.0f, 1e-5f), "x above max clamps to `high`");
        CHECK(near(p.vertices[1].r, 0.0f, 1e-5f), "x at min is exactly `low`");
    }

    // --- 3. Zero-extent axis paints everything `low`. ---
    {
        shapes::MeshData m;
        m.vertices = {at(0, 3, 0), at(0, 3, 0)}; // all y = 3
        m.indices = {0, 1, 0};
        const shapes::MeshData p = paintAxisGradient(m, 1, black, white); // auto range collapses
        CHECK(near(p.vertices[0].r, 0.0f, 1e-5f) && near(p.vertices[1].r, 0.0f, 1e-5f), "flat axis -> all `low`");
    }

    // --- 4. Radial gradient from origin: centre -> inner, far -> outer, clamped. ---
    {
        shapes::MeshData m;
        m.vertices = {at(0, 0, 0), at(1, 0, 0), at(2, 0, 0), at(5, 0, 0)}; // dist 0,1,2,5
        m.indices = {0, 1, 2};
        // inner=0, outer=2 : t = dist/2.
        const shapes::MeshData p = paintRadialGradient(m, 0, 0, 0, /*inner=*/0.0f, /*outer=*/2.0f, black, white);
        CHECK(near(p.vertices[0].r, 0.0f, 1e-5f), "centre gets innerColor (black)");
        CHECK(near(p.vertices[1].r, 0.5f, 1e-5f), "dist 1 of 2 -> half-way grey");
        CHECK(near(p.vertices[2].r, 1.0f, 1e-5f), "dist 2 = outer -> outerColor (white)");
        CHECK(near(p.vertices[3].r, 1.0f, 1e-5f), "dist 5 beyond outer clamps to outerColor");
    }

    // --- 5. Degenerate radial band (outer <= inner) is a hard ring at `inner`. ---
    {
        shapes::MeshData m;
        m.vertices = {at(0, 0, 0), at(3, 0, 0)}; // dist 0 (inside), 3 (outside)
        m.indices = {0, 1, 0};
        const shapes::MeshData p = paintRadialGradient(m, 0, 0, 0, /*inner=*/1.0f, /*outer=*/1.0f, black, white);
        CHECK(near(p.vertices[0].r, 0.0f, 1e-5f), "dist <= inner -> innerColor");
        CHECK(near(p.vertices[1].r, 1.0f, 1e-5f), "dist > inner -> outerColor (hard step)");
    }

    // --- 6. Empty mesh is safe. ---
    {
        CHECK(paintAxisGradient(shapes::MeshData{}, 1, black, white).vertices.empty(), "empty axis paint is safe");
        CHECK(paintRadialGradient(shapes::MeshData{}, 0, 0, 0, 0, 1, black, white).vertices.empty(),
              "empty radial paint is safe");
    }

    if (g_fail == 0) {
        std::printf("meshvertexcolorgradient: OK — axis ramps min->max (auto/clamped), radial ramps centre->outer.\n");
        return 0;
    }
    std::printf("meshvertexcolorgradient: %d failure(s).\n", g_fail);
    return 1;
}
