#pragma once

#include "maz/render/MeshMerge.hpp"
#include "maz/render/MeshTransform.hpp"
#include "maz/render/Shapes3D.hpp"

#include <vector>

// "SENTINEL" — a stylized humanoid android built entirely from Maz's own procedural mesh primitives
// (sphere / box / cylinder / capsule) welded together with the engine's mesh-transform + merge ops.
// No external model file: the whole character is authored in code, so it is deterministic, tiny, and
// unit-testable, and it drops straight into the lit-mesh path. The geometry is split into three
// material groups so the renderer can give each the right physically-based look:
//
//   body — brushed titanium plates (metallic, low roughness): head, torso, pelvis, thighs, feet, arms
//   dark — matte charcoal joints/underlayer (near-dielectric, rough): neck, forearms, hands, shins, knees
//   glow — cyan emissive accents: the visor, the chest core, and the edge strips
//
// The android faces +Z, stands on the y=0 ground plane, and is ~1.95 units tall. Per-part colour is
// baked into vertex colours; the metallic/roughness/emissive of each group is chosen by the caller.
namespace character {

using maz::render::Color;
using maz::render::mergeMeshes;
using maz::render::applyTransform;
using maz::render::rotationMatrix;
using maz::render::scaleMatrix;
using maz::render::translationMatrix;
using maz::math::vec3;
namespace shapes = maz::render::shapes;

// The three material groups. Each is one merged vertex/index buffer.
struct CharacterModel {
    shapes::MeshData body; // brushed metal
    shapes::MeshData dark; // matte joints
    shapes::MeshData glow; // emissive accents
};

namespace detail {

// A rectangular box of arbitrary dimensions (makeBox gives a unit cube; scale it).
inline shapes::MeshData boxDim(float sx, float sy, float sz, const Color& c) {
    return applyTransform(shapes::makeBox(1.0f, c), scaleMatrix({sx, sy, sz}));
}

// Place: translate a mesh to a world position (optionally pre-rotated about Z then X, in radians).
inline shapes::MeshData place(const shapes::MeshData& m, vec3 pos, float rotZ = 0.0f,
                              float rotX = 0.0f) {
    shapes::MeshData r = m;
    if (rotX != 0.0f) {
        r = applyTransform(r, rotationMatrix({1, 0, 0}, rotX));
    }
    if (rotZ != 0.0f) {
        r = applyTransform(r, rotationMatrix({0, 0, 1}, rotZ));
    }
    return applyTransform(r, translationMatrix(pos));
}

// Accumulate a mesh into a group.
inline void add(shapes::MeshData& group, const shapes::MeshData& part) {
    group = mergeMeshes(group, part);
}

// Add a part and its mirror across the x=0 plane (left+right limbs). Mirroring negates x on both the
// position and the geometry; applyTransform with a negative-x scale flips winding, so we instead build
// the mirrored copy by negating the x of an already-placed part via a scale about the origin AFTER
// placement — but that flips winding too. Simplest correct route: place twice with +x and -x and
// mirror the Z-rotation, which keeps winding intact for our symmetric parts.
inline void addPair(shapes::MeshData& group, const shapes::MeshData& part, vec3 rightPos,
                    float rotZ = 0.0f, float rotX = 0.0f) {
    add(group, place(part, rightPos, rotZ, rotX));
    add(group, place(part, {-rightPos.x, rightPos.y, rightPos.z}, -rotZ, rotX));
}

} // namespace detail

inline CharacterModel buildCharacter() {
    using namespace detail;
    CharacterModel m;

    // ---- palette ----------------------------------------------------------------------------
    const Color kSteel{0.34f, 0.38f, 0.45f, 1.0f};   // brushed titanium (mid-tone so metal keeps depth)
    const Color kTeal{0.04f, 0.30f, 0.42f, 1.0f};    // anodized hero-blue plates
    const Color kDark{0.05f, 0.06f, 0.08f, 1.0f};    // charcoal joints
    const Color kGlow{0.15f, 0.90f, 1.0f, 1.0f};     // cyan emissive

    // ---- legs (metal thighs/feet, dark shins/knees) -----------------------------------------
    const float hipX = 0.17f;
    // Feet — elongated boxes reaching forward.
    addPair(m.body, place(boxDim(0.19f, 0.11f, 0.36f, kSteel), {0, 0, 0.06f}), {hipX, 0.055f, 0.0f});
    // Shins — dark capsules.
    addPair(m.dark, shapes::makeCapsule(0.080f, 0.34f, 20, 6, kDark), {hipX, 0.36f, 0.01f});
    // Knees — dark spheres.
    addPair(m.dark, shapes::makeSphere(0.090f, 16, 22, kDark), {hipX, 0.60f, 0.0f});
    // Thighs — metal capsules.
    addPair(m.body, shapes::makeCapsule(0.100f, 0.30f, 22, 6, kSteel), {hipX, 0.84f, 0.0f});

    // ---- pelvis + torso ----------------------------------------------------------------------
    add(m.body, place(boxDim(0.44f, 0.22f, 0.26f, kTeal), {0, 1.02f, 0.0f}));      // pelvis (accent)
    add(m.body, place(boxDim(0.40f, 0.26f, 0.24f, kSteel), {0, 1.24f, 0.0f}));     // waist
    add(m.body, place(boxDim(0.52f, 0.34f, 0.28f, kSteel), {0, 1.54f, 0.0f}));     // chest
    add(m.body, place(boxDim(0.30f, 0.22f, 0.05f, kSteel), {0, 1.54f, 0.150f}));   // chest panel (raised)

    // ---- shoulders + arms --------------------------------------------------------------------
    const float shX = 0.30f;
    addPair(m.body, shapes::makeSphere(0.115f, 18, 24, kTeal), {shX, 1.60f, 0.0f});       // shoulder pauldrons (accent)
    addPair(m.body, shapes::makeCapsule(0.078f, 0.26f, 20, 6, kSteel), {shX + 0.02f, 1.40f, 0.0f}); // upper arm
    addPair(m.dark, shapes::makeSphere(0.072f, 14, 20, kDark), {shX + 0.03f, 1.18f, 0.0f});          // elbow
    addPair(m.dark, shapes::makeCapsule(0.066f, 0.24f, 18, 6, kDark), {shX + 0.03f, 0.98f, 0.0f});   // forearm
    addPair(m.dark, boxDim(0.10f, 0.17f, 0.09f, kDark), {shX + 0.03f, 0.77f, 0.0f});                 // hand

    // ---- neck + head -------------------------------------------------------------------------
    add(m.dark, place(shapes::makeCylinder(0.060f, 0.12f, 16, kDark), {0, 1.74f, 0.0f})); // neck
    // Head: a slightly taller helmet (scaled sphere) + a brow ridge, sitting close over the chest.
    add(m.body, place(applyTransform(shapes::makeSphere(0.175f, 22, 30, kSteel),
                                     scaleMatrix({0.95f, 1.06f, 1.0f})),
                      {0, 1.90f, 0.0f}));
    add(m.body, place(boxDim(0.26f, 0.06f, 0.10f, kTeal), {0, 1.96f, 0.11f}));   // brow (accent)
    // Crest fin on top for silhouette.
    add(m.body, place(boxDim(0.035f, 0.18f, 0.14f, kSteel), {0, 2.12f, -0.01f}));

    // ---- glowing accents ---------------------------------------------------------------------
    add(m.glow, place(boxDim(0.23f, 0.055f, 0.05f, kGlow), {0, 1.885f, 0.168f}));  // visor
    add(m.glow, place(shapes::makeSphere(0.058f, 16, 22, kGlow), {0, 1.54f, 0.175f})); // chest core
    add(m.glow, place(boxDim(0.16f, 0.03f, 0.03f, kGlow), {0, 2.15f, -0.01f}));  // crest tip glow
    // Thin energy strips down the outer thighs and forearms.
    addPair(m.glow, boxDim(0.028f, 0.28f, 0.028f, kGlow), {hipX + 0.105f, 0.84f, 0.0f});
    addPair(m.glow, boxDim(0.024f, 0.20f, 0.024f, kGlow), {shX + 0.085f, 0.98f, 0.0f});

    return m;
}

} // namespace character
