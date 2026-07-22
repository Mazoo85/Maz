// tests/render/meshquantize.cpp — verifies mesh vertex quantization (render::quantizeMesh /
// dequantizeMesh). The core guarantee of grid quantization: after a quantize->dequantize round-trip, every
// position and UV channel lands within HALF A GRID STEP of the original (grid step = extent / (2^bits-1)),
// more bits shrink the error, indices are preserved exactly, and a flat axis is reproduced exactly. Pure CPU,
// headless.
#include "maz/render/MeshQuantize.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

// A blob of vertices spread over a known box in X/Y/Z with UVs over [0,1]^2, plus a deliberately FLAT Z axis
// case handled separately. Positions chosen irregular so rounding is exercised.
static shapes::MeshData blob() {
    shapes::MeshData m;
    for (int i = 0; i < 64; ++i) {
        MeshVertex v{};
        const float f = static_cast<float>(i);
        v.px = -3.0f + std::fmod(f * 0.37f, 6.0f);   // within [-3, 3]
        v.py = 10.0f + std::fmod(f * 0.911f, 4.0f);  // within [10, 14]
        v.pz = std::fmod(f * 0.13f, 2.0f);           // within [0, 2)
        v.u = std::fmod(f * 0.041f, 1.0f);
        v.v = std::fmod(f * 0.083f, 1.0f);
        v.r = v.g = v.b = 1.0f;
        m.vertices.push_back(v);
    }
    for (std::uint32_t i = 0; i + 2 < m.vertices.size(); i += 3)
        m.indices.insert(m.indices.end(), {i, i + 1, i + 2});
    return m;
}

// Largest per-channel round-trip error, and the grid step for each attribute axis, at `bits`.
static float maxError(const shapes::MeshData& src, int bits, float stepOut[5]) {
    const QuantizedMesh q = quantizeMesh(src, bits);
    const shapes::MeshData r = dequantizeMesh(q);
    const float levels = static_cast<float>((1u << bits) - 1u);
    stepOut[0] = q.posExtent[0] / levels; stepOut[1] = q.posExtent[1] / levels;
    stepOut[2] = q.posExtent[2] / levels; stepOut[3] = q.uvExtent[0] / levels;
    stepOut[4] = q.uvExtent[1] / levels;
    float e = 0.0f;
    for (std::size_t i = 0; i < src.vertices.size(); ++i) {
        const MeshVertex& a = src.vertices[i];
        const MeshVertex& b = r.vertices[i];
        e = std::max(e, std::fabs(a.px - b.px)); e = std::max(e, std::fabs(a.py - b.py));
        e = std::max(e, std::fabs(a.pz - b.pz)); e = std::max(e, std::fabs(a.u - b.u));
        e = std::max(e, std::fabs(a.v - b.v));
    }
    return e;
}

int main() {
    const shapes::MeshData src = blob();

    // --- 1. Round-trip error is bounded by half a grid step at several bit depths. ---
    for (int bits : {8, 10, 12, 14}) {
        float step[5];
        const float err = maxError(src, bits, step);
        float halfStep = 0.0f;
        for (float s : step) halfStep = std::max(halfStep, s * 0.5f);
        // Allow a tiny float-rounding slack over the exact half-step bound.
        CHECK(err <= halfStep + 1e-5f, "round-trip error within half a grid step");
        CHECK(err > 0.0f, "quantization is actually lossy (non-trivial)");
    }

    // --- 2. More bits => smaller error (monotone). ---
    {
        float s[5];
        const float e8 = maxError(src, 8, s);
        const float e14 = maxError(src, 14, s);
        CHECK(e14 < e8, "more bits reduce the error");
    }

    // --- 3. Indices preserved; vertex count preserved. ---
    {
        const QuantizedMesh q = quantizeMesh(src, 12);
        const shapes::MeshData r = dequantizeMesh(q);
        CHECK(r.indices == src.indices, "indices survive the round-trip");
        CHECK(r.vertices.size() == src.vertices.size(), "vertex count preserved");
    }

    // --- 4. A flat axis (all Z equal) reconstructs exactly, no divide-by-zero. ---
    {
        shapes::MeshData flat;
        for (int i = 0; i < 8; ++i) {
            MeshVertex v{};
            v.px = static_cast<float>(i); v.py = static_cast<float>(-i); v.pz = 5.0f; // Z constant
            v.u = 0.25f; v.v = 0.75f; // UVs constant too
            flat.vertices.push_back(v);
        }
        const shapes::MeshData r = dequantizeMesh(quantizeMesh(flat, 10));
        bool exact = true;
        for (std::size_t i = 0; i < flat.vertices.size(); ++i)
            if (r.vertices[i].pz != 5.0f || r.vertices[i].u != 0.25f || r.vertices[i].v != 0.75f) exact = false;
        CHECK(exact, "flat axes reconstruct exactly");
    }

    // --- 5. Empty mesh is handled. ---
    {
        const QuantizedMesh q = quantizeMesh(shapes::MeshData{}, 12);
        CHECK(q.vertexCount == 0 && dequantizeMesh(q).vertices.empty(), "empty mesh round-trips to empty");
    }

    if (g_fail == 0) {
        std::printf("meshquantize: OK — half-step error bound, monotone in bits, indices + flat axes exact.\n");
        return 0;
    }
    std::printf("meshquantize: %d failure(s).\n", g_fail);
    return 1;
}
