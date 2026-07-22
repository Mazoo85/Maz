#pragma once

#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render MESH VERTEX QUANTIZATION — the lossy-but-bounded attribute compression a glTF/Draco-style
// exporter runs to shrink a mesh: instead of a full 32-bit float per position/UV channel, snap each channel
// to an N-bit integer grid spanning that attribute's bounding box, storing the compact ints plus the box
// (origin + extent) needed to reconstruct. Dequantizing maps the grid back to floats. Because the grid step
// is extent / (2^bits - 1), the reconstruction error on any channel is BOUNDED by half a grid step — a
// guarantee this unit-tests directly. Godot's importer exposes the same idea (import compression / the
// meshoptimizer + Draco path). This is the CPU quantize/dequantize core (the bit-packing to disk is a
// separate transport concern); it works on the geometry buffers headlessly, no GPU.
//
// Scope note (honest): uniform per-attribute-bbox quantization of positions and UVs with a provable error
// bound — the workhorse that gives most of the size win. Normal/tangent octahedral encoding and entropy
// coding of the index stream are documented follow-ups.
namespace maz::render {

// The reconstruction data for one quantized channel set: the box the grid spans and the bit depth. Positions
// use a 3D box; UVs a 2D box. Kept explicit so a dequantize is exact w.r.t. the stored grid.
struct QuantizedMesh {
    int bits = 0;                       // grid resolution = 2^bits - 1 steps per axis
    float posMin[3] = {0, 0, 0};
    float posExtent[3] = {0, 0, 0};     // max - min per axis (0 if the axis is flat)
    float uvMin[2] = {0, 0};
    float uvExtent[2] = {0, 0};
    std::vector<std::uint32_t> pos;     // 3 ints per vertex (x,y,z on the grid)
    std::vector<std::uint32_t> uv;      // 2 ints per vertex (u,v on the grid)
    std::vector<std::uint32_t> indices; // unchanged triangle indices
    std::uint32_t vertexCount = 0;
};

namespace detail {
inline std::uint32_t quantChannel(float v, float mn, float extent, std::uint32_t maxLevel) {
    if (extent <= 0.0f) return 0;
    float t = (v - mn) / extent;              // 0..1 within the box
    if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
    return static_cast<std::uint32_t>(t * static_cast<float>(maxLevel) + 0.5f); // round to nearest level
}
inline float dequantChannel(std::uint32_t q, float mn, float extent, std::uint32_t maxLevel) {
    if (extent <= 0.0f || maxLevel == 0) return mn;
    return mn + (static_cast<float>(q) / static_cast<float>(maxLevel)) * extent;
}
} // namespace detail

// Quantize positions + UVs to a `bits`-per-channel grid over each attribute's own bounding box. `bits` is
// clamped to 1..16. An empty mesh yields an empty result.
inline QuantizedMesh quantizeMesh(const shapes::MeshData& mesh, int bits) {
    QuantizedMesh q;
    if (bits < 1) bits = 1; else if (bits > 16) bits = 16;
    q.bits = bits;
    q.indices = mesh.indices;
    q.vertexCount = static_cast<std::uint32_t>(mesh.vertices.size());
    if (mesh.vertices.empty()) return q;

    float pMin[3] = {1e30f, 1e30f, 1e30f}, pMax[3] = {-1e30f, -1e30f, -1e30f};
    float uMin[2] = {1e30f, 1e30f}, uMax[2] = {-1e30f, -1e30f};
    for (const MeshVertex& v : mesh.vertices) {
        const float p[3] = {v.px, v.py, v.pz};
        for (int i = 0; i < 3; ++i) { pMin[i] = std::min(pMin[i], p[i]); pMax[i] = std::max(pMax[i], p[i]); }
        const float t[2] = {v.u, v.v};
        for (int i = 0; i < 2; ++i) { uMin[i] = std::min(uMin[i], t[i]); uMax[i] = std::max(uMax[i], t[i]); }
    }
    for (int i = 0; i < 3; ++i) { q.posMin[i] = pMin[i]; q.posExtent[i] = pMax[i] - pMin[i]; }
    for (int i = 0; i < 2; ++i) { q.uvMin[i] = uMin[i]; q.uvExtent[i] = uMax[i] - uMin[i]; }

    const std::uint32_t maxLevel = (1u << bits) - 1u;
    q.pos.resize(mesh.vertices.size() * 3);
    q.uv.resize(mesh.vertices.size() * 2);
    for (std::size_t vi = 0; vi < mesh.vertices.size(); ++vi) {
        const MeshVertex& v = mesh.vertices[vi];
        q.pos[vi * 3 + 0] = detail::quantChannel(v.px, q.posMin[0], q.posExtent[0], maxLevel);
        q.pos[vi * 3 + 1] = detail::quantChannel(v.py, q.posMin[1], q.posExtent[1], maxLevel);
        q.pos[vi * 3 + 2] = detail::quantChannel(v.pz, q.posMin[2], q.posExtent[2], maxLevel);
        q.uv[vi * 2 + 0] = detail::quantChannel(v.u, q.uvMin[0], q.uvExtent[0], maxLevel);
        q.uv[vi * 2 + 1] = detail::quantChannel(v.v, q.uvMin[1], q.uvExtent[1], maxLevel);
    }
    return q;
}

// Reconstruct a MeshData from a QuantizedMesh. Non-quantized attributes (normals, colours) are lost — this
// core targets position/UV, the channels that dominate size; callers that need normals re-derive them (see
// computeNormals) or extend the quantizer. Positions/UVs land within half a grid step of the originals.
inline shapes::MeshData dequantizeMesh(const QuantizedMesh& q) {
    shapes::MeshData mesh;
    mesh.indices = q.indices;
    if (q.vertexCount == 0) return mesh;
    const std::uint32_t maxLevel = (1u << q.bits) - 1u;
    mesh.vertices.resize(q.vertexCount);
    for (std::uint32_t vi = 0; vi < q.vertexCount; ++vi) {
        MeshVertex& v = mesh.vertices[vi];
        v = MeshVertex{};
        v.px = detail::dequantChannel(q.pos[vi * 3 + 0], q.posMin[0], q.posExtent[0], maxLevel);
        v.py = detail::dequantChannel(q.pos[vi * 3 + 1], q.posMin[1], q.posExtent[1], maxLevel);
        v.pz = detail::dequantChannel(q.pos[vi * 3 + 2], q.posMin[2], q.posExtent[2], maxLevel);
        v.u = detail::dequantChannel(q.uv[vi * 2 + 0], q.uvMin[0], q.uvExtent[0], maxLevel);
        v.v = detail::dequantChannel(q.uv[vi * 2 + 1], q.uvMin[1], q.uvExtent[1], maxLevel);
        v.r = v.g = v.b = 1.0f;
    }
    return mesh;
}

} // namespace maz::render
