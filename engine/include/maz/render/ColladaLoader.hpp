#pragma once

#include "maz/io/Xml.hpp"
#include "maz/render/Shapes.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

// maz::render COLLADA (.dae) mesh importer — closes a gap versus Godot, which imports Collada out of
// the box while Maz previously read only OBJ and glTF. COLLADA is an XML interchange format many DCC
// tools (Blender, Maya, SketchUp) still export, so supporting it widens what artists can bring in.
// `parseCollada` reads the text form into a `shapes::MeshData` ready for `Renderer::createMesh`, built
// on the header-only `io::XmlParser` pull parser (M174). It gathers every `<source>` float array, the
// `<vertices>` POSITION mapping, and the `<triangles>` / `<polylist>` index streams (with their
// per-semantic input offsets), then de-interleaves them into position/normal/uv vertices. Polygons
// with more than three corners are fan-triangulated. It is pure CPU string work — no GPU — so it
// unit-tests headlessly from an in-memory string; `loadCollada` wraps it for files.
//
// Scope note (honest): this reads the common exported-mesh case — the first `<geometry>`'s triangle or
// polylist primitives with VERTEX/NORMAL/TEXCOORD inputs. It does not apply node transforms, skinning,
// materials, multiple geometries, or `<trifans>`/`<tristrips>`; those are documented follow-ups. Malformed
// or unsupported documents return false rather than throwing.
namespace maz::render {

struct ColladaLoadOptions {
    bool flipV = true; // COLLADA's V origin is bottom-left; most GPU sampling wants top-left
    float r = 1.0f, g = 1.0f, b = 1.0f; // vertex tint (COLLADA colors/materials are out of scope)
};

namespace detail {

inline std::vector<float> parseFloatList(const std::string& s) {
    std::vector<float> out;
    const char* p = s.c_str();
    const char* end = p + s.size();
    while (p < end) {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
        if (p >= end) break;
        char* next = nullptr;
        const float v = std::strtof(p, &next);
        if (next == p) break;
        out.push_back(v);
        p = next;
    }
    return out;
}

inline std::vector<long> parseIntList(const std::string& s) {
    std::vector<long> out;
    const char* p = s.c_str();
    const char* end = p + s.size();
    while (p < end) {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
        if (p >= end) break;
        char* next = nullptr;
        const long v = std::strtol(p, &next, 10);
        if (next == p) break;
        out.push_back(v);
        p = next;
    }
    return out;
}

// Strip a leading '#' from a COLLADA URL reference ("#pos" -> "pos").
inline std::string stripHash(const std::string& s) {
    return (!s.empty() && s[0] == '#') ? s.substr(1) : s;
}

} // namespace detail

inline bool parseCollada(const std::string& text, shapes::MeshData& out,
                         const ColladaLoadOptions& opts = {}) {
    out.vertices.clear();
    out.indices.clear();

    io::XmlParser xml;
    xml.parse(text);

    std::unordered_map<std::string, std::vector<float>> sources; // source id -> floats
    std::unordered_map<std::string, std::string> vertexToPos;    // <vertices> id -> POSITION source id

    struct Input {
        std::string semantic;
        std::string source;
        int offset = 0;
    };
    std::vector<Input> inputs;
    std::vector<long> pIndices;
    std::vector<long> vcount; // polylist per-polygon corner counts (empty for <triangles>)
    bool havePrimitive = false;
    bool isPolylist = false;

    std::string curSourceId;   // id of the <source> we're inside
    std::string curVerticesId; // id of the <vertices> we're inside
    std::string textTarget;    // which element's text we're accumulating ("float_array"/"p"/"vcount")
    std::string textAccum;

    while (xml.read() && !xml.hasError() && !havePrimitive) {
        const auto type = xml.nodeType();
        if (type == io::XmlParser::NodeType::Element) {
            const std::string name = xml.nodeName();
            if (name == "source") {
                curSourceId = xml.getAttribute("id");
            } else if (name == "float_array") {
                if (!curSourceId.empty()) {
                    textTarget = "float_array";
                    textAccum.clear();
                }
            } else if (name == "vertices") {
                curVerticesId = xml.getAttribute("id");
            } else if (name == "input") {
                const std::string sem = xml.getAttribute("semantic");
                const std::string src = detail::stripHash(xml.getAttribute("source"));
                if (!curVerticesId.empty() && sem == "POSITION") {
                    vertexToPos[curVerticesId] = src;
                } else if (!curVerticesId.empty() && curSourceId.empty()) {
                    // primitive input (has an offset)
                    Input in;
                    in.semantic = sem;
                    in.source = src;
                    in.offset = static_cast<int>(std::strtol(xml.getAttribute("offset", "0").c_str(),
                                                             nullptr, 10));
                    inputs.push_back(in);
                }
            } else if (name == "triangles" || name == "polylist") {
                isPolylist = (name == "polylist");
                inputs.clear();
                pIndices.clear();
                vcount.clear();
                // primitive inputs live here; a <vertices> block already closed, so clear the markers
                curSourceId.clear();
                curVerticesId = "primitive"; // sentinel so <input> above is treated as primitive input
            } else if (name == "p") {
                textTarget = "p";
                textAccum.clear();
            } else if (name == "vcount") {
                textTarget = "vcount";
                textAccum.clear();
            }
        } else if (type == io::XmlParser::NodeType::Text) {
            if (!textTarget.empty()) textAccum += xml.nodeData();
        } else if (type == io::XmlParser::NodeType::ElementEnd) {
            const std::string name = xml.nodeName();
            if (name == "float_array" && textTarget == "float_array") {
                sources[curSourceId] = detail::parseFloatList(textAccum);
                textTarget.clear();
            } else if (name == "source") {
                curSourceId.clear();
            } else if (name == "vertices") {
                curVerticesId.clear();
            } else if (name == "p" && textTarget == "p") {
                pIndices = detail::parseIntList(textAccum);
                textTarget.clear();
            } else if (name == "vcount" && textTarget == "vcount") {
                vcount = detail::parseIntList(textAccum);
                textTarget.clear();
            } else if (name == "triangles" || name == "polylist") {
                havePrimitive = true;
            }
        }
    }

    if (xml.hasError() || inputs.empty() || pIndices.empty()) return false;

    // Resolve semantic inputs to source float arrays.
    int posOffset = -1, normOffset = -1, uvOffset = -1;
    const std::vector<float>* posSrc = nullptr;
    const std::vector<float>* normSrc = nullptr;
    const std::vector<float>* uvSrc = nullptr;
    int stride = 0;
    for (const auto& in : inputs) {
        stride = in.offset + 1 > stride ? in.offset + 1 : stride;
        if (in.semantic == "VERTEX") {
            const auto vit = vertexToPos.find(in.source);
            const std::string posId = vit != vertexToPos.end() ? vit->second : in.source;
            const auto sit = sources.find(posId);
            if (sit != sources.end()) posSrc = &sit->second;
            posOffset = in.offset;
        } else if (in.semantic == "NORMAL") {
            const auto sit = sources.find(in.source);
            if (sit != sources.end()) normSrc = &sit->second;
            normOffset = in.offset;
        } else if (in.semantic == "TEXCOORD") {
            const auto sit = sources.find(in.source);
            if (sit != sources.end()) uvSrc = &sit->second;
            uvOffset = in.offset;
        }
    }
    if (posSrc == nullptr || posOffset < 0 || stride <= 0) return false;

    // Emit one MeshVertex per referenced corner; build a corner list per polygon, then fan-triangulate.
    auto cornerCount = static_cast<long>(pIndices.size()) / stride;

    auto emitCorner = [&](long corner) -> uint32_t {
        const long base = corner * stride;
        MeshVertex mv{};
        mv.r = opts.r;
        mv.g = opts.g;
        mv.b = opts.b;
        const long pi = pIndices[static_cast<std::size_t>(base + posOffset)];
        if (pi >= 0 && (pi * 3 + 2) < static_cast<long>(posSrc->size())) {
            mv.px = (*posSrc)[static_cast<std::size_t>(pi * 3 + 0)];
            mv.py = (*posSrc)[static_cast<std::size_t>(pi * 3 + 1)];
            mv.pz = (*posSrc)[static_cast<std::size_t>(pi * 3 + 2)];
        }
        if (normSrc != nullptr && normOffset >= 0) {
            const long ni = pIndices[static_cast<std::size_t>(base + normOffset)];
            if (ni >= 0 && (ni * 3 + 2) < static_cast<long>(normSrc->size())) {
                mv.nx = (*normSrc)[static_cast<std::size_t>(ni * 3 + 0)];
                mv.ny = (*normSrc)[static_cast<std::size_t>(ni * 3 + 1)];
                mv.nz = (*normSrc)[static_cast<std::size_t>(ni * 3 + 2)];
            }
        }
        if (uvSrc != nullptr && uvOffset >= 0) {
            const long ui = pIndices[static_cast<std::size_t>(base + uvOffset)];
            if (ui >= 0 && (ui * 2 + 1) < static_cast<long>(uvSrc->size())) {
                mv.u = (*uvSrc)[static_cast<std::size_t>(ui * 2 + 0)];
                mv.v = (*uvSrc)[static_cast<std::size_t>(ui * 2 + 1)];
                if (opts.flipV) mv.v = 1.0f - mv.v;
            }
        }
        out.vertices.push_back(mv);
        return static_cast<uint32_t>(out.vertices.size() - 1);
    };

    if (isPolylist && !vcount.empty()) {
        long corner = 0;
        for (const long n : vcount) {
            if (n < 3 || corner + n > cornerCount) break;
            const uint32_t first = emitCorner(corner);
            uint32_t prev = emitCorner(corner + 1);
            for (long k = 2; k < n; ++k) {
                const uint32_t cur = emitCorner(corner + k);
                out.indices.push_back(first);
                out.indices.push_back(prev);
                out.indices.push_back(cur);
                prev = cur;
            }
            corner += n;
        }
    } else {
        // <triangles>: every three corners form one triangle.
        const long tris = cornerCount / 3;
        for (long t = 0; t < tris; ++t) {
            const uint32_t a = emitCorner(t * 3 + 0);
            const uint32_t b = emitCorner(t * 3 + 1);
            const uint32_t c = emitCorner(t * 3 + 2);
            out.indices.push_back(a);
            out.indices.push_back(b);
            out.indices.push_back(c);
        }
    }

    return !out.indices.empty();
}

inline bool loadCollada(const std::string& path, shapes::MeshData& out,
                        const ColladaLoadOptions& opts = {}) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const std::streamsize size = f.tellg();
    if (size < 0) return false;
    f.seekg(0);
    std::string text(static_cast<std::size_t>(size), '\0');
    f.read(text.data(), size);
    return parseCollada(text, out, opts);
}

} // namespace maz::render
