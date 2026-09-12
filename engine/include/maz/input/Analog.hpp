#pragma once

#include "maz/math/Math.hpp"

#include <cmath>

namespace maz::input {

// Analog-stick conditioning — Godot's `Input.get_vector` / `get_axis` deadzone maths.
//
// A raw thumbstick reports a 2D vector whose length can drift up to ~√2 at the diagonals and jitters
// around zero at rest. Feeding that straight into movement gives two classic bugs: the character
// creeps while the stick is "centred" (drift inside the deadzone), and moves ~40% faster on the
// diagonals than the cardinals (the square input region is bigger than the unit circle). Godot fixes
// both in `get_vector`: a **radial** deadzone (the whole vector's magnitude, not each axis) zeroes
// rest jitter, the remaining magnitude is **rescaled** so the deadzone edge maps to 0 and 1 maps to 1
// (no sudden jump as you leave the deadzone), and the magnitude is **clamped to the unit circle** so
// diagonals aren't faster. These are pure, stateless functions over raw values — no backend, no
// allocation — so they unit-test exactly and pair with any input source (the `platform::Input`
// gamepad axes, `ActionMap` axis actions, or a synthetic test vector).

// Clamp a deadzone parameter to a usable [0, 1) so the rescale denominator (1 - deadzone) is positive.
inline float sanitizeDeadzone(float deadzone) {
    if (deadzone < 0.0f) {
        return 0.0f;
    }
    if (deadzone > 0.999f) {
        return 0.999f;
    }
    return deadzone;
}

// One signed axis with a rescaled deadzone: |v| <= dz -> 0, otherwise the sign times
// (|v| - dz) / (1 - dz) clamped to 1. Godot's per-action strength remap, kept bidirectional.
inline float applyDeadzone(float value, float deadzone) {
    const float dz = sanitizeDeadzone(deadzone);
    const float m = std::fabs(value);
    if (m <= dz) {
        return 0.0f;
    }
    float t = (m - dz) / (1.0f - dz);
    if (t > 1.0f) {
        t = 1.0f;
    }
    return value < 0.0f ? -t : t;
}

// A 2D stick vector conditioned exactly like Godot `Input.get_vector`:
//   length <= deadzone            -> (0, 0)
//   length > 1                    -> raw / length            (clamp to the unit circle)
//   deadzone < length <= 1        -> (raw / length) * (length - deadzone) / (1 - deadzone)
// The result keeps the raw direction; its magnitude runs 0 at the deadzone edge to 1 at full tilt,
// and never exceeds 1 — so diagonals are no faster than cardinals.
inline math::vec2 analogVector(math::vec2 raw, float deadzone = 0.2f) {
    const float dz = sanitizeDeadzone(deadzone);
    const float length = std::sqrt(raw.x * raw.x + raw.y * raw.y);
    if (length <= dz || length <= 0.0f) {
        return math::vec2(0.0f, 0.0f);
    }
    if (length > 1.0f) {
        return math::vec2(raw.x / length, raw.y / length);
    }
    const float scale = ((length - dz) / (1.0f - dz)) / length;
    return math::vec2(raw.x * scale, raw.y * scale);
}

} // namespace maz::input
