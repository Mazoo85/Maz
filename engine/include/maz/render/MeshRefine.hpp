#pragma once

#include "maz/render/Shapes.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

// maz::render ADAPTIVE REFINEMENT — split a mesh's long edges until nothing is coarser than a given
// size in metres, leaving everything already fine enough exactly as it was.
//
// `Subdivision.hpp` next door does the textbook thing: every triangle becomes four, everywhere, and
// each pass multiplies the whole mesh by four whether it needed it or not. That is the right tool for
// smoothing a cage and the wrong one for this, where a room is a handful of enormous flat boxes and a
// doorknob is already a hundred small triangles. Refining both by the same factor spends all of the
// budget on the doorknob.
//
// WHY ANY OF THIS. A flat surface has nowhere to put detail. A wall built as one quad has four
// vertices, and anything worked out per vertex — a colour variation, a dirt gradient, a bake — can
// only be linear across the whole wall, which is another way of saying it cannot be seen. Splitting
// the wall into a grid costs nothing per pixel (the rasteriser already interpolates vertex colour) and
// gives every one of those effects somewhere to land.
//
// IT DOES NOT CRACK. The decision to split an edge depends on that edge alone — its two endpoints —
// so two triangles sharing an edge always agree about it, and a cache keyed on the edge hands them
// the same new vertex. That is the whole of why the one-split and two-split cases below are safe; a
// version that decided per triangle would leave a T-junction down every boundary and daylight through
// it.
namespace maz::render {

namespace refinedetail {

inline std::uint64_t edgeOf(std::uint32_t a, std::uint32_t b) {
    const std::uint32_t lo = a < b ? a : b;
    const std::uint32_t hi = a < b ? b : a;
    return (static_cast<std::uint64_t>(lo) << 32) | static_cast<std::uint64_t>(hi);
}

inline float edgeLength2(const MeshVertex& a, const MeshVertex& b) {
    const float dx = a.px - b.px;
    const float dy = a.py - b.py;
    const float dz = a.pz - b.pz;
    return dx * dx + dy * dy + dz * dz;
}

inline MeshVertex midpoint(const MeshVertex& a, const MeshVertex& b) {
    MeshVertex m;
    m.px = (a.px + b.px) * 0.5f;
    m.py = (a.py + b.py) * 0.5f;
    m.pz = (a.pz + b.pz) * 0.5f;
    // The normal is averaged rather than recomputed. On a flat face the two are identical, which is
    // every face this is used on; on a curved one averaging is what keeps the shading continuous.
    m.nx = (a.nx + b.nx) * 0.5f;
    m.ny = (a.ny + b.ny) * 0.5f;
    m.nz = (a.nz + b.nz) * 0.5f;
    m.r = (a.r + b.r) * 0.5f;
    m.g = (a.g + b.g) * 0.5f;
    m.b = (a.b + b.b) * 0.5f;
    m.u = (a.u + b.u) * 0.5f;
    m.v = (a.v + b.v) * 0.5f;
    return m;
}

} // namespace refinedetail

// Split every edge longer than `most` metres, and keep going until none is left or the mesh reaches
// `cap` vertices.
//
//   most   the longest edge to leave alone, in metres. 0.4 turns a three-metre wall into a grid of
//          roughly eight by eight.
//   cap    a ceiling on the vertex count. A pass that would cross it is not taken at all, rather than
//          taken halfway — half a pass is a mesh that is fine down one end and coarse down the other,
//          which shows.
inline void refineMesh(shapes::MeshData& mesh, float most, std::size_t cap = 200000u) {
    if (most <= 0.0f || mesh.indices.size() < 3u) {
        return;
    }
    const float longest2 = most * most;

    // Eight passes is a hard stop, not a tuning knob: each pass at least halves the longest edge, so
    // eight of them would refine a 256-metre edge down to the target. Anything still too long after
    // that is degenerate.
    for (int pass = 0; pass < 8; ++pass) {
        std::unordered_map<std::uint64_t, std::uint32_t> split;
        // Which edges want splitting, decided before any triangle is looked at, so the answer cannot
        // depend on the order triangles are visited in.
        for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            for (int e = 0; e < 3; ++e) {
                const std::uint32_t a = mesh.indices[i + static_cast<std::size_t>(e)];
                const std::uint32_t b = mesh.indices[i + static_cast<std::size_t>((e + 1) % 3)];
                if (refinedetail::edgeLength2(mesh.vertices[a], mesh.vertices[b]) > longest2) {
                    split.emplace(refinedetail::edgeOf(a, b), 0u);
                }
            }
        }
        if (split.empty() || mesh.vertices.size() + split.size() > cap) {
            return;
        }

        // The midpoints, in a settled order. Walking the map would deal them out in whatever order
        // the hash table happens to hold them, which differs between standard libraries — and the
        // browser and the command line build against different ones. Sorting by the edge key makes
        // the mesh come out the same everywhere, which a test elsewhere checks to the pixel.
        std::vector<std::uint64_t> keys;
        keys.reserve(split.size());
        for (const auto& kv : split) {
            keys.push_back(kv.first);
        }
        for (std::size_t i = 1; i < keys.size(); ++i) {
            const std::uint64_t k = keys[i];
            std::size_t j = i;
            while (j > 0 && keys[j - 1] > k) {
                keys[j] = keys[j - 1];
                --j;
            }
            keys[j] = k;
        }
        for (const std::uint64_t k : keys) {
            const auto a = static_cast<std::uint32_t>(k >> 32);
            const auto b = static_cast<std::uint32_t>(k & 0xffffffffu);
            split[k] = static_cast<std::uint32_t>(mesh.vertices.size());
            mesh.vertices.push_back(refinedetail::midpoint(mesh.vertices[a], mesh.vertices[b]));
        }

        std::vector<std::uint32_t> out;
        out.reserve(mesh.indices.size() * 2u);
        const auto midOf = [&split](std::uint32_t a, std::uint32_t b) -> std::uint32_t {
            const auto it = split.find(refinedetail::edgeOf(a, b));
            return it == split.end() ? 0xffffffffu : it->second;
        };
        for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            const std::uint32_t v[3] = {mesh.indices[i], mesh.indices[i + 1u], mesh.indices[i + 2u]};
            // m[e] is the midpoint of the edge from v[e] to v[e+1], if that edge is being split.
            const std::uint32_t m[3] = {midOf(v[0], v[1]), midOf(v[1], v[2]), midOf(v[2], v[0])};
            const int have = (m[0] != 0xffffffffu ? 1 : 0) + (m[1] != 0xffffffffu ? 1 : 0) +
                             (m[2] != 0xffffffffu ? 1 : 0);
            const auto emit = [&out](std::uint32_t a, std::uint32_t b, std::uint32_t c) {
                out.push_back(a);
                out.push_back(b);
                out.push_back(c);
            };
            if (have == 0) {
                emit(v[0], v[1], v[2]);
            } else if (have == 3) {
                emit(v[0], m[0], m[2]);
                emit(m[0], v[1], m[1]);
                emit(m[2], m[1], v[2]);
                emit(m[0], m[1], m[2]);
            } else if (have == 1) {
                // One cut, from the split edge's midpoint to the opposite corner.
                const int e = m[0] != 0xffffffffu ? 0 : (m[1] != 0xffffffffu ? 1 : 2);
                const std::uint32_t p = v[e];
                const std::uint32_t q = v[(e + 1) % 3];
                const std::uint32_t r = v[(e + 2) % 3];
                emit(p, m[e], r);
                emit(m[e], q, r);
            } else {
                // Two cuts. `e` is the edge NOT split, so the quad hangs off the corner opposite it.
                const int e = m[0] == 0xffffffffu ? 0 : (m[1] == 0xffffffffu ? 1 : 2);
                const std::uint32_t p = v[e];
                const std::uint32_t q = v[(e + 1) % 3];
                const std::uint32_t r = v[(e + 2) % 3];
                const std::uint32_t mq = m[(e + 1) % 3]; // on q -> r
                const std::uint32_t mr = m[(e + 2) % 3]; // on r -> p
                emit(p, q, mq);
                emit(p, mq, mr);
                emit(mr, mq, r);
            }
        }
        mesh.indices.swap(out);
    }
}

} // namespace maz::render
