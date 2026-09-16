#pragma once

#include "maz/render/Shapes.hpp" // shapes::MeshData, render::MeshVertex

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

// maz::render Wavefront OBJ writer — the export counterpart to ObjLoader.hpp, so a mesh composed in
// the editor (baked with editor::bakeComposite) can be handed straight to Blender / other DCC tools
// and engines. encodeObj serializes a MeshData to OBJ text: positions `v` (with the widely-supported
// `v x y z r g b` vertex-colour extension so the baked per-part tints survive), texcoords `vt`,
// normals `vn`, and triangle faces `f a/a/a …`. Each MeshVertex is already a unique combined vertex,
// so v/vt/vn share one 1-based index per face corner. Pure text, header-only, unit-tested headlessly;
// a load→save→load round-trip reproduces the geometry (the writer flips V back to match the loader).
namespace maz::render {

struct ObjWriteOptions {
    bool flipV = true;              // undo ObjLoader's top-left V so save->load round-trips OBJ's bottom-left
    bool includeVertexColor = true; // emit `v x y z r g b` (MeshLab/Blender read it; plain viewers ignore it)
    bool includeNormals = true;     // emit `vn` and `f a/a/a` (else `f a/a`)
    std::string objectName = "mesh"; // the `o <name>` group
};

namespace detail {
inline void objAppendFloat(std::string& s, float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.7g", static_cast<double>(v));
    s += buf;
}
inline void objAppendCorner(std::string& s, std::uint32_t oneBased, bool withNormal) {
    char buf[40];
    if (withNormal) {
        std::snprintf(buf, sizeof(buf), "%u/%u/%u", oneBased, oneBased, oneBased);
    } else {
        std::snprintf(buf, sizeof(buf), "%u/%u", oneBased, oneBased);
    }
    s += buf;
}
} // namespace detail

// Serialize `mesh` to Wavefront OBJ text (see the file header). Returns the OBJ document as a string;
// write it with io::writeFile or hand it to any OBJ importer.
inline std::string encodeObj(const shapes::MeshData& mesh, const ObjWriteOptions& opts = {}) {
    std::string s;
    s.reserve(mesh.vertices.size() * 48u + mesh.indices.size() * 12u + 64u);
    s += "# Maz OBJ export\n";
    s += "o ";
    s += opts.objectName;
    s += '\n';

    for (const MeshVertex& v : mesh.vertices) {
        s += "v ";
        detail::objAppendFloat(s, v.px);
        s += ' ';
        detail::objAppendFloat(s, v.py);
        s += ' ';
        detail::objAppendFloat(s, v.pz);
        if (opts.includeVertexColor) {
            s += ' ';
            detail::objAppendFloat(s, v.r);
            s += ' ';
            detail::objAppendFloat(s, v.g);
            s += ' ';
            detail::objAppendFloat(s, v.b);
        }
        s += '\n';
    }
    for (const MeshVertex& v : mesh.vertices) {
        s += "vt ";
        detail::objAppendFloat(s, v.u);
        s += ' ';
        detail::objAppendFloat(s, opts.flipV ? 1.0f - v.v : v.v);
        s += '\n';
    }
    if (opts.includeNormals) {
        for (const MeshVertex& v : mesh.vertices) {
            s += "vn ";
            detail::objAppendFloat(s, v.nx);
            s += ' ';
            detail::objAppendFloat(s, v.ny);
            s += ' ';
            detail::objAppendFloat(s, v.nz);
            s += '\n';
        }
    }

    const std::size_t vertCount = mesh.vertices.size();
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const std::uint32_t a = mesh.indices[i];
        const std::uint32_t b = mesh.indices[i + 1];
        const std::uint32_t c = mesh.indices[i + 2];
        if (a >= vertCount || b >= vertCount || c >= vertCount) {
            continue; // skip a malformed face rather than emit an out-of-range index
        }
        s += "f ";
        detail::objAppendCorner(s, a + 1u, opts.includeNormals);
        s += ' ';
        detail::objAppendCorner(s, b + 1u, opts.includeNormals);
        s += ' ';
        detail::objAppendCorner(s, c + 1u, opts.includeNormals);
        s += '\n';
    }
    return s;
}

} // namespace maz::render
