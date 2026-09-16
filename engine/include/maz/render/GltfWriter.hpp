#pragma once

#include "maz/render/GlbContainer.hpp" // buildGlb
#include "maz/render/Shapes.hpp"       // shapes::MeshData, render::MeshVertex

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// maz::render binary-glTF (.glb) writer — the export counterpart to render::loadGltf, so a mesh
// composed in the editor (baked with editor::bakeComposite) can be handed to any glTF tool/engine in
// the modern standard format. Unlike OBJ, glTF carries per-vertex colour natively (COLOR_0), so the
// baked per-part tints survive cleanly. encodeGlb serialises one MeshData as a single-node, single-
// mesh glTF 2.0 document: POSITION + NORMAL + COLOR_0 float accessors and a UNSIGNED_INT index
// accessor into one binary buffer, wrapped in the GLB container by buildGlb. Pure bytes, header-only,
// unit-tested headlessly (the emitted GLB re-parses with parseGlb and re-imports with loadGltf).
namespace maz::render {

namespace detail {
inline void gltfPushF32(std::vector<std::uint8_t>& b, float f) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &f, 4);
    b.push_back(static_cast<std::uint8_t>(bits & 0xffu));
    b.push_back(static_cast<std::uint8_t>((bits >> 8) & 0xffu));
    b.push_back(static_cast<std::uint8_t>((bits >> 16) & 0xffu));
    b.push_back(static_cast<std::uint8_t>((bits >> 24) & 0xffu));
}
inline void gltfPushU32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    b.push_back(static_cast<std::uint8_t>(v & 0xffu));
    b.push_back(static_cast<std::uint8_t>((v >> 8) & 0xffu));
    b.push_back(static_cast<std::uint8_t>((v >> 16) & 0xffu));
    b.push_back(static_cast<std::uint8_t>((v >> 24) & 0xffu));
}
inline void gltfAppendNum(std::string& s, double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.9g", v);
    s += buf;
}
inline void gltfAppendU(std::string& s, std::size_t v) {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%zu", v);
    s += buf;
}
} // namespace detail

// Serialize `mesh` to a self-contained binary glTF (.glb) byte buffer. Returns empty if the mesh has
// no geometry. Write the result with io::writeFile, or hand it to any glTF 2.0 importer.
inline std::vector<std::uint8_t> encodeGlb(const shapes::MeshData& mesh) {
    if (mesh.vertices.empty() || mesh.indices.size() < 3) {
        return {};
    }
    const std::size_t n = mesh.vertices.size();
    const std::size_t m = mesh.indices.size();

    // BIN blob: [positions vec3][normals vec3][colors vec4][indices u32], each already 4-byte aligned.
    std::vector<std::uint8_t> bin;
    bin.reserve(n * 40u + m * 4u);
    float mn[3] = {mesh.vertices[0].px, mesh.vertices[0].py, mesh.vertices[0].pz};
    float mx[3] = {mn[0], mn[1], mn[2]};
    for (const MeshVertex& v : mesh.vertices) {
        detail::gltfPushF32(bin, v.px);
        detail::gltfPushF32(bin, v.py);
        detail::gltfPushF32(bin, v.pz);
        mn[0] = v.px < mn[0] ? v.px : mn[0];
        mn[1] = v.py < mn[1] ? v.py : mn[1];
        mn[2] = v.pz < mn[2] ? v.pz : mn[2];
        mx[0] = v.px > mx[0] ? v.px : mx[0];
        mx[1] = v.py > mx[1] ? v.py : mx[1];
        mx[2] = v.pz > mx[2] ? v.pz : mx[2];
    }
    for (const MeshVertex& v : mesh.vertices) {
        detail::gltfPushF32(bin, v.nx);
        detail::gltfPushF32(bin, v.ny);
        detail::gltfPushF32(bin, v.nz);
    }
    for (const MeshVertex& v : mesh.vertices) {
        detail::gltfPushF32(bin, v.r);
        detail::gltfPushF32(bin, v.g);
        detail::gltfPushF32(bin, v.b);
        detail::gltfPushF32(bin, 1.0f); // COLOR_0 is VEC4; alpha 1
    }
    for (std::uint32_t idx : mesh.indices) {
        detail::gltfPushU32(bin, idx);
    }

    const std::size_t posBytes = n * 12u;
    const std::size_t normBytes = n * 12u;
    const std::size_t colBytes = n * 16u;
    const std::size_t idxBytes = m * 4u;
    const std::size_t total = posBytes + normBytes + colBytes + idxBytes;

    // Minimal glTF 2.0 JSON: one scene -> node -> mesh, four accessors over four buffer views.
    std::string j = R"({"asset":{"version":"2.0","generator":"Maz"},)";
    j += R"("scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0}],)";
    j += R"("meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"COLOR_0":2},"indices":3,"mode":4}]}],)";

    j += R"("accessors":[)";
    // 0: POSITION (needs min/max per spec)
    j += R"({"bufferView":0,"componentType":5126,"count":)";
    detail::gltfAppendU(j, n);
    j += R"(,"type":"VEC3","min":[)";
    detail::gltfAppendNum(j, mn[0]); j += ','; detail::gltfAppendNum(j, mn[1]); j += ',';
    detail::gltfAppendNum(j, mn[2]);
    j += R"(],"max":[)";
    detail::gltfAppendNum(j, mx[0]); j += ','; detail::gltfAppendNum(j, mx[1]); j += ',';
    detail::gltfAppendNum(j, mx[2]);
    j += "]},";
    // 1: NORMAL
    j += R"({"bufferView":1,"componentType":5126,"count":)";
    detail::gltfAppendU(j, n);
    j += R"(,"type":"VEC3"},)";
    // 2: COLOR_0
    j += R"({"bufferView":2,"componentType":5126,"count":)";
    detail::gltfAppendU(j, n);
    j += R"(,"type":"VEC4"},)";
    // 3: indices
    j += R"({"bufferView":3,"componentType":5125,"count":)";
    detail::gltfAppendU(j, m);
    j += R"(,"type":"SCALAR"}],)";

    j += R"("bufferViews":[)";
    j += R"({"buffer":0,"byteOffset":0,"byteLength":)";
    detail::gltfAppendU(j, posBytes);
    j += R"(,"target":34962},)";
    j += R"({"buffer":0,"byteOffset":)";
    detail::gltfAppendU(j, posBytes);
    j += R"(,"byteLength":)";
    detail::gltfAppendU(j, normBytes);
    j += R"(,"target":34962},)";
    j += R"({"buffer":0,"byteOffset":)";
    detail::gltfAppendU(j, posBytes + normBytes);
    j += R"(,"byteLength":)";
    detail::gltfAppendU(j, colBytes);
    j += R"(,"target":34962},)";
    j += R"({"buffer":0,"byteOffset":)";
    detail::gltfAppendU(j, posBytes + normBytes + colBytes);
    j += R"(,"byteLength":)";
    detail::gltfAppendU(j, idxBytes);
    j += R"(,"target":34963}],)";

    j += R"("buffers":[{"byteLength":)";
    detail::gltfAppendU(j, total);
    j += "}]}";

    return buildGlb(j, bin);
}

} // namespace maz::render
