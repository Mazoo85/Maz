#pragma once

// filmreel — drawing one frame of a SCRIPT FORGE reel into a CPU image.
//
// Split out of main.cpp so the renderer can be tested without running the command-line tool, which is
// how apps/zomboid's simulation is tested too. main.cpp is the argument parsing and the file writing;
// everything that decides what a frame looks like is here.
//
// The film is composed in a 1000 x 420 WORLD and shown through a 2.35:1 letterboxed window, and it is
// drawn in THREE PLANES with the figures between the middle one and the front one:
//
//   back   sky, far walls, distant scenery, windows and what is beyond them   (parallax 0.35)
//   mid    the floor and the structures the characters stand among            (parallax 1.0)
//   figures                                                                  (at the mid rate)
//   fore   one dark element close to the lens, partly outside the frame       (parallax 1.7)
//
// Each plane gets its own transform, scaled by its own rate, so a pan barely shifts the back and
// swings the front. That difference in speed is the depth -- it is why a set is not one flat backdrop,
// and it is the part of the browser's renderer that took longest to earn.
//
// WHAT IS FAITHFUL, all checked against the browser rather than eyeballed: the fifteen sets (mean
// difference under half a level out of 255, the remainder being anti-aliasing on curved edges and
// 8-bit gradient rounding), the figures (1.9e-05 of a pixel), the palette (241 cases, exactly), the
// camera (200 cases, to machine epsilon), the scatter every set is furnished from (bit for bit), and
// the cutting (the same instants, shot for shot).
//
// WHAT IS STILL MISSING, said plainly: the rack-focus decision for a fore element covering a
// close-up, and the typeface -- the browser picks a different real face per caption kind and the
// stroke font has one, so the distinction is carried by size and spacing instead. Sound is the
// browser's job. Output is a lossless frame sequence, not a video file: the engine has an IVF
// demuxer and no codec.
#include "maz/film/Air.hpp"
#include "maz/film/Camera.hpp"
#include "maz/film/Canvas.hpp"
#include "maz/film/Figure.hpp"
#include "maz/film/Glyphs.hpp"
#include "maz/film/Noise.hpp"
#include "maz/film/Palette.hpp"
#include "maz/film/Reel.hpp"
#include "maz/film/Sets.hpp"
#include "maz/render/PathFill.hpp"
#include "maz/render/StrokeFont.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace filmreel {

using maz::render::Color;
using maz::render::Image;
using maz::render::Path;

// A stable number from a name, for the choices that must not wander between frames.
inline std::uint32_t hashName(const std::string& s) { return maz::film::hashText(s); }

// Where the figures stand, and how tall they are, in WORLD units. Ported from figureLayout() in
// film/js/film-player.js -- including the part that looks wrong and is not: a close-up's feet are at
// y = 530, well below the world's 420, because the frame is meant to cut the body off at the chest
// the way a real close-up does.
struct Spot {
    float x = 0.0f;
    float ground = 0.0f;
    float height = 0.0f;
    std::string name;
    bool foreground = false; // drawn on the FORE plane: a listener close to the lens
};

inline std::vector<Spot> layoutFor(const maz::film::Shot& shot) {
    std::vector<Spot> out;
    if (shot.characters.empty()) {
        return out;
    }
    const std::string& first = shot.characters.front();
    const std::string speaker = shot.speaker.empty() ? first : shot.speaker;

    if (shot.framing == "close") {
        out.push_back({520.0f, 530.0f, 340.0f, speaker, false});
        return out;
    }
    if (shot.framing == "ots") {
        // The listener is a big dark shape at the edge; the speaker is beyond them.
        std::string other = speaker;
        for (const std::string& n : shot.characters) {
            if (n != speaker) {
                other = n;
            }
        }
        out.push_back({610.0f, 372.0f, 250.0f, speaker, false});
        out.push_back({250.0f, 470.0f, 430.0f, other, true});
        return out;
    }
    if (shot.framing == "low") {
        out.push_back({500.0f, 420.0f, 330.0f, speaker, false});
        return out;
    }
    const bool mid = shot.framing == "mid";
    if (shot.characters.size() == 1) {
        out.push_back({560.0f, 356.0f, mid ? 210.0f : 170.0f, first, false});
        return out;
    }
    out.push_back({360.0f, 356.0f, mid ? 205.0f : 168.0f, first, false});
    out.push_back({660.0f, 350.0f, mid ? 198.0f : 162.0f, shot.characters[1], false});
    return out;
}

// Which pose a beat puts a body in. The film's emotional shape reaches the picture through this table
// and nothing else.
inline const maz::film::Pose& poseForBeat(const std::string& beat, const std::string& who,
                                          std::uint32_t seed) {
    static const char* kByBeat[7][3] = {
        {"stand", "hands-in-pockets", "sit"},       // open
        {"turn-away", "reach", "stand"},            // spark
        {"walk", "point", "reach"},                 // push
        {"stand", "turn-away", "hands-in-pockets"}, // turn
        {"recoil", "slump", "head-in-hands"},       // crisis
        {"stand", "point", "reach"},                // choice
        {"stand", "hands-in-pockets", "sit"},       // after
    };
    static const char* kBeats[7] = {"open", "spark", "push", "turn", "crisis", "choice", "after"};
    int row = 0;
    for (int i = 0; i < 7; ++i) {
        if (beat == kBeats[i]) {
            row = i;
        }
    }
    const std::uint32_t pick = hashName(who + beat) + seed;
    const maz::film::Pose* p = maz::film::poseNamed(kByBeat[row][pick % 3u]);
    static const maz::film::Pose kFallback{};
    return p == nullptr ? kFallback : *p;
}

// One figure, in world units, through the plane's own canvas: a contact shadow that falls away from
// the light, then the same body three times -- a rim of the room's light on the side the light is on,
// the body itself near-black, and a wash of the character's own colour over the top.
inline void drawFigure(maz::film::Canvas& c, const maz::film::Palette& pal,
                       const maz::film::Voice* voice, const Spot& spot,
                       const maz::film::Pose& pose, bool speaking, float lightX) {
    const double h = static_cast<double>(spot.height);
    const float w = spot.height * 0.34f;

    const float lx = lightX < -1.0f ? -1.0f : (lightX > 1.0f ? 1.0f : lightX);

    c.setFill(Color{0.0f, 0.0f, 0.0f, 0.45f});
    c.beginPath();
    c.ellipse(spot.x - lx * w * 0.42f, spot.ground + 2.0f,
              w * (0.75f + std::fabs(lx) * 0.35f), spot.height * 0.035f);
    c.fill();

    const Color key = pal.key.toColor();
    const float rim = speaking ? 0.75f : 0.42f;
    const Path body = maz::film::bodyPath(h, pose);

    auto fillBody = [&](const Path& p, const Color& colour) {
        c.setFill(colour);
        c.beginPath();
        for (const auto& contour : p.contours()) {
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
    };

    Path lit = body;
    lit.translate(spot.x + lx * w * 0.055f, spot.ground - spot.height * 0.012f);
    fillBody(lit, Color{key.r, key.g, key.b, rim});

    Path solid = body;
    solid.translate(spot.x, spot.ground);
    fillBody(solid, Color{0.024f, 0.024f, 0.04f, 0.97f});

    if (voice != nullptr) {
        // The character's own hue, so two people in one frame are never confused.
        const float hue = voice->hue / 60.0f;
        const int sector = static_cast<int>(hue) % 6;
        const float f = hue - std::floor(hue);
        const float v = 0.58f, s = 0.65f;
        const float pp = v * (1.0f - s), q = v * (1.0f - s * f), t = v * (1.0f - s * (1.0f - f));
        Color tint{v, t, pp, 1.0f};
        if (sector == 1) tint = {q, v, pp, 1.0f};
        else if (sector == 2) tint = {pp, v, t, 1.0f};
        else if (sector == 3) tint = {pp, q, v, 1.0f};
        else if (sector == 4) tint = {t, pp, v, 1.0f};
        else if (sector == 5) tint = {v, pp, q, 1.0f};
        tint.a = speaking ? 0.16f : 0.08f;
        fillBody(solid, tint);
    }
}

// --------------------------------------------------------------------------------- captions

// Wrap to at most `maxLines`, dropping what will not fit. Ported from wrapLines() in
// film/js/film-player.js, including the cap: a caption that grew to four lines would crawl over the
// picture, so the fourth is simply not shown.
inline std::vector<std::string> wrapLines(const std::string& text, float maxWidth,
                                          const maz::render::TextStyle& style, std::size_t maxLines) {
    std::vector<std::string> lines;
    std::string line;
    std::string word;
    std::vector<std::string> words;
    for (const char ch : text) {
        if (ch == ' ' || ch == '\n' || ch == '\t') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word.push_back(ch);
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }
    for (const std::string& wd : words) {
        const std::string next = line.empty() ? wd : line + " " + wd;
        if (maz::render::textWidth(next, style) <= maxWidth || line.empty()) {
            line = next;
        } else {
            lines.push_back(line);
            line = wd;
            if (maxLines != 0 && lines.size() == maxLines) {
                line.clear();
                break;
            }
        }
    }
    if (!line.empty() && (maxLines == 0 || lines.size() < maxLines)) {
        lines.push_back(line);
    }
    return lines;
}

// Text with a soft drop shadow under it, so a caption stays readable over a bright set without a
// band behind it. The browser gets this from the canvas shadow (blur 14, offset y 2); here it is a
// few offset passes at low alpha, which is the same idea and costs a few small fills.
inline void shadowedText(Image& img, const std::string& text, float cx, float baseline,
                         const maz::render::TextStyle& style, const Color& colour, float alpha,
                         bool leftAligned = false, float leftX = 0.0f) {
    // Two offset passes, not four, and at six sub-scanlines rather than sixteen: this is a blur
    // standing in for the canvas shadow, and nobody reads the edge of a shadow. Five full-quality
    // passes of the same text made captions the most expensive thing in the frame.
    // Sub-sampling is deliberately low here and the reason is worth stating: a line of text is one
    // path of several thousand tiny contours, and unlike a set's big flat shapes none of its rows are
    // convex, so none of the rasterizer's shortcuts apply -- captions were the most expensive thing
    // left in a frame. Eight sub-scanlines on the words and four under a blur that nobody reads
    // sharply is indistinguishable at these sizes and a third of the cost.
    const float spread = std::fmax(1.0f, style.size * 0.055f);
    const Color shade{0.0f, 0.0f, 0.0f, 0.42f * alpha};
    for (int i = 0; i < 2; ++i) {
        const float ox = (i == 0 ? -spread : spread) * 0.7f;
        const float oy = spread;
        const maz::render::Path p =
            leftAligned ? maz::render::textPath(text, leftX + ox, baseline + oy, style)
                        : maz::render::textPathCentred(text, cx + ox, baseline + oy, style);
        maz::render::fillPath(img, p, shade, maz::render::FillRule::NonZero, 4);
    }
    const maz::render::Path p = leftAligned
                                    ? maz::render::textPath(text, leftX, baseline, style)
                                    : maz::render::textPathCentred(text, cx, baseline, style);
    maz::render::fillPath(img, p, Color{colour.r, colour.g, colour.b, colour.a * alpha},
                          maz::render::FillRule::NonZero, 8);
}

// Captions, ported from drawCaptions() in film/js/film-player.js. Drawn in SCREEN space, outside
// every plane and outside the roll, so a caption never tilts with the lens or slides with a pan.
//
// The layout is the browser's, not an invention: type scales with the frame (one unit is a 420th of
// its height), every caption fades in over the first eighth of its shot and out over the last, a slug
// line is a location stamp at the bottom LEFT with a coloured bar down its edge, and action and
// dialogue are anchored to the bottom of the frame and grow UPWARD so a three-line caption can never
// crawl over the picture.
//
// One honest difference: the browser picks a different typeface per kind -- bold sans for a title,
// monospace for a slug, italic serif for action. The stroke font has one face, so the distinction is
// carried by size, weight and letter-spacing instead.
inline void drawCaptions(Image& img, const maz::film::Palette& pal, const maz::film::Shot& shot,
                         float frameW, float frameH, float frameY, double progress) {
    if (shot.caption.empty()) {
        return;
    }
    const float unit = frameH / 420.0f;
    const float fadeIn = static_cast<float>(std::fmin(1.0, std::fmax(0.0, progress / 0.12)));
    const float fadeOut =
        1.0f - static_cast<float>(std::fmin(1.0, std::fmax(0.0, (progress - 0.88) / 0.12)));
    const float alpha = std::fmin(fadeIn, fadeOut);
    if (alpha <= 0.001f) {
        return;
    }
    const Color white{1.0f, 1.0f, 1.0f, 1.0f};
    const Color key = pal.key.toColor();
    const float cx = frameW * 0.5f;

    if (shot.kind == "title") {
        const maz::render::TextStyle big{52.0f * unit, 0.085f, 0.03f};
        const auto lines = wrapLines(shot.caption, frameW * 0.8f, big, 3);
        for (std::size_t i = 0; i < lines.size(); ++i) {
            shadowedText(img, lines[i], cx, frameY + frameH * 0.46f + static_cast<float>(i) * 60.0f * unit,
                         big, white, alpha);
        }
        if (!shot.subcaption.empty()) {
            maz::render::TextStyle small{17.0f * unit, 0.11f, 0.16f};
            std::string caps = shot.subcaption;
            for (char& ch : caps) {
                ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            }
            shadowedText(img, caps, cx,
                         frameY + frameH * 0.46f + static_cast<float>(lines.size()) * 60.0f * unit +
                             16.0f * unit,
                         small, Color{key.r, key.g, key.b, 0.9f}, alpha);
        }
        return;
    }
    if (shot.kind == "end") {
        const maz::render::TextStyle big{40.0f * unit, 0.085f, 0.03f};
        shadowedText(img, shot.caption, cx, frameY + frameH * 0.48f, big, white, alpha);
        if (!shot.subcaption.empty()) {
            const maz::render::TextStyle small{15.0f * unit, 0.11f, 0.14f};
            shadowedText(img, shot.subcaption, cx, frameY + frameH * 0.60f, small,
                         Color{key.r, key.g, key.b, 0.75f}, alpha);
        }
        return;
    }
    if (shot.kind == "establish") {
        // The slug line, bottom left, like a location stamp.
        const maz::render::TextStyle slug{16.0f * unit, 0.115f, 0.14f};
        std::string text = shot.caption;
        for (char& ch : text) {
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        }
        const float tw = maz::render::textWidth(text, slug);
        const float bx = frameW * 0.06f - 10.0f * unit;
        const float by = frameY + frameH * 0.80f;
        maz::render::Path box;
        box.rect(bx, by, tw + 28.0f * unit, 30.0f * unit);
        maz::render::fillPath(img, box, Color{0.0f, 0.0f, 0.0f, 0.55f * alpha});
        maz::render::Path bar;
        bar.rect(bx, by, 4.0f * unit, 30.0f * unit);
        maz::render::fillPath(img, bar, Color{key.r, key.g, key.b, alpha});
        shadowedText(img, text, 0.0f, by + 21.0f * unit, slug, white, alpha, /*leftAligned*/ true,
                     frameW * 0.06f + 6.0f * unit);
        return;
    }
    if (shot.kind == "action") {
        const maz::render::TextStyle body{19.0f * unit, 0.075f, 0.04f};
        const float lead = 26.0f * unit;
        const auto lines = wrapLines(shot.caption, frameW * 0.76f, body, 3);
        const float bottom = frameY + frameH * 0.90f;
        for (std::size_t i = 0; i < lines.size(); ++i) {
            const float y = bottom - static_cast<float>(lines.size() - 1 - i) * lead;
            shadowedText(img, lines[i], cx, y, body, Color{1.0f, 1.0f, 1.0f, 0.92f}, alpha);
        }
        return;
    }
    if (shot.kind == "line") {
        const maz::render::TextStyle body{23.0f * unit, 0.09f, 0.04f};
        const float lead = 30.0f * unit;
        const auto lines = wrapLines(shot.caption, frameW * 0.74f, body, 3);
        const float bottom = frameY + frameH * 0.90f;
        const float top = bottom - static_cast<float>(lines.size() - 1) * lead;

        std::string name = shot.speaker;
        if (!shot.parenthetical.empty()) {
            name += "  " + shot.parenthetical;
        }
        for (char& ch : name) {
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        }
        // The browser letter-spaces the name with thin spaces; the style's own tracking does it here.
        const maz::render::TextStyle who{17.0f * unit, 0.115f, 0.30f};
        shadowedText(img, name, cx, top - 34.0f * unit, who, Color{key.r, key.g, key.b, 0.95f}, alpha);

        for (std::size_t i = 0; i < lines.size(); ++i) {
            shadowedText(img, lines[i], cx, top + static_cast<float>(i) * lead, body, white, alpha);
        }
    }
}

// --------------------------------------------------------------------------------- the frame

// One frame of the film at `time`, drawn from the reel and nothing else. Pure: the same reel and the
// same time always give the same pixels.
inline Image drawFrame(const maz::film::Reel& reel, double time, int width, int height) {
    Image img(width, height, Color{0.0f, 0.0f, 0.0f, 1.0f});
    const maz::film::Shot* shot = maz::film::shotAt(reel, time);
    if (shot == nullptr || width < 8 || height < 8) {
        return img;
    }

    // The letterboxed window the film plays inside.
    const float frameW = static_cast<float>(width);
    const float frameH = std::fmin(static_cast<float>(height), frameW / maz::film::kAspect);
    const float frameY = (static_cast<float>(height) - frameH) / 2.0f;

    const double progress = shot->duration > 0.0 ? (time - shot->start) / shot->duration : 0.0;
    const maz::film::CameraShot cam = maz::film::cameraFor(*shot, progress, time);
    const maz::film::Palette pal = maz::film::paletteFor(reel.genre, shot->time, shot->mood);
    const maz::film::SetPainter& set = maz::film::setNamed(shot->set);
    const auto grain = maz::film::noise(maz::film::noiseKey(shot->set, reel.seed), 80);

    // What the room's light is doing right now: a lighthouse beam sweeping, headlights passing, a
    // failing bulb, cloud shadow -- or nothing, for the places that own no light of their own.
    const maz::film::LightState light =
        maz::film::lightAt(maz::film::lightKindFor(shot->set), time, shot->mood);

    const float scale = (frameW / maz::film::kWorldW) * static_cast<float>(cam.zoom);
    const float centreX = frameW * 0.5f;
    const float centreY = frameY + frameH * 0.5f;
    const float cosR = std::cos(static_cast<float>(cam.roll));
    const float sinR = std::sin(static_cast<float>(cam.roll));

    maz::film::Canvas canvas(img, static_cast<int>(frameY), static_cast<int>(frameH));

    // A plane's transform: into world space, scaled, offset by its own share of the pan, then rolled
    // about the centre of the frame -- one rotation for the whole picture, so the planes cannot drift
    // apart at their corners.
    //
    // panY is a framing CHOICE (a close-up's constant lift) and stays flat across every plane, because
    // scaling a constant offset by the rate would pull the planes apart on a shot with no move at all.
    // panYMove is camera MOTION and does get the rate, so a shaky frame shows the depth a pan does.
    auto usePlane = [&](float rate) {
        const float tx = centreX + static_cast<float>(cam.panX) * frameW * rate;
        const float ty = centreY + static_cast<float>(cam.panY) * frameH +
                         static_cast<float>(cam.panYMove) * frameH * rate;
        // world -> scaled -> translated
        const float a = scale, d = scale;
        const float e = tx - scale * maz::film::kWorldW * 0.5f;
        const float f = ty - scale * maz::film::kWorldH * 0.5f;
        // then rolled about (centreX, centreY)
        const float ra = cosR * a;
        const float rb = sinR * a;
        const float rc = -sinR * d;
        const float rd = cosR * d;
        const float re = cosR * (e - centreX) - sinR * (f - centreY) + centreX;
        const float rf = sinR * (e - centreX) + cosR * (f - centreY) + centreY;
        canvas.setTransform(ra, rb, rc, rd, re, rf);
    };

    if (shot->framing != "insert") {
        usePlane(maz::film::Parallax::kBack);
        set.back(canvas, pal, grain);

        usePlane(maz::film::Parallax::kMid);
        set.mid(canvas, pal, grain);

        // The figures, at the mid rate -- so at rest they land exactly where a single-plane renderer
        // would have put them.
        const auto spots = layoutFor(*shot);
        const maz::film::Voice* dummy = nullptr;
        (void)dummy;
        for (const Spot& spot : spots) {
            if (spot.foreground) {
                continue;
            }
            const bool speaking = !shot->speaker.empty() && spot.name == shot->speaker;
            drawFigure(canvas, pal, maz::film::voiceFor(reel, spot.name), spot,
                       poseForBeat(shot->beat, spot.name, reel.seed), speaking,
                       static_cast<float>(light.offset));
        }

        usePlane(maz::film::Parallax::kFore);
        set.fore(canvas, pal, grain, false);
        for (const Spot& spot : spots) {
            if (!spot.foreground) {
                continue;
            }
            const bool speaking = !shot->speaker.empty() && spot.name == shot->speaker;
            drawFigure(canvas, pal, maz::film::voiceFor(reel, spot.name), spot,
                       poseForBeat(shot->beat, spot.name, reel.seed), speaking,
                       static_cast<float>(light.offset));
        }
    } else {
        // An insert has no camera depth to it: the set shows faintly behind the object, on one plane.
        usePlane(maz::film::Parallax::kMid);
        set.back(canvas, pal, grain);
        set.mid(canvas, pal, grain);
        set.fore(canvas, pal, grain, false);
        canvas.setFill(Color{0.0f, 0.0f, 0.0f, 0.55f});
        canvas.fillRect(0, 0, maz::film::kWorldW, maz::film::kWorldH);
        maz::film::Gradient glow =
            maz::film::Canvas::radialGradient(500, 220, 10, 500, 220, 330);
        glow.addStop(0.0f, pal.key.toColor(0.30f));
        glow.addStop(1.0f, pal.key.toColor(0.0f));
        canvas.setFillGradient(glow);
        canvas.fillRect(120, 0, 760, maz::film::kWorldH);

        // The thing the story turns on, lit on the dark surface. The one shot in a film with no
        // people in it.
        canvas.save();
        canvas.concat(0.9f, 0.0f, 0.0f, 0.9f, 500.0f, 220.0f);
        maz::film::glyphFor(reel.object).draw(canvas, pal);
        canvas.restore();
    }

    // Everything between the audience and the picture, in SCREEN space with the roll undone, so none
    // of it tilts with the lens or slides with a pan. Weather reads as air in front of the lens
    // rather than part of the set, so a camera move leaving it alone is the point, not an oversight.
    {
        maz::film::Canvas air(img, static_cast<int>(frameY), static_cast<int>(frameH));
        air.setTransform(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, frameY);
        maz::film::drawWeather(air, maz::film::weatherFor(reel.genre, shot->time, shot->set), pal,
                               time, reel.seed, frameW, frameH);
        // A set that owns a moving light folds its brightness into the leak's alpha and its offset
        // into where the leak starts, rather than getting a draw call of its own -- which is what
        // keeps it out of the weather's and the vignette's way, all three being drawn right here.
        maz::film::drawLightLeak(air, pal, light.offset, light.brightness, frameW, frameH);
    }
    maz::film::drawVignette(img, pal, static_cast<int>(frameY),
                            static_cast<int>(frameY + frameH));

    drawCaptions(img, pal, *shot, frameW, frameH, frameY, progress);

    // Film stock, last of all: grain and a gate flicker, both stepping twelve times a second.
    maz::film::drawGrain(img, time, static_cast<int>(frameY),
                         static_cast<int>(frameY + frameH));

    // The letterbox bars sit over everything, so nothing can spill into them.
    for (int y = 0; y < static_cast<int>(frameY) && y < img.height(); ++y) {
        img.blendSpan(0, img.width(), y, Color{0.0f, 0.0f, 0.0f, 1.0f}, 1.0f);
    }
    for (int y = static_cast<int>(frameY + frameH); y < img.height(); ++y) {
        if (y >= 0) {
            img.blendSpan(0, img.width(), y, Color{0.0f, 0.0f, 0.0f, 1.0f}, 1.0f);
        }
    }

    // Black at the head and tail of the film, and a dip on every scene change.
    const float fade = static_cast<float>(maz::film::fadeAt(reel, *shot, time));
    if (fade > 0.0f) {
        for (int y = 0; y < img.height(); ++y) {
            img.blendSpan(0, img.width(), y, Color{0.0f, 0.0f, 0.0f, 1.0f}, fade);
        }
    }
    return img;
}

} // namespace filmreel
