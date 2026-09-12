#pragma once

#include "maz/film/Actor.hpp"
#include "maz/math/Math.hpp"

#include <cmath>
#include <cstdint>

// maz::film PERFORM — what a body is doing, one instant at a time.
//
// A body with joints is a puppet; this is what makes it an actor. Three things, because three things
// are what an audience reads:
//
//   WALKING   A real gait cycle. The feet are placed in the WORLD and the knees are solved to reach
//             them, so a planted foot does not move while it is planted — which is the whole
//             difference between walking and skating, and the thing that is instantly wrong in every
//             cheap character animation. The cycle is driven by DISTANCE TRAVELLED rather than by
//             time, so it cannot desynchronise from the movement no matter how the speed changes.
//   STANDING  Nobody stands still. Breathing, a slow shift of weight from one leg to the other, a
//             small drift of the head. Take it away and a waiting character reads as a photograph
//             someone has pasted into the shot.
//   SPEAKING  The hands move on the syllable, and the head turns to whoever is being spoken to.
//
// The numbers are from gait measurement, not invention: a step is about 0.41 of a person's height, a
// comfortable cadence is near 115 steps a minute (so a stride takes a little over a second), the
// pelvis rises and falls about 4cm TWICE per stride and swings side to side about 4cm ONCE, and the
// arms swing opposite the legs. Getting the twice/once wrong is what makes an animated walk bounce
// like a hobby-horse.
namespace maz::film {

// All in fractions of the character's height, except where marked.
inline constexpr float kStep = 0.330f;        // one step; a stride is two of them
inline constexpr float kStanceShare = 0.62f;  // how much of the cycle each foot spends on the ground
inline constexpr float kFootLift = 0.024f;    // how high the swinging foot clears the floor
inline constexpr float kWalkBase = 0.030f;    // each foot this far off the midline when walking
inline constexpr float kPelvisBob = 0.024f;   // peak to peak, twice a stride
inline constexpr float kPelvisSway = 0.021f;  // amplitude, once a stride
inline constexpr float kPelvisDrop = 0.028f;  // below the straight-legged canon, so the knees are bent
inline constexpr float kPelvisTwist = 0.10f;  // radians; the shoulders counter it
inline constexpr float kArmSwing = 0.42f;     // radians at the shoulder, at a walking pace
inline constexpr float kHeelLift = 0.036f;    // how far the ankle may rise as the foot rolls

// What this character is doing right now. Everything the performance needs and nothing about how they
// are built, so one motive drives a man, a woman or a child.
struct Motive {
    math::vec3 position{0.0f, 0.0f, 0.0f}; // where they are, in metres
    float facing = 0.0f;                   // which way they face; 0 is +Z
    float travelled = 0.0f;                // metres walked so far — this alone drives the gait cycle
    float walkSpeed = 0.0f;                // metres/second, for the lean and the arm swing; 0 = standing

    float lookYaw = 0.0f;   // where they are looking, relative to facing
    float lookPitch = 0.0f; //
    float speaking = 0.0f;  // 0..1, how much the hands are in it
    float syllable = 0.0f;  // 0..1 through the current syllable, for the hands to move on
    float tension = 0.0f;   // 0..1; a wound-up body stands taller and holds its arms closer
    int gestureHand = kRight;
    std::uint32_t seed = 0; // so two people standing together do not breathe in unison
};

namespace detail {

inline float fract(float v) { return v - std::floor(v); }
inline float smootherstep(float t) {
    const float u = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return u * u * u * (u * (u * 6.0f - 15.0f) + 10.0f);
}
// A small deterministic offset per character, so nobody's idle is anybody else's.
inline float seedPhase(std::uint32_t seed, std::uint32_t salt) {
    const std::uint32_t h = (seed * 2654435761u + salt * 40503u) ^ (seed >> 13);
    return static_cast<float>(h % 10000u) / 10000.0f;
}

} // namespace detail

// Where one foot is, in the character's own space, at this point in the cycle.
//   phase   0 at this foot's heel strike, rising to 1 at the next
//   z       forward; the foot is world-fixed through the stance, so in body space it runs backward
struct FootState {
    float z = 0.0f;
    float lift = 0.0f;  // above the ankle's resting height
    float pitch = 0.0f; // toe down is positive
    bool planted = false;
};

inline FootState footAt(float phase, float stepMetres) {
    FootState f;
    const float p = detail::fract(phase);
    // The foot travels this far, in body space, while it is on the ground: the body moves a full
    // stride per cycle, so a foot standing still in the world moves back by the stance share of it.
    const float travel = kStanceShare * 2.0f * stepMetres;
    if (p < kStanceShare) {
        const float u = p / kStanceShare;
        f.z = travel * (0.5f - u);
        f.lift = 0.0f;
        f.planted = true;
        // Heel down at the strike, flat through the middle, up onto the toe to push off.
        f.pitch = u < 0.18f ? -0.26f * (1.0f - u / 0.18f)
                            : (u > 0.72f ? 0.55f * (u - 0.72f) / 0.28f : 0.0f);
    } else {
        const float u = (p - kStanceShare) / (1.0f - kStanceShare);
        f.z = travel * (-0.5f + detail::smootherstep(u));
        f.lift = std::sin(u * 3.14159265f);
        f.planted = false;
        // Trailing toe still pointed as it leaves, swinging round to heel-first for the landing.
        f.pitch = 0.55f * (1.0f - detail::smootherstep(u * 1.6f)) - 0.26f * detail::smootherstep(u);
    }
    return f;
}

// The whole performance, as a pose this body can be built in.
inline BodyPose performAt(const Build& b, const Motive& mv, float seconds) {
    BodyPose p;
    p.position = mv.position;
    p.facing = mv.facing;
    p.ankleSet = true;

    const float H = b.height;
    const float stepMetres = kStep * H;
    const float ankleY = b.m(b.yAnkle);
    const bool walking = mv.walkSpeed > 0.05f;

    // How hard they are walking, 0 at a stroll and 1 at a brisk pace, which scales everything the
    // gait does rather than any of it being switched on.
    const float effort = walking ? std::fmin(1.0f, mv.walkSpeed / (0.80f * H)) : 0.0f;

    if (walking) {
        const float cycle = mv.travelled / (2.0f * stepMetres);
        const float twoPi = 6.28318530718f;
        // The right foot strikes at phase 0, the left half a cycle later.
        const FootState fr = footAt(cycle, stepMetres);
        const FootState fl = footAt(cycle + 0.5f, stepMetres);
        const FootState* foot[2] = {&fr, &fl};
        for (int s = 0; s < 2; ++s) {
            p.ankle[s] = math::vec3(sideSign(s) * b.m(kWalkBase), ankleY + foot[s]->lift * b.m(kFootLift),
                                    foot[s]->z);
            p.footPitch[s] = foot[s]->pitch * effort;
        }

        // The pelvis. TWICE a cycle up and down — lowest when both feet are down, highest over the
        // stance leg — and ONCE a cycle from side to side, leaning over whichever leg has the weight.
        const float bob = -b.m(kPelvisBob) * 0.5f * (1.0f + std::cos(twoPi * 2.0f * cycle));
        const float sway = -b.m(kPelvisSway) * std::sin(twoPi * cycle) * effort;
        p.hips = math::vec3(sway, b.m(b.yPelvis) - b.m(kPelvisDrop) + bob * effort, 0.0f);
        p.sway = -0.06f * std::sin(twoPi * cycle) * effort;

        // Hips forward with the swinging leg, shoulders the other way. This counter-rotation is what
        // lets the arms swing without the whole trunk following them round.
        p.pelvisTwist = kPelvisTwist * effort * std::sin(twoPi * cycle);
        p.twist = -1.7f * p.pelvisTwist;
        // A walker leans into it a little — but only a little, and the neck has to take the lean back
        // out or the character walks along staring at their own shoes.
        p.lean = 0.028f + 0.045f * effort;

        // The arms swing OPPOSITE their own leg. The elbow closes as the arm comes forward, which is
        // what a swinging arm does and what a straight-armed marching figure does not.
        for (int s = 0; s < 2; ++s) {
            const float legForward = foot[s]->z / (kStanceShare * stepMetres);
            const float swing = -legForward * kArmSwing * effort;
            p.arm[s].swing = swing;
            p.arm[s].spread = 0.09f + 0.03f * effort;
            p.arm[s].elbow = 0.18f + 0.55f * (swing > 0.0f ? swing : 0.0f) + 0.12f * effort;
        }
    } else {
        // ------------------------------------------------------------------ standing, and alive
        const float breath = seconds * 1.05f + detail::seedPhase(mv.seed, 1u) * 6.2831853f;
        const float shift = seconds * 0.155f + detail::seedPhase(mv.seed, 2u) * 6.2831853f;
        const float weight = std::sin(shift); // which leg the weight is over, drifting slowly

        p.hips = math::vec3(b.m(0.008f) * weight,
                            b.m(b.yPelvis) - b.m(kPelvisDrop) + b.m(0.0022f) * std::sin(breath), 0.0f);
        p.sway = 0.035f * weight;
        p.pelvisTwist = 0.02f * std::sin(shift * 0.7f);
        p.twist = -0.5f * p.pelvisTwist;
        p.lean = 0.02f + 0.0075f * std::sin(breath) - 0.06f * mv.tension;

        // The unweighted leg relaxes: its knee softens and the foot turns out a little.
        for (int s = 0; s < 2; ++s) {
            const float mine = sideSign(s) * weight; // +1 when the weight is on this leg
            p.ankle[s] = math::vec3(sideSign(s) * b.m(b.hipHalf * (1.0f - 0.06f * mine)), ankleY,
                                    b.m(0.012f) * (mine < 0.0f ? -mine : 0.0f));
            p.footPitch[s] = 0.0f;
            p.arm[s].swing = 0.02f * std::sin(breath + static_cast<float>(s) * 0.7f);
            p.arm[s].spread = 0.10f + 0.012f * std::sin(breath * 0.8f) - 0.045f * mv.tension;
            p.arm[s].elbow = 0.14f + 0.20f * mv.tension + 0.015f * std::sin(breath * 0.9f);
        }
    }

    // ---------------------------------------------------------------------- the foot rolls
    //
    // At the ends of a stance the foot is not flat: it lands on the heel and leaves off the toe, and
    // both of those raise the ANKLE above its resting height while the ground contact stays put. That
    // is also, exactly, what buys the leg the reach it needs at full stride — so rather than guess the
    // roll, the ankle is lifted by however much the leg is short of where the gait wants the foot, and
    // no more. It self-tunes to any build and any pace, and in the middle of a stance, where the foot
    // really is flat, it lifts by nothing at all.
    {
        const float legMax = 0.985f * (b.m(b.yHip - b.yKnee) + b.m(b.yKnee - b.yAnkle));
        const float maxRise = b.m(kHeelLift);
        const float hipY = p.hips.y - b.m(b.yPelvis - b.yHip);
        for (int s = 0; s < 2; ++s) {
            const float dx = p.ankle[s].x - (p.hips.x + sideSign(s) * b.m(b.hipHalf));
            const float dz = p.ankle[s].z - p.hips.z;
            const float flat = legMax * legMax - dx * dx - dz * dz;
            const float canDrop = flat > 0.0f ? std::sqrt(flat) : 0.0f;
            const float lowest = hipY - canDrop;
            if (p.ankle[s].y < lowest) {
                p.ankle[s].y = std::fmin(lowest, p.ankle[s].y + maxRise);
            }
        }
    }

    // ---------------------------------------------------------------------- speaking, over the top
    //
    // The hands move on the syllable, not on a timer of their own: a gesture that is not on the beat
    // of the voice reads as a dub. Only one hand leads, because two hands doing the same thing at the
    // same time is semaphore.
    if (mv.speaking > 0.01f) {
        const int h = mv.gestureHand == kLeft ? kLeft : kRight;
        const float beat = std::sin(mv.syllable * 3.14159265f);
        const float amount = mv.speaking * (0.65f + 0.35f * (1.0f - mv.tension));
        p.arm[h].elbow += (0.55f + 0.45f * beat) * amount;
        p.arm[h].swing += (0.10f + 0.22f * beat) * amount;
        p.arm[h].spread += 0.10f * amount;
        const int other = h == kRight ? kLeft : kRight;
        p.arm[other].elbow += 0.16f * amount;
        p.lean += 0.02f * amount * beat;
    }

    // ---------------------------------------------------------------------- where they are looking
    //
    // A head turn of more than about 50 degrees takes the shoulders with it; below that the neck does
    // it alone. Letting the neck do all of it gives an owl.
    const float yaw = mv.lookYaw;
    const float neckLimit = 0.85f;
    const float overshoot = yaw > neckLimit ? yaw - neckLimit : (yaw < -neckLimit ? yaw + neckLimit : 0.0f);
    p.headYaw = yaw - overshoot;
    p.twist += overshoot * 0.8f;
    p.headPitch = mv.lookPitch;
    // Whatever the spine has done, the head comes back to level: people carry their heads upright and
    // look where they are going, and a head that simply rides on top of the spine is the difference
    // between walking and trudging.
    p.neckPitch = mv.lookPitch * 0.35f + 0.03f * mv.tension - p.lean * 0.85f;
    return p;
}

// The same, straight to a skeleton.
inline Skeleton performSkeleton(const Build& b, const Motive& mv, float seconds) {
    return skeletonOf(b, performAt(b, mv, seconds));
}

} // namespace maz::film
