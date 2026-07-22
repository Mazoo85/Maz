#pragma once

#include "maz/render/Renderer.hpp" // Color (r,g,b,a floats)
#include "maz/render/Shapes.hpp"   // shapes::MeshData, MeshVertex

#include <algorithm>
#include <cmath>
#include <cstddef>

// maz::render VERTEX-COLOUR GRADIENT PAINT — tint a mesh's per-vertex RGB by WHERE each vertex sits, in one call.
// Two of the most common "give it a look without a texture" moves an artist makes:
//   • an AXIS gradient — colour fading from one shade to another along X, Y or Z. Green grass at a hill's base
//     fading to brown rock at its peak; a wall darker at the floor and lighter at the ceiling; a blade glowing
//     hotter toward its tip. `t` runs 0→1 as the vertex moves from `axisMin` to `axisMax` along the chosen axis
//     and the colour is a straight blend from `low` to `high`.
//   • a RADIAL gradient — colour fading outward from a point. A glow or scorch mark around a hit, a spotlight
//     pool on a floor, a target ring. `t` runs 0→1 as the vertex's distance from `centre` grows from `inner`
//     to `outer`, blending `innerColor` to `outerColor`.
// Both write straight into the RGB the mesh already stores (alpha is ignored) and leave positions, normals and
// UVs untouched — so the result composes with the other vertex-colour tools (smooth it with M564, add M556
// cavity, bake M553 AO). Header-only, pure CPU.
//
// Scope note (honest): this is a hard REPLACE of each vertex's RGB, not a blend over the existing colour — call
// it first, then layer AO/cavity/smoothing on top. The blend is linear in the stored [0,1] RGB (no gamma
// correction and no easing curve); pre-smooth the mesh or run M564 afterwards if you want a softer ramp. For the
// axis gradient, leaving `axisMin`/`axisMax` at their defaults (min ≥ max) auto-fits the range to the mesh's
// bounding box along that axis, so the gradient always spans the whole model.
namespace maz::render {

// Linear blend between two colours (RGB only; alpha left at Color's default). t is clamped to [0,1].
inline Color lerpColor(const Color& a, const Color& b, float t) {
    const float u = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    Color c;
    c.r = a.r + (b.r - a.r) * u;
    c.g = a.g + (b.g - a.g) * u;
    c.b = a.b + (b.b - a.b) * u;
    return c;
}

// Paint every vertex's RGB by its position along a world axis (0=X, 1=Y, 2=Z). t = (pos[axis]-axisMin) /
// (axisMax-axisMin) clamped to [0,1], then rgb = blend(low, high, t). If axisMin >= axisMax (the default), the
// range is taken from the mesh's bounding box along `axis`. A zero-extent axis paints everything `low`.
inline shapes::MeshData paintAxisGradient(const shapes::MeshData& mesh, int axis, const Color& low,
                                          const Color& high, float axisMin = 1.0f, float axisMax = 0.0f) {
    shapes::MeshData out = mesh;
    if (out.vertices.empty()) return out;
    const int a = axis < 0 ? 0 : (axis > 2 ? 2 : axis);

    auto coord = [a](const MeshVertex& v) -> float {
        return a == 0 ? v.px : (a == 1 ? v.py : v.pz);
    };

    float lo = axisMin, hi = axisMax;
    if (lo >= hi) { // auto-fit to the mesh's extent along this axis
        lo = coord(out.vertices[0]);
        hi = lo;
        for (const MeshVertex& v : out.vertices) {
            const float c = coord(v);
            if (c < lo) lo = c;
            if (c > hi) hi = c;
        }
    }
    const float span = hi - lo;
    const float invSpan = span > 0.0f ? 1.0f / span : 0.0f; // zero extent -> every t is 0 -> all `low`

    for (MeshVertex& v : out.vertices) {
        const float t = (coord(v) - lo) * invSpan;
        const Color c = lerpColor(low, high, t);
        v.r = c.r;
        v.g = c.g;
        v.b = c.b;
    }
    return out;
}

// Paint every vertex's RGB by its distance from `centre`. t = (dist-inner)/(outer-inner) clamped to [0,1], then
// rgb = blend(innerColor, outerColor, t). If outer <= inner the ramp collapses to a hard ring at `inner`
// (dist <= inner -> innerColor, else outerColor).
inline shapes::MeshData paintRadialGradient(const shapes::MeshData& mesh, float cx, float cy, float cz,
                                            float inner, float outer, const Color& innerColor,
                                            const Color& outerColor) {
    shapes::MeshData out = mesh;
    const float band = outer - inner;
    const float invBand = band > 0.0f ? 1.0f / band : 0.0f;

    for (MeshVertex& v : out.vertices) {
        const float dx = v.px - cx, dy = v.py - cy, dz = v.pz - cz;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        float t;
        if (invBand > 0.0f) {
            t = (dist - inner) * invBand;
        } else {
            t = dist <= inner ? 0.0f : 1.0f; // degenerate band -> hard step at `inner`
        }
        const Color c = lerpColor(innerColor, outerColor, t);
        v.r = c.r;
        v.g = c.g;
        v.b = c.b;
    }
    return out;
}

} // namespace maz::render
