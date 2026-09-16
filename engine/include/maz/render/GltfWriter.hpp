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

// One part of a multi-material glTF export: its geometry plus a pbrMetallicRoughness material. Used by
// encodeGlbParts so a composed asset (editor::bakeSegments) keeps each part's colour / roughness /
// metallic / emissive as a real glTF material that carries into other tools.
struct GlbPart {
    const shapes::MeshData* mesh = nullptr;
    float baseColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float metallic = 0.0f;
    float roughness = 0.6f;
    float emissive[3] = {0.0f, 0.0f, 0.0f};
};

// Serialize several parts as a single glTF mesh with one primitive + one pbrMetallicRoughness material
// per part (COLOR_0 vertex colours are also written, so importers that ignore material factors — Maz's
// own loadGltf — still show the tint). Returns empty if no part has geometry. Parts with fewer than 3
// indices or a null/empty mesh are skipped.
inline std::vector<std::uint8_t> encodeGlbParts(const std::vector<GlbPart>& parts) {
    std::vector<std::uint8_t> bin;
    std::string prims;    // mesh.primitives[] entries
    std::string mats;     // materials[] entries
    std::string accs;     // accessors[] entries
    std::string views;    // bufferViews[] entries
    std::size_t accCount = 0;
    std::size_t matCount = 0;
    std::size_t byteOffset = 0;

    for (const GlbPart& part : parts) {
        if (part.mesh == nullptr || part.mesh->vertices.empty() || part.mesh->indices.size() < 3) {
            continue;
        }
        const shapes::MeshData& mesh = *part.mesh;
        const std::size_t n = mesh.vertices.size();
        const std::size_t m = mesh.indices.size();

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
            detail::gltfPushF32(bin, 1.0f);
        }
        for (std::uint32_t idx : mesh.indices) {
            detail::gltfPushU32(bin, idx);
        }

        const std::size_t posOff = byteOffset;
        const std::size_t normOff = posOff + n * 12u;
        const std::size_t colOff = normOff + n * 12u;
        const std::size_t idxOff = colOff + n * 16u;
        byteOffset = idxOff + m * 4u;

        const std::size_t aPos = accCount, aNorm = accCount + 1, aCol = accCount + 2,
                          aIdx = accCount + 3;
        // Buffer views (one per attribute + indices), matching the accessor indices.
        auto view = [&](std::size_t off, std::size_t len, int target) {
            if (!views.empty()) views += ',';
            views += R"({"buffer":0,"byteOffset":)";
            detail::gltfAppendU(views, off);
            views += R"(,"byteLength":)";
            detail::gltfAppendU(views, len);
            views += R"(,"target":)";
            detail::gltfAppendU(views, static_cast<std::size_t>(target));
            views += '}';
        };
        view(posOff, n * 12u, 34962);
        view(normOff, n * 12u, 34962);
        view(colOff, n * 16u, 34962);
        view(idxOff, m * 4u, 34963);

        if (!accs.empty()) accs += ',';
        accs += R"({"bufferView":)";
        detail::gltfAppendU(accs, aPos);
        accs += R"(,"componentType":5126,"count":)";
        detail::gltfAppendU(accs, n);
        accs += R"(,"type":"VEC3","min":[)";
        detail::gltfAppendNum(accs, mn[0]); accs += ','; detail::gltfAppendNum(accs, mn[1]);
        accs += ','; detail::gltfAppendNum(accs, mn[2]);
        accs += R"(],"max":[)";
        detail::gltfAppendNum(accs, mx[0]); accs += ','; detail::gltfAppendNum(accs, mx[1]);
        accs += ','; detail::gltfAppendNum(accs, mx[2]);
        accs += "]},";
        accs += R"({"bufferView":)";
        detail::gltfAppendU(accs, aNorm);
        accs += R"(,"componentType":5126,"count":)";
        detail::gltfAppendU(accs, n);
        accs += R"(,"type":"VEC3"},)";
        accs += R"({"bufferView":)";
        detail::gltfAppendU(accs, aCol);
        accs += R"(,"componentType":5126,"count":)";
        detail::gltfAppendU(accs, n);
        accs += R"(,"type":"VEC4"},)";
        accs += R"({"bufferView":)";
        detail::gltfAppendU(accs, aIdx);
        accs += R"(,"componentType":5125,"count":)";
        detail::gltfAppendU(accs, m);
        accs += R"(,"type":"SCALAR"})";

        if (!mats.empty()) mats += ',';
        mats += R"({"pbrMetallicRoughness":{"baseColorFactor":[)";
        detail::gltfAppendNum(mats, part.baseColor[0]); mats += ',';
        detail::gltfAppendNum(mats, part.baseColor[1]); mats += ',';
        detail::gltfAppendNum(mats, part.baseColor[2]); mats += ',';
        detail::gltfAppendNum(mats, part.baseColor[3]);
        mats += R"(],"metallicFactor":)";
        detail::gltfAppendNum(mats, part.metallic);
        mats += R"(,"roughnessFactor":)";
        detail::gltfAppendNum(mats, part.roughness);
        mats += R"(},"emissiveFactor":[)";
        detail::gltfAppendNum(mats, part.emissive[0]); mats += ',';
        detail::gltfAppendNum(mats, part.emissive[1]); mats += ',';
        detail::gltfAppendNum(mats, part.emissive[2]);
        mats += "]}";

        if (!prims.empty()) prims += ',';
        prims += R"({"attributes":{"POSITION":)";
        detail::gltfAppendU(prims, aPos);
        prims += R"(,"NORMAL":)";
        detail::gltfAppendU(prims, aNorm);
        prims += R"(,"COLOR_0":)";
        detail::gltfAppendU(prims, aCol);
        prims += R"(},"indices":)";
        detail::gltfAppendU(prims, aIdx);
        prims += R"(,"material":)";
        detail::gltfAppendU(prims, matCount);
        prims += R"(,"mode":4})";

        accCount += 4;
        ++matCount;
    }

    if (bin.empty()) {
        return {};
    }

    std::string j = R"({"asset":{"version":"2.0","generator":"Maz"},)";
    j += R"("scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0}],)";
    j += R"("meshes":[{"primitives":[)";
    j += prims;
    j += R"(]}],"materials":[)";
    j += mats;
    j += R"(],"accessors":[)";
    j += accs;
    j += R"(],"bufferViews":[)";
    j += views;
    j += R"(],"buffers":[{"byteLength":)";
    detail::gltfAppendU(j, bin.size());
    j += "}]}";

    return buildGlb(j, bin);
}

} // namespace maz::render
