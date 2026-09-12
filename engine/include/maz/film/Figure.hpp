#pragma once

#include "maz/math/Math.hpp" // math::vec2
#include "maz/render/Path.hpp"

#include <array>
#include <cmath>
#include <string>
#include <vector>

// maz::film figures — a character as a silhouette, ported from film/js/film-figures.js.
//
// A character is TEN joint angles: head, torso, two arms with forearms, two legs with shins. The body
// is built as nine tapered quads and one ellipse for the head, all as sub-paths of ONE path filled
// once. That is not incidental -- drawing the segments as separate fills caused a 2.9-second stall in
// the browser, and collapsing them to a single fill is what fixed it. The same thing is true here.
//
// Angle convention (hand-carried from the browser and easy to get wrong): 0 hangs STRAIGHT DOWN, and a
// segment advances by (sin a, cos a) from its root, in a y-grows-downward space with the ground at
// y = 0. The torso is the one exception: it rests upright, so it is drawn at PI + torso.
namespace maz::film {

// Ten joints, in radians.
struct Pose {
    double head = 0.0;
    double torso = 0.0;
    double armL = 0.0;
    double foreL = 0.0;
    double armR = 0.0;
    double foreR = 0.0;
    double legL = 0.0;
    double shinL = 0.0;
    double legR = 0.0;
    double shinR = 0.0;
};

struct JointLimit {
    double lo;
    double hi;
};

// What a human body can actually do. A blend or a gesture must never leave these -- the whole point is
// that no amount of motion on top of a pose can produce a shape a person could not hold.
struct PoseLimits {
    JointLimit head{-0.6, 0.6};
    JointLimit torso{-0.5, 0.5};
    JointLimit armL{-2.6, 2.6};
    JointLimit foreL{-2.4, 0.2};
    JointLimit armR{-2.6, 2.6};
    JointLimit foreR{-0.2, 2.4};
    JointLimit legL{-0.9, 0.9};
    JointLimit shinL{-2.4, 0.1};
    JointLimit legR{-0.9, 0.9};
    JointLimit shinR{-2.4, 0.1};
};

inline constexpr PoseLimits kPoseLimits{};

namespace detail {

struct NamedPose {
    const char* name;
    Pose pose;
};

// The ten poses. Identical to POSES in film/js/film-figures.js.
inline const std::array<NamedPose, 10>& namedPoses() {
    static const std::array<NamedPose, 10> kPoses{{
        {"stand",            {0.00,  0.00,  0.12, -0.10, -0.12,  0.10,  0.04, -0.02, -0.04, -0.02}},
        {"hands-in-pockets", {-0.06, 0.04,  0.30, -0.70, -0.30,  0.70,  0.05, -0.02, -0.05, -0.02}},
        {"turn-away",        {0.42,  0.22,  0.05, -0.20, -0.35,  0.30,  0.10, -0.04, -0.12, -0.05}},
        {"reach",            {-0.16, -0.12, 1.70, -0.30, -0.20,  0.18,  0.14, -0.05, -0.16, -0.08}},
        {"point",            {-0.10, -0.06, 1.45, -0.08, -0.18,  0.16,  0.08, -0.03, -0.08, -0.03}},
        {"recoil",           {0.30,  0.34,  0.90, -1.40, -0.85,  1.35, -0.22, -0.30,  0.26, -0.24}},
        {"slump",            {0.34,  0.40,  0.08, -0.12, -0.08,  0.12,  0.06, -0.10, -0.06, -0.10}},
        {"head-in-hands",    {0.46,  0.30,  1.90, -2.00, -1.90,  1.95,  0.05, -0.05, -0.05, -0.05}},
        {"sit",              {0.05,  0.08,  0.35, -0.55, -0.35,  0.55,  0.85, -0.85,  0.80, -0.80}},
        {"walk",             {-0.04, -0.05, 0.55, -0.35, -0.55,  0.35,  0.60, -0.08, -0.35, -0.90}},
    }};
    return kPoses;
}

inline double clampTo(const JointLimit& l, double v) { return v < l.lo ? l.lo : (v > l.hi ? l.hi : v); }
inline bool inside(const JointLimit& l, double v) { return v >= l.lo && v <= l.hi; }

// One tapered quad from (x,y) along `angle` for `length`, `w0` wide at the root and `w1` at the tip.
// Appended as its own closed contour; returns the tip so the next segment can hang off it.
inline math::vec2 appendSegment(render::Path& path, double x, double y, double angle, double length,
                                double w0, double w1) {
    const double dx = std::sin(angle);
    const double dy = std::cos(angle);
    const double nx = dy; // normal, for the taper
    const double ny = -dx;
    const double tipX = x + dx * length;
    const double tipY = y + dy * length;
    path.moveTo(static_cast<float>(x + nx * w0), static_cast<float>(y + ny * w0));
    path.lineTo(static_cast<float>(tipX + nx * w1), static_cast<float>(tipY + ny * w1));
    path.lineTo(static_cast<float>(tipX - nx * w1), static_cast<float>(tipY - ny * w1));
    path.lineTo(static_cast<float>(x - nx * w0), static_cast<float>(y - ny * w0));
    path.close();
    return math::vec2{static_cast<float>(tipX), static_cast<float>(tipY)};
}

// Where the hips sit for a pose, after the figure has been planted on the ground (see lowestFootY).
inline double hipYFor(double h, const Pose& pose) {
    const double hipY = -h * 0.46;
    const double legLen = h * 0.24;
    const double footL = hipY + std::cos(pose.legL) * legLen + std::cos(pose.legL + pose.shinL) * legLen;
    const double footR = hipY + std::cos(pose.legR) * legLen + std::cos(pose.legR + pose.shinR) * legLen;
    const double lowest = footL > footR ? footL : footR;
    // A bent leg is shorter end to end than a straight one, so its foot stops short of the ground and
    // the figure would hang in the air. Shift the whole body down by the shortfall. A straight leg
    // reaches slightly PAST the ground line, and is deliberately left there -- the browser does the
    // same, and lifting the figure to meet it would make a standing character float.
    return lowest < 0.0 ? hipY - lowest : hipY;
}

} // namespace detail

// The ten pose names, in table order.
inline const std::vector<std::string>& poseNames() {
    static const std::vector<std::string> kNames = [] {
        std::vector<std::string> n;
        for (const detail::NamedPose& p : detail::namedPoses()) {
            n.emplace_back(p.name);
        }
        return n;
    }();
    return kNames;
}

// A pose by name, or null if there is no such pose.
inline const Pose* poseNamed(const std::string& name) {
    for (const detail::NamedPose& p : detail::namedPoses()) {
        if (name == p.name) {
            return &p.pose;
        }
    }
    return nullptr;
}

inline bool withinLimits(const Pose& p) {
    const PoseLimits& L = kPoseLimits;
    return detail::inside(L.head, p.head) && detail::inside(L.torso, p.torso) &&
           detail::inside(L.armL, p.armL) && detail::inside(L.foreL, p.foreL) &&
           detail::inside(L.armR, p.armR) && detail::inside(L.foreR, p.foreR) &&
           detail::inside(L.legL, p.legL) && detail::inside(L.shinL, p.shinL) &&
           detail::inside(L.legR, p.legR) && detail::inside(L.shinR, p.shinR);
}

inline Pose clampToLimits(const Pose& p) {
    const PoseLimits& L = kPoseLimits;
    Pose out;
    out.head = detail::clampTo(L.head, p.head);
    out.torso = detail::clampTo(L.torso, p.torso);
    out.armL = detail::clampTo(L.armL, p.armL);
    out.foreL = detail::clampTo(L.foreL, p.foreL);
    out.armR = detail::clampTo(L.armR, p.armR);
    out.foreR = detail::clampTo(L.foreR, p.foreR);
    out.legL = detail::clampTo(L.legL, p.legL);
    out.shinL = detail::clampTo(L.shinL, p.shinL);
    out.legR = detail::clampTo(L.legR, p.legR);
    out.shinR = detail::clampTo(L.shinR, p.shinR);
    return out;
}

// Where the lower foot lands, in the figure's own space (ground at y = 0, y grows downward). Zero or
// a little below means planted; above zero would mean hanging in the air.
inline double lowestFootY(double h, const Pose& pose) {
    const double hipY = detail::hipYFor(h, pose);
    const double legLen = h * 0.24;
    const double footL = hipY + std::cos(pose.legL) * legLen + std::cos(pose.legL + pose.shinL) * legLen;
    const double footR = hipY + std::cos(pose.legR) * legLen + std::cos(pose.legR + pose.shinR) * legLen;
    return footL > footR ? footL : footR;
}

// Where the head's centre ends up, for anything that needs to aim at it.
inline math::vec2 headCentre(double h, const Pose& pose) {
    const double hipY = detail::hipYFor(h, pose);
    const double unit = h * 0.01;
    // The torso tip, without building the path.
    const double torsoAngle = 3.14159265358979 + pose.torso;
    const double tipX = std::sin(torsoAngle) * h * 0.26;
    const double tipY = hipY + std::cos(torsoAngle) * h * 0.26;
    (void)unit;
    return math::vec2{static_cast<float>(tipX + h * 0.055 * std::sin(pose.head)),
                      static_cast<float>(tipY - h * 0.055 * std::cos(pose.head))};
}

// The whole body as ONE path: nine tapered limb segments and an ellipse for the head, in the figure's
// own space with the feet on y = 0 and the body rising to about y = -h. Fill it with the NONZERO rule
// -- every sub-path is wound the same way, so overlaps (the head against the torso, an arm across the
// chest) stay solid instead of cancelling to holes.
inline render::Path bodyPath(double h, const Pose& pose) {
    render::Path path;
    const double hipY = detail::hipYFor(h, pose);
    const double unit = h * 0.01;
    const double legLen = h * 0.24;

    // legs -- 0 already hangs straight down, so no flip is needed here
    const math::vec2 kneeL =
        detail::appendSegment(path, -unit * 3.0, hipY, pose.legL, legLen, unit * 4.5, unit * 3.2);
    detail::appendSegment(path, kneeL.x, kneeL.y, pose.legL + pose.shinL, legLen, unit * 3.2, unit * 2.4);
    const math::vec2 kneeR =
        detail::appendSegment(path, unit * 3.0, hipY, pose.legR, legLen, unit * 4.5, unit * 3.2);
    detail::appendSegment(path, kneeR.x, kneeR.y, pose.legR + pose.shinR, legLen, unit * 3.2, unit * 2.4);

    // torso, leaning from the hips -- the one segment that rests upright, hence the PI
    const math::vec2 tip = detail::appendSegment(path, 0.0, hipY, 3.14159265358979 + pose.torso,
                                                 h * 0.26, unit * 6.5, unit * 5.5);

    // arms, hung off the shoulders
    const math::vec2 elbowL = detail::appendSegment(path, tip.x - unit * 5.0, tip.y + unit * 1.5,
                                                    pose.armL, h * 0.19, unit * 3.0, unit * 2.2);
    detail::appendSegment(path, elbowL.x, elbowL.y, pose.armL + pose.foreL, h * 0.18, unit * 2.2,
                          unit * 1.6);
    const math::vec2 elbowR = detail::appendSegment(path, tip.x + unit * 5.0, tip.y + unit * 1.5,
                                                    pose.armR, h * 0.19, unit * 3.0, unit * 2.2);
    detail::appendSegment(path, elbowR.x, elbowR.y, pose.armR + pose.foreR, h * 0.18, unit * 2.2,
                          unit * 1.6);

    // head -- one more sub-path of the same fill, rotated with the neck
    const double headX = tip.x + h * 0.055 * std::sin(pose.head);
    const double headY = tip.y - h * 0.055 * std::cos(pose.head);
    path.ellipse(static_cast<float>(headX), static_cast<float>(headY), static_cast<float>(h * 0.052),
                 static_cast<float>(h * 0.062), static_cast<float>(pose.head));
    return path;
}

} // namespace maz::film
