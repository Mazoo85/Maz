#pragma once

#include "maz/render/MeshMerge.hpp"
#include "maz/render/MeshTransform.hpp"
#include "maz/render/Shapes3D.hpp"

#include <cmath>
#include <vector>

// "SENTINEL" — a stylized humanoid android built entirely from Maz's own procedural mesh primitives
// (sphere / box / cylinder / capsule) welded together with the engine's mesh-transform + merge ops.
// No external model file: the whole character is authored in code, so it is deterministic, tiny, and
// unit-testable, and it drops straight into the lit-mesh path. The geometry is split into three
// material groups so the renderer can give each the right physically-based look:
//
//   body — brushed metal plates (metallic, low roughness): head, torso, pelvis, thighs, feet, arms
//   dark — matte charcoal joints/underlayer (near-dielectric, rough): neck, forearms, hands, shins, knees
//   glow — emissive accents: the visor, the chest core, and the edge strips
//
// The android faces +Z, stands on the y=0 ground plane, and is ~2.1 units tall. buildCharacter() takes a
// Pose (arm attitude) and a Theme (colour palette); crucially the vertex/index COUNTS are identical for
// every pose+theme (only positions/normals/colours differ), so a viewer can stream new poses into a single
// dynamic mesh via Renderer::updateMesh without reallocating.
namespace character {

using maz::render::Color;
using maz::render::mergeMeshes;
using maz::render::applyTransform;
using maz::render::rotationMatrix;
using maz::render::scaleMatrix;
using maz::render::translationMatrix;
using maz::math::vec3;
namespace shapes = maz::render::shapes;

enum class Pose { Idle, APose, Action };
enum class Theme { Cyan, Crimson, Verdant };

// A colour palette + the emissive colour the renderer uses for the glow group.
struct Palette {
    Color steel;       // main metal
    Color accent;      // anodized accent plates (shoulders, pelvis, brow)
    Color dark;        // matte joints
    Color glow;        // emissive vertex tint
    float emissive[3]; // emissive colour the Vulkan material adds to the glow group
};

inline Palette paletteFor(Theme t) {
    switch (t) {
        case Theme::Crimson:
            return Palette{Color{0.40f, 0.35f, 0.35f, 1.0f}, Color{0.48f, 0.06f, 0.10f, 1.0f},
                           Color{0.09f, 0.05f, 0.05f, 1.0f}, Color{1.0f, 0.45f, 0.20f, 1.0f},
                           {1.7f, 0.55f, 0.18f}};
        case Theme::Verdant:
            return Palette{Color{0.33f, 0.40f, 0.36f, 1.0f}, Color{0.06f, 0.40f, 0.22f, 1.0f},
                           Color{0.05f, 0.07f, 0.06f, 1.0f}, Color{0.45f, 1.0f, 0.45f, 1.0f},
                           {0.45f, 1.6f, 0.45f}};
        case Theme::Cyan:
        default:
            return Palette{Color{0.34f, 0.38f, 0.45f, 1.0f}, Color{0.04f, 0.30f, 0.42f, 1.0f},
                           Color{0.05f, 0.06f, 0.08f, 1.0f}, Color{0.15f, 0.90f, 1.0f, 1.0f},
                           {0.25f, 1.30f, 1.60f}};
    }
}

// The three material groups. Each is one merged vertex/index buffer.
struct CharacterModel {
    shapes::MeshData body; // metal
    shapes::MeshData dark; // matte joints
    shapes::MeshData glow; // emissive accents
};

namespace detail {

inline shapes::MeshData boxDim(float sx, float sy, float sz, const Color& c) {
    return applyTransform(shapes::makeBox(1.0f, c), scaleMatrix({sx, sy, sz}));
}

// Place: translate a mesh to a world position, optionally pre-rotated about Z (radians).
inline shapes::MeshData place(const shapes::MeshData& m, vec3 pos, float rotZ = 0.0f) {
    shapes::MeshData r = m;
    if (rotZ != 0.0f) {
        r = applyTransform(r, rotationMatrix({0, 0, 1}, rotZ));
    }
    return applyTransform(r, translationMatrix(pos));
}

inline void add(shapes::MeshData& group, const shapes::MeshData& part) {
    group = mergeMeshes(group, part);
}

// Mirror a part across x=0 (left+right). Placing with +x and -x and mirrored Z-rotation keeps winding
// intact for our symmetric parts (a negative-x scale would flip triangle winding).
inline void addPair(shapes::MeshData& group, const shapes::MeshData& part, vec3 rightPos,
                    float rotZ = 0.0f) {
    add(group, place(part, rightPos, rotZ));
    add(group, place(part, {-rightPos.x, rightPos.y, rightPos.z}, -rotZ));
}

// Shoulder pose angles (radians) for right/left arms per pose. Positive swings the arm outward/up.
inline void poseArmAngles(Pose p, float& right, float& left) {
    switch (p) {
        case Pose::APose:  right = left = 0.62f; break;             // ~35° both, classic A-pose
        case Pose::Action: right = 2.15f; left = 0.32f; break;      // right arm thrown up, left slightly out
        case Pose::Idle:
        default:           right = left = 0.0f; break;              // arms hang
    }
}

} // namespace detail

inline CharacterModel buildCharacter(Pose pose = Pose::Idle, Theme theme = Theme::Cyan) {
    using namespace detail;
    CharacterModel m;
    const Palette pal = paletteFor(theme);
    const Color kSteel = pal.steel, kAccent = pal.accent, kDark = pal.dark, kGlow = pal.glow;

    // ---- legs (metal thighs/feet, dark shins/knees) — static across poses --------------------
    const float hipX = 0.17f;
    addPair(m.body, place(boxDim(0.19f, 0.11f, 0.36f, kSteel), {0, 0, 0.06f}), {hipX, 0.055f, 0.0f});
    addPair(m.dark, shapes::makeCapsule(0.080f, 0.34f, 20, 6, kDark), {hipX, 0.36f, 0.01f});
    addPair(m.dark, shapes::makeSphere(0.090f, 16, 22, kDark), {hipX, 0.60f, 0.0f});
    addPair(m.body, shapes::makeCapsule(0.100f, 0.30f, 22, 6, kSteel), {hipX, 0.84f, 0.0f});

    // ---- pelvis + torso ----------------------------------------------------------------------
    add(m.body, place(boxDim(0.44f, 0.22f, 0.26f, kAccent), {0, 1.02f, 0.0f}));    // pelvis (accent)
    add(m.body, place(boxDim(0.40f, 0.26f, 0.24f, kSteel), {0, 1.24f, 0.0f}));     // waist
    add(m.body, place(boxDim(0.52f, 0.34f, 0.28f, kSteel), {0, 1.54f, 0.0f}));     // chest
    add(m.body, place(boxDim(0.30f, 0.22f, 0.05f, kSteel), {0, 1.54f, 0.150f}));   // chest panel (raised)

    // ---- shoulders (static) + arms (posed) ---------------------------------------------------
    const float shX = 0.30f;
    addPair(m.body, shapes::makeSphere(0.115f, 18, 24, kAccent), {shX, 1.60f, 0.0f}); // pauldrons (accent)

    // Arm parts are placed relative to the shoulder pivot, then rotated about it by the pose angle.
    const shapes::MeshData upperArm = shapes::makeCapsule(0.078f, 0.26f, 20, 6, kSteel);
    const shapes::MeshData elbow = shapes::makeSphere(0.072f, 14, 20, kDark);
    const shapes::MeshData forearm = shapes::makeCapsule(0.066f, 0.24f, 18, 6, kDark);
    const shapes::MeshData hand = boxDim(0.10f, 0.17f, 0.09f, kDark);
    const shapes::MeshData armStrip = boxDim(0.024f, 0.20f, 0.024f, kGlow);
    float rightAng = 0.0f, leftAng = 0.0f;
    poseArmAngles(pose, rightAng, leftAng);
    auto buildArm = [&](float side, float ang) {
        const vec3 pivot(side * shX, 1.60f, 0.0f);
        const float a = side > 0 ? ang : -ang;
        const float ca = std::cos(a), sa = std::sin(a);
        auto armPart = [&](shapes::MeshData& group, const shapes::MeshData& mesh, vec3 rel) {
            const vec3 r(side * rel.x, rel.y, rel.z);
            const vec3 rr(r.x * ca - r.y * sa, r.x * sa + r.y * ca, r.z);
            add(group, place(mesh, pivot + rr, a));
        };
        armPart(m.body, upperArm, {0.02f, -0.20f, 0.0f});
        armPart(m.dark, elbow, {0.03f, -0.42f, 0.0f});
        armPart(m.dark, forearm, {0.03f, -0.62f, 0.0f});
        armPart(m.dark, hand, {0.03f, -0.83f, 0.0f});
        armPart(m.glow, armStrip, {0.085f, -0.62f, 0.0f});
    };
    buildArm(+1.0f, rightAng);
    buildArm(-1.0f, leftAng);

    // ---- neck + head -------------------------------------------------------------------------
    add(m.dark, place(shapes::makeCylinder(0.060f, 0.12f, 16, kDark), {0, 1.74f, 0.0f})); // neck
    add(m.body, place(applyTransform(shapes::makeSphere(0.175f, 22, 30, kSteel),
                                     scaleMatrix({0.95f, 1.06f, 1.0f})),
                      {0, 1.90f, 0.0f}));                                                  // head
    add(m.body, place(boxDim(0.26f, 0.06f, 0.10f, kAccent), {0, 1.96f, 0.11f}));          // brow (accent)
    add(m.body, place(boxDim(0.035f, 0.18f, 0.14f, kSteel), {0, 2.12f, -0.01f}));         // crest fin

    // ---- glowing accents (static) ------------------------------------------------------------
    add(m.glow, place(boxDim(0.23f, 0.055f, 0.05f, kGlow), {0, 1.885f, 0.168f}));  // visor
    add(m.glow, place(shapes::makeSphere(0.058f, 16, 22, kGlow), {0, 1.54f, 0.175f})); // chest core
    add(m.glow, place(boxDim(0.16f, 0.03f, 0.03f, kGlow), {0, 2.15f, -0.01f}));    // crest tip glow
    addPair(m.glow, boxDim(0.028f, 0.28f, 0.028f, kGlow), {hipX + 0.105f, 0.84f, 0.0f}); // thigh strips

    return m;
}

} // namespace character
