#pragma once

#include "maz/math/Math.hpp"        // math::vec3
#include "maz/render/MeshTools.hpp" // computeNormals (smooth vertex normals)
#include "maz/render/Shapes.hpp"    // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render DISPLACE / ROUGHEN — push each vertex along its (smooth) surface normal by a procedural NOISE
// amount, so a too-perfect surface gains organic bumpiness: a flat plane becomes rough ground, a smooth sphere
// becomes a lumpy rock or asteroid, a cylinder becomes a gnarled tree trunk. This is Blender's "Displace" modifier
// driven by a noise texture — the cheapest way to make procedural or CAD-clean geometry look natural. The offset
// is coherent VALUE NOISE (nearby vertices move together, so the surface undulates instead of turning to static)
// scaled by `amplitude`, with `frequency` setting how fine the bumps are (low = broad swells, high = tight
// pebbling) and `seed` picking a different random field. Everything is deterministic: the same mesh + amplitude +
// frequency + seed always yields the exact same result, so it is safe for networked/replayed procedural content.
// Reuses the engine's area-weighted `computeNormals` for the push direction. Header-only, std-only.
//
// Scope note (honest): vertices move only ALONG their normals (no sideways drift), by at most `amplitude` in
// magnitude (the noise is bounded to [-1,1]); positions change and the stored normals go stale, so re-run
// `computeNormals` afterwards if you want the lighting to follow the new bumps. The detail you can add is limited
// by the mesh's existing resolution — displacing a 2-triangle quad just tilts it; subdivide first (Subdivision)
// for fine roughness. Amplitude may be negative; frequency <= 0 is treated as a single broad lump.
namespace maz::render {

namespace detail {

// A 32-bit integer lattice hash (deterministic, decent avalanche) for value noise.
inline std::uint32_t noiseHash3(int x, int y, int z, std::uint32_t seed) {
    std::uint32_t h = seed + 0x9E3779B9u;
    h ^= static_cast<std::uint32_t>(x) * 0x85EBCA6Bu; h = (h << 13) | (h >> 19); h *= 0xC2B2AE35u;
    h ^= static_cast<std::uint32_t>(y) * 0x27D4EB2Fu; h = (h << 15) | (h >> 17); h *= 0x165667B1u;
    h ^= static_cast<std::uint32_t>(z) * 0x9E3779B1u; h = (h << 11) | (h >> 21); h *= 0x85EBCA77u;
    h ^= h >> 16;
    return h;
}

// Lattice corner value in [-1, 1].
inline float noiseCorner(int x, int y, int z, std::uint32_t seed) {
    return static_cast<float>(noiseHash3(x, y, z, seed)) * (2.0f / 4294967295.0f) - 1.0f;
}

// Coherent 3D value noise in [-1, 1]: trilinear-interpolate the eight lattice corners with a smootherstep fade.
inline float valueNoise3(math::vec3 p, std::uint32_t seed) {
    const float fx = std::floor(p.x), fy = std::floor(p.y), fz = std::floor(p.z);
    const int ix = static_cast<int>(fx), iy = static_cast<int>(fy), iz = static_cast<int>(fz);
    const float tx = p.x - fx, ty = p.y - fy, tz = p.z - fz;
    auto fade = [](float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); };
    const float wx = fade(tx), wy = fade(ty), wz = fade(tz);
    auto lerp = [](float a, float b, float w) { return a + (b - a) * w; };

    const float c000 = noiseCorner(ix, iy, iz, seed),         c100 = noiseCorner(ix + 1, iy, iz, seed);
    const float c010 = noiseCorner(ix, iy + 1, iz, seed),     c110 = noiseCorner(ix + 1, iy + 1, iz, seed);
    const float c001 = noiseCorner(ix, iy, iz + 1, seed),     c101 = noiseCorner(ix + 1, iy, iz + 1, seed);
    const float c011 = noiseCorner(ix, iy + 1, iz + 1, seed), c111 = noiseCorner(ix + 1, iy + 1, iz + 1, seed);

    const float x00 = lerp(c000, c100, wx), x10 = lerp(c010, c110, wx);
    const float x01 = lerp(c001, c101, wx), x11 = lerp(c011, c111, wx);
    const float y0 = lerp(x00, x10, wy), y1 = lerp(x01, x11, wy);
    return lerp(y0, y1, wz);
}

} // namespace detail

struct DisplaceResult {
    shapes::MeshData mesh;      // the displaced mesh
    float maxOffset = 0.0f;      // the largest distance any vertex actually moved (<= |amplitude|)
};

// Push each vertex along its smooth normal by amplitude * valueNoise(position * frequency). Deterministic.
inline DisplaceResult displaceMesh(const shapes::MeshData& mesh, float amplitude, float frequency = 1.0f,
                                   std::uint32_t seed = 0u) {
    DisplaceResult out;
    out.mesh = mesh;
    const std::size_t n = mesh.vertices.size();
    if (n == 0 || mesh.indices.size() < 3) return out;

    std::vector<math::vec3> pos(n);
    for (std::size_t i = 0; i < n; ++i)
        pos[i] = math::vec3(mesh.vertices[i].px, mesh.vertices[i].py, mesh.vertices[i].pz);
    const std::vector<math::vec3> nrm = computeNormals(pos, mesh.indices);

    const float f = frequency > 0.0f ? frequency : 0.0f; // <=0 -> sample one lattice cell (a single broad lump)
    for (std::size_t i = 0; i < n; ++i) {
        const math::vec3 sample(pos[i].x * f, pos[i].y * f, pos[i].z * f);
        const float d = amplitude * detail::valueNoise3(sample, seed);
        out.mesh.vertices[i].px += nrm[i].x * d;
        out.mesh.vertices[i].py += nrm[i].y * d;
        out.mesh.vertices[i].pz += nrm[i].z * d;
        const float mag = std::fabs(d); // |normal| is 1 (or 0 for an unreferenced vertex)
        if (mag > out.maxOffset) out.maxOffset = mag;
    }
    return out;
}

} // namespace maz::render
