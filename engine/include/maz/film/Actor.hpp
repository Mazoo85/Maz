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
//        wrist 0.485   ·  the wrist is level with the crotch, and the arms reach mid-thigh
//         knee 0.285   ·  the knee is NOT halfway down the leg; it is below halfway
//        ankle 0.045
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

// ------------------------------------------------------------------------------- what a body is like
//
// Heights are fractions of the total height; widths and radii likewise, so one number — `height` —
// scales the whole person and every proportion holds.
struct Build {
    float height = 1.78f; // metres, sole to crown
    float heads = 7.5f;   // how many head-heights tall; children are fewer, and it shows

    // the landmarks, as fractions of height
    float yAnkle = 0.045f;
    float yKnee = 0.285f;
    float yHip = 0.500f;
    float yPelvis = 0.520f;
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
    b.yHip = 0.492f;
    b.yPelvis = 0.512f;
    b.yWaist = 0.608f;
    b.yChest = 0.706f;
    b.yKnee = 0.272f;
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
struct Pose {
    math::vec3 position{0.0f, 0.0f, 0.0f}; // where they stand in the world, in metres
    float facing = 0.0f;                   // yaw about Y; 0 faces +Z

    math::vec3 hips{0.0f, 0.0f, 0.0f}; // pelvis, in their own space; y = 0 means "use the rest height"
    float lean = 0.0f;                 // forward/back through the spine, radians
    float sway = 0.0f;                 // sideways through the spine
    float twist = 0.0f;                // around, through the spine
    float neckPitch = 0.0f;
    float headYaw = 0.0f, headPitch = 0.0f;

    // An arm, at the shoulder and the elbow. `swing` is forward (+) or back; `spread` lifts it away
    // from the body; `elbow` bends it, and only ever one way, because an elbow does.
    struct Arm {
        float swing = 0.0f;
        float spread = 0.10f;
        float twist = 0.0f;
        float elbow = 0.12f;
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
inline Pose restPose(const Build& b) {
    Pose p;
    p.hips = math::vec3(0.0f, b.m(b.yPelvis), 0.0f);
    for (int s = 0; s < 2; ++s) {
        p.ankle[s] = math::vec3(sideSign(s) * b.m(b.hipHalf), b.m(b.yAnkle), 0.0f);
    }
    p.ankleSet = true;
    return p;
}

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
inline Skeleton skeletonOf(const Build& b, const Pose& poseIn) {
    using namespace detail;
    Pose pose = poseIn;
    if (!pose.ankleSet) {
        const Pose r = restPose(b);
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
                rotY(pose.twist * 0.25f) * rotX(pose.lean * 0.30f);
    sk.waist = sk.pelvis * move(0.0f, b.m(b.yWaist - b.yPelvis), 0.0f) * rotZ(pose.sway * 0.35f) *
               rotY(pose.twist * 0.35f) * rotX(pose.lean * 0.35f);
    sk.chest = sk.waist * move(0.0f, b.m(b.yChest - b.yWaist), 0.0f) * rotZ(pose.sway * 0.35f) *
               rotY(pose.twist * 0.40f) * rotX(pose.lean * 0.35f);
    sk.yoke = sk.chest * move(0.0f, b.m(b.yShoulder - b.yChest), 0.0f);
    sk.neck = sk.yoke * move(0.0f, b.m(b.yNeck - b.yShoulder), 0.0f) * rotX(pose.neckPitch);
    // The head sits a neck's length above, and its centre is half a head-height above the chin. The
    // chin is one head-height below the crown, so where it falls depends on HOW MANY HEADS tall this
    // body is — hardcoding an adult's 0.867 gives a child a head that floats above their own crown.
    const float chinFraction = 1.0f - 1.0f / b.heads;
    const float chinToCentre = b.m(b.headHalfH());
    sk.head = sk.neck * move(0.0f, b.m(chinFraction - b.yNeck) + chinToCentre, 0.0f) *
              rotY(pose.headYaw) * rotX(pose.headPitch);

    const float upperArm = b.m(b.yShoulder - b.yElbow);
    const float foreArm = b.m(b.yElbow - b.yWrist);
    for (int s = 0; s < 2; ++s) {
        const float sx = sideSign(s);
        const Pose::Arm& a = pose.arm[s];
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
    const math::vec3 forward = math::vec3(root * math::vec4(0.0f, 0.0f, 1.0f, 0.0f));
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
    for (int i = 0; i < points; ++i) {
        const float a = 6.28318530718f * static_cast<float>(i) / static_cast<float>(points);
        const math::vec4 p(std::cos(a) * halfX, localY, std::sin(a) * halfZ, 1.0f);
        out.push_back(math::vec3(frame * p));
    }
    return out;
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

} // namespace detail

// The whole surface, as one mesh with the colours baked into the vertices.
inline render::shapes::MeshData buildBody(const Build& b, const Skeleton& sk) {
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
        ribs.push_back(ring(sk.chest, 0.0f, b.m(b.chestHalfW), b.m(b.chestHalfD), P));
        ribs.push_back(ring(sk.yoke, -b.m(0.030f), b.m(b.yokeHalfW), b.m(b.yokeHalfD), P));
        ribs.push_back(ring(sk.yoke, 0.0f, b.m(b.yokeHalfW * 0.94f), b.m(b.yokeHalfD * 0.92f), P));
        // The trapezius. Going from the width of the shoulders to the width of the neck in one step
        // gives a figure a cone for a top and makes the neck look twice its length; the slope has to
        // take a run at it.
        ribs.push_back(ring(sk.yoke, b.m(0.012f), b.m(b.yokeHalfW * 0.74f), b.m(b.yokeHalfD * 0.80f), P));
        ribs.push_back(ring(sk.yoke, b.m(0.026f), b.m(b.yokeHalfW * 0.44f), b.m(b.yokeHalfD * 0.60f), P));
        add(m, tint(render::skinSections(ribs, true, false), b.top));
        // Cap the two open ends so the body is not a pipe you can see down.
        add(m, render::applyTransform(
                   render::applyTransform(render::shapes::makeSphere(b.m(b.pelvisHalfW) * 0.98f, 10, 16,
                                                                     b.legwear),
                                          render::scaleMatrix(math::vec3(1.0f, 0.55f, 0.70f))),
                   sk.pelvis * detail::move(0.0f, -b.m(0.050f), 0.0f)));
        add(m, render::applyTransform(render::shapes::makeSphere(b.m(b.yokeHalfW * 0.44f), 8, 14, b.top),
                                      sk.yoke * detail::move(0.0f, b.m(0.022f), 0.0f)));
    }

    // ---- neck and head ---------------------------------------------------------------------------
    {
        const math::vec3 neckBase = Skeleton::at(sk.neck);
        const math::vec3 headBase =
            Skeleton::at(sk.head * math::vec4(0.0f, -b.m(b.headHalfH()) * 0.80f, 0.0f, 1.0f));
        add(m, limb(neckBase, headBase, b.m(b.neckR), b.m(b.neckR * 0.94f), b.skin, 10));

        // The canon measures the head from the chin to the top of the HAIR, so the skull is a little
        // shorter than a head-height and the hair makes up the rest. Getting this backwards adds two
        // centimetres to everyone, which is invisible in one figure and obvious the moment two of them
        // stand next to a door frame.
        const math::mat4 headScale = render::scaleMatrix(
            math::vec3(b.m(b.headHalfW()), b.m(b.headHalfH() * 0.93f), b.m(b.headHalfD())));
        add(m, render::applyTransform(
                   render::applyTransform(render::shapes::makeSphere(1.0f, 14, 20, b.skin), headScale),
                   sk.head));
        // A jaw: a smaller mass set forward and down. A head that is one egg reads as a mannequin.
        add(m, render::applyTransform(
                   render::applyTransform(render::shapes::makeSphere(1.0f, 10, 14, b.skin),
                                          render::scaleMatrix(math::vec3(b.m(b.headHalfW() * 0.80f),
                                                                         b.m(b.headHalfH() * 0.46f),
                                                                         b.m(b.headHalfD() * 0.82f)))),
                   sk.head * detail::move(0.0f, -b.m(b.headHalfH() * 0.42f), b.m(b.headHalfD() * 0.08f))));
        // Hair: a cap set back and up, so it is proud of the skull over the crown and the back of the
        // head and clear of the face at the front. Its top lands exactly on the crown.
        add(m, render::applyTransform(
                   render::applyTransform(render::shapes::makeSphere(1.0f, 12, 18, b.hair),
                                          render::scaleMatrix(math::vec3(b.m(b.headHalfW() * 1.06f),
                                                                         b.m(b.headHalfH() * 0.94f),
                                                                         b.m(b.headHalfD() * 1.06f)))),
                   sk.head * detail::move(0.0f, b.m(b.headHalfH() * 0.06f), -b.m(b.headHalfD() * 0.10f))));
        // A nose, because at any distance a nose is what says which way a head is turned.
        add(m, render::applyTransform(
                   render::shapes::makeCone(b.m(b.headHalfW() * 0.20f), b.m(b.headHalfD() * 0.30f), 8,
                                            b.skin),
                   sk.head * detail::move(0.0f, -b.m(b.headHalfH() * 0.06f), b.m(b.headHalfD() * 0.92f)) *
                       detail::rotX(1.5707963f)));
        // Eyes, set into the face at the halfway line, where eyes are.
        const render::Color eyeWhite{0.90f, 0.89f, 0.87f, 1.0f};
        const render::Color iris{0.14f, 0.13f, 0.12f, 1.0f};
        for (int s = 0; s < 2; ++s) {
            const float sx = sideSign(s) * b.m(b.headHalfW() * 0.40f);
            const math::mat4 socket =
                sk.head * detail::move(sx, b.m(b.headHalfH() * 0.06f), b.m(b.headHalfD() * 0.80f));
            add(m, render::applyTransform(
                       render::shapes::makeSphere(b.m(b.headHalfW() * 0.20f), 8, 10, eyeWhite), socket));
            add(m, render::applyTransform(
                       render::shapes::makeSphere(b.m(b.headHalfW() * 0.11f), 6, 10, iris),
                       socket * detail::move(0.0f, 0.0f, b.m(b.headHalfW() * 0.12f))));
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
        const math::vec3 sleeveEnd = sh + (el - sh) * 0.50f;
        add(m, limb(sh, sleeveEnd, b.m(b.upperArmR * 1.24f), b.m(b.upperArmR * 1.04f), b.top));
        // The hand: a flattened wedge from the wrist, which at any real distance is what a hand is.
        add(m, render::applyTransform(
                   render::applyTransform(render::shapes::makeSphere(1.0f, 8, 12, b.skin),
                                          render::scaleMatrix(math::vec3(b.m(b.wristR * 1.05f),
                                                                         b.m(b.handLen * 0.44f),
                                                                         b.m(b.wristR * 1.45f)))),
                   sk.wrist[s] * detail::move(0.0f, -b.m(b.handLen) * 0.40f, 0.0f)));
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
inline render::shapes::MeshData buildActor(const Build& b, const Pose& p) {
    return buildBody(b, skeletonOf(b, p));
}

} // namespace maz::film
