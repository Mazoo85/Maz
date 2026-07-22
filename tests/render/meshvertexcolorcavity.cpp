// tests/render/meshvertexcolorcavity.cpp — verifies the cavity (curvature) vertex-color bake
// (render::computeCavity / bakeCavityToVertexColor). Ground truths: a smooth sphere reads uniformly CONVEX
// (negative, low variance); a V-shaped valley reads CONCAVE (positive) along its crease; an inverted roof reads
// CONVEX (negative) along its ridge; a flat grid reads ~0; and the colour bake darkens concave creases and
// lightens convex ridges. Pure CPU, headless.
#include "maz/render/MeshVertexColorCavity.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}

// A UV sphere centred at the origin (radius r). Winding gives outward normals.
static shapes::MeshData sphere(float r, int rings, int sectors) {
    shapes::MeshData m;
    for (int i = 0; i <= rings; ++i) {
        const float phi = 3.14159265f * static_cast<float>(i) / static_cast<float>(rings); // 0..pi
        const float y = std::cos(phi), sr = std::sin(phi);
        for (int j = 0; j <= sectors; ++j) {
            const float th = 2.0f * 3.14159265f * static_cast<float>(j) / static_cast<float>(sectors);
            m.vertices.push_back(vtx(r * sr * std::cos(th), r * y, r * sr * std::sin(th)));
        }
    }
    const int stride = sectors + 1;
    for (int i = 0; i < rings; ++i) {
        for (int j = 0; j < sectors; ++j) {
            const std::uint32_t a = static_cast<std::uint32_t>(i * stride + j);
            const std::uint32_t b = static_cast<std::uint32_t>((i + 1) * stride + j);
            m.indices.insert(m.indices.end(), {a, a + 1u, b, a + 1u, b + 1u, b}); // CCW -> outward normals
        }
    }
    return m;
}

// A 3-column x N-row grid folded along the middle column. `midY` sets the crease height; the outer columns sit
// at y=0. midY<0 => a V-valley (concave crease); midY>0 => an inverted roof (convex ridge). Column x = -1,0,1.
// Winding is CCW seen from +Y so smooth normals point generally upward. Returns the crease-vertex index range.
static shapes::MeshData foldedGrid(float midY, int rows, std::size_t& firstCrease, std::size_t& lastCrease) {
    shapes::MeshData m;
    const float xs[3] = {-1.0f, 0.0f, 1.0f};
    for (int rz = 0; rz < rows; ++rz) {
        const float z = static_cast<float>(rz);
        for (int cx = 0; cx < 3; ++cx) {
            const float y = (cx == 1) ? midY : 0.0f;
            m.vertices.push_back(vtx(xs[cx], y, z));
        }
    }
    for (int rz = 0; rz < rows - 1; ++rz) {
        for (int cx = 0; cx < 2; ++cx) {
            const std::uint32_t a = static_cast<std::uint32_t>(rz * 3 + cx);
            const std::uint32_t b = static_cast<std::uint32_t>((rz + 1) * 3 + cx);
            const std::uint32_t c = a + 1u;
            const std::uint32_t d = b + 1u;
            m.indices.insert(m.indices.end(), {a, b, c, c, b, d}); // CCW from above
        }
    }
    firstCrease = 1;                                   // column index 1 in row 0
    lastCrease = static_cast<std::size_t>((rows - 1) * 3 + 1); // column 1 in last row
    return m;
}

int main() {
    // --- 1. Smooth sphere: uniformly convex (negative), low variance. ---
    {
        const shapes::MeshData m = sphere(1.0f, 16, 24);
        const std::vector<float> cav = computeCavity(m);
        float mn = 1e9f, mx = -1e9f, sum = 0.0f; std::size_t n = 0;
        for (float c : cav) {
            if (c == 0.0f) continue; // skip the untouched poles seam if any
            mn = std::min(mn, c);
            mx = std::max(mx, c);
            sum += c;
            ++n;
        }
        const float mean = sum / static_cast<float>(n);
        CHECK(mean < 0.0f, "sphere reads convex on average (negative cavity)");
        CHECK((mx - mn) < 0.15f, "sphere cavity is nearly uniform across the surface");
    }

    // --- 2. V-valley: the crease column reads concave (positive) and stronger than the flat outer columns. ---
    {
        std::size_t fc = 0, lc = 0;
        const shapes::MeshData m = foldedGrid(-1.0f, 6, fc, lc); // middle column dips down => valley
        const std::vector<float> cav = computeCavity(m);
        // A mid-crease vertex (interior row, column 1) vs a flat outer vertex (column 0).
        const std::size_t crease = 3 * 2 + 1; // row 2, column 1
        const std::size_t flat = 3 * 2 + 0;   // row 2, column 0
        CHECK(cav[crease] > 0.05f, "valley crease vertex reads concave (positive)");
        CHECK(cav[crease] > cav[flat], "crease is more concave than the flat flank");
    }

    // --- 3. Inverted roof: the ridge column reads convex (negative). ---
    {
        std::size_t fc = 0, lc = 0;
        const shapes::MeshData m = foldedGrid(1.0f, 6, fc, lc); // middle column rises => roof ridge
        const std::vector<float> cav = computeCavity(m);
        const std::size_t ridge = 3 * 2 + 1;
        CHECK(cav[ridge] < -0.05f, "roof ridge vertex reads convex (negative)");
    }

    // --- 4. Flat grid: cavity ~0 everywhere. ---
    {
        std::size_t fc = 0, lc = 0;
        const shapes::MeshData m = foldedGrid(0.0f, 6, fc, lc); // all columns coplanar
        const std::vector<float> cav = computeCavity(m);
        bool flat = true;
        for (float c : cav) if (std::fabs(c) > 1e-4f) flat = false;
        CHECK(flat, "a flat grid has no cavity signal");
    }

    // --- 5. Colour bake: crease darkens, ridge/flat stays brighter; channels stay in [0,1]. ---
    {
        std::size_t fc = 0, lc = 0;
        const shapes::MeshData valley = foldedGrid(-1.0f, 6, fc, lc);
        const shapes::MeshData baked = bakeCavityToVertexColor(valley, 1.0f, 4.0f);
        const std::size_t crease = 3 * 2 + 1, flat = 3 * 2 + 0;
        CHECK(baked.vertices[crease].r < baked.vertices[flat].r, "cavity bake darkens the concave crease");
        CHECK(baked.vertices[crease].r < 0.999f, "crease colour is actually darkened below white");
        bool inRange = true;
        for (const auto& v : baked.vertices)
            if (v.r < 0.0f || v.r > 1.0f || v.g < 0.0f || v.g > 1.0f || v.b < 0.0f || v.b > 1.0f) inRange = false;
        CHECK(inRange, "all baked colours stay within [0,1]");

        // A convex ridge should brighten a non-white base colour rather than darken it.
        shapes::MeshData roof = foldedGrid(1.0f, 6, fc, lc);
        for (auto& v : roof.vertices) { v.r = v.g = v.b = 0.5f; }
        const shapes::MeshData bakedRoof = bakeCavityToVertexColor(roof, 1.0f, 4.0f);
        CHECK(bakedRoof.vertices[3 * 2 + 1].r > 0.5f, "cavity bake lightens the convex ridge");
    }

    // --- 6. Empty / tiny meshes are safe. ---
    {
        CHECK(computeCavity(shapes::MeshData{}).empty(), "empty mesh -> no cavity values");
        CHECK(bakeCavityToVertexColor(shapes::MeshData{}).vertices.empty(), "empty mesh -> empty bake");
    }

    if (g_fail == 0) {
        std::printf("meshvertexcolorcavity: OK — sphere uniform-convex, valley concave, roof convex, flat zero, bake darkens creases.\n");
        return 0;
    }
    std::printf("meshvertexcolorcavity: %d failure(s).\n", g_fail);
    return 1;
}
