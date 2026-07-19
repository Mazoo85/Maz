#pragma once

#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// maz::render Wavefront MTL (.mtl) material-library parser — the companion to the OBJ mesh loader. An OBJ
// file references materials by name (`mtllib foo.mtl` + `usemtl name`); the actual colors, shininess,
// transparency, and texture-map paths live in the sibling `.mtl` file. Godot's OBJ importer reads it to
// build real materials; Maz's OBJ loader parsed only geometry and skipped `mtllib`/`usemtl`, so imported
// models came in untextured and flat-colored. `parseMtl` reads the text library into a list of
// `MtlMaterial` records (ambient/diffuse/specular colors, specular exponent, optical density, dissolve,
// illumination model, and the diffuse/ambient/specular/bump texture-map filenames). Pure text parsing — no
// GPU — so it unit-tests headlessly from an in-memory string; `loadMtl` wraps it for files, and
// `findMaterial` resolves a `usemtl` name against the parsed list.
//
// Scope note (honest): the common, widely-emitted subset — Ka/Kd/Ks/Ns/Ni/d/Tr/illum and
// map_Ka/map_Kd/map_Ks/map_Bump(bump). A single grayscale value is accepted where a color is expected.
// Texture lines take the final whitespace-delimited token as the filename, so leading option flags
// (`-o`, `-s`, `-bm`, …) are skipped rather than interpreted. Spectral/xyz color spaces and PBR extension
// tags (Pr/Pm/Pc/…) are not decoded. A library with no `newmtl` yields an empty list.
namespace maz::render {

struct MtlMaterial {
    std::string name;
    float ka[3] = {0.2f, 0.2f, 0.2f};   // ambient reflectivity
    float kd[3] = {0.8f, 0.8f, 0.8f};   // diffuse reflectivity
    float ks[3] = {0.0f, 0.0f, 0.0f};   // specular reflectivity
    float ns = 0.0f;                    // specular exponent (shininess), 0..1000
    float ni = 1.0f;                    // optical density (index of refraction)
    float d = 1.0f;                     // dissolve: 1 = opaque, 0 = fully transparent
    int illum = 1;                      // illumination model
    std::string mapKa;                  // ambient texture
    std::string mapKd;                  // diffuse texture
    std::string mapKs;                  // specular texture
    std::string mapBump;                // bump / normal texture
};

namespace detail {

// Read up to three floats from a stream; if only one is present, replicate it across all three (a single
// grayscale value is legal where MTL expects an RGB color).
inline void mtlReadColor(std::istringstream& ss, float out[3]) {
    float a = 0.0f, b = 0.0f, c = 0.0f;
    ss >> a;
    if (ss >> b) {
        if (!(ss >> c)) c = b; // two values is malformed; keep it defined
        out[0] = a;
        out[1] = b;
        out[2] = c;
    } else {
        out[0] = out[1] = out[2] = a; // grayscale
    }
}

// The filename of a texture-map line is its last whitespace-delimited token, so any leading option flags
// are skipped. Returns empty if the line has no argument.
inline std::string mtlMapPath(std::istringstream& ss) {
    std::string tok, last;
    while (ss >> tok) last = tok;
    return last;
}

} // namespace detail

inline bool parseMtl(const std::string& text, std::vector<MtlMaterial>& out) {
    out.clear();
    std::istringstream in(text);
    std::string line;
    MtlMaterial* cur = nullptr;
    while (std::getline(in, line)) {
        // Strip a trailing CR (Windows line endings).
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ss(line);
        std::string tag;
        if (!(ss >> tag)) continue;      // blank line
        if (tag.empty() || tag[0] == '#') continue; // comment

        if (tag == "newmtl") {
            MtlMaterial m;
            ss >> m.name;
            out.push_back(m);
            cur = &out.back();
            continue;
        }
        if (!cur) continue; // data before the first newmtl — ignore

        if (tag == "Ka") {
            detail::mtlReadColor(ss, cur->ka);
        } else if (tag == "Kd") {
            detail::mtlReadColor(ss, cur->kd);
        } else if (tag == "Ks") {
            detail::mtlReadColor(ss, cur->ks);
        } else if (tag == "Ns") {
            ss >> cur->ns;
        } else if (tag == "Ni") {
            ss >> cur->ni;
        } else if (tag == "d") {
            ss >> cur->d;
        } else if (tag == "Tr") {
            float tr = 0.0f;
            ss >> tr;
            cur->d = 1.0f - tr; // Tr is the inverse of d
        } else if (tag == "illum") {
            ss >> cur->illum;
        } else if (tag == "map_Ka") {
            cur->mapKa = detail::mtlMapPath(ss);
        } else if (tag == "map_Kd") {
            cur->mapKd = detail::mtlMapPath(ss);
        } else if (tag == "map_Ks") {
            cur->mapKs = detail::mtlMapPath(ss);
        } else if (tag == "map_Bump" || tag == "map_bump" || tag == "bump") {
            cur->mapBump = detail::mtlMapPath(ss);
        }
        // Unknown tags are ignored.
    }
    return !out.empty();
}

// Resolve a `usemtl` name against a parsed library. Returns nullptr when absent.
inline const MtlMaterial* findMaterial(const std::vector<MtlMaterial>& lib, const std::string& name) {
    for (const MtlMaterial& m : lib)
        if (m.name == name) return &m;
    return nullptr;
}

inline bool loadMtl(const std::string& path, std::vector<MtlMaterial>& out) {
    std::ifstream f(path);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    return parseMtl(ss.str(), out);
}

} // namespace maz::render
