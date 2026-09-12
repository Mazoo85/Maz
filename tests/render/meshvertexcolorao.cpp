// tests/render/meshvertexcolorao.cpp — verifies vertex-color AO bake (render::bakeAoToVertexColor). Ground
// truths: an open surface stays bright (nothing occludes it), a surface trapped under a low ceiling darkens
// strongly (the hemisphere is blocked), strength=0 is a no-op, and baking is multiplicative so an existing tint
// is preserved. Built on the M533 hemisphere AO raycaster. Pure CPU, headless.
#include "maz/render/MeshVertexColorAo.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex cv(float x, float y, float z, float r, float g, float b) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = r; v.g = g; v.b = b; return v;
}

// A quad in the XZ plane at height `y` spanning [x0,x1]x[z0,z1], wound so its normal points +Y (up).
static void addUpQuadSized(shapes::MeshData& m, float y, float x0, float x1, float z0, float z1,
                           float r, float g, float b) {
    const std::uint32_t base = static_cast<std::uint32_t>(m.vertices.size());
    m.vertices.push_back(cv(x0, y, z0, r, g, b));
    m.vertices.push_back(cv(x1, y, z0, r, g, b));
    m.vertices.push_back(cv(x1, y, z1, r, g, b));
    m.vertices.push_back(cv(x0, y, z1, r, g, b));
    // Wound so the face normal points +Y (up), so the AO hemisphere faces up toward any ceiling.
    m.indices.insert(m.indices.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
}
// A 2x2 floor at [0,2]x[0,2].
static void addUpQuad(shapes::MeshData& m, float y, float r, float g, float b) {
    addUpQuadSized(m, y, 0, 2, 0, 2, r, g, b);
}
// A ceiling that overhangs the floor so even its corners are occluded.
static void addCeiling(shapes::MeshData& m, float y, float r, float g, float b) {
    addUpQuadSized(m, y, -2, 4, -2, 4, r, g, b);
}

int main() {
    // --- 1. An open upward-facing quad stays bright (nothing occludes it). ---
    {
        shapes::MeshData m;
        addUpQuad(m, 0.0f, 1.0f, 1.0f, 1.0f);
        const shapes::MeshData r = bakeAoToVertexColor(m, 1.0f, 64, 1e30f);
        bool bright = true;
        for (const auto& v : r.vertices) if (v.r < 0.85f) bright = false;
        CHECK(bright, "an open surface keeps its colour (AO ~ 0)");
    }

    // --- 2. A floor trapped under a low ceiling darkens strongly. ---
    {
        shapes::MeshData m;
        addUpQuad(m, 0.0f, 1.0f, 1.0f, 1.0f); // floor (verts 0..3)
        addCeiling(m, 0.3f, 1.0f, 1.0f, 1.0f); // ceiling (verts 4..7)
        const shapes::MeshData r = bakeAoToVertexColor(m, 1.0f, 64, 1e30f);
        float darkest = 1.0f;
        for (const auto& v : r.vertices) if (v.r < darkest) darkest = v.r;
        CHECK(darkest < 0.5f, "a surface under a low ceiling is heavily occluded (dark)");
        // The floor vertices specifically (indices 0..3) should be well below open brightness.
        bool floorDark = true;
        for (int i = 0; i < 4; ++i) if (r.vertices[static_cast<std::size_t>(i)].r > 0.6f) floorDark = false;
        CHECK(floorDark, "every floor vertex under the ceiling is darkened");
    }

    // --- 3. strength = 0 is a no-op. ---
    {
        shapes::MeshData m;
        addUpQuad(m, 0.0f, 1.0f, 1.0f, 1.0f);
        addUpQuad(m, 0.3f, 1.0f, 1.0f, 1.0f);
        const shapes::MeshData r = bakeAoToVertexColor(m, 0.0f, 64, 1e30f);
        bool unchanged = true;
        for (const auto& v : r.vertices) if (v.r != 1.0f) unchanged = false;
        CHECK(unchanged, "strength 0 leaves colours untouched");
    }

    // --- 4. Baking is multiplicative: an existing tint (and its ratios) survives. ---
    {
        shapes::MeshData m;
        addUpQuad(m, 0.0f, 1.0f, 0.5f, 0.0f); // tinted floor
        addCeiling(m, 0.3f, 1.0f, 0.5f, 0.0f); // ceiling (verts 4..7)
        const shapes::MeshData r = bakeAoToVertexColor(m, 1.0f, 64, 1e30f);
        bool tintKept = true;
        for (const auto& v : r.vertices) {
            if (v.b != 0.0f) tintKept = false;                       // zero channel stays zero
            if (v.r > 1e-4f && std::abs(v.g / v.r - 0.5f) > 1e-4f) tintKept = false; // g:r ratio preserved
        }
        CHECK(tintKept, "AO multiplies colour, preserving the tint's channel ratios");
    }

    // --- 5. Empty mesh is safe. ---
    {
        const shapes::MeshData r = bakeAoToVertexColor(shapes::MeshData{});
        CHECK(r.vertices.empty(), "empty mesh -> empty");
    }

    if (g_fail == 0) {
        std::printf("meshvertexcolorao: OK — open stays bright, under-ceiling darkens, strength0 no-op, tint kept.\n");
        return 0;
    }
    std::printf("meshvertexcolorao: %d failure(s).\n", g_fail);
    return 1;
}
