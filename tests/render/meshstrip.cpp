// tests/render/meshstrip.cpp — verifies triangle-strip generation (render::buildTriangleStrips) and its exact
// inverse (expandTriangleStrips). The core invariant: packing a triangle list into strips and expanding it back
// must reproduce the SAME SET of triangles (each face's three vertices), and a connected mesh must actually
// compress (fewer indices than 3*triangleCount). Pure CPU, headless.
#include "maz/render/MeshStrip.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

// The multiset of triangles as sorted vertex-triples — winding-agnostic identity of a triangle set.
static std::vector<std::array<std::uint32_t, 3>> triSet(const std::vector<std::uint32_t>& idx) {
    std::vector<std::array<std::uint32_t, 3>> s;
    for (std::size_t t = 0; t + 2 < idx.size(); t += 3) {
        std::array<std::uint32_t, 3> tri = {idx[t], idx[t + 1], idx[t + 2]};
        std::sort(tri.begin(), tri.end());
        s.push_back(tri);
    }
    std::sort(s.begin(), s.end());
    return s;
}

// A grid of quads split into triangles: N*N quads -> 2*N*N triangles, highly strippable.
static std::vector<std::uint32_t> gridIndices(int N) {
    std::vector<std::uint32_t> idx;
    auto id = [&](int x, int z) { return static_cast<std::uint32_t>(z * (N + 1) + x); };
    for (int z = 0; z < N; ++z)
        for (int x = 0; x < N; ++x) {
            idx.insert(idx.end(), {id(x, z), id(x + 1, z), id(x + 1, z + 1),
                                   id(x, z), id(x + 1, z + 1), id(x, z + 1)});
        }
    return idx;
}

int main() {
    // --- 1. Grid: strips round-trip the triangle set exactly, and actually compress. ---
    {
        const std::vector<std::uint32_t> tris = gridIndices(6);
        const TriangleStrips strips = buildTriangleStrips(tris);
        const std::vector<std::uint32_t> back = expandTriangleStrips(strips);
        CHECK(strips.triangleCount == tris.size() / 3, "all triangles accounted for");
        CHECK(triSet(back) == triSet(tris), "expanded strips reproduce the original triangle set");
        CHECK(strips.stripCount >= 1, "at least one strip");
        CHECK(strips.stripCount < strips.triangleCount, "stripping merged triangles into runs");
        CHECK(strips.indexCountVerts() < 3 * strips.triangleCount, "strip form uses fewer indices than the list");
    }

    // --- 2. Two triangles sharing an edge become one strip of four. ---
    {
        const std::vector<std::uint32_t> quad = {0,1,2, 0,2,3};
        const TriangleStrips strips = buildTriangleStrips(quad);
        CHECK(strips.stripCount == 1, "the quad is a single strip");
        CHECK(strips.indices.size() == 4, "one strip of four vertices for two triangles");
        CHECK(triSet(expandTriangleStrips(strips)) == triSet(quad), "quad round-trips");
    }

    // --- 3. A single triangle is a strip of three, round-tripping to itself. ---
    {
        const std::vector<std::uint32_t> one = {5,6,7};
        const TriangleStrips strips = buildTriangleStrips(one);
        CHECK(strips.stripCount == 1 && strips.indices.size() == 3, "single triangle -> strip of 3");
        CHECK(triSet(expandTriangleStrips(strips)) == triSet(one), "single triangle round-trips");
    }

    // --- 4. Disconnected triangles become separate strips (no false merges). ---
    {
        const std::vector<std::uint32_t> two = {0,1,2, 10,11,12}; // share nothing
        const TriangleStrips strips = buildTriangleStrips(two);
        CHECK(strips.stripCount == 2, "two disconnected triangles -> two strips");
        CHECK(triSet(expandTriangleStrips(strips)) == triSet(two), "disconnected pair round-trips");
    }

    // --- 5. Degenerate triangles are dropped, not stripped. ---
    {
        const std::vector<std::uint32_t> withDegen = {0,1,2, 3,3,3, 0,2,4};
        const TriangleStrips strips = buildTriangleStrips(withDegen);
        CHECK(strips.triangleCount == 2, "the degenerate triangle is dropped");
        const std::vector<std::uint32_t> good = {0,1,2, 0,2,4};
        CHECK(triSet(expandTriangleStrips(strips)) == triSet(good), "only the real triangles survive");
    }

    // --- 6. A cube round-trips its 12 triangles. ---
    {
        std::vector<std::uint32_t> cube = {
            0,2,1, 0,3,2,  4,5,6, 4,6,7,  0,1,5, 0,5,4,
            3,7,6, 3,6,2,  0,4,7, 0,7,3,  1,2,6, 1,6,5};
        const TriangleStrips strips = buildTriangleStrips(cube);
        CHECK(strips.triangleCount == 12, "cube has 12 triangles");
        CHECK(triSet(expandTriangleStrips(strips)) == triSet(cube), "cube triangle set round-trips");
    }

    // --- 7. Empty input is safe. ---
    {
        const TriangleStrips strips = buildTriangleStrips(std::vector<std::uint32_t>{});
        CHECK(strips.indices.empty() && strips.stripCount == 0, "empty -> empty");
        CHECK(expandTriangleStrips(strips).empty(), "expanding empty -> empty");
    }

    if (g_fail == 0) {
        std::printf("meshstrip: OK — grid round-trips + compresses, quad->1 strip of 4, cube round-trips, degen dropped.\n");
        return 0;
    }
    std::printf("meshstrip: %d failure(s).\n", g_fail);
    return 1;
}
