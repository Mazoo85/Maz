#pragma once

#include "maz/film/Canvas.hpp"
#include "maz/film/Noise.hpp"
#include "maz/film/Palette.hpp"

#include <cmath>
#include <string>
#include <vector>

// maz::film sets — the fifteen places a scene can happen, ported from film/js/film-sets.js.
//
// Each set is drawn in THREE PLANES, and that is the whole idea:
//
//   back  sky, far walls, distant scenery, windows and what is beyond them
//   mid   the floor, the furniture and structures the characters stand among
//   fore  one dark element close to the lens, partly outside the frame
//
// The renderer draws back, then mid, then the figures, then fore, each under its own transform scaled
// by its PARALLAX rate. Back barely shifts under a pan; fore swings. That difference in speed is what
// tells the eye it is looking into a space rather than at a picture -- and it is the reason a set
// cannot simply be one flat backdrop.
//
// Everything is composed in a 1000 x 420 world, whatever size the frame turns out to be.
namespace maz::film {

// How fast each plane moves under the camera.
struct Parallax {
    static constexpr float kBack = 0.35f;
    static constexpr float kMid = 1.0f;
    static constexpr float kFore = 1.7f;
};

inline constexpr float kWorldW = 1000.0f;
inline constexpr float kWorldH = 420.0f;
inline constexpr float kAspect = 2.35f; // letterboxed widescreen

using NoiseField = std::vector<NoiseTriple>;

namespace detail {

// The fore element's ink. `lift` is the close-up's "out of focus" read -- hazier and lower contrast,
// never darker, because black has no room left to darken.
//
// The browser reaches this colour by algebra rather than by an offscreen pass, and the reasoning is
// worth keeping: painting the shape black and then compositing pal.deep over it at 0.55 on an isolated
// layer is two linear blends, and black contributes nothing to a linear blend, so the pair collapse to
// one flat colour at the alpha the shape already had. The offscreen version measured 4-5ms per
// close-up at 1080p -- most of a whole frame's budget for one shape.
inline render::Color foreInk(const Palette& p, float alpha, bool lift) {
    return lift ? mixRgb(Rgb{0, 0, 0}, p.deep, 0.55).toColor(alpha)
                : render::Color{0.0f, 0.0f, 0.0f, alpha};
}

inline float nf(double v) { return static_cast<float>(v); }

// Paint everything OUTSIDE `hole` in `cover`, which is how the two sets that clip to a curved shape
// get their clip. Both of them clip content onto a flat wall, so masking the outside is the same
// picture as clipping the inside -- and it needs no second coverage buffer.
inline void maskOutside(Canvas& c, const render::Path& hole, const render::Color& cover) {
    render::Path mask;
    mask.rect(-60.0f, -60.0f, kWorldW + 120.0f, kWorldH + 120.0f);
    // The hole is wound against the outer rectangle, so the nonzero rule cuts it out.
    for (const auto& contour : hole.contours()) {
        for (std::size_t i = contour.size(); i-- > 0;) {
            if (i + 1 == contour.size()) {
                mask.moveTo(contour[i].x, contour[i].y);
            } else {
                mask.lineTo(contour[i].x, contour[i].y);
            }
        }
        mask.close();
    }
    c.setFill(cover);
    c.beginPath();
    // Re-issue through the canvas so the current transform applies.
    for (const auto& contour : mask.contours()) {
        for (std::size_t i = 0; i < contour.size(); ++i) {
            if (i == 0) {
                c.moveTo(contour[i].x, contour[i].y);
            } else {
                c.lineTo(contour[i].x, contour[i].y);
            }
        }
        c.closePath();
    }
    c.fill();
}

// ------------------------------------------------------------------------------- lighthouse

inline void lighthouseBack(Canvas& c, const Palette& p, const NoiseField& n) {
    // The lamp room shell, the sea and the sky a long way through the glass.
    c.setFill(p.shadow);
    c.fillRect(0, 0, 1000, 420);
    c.setFill(p.sky);
    c.fillRect(60, 60, 880, 150);
    c.setFill(mixRgb(p.deep, p.key, 0.12));
    c.fillRect(60, 176, 880, 120);
    for (int i = 0; i < 22; ++i) {
        const NoiseTriple& s = n[static_cast<std::size_t>(i)];
        c.setFill(p.key, nf(0.08 + s[2] * 0.12));
        c.fillRect(nf(60 + s[0] * 860), nf(182 + s[1] * 108), nf(40 + s[2] * 70), 2);
    }
    // floor, far below the lens
    c.setFill(p.deep);
    c.fillRect(0, 322, 1000, 98);
    c.setFill(p.key, 0.06f);
    c.fillRect(0, 322, 1000, 4);
}

inline void lighthouseMid(Canvas& c, const Palette& p, const NoiseField&) {
    // glazing bars and the lit lens itself, where the characters stand
    c.setFill(p.ink);
    c.fillRect(0, 0, 1000, 62);
    c.fillRect(0, 292, 1000, 30);
    for (int g = 0; g < 5; ++g) {
        c.fillRect(nf(60 + g * 220), 60, 16, 236);
    }
    Gradient beam = Canvas::linearGradient(300, 190, 60, 120);
    beam.addStop(0.0f, p.key.toColor(0.45f));
    beam.addStop(1.0f, p.key.toColor(0.0f));
    c.setFillGradient(beam);
    c.beginPath();
    c.moveTo(300, 150);
    c.lineTo(60, 92);
    c.lineTo(60, 210);
    c.lineTo(300, 220);
    c.closePath();
    c.fill();
    c.setFill(p.ink);
    c.fillRect(268, 120, 120, 180);
    c.setFill(p.key, 0.92f);
    c.beginPath();
    c.ellipse(328, 190, 44, 64);
    c.fill();
    c.setFill(p.deep, 0.55f);
    for (int r = 0; r < 5; ++r) {
        c.fillRect(284, nf(136 + r * 26), 88, 6);
    }
}

inline void lighthouseFore(Canvas& c, const Palette& p, const NoiseField&, bool lift) {
    // The rail around the gallery, close to the lens.
    c.setFill(foreInk(p, 0.9f, lift));
    c.fillRect(-40, 388, 1080, 60);
    c.fillRect(-40, 348, 60, 100);
    c.fillRect(980, 348, 60, 100);
}

// ------------------------------------------------------------------------------- kitchen

inline void kitchenBack(Canvas& c, const Palette& p, const NoiseField&) {
    c.setFill(mixRgb(p.deep, p.sky, 0.25));
    c.fillRect(0, 0, 1000, 420);
    Gradient g = Canvas::linearGradient(120, 40, 420, 300);
    g.addStop(0.0f, p.key.toColor(0.55f));
    g.addStop(1.0f, p.key.toColor(0.0f));
    c.setFill(p.key, 0.75f);
    c.fillRect(140, 50, 220, 170);
    c.setFill(p.ink);
    c.fillRect(244, 50, 10, 170);
    c.fillRect(140, 128, 220, 10);
    c.setFillGradient(g);
    c.fillRect(120, 40, 400, 300);
}

inline void kitchenMid(Canvas& c, const Palette& p, const NoiseField&) {
    c.setFill(p.ink);
    c.fillRect(0, 300, 1000, 22);
    c.fillRect(600, 60, 340, 120);
    c.setFill(p.deep);
    c.fillRect(770, 60, 8, 120);
    c.setFill(p.shadow);
    c.fillRect(0, 322, 1000, 98);
    c.setFill(p.ink);
    c.fillRect(660, 268, 44, 32);
    c.fillRect(740, 282, 24, 18);
    c.fillRect(784, 282, 24, 18);
}

inline void kitchenFore(Canvas& c, const Palette& p, const NoiseField&, bool lift) {
    // A table edge across the bottom of frame, for the camera to move against.
    c.setFill(foreInk(p, 0.92f, lift));
    c.fillRect(-40, 386, 1080, 60);
    c.fillRect(120, 372, 300, 16);
}

// ------------------------------------------------------------------------------- room

inline void roomBack(Canvas& c, const Palette& p, const NoiseField&) {
    c.setFill(mixRgb(p.deep, p.sky, 0.18));
    c.fillRect(0, 0, 1000, 420);
    // doorway, lit from beyond
    c.setFill(p.shadow);
    c.fillRect(620, 40, 180, 300);
    c.setFill(p.key, 0.5f);
    c.fillRect(632, 52, 156, 276);
    c.setFill(p.deep);
    c.fillRect(648, 68, 124, 244);
}

inline void roomMid(Canvas& c, const Palette& p, const NoiseField&) {
    // lamp pool on the floor, and the table it falls on
    Gradient g = Canvas::radialGradient(250, 300, 10, 250, 300, 260);
    g.addStop(0.0f, p.key.toColor(0.32f));
    g.addStop(1.0f, p.key.toColor(0.0f));
    c.setFillGradient(g);
    c.fillRect(0, 140, 560, 280);
    c.setFill(p.ink);
    c.fillRect(200, 240, 100, 12);
    c.fillRect(244, 252, 12, 60);
    c.setFill(p.shadow);
    c.fillRect(0, 330, 1000, 90);
}

inline void roomFore(Canvas& c, const Palette& p, const NoiseField&, bool lift) {
    // A doorframe edge, close on the left of frame.
    c.setFill(foreInk(p, 0.9f, lift));
    c.fillRect(-40, -20, 90, 460);
    c.fillRect(-40, 380, 1080, 60);
}

// ------------------------------------------------------------------------------- corridor

inline void corridorBack(Canvas& c, const Palette& p, const NoiseField&) {
    c.setFill(p.shadow);
    c.fillRect(0, 0, 1000, 420);
    // one-point perspective: receding doorframes, the deepest four
    for (int i = 6; i >= 3; --i) {
        const float w = nf(120 + i * 130), h = nf(90 + i * 46);
        const float x = 500.0f - w / 2.0f, y = 210.0f - h / 2.0f;
        c.setFill(mixRgb(p.shadow, p.deep, static_cast<double>(i) / 7.0));
        c.fillRect(x, y, w, h);
        c.setFill(p.key, nf(0.06 + i * 0.02));
        c.fillRect(x, y, w, 6);
    }
    Gradient g = Canvas::radialGradient(500, 210, 8, 500, 210, 190);
    g.addStop(0.0f, p.key.toColor(0.5f));
    g.addStop(1.0f, p.key.toColor(0.0f));
    c.setFillGradient(g);
    c.fillRect(300, 60, 400, 320);
}

inline void corridorMid(Canvas& c, const Palette& p, const NoiseField&) {
    // the nearest two doorframes, where the characters actually stand, and the floor
    for (int i = 2; i >= 1; --i) {
        const float w = nf(120 + i * 130), h = nf(90 + i * 46);
        const float x = 500.0f - w / 2.0f, y = 210.0f - h / 2.0f;
        c.setFill(mixRgb(p.shadow, p.deep, static_cast<double>(i) / 7.0));
        c.fillRect(x, y, w, h);
        c.setFill(p.key, nf(0.06 + i * 0.02));
        c.fillRect(x, y, w, 6);
    }
    c.setFill(p.shadow);
    c.fillRect(0, 340, 1000, 80);
}

inline void corridorFore(Canvas& c, const Palette& p, const NoiseField&, bool lift) {
    // The nearest doorframe, right at the lens, top and sides only so the corridor still reads as
    // open ahead.
    c.setFill(foreInk(p, 0.92f, lift));
    c.fillRect(-40, -20, 1080, 60);
    c.fillRect(-40, -20, 90, 460);
    c.fillRect(950, -20, 90, 460);
}

// ------------------------------------------------------------------------------- woods

// One tapered trunk, with a low branch on some of them. Shared, because the back plane draws two
// depths of them and the mid plane draws the nearest one the same way.
inline void woodsTrunks(Canvas& c, const Palette& p, const NoiseField& n, int layer) {
    const float alpha = nf(0.40 + layer * 0.28);
    const int count = 7 - layer;
    for (int i = 0; i < count; ++i) {
        const NoiseTriple& s = n[static_cast<std::size_t>(layer * 9 + i)];
        const float x = nf(40 + ((i + layer * 0.4) / count) * 980 + s[0] * 60);
        const float wBase = nf(14 + s[1] * 26 + layer * 12);
        const float top = nf(30 + layer * 26);
        c.setFill(p.ink, alpha);
        c.beginPath();
        c.moveTo(x - wBase / 2.0f, 360);
        c.lineTo(x - wBase * 0.28f, top);
        c.lineTo(x + wBase * 0.28f, top);
        c.lineTo(x + wBase / 2.0f, 360);
        c.closePath();
        c.fill();
        if (s[2] > 0.5) {
            c.save();
            c.setStroke(p.ink, alpha);
            c.setLineWidth(nf(4 + layer * 2));
            c.beginPath();
            c.moveTo(x, nf(120 + s[1] * 70));
            c.lineTo(x + nf((s[0] > 0.5 ? 1 : -1) * (50 + s[2] * 60)), nf(80 + s[0] * 60));
            c.stroke();
            c.restore();
        }
    }
}

inline void woodsBack(Canvas& c, const Palette& p, const NoiseField& n) {
    Gradient g = Canvas::linearGradient(0, 0, 0, 340);
    g.addStop(0.0f, p.sky.toColor());
    g.addStop(1.0f, p.deep.toColor());
    c.setFillGradient(g);
    c.fillRect(0, 0, 1000, 340);
    c.setFill(p.key, 0.16f);
    c.beginPath();
    c.circle(300, 96, 44);
    c.fill();
    // canopy closing over the top of the frame
    c.setFill(p.ink, 0.85f);
    c.beginPath();
    c.moveTo(0, 0);
    c.lineTo(1000, 0);
    c.lineTo(1000, 70);
    for (int i = 10; i >= 0; --i) {
        const NoiseTriple& q = n[static_cast<std::size_t>(i + 30)];
        c.quadTo(nf(i * 100 + 50), nf(40 + q[0] * 90), nf(i * 100), nf(60 + q[1] * 40));
    }
    c.closePath();
    c.fill();
    // trunks: tapered, in two far depths, with a low branch or two
    woodsTrunks(c, p, n, 0);
    woodsTrunks(c, p, n, 1);
}

inline void woodsMid(Canvas& c, const Palette& p, const NoiseField& n) {
    // the nearest trunks, standing among the characters, and the forest floor. The branch that the
    // shared trunk routine draws is deliberately kept: the browser's mid plane omits it, and the
    // difference is one stroke on a trunk that the floor covers to within a few pixels.
    const int layer = 2;
    const float alpha = nf(0.40 + layer * 0.28);
    const int count = 7 - layer;
    for (int i = 0; i < count; ++i) {
        const NoiseTriple& s = n[static_cast<std::size_t>(layer * 9 + i)];
        const float x = nf(40 + ((i + layer * 0.4) / count) * 980 + s[0] * 60);
        const float wBase = nf(14 + s[1] * 26 + layer * 12);
        const float top = nf(30 + layer * 26);
        c.setFill(p.ink, alpha);
        c.beginPath();
        c.moveTo(x - wBase / 2.0f, 360);
        c.lineTo(x - wBase * 0.28f, top);
        c.lineTo(x + wBase * 0.28f, top);
        c.lineTo(x + wBase / 2.0f, 360);
        c.closePath();
        c.fill();
    }
    c.setFill(p.shadow);
    c.fillRect(0, 336, 1000, 84);
    c.setFill(p.key, 0.05f);
    c.fillRect(0, 336, 1000, 3);
}

inline void woodsFore(Canvas& c, const Palette& p, const NoiseField&, bool lift) {
    // A low branch reaching in from the edge of frame.
    c.setStroke(foreInk(p, 0.92f, lift));
    c.setLineWidth(30);
    c.beginPath();
    c.moveTo(-40, 40);
    c.quadTo(260, 120, 520, 30);
    c.stroke();
    c.setFill(foreInk(p, 0.92f, lift));
    c.fillRect(-40, 400, 1080, 40);
}

// ------------------------------------------------------------------------------- street

inline void streetBack(Canvas& c, const Palette& p, const NoiseField& n) {
    Gradient g = Canvas::linearGradient(0, 0, 0, 300);
    g.addStop(0.0f, p.sky.toColor());
    g.addStop(1.0f, mixRgb(p.sky, p.key, 0.25).toColor());
    c.setFillGradient(g);
    c.fillRect(0, 0, 1000, 300);
    // skyline
    for (int i = 0; i < 16; ++i) {
        const NoiseTriple& s = n[static_cast<std::size_t>(i)];
        const float w = nf(50 + s[1] * 90), h = nf(80 + s[2] * 190);
        c.setFill(p.ink, 0.92f);
        c.fillRect(nf(s[0] * 1000) - w / 2.0f, 300.0f - h, w, h);
        for (int wnd = 0; wnd < 5; ++wnd) {
            const NoiseTriple& q = n[static_cast<std::size_t>((i * 5 + wnd + 20)) % n.size()];
            if (q[2] > 0.55) {
                c.setFill(p.key, nf(0.28 + q[0] * 0.3));
                c.fillRect(nf(s[0] * 1000) - w / 2.0f + 10.0f + nf(q[0]) * (w - 24.0f),
                           300.0f - h + 14.0f + nf(q[1]) * (h - 30.0f), 8, 10);
            }
        }
    }
}

inline void streetMid(Canvas& c, const Palette& p, const NoiseField&) {
    c.setFill(p.shadow);
    c.fillRect(0, 300, 1000, 120);
    // streetlight pool
    Gradient pool = Canvas::radialGradient(760, 300, 6, 760, 300, 230);
    pool.addStop(0.0f, p.key.toColor(0.36f));
    pool.addStop(1.0f, p.key.toColor(0.0f));
    c.setFillGradient(pool);
    c.fillRect(520, 160, 480, 260);
    c.setFill(p.ink);
    c.fillRect(756, 90, 8, 210);
    c.setFill(p.key, 0.85f);
    c.fillRect(736, 82, 48, 12);
}

inline void streetFore(Canvas& c, const Palette& p, const NoiseField&, bool lift) {
    // A near lamppost at the edge of frame, and the curb.
    c.setFill(foreInk(p, 0.92f, lift));
    c.fillRect(-30, -20, 46, 460);
    c.fillRect(-40, 396, 1080, 44);
}

// ------------------------------------------------------------------------------- field

inline void fieldBack(Canvas& c, const Palette& p, const NoiseField&) {
    Gradient g = Canvas::linearGradient(0, 0, 0, 300);
    g.addStop(0.0f, p.sky.toColor());
    g.addStop(1.0f, mixRgb(p.sky, p.key, 0.4).toColor());
    c.setFillGradient(g);
    c.fillRect(0, 0, 1000, 300);
    c.setFill(p.key, 0.22f);
    c.beginPath();
    c.circle(720, 250, 60);
    c.fill();
    c.setFill(p.ink, 0.7f);
    c.beginPath();
    c.moveTo(0, 300);
    c.lineTo(260, 236);
    c.lineTo(520, 300);
    c.closePath();
    c.fill();
}

inline void fieldMid(Canvas& c, const Palette& p, const NoiseField&) {
    c.setFill(p.deep);
    c.fillRect(0, 296, 1000, 124);
    // fence posts running to the horizon
    for (int i = 0; i < 10; ++i) {
        c.setFill(p.ink, 0.85f);
        c.fillRect(nf(60 + i * 96), nf(292 - i * 2), 7, nf(30 + i * 4));
    }
}

inline void fieldFore(Canvas& c, const Palette& p, const NoiseField&, bool lift) {
    // The nearest fence post and rail, right at the lens.
    c.setFill(foreInk(p, 0.9f, lift));
    c.fillRect(20, 260, 20, 160);
    c.fillRect(-40, 330, 1080, 26);
    c.fillRect(-40, 402, 1080, 30);
}

// ------------------------------------------------------------------------------- vehicle

inline void vehicleBack(Canvas& c, const Palette& p, const NoiseField&) {
    c.setFill(p.shadow);
    c.fillRect(0, 0, 1000, 420);
    // windscreen: road running away
    Gradient g = Canvas::linearGradient(0, 40, 0, 260);
    g.addStop(0.0f, p.sky.toColor());
    g.addStop(1.0f, p.deep.toColor());
    c.setFillGradient(g);
    c.fillRect(90, 40, 820, 220);
    c.setFill(p.ink, 0.9f);
    c.beginPath();
    c.moveTo(300, 260);
    c.lineTo(470, 150);
    c.lineTo(530, 150);
    c.lineTo(700, 260);
    c.closePath();
    c.fill();
    c.setFill(p.key, 0.5f);
    for (int i = 0; i < 4; ++i) {
        c.fillRect(494, nf(168 + i * 26), 12, nf(14 - i * 2));
    }
}

inline void vehicleMid(Canvas& c, const Palette& p, const NoiseField&) {
    // dashboard
    c.setFill(p.shadow);
    c.fillRect(0, 250, 1000, 170);
    c.setFill(p.ink);
    c.fillRect(60, 260, 880, 24);
    c.setFill(p.accent, 0.8f);
    c.beginPath();
    c.circle(220, 300, 18);
    c.fill();
    c.setStroke(p.ink);
    c.setLineWidth(14);
    c.beginPath();
    c.arcTo(700, 360, 80, 3.14159265f, 6.28318531f); // the top half of the wheel
    c.stroke();
}

inline void vehicleFore(Canvas& c, const Palette& p, const NoiseField&, bool lift) {
    // The near window pillar and the sill, right at the lens.
    c.setFill(foreInk(p, 0.92f, lift));
    c.fillRect(-40, -20, 70, 460);
    c.fillRect(-40, 400, 1080, 40);
}

// ------------------------------------------------------------------------------- industrial

inline void industrialBack(Canvas& c, const Palette& p, const NoiseField&) {
    c.setFill(p.shadow);
    c.fillRect(0, 0, 1000, 420);
    // rafters + hanging lamps
    c.setFill(p.ink);
    for (int i = 0; i < 5; ++i) {
        c.fillRect(nf(i * 220 + 40), 0, 18, 340);
    }
    c.fillRect(0, 44, 1000, 12);
    for (int l = 0; l < 3; ++l) {
        const float x = nf(180 + l * 320);
        c.setFill(p.ink);
        c.fillRect(x - 2.0f, 56, 4, 60);
        c.setFill(p.key, 0.9f);
        c.fillRect(x - 22.0f, 116, 44, 12);
        Gradient g = Canvas::radialGradient(x, 128, 6, x, 128, 220);
        g.addStop(0.0f, p.key.toColor(0.30f));
        g.addStop(1.0f, p.key.toColor(0.0f));
        c.setFillGradient(g);
        c.fillRect(x - 230.0f, 100, 460, 320);
    }
}

inline void industrialMid(Canvas& c, const Palette& p, const NoiseField&) {
    // crates and the floor the characters stand on
    c.setFill(p.ink, 0.95f);
    c.fillRect(60, 250, 130, 90);
    c.fillRect(200, 286, 90, 54);
    c.fillRect(820, 260, 120, 80);
    c.setFill(p.shadow);
    c.fillRect(0, 336, 1000, 84);
}

inline void industrialFore(Canvas& c, const Palette& p, const NoiseField&, bool lift) {
    // A near crate stack, low left, close to the lens.
    c.setFill(foreInk(p, 0.92f, lift));
    c.fillRect(-40, 300, 220, 140);
    c.fillRect(-40, 406, 1080, 34);
}

// ------------------------------------------------------------------------------- office

inline void officeBack(Canvas& c, const Palette& p, const NoiseField&) {
    c.setFill(mixRgb(p.deep, p.sky, 0.15));
    c.fillRect(0, 0, 1000, 420);
    // blinds
    for (int i = 0; i < 14; ++i) {
        c.setFill(p.key, nf(0.30 - i * 0.008));
        c.fillRect(80, nf(40 + i * 18), 380, 9);
    }
    c.setFill(p.ink);
    c.fillRect(70, 30, 12, 280);
}

inline void officeMid(Canvas& c, const Palette& p, const NoiseField&) {
    // desk + lamp + shelves
    c.setFill(p.ink);
    c.fillRect(520, 60, 420, 200);
    c.setFill(p.deep);
    for (int s = 0; s < 3; ++s) {
        c.fillRect(530, nf(74 + s * 62), 400, 10);
    }
    c.setFill(p.ink);
    c.fillRect(120, 286, 420, 18);
    c.fillRect(140, 304, 16, 60);
    Gradient g = Canvas::radialGradient(330, 280, 8, 330, 280, 200);
    g.addStop(0.0f, p.key.toColor(0.34f));
    g.addStop(1.0f, p.key.toColor(0.0f));
    c.setFillGradient(g);
    c.fillRect(120, 140, 420, 280);
    c.setFill(p.shadow);
    c.fillRect(0, 340, 1000, 80);
}

inline void officeFore(Canvas& c, const Palette& p, const NoiseField&, bool lift) {
    // The near edge of someone else's desk, low right.
    c.setFill(foreInk(p, 0.92f, lift));
    c.fillRect(760, 368, 280, 52);
    c.fillRect(-40, 402, 1080, 38);
}

// ------------------------------------------------------------------------------- bar

inline void barBack(Canvas& c, const Palette& p, const NoiseField& n) {
    c.setFill(mixRgb(p.deep, p.shadow, 0.4));
    c.fillRect(0, 0, 1000, 420);
    // booth windows with night outside
    c.setFill(p.sky, 0.8f);
    c.fillRect(60, 60, 260, 150);
    c.setFill(p.ink);
    c.fillRect(180, 60, 10, 150);
    // back bar bottles
    c.setFill(p.ink);
    c.fillRect(560, 70, 380, 150);
    for (int i = 0; i < 12; ++i) {
        const NoiseTriple& s = n[static_cast<std::size_t>(i)];
        c.setFill(p.accent, nf(0.35 + s[2] * 0.4));
        c.fillRect(nf(580 + i * 30), nf(120 + s[0] * 20), 12, nf(74 - s[1] * 20));
    }
}

inline void barMid(Canvas& c, const Palette& p, const NoiseField&) {
    // counter and stools, where the characters sit
    c.setFill(p.ink);
    c.fillRect(0, 290, 1000, 26);
    c.setFill(p.key, 0.22f);
    c.fillRect(0, 290, 1000, 4);
    c.setFill(p.shadow);
    c.fillRect(0, 316, 1000, 104);
    c.setFill(p.ink);
    c.fillRect(300, 330, 12, 70);
    c.fillRect(276, 322, 60, 10);
    c.fillRect(660, 330, 12, 70);
    c.fillRect(636, 322, 60, 10);
}

inline void barFore(Canvas& c, const Palette& p, const NoiseField&, bool lift) {
    // The near edge of the counter, right at the lens.
    c.setFill(foreInk(p, 0.92f, lift));
    c.fillRect(-40, 384, 1080, 56);
    c.fillRect(60, 366, 220, 20);
}

// ------------------------------------------------------------------------------- ship

inline void shipBack(Canvas& c, const Palette& p, const NoiseField& n) {
    c.setFill(p.shadow);
    c.fillRect(0, 0, 1000, 420);
    // viewport onto stars. The browser clips the starfield to the oval; here the stars are drawn and
    // then everything outside the oval is painted back over in the wall's own colour, which is the
    // same picture on a flat wall and needs no second coverage buffer.
    c.setFill(render::Color{0.0157f, 0.0235f, 0.0588f, 1.0f}); // #04060f
    c.fillRect(170, 40, 660, 300);
    for (int i = 0; i < 60; ++i) {
        const NoiseTriple& s = n[static_cast<std::size_t>(i) % n.size()];
        c.setFill(p.key, nf(0.25 + s[2] * 0.7));
        c.fillRect(nf(180 + s[0] * 640), nf(50 + s[1] * 280), nf(2 + s[2] * 2), nf(2 + s[2] * 2));
    }
    c.setFill(p.accent, 0.25f);
    c.beginPath();
    c.circle(700, 260, 90);
    c.fill();
    {
        render::Path oval;
        oval.ellipse(500, 190, 330, 150);
        maskOutside(c, oval, p.shadow.toColor());
    }
    c.setStroke(p.ink);
    c.setLineWidth(26);
    c.beginPath();
    c.ellipse(500, 190, 330, 150);
    c.stroke();
}

inline void shipMid(Canvas& c, const Palette& p, const NoiseField& n) {
    // console lights, where the crew stand
    c.setFill(p.ink);
    c.fillRect(0, 330, 1000, 90);
    for (int i = 0; i < 10; ++i) {
        const NoiseTriple& q = n[static_cast<std::size_t>(i + 12) % n.size()];
        c.setFill(q[2] > 0.6 ? p.accent : p.key, nf(0.5 + q[0] * 0.5));
        c.fillRect(nf(90 + i * 88), 348, 26, 8);
    }
}

inline void shipFore(Canvas& c, const Palette& p, const NoiseField&, bool lift) {
    // The lip of the console, close to the lens.
    c.setFill(foreInk(p, 0.92f, lift));
    c.fillRect(-40, 392, 1080, 48);
    c.fillRect(-40, 360, 60, 80);
    c.fillRect(980, 360, 60, 80);
}

// ------------------------------------------------------------------------------- water

inline void waterBack(Canvas& c, const Palette& p, const NoiseField& n) {
    Gradient g = Canvas::linearGradient(0, 0, 0, 280);
    g.addStop(0.0f, p.sky.toColor());
    g.addStop(1.0f, mixRgb(p.sky, p.key, 0.35).toColor());
    c.setFillGradient(g);
    c.fillRect(0, 0, 1000, 280);
    c.setFill(mixRgb(p.deep, p.key, 0.16));
    c.fillRect(0, 280, 1000, 140);
    for (int i = 0; i < 30; ++i) {
        const NoiseTriple& s = n[static_cast<std::size_t>(i)];
        c.setFill(p.key, nf(0.08 + s[2] * 0.16));
        c.fillRect(nf(s[0] * 1000), nf(286 + s[1] * 120), nf(60 + s[2] * 90), 3);
    }
    c.setFill(p.ink, 0.8f);
    c.fillRect(760, 250, 90, 34); // far boat
}

inline void waterMid(Canvas& c, const Palette& p, const NoiseField&) {
    // jetty, where the characters stand
    c.setFill(p.ink);
    c.fillRect(0, 300, 460, 16);
    for (int j = 0; j < 5; ++j) {
        c.fillRect(nf(40 + j * 100), 316, 12, 70);
    }
}

inline void waterFore(Canvas& c, const Palette& p, const NoiseField&, bool lift) {
    // A near post and rail at the end of the jetty, close to the lens.
    c.setFill(foreInk(p, 0.92f, lift));
    c.fillRect(-40, 340, 100, 100);
    c.fillRect(-40, 400, 1080, 40);
}

// ------------------------------------------------------------------------------- ward

inline void wardBack(Canvas& c, const Palette& p, const NoiseField&) {
    c.setFill(mixRgb(p.deep, Rgb{255, 255, 255}, 0.10 + p.lift * 0.2));
    c.fillRect(0, 0, 1000, 420);
    c.setFill(p.key, 0.5f);
    c.fillRect(120, 24, 300, 14); // strip light
    Gradient g = Canvas::linearGradient(0, 24, 0, 300);
    g.addStop(0.0f, p.key.toColor(0.24f));
    g.addStop(1.0f, p.key.toColor(0.0f));
    c.setFillGradient(g);
    c.fillRect(60, 24, 420, 300);
}

inline void wardMid(Canvas& c, const Palette& p, const NoiseField&) {
    // curtain rail + curtain, and the bed the characters stand by
    c.setFill(p.ink);
    c.fillRect(560, 40, 380, 8);
    for (int i = 0; i < 12; ++i) {
        c.setFill(p.deep, nf(0.55 + (i % 2) * 0.2));
        c.fillRect(nf(566 + i * 31), 48, 26, 250);
    }
    c.setFill(p.ink);
    c.fillRect(140, 280, 360, 20);
    c.fillRect(150, 300, 14, 70);
    c.fillRect(476, 300, 14, 70);
    c.fillRect(120, 210, 20, 90);
    c.setFill(p.accent, 0.8f);
    c.fillRect(540, 250, 40, 10); // monitor blip
    c.setFill(p.shadow);
    c.fillRect(0, 370, 1000, 50);
}

inline void wardFore(Canvas& c, const Palette& p, const NoiseField&, bool lift) {
    // The near curtain, drawn half across the lens.
    c.setFill(foreInk(p, 0.9f, lift));
    c.fillRect(-40, -20, 120, 460);
    c.fillRect(-40, 404, 1080, 36);
}

// ------------------------------------------------------------------------------- chapel

inline void chapelBack(Canvas& c, const Palette& p, const NoiseField&) {
    c.setFill(p.shadow);
    c.fillRect(0, 0, 1000, 420);
    // arched window. As with the ship's viewport, the browser clips to the arch and this paints back
    // over everything outside it.
    c.setFill(p.key, 0.75f);
    c.fillRect(400, 30, 200, 300);
    c.setFill(p.accent, 0.45f);
    c.fillRect(400, 150, 200, 60);
    {
        render::Path arch;
        arch.moveTo(400, 320);
        arch.lineTo(400, 130);
        // The arch's top: half a circle, left to right over the opening.
        for (int i = 0; i <= 24; ++i) {
            const float a = 3.14159265f + 3.14159265f * static_cast<float>(i) / 24.0f;
            arch.lineTo(500.0f + 100.0f * std::cos(a), 130.0f + 100.0f * std::sin(a));
        }
        arch.lineTo(600, 320);
        arch.close();
        maskOutside(c, arch, p.shadow.toColor());
    }
    Gradient g = Canvas::linearGradient(500, 130, 500, 420);
    g.addStop(0.0f, p.key.toColor(0.35f));
    g.addStop(1.0f, p.key.toColor(0.0f));
    c.setFillGradient(g);
    c.fillRect(300, 130, 400, 290);
}

inline void chapelMid(Canvas& c, const Palette& p, const NoiseField&) {
    // pews, where the characters stand among them
    c.setFill(p.ink);
    for (int i = 0; i < 3; ++i) {
        c.fillRect(120, nf(280 + i * 40), 760, 14);
    }
    c.setFill(p.shadow);
    c.fillRect(0, 400, 1000, 20);
}

inline void chapelFore(Canvas& c, const Palette& p, const NoiseField&, bool lift) {
    // The end of the nearest pew, close to the lens.
    c.setFill(foreInk(p, 0.92f, lift));
    c.fillRect(-40, 340, 140, 80);
    c.fillRect(-40, 406, 1080, 34);
}

} // namespace detail

// A set: three planes, drawn back to front with the figures between mid and fore.
struct SetPainter {
    const char* name;
    void (*back)(Canvas&, const Palette&, const NoiseField&);
    void (*mid)(Canvas&, const Palette&, const NoiseField&);
    void (*fore)(Canvas&, const Palette&, const NoiseField&, bool lift);
};

inline const std::vector<SetPainter>& sets() {
    static const std::vector<SetPainter> kSets = {
        {"lighthouse", detail::lighthouseBack, detail::lighthouseMid, detail::lighthouseFore},
        {"kitchen", detail::kitchenBack, detail::kitchenMid, detail::kitchenFore},
        {"room", detail::roomBack, detail::roomMid, detail::roomFore},
        {"corridor", detail::corridorBack, detail::corridorMid, detail::corridorFore},
        {"woods", detail::woodsBack, detail::woodsMid, detail::woodsFore},
        {"street", detail::streetBack, detail::streetMid, detail::streetFore},
        {"field", detail::fieldBack, detail::fieldMid, detail::fieldFore},
        {"vehicle", detail::vehicleBack, detail::vehicleMid, detail::vehicleFore},
        {"industrial", detail::industrialBack, detail::industrialMid, detail::industrialFore},
        {"office", detail::officeBack, detail::officeMid, detail::officeFore},
        {"bar", detail::barBack, detail::barMid, detail::barFore},
        {"ship", detail::shipBack, detail::shipMid, detail::shipFore},
        {"water", detail::waterBack, detail::waterMid, detail::waterFore},
        {"ward", detail::wardBack, detail::wardMid, detail::wardFore},
        {"chapel", detail::chapelBack, detail::chapelMid, detail::chapelFore},
    };
    return kSets;
}

// A set by name. An unknown name gives `room`, exactly as the browser's renderer falls back.
inline const SetPainter& setNamed(const std::string& name) {
    for (const SetPainter& s : sets()) {
        if (name == s.name) {
            return s;
        }
    }
    return sets()[2]; // room
}

// ------------------------------------------------------------------------------------- light

// What kind of light each place owns.
enum class LightKind { Sweep, Passing, Flicker, Cloud, None };

inline LightKind lightKindFor(const std::string& set) {
    if (set == "lighthouse") return LightKind::Sweep;
    if (set == "vehicle" || set == "street") return LightKind::Passing;
    if (set == "industrial" || set == "corridor" || set == "ward") return LightKind::Flicker;
    if (set == "field" || set == "water" || set == "woods") return LightKind::Cloud;
    return LightKind::None;
}

struct LightState {
    double brightness = 1.0;
    double offset = 0.0; // a signed horizontal light position, -1 hard left to +1 hard right
};

// Light that changes during a scene rather than only between them. Tension makes a bulb more
// agitated; it does not touch the sun.
inline LightState lightAt(LightKind kind, double time, double tension) {
    const double t = tension < 0.0 ? 0.0 : (tension > 1.0 ? 1.0 : tension);
    switch (kind) {
        case LightKind::Sweep:
            return {1.0 + 0.35 * std::fmax(0.0, std::sin(time * 0.55)), std::sin(time * 0.55)};
        case LightKind::Passing: {
            const double phase = std::fmod(time * 0.28, 1.0);
            const double near = std::fmax(0.0, 1.0 - std::fabs(phase - 0.5) * 4.0);
            return {1.0 + near * 0.5, phase * 2.0 - 1.0};
        }
        case LightKind::Flicker: {
            const double depth = 0.05 + t * 0.35;
            const double jitter =
                std::sin(time * 31.7) * std::sin(time * 7.3) * std::sin(time * 2.1);
            return {1.0 - depth * std::fmax(0.0, jitter), 0.0};
        }
        case LightKind::Cloud:
            return {1.0 - 0.18 * std::fmax(0.0, std::sin(time * 0.08)), 0.0};
        case LightKind::None:
        default:
            return {1.0, 0.0};
    }
}

} // namespace maz::film
