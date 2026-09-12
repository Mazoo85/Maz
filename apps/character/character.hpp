#pragma once

#include "maz/render/MeshMerge.hpp"
#include "maz/render/MeshTransform.hpp"
#include "maz/render/Shapes3D.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <vector>

// "SENTINEL" — a stylized humanoid android built entirely from Maz's own procedural mesh primitives
// (sphere / box / cylinder / capsule) welded with the engine's mesh-transform + merge ops. No external
// model file — the whole character, its rig, and its animation are authored in code, so it is
// deterministic, tiny, and unit-testable, and it drops straight into the lit-mesh path.
//
// Geometry is split into three material groups for the PBR renderer:
//   body — brushed metal plates      dark — matte charcoal joints      glow — emissive accents
//
// The character is posed by a small RIG (shoulder + elbow joints, head look, body bob). buildCharacter()
// rebuilds the merged meshes for a given Rig + Theme; crucially the vertex/index COUNTS are identical for
// every rig+theme (only positions/normals/colours differ), so a viewer can stream a fresh animation frame
// into one dynamic mesh via Renderer::updateMesh every frame with no reallocation. animate() drives the
// rig procedurally (breathing idle, wave, power-up); the static presets (A-pose, action) ignore time.
namespace character {

using maz::render::Color;
using maz::render::mergeMeshes;
using maz::render::applyTransform;
using maz::render::scaleMatrix;
using maz::render::translationMatrix;
using maz::math::vec3;
using maz::math::mat4;
namespace shapes = maz::render::shapes;

// ---- themes ----------------------------------------------------------------------------------
enum class Theme { Cyan, Crimson, Verdant, Gold, Obsidian, Ice };
constexpr int kThemeCount = 6;

struct Palette {
    Color steel;       // main metal
    Color accent;      // anodized accent plates
    Color dark;        // matte joints
    Color glow;        // emissive vertex tint
    float emissive[3]; // emissive colour the Vulkan material adds to the glow group
};

inline Palette paletteFor(Theme t) {
    switch (t) {
        case Theme::Crimson:
            return {Color{0.40f, 0.35f, 0.35f, 1}, Color{0.48f, 0.06f, 0.10f, 1},
                    Color{0.09f, 0.05f, 0.05f, 1}, Color{1.0f, 0.45f, 0.20f, 1}, {1.7f, 0.55f, 0.18f}};
        case Theme::Verdant:
            return {Color{0.33f, 0.40f, 0.36f, 1}, Color{0.06f, 0.40f, 0.22f, 1},
                    Color{0.05f, 0.07f, 0.06f, 1}, Color{0.45f, 1.0f, 0.45f, 1}, {0.45f, 1.6f, 0.45f}};
        case Theme::Gold:
            return {Color{0.62f, 0.48f, 0.20f, 1}, Color{0.30f, 0.20f, 0.05f, 1},
                    Color{0.10f, 0.08f, 0.05f, 1}, Color{1.0f, 0.85f, 0.35f, 1}, {1.6f, 1.25f, 0.4f}};
        case Theme::Obsidian:
            return {Color{0.14f, 0.15f, 0.18f, 1}, Color{0.30f, 0.10f, 0.42f, 1},
                    Color{0.04f, 0.04f, 0.05f, 1}, Color{0.75f, 0.35f, 1.0f, 1}, {0.9f, 0.35f, 1.7f}};
        case Theme::Ice:
            return {Color{0.55f, 0.62f, 0.70f, 1}, Color{0.30f, 0.52f, 0.62f, 1},
                    Color{0.10f, 0.13f, 0.16f, 1}, Color{0.70f, 0.95f, 1.0f, 1}, {0.7f, 1.1f, 1.5f}};
        case Theme::Cyan:
        default:
            return {Color{0.34f, 0.38f, 0.45f, 1}, Color{0.04f, 0.30f, 0.42f, 1},
                    Color{0.05f, 0.06f, 0.08f, 1}, Color{0.15f, 0.90f, 1.0f, 1}, {0.25f, 1.30f, 1.60f}};
    }
}

// ---- rig + animation -------------------------------------------------------------------------
// Joint angles in radians; bodyY is a whole-body vertical offset (a bob/breathe). Right/left are the
// character's own right/left. All zero = a relaxed rest pose (arms down), which is left/right symmetric.
struct Rig {
    float bodyY = 0.0f;
    float headYaw = 0.0f, headPitch = 0.0f;
    float shR = 0.0f, shL = 0.0f;   // shoulder swing out/up (about Z)
    float shRx = 0.0f, shLx = 0.0f; // shoulder swing forward/back (about X)
    float elR = 0.0f, elL = 0.0f;   // elbow bend (about X)
};

enum class Anim { Idle, Wave, PowerUp, APose, Action };
constexpr int kAnimCount = 5;

// Procedural rig for an animation state at time t (seconds). Static presets ignore t.
inline Rig animate(Anim a, float t) {
    Rig r;
    const float s16 = std::sin(t * 1.6f);
    switch (a) {
        case Anim::Idle:
            r.bodyY = 0.020f * s16;
            r.shRx = r.shLx = 0.10f * s16;
            r.elR = r.elL = 0.18f;
            r.headYaw = 0.14f * std::sin(t * 0.7f);
            r.headPitch = 0.03f * std::sin(t * 1.1f) - 0.02f;
            break;
        case Anim::Wave:
            r.bodyY = 0.015f * s16;
            r.shLx = 0.09f * s16;
            r.elL = 0.20f;                       // left arm relaxed
            r.shR = 2.05f;                       // right arm up
            r.elR = 0.55f;
            r.shRx = 0.32f * std::sin(t * 7.0f); // waving
            r.headYaw = 0.10f;
            break;
        case Anim::PowerUp:
            r.shR = r.shL = 0.55f;               // arms out and slightly back
            r.shRx = r.shLx = -0.18f;
            r.elR = r.elL = 0.35f;
            r.headPitch = -0.12f;                // look up
            r.bodyY = 0.030f * std::sin(t * 3.0f);
            break;
        case Anim::APose:
            r.shR = r.shL = 0.62f;
            break;
        case Anim::Action:
            r.shR = 2.15f;
            r.shL = 0.32f;
            break;
    }
    return r;
}

// The three material groups. Each is one merged vertex/index buffer.
struct CharacterModel {
    shapes::MeshData body, dark, glow;
};

namespace detail {

inline mat4 rotX(float a) { return glm::rotate(mat4(1.0f), a, vec3(1, 0, 0)); }
inline mat4 rotY(float a) { return glm::rotate(mat4(1.0f), a, vec3(0, 1, 0)); }
inline mat4 rotZ(float a) { return glm::rotate(mat4(1.0f), a, vec3(0, 0, 1)); }

inline shapes::MeshData boxDim(float sx, float sy, float sz, const Color& c) {
    return applyTransform(shapes::makeBox(1.0f, c), scaleMatrix({sx, sy, sz}));
}
inline shapes::MeshData at(const shapes::MeshData& m, vec3 pos) {
    return applyTransform(m, translationMatrix(pos));
}
inline void add(shapes::MeshData& g, const shapes::MeshData& p) { g = mergeMeshes(g, p); }
inline void addXf(shapes::MeshData& g, const shapes::MeshData& p, const mat4& M) {
    add(g, applyTransform(p, M));
}
// Add a part and its x-mirror (symmetric parts keep winding when placed with +x/-x).
inline void addPair(shapes::MeshData& g, const shapes::MeshData& p, vec3 rightPos) {
    add(g, at(p, rightPos));
    add(g, at(p, {-rightPos.x, rightPos.y, rightPos.z}));
}

} // namespace detail

inline CharacterModel buildCharacter(const Rig& rig = {}, Theme theme = Theme::Cyan) {
    using namespace detail;
    CharacterModel m;
    const Palette pal = paletteFor(theme);
    const Color kSteel = pal.steel, kAccent = pal.accent, kDark = pal.dark, kGlow = pal.glow;

    // ---- legs (static) -----------------------------------------------------------------------
    const float hipX = 0.17f;
    addPair(m.body, at(boxDim(0.19f, 0.11f, 0.36f, kSteel), {0, 0, 0.06f}), {hipX, 0.055f, 0.0f});
    addPair(m.dark, shapes::makeCapsule(0.080f, 0.34f, 20, 6, kDark), {hipX, 0.36f, 0.01f});
    addPair(m.body, boxDim(0.13f, 0.10f, 0.15f, kAccent), {hipX, 0.60f, 0.055f}); // knee guards
    addPair(m.dark, shapes::makeSphere(0.090f, 16, 22, kDark), {hipX, 0.60f, 0.0f});
    addPair(m.body, shapes::makeCapsule(0.100f, 0.30f, 22, 6, kSteel), {hipX, 0.84f, 0.0f});

    // ---- pelvis + torso + backpack (static) --------------------------------------------------
    add(m.body, at(boxDim(0.44f, 0.22f, 0.26f, kAccent), {0, 1.02f, 0.0f}));    // pelvis
    add(m.body, at(boxDim(0.40f, 0.26f, 0.24f, kSteel), {0, 1.24f, 0.0f}));     // waist
    add(m.body, at(boxDim(0.52f, 0.34f, 0.28f, kSteel), {0, 1.54f, 0.0f}));     // chest
    add(m.body, at(boxDim(0.30f, 0.22f, 0.05f, kSteel), {0, 1.54f, 0.150f}));   // chest panel
    add(m.body, at(boxDim(0.34f, 0.30f, 0.10f, kSteel), {0, 1.46f, -0.175f}));  // backpack
    add(m.dark, at(boxDim(0.34f, 0.07f, 0.24f, kDark), {0, 1.70f, 0.0f}));      // collar
    addPair(m.glow, boxDim(0.05f, 0.05f, 0.04f, kGlow), {0.10f, 1.34f, -0.225f}); // back thrusters
    addPair(m.glow, boxDim(0.025f, 0.14f, 0.02f, kGlow), {0.13f, 1.54f, 0.145f}); // chest vents

    // ---- shoulders (static) + two-joint arms (posed) -----------------------------------------
    const float shX = 0.30f;
    addPair(m.body, shapes::makeSphere(0.115f, 18, 24, kAccent), {shX, 1.60f, 0.0f}); // pauldrons

    const shapes::MeshData upperArm = shapes::makeCapsule(0.078f, 0.26f, 20, 6, kSteel);
    const shapes::MeshData elbow = shapes::makeSphere(0.072f, 14, 20, kDark);
    const shapes::MeshData forearm = shapes::makeCapsule(0.066f, 0.24f, 18, 6, kDark);
    const shapes::MeshData hand = boxDim(0.10f, 0.17f, 0.09f, kDark);
    const shapes::MeshData armStrip = boxDim(0.024f, 0.20f, 0.024f, kGlow);
    auto buildArm = [&](float side, float shZ, float shXa, float elXa) {
        const vec3 pivot(side * shX, 1.60f, 0.0f);
        // Shoulder joint: swing about Z (mirrored for the left) then about X.
        const mat4 Tsh = translationMatrix(pivot) * rotZ(side > 0 ? shZ : -shZ) * rotX(shXa);
        addXf(m.body, upperArm, Tsh * translationMatrix({side * 0.02f, -0.20f, 0.0f}));
        // Elbow joint: 0.42 below the shoulder pivot, bends about X.
        const mat4 Tel = Tsh * translationMatrix({side * 0.03f, -0.42f, 0.0f}) * rotX(elXa);
        addXf(m.dark, elbow, Tel);
        addXf(m.dark, forearm, Tel * translationMatrix({0.0f, -0.20f, 0.0f}));
        addXf(m.dark, hand, Tel * translationMatrix({0.0f, -0.41f, 0.0f}));
        addXf(m.glow, armStrip, Tel * translationMatrix({side * 0.055f, -0.20f, 0.0f}));
    };
    buildArm(+1.0f, rig.shR, rig.shRx, rig.elR);
    buildArm(-1.0f, rig.shL, rig.shLx, rig.elL);

    // ---- neck + head assembly (rotated about the neck by the head look) -----------------------
    add(m.dark, at(shapes::makeCylinder(0.060f, 0.12f, 16, kDark), {0, 1.74f, 0.0f})); // neck
    const vec3 neck(0.0f, 1.76f, 0.0f);
    const mat4 Th = translationMatrix(neck) * rotY(rig.headYaw) * rotX(rig.headPitch) *
                    translationMatrix(-neck);
    addXf(m.body,
          at(applyTransform(shapes::makeSphere(0.175f, 22, 30, kSteel), scaleMatrix({0.95f, 1.06f, 1.0f})),
             {0, 1.90f, 0.0f}),
          Th);                                                                          // head
    addXf(m.body, at(boxDim(0.26f, 0.06f, 0.10f, kAccent), {0, 1.96f, 0.11f}), Th);     // brow
    addXf(m.body, at(boxDim(0.035f, 0.18f, 0.14f, kSteel), {0, 2.12f, -0.01f}), Th);    // crest
    addXf(m.glow, at(boxDim(0.23f, 0.055f, 0.05f, kGlow), {0, 1.885f, 0.168f}), Th);    // visor
    addXf(m.glow, at(boxDim(0.16f, 0.03f, 0.03f, kGlow), {0, 2.15f, -0.01f}), Th);      // crest tip

    // ---- glowing accents (static) ------------------------------------------------------------
    add(m.glow, at(shapes::makeSphere(0.058f, 16, 22, kGlow), {0, 1.54f, 0.175f}));     // chest core
    addPair(m.glow, boxDim(0.028f, 0.28f, 0.028f, kGlow), {hipX + 0.105f, 0.84f, 0.0f}); // thigh strips

    // ---- whole-body bob ----------------------------------------------------------------------
    if (rig.bodyY != 0.0f) {
        const mat4 Tb = translationMatrix({0.0f, rig.bodyY, 0.0f});
        m.body = applyTransform(m.body, Tb);
        m.dark = applyTransform(m.dark, Tb);
        m.glow = applyTransform(m.glow, Tb);
    }
    return m;
}

} // namespace character
