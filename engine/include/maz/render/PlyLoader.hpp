#pragma once

#include "maz/render/Shapes.hpp"

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

// maz::render PLY (Stanford .ply) mesh importer — closes another import gap versus Godot's asset
// pipeline. PLY is the standard output of 3D scanners and tools like MeshLab/CloudCompare, storing a
// vertex list (position, optional normal / color / texcoord) and a face list. `parsePly` reads both
// the ASCII and binary (little- and big-endian) encodings into a `shapes::MeshData` ready for
// `Renderer::createMesh`, mapping the usual property names (x/y/z, nx/ny/nz, red/green/blue,
// s/t or u/v) and fan-triangulating faces with more than three vertices. It is pure CPU byte/string
// work — no GPU — so it unit-tests headlessly from an in-memory buffer; `loadPly` wraps it for files.
//
// Scope note (honest): reads the common `vertex` + `face` elements with scalar vertex properties and a
// single face index list. It does not interpret custom elements, edge lists, or material blocks; unknown
// vertex properties are skipped by type. Malformed input returns false rather than throwing.
namespace maz::render {

struct PlyLoadOptions {
    bool flipV = false;                 // PLY has no universal V convention; flip if your texcoords need it
    float r = 1.0f, g = 1.0f, b = 1.0f; // fallback tint when the file carries no vertex color
};

namespace detail {

enum class PlyType { I8, U8, I16, U16, I32, U32, F32, F64, None };

inline PlyType plyType(const std::string& s) {
    if (s == "char" || s == "int8") return PlyType::I8;
    if (s == "uchar" || s == "uint8") return PlyType::U8;
    if (s == "short" || s == "int16") return PlyType::I16;
    if (s == "ushort" || s == "uint16") return PlyType::U16;
    if (s == "int" || s == "int32") return PlyType::I32;
    if (s == "uint" || s == "uint32") return PlyType::U32;
    if (s == "float" || s == "float32") return PlyType::F32;
    if (s == "double" || s == "float64") return PlyType::F64;
    return PlyType::None;
}

inline int plySize(PlyType t) {
    switch (t) {
        case PlyType::I8:
        case PlyType::U8: return 1;
        case PlyType::I16:
        case PlyType::U16: return 2;
        case PlyType::I32:
        case PlyType::U32:
        case PlyType::F32: return 4;
        case PlyType::F64: return 8;
        default: return 0;
    }
}

inline bool plyIsFloat(PlyType t) { return t == PlyType::F32 || t == PlyType::F64; }

// A semantic slot a vertex property maps to.
enum class PlySem { X, Y, Z, NX, NY, NZ, R, G, B, S, T, Ignore };

inline PlySem plySem(const std::string& n) {
    if (n == "x") return PlySem::X;
    if (n == "y") return PlySem::Y;
    if (n == "z") return PlySem::Z;
    if (n == "nx") return PlySem::NX;
    if (n == "ny") return PlySem::NY;
    if (n == "nz") return PlySem::NZ;
    if (n == "red" || n == "r") return PlySem::R;
    if (n == "green" || n == "g") return PlySem::G;
    if (n == "blue" || n == "b") return PlySem::B;
    if (n == "s" || n == "u" || n == "texture_u") return PlySem::S;
    if (n == "t" || n == "v" || n == "texture_v") return PlySem::T;
    return PlySem::Ignore;
}

// Sequential reader that pulls typed scalars from either an ASCII token stream or a binary byte cursor.
struct PlyReader {
    bool ascii = true;
    bool bigEndian = false;
    // ascii
    const std::string* text = nullptr;
    std::size_t pos = 0;
    // binary
    const unsigned char* bytes = nullptr;
    std::size_t cursor = 0;
    std::size_t size = 0;
    bool error = false;

    double next(PlyType t) {
        if (ascii) {
            while (pos < text->size() && (std::isspace(static_cast<unsigned char>((*text)[pos])))) ++pos;
            if (pos >= text->size()) {
                error = true;
                return 0.0;
            }
            const char* start = text->c_str() + pos;
            char* end = nullptr;
            const double v = std::strtod(start, &end);
            if (end == start) {
                error = true;
                return 0.0;
            }
            pos += static_cast<std::size_t>(end - start);
            return v;
        }
        const int n = plySize(t);
        if (n == 0 || cursor + static_cast<std::size_t>(n) > size) {
            error = true;
            return 0.0;
        }
        unsigned char buf[8];
        for (int i = 0; i < n; ++i) buf[i] = bytes[cursor + static_cast<std::size_t>(i)];
        if (bigEndian) {
            for (int i = 0; i < n / 2; ++i) {
                const unsigned char tmp = buf[i];
                buf[i] = buf[n - 1 - i];
                buf[n - 1 - i] = tmp;
            }
        }
        cursor += static_cast<std::size_t>(n);
        switch (t) {
            case PlyType::I8: return static_cast<double>(static_cast<std::int8_t>(buf[0]));
            case PlyType::U8: return static_cast<double>(buf[0]);
            case PlyType::I16: {
                std::int16_t v;
                std::memcpy(&v, buf, 2);
                return static_cast<double>(v);
            }
            case PlyType::U16: {
                std::uint16_t v;
                std::memcpy(&v, buf, 2);
                return static_cast<double>(v);
            }
            case PlyType::I32: {
                std::int32_t v;
                std::memcpy(&v, buf, 4);
                return static_cast<double>(v);
            }
            case PlyType::U32: {
                std::uint32_t v;
                std::memcpy(&v, buf, 4);
                return static_cast<double>(v);
            }
            case PlyType::F32: {
                float v;
                std::memcpy(&v, buf, 4);
                return static_cast<double>(v);
            }
            case PlyType::F64: {
                double v;
                std::memcpy(&v, buf, 8);
                return v;
            }
            default: error = true; return 0.0;
        }
    }
};

struct PlyProp {
    PlyType type = PlyType::None;
    PlySem sem = PlySem::Ignore;
};

} // namespace detail

inline bool parsePly(const std::string& bytes, shapes::MeshData& out, const PlyLoadOptions& opts = {}) {
    using namespace detail;
    out.vertices.clear();
    out.indices.clear();

    // --- Locate the end of the header (the line "end_header"). ---
    const std::size_t hdrEnd = bytes.find("end_header");
    if (bytes.compare(0, 3, "ply") != 0 || hdrEnd == std::string::npos) return false;
    std::size_t dataStart = bytes.find('\n', hdrEnd);
    if (dataStart == std::string::npos) return false;
    ++dataStart; // first byte after the end_header newline

    // --- Parse the header text line by line. ---
    bool ascii = true;
    bool bigEndian = false;
    long vertexCount = 0;
    long faceCount = 0;
    std::vector<PlyProp> vprops;
    PlyType faceCountType = PlyType::U8;
    PlyType faceIndexType = PlyType::I32;
    enum class Elem { None, Vertex, Face } cur = Elem::None;

    std::size_t lp = 0;
    const std::string header = bytes.substr(0, dataStart);
    while (lp < header.size()) {
        std::size_t nl = header.find('\n', lp);
        if (nl == std::string::npos) nl = header.size();
        std::string line = header.substr(lp, nl - lp);
        lp = nl + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        // tokenize
        std::vector<std::string> tok;
        std::size_t i = 0;
        while (i < line.size()) {
            while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
            std::size_t j = i;
            while (j < line.size() && !std::isspace(static_cast<unsigned char>(line[j]))) ++j;
            if (j > i) tok.push_back(line.substr(i, j - i));
            i = j;
        }
        if (tok.empty()) continue;
        if (tok[0] == "format" && tok.size() >= 2) {
            ascii = (tok[1] == "ascii");
            bigEndian = (tok[1] == "binary_big_endian");
        } else if (tok[0] == "element" && tok.size() >= 3) {
            if (tok[1] == "vertex") {
                cur = Elem::Vertex;
                vertexCount = std::strtol(tok[2].c_str(), nullptr, 10);
            } else if (tok[1] == "face") {
                cur = Elem::Face;
                faceCount = std::strtol(tok[2].c_str(), nullptr, 10);
            } else {
                cur = Elem::None;
            }
        } else if (tok[0] == "property") {
            if (cur == Elem::Vertex && tok.size() >= 3) {
                PlyProp p;
                p.type = plyType(tok[1]);
                p.sem = plySem(tok[2]);
                vprops.push_back(p);
            } else if (cur == Elem::Face && tok.size() >= 5 && tok[1] == "list") {
                faceCountType = plyType(tok[2]);
                faceIndexType = plyType(tok[3]);
            }
        } else if (tok[0] == "end_header") {
            break;
        }
    }
    if (vertexCount < 0 || faceCount < 0 || vprops.empty()) return false;
    // Guard against a corrupt/hostile header declaring an enormous element count: each vertex or face needs
    // at least one byte of payload, so a count larger than the whole input can never be legitimate. Reject
    // it rather than reserve()-ing gigabytes and OOM-crashing on a malicious .ply.
    if (static_cast<std::size_t>(vertexCount) > bytes.size() ||
        static_cast<std::size_t>(faceCount) > bytes.size()) {
        return false;
    }

    // --- Read the data. ---
    PlyReader rd;
    rd.ascii = ascii;
    rd.bigEndian = bigEndian;
    if (ascii) {
        rd.text = &bytes;
        rd.pos = dataStart;
    } else {
        rd.bytes = reinterpret_cast<const unsigned char*>(bytes.data());
        rd.cursor = dataStart;
        rd.size = bytes.size();
    }

    out.vertices.reserve(static_cast<std::size_t>(vertexCount));
    for (long v = 0; v < vertexCount; ++v) {
        MeshVertex mv{};
        mv.r = opts.r;
        mv.g = opts.g;
        mv.b = opts.b;
        for (const PlyProp& p : vprops) {
            const double val = rd.next(p.type);
            const float f = static_cast<float>(val);
            const float col = plyIsFloat(p.type) ? f : f / 255.0f; // 0..255 int colors normalize
            switch (p.sem) {
                case PlySem::X: mv.px = f; break;
                case PlySem::Y: mv.py = f; break;
                case PlySem::Z: mv.pz = f; break;
                case PlySem::NX: mv.nx = f; break;
                case PlySem::NY: mv.ny = f; break;
                case PlySem::NZ: mv.nz = f; break;
                case PlySem::R: mv.r = col; break;
                case PlySem::G: mv.g = col; break;
                case PlySem::B: mv.b = col; break;
                case PlySem::S: mv.u = f; break;
                case PlySem::T: mv.v = opts.flipV ? 1.0f - f : f; break;
                case PlySem::Ignore: break;
            }
        }
        if (rd.error) return false;
        out.vertices.push_back(mv);
    }

    for (long fi = 0; fi < faceCount; ++fi) {
        const long n = static_cast<long>(rd.next(faceCountType));
        if (rd.error || n < 0) return false;
        std::vector<uint32_t> face;
        face.reserve(static_cast<std::size_t>(n));
        for (long k = 0; k < n; ++k) {
            const long idx = static_cast<long>(rd.next(faceIndexType));
            if (rd.error || idx < 0 || idx >= vertexCount) return false;
            face.push_back(static_cast<uint32_t>(idx));
        }
        for (long k = 2; k < n; ++k) { // fan-triangulate
            out.indices.push_back(face[0]);
            out.indices.push_back(face[static_cast<std::size_t>(k - 1)]);
            out.indices.push_back(face[static_cast<std::size_t>(k)]);
        }
    }

    return !out.vertices.empty();
}

inline bool loadPly(const std::string& path, shapes::MeshData& out, const PlyLoadOptions& opts = {}) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const std::streamsize size = f.tellg();
    if (size < 0) return false;
    f.seekg(0);
    std::string text(static_cast<std::size_t>(size), '\0');
    f.read(text.data(), size);
    return parsePly(text, out, opts);
}

} // namespace maz::render
