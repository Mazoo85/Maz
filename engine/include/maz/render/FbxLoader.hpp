#pragma once

#include "maz/render/Shapes.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// maz::render FBX (.fbx) geometry importer — closes the biggest remaining import gap versus Godot's asset
// pipeline. FBX is the de-facto interchange format out of Maya/3ds Max/Blender/mixamo, so reading even its
// geometry lets those tools' meshes drop straight into Maz. This importer handles the **ASCII** FBX form:
// it locates the mesh's `Vertices` array (flat x,y,z triples) and its `PolygonVertexIndex` array (FBX packs
// each polygon's final index as the bitwise-complement, ~i, i.e. a negative value, to mark the polygon
// boundary — so polygons of any size are delimited without a separate count), fan-triangulates every
// polygon, and emits a `shapes::MeshData` (three vertices per triangle, sequential indices) ready for
// `Renderer::createMesh`. Pure CPU string/number work — no GPU — so it unit-tests headlessly from an
// in-memory buffer; `loadFbx` wraps it for files.
//
// Scope note (honest): ASCII FBX, geometry of the FIRST mesh only, positions + fan-triangulation. Normals
// are derived from the winding (FBX's LayerElementNormal mapping/reference modes are not yet decoded), and
// UVs/materials/skins/animation are out of scope for this milestone. Binary FBX, multi-mesh scenes, and
// LayerElement normals/UVs are documented follow-ups. Malformed or non-mesh input returns false.
namespace maz::render {

struct FbxLoadOptions {
    float r = 1.0f, g = 1.0f, b = 1.0f; // vertex tint (this importer does not decode FBX materials yet)
};

namespace detail {

// Extract the numeric payload of an FBX property array: given a key like "Vertices" or
// "PolygonVertexIndex", find the following `a:` token and collect every number up to the closing `}`.
// FBX writes these as `Key: *N { a: 1,2,3, ... }`, possibly spanning many lines. Returns false if the key
// or its array is absent.
inline bool fbxGrabArray(const std::string& s, const std::string& key, std::vector<double>& out) {
    out.clear();
    std::size_t k = s.find(key);
    while (k != std::string::npos) {
        // Ensure the key is followed (after optional spaces) by ':' so we match a property, not a substring.
        std::size_t c = s.find(':', k);
        if (c != std::string::npos) {
            const std::string between = s.substr(k + key.size(), c - (k + key.size()));
            bool onlySpace = true;
            for (char ch : between) {
                if (ch != ' ' && ch != '\t') { onlySpace = false; break; }
            }
            if (onlySpace) break;
        }
        k = s.find(key, k + key.size());
    }
    if (k == std::string::npos) return false;

    const std::size_t a = s.find("a:", k);
    if (a == std::string::npos) return false;
    const std::size_t end = s.find('}', a);
    const std::size_t stop = (end == std::string::npos) ? s.size() : end;

    const char* p = s.c_str() + a + 2;      // just past "a:"
    const char* e = s.c_str() + stop;
    while (p < e) {
        // Skip anything that can't start a number.
        while (p < e && *p != '-' && *p != '+' && *p != '.' &&
               !(*p >= '0' && *p <= '9')) {
            ++p;
        }
        if (p >= e) break;
        char* next = nullptr;
        const double v = std::strtod(p, &next);
        if (next == p) { ++p; continue; }   // not actually a number, advance
        out.push_back(v);
        p = next;
    }
    return !out.empty();
}

} // namespace detail

// Parse ASCII FBX text into geometry. Returns false if no mesh geometry is found.
inline bool parseFbxAscii(const std::string& text, shapes::MeshData& out,
                          const FbxLoadOptions& opt = {}) {
    out.vertices.clear();
    out.indices.clear();

    std::vector<double> verts;
    std::vector<double> polyIdx;
    if (!detail::fbxGrabArray(text, "Vertices", verts)) return false;
    if (!detail::fbxGrabArray(text, "PolygonVertexIndex", polyIdx)) return false;
    if (verts.size() < 9 || (verts.size() % 3) != 0) return false; // need at least one triangle's worth

    const std::size_t vertCount = verts.size() / 3;

    auto pos = [&](int idx, float& x, float& y, float& z) -> bool {
        if (idx < 0 || static_cast<std::size_t>(idx) >= vertCount) return false;
        const std::size_t o = static_cast<std::size_t>(idx) * 3;
        x = static_cast<float>(verts[o]);
        y = static_cast<float>(verts[o + 1]);
        z = static_cast<float>(verts[o + 2]);
        return true;
    };

    auto emitTri = [&](int ia, int ib, int ic) {
        float ax, ay, az, bx, by, bz, cx, cy, cz;
        if (!pos(ia, ax, ay, az) || !pos(ib, bx, by, bz) || !pos(ic, cx, cy, cz)) return;
        // Face normal from CCW winding.
        const float ux = bx - ax, uy = by - ay, uz = bz - az;
        const float wx = cx - ax, wy = cy - ay, wz = cz - az;
        float nx = uy * wz - uz * wy;
        float ny = uz * wx - ux * wz;
        float nz = ux * wy - uy * wx;
        const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 1e-12f) { nx /= len; ny /= len; nz /= len; }
        const std::uint32_t base = static_cast<std::uint32_t>(out.vertices.size());
        const float px[3] = {ax, bx, cx}, py[3] = {ay, by, cy}, pz[3] = {az, bz, cz};
        for (int i = 0; i < 3; ++i) {
            MeshVertex v{};
            v.px = px[i]; v.py = py[i]; v.pz = pz[i];
            v.nx = nx; v.ny = ny; v.nz = nz;
            v.r = opt.r; v.g = opt.g; v.b = opt.b;
            v.u = 0.0f; v.v = 0.0f;
            out.vertices.push_back(v);
            out.indices.push_back(base + static_cast<std::uint32_t>(i));
        }
    };

    // Walk the polygon-index stream. A negative value marks the last index of a polygon (real = ~value),
    // so accumulate a polygon then fan-triangulate it.
    std::vector<int> poly;
    for (double d : polyIdx) {
        int idx = static_cast<int>(d);
        bool last = false;
        if (idx < 0) { idx = -idx - 1; last = true; } // ~idx == -idx-1
        poly.push_back(idx);
        if (last) {
            for (std::size_t i = 1; i + 1 < poly.size(); ++i) {
                emitTri(poly[0], poly[i], poly[i + 1]);
            }
            poly.clear();
        }
    }

    return !out.vertices.empty();
}

// Load and parse an ASCII .fbx file. Returns false on read/parse failure.
inline bool loadFbx(const std::string& path, shapes::MeshData& out, const FbxLoadOptions& opt = {}) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    return parseFbxAscii(ss.str(), out, opt);
}

} // namespace maz::render
