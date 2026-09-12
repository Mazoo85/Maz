// tests/render/meshtopologysummary.cpp — verifies the topology summary (render::summarizeTopology). Ground
// truths of surface topology: a closed cube is genus 0 with Euler characteristic 2, one component, no holes; a
// torus is genus 1 with Euler 0; a cube missing a face is an open disk (Euler 1, one boundary loop, genus 0);
// two separate cubes are two components; a non-manifold fan leaves genus undefined. Pure CPU, headless.
#include "maz/render/MeshTopologySummary.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}

static shapes::MeshData cube(float ox = 0.0f) {
    shapes::MeshData m;
    m.vertices = {vtx(ox+0,0,0), vtx(ox+1,0,0), vtx(ox+1,1,0), vtx(ox+0,1,0),
                  vtx(ox+0,0,1), vtx(ox+1,0,1), vtx(ox+1,1,1), vtx(ox+0,1,1)};
    m.indices = {0,2,1, 0,3,2,  4,5,6, 4,6,7,  0,1,5, 0,5,4,
                 3,7,6, 3,6,2,  0,4,7, 0,7,3,  1,2,6, 1,6,5};
    return m;
}

// Closed welded torus: nu segments around the major ring, nv around the tube; no seam duplication.
static shapes::MeshData torus(int nu, int nv, float R, float r) {
    shapes::MeshData m;
    const double pi = 3.14159265358979323846;
    for (int i = 0; i < nu; ++i)
        for (int j = 0; j < nv; ++j) {
            const double u = 2.0 * pi * i / nu, v = 2.0 * pi * j / nv;
            const double cx = (R + r * std::cos(v)) * std::cos(u);
            const double cy = (R + r * std::cos(v)) * std::sin(u);
            const double cz = r * std::sin(v);
            m.vertices.push_back(vtx(static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(cz)));
        }
    auto id = [&](int i, int j) { return static_cast<std::uint32_t>((i % nu) * nv + (j % nv)); };
    for (int i = 0; i < nu; ++i)
        for (int j = 0; j < nv; ++j)
            m.indices.insert(m.indices.end(),
                             {id(i, j), id(i + 1, j), id(i + 1, j + 1), id(i, j), id(i + 1, j + 1), id(i, j + 1)});
    return m;
}

int main() {
    // --- 1. Closed cube: genus 0, Euler 2, one piece, no holes, closed & manifold. ---
    {
        const TopologySummary s = summarizeTopology(cube());
        CHECK(s.vertexCount == 8 && s.triangleCount == 12 && s.edgeCount == 18, "cube V/E/F = 8/18/12");
        CHECK(s.eulerCharacteristic == 2, "cube Euler characteristic is 2");
        CHECK(s.genus == 0, "cube is genus 0 (a sphere)");
        CHECK(s.componentCount == 1 && s.boundaryLoopCount == 0, "one piece, no holes");
        CHECK(s.closed && s.manifold, "cube is a closed manifold solid");
    }

    // --- 2. Torus: genus 1, Euler 0. ---
    {
        const TopologySummary s = summarizeTopology(torus(8, 6, 2.0f, 0.6f));
        CHECK(s.eulerCharacteristic == 0, "torus Euler characteristic is 0");
        CHECK(s.genus == 1, "torus is genus 1 (one handle)");
        CHECK(s.componentCount == 1 && s.boundaryLoopCount == 0, "torus is one closed piece");
        CHECK(s.closed && s.manifold, "torus is a closed manifold");
    }

    // --- 3. Cube missing its top face: open disk — Euler 1, one boundary loop, genus 0, not closed. ---
    {
        shapes::MeshData m = cube();
        m.indices.resize(m.indices.size() - 6); // drop the last face (two triangles)
        const TopologySummary s = summarizeTopology(m);
        CHECK(!s.closed, "open box is not watertight");
        CHECK(s.boundaryLoopCount == 1, "the missing face leaves exactly one boundary loop");
        CHECK(s.eulerCharacteristic == 1, "an open disk has Euler characteristic 1");
        CHECK(s.genus == 0 && s.manifold, "open box is a genus-0 manifold disk");
    }

    // --- 4. Two separate cubes: two components, still genus 0 overall. ---
    {
        shapes::MeshData a = cube(0.0f), b = cube(5.0f);
        shapes::MeshData m = a;
        const std::uint32_t base = static_cast<std::uint32_t>(a.vertices.size());
        for (const auto& v : b.vertices) m.vertices.push_back(v);
        for (std::uint32_t i : b.indices) m.indices.push_back(i + base);
        const TopologySummary s = summarizeTopology(m);
        CHECK(s.componentCount == 2, "two disjoint cubes are two components");
        CHECK(s.eulerCharacteristic == 4, "two spheres give Euler characteristic 4");
        CHECK(s.genus == 0, "two genus-0 pieces are genus 0 overall");
        CHECK(s.closed && s.manifold, "both cubes closed and manifold");
    }

    // --- 5. Non-manifold fan (edge shared by 3 triangles): genus undefined. ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(0,1,0), vtx(0,0,1), vtx(0,-1,0)};
        m.indices = {0,1,2, 0,1,3, 0,1,4}; // edge (0,1) shared by three triangles
        const TopologySummary s = summarizeTopology(m);
        CHECK(!s.manifold && s.nonManifoldEdgeCount >= 1, "the shared edge is flagged non-manifold");
        CHECK(s.genus == -1, "genus is undefined for a non-manifold mesh");
    }

    // --- 6. Empty mesh is safe. ---
    {
        const TopologySummary s = summarizeTopology(shapes::MeshData{});
        CHECK(s.triangleCount == 0 && s.componentCount == 0, "empty mesh -> zeroed summary");
    }

    if (g_fail == 0) {
        std::printf("meshtopologysummary: OK — cube genus0/chi2, torus genus1/chi0, open disk chi1, 2 comps, non-manifold.\n");
        return 0;
    }
    std::printf("meshtopologysummary: %d failure(s).\n", g_fail);
    return 1;
}
