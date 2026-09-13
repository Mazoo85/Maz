#pragma once

#include "maz/film/Air.hpp"
#include "maz/math/Math.hpp"
#include "maz/render/Shapes.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

// maz::film AIR IN THE ROOM — the weather, as things with positions.
//
// `Air.hpp` draws weather in SCREEN space: streaks painted across the finished frame, deliberately not
// moving with the camera. That is the right answer for a painted film and it is the wrong one here,
// because in three dimensions the air is SOMEWHERE. Rain that falls between the camera and a face is
// the shot; rain painted on top of it is a screen saver. So this puts the same weather in the room —
// particles with positions, in front of some things and behind others, lit by the same key, getting
// smaller as they go back.
//
// It borrows Air.hpp's vocabulary rather than inventing its own, and that is the point: weatherFor()
// is what chooses, both renderers ask it, and a shot that rains in the flat look rains in the 3D one.
// Anything else and the Look control stops being a look and becomes a different film.
//
// Fog, haze and shimmer are not drawn here at all. They are the air ITSELF rather than things in it,
// and the 3D renderer already has them: every surface fades toward the sky colour with distance.
// Faking fog with a few big soft sprites on top of that would be worse, not better — two different
// depth cues disagreeing about how far away the back wall is.
namespace maz::film {

// Whether this weather is made of things you can see one at a time.
inline bool weatherHasParticles(Weather w) {
    return w == Weather::Rain || w == Weather::Dust || w == Weather::Embers;
}

namespace airdetail {
// A deterministic 0..1 from two integers. Nothing but integer arithmetic, so it is the same number in
// a browser as it is on a build machine — the whole reason the weather can be compared pixel for
// pixel between the two.
inline float rollOf(std::uint32_t seed, std::uint32_t index) {
    std::uint32_t h = seed * 2654435761u + index * 2246822519u;
    h ^= h >> 15;
    h *= 2654435761u;
    h ^= h >> 13;
    return static_cast<float>(h % 100000u) / 100000.0f;
}

// Wrap a value into [0, span), so a particle that falls off the bottom of the box comes back in at
// the top instead of the weather running out halfway through a shot.
inline float wrapped(float v, float span) {
    if (span <= 1e-6f) {
        return 0.0f;
    }
    const float k = std::floor(v / span);
    return v - k * span;
}

} // namespace airdetail

// The weather, as geometry, in the world.
//
// The particles fill a box in FRONT OF THE CAMERA rather than a fixed volume of the set: weather is
// everywhere, and a shot only ever sees the part of it the lens is pointed at. Filling the set
// instead means a long lens looks through an empty room while the rain falls somewhere off to the
// side.
//
//   eye, at   where the camera is and what it is looking at
//   seconds   the film's own clock, so the rain does not restart at every cut
//   tint      what colour the air is here, which is the set's own light
inline render::shapes::MeshData airMesh(Weather kind, const math::vec3& eye, const math::vec3& at,
                                        double seconds, std::uint32_t seed,
                                        const render::Color& tint, float mood) {
    render::shapes::MeshData m;
    if (!weatherHasParticles(kind)) {
        return m;
    }

    math::vec3 forward = at - eye;
    const float len = std::sqrt(math::dot(forward, forward));
    forward = len > 1e-5f ? forward / len : math::vec3(0.0f, 0.0f, 1.0f);
    math::vec3 right = math::cross(forward, math::vec3(0.0f, 1.0f, 0.0f));
    const float rl = std::sqrt(math::dot(right, right));
    right = rl > 1e-5f ? right / rl : math::vec3(1.0f, 0.0f, 0.0f);
    const math::vec3 upward = math::cross(right, forward);

    // How much of it there is, how fast it goes, how big each one is, and how bright. These are the
    // whole of the difference between rain and dust, and they are separated out rather than branched
    // through because that is what makes it possible to see, in one place, that snow is slow and
    // rain is not.
    int count = 260;
    float fall = 7.0f;      // metres a second, downward
    float drift = 0.0f;     // metres a second, sideways
    float length = 0.42f;   // how long a streak is
    float wide = 0.0045f;   // and how wide
    float far = 11.0f;      // how deep the box in front of the lens goes
    render::Color colour = tint;
    if (kind == Weather::Dust) {
        count = 150;
        fall = 0.10f;
        drift = 0.16f;
        length = 0.016f;
        wide = 0.015f;
        far = 7.0f;
        colour = render::Color{tint.r * 1.25f, tint.g * 1.20f, tint.b * 1.05f, 1.0f};
    } else if (kind == Weather::Embers) {
        count = 120;
        fall = -0.55f; // they go UP, which is the whole of what an ember is
        drift = 0.28f;
        length = 0.034f;
        wide = 0.024f;
        far = 12.0f;
        colour = render::Color{1.0f, 0.62f, 0.22f, 1.0f};
    }
    // Harder weather in a wound-up shot, which is a cheat and is also what a director would ask for.
    const float harder = 1.0f + 0.35f * (mood < 0.0f ? 0.0f : (mood > 1.0f ? 1.0f : mood));
    count = static_cast<int>(static_cast<float>(count) * harder);
    fall *= harder;

    // The box, in camera space: wide enough to cover the lens at the far plane, tall enough that the
    // top of it is out of frame.
    const float near = 1.15f;
    const float half = 4.2f;
    const float high = 5.0f;
    const float span = high * 2.0f;

    m.vertices.reserve(static_cast<std::size_t>(count) * 4u);
    m.indices.reserve(static_cast<std::size_t>(count) * 6u);
    for (int i = 0; i < count; ++i) {
        const std::uint32_t id = static_cast<std::uint32_t>(i);
        // One roll per statement. Two of them inside one call and the compiler is free to evaluate
        // them in either order — and GCC and Clang really do choose differently, so the same film
        // would rain in different places depending on which one built the renderer. That has happened
        // here once already and it cost an afternoon to find.
        const float a = airdetail::rollOf(seed, id * 3u + 1u);
        const float b = airdetail::rollOf(seed, id * 3u + 2u);
        const float c = airdetail::rollOf(seed, id * 3u + 3u);

        const float sideways = (a * 2.0f - 1.0f) * half;
        const float depth = near + b * (far - near);
        // Every particle on its own clock, so they do not all cross the frame in step.
        const float phase = c * span;
        const float t = static_cast<float>(seconds);
        const float dropped = airdetail::wrapped(phase + fall * t, span);
        const float height = high - dropped;
        const float sway = drift * std::sin(t * 0.9f + c * 6.2831853f);

        const math::vec3 centre = eye + forward * depth + right * (sideways + sway) + upward * height;
        // A streak lies along the way it is falling; a flake or a mote is square to the lens. Either
        // way the width runs across the view, or the quad is invisible edge-on to the camera.
        const math::vec3 along = math::vec3(0.0f, kind == Weather::Embers ? length : -length, 0.0f);
        math::vec3 across = math::cross(along, forward);
        const float al = std::sqrt(math::dot(across, across));
        across = al > 1e-6f ? across * (wide / al) : right * wide;

        const math::vec3 p0 = centre - along * 0.5f - across;
        const math::vec3 p1 = centre - along * 0.5f + across;
        const math::vec3 p2 = centre + along * 0.5f + across;
        const math::vec3 p3 = centre + along * 0.5f - across;
        const std::uint32_t base = static_cast<std::uint32_t>(m.vertices.size());
        const math::vec3 facing = -forward;
        const math::vec3 pts[4] = {p0, p1, p2, p3};
        for (const math::vec3& p : pts) {
            m.vertices.push_back(render::MeshVertex{p.x, p.y, p.z, facing.x, facing.y, facing.z,
                                                    colour.r, colour.g, colour.b, 0.0f, 0.0f});
        }
        m.indices.push_back(base + 0);
        m.indices.push_back(base + 1);
        m.indices.push_back(base + 2);
        m.indices.push_back(base + 0);
        m.indices.push_back(base + 2);
        m.indices.push_back(base + 3);
    }
    return m;
}

// How solid the air is drawn, and how much of it lights itself. Rain is nearly transparent and snow is
// nearly not; an ember is its own light source and takes no notice of the key at all.
inline float airAlpha(Weather kind) {
    switch (kind) {
        case Weather::Rain: return 0.34f;
        case Weather::Dust: return 0.22f;
        case Weather::Embers: return 0.85f;
        default: return 0.0f;
    }
}

inline float airGlow(Weather kind) {
    return kind == Weather::Embers ? 0.85f : 0.10f;
}

} // namespace maz::film
