// Adaptive refinement: split what is too coarse, leave what is not, and do not tear a hole doing it.

#include "maz/render/MeshRefine.hpp"
#include "maz/render/Shapes.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

using maz::render::shapes::MeshData;

float longestEdge(const MeshData& m) {
    float worst = 0.0f;
    for (std::size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        for (int e = 0; e < 3; ++e) {
            const auto& a = m.vertices[m.indices[i + static_cast<std::size_t>(e)]];
            const auto& b = m.vertices[m.indices[i + static_cast<std::size_t>((e + 1) % 3)]];
            const float d = std::sqrt((a.px - b.px) * (a.px - b.px) + (a.py - b.py) * (a.py - b.py) +
                                      (a.pz - b.pz) * (a.pz - b.pz));
            if (d > worst) {
                worst = d;
            }
        }
    }
    return worst;
}

float surfaceArea(const MeshData& m) {
    float total = 0.0f;
    for (std::size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        const auto& a = m.vertices[m.indices[i]];
        const auto& b = m.vertices[m.indices[i + 1u]];
        const auto& c = m.vertices[m.indices[i + 2u]];
        const float ux = b.px - a.px, uy = b.py - a.py, uz = b.pz - a.pz;
        const float vx = c.px - a.px, vy = c.py - a.py, vz = c.pz - a.pz;
        const float cx = uy * vz - uz * vy;
        const float cy = uz * vx - ux * vz;
        const float cz = ux * vy - uy * vx;
        total += 0.5f * std::sqrt(cx * cx + cy * cy + cz * cz);
    }
    return total;
}

// Every edge of a closed surface is shared by exactly two triangles. A T-junction — one triangle
// split along an edge its neighbour left whole — shows up here as an edge used once, which is the
// crack this is really looking for.
//
// Edges are keyed on POSITION rather than index, because refinement is free to hand two corners of
// the same seam different indices and that is not a hole.
bool closed(const MeshData& m) {
    const auto key = [&m](std::uint32_t i) {
        const auto& v = m.vertices[i];
        const auto q = [](float f) { return static_cast<long>(std::lround(f * 8192.0f)); };
        return std::array<long, 3>{q(v.px), q(v.py), q(v.pz)};
    };
    std::map<std::pair<std::array<long, 3>, std::array<long, 3>>, int> used;
    for (std::size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        for (int e = 0; e < 3; ++e) {
            auto a = key(m.indices[i + static_cast<std::size_t>(e)]);
            auto b = key(m.indices[i + static_cast<std::size_t>((e + 1) % 3)]);
            if (b < a) {
                std::swap(a, b);
            }
            used[{a, b}] += 1;
        }
    }
    for (const auto& kv : used) {
        if (kv.second != 2) {
            return false;
        }
    }
    return true;
}

} // namespace

int main() {
    using maz::render::refineMesh;
    using maz::render::Color;

    const Color grey{0.5f, 0.5f, 0.5f, 1.0f};

    // ---- a big box comes back fine, and still a box ------------------------------------------------
    {
        MeshData m = maz::render::shapes::makeBox(4.0f, grey);
        const float areaBefore = surfaceArea(m);
        check(longestEdge(m) > 3.9f, "a four-metre box starts with four-metre edges");
        check(closed(m), "and starts closed");

        refineMesh(m, 0.4f);

        check(longestEdge(m) <= 0.4f + 1e-4f, "after refining, no edge is longer than asked for");
        check(std::fabs(surfaceArea(m) - areaBefore) < areaBefore * 1e-3f,
              "and the surface is the same size it was — splitting adds vertices, not shape");
        check(closed(m), "and the box is still closed: no crack, no T-junction");
        check(m.vertices.size() > 500u, "and it actually gained vertices");
    }

    // ---- a THIN box: the case a wall actually is --------------------------------------------------
    //
    // A cube splits every edge on every pass, so it only ever exercises the all-three-split branch.
    // A wall is six metres across, three high and twelve centimetres thick, and that thickness is
    // already finer than the target — so its triangles come up with one or two long edges and one
    // short one, which is where the other two branches live, and where a crack would come from. The
    // refinement is only ever going to see this shape in practice.
    {
        MeshData m = maz::render::shapes::makeBox(1.0f, grey);
        for (auto& v : m.vertices) {
            v.px *= 6.0f;
            v.py *= 3.0f;
            v.pz *= 0.12f;
        }
        check(closed(m), "the thin box starts closed");
        refineMesh(m, 0.4f);
        check(longestEdge(m) <= 0.4f + 1e-4f, "a thin box refines to the target too");
        check(closed(m), "and a thin box is still closed — the one- and two-cut branches do not tear");

        // And nothing floated off the box. Every vertex has to sit ON one of the six faces, which
        // means at least one of its coordinates is still hard against the box's extent. (The first
        // version of this check asked that every vertex be on one of the two BIG faces, which is
        // wrong: the thin rim is a face too, and splitting its long diagonal puts a legitimate
        // vertex halfway through the thickness.)
        float worstOff = 1.0f;
        for (const auto& v : m.vertices) {
            const float onFace = std::max(std::max(std::fabs(v.px) / 3.0f, std::fabs(v.py) / 1.5f),
                                          std::fabs(v.pz) / 0.06f);
            worstOff = std::min(worstOff, onFace);
        }
        check(worstOff > 1.0f - 1e-5f, "and every vertex is still on the surface of the box");
    }

    // ---- a triangle's new corners land ON the original face ----------------------------------------
    //
    // The whole point is more places to put detail, not a different shape. Every vertex of a refined
    // flat face has to satisfy the plane it came from.
    {
        MeshData m;
        m.vertices.push_back(maz::render::MeshVertex{0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                                                     1.0f, 1.0f, 1.0f, 0.0f, 0.0f});
        m.vertices.push_back(maz::render::MeshVertex{3.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                                                     1.0f, 1.0f, 1.0f, 1.0f, 0.0f});
        m.vertices.push_back(maz::render::MeshVertex{0.0f, 0.0f, 3.0f, 0.0f, 1.0f, 0.0f,
                                                     1.0f, 1.0f, 1.0f, 0.0f, 1.0f});
        m.indices = {0u, 1u, 2u};
        refineMesh(m, 0.5f);
        float offPlane = 0.0f;
        for (const auto& v : m.vertices) {
            if (std::fabs(v.py) > offPlane) {
                offPlane = std::fabs(v.py);
            }
        }
        check(offPlane < 1e-6f, "every new vertex of a flat face lies in that face's plane");
        check(longestEdge(m) <= 0.5f + 1e-4f, "and the triangle is refined to the size asked for");
    }

    // ---- colour and normal are carried, not reset --------------------------------------------------
    {
        MeshData m = maz::render::shapes::makeBox(2.0f, Color{0.25f, 0.5f, 0.75f, 1.0f});
        refineMesh(m, 0.3f);
        float worstColour = 0.0f;
        float worstNormal = 0.0f;
        for (const auto& v : m.vertices) {
            worstColour = std::max(worstColour, std::fabs(v.r - 0.25f));
            worstColour = std::max(worstColour, std::fabs(v.g - 0.5f));
            worstColour = std::max(worstColour, std::fabs(v.b - 0.75f));
            const float len = std::sqrt(v.nx * v.nx + v.ny * v.ny + v.nz * v.nz);
            worstNormal = std::max(worstNormal, std::fabs(len - 1.0f));
        }
        check(worstColour < 1e-6f, "a new vertex keeps the colour of the face it was cut from");
        check(worstNormal < 1e-6f, "and its normal is still a unit normal of that flat face");
    }

    // ---- what is already fine enough is left completely alone --------------------------------------
    {
        MeshData m = maz::render::shapes::makeBox(0.05f, grey);
        const std::size_t before = m.vertices.size();
        const std::size_t tris = m.indices.size();
        refineMesh(m, 0.4f);
        check(m.vertices.size() == before && m.indices.size() == tris,
              "a five-centimetre box is untouched by a forty-centimetre target — a doorknob does not "
              "pay for a wall");
    }

    // ---- the same twice ----------------------------------------------------------------------------
    //
    // Midpoints are created in a sorted order rather than whatever order a hash table holds them in,
    // because the browser and the command line build against different standard libraries and a mesh
    // that comes out in a different order is a film that comes out different.
    {
        MeshData a = maz::render::shapes::makeBox(3.0f, grey);
        MeshData b = maz::render::shapes::makeBox(3.0f, grey);
        refineMesh(a, 0.35f);
        refineMesh(b, 0.35f);
        bool same = a.vertices.size() == b.vertices.size() && a.indices == b.indices;
        if (same) {
            for (std::size_t i = 0; i < a.vertices.size(); ++i) {
                if (a.vertices[i].px != b.vertices[i].px || a.vertices[i].py != b.vertices[i].py ||
                    a.vertices[i].pz != b.vertices[i].pz) {
                    same = false;
                    break;
                }
            }
        }
        check(same, "refining the same mesh twice gives the same mesh, vertex for vertex");
    }

    // ---- and the same however the triangles were listed ---------------------------------------------
    //
    // The check above is nearly worthless on its own: two runs in one process walk the same hash
    // table in the same order and agree whatever the code does. What the sorted midpoint order is
    // actually for is that the browser and the command line link different standard libraries, whose
    // hash tables iterate differently — and that cannot be reproduced here at all.
    //
    // What CAN be reproduced is the property underneath it. If new vertices are created in sorted
    // edge order they depend on the set of edges alone; if they are created in hash order they depend
    // on the order the edges were inserted, which is the order the triangles were visited. So listing
    // the same triangles backwards is a stand-in for linking a different standard library: with the
    // sort the vertex array comes out identical, without it the midpoints come out shuffled.
    {
        MeshData a = maz::render::shapes::makeBox(1.0f, grey);
        for (auto& v : a.vertices) {
            v.px *= 6.0f;
            v.py *= 3.0f;
            v.pz *= 0.12f;
        }
        MeshData b = a;
        for (std::size_t i = 0; i * 3u + 2u < b.indices.size() / 2u * 2u; ++i) {
            const std::size_t j = b.indices.size() / 3u - 1u - i;
            if (j <= i) {
                break;
            }
            for (int k = 0; k < 3; ++k) {
                std::swap(b.indices[i * 3u + static_cast<std::size_t>(k)],
                          b.indices[j * 3u + static_cast<std::size_t>(k)]);
            }
        }
        refineMesh(a, 0.4f);
        refineMesh(b, 0.4f);
        bool same = a.vertices.size() == b.vertices.size();
        if (same) {
            for (std::size_t i = 0; i < a.vertices.size(); ++i) {
                if (a.vertices[i].px != b.vertices[i].px || a.vertices[i].py != b.vertices[i].py ||
                    a.vertices[i].pz != b.vertices[i].pz) {
                    same = false;
                    break;
                }
            }
        }
        check(same, "the new vertices come out in the same order however the triangles were listed");
    }

    // ---- refusals ----------------------------------------------------------------------------------
    {
        MeshData m = maz::render::shapes::makeBox(4.0f, grey);
        const std::size_t before = m.vertices.size();
        refineMesh(m, 0.0f);
        check(m.vertices.size() == before, "a target of zero refines nothing rather than looping");
        refineMesh(m, -1.0f);
        check(m.vertices.size() == before, "and neither does a negative one");

        MeshData empty;
        refineMesh(empty, 0.4f);
        check(empty.vertices.empty(), "an empty mesh comes back empty");

        // The cap stops a pass rather than truncating one: the mesh is left coarse and whole.
        MeshData capped = maz::render::shapes::makeBox(4.0f, grey);
        refineMesh(capped, 0.05f, 100u);
        check(capped.vertices.size() <= 100u, "the cap is respected");
        check(closed(capped), "and a mesh stopped by the cap is still closed");
    }

    if (failures == 0) {
        std::printf("mesh refine: all checks passed\n");
    }
    return failures == 0 ? 0 : 1;
}
