#pragma once

#include "maz/film/Reel.hpp"

#include <cmath>

// maz::film camera — how close the camera is, where it points, and how it moves, ported from
// framingFor() and fadeAmount() in film/js/film-player.js.
//
// A shot carries two separate things: a FRAMING (wide, mid, close, two-shot, over-the-shoulder,
// insert, low) which says how close the camera sits, and a CAMERA MOVE (push, pull, pan, track,
// handheld, whip) which says what it does during the shot. They multiply.
//
// One distinction is load-bearing and easy to lose: `panY` is a framing CHOICE -- where the frame
// sits vertically, applied flat to everything -- while `panYMove` is camera MOTION, which a renderer
// with depth planes must parallax across them, because a real handheld camera's shake moves near
// things more than far ones. A still close-up's vertical offset must never pull the planes apart.
namespace maz::film {

struct CameraShot {
    double zoom = 1.0;
    double panX = 0.0;     // framing and movement, as a fraction of frame width
    double panY = 0.0;     // a framing choice: applied flat to every plane
    double panYMove = 0.0; // camera motion: parallaxed across planes
    double roll = 0.0;     // radians
};

inline constexpr double kFadeSeconds = 0.45; // black at the head and tail, and a dip between scenes

namespace detail {

inline double clamp01d(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }
inline double easeInOutd(double t) {
    return t < 0.5 ? 2.0 * t * t : -1.0 + (4.0 - 2.0 * t) * t;
}

} // namespace detail

// The camera for a shot at `progress` (0..1 through it). `time` is the FILM's elapsed seconds, not the
// shot's: handheld and whip shake on the film clock, because progress alone would make two shots of
// different lengths shake at different speeds.
inline CameraShot cameraFor(const Shot& shot, double progress, double time) {
    const double p = detail::easeInOutd(detail::clamp01d(progress));
    CameraShot cam;

    if (shot.framing == "mid") {
        cam.zoom = 1.35;
    } else if (shot.framing == "two") {
        cam.zoom = 1.5;
    } else if (shot.framing == "close") {
        cam.zoom = 1.95;
    } else if (shot.framing == "insert") {
        cam.zoom = 1.15;
    } else if (shot.framing == "ots") {
        cam.zoom = 1.75;
    } else if (shot.framing == "low") {
        cam.zoom = 1.45;
    }

    // A close-up looks at whoever is speaking, so the frame sits on their side.
    if (shot.framing == "close" && !shot.speaker.empty()) {
        cam.panX = static_cast<double>(shot.side) * 0.16;
    }
    if (shot.framing == "close") {
        cam.panY = -0.05;
    }

    if (shot.camera == "push") {
        cam.zoom *= 1.0 + 0.13 * p;
    } else if (shot.camera == "push-slow") {
        cam.zoom *= 1.0 + 0.06 * p;
    } else if (shot.camera == "pull") {
        cam.zoom *= 1.16 - 0.16 * p;
    } else if (shot.camera == "pan-l") {
        cam.panX += 0.09 - 0.18 * p;
    } else if (shot.camera == "pan-r") {
        cam.panX += -0.09 + 0.18 * p;
    } else if (shot.camera == "handheld") {
        // Three detuned sines beat against each other, so the shake never loops visibly. How wound up
        // the scene is decides how far it drifts.
        const double shake = 0.004 + shot.mood * 0.020;
        cam.panX += std::sin(time * 2.7) * shake + std::sin(time * 6.1) * shake * 0.4;
        cam.panYMove += std::cos(time * 3.3) * shake * 0.8 + std::sin(time * 5.2) * shake * 0.3;
        cam.zoom *= 1.0 + 0.01 * p;
    } else if (shot.camera == "track-l") {
        cam.panX += 0.13 - 0.26 * p;
    } else if (shot.camera == "track-r") {
        cam.panX += -0.13 + 0.26 * p;
    } else if (shot.camera == "whip") {
        // Fast at the start and settling: the tail of a whip pan, not the middle. The swing is held to
        // 0.09 deliberately -- this move adds no zoom of its own, so unlike push and pull there is no
        // overscan covering a wider one, and whatever it reaches is bare edge on the back plane.
        cam.panX += 0.09 * std::pow(1.0 - p, 3.0);
    } else {
        cam.zoom *= 1.0 + 0.018 * p; // never perfectly still
    }

    // A couple of degrees of roll, and only where the story has come apart.
    cam.roll = shot.mood > 0.7 ? ((shot.mood - 0.7) / 0.3) * 0.035 : 0.0;
    return cam;
}

// How black the frame is at `time`: 1 is fully black, 0 is clear. Black at the head and tail of the
// film, and a dip on every scene change -- a cut inside a scene does not dip, which is what makes a
// scene change read as a scene change.
inline double fadeAt(const Reel& reel, const Shot& shot, double time) {
    if (time < kFadeSeconds) {
        return 1.0 - time / kFadeSeconds;
    }
    if (time > reel.duration - kFadeSeconds) {
        return detail::clamp01d((time - (reel.duration - kFadeSeconds)) / kFadeSeconds);
    }
    const std::size_t i = static_cast<std::size_t>(shot.index);
    double out = 0.0;
    const double half = kFadeSeconds / 2.0;
    if (i + 1 < reel.shots.size() && reel.shots[i + 1].scene != shot.scene) {
        const double toEnd = shot.end() - time;
        if (toEnd < half) {
            out = std::fmax(out, 1.0 - toEnd / half);
        }
    }
    if (i > 0 && reel.shots[i - 1].scene != shot.scene) {
        const double since = time - shot.start;
        if (since < half) {
            out = std::fmax(out, 1.0 - since / half);
        }
    }
    return out;
}

} // namespace maz::film
