#pragma once

#include "maz/film/Canvas.hpp"
#include "maz/film/Figure.hpp"
#include "maz/film/Noise.hpp"
#include "maz/film/Palette.hpp"

#include <cmath>
#include <cstdint>
#include <string>

// maz::film -- light catching a face, and something in a hand.
//
// The figures are silhouettes, and that is the look, so this is not a face drawn on a silhouette: it is
// two small highlights where the eyes are and a line where the mouth is, in the room's own key colour.
// From across a room you still see a shape; up close you see somebody thinking. Without it there is no
// reaction shot, and the reaction shot is half of film grammar -- the whole point of cutting to
// somebody is to watch them take something in.
//
// Gated on SIZE, not on framing. A head six pixels across cannot hold a face, and a rule expressed in
// framings has to be plumbed from the director through the artist and kept in step forever; "draw it
// when it is big enough to read" needs no plumbing and cannot fall out of step.
//
// A port of film/js/film-figures.js (drawFace, drawHeld, blinkAt), and a port on purpose: a film whose
// people have faces in one renderer and not the other is not one film.
namespace maz::film {

// Head radius in pixels below which a face is mud.
inline constexpr double kFaceMinHead = 13.0;

// Eyes shut for a moment, about every four seconds, off the figure's own seed so two people in a
// two-shot never blink together.
//
// The browser's `(seed >>> 3) % 24` and `seed % 100` are uint32 operations, so the same expressions on
// a std::uint32_t give the same numbers; the products are taken into doubles before they are
// multiplied, exactly as JavaScript does.
inline bool blinkAt(double seconds, std::uint32_t seed) {
    const double period = 3.4 + static_cast<double>((seed >> 3) % 24u) * 0.1;
    double phase = std::fmod(seconds + static_cast<double>(seed % 100u) * 0.037, period) / period;
    if (phase < 0.0) {
        phase += 1.0;                     // JS % keeps the dividend's sign; a negative clock still blinks
    }
    return phase > 0.972;
}

struct FaceLook {
    bool speaking = false;
    double mouthOpen = 0.0;   // 0 shut, 1 wide, on the syllable clock
    double seconds = 0.0;
    std::uint32_t seed = 0;
};

// The face, on top of the silhouette, in the figure's own space.
inline void drawFace(Canvas& c, const Palette& pal, const Joints& joints, const FaceLook& look) {
    const double rx = joints.headRx;
    const double ry = joints.headRy;
    const double open = look.mouthOpen < 0.0 ? 0.0 : (look.mouthOpen > 1.0 ? 1.0 : look.mouthOpen);
    const bool shut = blinkAt(look.seconds, look.seed);

    // Eyes look where the head is turning. The head already rotates; shifting the pupils on top of that
    // is the difference between a head pointed at somebody and a person looking at them.
    const double lookX = std::sin(joints.headAngle) * rx * 0.34;

    c.save();
    c.concat(1.0f, 0.0f, 0.0f, 1.0f, static_cast<float>(joints.headX), static_cast<float>(joints.headY));
    const float ca = static_cast<float>(std::cos(joints.headAngle));
    const float sa = static_cast<float>(std::sin(joints.headAngle));
    c.concat(ca, sa, -sa, ca, 0.0f, 0.0f);

    c.setFill(pal.key, look.speaking ? 0.85f : 0.6f);

    const double eyeY = -ry * 0.16;
    const double eyeDx = rx * 0.36;
    const double eyeR = rx * 0.112;
    for (int side = -1; side <= 1; side += 2) {
        const double x = static_cast<double>(side) * eyeDx + lookX;
        if (shut) {
            // A closed eye is a line, not a dot. Drawing nothing reads as a skull.
            c.fillRect(static_cast<float>(x - eyeR * 1.3), static_cast<float>(eyeY - eyeR * 0.32),
                       static_cast<float>(eyeR * 2.6), static_cast<float>(eyeR * 0.64));
        } else {
            c.beginPath();
            c.ellipse(static_cast<float>(x), static_cast<float>(eyeY), static_cast<float>(eyeR),
                      static_cast<float>(eyeR * 1.05));
            c.fill();
        }
    }

    // The mouth. Shut it is a line; open it is the same line given height, on the syllable clock the
    // gesture and the score already share.
    const double mouthY = ry * 0.40;
    const double mouthW = rx * 0.36;
    const double mouthH = rx * 0.055 + open * rx * 0.34;
    c.setFill(pal.key, look.speaking ? 0.7f : 0.34f);
    c.beginPath();
    c.ellipse(0.0f, static_cast<float>(mouthY), static_cast<float>(mouthW * (0.8 + open * 0.2)),
              static_cast<float>(mouthH * 0.5));
    c.fill();

    c.restore();
}

// ------------------------------------------------------------------ something held
//
// The whole story turns on an object and until this, no character ever touched one: the insert shot
// drew it floating on its own. A held thing is small, so it is a shape and a colour rather than a
// drawing -- at this size the insert glyphs are mud.

enum class HeldShape { Long, Flat, Round, Box };

inline HeldShape heldShapeFor(const std::string& object) {
    std::string word;
    for (char ch : object) {
        word += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    auto has = [&word](const char* needle) { return word.find(needle) != std::string::npos; };
    if (has("key") || has("knife") || has("pen") || has("screwdriver") || has("wrench") ||
        has("torch") || has("flashlight") || has("bottle")) {
        return HeldShape::Long;
    }
    if (has("letter") || has("photo") || has("photograph") || has("card") || has("note") ||
        has("map") || has("ticket") || has("page") || has("book") || has("file") || has("receipt")) {
        return HeldShape::Flat;
    }
    if (has("ring") || has("coin") || has("watch") || has("stone") || has("ball") ||
        has("locket") || has("medal") || has("disc") || has("record")) {
        return HeldShape::Round;
    }
    return HeldShape::Box;
}

inline void drawHeld(Canvas& c, const Palette& pal, double handX, double handY, double handAngle,
                     double h, const std::string& object) {
    const double u = h * 0.016;
    if (u < 1.1) {
        return;                            // too small to be anything but a speck
    }
    c.save();
    c.concat(1.0f, 0.0f, 0.0f, 1.0f, static_cast<float>(handX), static_cast<float>(handY));
    const float ca = static_cast<float>(std::cos(handAngle));
    const float sa = static_cast<float>(std::sin(handAngle));
    c.concat(ca, sa, -sa, ca, 0.0f, 0.0f);
    c.setFill(pal.accent, 0.92f);

    const float fu = static_cast<float>(u);
    switch (heldShapeFor(object)) {
        case HeldShape::Long:
            c.fillRect(-fu * 0.25f, -fu * 1.5f, fu * 0.5f, fu * 3.0f);
            c.fillRect(-fu * 0.9f, -fu * 1.5f, fu * 1.8f, fu * 0.9f);
            break;
        case HeldShape::Flat:
            c.fillRect(-fu * 1.5f, -fu, fu * 3.0f, fu * 2.0f);
            break;
        case HeldShape::Round:
            c.beginPath();
            c.circle(0.0f, 0.0f, fu * 1.25f);
            c.fill();
            break;
        case HeldShape::Box:
        default:
            c.fillRect(-fu * 1.2f, -fu * 1.1f, fu * 2.4f, fu * 2.2f);
            break;
    }
    c.restore();
}

} // namespace maz::film
