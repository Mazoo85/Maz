#pragma once

#include "maz/anim/IK.hpp"
#include "maz/math/Math.hpp"
#include "maz/render/MeshMerge.hpp"
#include "maz/render/MeshSkin.hpp"
#include "maz/render/MeshTransform.hpp"
#include "maz/render/Shapes.hpp"
#include "maz/render/Shapes3D.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

// maz::film ACTOR — a person, built to the proportions of a person.
//
// The film's figures were flat silhouettes: a stack of shapes with no depth, no volume, and sizes
// chosen by eye. This is the same character as a BODY — a skeleton of joints in the places a human
// skeleton has them, with a surface lofted over it — so that when the camera moves around it, it turns
// like a solid thing, and when it is lit from the side, one side of it is in shade.
//
// Every measurement here is a fraction of the character's HEIGHT, taken from the figure-drawing canon
// that artists have used since Vitruvius, because "looks about right" is exactly how the old figures
// went wrong:
//
//        crown 1.000   ·  an adult is 7.5 heads tall, so the head is 0.133 of the height
//        chin  0.867   ·  eyes sit halfway up the head, not near the top
//     shoulder 0.815   ·  shoulders are two head-widths across for a man, less for a woman
//        elbow 0.625   ·  the elbow is level with the navel
//    hip joint 0.530   ·  the FEMORAL HEAD, which is NOT the crotch and NOT the halfway line
//        wrist 0.485   ·  the wrist is level with the crotch, and the arms reach mid-thigh
//         knee 0.285   ·  the knee is NOT halfway down the leg; it is below halfway
//        ankle 0.039
//
// The hip joint is the one worth spelling out, because getting it wrong is invisible standing still
// and impossible to recover from in a walk. "Halfway up" is the CROTCH, at about 0.48; the joint the
// leg actually swings from is the head of the femur, 0.53 up, tucked inside the pelvis. Put the joint
// at the crotch and the leg comes out 7% short — which nobody can see in a photograph, and which means
// the leg cannot reach the floor at the end of a normal stride, so the foot skates or the shin
// stretches. Half a walk cycle's worth of trouble, from one landmark.
//
// The arms hang forward-kinematically from the shoulders (angles at the shoulder and elbow, which is
// how an arm is posed), and the legs run backward from the feet (the foot is placed on the ground and
// the knee is solved, which is how a leg is posed — a walk with feet that slide is not a walk). The
// leg solve reuses the engine's own anim::solveTwoBoneIK in the plane of each leg.
namespace maz::film {

// The character's own right is at LOCAL -X. That falls out of facing +Z in a right-handed world with
// +Y up: someone facing you has their right hand on your left. `kRight` / `kLeft` are theirs, not the
// camera's, everywhere in this file.
constexpr int kRight = 0;
constexpr int kLeft = 1;
inline float sideSign(int side) { return side == kLeft ? 1.0f : -1.0f; }
inline float clampUnit(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// ------------------------------------------------------------------------------- what a body is like
//
// Heights are fractions of the total height; widths and radii likewise, so one number — `height` —
// scales the whole person and every proportion holds.
struct Build {
    float height = 1.78f; // metres, sole to crown
    float heads = 7.5f;   // how many head-heights tall; children are fewer, and it shows

    // the landmarks, as fractions of height
    float yAnkle = 0.039f;
    float yKnee = 0.285f;
    float yHip = 0.530f;   // the femoral head, inside the pelvis
    float yPelvis = 0.545f;
    float yWaist = 0.620f;
    float yChest = 0.720f;
    float yShoulder = 0.815f;
    float yNeck = 0.845f;
    float yElbow = 0.625f;
    float yWrist = 0.485f;

    // widths and depths, as fractions of height
    float shoulderHalf = 0.105f; // shoulder joint, sideways from the midline
    float hipHalf = 0.052f;      // hip joint, sideways from the midline
    float pelvisHalfW = 0.086f, pelvisHalfD = 0.060f;
    float waistHalfW = 0.072f, waistHalfD = 0.050f;
    float chestHalfW = 0.094f, chestHalfD = 0.064f;
    float yokeHalfW = 0.112f, yokeHalfD = 0.058f; // across the top of the shoulders

    float upperArmR = 0.030f, elbowR = 0.026f, foreArmR = 0.025f, wristR = 0.018f;
    float thighR = 0.054f, kneeR = 0.044f, calfR = 0.041f, ankleR = 0.023f;
    float neckR = 0.031f;
    float handLen = 0.105f, footLen = 0.150f;

    render::Color skin{0.76f, 0.60f, 0.50f, 1.0f};
    render::Color hair{0.16f, 0.12f, 0.10f, 1.0f};
    render::Color top{0.26f, 0.30f, 0.38f, 1.0f};
    render::Color legwear{0.18f, 0.19f, 0.24f, 1.0f};
    render::Color shoes{0.10f, 0.10f, 0.11f, 1.0f};

    // ---- what they are wearing, and what their hair does ----------------------------------------
    //
    // Not decoration. At the distance a film watches people from, the SILHOUETTE is most of who
    // somebody is — you know which one is which from across a car park, in the dark, before you can
    // see a face. Two figures in the same painted T-shirt with the same helmet of hair are one figure
    // drawn twice, whatever colour they have been tinted, and that is what every character in this
    // renderer was until now.
    //
    // All of these are heights as a fraction of the body's own height, so they mean the same thing on
    // a tall man, a short woman and a child.
    float coatY = 0.0f;   // where a coat or jacket hem hangs to; 0 means no coat
    float skirtY = 0.0f;  // where a skirt hem hangs to; 0 means trousers
    float sleeve = 0.50f; // how far down the arm the sleeve runs: 0.5 short, 1.0 the elbow, 1.6 cuff
    float hairY = 0.0f;   // where the hair falls to down the back; 0 means it stops at the head
    float fringe = 0.4f;  // 0 swept straight back off the forehead, 1 a fringe down over it

    float headHalfH() const { return 0.5f / heads; }
    float headHalfW() const { return headHalfH() * 0.70f; }
    float headHalfD() const { return headHalfH() * 0.80f; }
    float m(float fraction) const { return fraction * height; } // a fraction of height, in metres
};

// A man of average build. The defaults above.
inline Build adultMale() { return Build{}; }

// A woman: shorter on average, narrower across the shoulders, wider at the hip, a smaller hand and
// foot. The waist-to-hip difference is the single measurement that makes a figure read as female at a
// distance, so it is the one not to fudge.
inline Build adultFemale() {
    Build b;
    b.height = 1.65f;
    b.shoulderHalf = 0.092f;
    b.yokeHalfW = 0.098f;
    b.chestHalfW = 0.085f;
    b.chestHalfD = 0.062f;
    b.waistHalfW = 0.064f;
    b.waistHalfD = 0.046f;
    b.pelvisHalfW = 0.092f;
    b.pelvisHalfD = 0.060f;
    b.hipHalf = 0.056f;
    b.upperArmR = 0.027f;
    b.elbowR = 0.023f;
    b.foreArmR = 0.022f;
    b.wristR = 0.016f;
    b.thighR = 0.053f;
    b.kneeR = 0.040f;
    b.calfR = 0.038f;
    b.ankleR = 0.020f;
    b.neckR = 0.027f;
    b.handLen = 0.098f;
    b.footLen = 0.140f;
    return b;
}

// A child of about eight. The head is the giveaway: a child is around six heads tall, not seven and a
// half, so scaling an adult down produces a small adult, which is uncanny, and never a child.
inline Build child() {
    Build b;
    b.height = 1.28f;
    b.heads = 6.0f;
    b.yShoulder = 0.800f;
    b.yNeck = 0.832f;
    b.yElbow = 0.608f;
    b.yWrist = 0.462f;
    b.yHip = 0.516f;
    b.yPelvis = 0.532f;
    b.yWaist = 0.608f;
    b.yChest = 0.706f;
    b.yKnee = 0.272f;
    b.yAnkle = 0.038f;
    b.shoulderHalf = 0.094f;
    b.yokeHalfW = 0.100f;
    b.chestHalfW = 0.086f;
    b.waistHalfW = 0.076f;
    b.pelvisHalfW = 0.082f;
    b.upperArmR = 0.031f;
    b.elbowR = 0.027f;
    b.foreArmR = 0.026f;
    b.wristR = 0.019f;
    b.thighR = 0.053f;
    b.kneeR = 0.043f;
    b.calfR = 0.040f;
    b.ankleR = 0.023f;
    b.neckR = 0.030f;
    return b;
}

// ------------------------------------------------------------------------------------------ the pose
//
// Everything except `position` and `facing` is in the character's OWN space: the floor at y = 0, +Z the
// way they are facing, +X their left.
// Named BodyPose rather than Pose because the flat renderer already has a maz::film::Pose: a set of 2D
// limb angles for a silhouette. This is a different thing entirely — a whole skeleton's worth of
// placement — and the two live side by side while both renderers do.
struct BodyPose {
    math::vec3 position{0.0f, 0.0f, 0.0f}; // where they stand in the world, in metres
    float facing = 0.0f;                   // yaw about Y; 0 faces +Z

    math::vec3 hips{0.0f, 0.0f, 0.0f}; // pelvis, in their own space; y = 0 means "use the rest height"
    float lean = 0.0f;                 // forward/back through the spine, radians
    float sway = 0.0f;                 // sideways through the spine
    float twist = 0.0f;                // around, through the spine, from the waist up
    // The pelvis turns on its own, because in a walk the hips and the shoulders turn OPPOSITE ways —
    // the hips follow the swinging leg and the ribcage counters it. One twist for the whole trunk
    // gives a figure that turns like a plank, which is the most obvious tell of a bad walk.
    float pelvisTwist = 0.0f;
    float neckPitch = 0.0f;
    float headYaw = 0.0f, headPitch = 0.0f;

    // An arm, at the shoulder and the elbow. `swing` is forward (+) or back; `spread` lifts it away
    // from the body; `elbow` bends it, and only ever one way, because an elbow does.
    struct Arm {
        float swing = 0.0f;
        float spread = 0.10f;
        float twist = 0.0f;
        float elbow = 0.12f;

        // Or: where the HAND is going, in the body's own space. When `handSet` the arm is solved to
        // reach it and the three angles above are ignored — the same trick the legs have always used,
        // for the same reason. A hand that is meant to arrive at a door handle, a table top, or the
        // other person's palm has to arrive there; posed by angles it arrives somewhere near, and
        // "near" is the difference between handing something over and miming it.
        bool handSet = false;
        math::vec3 hand{0.0f, 0.0f, 0.0f};
    };
    Arm arm[2];

    // The feet are PLACED, not angled — the ankle goes where the ground is and the knee is worked out
    // from there. This is the difference between walking and skating.
    math::vec3 ankle[2];
    float footPitch[2] = {0.0f, 0.0f};
    bool ankleSet = false; // false means "stand at rest", filled in by restPose
};

// Standing at rest: weight even, feet a hip's width apart, arms hanging with the small bend an arm
// actually has.
inline BodyPose restPose(const Build& b) {
    BodyPose p;
    p.hips = math::vec3(0.0f, b.m(b.yPelvis), 0.0f);
    for (int s = 0; s < 2; ++s) {
        p.ankle[s] = math::vec3(sideSign(s) * b.m(b.hipHalf), b.m(b.yAnkle), 0.0f);
    }
    p.ankleSet = true;
    return p;
}

// ------------------------------------------------------------------------------------------ the face
//
// A face is not part of the pose: it moves on its own clock — syllables, blinks, a reaction to
// something somebody else said — while the body is doing something slower. So it is its own small
// bundle of numbers, and every one of them is a thing an audience can name.
//
// What an audience actually reads, in order: the EYEBROWS first, by a long way, then the mouth, then
// the eyes. That is why there are two numbers for the brows and only one for the gaze.
struct Face {
    float mouthOpen = 0.0f; // 0 shut, 1 wide — on the syllable clock, so the mouth is on the voice
    float smile = 0.0f;     // -1 the corners pulled down, +1 up
    float browLift = 0.0f;  // -1 lowered and heavy, +1 raised
    float browTilt = 0.0f;  // +1 inner ends up, which is worry; -1 inner ends down, which is anger
    float squint = 0.0f;    // 0 open, 1 nearly shut
    float blink = 0.0f;     // 0 open, 1 shut — the fast one, over in a tenth of a second
    float gaze = 0.0f;      // -1 to +1 across: where the eyes point inside the head
};

// --------------------------------------------------------------------------------------- the skeleton

struct Skeleton {
    math::mat4 pelvis{1.0f}, waist{1.0f}, chest{1.0f}, yoke{1.0f}, neck{1.0f}, head{1.0f};
    math::mat4 shoulder[2]{math::mat4(1.0f), math::mat4(1.0f)};
    math::mat4 elbow[2]{math::mat4(1.0f), math::mat4(1.0f)};
    math::mat4 wrist[2]{math::mat4(1.0f), math::mat4(1.0f)};
    math::mat4 hip[2]{math::mat4(1.0f), math::mat4(1.0f)};
    math::mat4 knee[2]{math::mat4(1.0f), math::mat4(1.0f)};
    math::mat4 ankle[2]{math::mat4(1.0f), math::mat4(1.0f)};
    // False when the foot was asked for somewhere the leg could not reach, and so fell short of it.
    bool reached[2] = {true, true};

    static math::vec3 at(const math::mat4& m) { return math::vec3(m[3]); }
    // The same question asked of a point already carried through a frame.
    static math::vec3 at(const math::vec4& p) { return math::vec3(p); }
    // The tip of each chain, which is what a camera looks at and what a prop is held by.
    math::vec3 handAt(const Build& b, int side) const {
        return at(wrist[side] * math::vec4(0.0f, -b.m(b.handLen) * 0.55f, 0.0f, 1.0f));
    }
    // The end of the fingers, which is where the drawing canon measures an arm to and is not the same
    // point as the middle of the palm, where a thing is held.
    math::vec3 fingertipAt(const Build& b, int side) const {
        return at(wrist[side] * math::vec4(0.0f, -b.m(b.handLen), 0.0f, 1.0f));
    }
    math::vec3 crown(const Build& b) const {
        return at(head * math::vec4(0.0f, b.m(b.headHalfH()), 0.0f, 1.0f));
    }
    // Eye height, which is where a camera at "eye level" goes: halfway up the head, a little forward.
    math::vec3 eyes(const Build& b) const {
        return at(head * math::vec4(0.0f, b.m(b.headHalfH()) * 0.14f, b.m(b.headHalfD()) * 0.92f, 1.0f));
    }
};

namespace detail {

inline math::mat4 rotX(float a) { return glm::rotate(math::mat4(1.0f), a, math::vec3(1, 0, 0)); }
inline math::mat4 rotY(float a) { return glm::rotate(math::mat4(1.0f), a, math::vec3(0, 1, 0)); }
inline math::mat4 rotZ(float a) { return glm::rotate(math::mat4(1.0f), a, math::vec3(0, 0, 1)); }
inline math::mat4 move(float x, float y, float z) {
    return glm::translate(math::mat4(1.0f), math::vec3(x, y, z));
}

// A frame sitting at `from` whose local -Y runs down the bone to `to`, rolled so its +Z leans toward
// `hint`. This is how a joint gets an orientation out of two points: bones are lines, and a line is one
// axis short of a frame.
inline math::mat4 boneFrame(const math::vec3& from, const math::vec3& to, const math::vec3& hint) {
    math::vec3 down = to - from;
    const float len = std::sqrt(down.x * down.x + down.y * down.y + down.z * down.z);
    down = len > 1e-6f ? down / len : math::vec3(0.0f, -1.0f, 0.0f);
    const math::vec3 up = -down;
    math::vec3 side = math::cross(up, hint);
    float sl = std::sqrt(side.x * side.x + side.y * side.y + side.z * side.z);
    if (sl < 1e-5f) {
        // The hint lies along the bone, which says nothing about the roll. Any perpendicular will do.
        side = math::cross(up, math::vec3(1.0f, 0.0f, 0.0f));
        sl = std::sqrt(side.x * side.x + side.y * side.y + side.z * side.z);
        if (sl < 1e-5f) {
            side = math::vec3(0.0f, 0.0f, 1.0f);
            sl = 1.0f;
        }
    }
    side /= sl;
    const math::vec3 fwd = math::cross(side, up);
    math::mat4 m(1.0f);
    m[0] = math::vec4(side, 0.0f);
    m[1] = math::vec4(up, 0.0f);
    m[2] = math::vec4(fwd, 0.0f);
    m[3] = math::vec4(from, 1.0f);
    return m;
}

} // namespace detail

// Where every joint of this body is, for this pose.
inline Skeleton skeletonOf(const Build& b, const BodyPose& poseIn) {
    using namespace detail;
    BodyPose pose = poseIn;
    if (!pose.ankleSet) {
        const BodyPose r = restPose(b);
        pose.ankle[0] = r.ankle[0];
        pose.ankle[1] = r.ankle[1];
        pose.ankleSet = true;
    }
    if (pose.hips.y == 0.0f) {
        pose.hips.y = b.m(b.yPelvis);
    }

    const math::mat4 root = move(pose.position.x, pose.position.y, pose.position.z) * rotY(pose.facing);

    Skeleton sk;
    // The spine takes the lean in three helpings rather than one hinge at the waist, because a back
    // is a curve: bending it all at one joint is what makes a figure look like a hinged doll.
    sk.pelvis = root * move(pose.hips.x, pose.hips.y, pose.hips.z) * rotZ(pose.sway * 0.30f) *
                rotY(pose.pelvisTwist) * rotX(pose.lean * 0.30f);
    sk.waist = sk.pelvis * move(0.0f, b.m(b.yWaist - b.yPelvis), 0.0f) * rotZ(pose.sway * 0.35f) *
               rotY(pose.twist * 0.45f) * rotX(pose.lean * 0.35f);
    sk.chest = sk.waist * move(0.0f, b.m(b.yChest - b.yWaist), 0.0f) * rotZ(pose.sway * 0.35f) *
               rotY(pose.twist * 0.55f) * rotX(pose.lean * 0.35f);
    sk.yoke = sk.chest * move(0.0f, b.m(b.yShoulder - b.yChest), 0.0f);
    sk.neck = sk.yoke * move(0.0f, b.m(b.yNeck - b.yShoulder), 0.0f) * rotX(pose.neckPitch);
    // The head sits a neck's length above, and its centre is half a head-height above the chin. The
    // chin is one head-height below the crown, so where it falls depends on HOW MANY HEADS tall this
    // body is — hardcoding an adult's 0.867 gives a child a head that floats above their own crown.
    const float chinFraction = 1.0f - 1.0f / b.heads;
    const float chinToCentre = b.m(b.headHalfH());
    sk.head = sk.neck * move(0.0f, b.m(chinFraction - b.yNeck) + chinToCentre, 0.0f) *
              rotY(pose.headYaw) * rotX(pose.headPitch);

    const math::vec3 forward = math::vec3(root * math::vec4(0.0f, 0.0f, 1.0f, 0.0f));
    const math::vec3 rightward = math::vec3(root * math::vec4(1.0f, 0.0f, 0.0f, 0.0f));

    const float upperArm = b.m(b.yShoulder - b.yElbow);
    const float foreArm = b.m(b.yElbow - b.yWrist);
    for (int s = 0; s < 2; ++s) {
        const float sx = sideSign(s);
        const BodyPose::Arm& a = pose.arm[s];
        if (a.handSet) {
            // Solved, exactly as a leg is: the same two-bone solve, the same carrying of a flat answer
            // back out into three dimensions, and the same refusal to stretch a bone that will not
            // reach. What differs is only where the middle joint is pushed — a knee goes forward, an
            // elbow goes back and a little out, and getting that backwards gives a figure that reaches
            // for things with its elbow leading.
            const math::vec3 shoulderPos =
                Skeleton::at(sk.yoke * math::vec4(sx * b.m(b.shoulderHalf), 0.0f, 0.0f, 1.0f));
            const math::vec3 palm = Skeleton::at(root * math::vec4(a.hand, 1.0f));
            // The target is the PALM, because that is what holds things; the wrist has to stop a hand's
            // length short of it or everybody reaches past what they are reaching for.
            const math::vec3 elbowGoes =
                forward * -0.80f + rightward * (sx * 0.45f) + math::vec3(0.0f, -0.30f, 0.0f);
            const float palmOut = b.m(b.handLen) * 0.55f;

            // Solved twice, and the second pass is not a nicety. The wrist has to stop a hand's length
            // short of the target ALONG THE FOREARM, and which way the forearm points is not known
            // until the arm has been solved — so the first pass guesses the direction as
            // shoulder-to-target, and with a bent elbow that guess is wrong by most of a hand. Aiming
            // once put everybody's palm seven centimetres past what they were reaching for.
            math::vec3 lastBone = palm - shoulderPos;
            math::vec3 elbowPos = shoulderPos;
            math::vec3 wristPos = palm;
            for (int pass = 0; pass < 2; ++pass) {
                float boneLen = std::sqrt(math::dot(lastBone, lastBone));
                const math::vec3 along =
                    boneLen > 1e-6f ? lastBone / boneLen : math::vec3(0.0f, -1.0f, 0.0f);
                const math::vec3 wristWant = palm - along * palmOut;

                math::vec3 axis = wristWant - shoulderPos;
                float reach = std::sqrt(math::dot(axis, axis));
                axis = reach > 1e-6f ? axis / reach : math::vec3(0.0f, -1.0f, 0.0f);
                math::vec3 pole = elbowGoes - axis * math::dot(elbowGoes, axis);
                const float pl = std::sqrt(math::dot(pole, pole));
                pole = pl > 1e-5f ? pole / pl : rightward * sx;

                const anim::IKResult ik = anim::solveTwoBoneIK(math::vec2(0.0f, 0.0f), upperArm,
                                                               foreArm, math::vec2(reach, 0.0f), 1.0f);
                elbowPos = shoulderPos + axis * ik.mid.x + pole * ik.mid.y;
                wristPos = ik.reachable ? wristWant : shoulderPos + axis * ik.end.x + pole * ik.end.y;
                lastBone = wristPos - elbowPos;
                sk.shoulder[s] = boneFrame(shoulderPos, elbowPos, pole);
                sk.elbow[s] = boneFrame(elbowPos, wristPos, pole);
                // The hand carries on in the line of the forearm, as it does on the angled path too.
                sk.wrist[s] = boneFrame(wristPos, wristPos + lastBone, pole);
            }
            continue;
        }
        sk.shoulder[s] = sk.yoke * move(sx * b.m(b.shoulderHalf), 0.0f, 0.0f) * rotZ(sx * a.spread) *
                         rotX(a.swing) * rotY(sx * a.twist);
        // An elbow bends one way only: the forearm comes forward, never backward through the arm.
        const float bend = a.elbow < 0.0f ? 0.0f : (a.elbow > 2.7f ? 2.7f : a.elbow);
        // Negative about X, because a positive rotation there would carry the forearm BACKWARD, and an
        // elbow that bends backward is the single most alarming thing a figure can do.
        sk.elbow[s] = sk.shoulder[s] * move(0.0f, -upperArm, 0.0f) * rotX(-bend);
        sk.wrist[s] = sk.elbow[s] * move(0.0f, -foreArm, 0.0f);
    }

    // ---- the legs, solved from the feet up -------------------------------------------------------
    const float thigh = b.m(b.yHip - b.yKnee);
    const float shin = b.m(b.yKnee - b.yAnkle);
    for (int s = 0; s < 2; ++s) {
        const float sx = sideSign(s);
        const math::vec3 hipPos =
            Skeleton::at(sk.pelvis * math::vec4(sx * b.m(b.hipHalf), b.m(b.yHip - b.yPelvis), 0.0f, 1.0f));
        const math::vec3 anklePos = Skeleton::at(root * math::vec4(pose.ankle[s], 1.0f));

        // Solve in the plane that contains the hip, the ankle and the direction the body faces. In
        // that plane it is the engine's own two-bone solve, unchanged; all that is needed here is to
        // carry the answer back out into three dimensions.
        math::vec3 axis = anklePos - hipPos;
        float reach = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
        axis = reach > 1e-6f ? axis / reach : math::vec3(0.0f, -1.0f, 0.0f);
        math::vec3 pole = forward - axis * math::dot(forward, axis);
        float pl = std::sqrt(pole.x * pole.x + pole.y * pole.y + pole.z * pole.z);
        pole = pl > 1e-5f ? pole / pl : math::vec3(0.0f, 0.0f, 1.0f);

        const anim::IKResult ik = anim::solveTwoBoneIK(math::vec2(0.0f, 0.0f), thigh, shin,
                                                       math::vec2(reach, 0.0f), 1.0f);
        const math::vec3 kneePos = hipPos + axis * ik.mid.x + pole * ik.mid.y;
        // If the foot was asked for somewhere the leg cannot get to, the foot falls SHORT of it. The
        // alternative is to leave the ankle where it was asked for and let the shin grow to meet it,
        // which no one ever notices in a still and which makes a walking figure's legs pump like
        // telescopes. A bone is a fixed length; that is what makes it a bone.
        const math::vec3 footPos = ik.reachable ? anklePos : hipPos + axis * ik.end.x + pole * ik.end.y;
        sk.reached[s] = ik.reachable;

        sk.hip[s] = boneFrame(hipPos, kneePos, pole);
        sk.knee[s] = boneFrame(kneePos, footPos, pole);
        sk.ankle[s] = boneFrame(footPos, footPos - math::vec3(0.0f, 1.0f, 0.0f), forward) *
                      rotX(pose.footPitch[s]);
    }
    return sk;
}

// ------------------------------------------------------------------------------------------- the skin

namespace detail {

inline render::shapes::MeshData tint(render::shapes::MeshData m, const render::Color& c) {
    for (render::MeshVertex& v : m.vertices) {
        v.r = c.r;
        v.g = c.g;
        v.b = c.b;
    }
    return m;
}

inline void add(render::shapes::MeshData& into, const render::shapes::MeshData& part) {
    into = render::mergeMeshes(into, part);
}

// A ring of points around a frame's local Y axis, an ellipse rather than a circle because a body is
// wider than it is deep everywhere except the head.
inline std::vector<math::vec3> ring(const math::mat4& frame, float localY, float halfX, float halfZ,
                                    int points) {
    std::vector<math::vec3> out;
    out.reserve(static_cast<std::size_t>(points));
    // CLOCKWISE about the frame's +Y, looking down it. The direction matters and is not a detail:
    // skinSections bridges rib to rib in the order the points are given, so the direction the ring is
    // wound decides which way the surface faces. Wound the other way, every lofted piece of the body —
    // both arms, both legs, the whole torso — comes out INSIDE OUT: the renderer culls the outside and
    // draws the inside, and the normals point into the body. It is nearly invisible on a convex limb,
    // which is how it survived being looked at for a long time, and it makes shadows impossible: a
    // shadow map fed inside-out geometry records the near surface of everything and puts every lit
    // surface in its own shadow.
    for (int i = 0; i < points; ++i) {
        const float a = -6.28318530718f * static_cast<float>(i) / static_cast<float>(points);
        const math::vec4 p(std::cos(a) * halfX, localY, std::sin(a) * halfZ, 1.0f);
        out.push_back(math::vec3(frame * p));
    }
    return out;
}

// Move a whole rib along its own frame's forward axis, which is how a chest gets in front of a spine.
inline std::vector<math::vec3> shifted(std::vector<math::vec3> rib, const math::mat4& frame,
                                       float forward) {
    const math::vec3 dir = math::vec3(frame * math::vec4(0.0f, 0.0f, 1.0f, 0.0f));
    for (math::vec3& p : rib) {
        p += dir * forward;
    }
    return rib;
}

// A limb: one continuous tapered surface from joint to joint, ROUNDED at both ends.
//
// The first version of this was a tube with a sphere stuck on each end to close it, and it made a
// figure that looked like a wooden artist's mannequin — a bead at every elbow and knee. The spheres
// were not too big; the problem is that a tube and a sphere are two surfaces, so the shading breaks at
// the seam between them and the eye reads two objects. Lofting the cap as part of the same skin means
// one surface, one set of smoothed normals, and a knee that is a knee.
//
// The tips of the caps are left very slightly open, and every one of them is buried: a shoulder inside
// its deltoid, a wrist inside its hand, a knee tip inside the shin above it, an ankle inside the shoe.
inline render::shapes::MeshData limb(const math::vec3& from, const math::vec3& to, float rFrom,
                                     float rTo, const render::Color& c, int sides = 12) {
    math::vec3 dir = to - from;
    const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len < 1e-5f) {
        return render::shapes::MeshData{};
    }
    dir /= len;
    const math::mat4 frame = boneFrame(from, to, std::fabs(dir.y) > 0.9f ? math::vec3(0.0f, 0.0f, 1.0f)
                                                                        : math::vec3(0.0f, 1.0f, 0.0f));
    // Latitudes of the rounded cap, as the sine of the angle up from the equator.
    const float cap[4] = {0.06f, 0.38f, 0.72f, 1.0f};
    std::vector<std::vector<math::vec3>> ribs;
    ribs.reserve(12);
    for (int i = 0; i < 4; ++i) {
        const float sn = cap[i];
        const float cs = std::sqrt(1.0f - sn * sn);
        ribs.push_back(ring(frame, rFrom * cs, rFrom * sn, rFrom * sn, sides));
    }
    // The shaft, with a slight belly a third of the way down: a limb that tapers in a dead straight
    // line reads as a pipe, and the belly is where a muscle is.
    const float t[2] = {0.30f, 0.68f};
    const float belly[2] = {1.04f, 0.99f};
    for (int i = 0; i < 2; ++i) {
        const float r = (rFrom + (rTo - rFrom) * t[i]) * belly[i];
        ribs.push_back(ring(frame, -len * t[i], r, r, sides));
    }
    for (int i = 3; i >= 0; --i) {
        const float sn = cap[i];
        const float cs = std::sqrt(1.0f - sn * sn);
        ribs.push_back(ring(frame, -len - rTo * cs, rTo * sn, rTo * sn, sides));
    }
    return tint(render::skinSections(ribs, true, false), c);
}


// ------------------------------------------------------------------------------------------------
// THE HEAD ITSELF, as one sculpted surface.
//
// A head was a sphere for a while, with a brow ridge, two cheekbones and a jaw stuck onto it as
// separate lumps. It never worked, and it could not: two nearly-parallel surfaces meet along a curve
// that wanders by a whole facet at a time, so every one of those lumps showed its own rim and the face
// came out as four pale eggs glued to a fifth. The same failure put a sawtooth on the hairline.
//
// So the brow, the eye sockets, the cheekbones, the jaw and the chin are all shaped into ONE lofted
// skin here. There is no rim to show because there is no join. The cost is a table of numbers instead
// of a call to makeSphere, and the table is the more honest thing anyway: it is a head, written down.
//
// Every row is a cross-section, from under the chin up to the crown:
//
//   y       height, in head half-heights  (chin -0.90, crown +0.93)
//   rw, rd  how wide and how deep that section is, as fractions of the head's half-width and depth
//   zs      the whole section shifted forward — the lower face leads the skull
//   bulge   pushed forward across the FRONT only, and this is where a face gets its features:
//           positive at the brow and the cheekbones, negative at the eye sockets
//   ridge   pushed forward down the MIDDLE of the front only — the bridge of the nose. Drawn as its
//           own tapered tube instead it comes out as a pipe laid down the face, because it is one:
//           a nose is not an object on a head, it is the head's own surface coming forward.
//   jaw     how much this section swings down when the mouth opens: all of it at the chin, none of
//           it above the cheekbones, so the jaw hinges instead of the whole head sliding
struct HeadRow {
    float y, rw, rd, zs, bulge, ridge, jaw;
};

// How much of the ridge a point gets, by how near the middle of the face it is. `c` is the sideways
// position as a fraction of that section's half-width, so it is -1 at one ear and +1 at the other.
inline float ridgeAt(float c) { return std::exp(-(c / 0.165f) * (c / 0.165f)); }

inline const HeadRow* headRows(int& count) {
    static const HeadRow rows[] = {
        {-0.960f, 0.30f, 0.40f, 0.010f, 0.000f, 0.000f, 1.00f}, // under the jaw, where the neck goes
        {-0.880f, 0.44f, 0.58f, 0.025f, 0.055f, 0.000f, 1.00f}, // the chin, which is a point of it
        {-0.740f, 0.60f, 0.73f, 0.030f, 0.030f, 0.000f, 0.95f},
        {-0.600f, 0.75f, 0.84f, 0.025f, 0.015f, 0.000f, 0.80f}, // the mouth sits here
        {-0.460f, 0.87f, 0.92f, 0.020f, 0.008f, 0.020f, 0.55f}, // the angle of the jaw
        {-0.300f, 0.95f, 0.97f, 0.015f, 0.012f, 0.175f, 0.28f}, // and the end of the nose
        {-0.120f, 1.00f, 1.00f, 0.010f, 0.026f, 0.150f, 0.06f}, // the cheekbones: the widest of it
        {+0.070f, 0.99f, 0.99f, 0.000f, -0.026f, 0.090f, 0.00f}, // the eye sockets, set back
        {+0.220f, 0.97f, 0.98f, 0.000f, 0.032f, 0.020f, 0.00f},  // the brow ridge
        {+0.380f, 0.93f, 0.95f, 0.000f, -0.010f, 0.000f, 0.00f}, // forehead falling away above it
        {+0.560f, 0.85f, 0.88f, -0.005f, 0.000f, 0.000f, 0.00f},
        {+0.720f, 0.68f, 0.72f, -0.010f, 0.000f, 0.000f, 0.00f},
        {+0.850f, 0.44f, 0.48f, -0.010f, 0.000f, 0.000f, 0.00f},
        {+0.930f, 0.10f, 0.11f, -0.010f, 0.000f, 0.000f, 0.00f}, // the crown
    };
    count = static_cast<int>(sizeof(rows) / sizeof(rows[0]));
    return rows;
}

// The cross-section at any height, on a curve through the rows rather than a straight line between
// them. Straight lines put a crease across the face at every row — the surface changes slope all at
// once, the smoothed normals change with it, and a head gets a seam across the forehead and another
// along the jaw. A Catmull-Rom through the four nearest rows costs a dozen lines and there is no seam
// because there is no corner.
inline float throughRows(float p0, float p1, float p2, float p3, float t) {
    const float t2 = t * t, t3 = t2 * t;
    return 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                   (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

inline HeadRow headAt(float y) {
    int n = 0;
    const HeadRow* rows = headRows(n);
    if (y <= rows[0].y) return rows[0];
    if (y >= rows[n - 1].y) return rows[n - 1];
    int i = 0;
    while (i < n - 2 && y > rows[i + 1].y) ++i;
    auto at = [&](int k) -> const HeadRow& { return rows[k < 0 ? 0 : (k >= n ? n - 1 : k)]; };
    const HeadRow& p0 = at(i - 1);
    const HeadRow& p1 = at(i);
    const HeadRow& p2 = at(i + 1);
    const HeadRow& p3 = at(i + 2);
    const float t = (y - p1.y) / (p2.y - p1.y);
    HeadRow r;
    r.y = y;
    r.rw = throughRows(p0.rw, p1.rw, p2.rw, p3.rw, t);
    r.rd = throughRows(p0.rd, p1.rd, p2.rd, p3.rd, t);
    r.zs = throughRows(p0.zs, p1.zs, p2.zs, p3.zs, t);
    r.bulge = throughRows(p0.bulge, p1.bulge, p2.bulge, p3.bulge, t);
    r.ridge = throughRows(p0.ridge, p1.ridge, p2.ridge, p3.ridge, t);
    r.jaw = throughRows(p0.jaw, p1.jaw, p2.jaw, p3.jaw, t);
    // A curve through points can overshoot between them; a head with a negative radius is a head
    // turned inside out, and a nose that goes negative under its own tip is a dent.
    if (r.rw < 0.0f) r.rw = 0.0f;
    if (r.rd < 0.0f) r.rd = 0.0f;
    if (r.ridge < 0.0f) r.ridge = 0.0f;
    return r;
}

// How far forward the face is at a point on it. Every feature below — a brow, an eye, a nostril, a
// lip — is laid onto the answer, and it has to know the SIDEWAYS position as well as the height. The
// first version did not, and it floated every feature off the face: a head is a ball, so at the eyes
// the surface is already a tenth of a head further back than it is at the nose and at the cheekbones a
// fifth of one. Features laid on the depth measured down the middle of the face stood out like golf
// balls and flying saucers. `jawDrop` is deliberately ignored — a feature that rides the jaw is moved
// by the jaw separately.
inline float headFrontZ(float hw, float hh, float hd, float x, float y) {
    const HeadRow r = headAt(y / hh);
    const float wide = r.rw * hw;
    if (wide <= 1e-6f) return r.zs * hd;
    const float c = x / wide;
    const float s = c * c >= 1.0f ? 0.0f : std::sqrt(1.0f - c * c);
    return r.zs * hd + r.rd * hd * s + (r.bulge + r.ridge * ridgeAt(c)) * hd * s * s;
}

// The skin over all of it.
inline render::shapes::MeshData skullShell(float hw, float hh, float hd, float jawDrop,
                                           const render::Color& c) {
    int n = 0;
    const HeadRow* rows = headRows(n);
    // Points around each section, and they are NOT spread evenly. A head is all face: everything worth
    // looking at lives in the sixty degrees or so at the front, and the back is a smooth dome that
    // three points could describe. Spread evenly, thirty-odd points put only two or three across the
    // whole bridge of the nose — which is why the nose, correctly shaped in the table, came out as a
    // faint bump with a knob on the end. Bunched toward the face they cost nothing extra and the nose
    // is a nose.
    const int points = 40;
    const float bunch = 0.65f;
    std::vector<std::vector<math::vec3>> ribs;
    ribs.reserve(static_cast<std::size_t>(n) * 2);
    for (int i = 0; i < n; ++i) {
        // One rib on each row and two more between it and the next. The rows carry the shape; the rest
        // keep the silhouette from going faceted, which on a head is most visible exactly where it is
        // least wanted — around the chin and along the top of the skull.
        const int steps = (i + 1 < n) ? 3 : 1;
        for (int h = 0; h < steps; ++h) {
            const float y = rows[i].y + (rows[i + 1 < n ? i + 1 : i].y - rows[i].y) *
                                            (static_cast<float>(h) / static_cast<float>(steps));
            const HeadRow r = headAt(y);
            std::vector<math::vec3> rib;
            rib.reserve(static_cast<std::size_t>(points));
            for (int k = 0; k < points; ++k) {
                // Clockwise about +Y, exactly as ring() is wound, and for the same reason: the other
                // way round the head is inside out and shadows itself.
                const float u = static_cast<float>(k) / static_cast<float>(points);
                // Counter-clockwise, for the reason given in hairCap: these ribs run up the head, and
                // ring()'s direction is the one for ribs that run down a limb. Wound the other way the
                // whole head is inside out — the front of the face is culled and what you see instead
                // is the inside of the back of the skull, which is smooth, has no chin, and is exactly
                // as convincing as that sounds. It cost an afternoon to notice, because an inside-out
                // head still looks like a head.
                //
                // The face is at u = 0.25; the warp slows the walk around the section there and
                // hurries it round the back, and is monotone for any bunch below 1.
                const float a = 6.28318530718f *
                                (u - bunch * std::sin(6.28318530718f * (u - 0.25f)) / 6.28318530718f);
                const float s = std::sin(a); // +1 at the face, -1 at the back of the head
                const float front = s > 0.0f ? s * s : 0.0f;
                const float across = std::cos(a);
                rib.push_back(math::vec3(across * r.rw * hw, y * hh - jawDrop * r.jaw,
                                         s * r.rd * hd + r.zs * hd +
                                             front * (r.bulge + r.ridge * ridgeAt(across)) * hd));
            }
            ribs.push_back(rib);
        }
    }
    // A crown to close the top, the same trick the hair cap uses.
    ribs.push_back(std::vector<math::vec3>(static_cast<std::size_t>(points),
                                           math::vec3(0.0f, hh * 0.945f, -hd * 0.01f)));
    return tint(render::skinSections(ribs, true, false), c);
}

// Something that HANGS: a coat, a jacket, a skirt. Lofted from the body down to a hem, flaring as it
// goes, because a hem is always wider than the waist above it and a garment that does not flare is a
// tube of paint.
//
// It hangs off the pelvis rather than off the legs, which is the whole difference between a coat and
// a pair of painted-on trousers: the legs move inside it, and the hem swings with the hips.
inline render::shapes::MeshData hanging(const math::mat4& frame, float topY, float hemY, float topW,
                                        float topD, float hemW, float hemD, const render::Color& c) {
    if (hemY >= topY) {
        return render::shapes::MeshData{};
    }
    const int points = 20;
    const int steps = 5;
    std::vector<std::vector<math::vec3>> ribs;
    ribs.reserve(static_cast<std::size_t>(steps) + 2);
    float wideHere = topW;
    float deepHere = topD;
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        // Wound counter-clockwise about +Y, because these ribs run DOWNWARD — the opposite of the
        // head's, and the same as limb()'s. Wound the other way the coat is inside out.
        const float y = topY + (hemY - topY) * t;
        // The flare is not linear: a coat hangs straight off the shoulders and opens out near the
        // bottom, so the width follows the square of how far down it is.
        const float f = t * t;
        wideHere = topW + (hemW - topW) * f;
        deepHere = topD + (hemD - topD) * f;
        std::vector<math::vec3> rib;
        rib.reserve(static_cast<std::size_t>(points));
        for (int k = 0; k < points; ++k) {
            const float a = -6.28318530718f * static_cast<float>(k) / static_cast<float>(points);
            rib.push_back(math::vec3(frame * math::vec4(std::cos(a) * wideHere, y,
                                                        std::sin(a) * deepHere, 1.0f)));
        }
        ribs.push_back(rib);
    }
    // A hem that is a rim you can see the inside of, because that is what a hem is. It is turned in
    // from whatever the last rib actually came out at, not from what the hem was ASKED for: taken
    // from the nominal width it goes on flaring after the cloth has stopped, and the lip ends up
    // wider than the garment it belongs to.
    std::vector<math::vec3> lip;
    lip.reserve(static_cast<std::size_t>(points));
    const float edgeW = wideHere * 0.90f;
    const float edgeD = deepHere * 0.90f;
    for (int k = 0; k < points; ++k) {
        const float a = -6.28318530718f * static_cast<float>(k) / static_cast<float>(points);
        lip.push_back(math::vec3(
            frame * math::vec4(std::cos(a) * edgeW, hemY + 0.012f, std::sin(a) * edgeD, 1.0f)));
    }
    ribs.push_back(lip);
    return tint(render::skinSections(ribs, true, false), c);
}

// A head of hair with an authored HAIRLINE, lofted rather than intersected.
//
// The obvious way to put hair on a head is a second, slightly bigger sphere, and it does not work. Two
// nearly-parallel surfaces meet along a curve that wanders by a whole facet at a time, so the hairline
// comes out as a sawtooth — a badly cut stencil sitting on the forehead, and the first thing the eye
// goes to on the whole figure.
//
// Lofting the cap from a rim I choose puts the edge exactly where I put it. The rim is then free to be
// where a hairline actually is, which is not a circle: high across the front, falling away fast at the
// temples, and low round the back to the nape. `capH` is where the CROWN goes — the top of the hair,
// not the top of the skull, because the canon measures a head from the chin to the top of the hair and
// a figure whose hair is drawn above that mark is taller than it says it is. The cap sits `proud`
// outside the skull's own ellipsoid, so it clears it everywhere by the same margin and never fights
// with it.
// `fringe` pulls the front of the rim down over the forehead; `fallTo` is how far below the head's
// centre the hair carries on down the back, in head half-heights, and is what makes the difference
// between a crop and somebody with hair on their shoulders.
inline render::shapes::MeshData hairCap(float hw, float hh, float hd, float capH, float fringe,
                                        float fallTo, const render::Color& c) {
    const int points = 48;
    const int steps = 8;
    const float proud = 1.105f;
    std::vector<std::vector<math::vec3>> ribs;
    ribs.reserve(static_cast<std::size_t>(steps) + 8);

    // The FALL: hair that carries on below the hairline, down the back and over the shoulders. It is
    // built as its OWN closed tube rather than as more ribs on the cap, and that is the whole trick.
    // Bridged onto the cap, the surface has to get from the rim — which is high at the forehead and
    // low at the nape — down to a ring that is level, and the only way across the front is a sheet
    // straight down the face. It looks exactly like somebody wearing their hair over their eyes.
    //
    // Separate, the fall starts already tucked in behind the cheekbones and never goes near the face.
    // The two overlap around the sides and the back, where they are both hair and nobody can tell.
    render::shapes::MeshData whole;
    if (fallTo < -1.0f) {
        const int drop = 6;
        std::vector<std::vector<math::vec3>> fallRibs;
        fallRibs.reserve(static_cast<std::size_t>(drop) + 1);
        for (int i = 0; i <= drop; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(drop);
            const float y = hh * (0.16f + (fallTo - 0.16f) * t);
            std::vector<math::vec3> rib;
            rib.reserve(static_cast<std::size_t>(points));
            for (int k = 0; k < points; ++k) {
                // Downward ribs, so wound the other way from the cap's — the direction that decides
                // which side is the outside depends on which way the stack runs.
                const float a = -6.28318530718f * static_cast<float>(k) / static_cast<float>(points);
                const float toFront = std::sin(a);
                const float front = toFront > 0.0f ? toFront * toFront : 0.0f;
                // Tucked in hard at the front from the very top of the fall, and further as it goes
                // down: hair falls BEHIND a face, and behind the jaw it is nearly at the neck.
                const float tuck = 1.0f - front * (0.52f + 0.34f * t);
                const float flare = 1.0f + 0.10f * t * (1.0f - front);
                rib.push_back(math::vec3(std::cos(a) * hw * proud * tuck * flare, y,
                                         std::sin(a) * hd * proud * tuck * flare -
                                             hd * (0.30f + 0.22f * t) * front));
            }
            fallRibs.push_back(rib);
        }
        whole = tint(render::skinSections(fallRibs, true, false), c);
    }

    for (int s = 0; s <= steps; ++s) {
        const float u = static_cast<float>(s) / static_cast<float>(steps);
        std::vector<math::vec3> rib;
        rib.reserve(static_cast<std::size_t>(points));
        for (int i = 0; i < points; ++i) {
            // Wound COUNTER-clockwise about +Y. ring() winds the other way, and the difference is not
            // a taste: skinSections decides which side of the surface is the outside from the order of
            // the points AND the order of the ribs, and ring()'s direction is the right one for a stack
            // that runs DOWNWARD, which is how limb() builds an arm. These ribs run upward, so the same
            // winding gives a shell that is inside out — the renderer then culls the surface facing you
            // and draws the far one instead.
            const float a = 6.28318530718f * static_cast<float>(i) / static_cast<float>(points);
            const float toFront = std::sin(a); // +1 at the face, -1 at the back of the head
            // The rim. The exponent is what makes it a hairline rather than a headband: at 1.0 the rim
            // falls away from the front in a slow cosine and the result is a swimming cap.
            const float fall =
                (toFront < 0.0f ? -1.0f : 1.0f) * std::pow(std::fabs(toFront), 1.5f);
            // A fringe pulls the front of the rim down the forehead; swept back, it sits higher.
            const float front = toFront > 0.0f ? toFront * toFront : 0.0f;
            const float rimY = hh * (-0.02f + 0.46f * fall) - hh * 0.30f * fringe * front;
            const float y = rimY + (capH - rimY) * std::sin(u * 1.5707963f);
            const float t = y / capH;
            const float k = t * t >= 1.0f ? 0.0f : std::sqrt(1.0f - t * t);
            rib.push_back(math::vec3(std::cos(a) * hw * proud * k, y, std::sin(a) * hd * proud * k));
        }
        ribs.push_back(rib);
    }
    // One last rib collapsed onto the crown, so the cap is closed rather than a tube with a hole in the
    // top of it. The triangles that come out of it have no area, contribute nothing to the smoothed
    // normals, and cost nothing to draw.
    ribs.push_back(
        std::vector<math::vec3>(static_cast<std::size_t>(points), math::vec3(0.0f, capH, 0.0f)));
    add(whole, tint(render::skinSections(ribs, true, false), c));
    return whole;
}

} // namespace detail

// The whole surface, as one mesh with the colours baked into the vertices.
inline render::shapes::MeshData buildBody(const Build& b, const Skeleton& sk,
                                         const Face& face = Face()) {
    using namespace detail;
    render::shapes::MeshData m;
    const int P = 16; // points around the torso

    // ---- torso: one surface lofted from the hips to the shoulders, following the spine ------------
    {
        std::vector<std::vector<math::vec3>> ribs;
        ribs.push_back(ring(sk.pelvis, -b.m(0.055f), b.pelvisHalfW * b.height * 0.86f,
                            b.pelvisHalfD * b.height * 0.86f, P));
        ribs.push_back(ring(sk.pelvis, 0.0f, b.m(b.pelvisHalfW), b.m(b.pelvisHalfD), P));
        ribs.push_back(ring(sk.waist, 0.0f, b.m(b.waistHalfW), b.m(b.waistHalfD), P));
        // The chest sits FORWARD of the spine and the shoulders sit back over it. A torso lofted as a
        // stack of centred ellipses is perfectly symmetric front to back, so a figure photographed
        // from behind is the same picture as one photographed from the front, and in a two-shot
        // nobody can tell which way anyone is facing.
        ribs.push_back(shifted(ring(sk.chest, 0.0f, b.m(b.chestHalfW), b.m(b.chestHalfD), P), sk.chest,
                               b.m(0.013f)));
        ribs.push_back(shifted(ring(sk.yoke, -b.m(0.030f), b.m(b.yokeHalfW), b.m(b.yokeHalfD), P),
                               sk.yoke, -b.m(0.006f)));
        ribs.push_back(ring(sk.yoke, 0.0f, b.m(b.yokeHalfW * 0.94f), b.m(b.yokeHalfD * 0.92f), P));
        // The trapezius. Going from the width of the shoulders to the width of the neck in one step
        // gives a figure a cone for a top and makes the neck look twice its length; the slope has to
        // take a run at it.
        ribs.push_back(ring(sk.yoke, b.m(0.012f), b.m(b.yokeHalfW * 0.74f), b.m(b.yokeHalfD * 0.80f), P));
        ribs.push_back(ring(sk.yoke, b.m(0.018f), b.m(b.yokeHalfW * 0.40f), b.m(b.yokeHalfD * 0.50f), P));
        add(m, tint(render::skinSections(ribs, true, false), b.top));
        // Cap the two open ends so the body is not a pipe you can see down.
        add(m, render::applyTransform(
                   render::applyTransform(render::shapes::makeSphere(b.m(b.pelvisHalfW) * 0.98f, 10, 16,
                                                                     b.legwear),
                                          render::scaleMatrix(math::vec3(1.0f, 0.55f, 0.70f))),
                   sk.pelvis * detail::move(0.0f, -b.m(0.050f), 0.0f)));
        // A FLAT plug over the neck hole, not a ball in it. A sphere wide enough to close an opening
        // that size stands its own radius above it — nine centimetres, which is past the chin — so the
        // figure came out with its head resting on a blue football and no neck at all. It was there
        // from the start and only showed once the head stopped being wide enough to hide it.
        add(m, render::applyTransform(
                   render::applyTransform(
                       render::shapes::makeSphere(b.m(b.yokeHalfW * 0.40f), 8, 16, b.top),
                       render::scaleMatrix(math::vec3(1.0f, 0.30f, 0.92f))),
                   sk.yoke * detail::move(0.0f, b.m(0.019f), 0.0f)));
    }

    // ---- neck and head ---------------------------------------------------------------------------
    //
    // Everything in here is in HEAD-LOCAL units — a fraction of the head's own half-width, half-height
    // and half-depth — so a child's face is a child's face rather than an adult's shrunk.
    //
    // The heights are not invented. A head divides into four almost equal parts, and a face that gets
    // them wrong is wrong in a way anybody can see without being able to say why. Measuring from the
    // crown of the hair (+1.00) down to the chin (-0.90), in head half-heights:
    //
    //     hairline   +0.52      a quarter of the way down
    //     brow       +0.22      the eyebrows sit on it
    //     eyes       +0.07      halfway down the head, just under the brow
    //     nose base  -0.40      halfway again, from the eyes to the chin
    //     mouth      -0.58      a third of the way from the nose base to the chin
    //     chin       -0.90
    //
    // The first draft of this face had the nose at -0.09 and the mouth at -0.40 — both of them a third
    // of a head too high — and the result was a button nose jammed under the eyes with a mouth right
    // behind it and a vast empty chin below. Nothing else about the face mattered while that was true.
    {
        const float hw = b.m(b.headHalfW());
        const float hh = b.m(b.headHalfH());
        const float hd = b.m(b.headHalfD());
        const float lidShut = clampUnit(face.blink + face.squint * 0.45f);
        const float openMouth = clampUnit(face.mouthOpen);
        // How far the jaw swings down when the mouth opens. The lower lip rides with it.
        const float jawDrop = hh * 0.085f * openMouth;

        const math::vec3 neckBase = Skeleton::at(sk.neck);
        // Far enough up inside the head that there is no join to see, and no further. limb() finishes
        // with a ROUNDED CAP that stands a whole radius past the point it is given — five centimetres
        // on a neck — so a neck aimed at the middle of the head arrives as a dome behind the mouth and
        // the figure grows a muzzle. Aimed under the jaw, the same cap is buried in the chin.
        // A neck TAPERS into the head, and it has to: limb() finishes with a rounded cap standing a
        // whole radius past the point it is given, so a full-width neck aimed into the skull arrives
        // as a five-centimetre dome and the figure gets a pear hanging under its chin. Narrowed at the
        // top, the same cap is buried in the jaw and what shows below is a neck.
        const math::vec3 headBase = Skeleton::at(sk.head * math::vec4(0.0f, -hh * 0.85f, 0.0f, 1.0f));
        add(m, limb(neckBase, headBase, b.m(b.neckR * 1.10f), b.m(b.neckR * 0.84f), b.skin, 20));

        // The head: one sculpted surface, brow, cheekbones, jaw and chin shaped into it. See
        // skullShell for why it is not a sphere with lumps on.
        add(m, render::applyTransform(skullShell(hw, hh, hd, jawDrop, b.skin), sk.head));
        auto faceZ = [&](float x, float y) { return headFrontZ(hw, hh, hd, x, y); };

        add(m, render::applyTransform(
                   hairCap(hw, hh, hd, hh, b.fringe,
                           b.hairY > 0.0f ? (b.hairY * b.height - Skeleton::at(sk.head).y) / hh : 0.0f,
                           b.hair),
                   sk.head));

        // There is no brow ridge or cheekbone drawn here, and there used to be. They are shaped
        // into the head itself now — see the table in skullShell — because as separate lumps they
        // read as four pale eggs glued to the face, whatever size they were made.

        // EYEBROWS. The first thing an audience reads on a face and the cheapest to draw: two dark
        // bars that lift, lower and tilt. Everything else here could be right and a face with no
        // eyebrows would still be unreadable.
        for (int s2 = 0; s2 < 2; ++s2) {
            const float side = sideSign(s2);
            // Tilt is about the INNER end, so a positive tilt lifts the inside of both brows — the
            // shape of worry — rather than rotating both the same way, which is a raised eyebrow on
            // one side and a lowered one on the other.
            const float lean = -side * face.browTilt * 0.34f;
            const float browX = side * hw * 0.375f;
            const float browY = hh * (0.225f + face.browLift * 0.085f);
            // Four short pieces rather than one long bar, each laid on the face where IT is. A brow is
            // a third of the width of a head, and across that much of a head the surface falls back by
            // a quarter of its own depth — so a single straight bar has its inner end on the face and
            // its outer end hanging in the air beside it, which is exactly how it looked.
            for (int k = 0; k < 5; ++k) {
                const float along = (static_cast<float>(k) - 2.0f) * hw * 0.060f;
                const float bx = browX + along * std::cos(lean);
                const float by = browY + along * std::sin(lean);
                add(m, render::applyTransform(
                           render::applyTransform(render::shapes::makeBox(1.0f, b.hair),
                                                  render::scaleMatrix(math::vec3(
                                                      hw * 0.130f, hh * 0.070f, hd * 0.090f))),
                           sk.head * detail::move(bx, by, faceZ(bx, by) - hd * 0.032f) *
                               detail::rotZ(lean)));
            }
        }

        // EYES, on the halfway line, one eye-width apart, because that is where eyes are.
        //
        // A blink is the EYE closing, not a lid drawn on top of it. The first attempt modelled an
        // upper lid as its own sphere coming down over the eyeball, which is how a lid works and is
        // not how one can be drawn at this size: a sphere big enough to cover the eye is bigger than
        // the eye, so it stands proud of the face, and the result was two pale eggs parked on the
        // forehead. Squashing the eye itself is the trick every animator uses, costs nothing, and can
        // never poke out of a face because it only ever gets smaller.
        // Not white. A sclera lit by the same key as the skin beside it is a warm grey, and painted
        // any brighter it reads as enamel — two boiled eggs in a face.
        const render::Color eyeWhite{0.78f, 0.77f, 0.74f, 1.0f};
        const render::Color iris{0.11f, 0.10f, 0.10f, 1.0f};
        const float eyeR = hw * 0.192f;
        const float eyeY = hh * 0.07f;
        const float openness = 1.0f - 0.94f * lidShut;
        // An eye is WIDER THAN IT IS TALL and it is barely proud of the face. A ball is neither, and a
        // ball is what was here: two white spheres standing out of the front of a head, which is the
        // single most doll-like thing a face can do. What is wanted is a shallow lens — wide, low, and
        // sunk until only the front of it breaks the surface.
        const float tall = 0.62f, deep = 0.30f;
        for (int s2 = 0; s2 < 2; ++s2) {
            const float sx = sideSign(s2) * hw * 0.375f;
            // A closing eye also settles: the slit that is left sits where the bottom lid is, not
            // floating at the middle of the socket.
            const math::mat4 socket =
                sk.head * detail::move(sx, eyeY - hh * 0.030f * lidShut,
                                       faceZ(sx, eyeY) - eyeR * deep * 0.30f);
            add(m, render::applyTransform(
                       render::applyTransform(render::shapes::makeSphere(eyeR, 11, 18, eyeWhite),
                                              render::scaleMatrix(
                                                  math::vec3(1.0f, tall * openness, deep))),
                       socket));
            // The iris goes where the eyes are LOOKING. A head turned toward somebody with its eyes
            // still pointing dead ahead is the difference between attention and a doll.
            // The iris has to be brought forward until its rim meets the white's surface, or all that
            // shows of it is the very tip poking through and the eye reads as blank.
            add(m, render::applyTransform(
                       render::applyTransform(render::shapes::makeSphere(hw * 0.088f, 9, 14, iris),
                                              render::scaleMatrix(math::vec3(1.0f, openness, 0.22f))),
                       socket * detail::move(face.gaze * eyeR * 0.32f, 0.0f, eyeR * 0.30f)));
        }

        // A NOSE, which at any distance is what says which way a head is turned — and close up is
        // most of what stops a face being a mask. It is three pieces and needs to be: a bridge coming
        // out of the brow, a ball at the end of it on the nose line, and the wings either side.
        {
            const float baseY = -hh * 0.40f;                 // the nose line, halfway eyes-to-chin
            const render::Color nostril{b.skin.r * 0.34f, b.skin.g * 0.27f, b.skin.b * 0.25f, 1.0f};
            // There is no bridge drawn here. It is shaped into the head — the `ridge` column of the
            // table in skullShell — because every attempt to draw it as its own piece produced a pipe
            // laid down the middle of a face, whether the piece was an ellipsoid (a dart, pointed at
            // both ends) or a tapered loft (a tube). Only the end of the nose is its own mass.
            // The ball on the end, which is the furthest-forward point of the whole head.
            // The ball sits ON the nose line, not across it: its underside is the base of the nose,
            // and everything below that is the philtrum and then the mouth. Centred on the line
            // instead, the nose hangs over the top lip and the face has no upper lip at all.
            const float ballY = baseY + hh * 0.105f;
            const float ballZ = faceZ(0.0f, ballY) - hd * 0.070f;
            add(m, render::applyTransform(
                       render::applyTransform(render::shapes::makeSphere(1.0f, 14, 18, b.skin),
                                              render::scaleMatrix(math::vec3(hw * 0.135f, hh * 0.085f,
                                                                             hd * 0.115f))),
                       sk.head * detail::move(0.0f, ballY, ballZ)));
            // The wings either side of it, kept small and mostly buried. Made any bigger they stop
            // being part of the nose and become two more balls in a cluster of them.
            for (int s2 = 0; s2 < 2; ++s2) {
                const float side = sideSign(s2);
                const float wingX = side * hw * 0.135f;
                add(m, render::applyTransform(
                           render::applyTransform(render::shapes::makeSphere(1.0f, 12, 14, b.skin),
                                                  render::scaleMatrix(math::vec3(
                                                      hw * 0.068f, hh * 0.046f, hd * 0.062f))),
                           sk.head * detail::move(wingX, baseY + hh * 0.060f,
                                                  faceZ(wingX, baseY) - hd * 0.012f)));
                // Two dark marks tucked UNDER the end of it, where they are shaded from the key and
                // read as holes. Sitting them on the front of the nose instead just hangs two dark
                // beads off it.
                add(m, render::applyTransform(
                           render::applyTransform(render::shapes::makeSphere(1.0f, 8, 10, nostril),
                                                  render::scaleMatrix(math::vec3(
                                                      hw * 0.038f, hh * 0.016f, hd * 0.038f))),
                           sk.head * detail::move(side * hw * 0.062f, baseY + hh * 0.012f,
                                                  ballZ + hd * 0.048f)));
            }
        }

        // THE MOUTH. Dark inside, lips around it, and the corners carrying the expression.
        {
            const float mouthY = -hh * 0.58f;
            // The mouth is on the JAW, not on the skull, and the jaw stands well in front of it —
            // laid onto the skull instead, a mouth is a line somewhere inside the chin.
            const float mouthZ = faceZ(0.0f, mouthY);
            const render::Color inside{b.skin.r * 0.15f, b.skin.g * 0.11f, b.skin.b * 0.11f, 1.0f};
            // Only a little darker and a little redder than the face. Lips that read as lipstick are
            // a different film; what is wanted is the line between them, and the line comes from the
            // dark slot behind, not from the colour in front.
            const render::Color lip{b.skin.r * 0.88f, b.skin.g * 0.68f, b.skin.b * 0.63f, 1.0f};

            // The dark is set BACK and the lips stand in front of it. The other way round — which is
            // what it was — the dark mass covers the lips and a mouth is a painted line.
            add(m, render::applyTransform(
                       render::applyTransform(render::shapes::makeSphere(1.0f, 10, 14, inside),
                                              render::scaleMatrix(math::vec3(
                                                  hw * (0.300f + 0.03f * openMouth),
                                                  hh * (0.075f + 0.20f * openMouth), hd * 0.12f))),
                       sk.head * detail::move(0.0f, mouthY - hh * 0.10f * openMouth - jawDrop * 0.5f,
                                              mouthZ - hd * 0.135f)));
            // An upper lip that stays put and a lower one that goes down with the jaw.
            add(m, render::applyTransform(
                       render::applyTransform(render::shapes::makeSphere(1.0f, 10, 12, lip),
                                              render::scaleMatrix(math::vec3(hw * 0.310f, hh * 0.040f,
                                                                             hd * 0.080f))),
                       sk.head * detail::move(0.0f, mouthY + hh * 0.040f, mouthZ - hd * 0.044f)));
            add(m, render::applyTransform(
                       render::applyTransform(render::shapes::makeSphere(1.0f, 10, 12, lip),
                                              render::scaleMatrix(math::vec3(hw * 0.290f, hh * 0.046f,
                                                                             hd * 0.080f))),
                       sk.head * detail::move(0.0f,
                                              mouthY - hh * (0.044f + 0.21f * openMouth) - jawDrop,
                                              mouthZ - hd * 0.044f)));
            // The corners. A smile is not a curved line at this size — it is where the two ends of the
            // mouth sit, and moving them a couple of millimetres is the whole difference between
            // somebody pleased and somebody about to say something they will regret.
            for (int s2 = 0; s2 < 2; ++s2) {
                add(m, render::applyTransform(
                           render::applyTransform(render::shapes::makeSphere(1.0f, 8, 10, inside),
                                                  render::scaleMatrix(math::vec3(
                                                      hw * 0.055f, hh * 0.030f, hd * 0.06f))),
                           sk.head * detail::move(sideSign(s2) * hw * 0.300f,
                                                  mouthY + face.smile * hh * 0.075f - jawDrop * 0.4f,
                                                  faceZ(hw * 0.300f, mouthY) - hd * 0.048f)));
            }
        }
    }

    // ---- arms ------------------------------------------------------------------------------------
    for (int s = 0; s < 2; ++s) {
        const math::vec3 sh = Skeleton::at(sk.shoulder[s]);
        const math::vec3 el = Skeleton::at(sk.elbow[s]);
        const math::vec3 wr = Skeleton::at(sk.wrist[s]);
        // One arm, in skin, from shoulder to wrist — and a sleeve laid over the top of it. Drawing the
        // upper arm in the shirt colour and the forearm in skin puts the change of colour exactly at
        // the elbow, which is the one place a sleeve never ends.
        add(m, limb(sh, el, b.m(b.upperArmR), b.m(b.elbowR), b.skin));
        add(m, limb(el, wr, b.m(b.foreArmR), b.m(b.wristR), b.skin));
        // The sleeve doubles as the deltoid: it is widest where it caps the shoulder and narrows down
        // the arm, which is the shape a shoulder is. A separate ball for the deltoid is a second
        // surface, and a second surface at a joint is the bead problem all over again.
        // How far down the arm it runs is what tells a short-sleeved shirt from a coat at fifty
        // metres, which is further than any face can be read from.
        const float run = b.sleeve < 0.15f ? 0.15f : b.sleeve;
        const math::vec3 sleeveEnd =
            run <= 1.0f ? sh + (el - sh) * run : el + (wr - el) * (run - 1.0f);
        const float cuffR = run <= 1.0f ? b.m(b.upperArmR * 1.04f) : b.m(b.foreArmR * 1.18f);
        add(m, limb(sh, sleeveEnd, b.m(b.upperArmR * 1.24f), cuffR, b.top));
        // The hand: a flattened wedge from the wrist, which at any real distance is what a hand is.
        add(m, render::applyTransform(
                   render::applyTransform(render::shapes::makeSphere(1.0f, 8, 12, b.skin),
                                          render::scaleMatrix(math::vec3(b.m(b.wristR * 1.05f),
                                                                         b.m(b.handLen * 0.44f),
                                                                         b.m(b.wristR * 1.45f)))),
                   sk.wrist[s] * detail::move(0.0f, -b.m(b.handLen) * 0.40f, 0.0f)));
    }

    // ---- what they are wearing --------------------------------------------------------------------
    //
    // Drawn after the torso and before the legs, so a coat hangs over the trousers rather than the
    // other way round, and so a skirt covers the top of the thighs.
    if (b.skirtY > 0.0f && b.skirtY < b.yPelvis) {
        add(m, hanging(sk.pelvis, b.m(0.02f), -b.m(b.yPelvis - b.skirtY), b.m(b.pelvisHalfW * 1.02f),
                       b.m(b.pelvisHalfD * 1.02f), b.m(b.pelvisHalfW * 1.62f),
                       b.m(b.pelvisHalfD * 1.62f), b.legwear));
    }
    if (b.coatY > 0.0f && b.coatY < b.yChest) {
        add(m, hanging(sk.chest, 0.0f, -b.m(b.yChest - b.coatY), b.m(b.chestHalfW * 1.10f),
                       b.m(b.chestHalfD * 1.12f), b.m(b.chestHalfW * 1.44f),
                       b.m(b.chestHalfD * 1.40f), b.top));
    }

    // ---- legs ------------------------------------------------------------------------------------
    for (int s = 0; s < 2; ++s) {
        const math::vec3 hp = Skeleton::at(sk.hip[s]);
        const math::vec3 kn = Skeleton::at(sk.knee[s]);
        const math::vec3 an = Skeleton::at(sk.ankle[s]);
        add(m, limb(hp, kn, b.m(b.thighR), b.m(b.kneeR), b.legwear));
        add(m, limb(kn, an, b.m(b.calfR), b.m(b.ankleR), b.legwear));
        // The foot runs FORWARD from the ankle and sits on the floor, which is the whole reason the
        // ankle is 4.5% of the height above the ground rather than on it.
        const math::mat4 footFrame = sk.ankle[s];
        add(m, render::applyTransform(
                   render::applyTransform(render::shapes::makeSphere(1.0f, 8, 14, b.shoes),
                                          render::scaleMatrix(math::vec3(b.m(b.ankleR * 1.52f),
                                                                         b.m(b.yAnkle * 0.58f),
                                                                         b.m(b.footLen * 0.54f)))),
                   footFrame * detail::move(0.0f, -b.m(b.yAnkle) * 0.42f, b.m(b.footLen) * 0.16f)));
        // The heel, squared off behind, so the shoe is not an egg.
        add(m, render::applyTransform(
                   render::applyTransform(render::shapes::makeBox(1.0f, b.shoes),
                                          render::scaleMatrix(math::vec3(b.m(b.ankleR * 1.34f),
                                                                         b.m(b.yAnkle * 0.80f),
                                                                         b.m(b.footLen * 0.34f)))),
                   footFrame * detail::move(0.0f, -b.m(b.yAnkle) * 0.46f, -b.m(b.footLen) * 0.16f)));
    }
    return m;
}

// The whole thing in one call, for when the caller has no use for the joints.
inline render::shapes::MeshData buildActor(const Build& b, const BodyPose& p,
                                          const Face& face = Face()) {
    return buildBody(b, skeletonOf(b, p), face);
}

} // namespace maz::film
