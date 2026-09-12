// tests/render/meshexplode.cpp — verifies the explode operation (render::explodeFaces / explodeFacesRadial).
// Ground truths: every triangle becomes 3 private vertices (unwelded); explodeFaces pushes each face along its
// own outward normal so a cube's bbox grows by 2*distance; explodeFacesRadial pushes faces away from a centre;
// distance 0 just facet-splits without moving; each corner carries the flat face normal. Pure CPU, headless.
#include "maz/render/MeshExplode.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

// A unit cube centred at the origin (edge 2, from -1..+1) with outward-facing winding on each of its 6 quads.
static shapes::MeshData cube() {
    shapes::MeshData m;
    auto quad = [&](maz::math::vec3 a, maz::math::vec3 b, maz::math::vec3 c, maz::math::vec3 d) {
        const std::uint32_t base = static_cast<std::uint32_t>(m.vertices.size());
        auto v = [](maz::math::vec3 p) { MeshVertex x{}; x.px = p.x; x.py = p.y; x.pz = p.z; x.r = x.g = x.b = 1; return x; };
        m.vertices.push_back(v(a)); m.vertices.push_back(v(b));
        m.vertices.push_back(v(c)); m.vertices.push_back(v(d));
        m.indices.push_back(base + 0); m.indices.push_back(base + 1); m.indices.push_back(base + 2);
        m.indices.push_back(base + 0); m.indices.push_back(base + 2); m.indices.push_back(base + 3);
    };
    using V = maz::math::vec3;
    quad(V(-1,-1, 1), V( 1,-1, 1), V( 1, 1, 1), V(-1, 1, 1)); // +Z
    quad(V( 1,-1,-1), V(-1,-1,-1), V(-1, 1,-1), V( 1, 1,-1)); // -Z
    quad(V( 1,-1, 1), V( 1,-1,-1), V( 1, 1,-1), V( 1, 1, 1)); // +X
    quad(V(-1,-1,-1), V(-1,-1, 1), V(-1, 1, 1), V(-1, 1,-1)); // -X
    quad(V(-1, 1, 1), V( 1, 1, 1), V( 1, 1,-1), V(-1, 1,-1)); // +Y
    quad(V(-1,-1,-1), V( 1,-1,-1), V( 1,-1, 1), V(-1,-1, 1)); // -Y
    return m;
}

// Axis-aligned bounds of a mesh.
static void bounds(const shapes::MeshData& m, maz::math::vec3& lo, maz::math::vec3& hi) {
    lo = maz::math::vec3(m.vertices[0].px, m.vertices[0].py, m.vertices[0].pz);
    hi = lo;
    for (const MeshVertex& v : m.vertices) {
        lo = maz::math::vec3(std::fmin(lo.x, v.px), std::fmin(lo.y, v.py), std::fmin(lo.z, v.pz));
        hi = maz::math::vec3(std::fmax(hi.x, v.px), std::fmax(hi.y, v.py), std::fmax(hi.z, v.pz));
    }
}

int main() {
    const shapes::MeshData c = cube();
    const std::size_t triN = c.indices.size() / 3; // 12

    // --- 1. Unwelding: N triangles -> 3N vertices, sequential indices. ---
    {
        const shapes::MeshData e = explodeFaces(c, 0.0f);
        CHECK(e.vertices.size() == triN * 3, "every triangle gets its own 3 vertices");
        CHECK(e.indices.size() == c.indices.size(), "index count is unchanged (still N triangles)");
        bool seq = true;
        for (std::uint32_t i = 0; i < e.indices.size(); ++i)
            if (e.indices[i] != i) seq = false;
        CHECK(seq, "indices are the sequential 0,1,2,... of an unwelded mesh");
    }

    // --- 2. distance 0 doesn't move anything: bounds match the original cube (-1..1). ---
    {
        const shapes::MeshData e = explodeFaces(c, 0.0f);
        maz::math::vec3 lo, hi; bounds(e, lo, hi);
        CHECK(near(lo.x, -1, 1e-5f) && near(hi.x, 1, 1e-5f), "distance 0 leaves the cube where it was");
    }

    // --- 3. explodeFaces by 0.5 pushes each face out along its own normal: bbox grows by 2*0.5 per axis. ---
    {
        const shapes::MeshData e = explodeFaces(c, 0.5f);
        maz::math::vec3 lo, hi; bounds(e, lo, hi);
        // +X face moves to x=1.5, -X face to x=-1.5, etc.
        CHECK(near(hi.x, 1.5f, 1e-4f) && near(lo.x, -1.5f, 1e-4f), "faces slide out along +/-X by 0.5");
        CHECK(near(hi.y, 1.5f, 1e-4f) && near(lo.y, -1.5f, 1e-4f), "faces slide out along +/-Y by 0.5");
        CHECK(near(hi.z, 1.5f, 1e-4f) && near(lo.z, -1.5f, 1e-4f), "faces slide out along +/-Z by 0.5");
    }

    // --- 4. Each exploded corner carries a UNIT face normal. ---
    {
        const shapes::MeshData e = explodeFaces(c, 0.5f);
        bool allUnit = true;
        for (const MeshVertex& v : e.vertices) {
            const float n = std::sqrt(v.nx * v.nx + v.ny * v.ny + v.nz * v.nz);
            if (!near(n, 1.0f, 1e-4f)) allUnit = false;
        }
        CHECK(allUnit, "every corner has a unit-length flat face normal");
    }

    // --- 5. explodeFacesRadial about the cube centre also grows the bbox symmetrically. ---
    {
        const shapes::MeshData e = explodeFacesRadial(c, 1.0f); // auto centre = origin
        maz::math::vec3 lo, hi; bounds(e, lo, hi);
        CHECK(hi.x > 1.0f && lo.x < -1.0f, "radial explode pushes faces outward from the centre");
        CHECK(near(hi.x, -lo.x, 1e-4f) && near(hi.y, -lo.y, 1e-4f), "the explode is symmetric about the centre");
    }

    // --- 6. Empty mesh is safe. ---
    {
        CHECK(explodeFaces(shapes::MeshData{}, 1.0f).vertices.empty(), "explodeFaces empty -> empty");
        CHECK(explodeFacesRadial(shapes::MeshData{}, 1.0f).vertices.empty(), "explodeFacesRadial empty -> empty");
    }

    if (g_fail == 0) {
        std::printf("meshexplode: OK — faces unweld and push out along normal / from centre, corners carry face normals.\n");
        return 0;
    }
    std::printf("meshexplode: %d failure(s).\n", g_fail);
    return 1;
}
