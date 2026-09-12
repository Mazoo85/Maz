#pragma once

#include "maz/render/Shapes.hpp"

#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

// maz::render STL (.stl) mesh importer — closes another import gap versus Godot's asset pipeline. STL is
// the universal interchange format for 3D printing and CAD (SolidWorks, Blender, Meshmixer, every slicer),
// storing a flat triangle soup: each triangle carries a face normal and its three corner positions, with no
// shared vertices, UVs, or materials. `parseStl` reads BOTH encodings — the ASCII form (`solid` /
// `facet normal` / `vertex` keywords) and the binary form (80-byte header + uint32 triangle count + 50-byte
// records) — auto-detecting which one a buffer is, and emits a `shapes::MeshData` (three vertices per
// triangle, sequential indices) ready for `Renderer::createMesh`. Pure CPU byte/string work — no GPU — so it
// unit-tests headlessly from an in-memory buffer; `loadStl` wraps it for files.
//
// Scope note (honest): triangle geometry + per-face normals only. STL stores no texture coordinates and no
// standard color, so UVs are zero and every vertex takes the fallback tint; the rare per-face color found in
// some binary "attribute byte count" extensions is not decoded. Because STL normals are frequently absent or
// wrong, `recomputeNormals` (and the automatic fallback when a stored normal is zero-length) derives the
// geometric normal from the winding. Malformed input returns false rather than throwing.
namespace maz::render {

struct StlLoadOptions {
    float r = 1.0f, g = 1.0f, b = 1.0f; // vertex tint (STL carries no standard color)
    bool recomputeNormals = false;      // ignore stored normals, derive from triangle winding (CCW)
};

namespace detail {

// Binary STL detection: a binary file is exactly 84 + 50*triangleCount bytes (80-byte header + 4-byte count
// + 50 bytes per triangle). ASCII files never satisfy that identity for the count stored at offset 80, so it
// is the standard, reliable discriminator — more robust than sniffing for a leading "solid" (binary headers
// can legally begin with those bytes too).
inline bool stlIsBinary(const std::uint8_t* data, std::size_t size) {
    if (size < 84) return false;
    std::uint32_t tris = static_cast<std::uint32_t>(data[80]) | (static_cast<std::uint32_t>(data[81]) << 8) |
                         (static_cast<std::uint32_t>(data[82]) << 16) |
                         (static_cast<std::uint32_t>(data[83]) << 24);
    return static_cast<std::size_t>(84) + static_cast<std::size_t>(tris) * 50u == size;
}

// Read a 32-bit little-endian float from a byte cursor.
inline float stlF32le(const std::uint8_t* p) {
    std::uint32_t bits = static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
                         (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

// Append one triangle (three positions + a shared normal) as three MeshVertices. When the stored normal is
// zero-length or `recompute` is set, derive it from the winding (CCW front face).
inline void stlEmitTriangle(shapes::MeshData& out, const float p0[3], const float p1[3], const float p2[3],
                            float nx, float ny, float nz, const StlLoadOptions& opts, bool recompute) {
    if (recompute || (nx == 0.0f && ny == 0.0f && nz == 0.0f)) {
        const float ax = p1[0] - p0[0], ay = p1[1] - p0[1], az = p1[2] - p0[2];
        const float bx = p2[0] - p0[0], by = p2[1] - p0[1], bz = p2[2] - p0[2];
        nx = ay * bz - az * by;
        ny = az * bx - ax * bz;
        nz = ax * by - ay * bx;
    }
    const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len > 0.0f) {
        nx /= len;
        ny /= len;
        nz /= len;
    }
    const float* verts[3] = {p0, p1, p2};
    for (const float* v : verts) {
        MeshVertex mv{};
        mv.px = v[0];
        mv.py = v[1];
        mv.pz = v[2];
        mv.nx = nx;
        mv.ny = ny;
        mv.nz = nz;
        mv.r = opts.r;
        mv.g = opts.g;
        mv.b = opts.b;
        mv.u = 0.0f;
        mv.v = 0.0f;
        out.indices.push_back(static_cast<std::uint32_t>(out.vertices.size()));
        out.vertices.push_back(mv);
    }
}

} // namespace detail

inline bool parseStl(const std::uint8_t* data, std::size_t size, shapes::MeshData& out,
                     const StlLoadOptions& opts = {}) {
    out.vertices.clear();
    out.indices.clear();
    if (!data || size == 0) return false;

    if (detail::stlIsBinary(data, size)) {
        std::uint32_t tris = static_cast<std::uint32_t>(data[80]) |
                             (static_cast<std::uint32_t>(data[81]) << 8) |
                             (static_cast<std::uint32_t>(data[82]) << 16) |
                             (static_cast<std::uint32_t>(data[83]) << 24);
        std::size_t off = 84;
        for (std::uint32_t t = 0; t < tris; ++t) {
            const std::uint8_t* rec = data + off;
            const float nx = detail::stlF32le(rec + 0);
            const float ny = detail::stlF32le(rec + 4);
            const float nz = detail::stlF32le(rec + 8);
            float p0[3] = {detail::stlF32le(rec + 12), detail::stlF32le(rec + 16), detail::stlF32le(rec + 20)};
            float p1[3] = {detail::stlF32le(rec + 24), detail::stlF32le(rec + 28), detail::stlF32le(rec + 32)};
            float p2[3] = {detail::stlF32le(rec + 36), detail::stlF32le(rec + 40), detail::stlF32le(rec + 44)};
            detail::stlEmitTriangle(out, p0, p1, p2, nx, ny, nz, opts, opts.recomputeNormals);
            off += 50; // 48 float bytes + 2-byte attribute count
        }
        return !out.vertices.empty();
    }

    // ASCII: token stream. Collect the facet normal, then three vertex positions, then emit on the third.
    const char* text = reinterpret_cast<const char*>(data);
    std::size_t i = 0;
    auto nextToken = [&](std::string& tok) -> bool {
        while (i < size && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
        if (i >= size) return false;
        std::size_t start = i;
        while (i < size && !std::isspace(static_cast<unsigned char>(text[i]))) ++i;
        tok.assign(text + start, i - start);
        return true;
    };
    auto readFloat = [&](float& f) -> bool {
        std::string tok;
        if (!nextToken(tok)) return false;
        char* end = nullptr;
        f = std::strtof(tok.c_str(), &end);
        return end != tok.c_str();
    };

    float normal[3] = {0.0f, 0.0f, 0.0f};
    float pts[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    int have = 0; // vertices collected for the current facet
    std::string tok;
    bool sawFacet = false;
    while (nextToken(tok)) {
        if (tok == "facet") {
            // Optional "normal ni nj nk".
            std::size_t save = i;
            std::string maybe;
            if (nextToken(maybe) && maybe == "normal") {
                if (!(readFloat(normal[0]) && readFloat(normal[1]) && readFloat(normal[2]))) return false;
            } else {
                i = save;
                normal[0] = normal[1] = normal[2] = 0.0f;
            }
            have = 0;
            sawFacet = true;
        } else if (tok == "vertex") {
            if (have >= 3) return false; // more than a triangle per facet — unsupported
            if (!(readFloat(pts[have][0]) && readFloat(pts[have][1]) && readFloat(pts[have][2]))) return false;
            ++have;
        } else if (tok == "endfacet") {
            if (have != 3) return false;
            detail::stlEmitTriangle(out, pts[0], pts[1], pts[2], normal[0], normal[1], normal[2], opts,
                                    opts.recomputeNormals);
            have = 0;
        }
        // All other tokens (solid, outer, loop, endloop, endsolid, names) are ignored.
    }
    if (!sawFacet) return false;
    return !out.vertices.empty();
}

inline bool parseStl(const std::vector<std::uint8_t>& bytes, shapes::MeshData& out,
                     const StlLoadOptions& opts = {}) {
    return parseStl(bytes.data(), bytes.size(), out, opts);
}

inline bool loadStl(const std::string& path, shapes::MeshData& out, const StlLoadOptions& opts = {}) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const std::streamsize size = f.tellg();
    if (size < 0) return false;
    f.seekg(0);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (size > 0) f.read(reinterpret_cast<char*>(bytes.data()), size);
    return parseStl(bytes, out, opts);
}

} // namespace maz::render
