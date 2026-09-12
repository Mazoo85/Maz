#pragma once

#include "maz/math/Math.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace maz::render {

// Billboarding — Godot's SpriteBase3D / GeometryInstance3D billboard modes. A billboard is a flat quad that
// turns to face the camera every frame, so a 2D image reads as a 3D object: trees, grass, smoke, health
// bars, distant-object impostors. This builds the model matrix that orients such a quad. Godot exposes three
// modes and so does this: `Disabled` (no turning — an ordinary placed quad), `Enabled` (full billboard — the
// quad's plane always squarely faces the camera), and `YBillboard` (fixed-Y — the quad yaws to face the
// camera horizontally but stays perfectly upright, the right choice for trees/characters that shouldn't tip
// back when you look down at them). It reads the camera's basis straight out of the view matrix, so it needs
// no camera object — just the same `view` you feed the renderer. Header-only, math-only; unit-tests the
// matrix without a GPU.

enum class BillboardMode {
    Disabled,   // no billboarding — a normally-oriented quad
    Enabled,    // full billboard — the quad plane always faces the camera
    YBillboard, // fixed vertical axis — yaws to face the camera but stays upright
};

// Build the model matrix (translate * rotate * scale) that places a unit quad at `position`, scaled by
// `scale`, oriented per `mode` relative to the camera described by `view` (a world->view matrix).
inline math::mat4 buildBillboard(const math::vec3& position, const math::vec3& scale,
                                 const math::mat4& view, BillboardMode mode) {
    // The camera's world-space basis lives in the rows of the view matrix's rotation block.
    const math::vec3 camRight(view[0][0], view[1][0], view[2][0]);
    const math::vec3 camUp(view[0][1], view[1][1], view[2][1]);
    const math::vec3 camFwd(-view[0][2], -view[1][2], -view[2][2]); // direction the camera looks

    math::mat3 rot(1.0f);
    if (mode == BillboardMode::Enabled) {
        const math::vec3 x = glm::normalize(camRight);
        const math::vec3 y = glm::normalize(camUp);
        const math::vec3 z = glm::normalize(glm::cross(x, y)); // quad normal -> back toward the camera
        rot = math::mat3(x, y, z);
    } else if (mode == BillboardMode::YBillboard) {
        const math::vec3 up(0.0f, 1.0f, 0.0f);
        // Flatten the camera's look direction into the ground plane; the quad normal faces back along it.
        const math::vec3 fwdFlat(camFwd.x, 0.0f, camFwd.z);
        const float fl = glm::length(fwdFlat);
        const math::vec3 z = fl > 1e-4f ? glm::normalize(-fwdFlat)
                                        : glm::normalize(math::vec3(camRight.x, 0.0f, camRight.z));
        const math::vec3 x = glm::normalize(glm::cross(up, z));
        const math::vec3 zz = glm::cross(x, up); // re-orthogonalize
        rot = math::mat3(x, up, zz);
    }
    // Disabled leaves rot as identity.

    const math::mat4 t = glm::translate(math::mat4(1.0f), position);
    const math::mat4 s = glm::scale(math::mat4(1.0f), scale);
    return t * math::mat4(rot) * s;
}

} // namespace maz::render
