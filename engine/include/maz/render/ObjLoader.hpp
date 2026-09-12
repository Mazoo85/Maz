#pragma once

#include "maz/render/Shapes.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// maz::render Wavefront OBJ loader — a second model importer alongside the glTF path (Maz had only
// glTF). OBJ is the lingua franca hand-off format from almost every DCC tool and asset pack, so
// supporting it widens what artists can bring in. parseObj reads the text form (positions `v`,
// texcoords `vt`, normals `vn`, faces `f`) into a MeshData ready for Renderer::createMesh: it
// resolves OBJ's separate 1-based (and negative/relative) index pools, deduplicates each unique
// position/uv/normal combination into one vertex, and fan-triangulates n-gon faces. Pure text
// parsing (no GPU), so it unit-tests headlessly from an in-memory string; loadObj wraps it for files.
namespace maz::render {

struct ObjLoadOptions {
    bool flipV = true;                 // OBJ's V origin is bottom-left; most GPU sampling wants top-left
    Color color{1.0f, 1.0f, 1.0f, 1.0f}; // tint applied to every vertex (OBJ carries no vertex color)
};

inline bool parseObj(const std::string& text, shapes::MeshData& out, const ObjLoadOptions& opts = {}) {
    out.vertices.clear();
    out.indices.clear();

    std::vector<std::array<float, 3>> positions;
    std::vector<std::array<float, 2>> texcoords;
    std::vector<std::array<float, 3>> normals;
    std::unordered_map<std::string, uint32_t> combos; // "pi/ti/ni" -> vertex index

    auto resolve = [](int idx, size_t count) -> long {
        // OBJ indices are 1-based; negatives count back from the current end. Returns -1 if invalid.
        if (idx > 0) {
            return static_cast<long>(idx) - 1;
        }
        if (idx < 0) {
            return static_cast<long>(count) + idx;
        }
        return -1; // 0 is not a valid OBJ index
    };

    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        // Trim a trailing CR (Windows line endings) so token parsing stays clean.
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::istringstream ls(line);
        std::string tag;
        ls >> tag;
        if (tag == "v") {
            std::array<float, 3> p{0.0f, 0.0f, 0.0f};
            ls >> p[0] >> p[1] >> p[2];
            positions.push_back(p);
        } else if (tag == "vt") {
            std::array<float, 2> t{0.0f, 0.0f};
            ls >> t[0] >> t[1];
            texcoords.push_back(t);
        } else if (tag == "vn") {
            std::array<float, 3> n{0.0f, 0.0f, 0.0f};
            ls >> n[0] >> n[1] >> n[2];
            normals.push_back(n);
        } else if (tag == "f") {
            // Collect this face's vertices (as dedup'd MeshData indices), then fan-triangulate.
            std::vector<uint32_t> face;
            std::string vtok;
            while (ls >> vtok) {
                // Parse "pi", "pi/ti", "pi/ti/ni", or "pi//ni".
                int pi = 0;
                int ti = 0;
                int ni = 0;
                {
                    size_t s0 = vtok.find('/');
                    if (s0 == std::string::npos) {
                        pi = std::atoi(vtok.c_str());
                    } else {
                        pi = std::atoi(vtok.substr(0, s0).c_str());
                        size_t s1 = vtok.find('/', s0 + 1);
                        if (s1 == std::string::npos) {
                            ti = std::atoi(vtok.substr(s0 + 1).c_str());
                        } else {
                            if (s1 > s0 + 1) {
                                ti = std::atoi(vtok.substr(s0 + 1, s1 - s0 - 1).c_str());
                            }
                            ni = std::atoi(vtok.substr(s1 + 1).c_str());
                        }
                    }
                }
                const long rp = resolve(pi, positions.size());
                if (rp < 0 || rp >= static_cast<long>(positions.size())) {
                    continue; // skip a face-vertex with no valid position
                }
                const long rt = ti != 0 ? resolve(ti, texcoords.size()) : -1;
                const long rn = ni != 0 ? resolve(ni, normals.size()) : -1;

                const std::string key = std::to_string(rp) + "/" + std::to_string(rt) + "/" +
                                        std::to_string(rn);
                auto it = combos.find(key);
                uint32_t vidx = 0;
                if (it != combos.end()) {
                    vidx = it->second;
                } else {
                    MeshVertex mv{};
                    mv.px = positions[static_cast<size_t>(rp)][0];
                    mv.py = positions[static_cast<size_t>(rp)][1];
                    mv.pz = positions[static_cast<size_t>(rp)][2];
                    if (rn >= 0 && rn < static_cast<long>(normals.size())) {
                        mv.nx = normals[static_cast<size_t>(rn)][0];
                        mv.ny = normals[static_cast<size_t>(rn)][1];
                        mv.nz = normals[static_cast<size_t>(rn)][2];
                    }
                    if (rt >= 0 && rt < static_cast<long>(texcoords.size())) {
                        mv.u = texcoords[static_cast<size_t>(rt)][0];
                        mv.v = opts.flipV ? 1.0f - texcoords[static_cast<size_t>(rt)][1]
                                          : texcoords[static_cast<size_t>(rt)][1];
                    }
                    mv.r = opts.color.r;
                    mv.g = opts.color.g;
                    mv.b = opts.color.b;
                    vidx = static_cast<uint32_t>(out.vertices.size());
                    out.vertices.push_back(mv);
                    combos.emplace(key, vidx);
                }
                face.push_back(vidx);
            }
            for (size_t i = 2; i < face.size(); ++i) {
                out.indices.push_back(face[0]);
                out.indices.push_back(face[i - 1]);
                out.indices.push_back(face[i]);
            }
        }
        // All other tags (#, o, g, s, mtllib, usemtl, ...) are ignored.
    }

    return !out.vertices.empty() && !out.indices.empty();
}

inline bool loadObj(const char* path, shapes::MeshData& out, const ObjLoadOptions& opts = {}) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return parseObj(ss.str(), out, opts);
}

} // namespace maz::render
