#pragma once

#include "maz/film/Camera.hpp"
#include "maz/film/Figure.hpp"
#include "maz/film/Noise.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

// maz::film -- the difference between a pose and a performance.
//
// Figure.hpp knows what shape a body holds. This is everything that MOVES it, and without it the
// native renderer draws people who are physically correct and completely dead: a figure holds one
// pose for the whole shot, never breathes, never looks at the person they are talking to, and snaps
// to a new shape at every cut.
//
// The browser has had all five of these since the renderer was written. The port took the poses and
// checked them against the browser to a fraction of a pixel, and did not take the motion -- so the
// native renderer's people were right and still. This closes that.
//
//   aliveAt    nobody standing still is still
//   gazeAt     turn toward whoever else is in the scene
//   blendPoses ease out of the pose the last shot left them in
//   walkAt     a stride, on the legs that are already there
//   gestureAt  the head and hand move on the speaker's own syllables
//
// AGREE WITH THE BROWSER is the rule this file lives by, as the rest of the port does. Every constant
// here is the browser's constant and every expression is the browser's expression in the same order,
// because a native renderer whose people move differently is not a second renderer of the same film.
// The one place that needs care is noted where it happens.
//
// Pure std + Figure. No GPU, no window, no canvas.
namespace maz::film {

inline constexpr double kBreathRate = 0.55;  // chest cycles per second, a resting adult
inline constexpr double kWeightRate = 0.21;  // the slower rock between one leg and the other
inline constexpr double kSettleRate = 0.13;  // the head, slower still, trailing the weight

inline constexpr double kGazeSpan = 320.0;   // world units at which the turn is essentially full
inline constexpr double kGazeHead = 0.34;    // radians of head turn at full
inline constexpr double kGazeTorso = 0.11;   // the torso follows, less

inline constexpr double kStride = 0.34;      // radians the hip swings either side of rest

// How long a figure takes to ease out of the pose the previous shot left them in.
inline constexpr double kPoseEase = 0.32;
// A walk: strides per second, and how far across the frame it carries them, in world units.
inline constexpr double kWalkRate = 0.85;
inline constexpr double kWalkTravel = 190.0;

// clamp01d lives in Camera.hpp, which this includes through Figure.hpp -- one definition, not two.

// A standing person is never still.
//
// Three slow cycles, all small, all on joints that already exist: weight rocks between the legs, the
// chest rises and falls, and the head settles after the weight does. This is most of what separates a
// puppet from somebody waiting for an answer.
//
// Driven by the shot clock and the figure's own seed -- never a random number, because two renders of
// one film have to match frame for frame. The seed only shifts the phase, so two people in a two-shot
// are not a chorus line.
//
// The phase line is the one place this file has to be careful. The browser writes `(seed % 17) * 0.37`
// on a JS number, where `%` is the remainder of a double; `seed` arrives as a uint32 from hashText, so
// the same expression on a std::uint32_t gives the same value, and the result is taken into a double
// before it is multiplied. Doing that multiply in integer arithmetic would be a different number.
inline Pose aliveAt(const Pose& pose, double seconds, std::uint32_t seed) {
    Pose out = pose;
    const double phase = static_cast<double>(seed % 17u) * 0.37;
    const double twoPi = 6.283185307179586;

    const double breath = std::sin((seconds * kBreathRate + phase) * twoPi);
    const double weight = std::sin((seconds * kWeightRate + phase * 0.61) * twoPi);
    const double settle = std::sin((seconds * kSettleRate + phase * 1.31) * twoPi);

    out.torso = detail::clampTo(kPoseLimits.torso, pose.torso + breath * 0.022);
    // The legs take the weight in opposition -- one straightens as the other gives.
    out.legL = detail::clampTo(kPoseLimits.legL, pose.legL + weight * 0.020);
    out.legR = detail::clampTo(kPoseLimits.legR, pose.legR - weight * 0.020);
    out.shinL = detail::clampTo(kPoseLimits.shinL, pose.shinL - weight * 0.010);
    out.shinR = detail::clampTo(kPoseLimits.shinR, pose.shinR + weight * 0.010);
    // The head trails the weight rather than leading it.
    out.head = detail::clampTo(kPoseLimits.head, pose.head + settle * 0.026 + breath * 0.008);
    return out;
}

// Turn a figure toward whoever they are sharing the scene with.
//
// Two figures facing straight out of the screen no matter where the other one stands is what makes a
// two-shot read as two portraits rather than a conversation. The head turns most and the torso follows
// about a third as far, because people lead with the head.
inline Pose gazeAt(const Pose& pose, double selfX, double otherX, double amount) {
    Pose out = pose;
    const double dx = otherX - selfX;
    if (dx == 0.0) {
        return out;                          // nobody turns toward themselves
    }
    const double strength = detail::clamp01d(std::fabs(dx) / kGazeSpan);
    const double scale = (dx < 0.0 ? -1.0 : 1.0) * strength * detail::clamp01d(amount);
    out.head = detail::clampTo(kPoseLimits.head, pose.head + kGazeHead * scale);
    out.torso = detail::clampTo(kPoseLimits.torso, pose.torso + kGazeTorso * scale);
    return out;
}

// Ease from one pose to another instead of snapping at the cut.
//
// Every joint is clamped on the way out, because the two ends are each inside the limits and the
// straight line between two angles is not always inside anything. `t` is clamped rather than
// extrapolated: overshooting a pose is how you get an elbow through a ribcage.
inline Pose blendPoses(const Pose& a, const Pose& b, double t) {
    const double k = detail::clamp01d(t);
    Pose out;
    auto mix = [&](const JointLimit& limit, double from, double to) {
        return detail::clampTo(limit, from + (to - from) * k);
    };
    out.head = mix(kPoseLimits.head, a.head, b.head);
    out.torso = mix(kPoseLimits.torso, a.torso, b.torso);
    out.armL = mix(kPoseLimits.armL, a.armL, b.armL);
    out.foreL = mix(kPoseLimits.foreL, a.foreL, b.foreL);
    out.armR = mix(kPoseLimits.armR, a.armR, b.armR);
    out.foreR = mix(kPoseLimits.foreR, a.foreR, b.foreR);
    out.legL = mix(kPoseLimits.legL, a.legL, b.legL);
    out.shinL = mix(kPoseLimits.shinL, a.shinL, b.shinL);
    out.legR = mix(kPoseLimits.legR, a.legR, b.legR);
    out.shinR = mix(kPoseLimits.shinR, a.shinR, b.shinR);
    return out;
}

// A walk cycle on the legs that are already there.
//
// `phase` runs 0..1 over one full stride. The two legs are half a cycle apart -- in phase they would be
// a bunny hop -- and each knee bends only on its swing, because a knee that bends on the stance leg
// drops the figure through the floor. bodyPath plants the figure by finding the lower foot and shifting
// the body down to meet the ground, so a cycle that always leaves one leg near straight keeps the walk
// on the floor instead of bobbing.
inline Pose walkAt(const Pose& pose, double phase) {
    Pose out = pose;
    const double a = phase * 6.283185307179586;
    const double swingL = std::sin(a);
    const double swingR = std::sin(a + 3.141592653589793);   // half a cycle behind

    out.legL = detail::clampTo(kPoseLimits.legL, pose.legL + swingL * kStride);
    out.legR = detail::clampTo(kPoseLimits.legR, pose.legR + swingR * kStride);
    // The knee folds only while the leg is coming forward. The max(0, ...) keeps the stance leg
    // straight, which is what holds the figure on the floor.
    out.shinL = detail::clampTo(kPoseLimits.shinL, pose.shinL - std::max(0.0, swingL) * 0.44);
    out.shinR = detail::clampTo(kPoseLimits.shinR, pose.shinR - std::max(0.0, swingR) * 0.44);
    // Arms counter-swing to the opposite leg, which is what makes it read as a walk and not a shuffle.
    out.armL = detail::clampTo(kPoseLimits.armL, pose.armL + swingR * 0.26);
    out.armR = detail::clampTo(kPoseLimits.armR, pose.armR + swingL * 0.26);
    return out;
}

// One syllable of movement: the head dips and the nearer hand lifts, both returning to rest by the end
// so syllables run back to back without the body drifting. The score fires a blip on this same clock,
// and so does the mouth.
inline Pose gestureAt(const Pose& pose, double phase) {
    const double clamped = detail::clamp01d(phase);
    // sin(pi) is not exactly zero in double precision, and that residue would keep a syllable from
    // landing exactly back at rest; the ends are pinned rather than computed.
    const double swing = (clamped <= 0.0 || clamped >= 1.0)
        ? 0.0
        : std::sin(clamped * 3.141592653589793);             // 0 -> 1 -> 0

    Pose out = pose;
    out.head = detail::clampTo(kPoseLimits.head, pose.head - swing * 0.07);
    out.torso = detail::clampTo(kPoseLimits.torso, pose.torso - swing * 0.02);
    out.armR = detail::clampTo(kPoseLimits.armR, pose.armR - swing * 0.22);
    out.foreR = detail::clampTo(kPoseLimits.foreR, pose.foreR + swing * 0.30);
    return out;
}

// Which pose a beat puts a body in.
//
// This is a straight port of the browser's chooser (film-figures.js poseFor) and it replaces one that
// was not: the native renderer picked its pose from hash(name + beat) + seed, which is a perfectly
// reasonable rule and a DIFFERENT one, so the two renderers have been putting the same character in
// different shapes in the same shot since the port was written. Nothing caught it because the pose
// fixtures were captured per-pose rather than per-shot.
//
// Three things the browser's rule does that the old one did not:
//   * a speaker never turns away -- turning away is a thing you do while somebody else is talking
//   * tension leans the draw toward the later, more extreme entries in each beat's list
//   * the seed is the SHOT's key, not the character's, so both people in a scene share a register
inline const std::vector<std::string>& posesForBeat(const std::string& beat) {
    static const std::map<std::string, std::vector<std::string>> kByBeat = {
        {"open", {"stand", "hands-in-pockets", "sit"}},
        {"spark", {"turn-away", "reach", "stand"}},
        {"push", {"walk", "point", "reach"}},
        {"turn", {"stand", "turn-away", "hands-in-pockets"}},
        {"crisis", {"recoil", "slump", "head-in-hands"}},
        {"choice", {"stand", "point", "reach"}},
        {"after", {"stand", "hands-in-pockets", "sit"}}
    };
    const auto it = kByBeat.find(beat);
    return it == kByBeat.end() ? kByBeat.at("open") : it->second;
}

inline std::string poseNameFor(const std::string& beat, double tension, bool isSpeaker,
                               std::uint32_t seed) {
    std::vector<std::string> pool = posesForBeat(beat);
    if (isSpeaker) {
        std::vector<std::string> facing;
        for (const std::string& name : pool) {
            if (name != "turn-away") {
                facing.push_back(name);
            }
        }
        pool = facing.empty() ? std::vector<std::string>{"stand"} : facing;
    }
    // Math.round on a positive is floor(x + 0.5); written out so the agreement is visible.
    const std::uint32_t rounded =
        static_cast<std::uint32_t>(std::floor(tension * 1000.0 + 0.5));
    Rng rng((hashText(beat + ":" + std::to_string(seed)) ^ rounded) & 0xFFFFFFFFu);
    const double raw = rng.next() * (1.0 - tension * 0.35) + tension * 0.35;
    const double bias = std::min(0.999, std::max(0.0, raw));
    const std::size_t at =
        static_cast<std::size_t>(bias * static_cast<double>(pool.size())) % pool.size();
    return pool[at];
}

inline const Pose& poseFor(const std::string& beat, double tension, bool isSpeaker,
                           std::uint32_t seed) {
    const Pose* p = poseNamed(poseNameFor(beat, tension, isSpeaker, seed));
    static const Pose kFallback{};
    return p == nullptr ? kFallback : *p;
}

// The key a shot draws its poses from: the film's seed and the shot's own start. Both figures in a
// scene share it, which is what keeps them in the same register as each other.
inline std::uint32_t shotKey(std::uint32_t seed, double start) {
    return static_cast<std::uint32_t>(
        (static_cast<std::int64_t>(seed) +
         static_cast<std::int64_t>(std::floor(start * 100.0 + 0.5))) & 0xFFFFFFFF);
}

// How many syllables a line of dialogue has -- the clock the gesture, the mouth and the score all
// share, so it has to be the browser's number and not a better one.
//
// The browser does NOT count vowels. It counts WORDS and multiplies by 1.7 (parse.js syllablesFor),
// after replacing everything that is not a letter, digit, apostrophe or space with a space. The first
// draft of this function counted vowel groups, which is a more defensible way to count syllables and
// the wrong one: it would have put the native renderer's gestures on a different clock from the
// browser's, and a mouth moving out of step with the sound is exactly what this port exists to avoid.
//
// Math.round on a positive number is floor(x + 0.5); std::round rounds half away from zero, which
// agrees for positives, but floor(x + 0.5) is written out so the agreement is visible rather than
// assumed.
inline int syllablesFor(const std::string& text) {
    int words = 0;
    bool inWord = false;
    for (char raw : text) {
        const unsigned char c = static_cast<unsigned char>(raw);
        const bool speech = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                            (c >= '0' && c <= '9') || c == '\'';
        if (speech && !inWord) {
            ++words;
        }
        inWord = speech;
    }
    const int scaled = static_cast<int>(std::floor(static_cast<double>(words) * 1.7 + 0.5));
    return scaled < 2 ? 2 : scaled;
}

} // namespace maz::film
