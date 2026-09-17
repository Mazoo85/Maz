#pragma once

#include "maz/render/Shapes.hpp"

#include <cstddef>
#include <cstdint>

// maz::render SURFACE GRAIN — break a flat colour up so a wall stops reading as paint.
//
// Every surface in these films is one flat colour. A wall, a floor, a table top: one value, edge to
// edge, and the eye reads that instantly as a drawing rather than a room. Nothing in a real room is
// one value — plaster is patchy, paint is uneven, a floor is worn where it is walked on and clean
// where it is not.
//
// The field itself, `grainAt`, is used in two places and they do different jobs.
//
// `SoftRaster.hpp` samples it PER PIXEL, near the camera, and that is what actually makes a surface
// look like a material. It costs about two milliseconds of a frame in a browser, which the budget
// turns out to have.
//
// `grainMesh` below tints VERTICES, which costs nothing per pixel because the rasteriser already
// interpolates colour. That was tried first as the whole answer and it is not: with the walls cut
// into a grid a third of a metre across and the amount pushed to three and a half times what was
// wanted, the difference was still barely visible — a third of a metre four metres away is about ten
// pixels, and a ramp over ten pixels reads as lighting. What it IS good for is the thing a box with
// no vertices inside it can still show: one nudge of its own, so that five identical crates stop
// being five copies of one crate.
//
// WORLD SPACE, not object space. Two boxes that meet — a wall and its skirting board, a floor and the
// step onto it — have to agree about the dirt where they meet, or the seam between them lights up.
// Sampling by where a vertex IS rather than where it is within its own box gets that for free.
//
// DETERMINISTIC, and that is not decoration. This mesh is built twice, once by the command-line
// renderer and once by the same code compiled to WebAssembly, and a test compares the two pictures.
// So: integer hashing and plain float arithmetic only. No `sin`, no `pow`, nothing from a maths
// library that two platforms are allowed to round differently.
namespace maz::render {

namespace grain {

// An integer avalanche. Every output bit depends on every input bit, which is all that is wanted:
// neighbouring cells have to come out unrelated, or the noise has visible structure in it.
inline std::uint32_t hash3(std::int32_t x, std::int32_t y, std::int32_t z, std::uint32_t seed) {
    std::uint32_t h = seed * 0x9e3779b9u;
    h ^= static_cast<std::uint32_t>(x) * 0x85ebca6bu;
    h = (h ^ (h >> 15)) * 0xc2b2ae35u;
    h ^= static_cast<std::uint32_t>(y) * 0x27d4eb2fu;
    h = (h ^ (h >> 13)) * 0x165667b1u;
    h ^= static_cast<std::uint32_t>(z) * 0x9e3779b9u;
    h = (h ^ (h >> 16)) * 0x7feb352du;
    return h ^ (h >> 15);
}

// The hash as a float in [0, 1). Twenty-four bits, so it divides exactly and the same everywhere.
inline float unitOf(std::uint32_t h) {
    return static_cast<float>(h >> 8) * (1.0f / 16777216.0f);
}

inline std::int32_t floorToInt(float v) {
    const auto i = static_cast<std::int32_t>(v);
    return v < static_cast<float>(i) ? i - 1 : i;
}

// Smoothstep, written out. Interpolating the cells linearly leaves a visible crease along every cell
// boundary, because the gradient jumps there; this has zero gradient at both ends and the creases go.
inline float ease(float t) { return t * t * (3.0f - 2.0f * t); }

} // namespace grain

// Value noise in three dimensions, in [-1, 1]. `cell` is the size of one cell in metres, so the
// number means something physical: 0.4 is a patch the size of a hand, 2.0 is a patch the size of a
// person.
inline float grainAt(float x, float y, float z, float cell, std::uint32_t seed) {
    if (cell <= 0.0f) {
        return 0.0f;
    }
    const float inv = 1.0f / cell;
    // TURNED, before anything else, and this is a fix rather than a flourish.
    //
    // Value noise is built on a grid, and every wall, floor and ceiling in these sets is axis
    // aligned — so the grid lined up with the surfaces exactly, and at any amount worth seeing a
    // regular diagonal lattice came up across the flat ones. It looked like woven fabric, which is
    // an interesting thing for a wall to look like and not the thing that was wanted.
    //
    // Sampling on a turned frame means no flat surface in the film can ever be parallel to the
    // lattice. The numbers are a rotation of 0.9 radians about (0.3, 0.87, 0.39) normalised, worked
    // out once and written down rather than computed here — a `sin` at every pixel would be both
    // wasteful and, being a maths-library call, the one thing this file must not contain.
    const float rx = x * 0.655699f + y * -0.206792f + z * 0.726151f;
    const float ry = x * 0.404509f + y * 0.908300f + z * -0.106599f;
    const float rz = x * -0.637519f + y * 0.363632f + z * 0.679221f;
    const float fx = rx * inv;
    const float fy = ry * inv;
    const float fz = rz * inv;
    const std::int32_t ix = grain::floorToInt(fx);
    const std::int32_t iy = grain::floorToInt(fy);
    const std::int32_t iz = grain::floorToInt(fz);
    const float tx = grain::ease(fx - static_cast<float>(ix));
    const float ty = grain::ease(fy - static_cast<float>(iy));
    const float tz = grain::ease(fz - static_cast<float>(iz));

    float face[2];
    for (int dz = 0; dz < 2; ++dz) {
        float row[2];
        for (int dy = 0; dy < 2; ++dy) {
            const float a = grain::unitOf(grain::hash3(ix, iy + dy, iz + dz, seed));
            const float b = grain::unitOf(grain::hash3(ix + 1, iy + dy, iz + dz, seed));
            row[dy] = a + (b - a) * tx;
        }
        face[dz] = row[0] + (row[1] - row[0]) * ty;
    }
    return (face[0] + (face[1] - face[0]) * tz) * 2.0f - 1.0f;
}

// How a surface is worn.
struct Grain {
    // Two scales, because one is not enough and three cannot be told from two. The broad one is what
    // stops a wall being one value; the close one is what stops the broad one looking like a stain.
    float broad = 1.7f;   // metres per cell of the large variation
    float close = 0.38f;  // metres per cell of the small variation
    float amount = 0.10f; // how far the brightness swings either way, as a fraction of itself

    // Rooms are dirty at the bottom. Skirting boards are scuffed, floors are worn where they are
    // walked, and the corner where a wall meets a floor collects everything. 0 turns it off.
    float low = 0.0f;       // how much darker the bottom is
    float lowOver = 1.1f;   // metres over which that fades back to clean
    float floorAt = 0.0f;   // the height the darkening is measured from

    std::uint32_t seed = 1u;
};

// Tint every vertex of a mesh by where it is in the world.
//
// Brightness only — every channel by the same factor. Letting the hue wander as well was tried and
// is wrong here: the palette is chosen per shot and per genre and carries the whole mood of the
// film, and noise that pulls colours off it fights the one decision the picture most depends on.
inline void grainMesh(shapes::MeshData& mesh, const Grain& g) {
    if (g.amount <= 0.0f && g.low <= 0.0f) {
        return;
    }
    for (MeshVertex& v : mesh.vertices) {
        float k = 1.0f;
        if (g.amount > 0.0f) {
            // The two scales are given different seeds rather than different offsets. Offsetting one
            // sample of the same field leaves the two correlated along the offset, which shows up as
            // faint diagonal banding — seeding them apart makes them genuinely independent.
            const float a = grainAt(v.px, v.py, v.pz, g.broad, g.seed);
            const float b = grainAt(v.px, v.py, v.pz, g.close, g.seed ^ 0x5bf03635u);
            k += g.amount * (a * 0.68f + b * 0.32f);
        }
        if (g.low > 0.0f && g.lowOver > 0.0f) {
            const float up = (v.py - g.floorAt) / g.lowOver;
            const float near_ = up <= 0.0f ? 1.0f : (up >= 1.0f ? 0.0f : 1.0f - up);
            // Squared, so the darkening is concentrated in the bottom few centimetres rather than
            // spread evenly up the wall — which is where it actually is, and which also keeps it out
            // of the way of anybody's face.
            k -= g.low * near_ * near_;
        }
        if (k < 0.0f) {
            k = 0.0f;
        }
        v.r *= k;
        v.g *= k;
        v.b *= k;
    }
}

} // namespace maz::render
