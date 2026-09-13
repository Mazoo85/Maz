#pragma once

#include "maz/film/Actor.hpp"
#include "maz/film/Face.hpp"

#include <cmath>
#include <cstdint>
#include <string>

// maz::film EXPRESSION — what a face is doing, and why.
//
// Actor.hpp knows how to BUILD a face out of brows, lids, a mouth and a jaw. This decides what those
// numbers should be, from the one thing the film already knows about every shot: which beat of the
// story it is.
//
// That is the whole idea, and it is worth being plain about why it is the right one. The alternative —
// picking expressions at random, or cycling through a list — produces a character who is astonished
// during small talk and blank at the worst moment of their life. The reel already carries the beat
// (open, spark, push, turn, crisis, choice, after), the mood, and who is speaking. A face built from
// those is a face that is reacting to the scene it is in, for free, in every shot of every film.
//
// A reaction shot is half of film grammar: the reason to cut to somebody is to watch them take
// something in. Before this there was nothing to watch — a pale mask with two dots on it — and the
// cut to it was a cut to nothing.
namespace maz::film {

// A blink, as a number between 0 and 1 rather than a yes or a no.
//
// Face.hpp's blinkAt is a port of the browser's, and has to stay exactly what the browser does, so it
// is left alone. In three dimensions a lid is geometry that moves, so the same clock is read as a
// ramp: shut fast, open slower, which is what an eyelid does and is most of what makes a blink read
// as a blink rather than a dropped frame.
inline float blinkAmount(double seconds, std::uint32_t seed) {
    const double period = 3.4 + static_cast<double>((seed >> 3) % 24u) * 0.1;
    double phase = std::fmod(seconds + static_cast<double>(seed % 100u) * 0.037, period) / period;
    if (phase < 0.0) {
        phase += 1.0;
    }
    // The blink occupies the last 2.8% of the cycle — about a tenth of a second, which is what a blink
    // takes. The first third of that is the lid coming down; the rest is it going back up.
    const double from = 0.972;
    if (phase < from) {
        return 0.0f;
    }
    const double into = (phase - from) / (1.0 - from);
    return static_cast<float>(into < 0.34 ? into / 0.34 : 1.0 - (into - 0.34) / 0.66);
}

// The resting face of a story beat: what somebody's face is doing when nothing else is happening to
// it. `mood` is the shot's own 0..1, and it scales everything — the same beat played calm and played
// wound-up is the same expression at two strengths, which is how a performance builds.
inline Face faceForBeat(const std::string& beat, double mood) {
    const float m = static_cast<float>(mood < 0.0 ? 0.0 : (mood > 1.0 ? 1.0 : mood));
    Face f;
    if (beat == "open" || beat == "title" || beat == "end") {
        f.browLift = 0.05f;
        f.smile = 0.05f;
    } else if (beat == "spark") {
        f.browLift = 0.55f + 0.25f * m;        // something has just landed
        f.smile = 0.25f;
        f.squint = -0.10f;
    } else if (beat == "push") {
        f.browLift = -0.20f - 0.25f * m;       // set, getting on with it
        f.smile = -0.05f;
        f.squint = 0.12f * m;
    } else if (beat == "turn") {
        f.browLift = 0.70f + 0.20f * m;        // the shot where somebody finds out
        f.browTilt = 0.25f;
        f.smile = -0.15f;
    } else if (beat == "crisis") {
        f.browLift = -0.55f - 0.35f * m;       // down and knitted
        f.browTilt = -0.45f * m;
        f.smile = -0.55f - 0.25f * m;
        f.squint = 0.35f + 0.25f * m;
    } else if (beat == "choice") {
        f.browLift = -0.10f;
        f.browTilt = 0.60f * (0.5f + 0.5f * m); // inner ends up: the shape of not wanting to
        f.smile = -0.30f;
        f.squint = 0.15f;
    } else if (beat == "after") {
        f.browLift = 0.10f;
        f.browTilt = 0.25f;
        f.smile = 0.20f - 0.25f * m;
        f.squint = 0.05f;
    }
    return f;
}

// Everything a face is doing at one instant.
//
//   speaking   whether this is the person talking
//   syllable   0..1 through the current syllable, which the reel already counts for the gestures
//   toward    -1..1, where they are looking relative to straight ahead
struct Reacting {
    std::string beat;
    double mood = 0.0;
    bool speaking = false;
    float syllable = 0.0f;
    float toward = 0.0f;
    double seconds = 0.0;
    std::uint32_t seed = 0;
};

inline Face faceAt(const Reacting& r) {
    Face f = faceForBeat(r.beat, r.mood);

    // The mouth opens on the syllable, which is the same clock the hands move on — so the gesture, the
    // mouth and the voice are one performance rather than three things happening at once.
    if (r.speaking) {
        const float beatPhase = std::sin(r.syllable * 3.14159265f);
        f.mouthOpen = 0.12f + 0.62f * (beatPhase < 0.0f ? 0.0f : beatPhase);
        // Talking lifts the brows a little on the stressed part of a word. Almost nobody can name it
        // and everybody notices when it is missing.
        f.browLift += 0.12f * beatPhase;
    } else {
        // A listener is not a statue: the jaw stays shut, but the face keeps moving, slowly, on its
        // own seed so two people listening to the same line do not do it in unison.
        const float slow = std::sin(static_cast<float>(r.seconds) * 0.41f +
                                    static_cast<float>(r.seed % 97u) * 0.064f);
        f.browLift += 0.07f * slow;
        f.squint += 0.04f * slow;
    }

    f.blink = blinkAmount(r.seconds, r.seed);
    f.gaze = r.toward < -1.0f ? -1.0f : (r.toward > 1.0f ? 1.0f : r.toward);

    // Nothing leaves here out of range: a brow at 3 is a brow through the top of the head.
    auto bound = [](float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); };
    f.browLift = bound(f.browLift, -1.0f, 1.0f);
    f.browTilt = bound(f.browTilt, -1.0f, 1.0f);
    f.smile = bound(f.smile, -1.0f, 1.0f);
    f.squint = bound(f.squint, -0.3f, 1.0f);
    f.mouthOpen = bound(f.mouthOpen, 0.0f, 1.0f);
    f.blink = bound(f.blink, 0.0f, 1.0f);
    return f;
}

} // namespace maz::film
