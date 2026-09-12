// tests/render/mesharrow.cpp — verifies the solid arrow (render::buildArrow). Ground truths: an N-segment arrow has
// 3N+2 vertices and 6N triangles; the tip sits at (0, length, 0) and the base at y=0; the widest radius is the head
// radius; it is a closed watertight solid (every edge shared by exactly two faces, Euler V-E+F==2); bad params give
// an empty mesh. Pure CPU, headless.
#include "maz/render/MeshArrow.hpp"

#include <cmath>
#include <cstdio>
#include <unordered_map>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

static bool manifoldClosed(const shapes::MeshData& m, std::size_t& edgeCount) {
    std::unordered_map<std::uint64_t, int> edges;
    for (std::size_t f = 0; f < m.indices.size(); f += 3) {
        const std::uint32_t v[3] = {m.indices[f], m.indices[f + 1], m.indices[f + 2]};
        for (int e = 0; e < 3; ++e) {
            std::uint32_t a = v[e], b = v[(e + 1) % 3];
            const std::uint64_t key = a < b ? (static_cast<std::uint64_t>(a) << 32) | b
                                            : (static_cast<std::uint64_t>(b) << 32) | a;
            ++edges[key];
        }
    }
    edgeCount = edges.size();
    for (const auto& kv : edges) if (kv.second != 2) return false;
    return true;
}

int main() {
    const int seg = 12;
    const float length = 2.0f, headR = 0.3f;
    const shapes::MeshData a = buildArrow(length, 0.1f, headR, 0.5f, seg);

    // --- 1. Counts: 3N+2 vertices, 6N triangles. ---
    {
        CHECK(a.vertices.size() == static_cast<std::size_t>(3 * seg + 2), "3N+2 vertices");
        CHECK(a.indices.size() == static_cast<std::size_t>(6 * seg) * 3u, "6N triangles");
    }

    // --- 2. Base at y=0, tip at y=length, widest radius = head radius. ---
    {
        float miny = 1e9f, maxy = -1e9f, maxr = 0.0f;
        bool tip = false;
        for (const MeshVertex& v : a.vertices) {
            miny = std::fmin(miny, v.py); maxy = std::fmax(maxy, v.py);
            maxr = std::fmax(maxr, std::sqrt(v.px * v.px + v.pz * v.pz));
            if (near(v.px, 0.0f, 1e-6f) && near(v.pz, 0.0f, 1e-6f) && near(v.py, length, 1e-5f)) tip = true;
        }
        CHECK(near(miny, 0.0f, 1e-6f), "base sits at y=0");
        CHECK(near(maxy, length, 1e-5f), "arrow reaches y=length");
        CHECK(tip, "there is a tip vertex at (0, length, 0)");
        CHECK(near(maxr, headR, 1e-5f), "widest radius equals the head radius");
    }

    // --- 3. Closed watertight solid: Euler V-E+F==2, every edge shared by exactly two faces. ---
    {
        std::size_t E = 0;
        const bool ok = manifoldClosed(a, E);
        const std::size_t V = a.vertices.size(), F = a.indices.size() / 3;
        CHECK(ok, "every edge shared by exactly two faces (closed manifold)");
        CHECK(static_cast<long>(V) - static_cast<long>(E) + static_cast<long>(F) == 2, "Euler V-E+F==2 (closed solid)");
    }

    // --- 4. Bad params -> empty. ---
    {
        CHECK(buildArrow(0.0f).vertices.empty(), "zero length -> empty");
        CHECK(buildArrow(1.0f, 0.0f).vertices.empty(), "zero shaft radius -> empty");
        CHECK(buildArrow(1.0f, 0.03f, 0.08f, 0.25f, 2).vertices.empty(), "fewer than 3 segments -> empty");
    }

    if (g_fail == 0) {
        std::printf("mesharrow: OK — 3N+2/6N counts, tip at length, head-radius width, watertight, safe.\n");
        return 0;
    }
    std::printf("mesharrow: %d failure(s).\n", g_fail);
    return 1;
}
