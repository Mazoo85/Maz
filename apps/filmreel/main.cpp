// filmreel — draws a SCRIPT FORGE film reel natively, with no browser.
//
// The browser records a film by playing it: canvas.captureStream(30) into a MediaRecorder. That is a
// REALTIME capture, so a three-minute film takes three minutes, a frame the browser misses is missing
// from the file, and the container has to be patched afterwards to carry a duration the recorder did
// not know when it wrote its header. This draws frame n at time n/fps and writes it. It cannot drop a
// frame, it knows the duration before it starts, and it runs as fast as the machine allows.
//
//   filmreel --reel film.reel.json --out frames/ --fps 24 --width 1280
//   filmreel --reel film.reel.json --contact sheet.qoi          (one sheet of the whole film)
//
// HONEST SCOPE. The people, the light, the timing and the cutting are ported faithfully and checked
// against the browser: the figures match its geometry to 1.9e-05 of a pixel, the palette matches every
// channel of 241 cases, the camera matches 200 cases to machine epsilon, and the reel is cut at
// exactly the same instants. The SCENERY is not: film-sets.js is 25KB of per-set artwork and porting
// it is a sub-project of its own, so this draws a palette-correct backdrop -- sky, horizon, floor,
// light wash and a plain silhouette skyline -- which is right in colour and composition and plain in
// detail. Sound is still the browser's job. Output is a lossless frame sequence, not a video file:
// the engine has an IVF demuxer, not a codec.
#include "maz/film/Camera.hpp"
#include "maz/film/Figure.hpp"
#include "maz/film/Palette.hpp"
#include "maz/film/Reel.hpp"
#include "maz/render/ImageCodecPnm.hpp"
#include "maz/render/ImageCodecQoi.hpp"
#include "maz/render/PathFill.hpp"
#include "maz/render/StrokeFont.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

using maz::render::Color;
using maz::render::Image;
using maz::render::Path;

// ------------------------------------------------------------------------------------- painting

// Alpha-composite a flat colour over one pixel.
void blend(Image& img, int x, int y, const Color& c, float a) {
    if (a <= 0.0f) {
        return;
    }
    maz::render::detail::blendCoverage(img, x, y, c, a);
}

// A vertical gradient across a band of rows, composited rather than replacing, so it can be laid over
// what is already there.
void gradientBand(Image& img, int y0, int y1, const Color& top, const Color& bottom, float alpha) {
    if (y1 <= y0) {
        return;
    }
    for (int y = y0; y < y1; ++y) {
        const float t = static_cast<float>(y - y0) / static_cast<float>(y1 - y0);
        const Color c{top.r + (bottom.r - top.r) * t, top.g + (bottom.g - top.g) * t,
                      top.b + (bottom.b - top.b) * t, 1.0f};
        for (int x = 0; x < img.width(); ++x) {
            blend(img, x, y, c, alpha);
        }
    }
}

// A soft radial wash -- the room's light falling on the back wall.
void glow(Image& img, float cx, float cy, float radius, const Color& c, float strength) {
    const int x0 = std::max(0, static_cast<int>(cx - radius));
    const int x1 = std::min(img.width(), static_cast<int>(cx + radius) + 1);
    const int y0 = std::max(0, static_cast<int>(cy - radius));
    const int y1 = std::min(img.height(), static_cast<int>(cy + radius) + 1);
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const float dx = (static_cast<float>(x) - cx) / radius;
            const float dy = (static_cast<float>(y) - cy) / radius;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d >= 1.0f) {
                continue;
            }
            const float falloff = (1.0f - d) * (1.0f - d);
            blend(img, x, y, c, falloff * strength);
        }
    }
}

void veil(Image& img, const Color& c, float alpha) {
    if (alpha <= 0.0f) {
        return;
    }
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            blend(img, x, y, c, alpha);
        }
    }
}

// ------------------------------------------------------------------------------------- the frame

// A stable number from a name, so the same set is lit and shaped the same way every time.
std::uint32_t hashName(const std::string& s) {
    std::uint32_t h = 2166136261u;
    for (const char c : s) {
        h ^= static_cast<std::uint32_t>(static_cast<unsigned char>(c));
        h *= 16777619u;
    }
    return h;
}

// Where the light is, as a signed position across the frame: -1 hard left, +1 hard right.
//
// A stand-in. film-sets.js carries a LIGHT BEHAVIOUR per set -- a lighthouse beam that sweeps, a car's
// headlights that pass, a flicker, a moving cloud -- and its offset genuinely moves during a shot.
// Porting that belongs with the sets. This is steady and merely per-set, which is enough to put the
// rim light and the contact shadow on the right side of a character and no more than that.
float lightXFor(const std::string& set, const std::string& hour) {
    if (hour == "DAY") {
        return -0.25f; // daylight comes from high and a little to one side
    }
    const std::uint32_t h = hashName(set);
    return -0.85f + static_cast<float>(h % 1000u) / 1000.0f * 1.7f;
}

// Which of the buildable sets are actually outdoors. Everything else is a room, and a room has a
// wall behind it rather than a horizon.
bool outdoors(const std::string& set) {
    return set == "woods" || set == "street" || set == "field" || set == "water";
}

// The camera as a 2x3 affine over screen-space world coordinates.
void applyCamera(Path& path, const maz::film::CameraShot& cam, float w, float h) {
    const float cx = w * 0.5f;
    const float cy = h * 0.5f;
    const float z = static_cast<float>(cam.zoom);
    const float cr = std::cos(static_cast<float>(cam.roll)) * z;
    const float sr = std::sin(static_cast<float>(cam.roll)) * z;
    // Positive panX moves the CAMERA right, so the world moves left.
    const float ox = -static_cast<float>(cam.panX) * w;
    const float oy = -static_cast<float>(cam.panY + cam.panYMove) * h;
    path.transform(cr, sr, -sr, cr, cx - cr * cx + sr * cy + ox, cy - sr * cx - cr * cy + oy);
}

struct Spot {
    float x = 0.0f;      // centre, as a fraction of frame width
    float ground = 0.0f; // feet, as a fraction of frame height
    float height = 0.0f; // as a fraction of frame height
    std::string name;
    std::string fixedPose; // set when the framing needs this figure held still, not acting
};

// Where the figures stand. The browser's own layout is entangled with its three parallax planes and a
// 1000x420 world that the sets define; with the sets deferred, this is a native screen-space layout in
// the same spirit -- a single figure off centre, two figures apart and slightly different sizes so the
// frame does not read as a diagram, a close-up cut off at the chest.
std::vector<Spot> layoutFor(const maz::film::Shot& shot) {
    std::vector<Spot> out;
    if (shot.characters.empty()) {
        return out;
    }
    const std::string& first = shot.characters.front();
    if (shot.framing == "close") {
        out.push_back({0.52f, 1.30f, 0.92f, shot.speaker.empty() ? first : shot.speaker, ""});
        return out;
    }
    if (shot.framing == "ots") {
        const std::string speaker = shot.speaker.empty() ? first : shot.speaker;
        std::string other = speaker;
        for (const std::string& n : shot.characters) {
            if (n != speaker) {
                other = n;
            }
        }
        out.push_back({0.61f, 0.90f, 0.50f, speaker, ""});
        // The listener is a shoulder at the edge of frame, not a second performance. The first
        // version let them take the beat's pose like anyone else, and at this size and zoom a `reach`
        // -- one arm straight out -- filled a quarter of the frame with a black slab. Held still on
        // purpose, and smaller than the browser's, which can afford a bigger one because it has a
        // real set behind it to read against.
        Spot shoulder{0.17f, 1.08f, 0.72f, other, ""};
        shoulder.fixedPose = "turn-away";
        out.push_back(shoulder);
        return out;
    }
    if (shot.framing == "low") {
        out.push_back({0.50f, 0.99f, 0.66f, shot.speaker.empty() ? first : shot.speaker, ""});
        return out;
    }
    const float tall = shot.framing == "mid" ? 0.56f : 0.46f;
    if (shot.characters.size() == 1) {
        out.push_back({0.57f, 0.85f, tall, first, ""});
        return out;
    }
    out.push_back({0.36f, 0.85f, tall, first, ""});
    out.push_back({0.66f, 0.84f, tall * 0.96f, shot.characters[1], ""});
    return out;
}

void drawFigure(Image& img, const maz::film::Palette& pal, const maz::film::Voice* voice,
                const Spot& spot, const maz::film::Pose& pose, bool speaking, float lightX,
                const maz::film::CameraShot& cam, float w, float h) {
    const double height = static_cast<double>(spot.height * h);
    const float gx = spot.x * w;
    const float gy = spot.ground * h;
    const float bodyW = static_cast<float>(height) * 0.34f;

    // The shadow falls AWAY from the light and stretches as the light drops toward the horizon, which
    // is what makes a floor read as a floor.
    {
        Path shadow;
        shadow.ellipse(gx - lightX * bodyW * 0.42f, gy + 2.0f,
                       bodyW * (0.75f + std::fabs(lightX) * 0.35f),
                       static_cast<float>(height) * 0.035f);
        applyCamera(shadow, cam, w, h);
        maz::render::fillPath(img, shadow, Color{0.0f, 0.0f, 0.0f, 0.45f});
    }

    const Color key = pal.key.toColor();
    const float rim = speaking ? 0.75f : 0.42f;

    // The same body three times: a rim of the room's light on the side the light is on, the body
    // itself near-black, and a wash of the character's own colour over the top.
    Path body = maz::film::bodyPath(height, pose);
    {
        Path lit = body;
        lit.translate(gx + lightX * bodyW * 0.055f, gy - static_cast<float>(height) * 0.012f);
        applyCamera(lit, cam, w, h);
        maz::render::fillPath(img, lit, Color{key.r, key.g, key.b, rim});
    }
    body.translate(gx, gy);
    applyCamera(body, cam, w, h);
    maz::render::fillPath(img, body, Color{0.024f, 0.024f, 0.04f, 0.97f});

    if (voice != nullptr) {
        // The character's own hue, so two people in one frame are never confused.
        const float hue = voice->hue / 60.0f;
        const int sector = static_cast<int>(hue) % 6;
        const float f = hue - std::floor(hue);
        const float v = 0.58f, s = 0.65f;
        const float p = v * (1.0f - s), q = v * (1.0f - s * f), t = v * (1.0f - s * (1.0f - f));
        Color tint{v, t, p, 1.0f};
        if (sector == 1) tint = {q, v, p, 1.0f};
        else if (sector == 2) tint = {p, v, t, 1.0f};
        else if (sector == 3) tint = {p, q, v, 1.0f};
        else if (sector == 4) tint = {t, p, v, 1.0f};
        else if (sector == 5) tint = {v, p, q, 1.0f};
        tint.a = speaking ? 0.16f : 0.08f;
        maz::render::fillPath(img, body, tint);
    }
}

// The parts of the backdrop that do not move: the sky, the wall, the light in the room, the floor and
// the vignette. All full-frame pixel work, and all identical for every frame of a shot -- so it is
// built ONCE per shot and copied, rather than recomputed for every one of the hundred-odd frames a
// shot lasts. Recomputing it was most of why the first version rendered at a third of realtime.
Image buildWash(const maz::film::Palette& pal, const maz::film::Shot& shot, float lightX, int width,
                int height) {
    Image img(width, height, Color{0.0f, 0.0f, 0.0f, 1.0f});
    const float w = static_cast<float>(width);
    const float h = static_cast<float>(height);
    const int horizon = static_cast<int>(h * 0.72f);
    const Color key = pal.key.toColor();
    const Color ink = pal.ink.toColor();

    gradientBand(img, 0, horizon, pal.sky.toColor(), pal.deep.toColor(), 1.0f);
    gradientBand(img, horizon, height, pal.deep.toColor(), ink, 1.0f);

    if (outdoors(shot.set)) {
        // Outside, the light is the sky itself: a wide, high wash and nothing between it and us.
        glow(img, w * (0.5f + lightX * 0.42f), h * 0.30f, h * 0.70f, key,
             0.07f + static_cast<float>(pal.lift) * 0.13f);
    } else {
        // A room: a flat far wall, unevenly lit -- brighter near the window, falling off away from it.
        Path wall;
        wall.rect(-0.05f * w, h * 0.06f, w * 1.1f, static_cast<float>(horizon) - h * 0.06f);
        maz::render::fillPath(img, wall, Color{ink.r * 0.62f, ink.g * 0.60f, ink.b * 0.70f, 0.96f});
        glow(img, w * (0.5f + lightX * 0.34f), h * 0.34f, h * 0.60f, key, 0.10f);
        glow(img, w * (0.5f + lightX * 0.34f), static_cast<float>(horizon) + h * 0.03f, h * 0.28f, key,
             0.09f);
    }

    // The floor, darker than the wall, so the room has a floor rather than a seam.
    for (int y = horizon; y < height; ++y) {
        const float t = static_cast<float>(y - horizon) / static_cast<float>(height - horizon);
        for (int x = 0; x < width; ++x) {
            blend(img, x, y, Color{0.0f, 0.0f, 0.0f, 1.0f}, 0.30f + t * 0.34f);
        }
    }

    // A vignette. Without it every frame reads as one even wash, whatever the palette is doing.
    const float cx = w * 0.5f, cy = h * 0.52f;
    const float maxR = std::sqrt(cx * cx + cy * cy);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float dx = (static_cast<float>(x) - cx) / maxR;
            const float dy = (static_cast<float>(y) - cy) / maxR;
            const float d = std::sqrt(dx * dx + dy * dy);
            const float amount = d < 0.45f ? 0.0f : (d - 0.45f) / 0.55f;
            blend(img, x, y, Color{0.0f, 0.0f, 0.0f, 1.0f}, amount * amount * 0.72f);
        }
    }
    return img;
}

// The parts of the backdrop that DO move with the camera, and are cheap because they are a handful of
// small shapes rather than a million pixels: what is on the far wall, or on the horizon.
void drawSetPieces(Image& img, const maz::film::Palette& pal, const maz::film::Shot& shot,
                   float lightX, const maz::film::CameraShot& cam, float w, float h) {
    const int horizon = static_cast<int>(h * 0.72f);
    const Color key = pal.key.toColor();
    const Color ink = pal.ink.toColor();

    // Stable per set, so the same room is the same room every time it is cut back to.
    std::uint32_t r = hashName(shot.set + shot.time);
    auto next = [&r]() {
        r = r * 1664525u + 1013904223u;
        return static_cast<float>((r >> 8) % 1000u) / 1000.0f;
    };

    if (outdoors(shot.set)) {
        // A low, ragged horizon: trees, roofs, rocks -- read as distance, not as a downtown. The
        // first version gave EVERY set a city skyline, which put one behind a lighthouse lamp room, a
        // moving car and a hallway: invisible in the code, obvious the moment a sheet was on screen.
        Path back;
        float x = -0.08f * w;
        while (x < w * 1.08f) {
            const float bw = w * (0.06f + next() * 0.13f);
            const float bh = h * (0.03f + next() * 0.09f);
            back.rect(x, static_cast<float>(horizon) - bh, bw, bh + h * 0.05f);
            x += bw * (0.86f + next() * 0.3f);
        }
        applyCamera(back, cam, w, h);
        maz::render::fillPath(img, back, Color{ink.r * 0.55f, ink.g * 0.55f, ink.b * 0.62f, 0.94f});
    } else {
        // A window: small, high, off to the light's side, and divided. The first version cut a hole in
        // the wall and let the sky through, which put a big pale rectangle in the middle of every
        // interior and read as a projector screen.
        const float ox = w * (0.5f + lightX * 0.34f);
        const float ow = w * (0.045f + next() * 0.025f);
        const float oy = h * (0.14f + next() * 0.08f);
        const float oh = h * (0.16f + next() * 0.10f);
        Path window;
        window.rect(ox - ow, oy, ow * 2.0f, oh);
        applyCamera(window, cam, w, h);
        maz::render::fillPath(img, window,
                              Color{key.r, key.g, key.b, 0.16f + static_cast<float>(pal.lift) * 0.30f});

        Path bars;
        bars.rect(ox - ow * 0.06f, oy, ow * 0.12f, oh);
        bars.rect(ox - ow, oy + oh * 0.46f, ow * 2.0f, oh * 0.07f);
        applyCamera(bars, cam, w, h);
        maz::render::fillPath(img, bars, Color{ink.r * 0.5f, ink.g * 0.5f, ink.b * 0.58f, 0.92f});
    }

    Path floorLine;
    floorLine.rect(-0.1f * w, static_cast<float>(horizon), w * 1.2f, std::fmax(1.0f, h * 0.003f));
    applyCamera(floorLine, cam, w, h);
    maz::render::fillPath(img, floorLine, Color{key.r, key.g, key.b, 0.10f});
}

// Break a caption into lines that fit the frame.
std::vector<std::string> wrapLines(const std::string& text, float maxWidth,
                                   const maz::render::TextStyle& style) {
    std::vector<std::string> lines;
    std::string line;
    std::string word;
    auto flushWord = [&]() {
        if (word.empty()) {
            return;
        }
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (maz::render::textWidth(candidate, style) > maxWidth && !line.empty()) {
            lines.push_back(line);
            line = word;
        } else {
            line = candidate;
        }
        word.clear();
    };
    for (const char c : text) {
        if (c == ' ' || c == '\n') {
            flushWord();
        } else {
            word.push_back(c);
        }
    }
    flushWord();
    if (!line.empty()) {
        lines.push_back(line);
    }
    return lines;
}

void drawCaptions(Image& img, const maz::film::Palette& pal, const maz::film::Shot& shot, float w,
                  float h) {
    if (shot.caption.empty()) {
        return;
    }
    const Color key = pal.key.toColor();

    if (shot.kind == "title" || shot.kind == "end") {
        maz::render::TextStyle big{h * 0.085f, 0.075f, 0.06f};
        const auto lines = wrapLines(shot.caption, w * 0.84f, big);
        float y = h * 0.47f - static_cast<float>(lines.size() - 1) * big.size * 0.7f;
        for (const std::string& line : lines) {
            maz::render::fillPath(img, maz::render::textPathCentred(line, w * 0.5f, y, big),
                                  Color{key.r, key.g, key.b, 0.95f});
            y += big.size * 1.4f;
        }
        if (!shot.subcaption.empty()) {
            maz::render::TextStyle small{h * 0.032f, 0.10f, 0.09f};
            maz::render::fillPath(img,
                                  maz::render::textPathCentred(shot.subcaption, w * 0.5f,
                                                               y + h * 0.035f, small),
                                  Color{key.r, key.g, key.b, 0.55f});
        }
        return;
    }

    // A slug line is the scene's own heading; dialogue is what somebody said. They are set apart so a
    // viewer never has to wonder which they are reading.
    const bool slug = shot.kind == "establish";
    maz::render::TextStyle style{h * (slug ? 0.036f : 0.045f), slug ? 0.10f : 0.09f, slug ? 0.10f : 0.05f};
    const auto lines = wrapLines(shot.caption, w * 0.80f, style);
    float y = h * (slug ? 0.13f : 0.88f) - static_cast<float>(lines.size() - 1) * style.size * 1.35f;

    if (!slug) {
        // A band behind the words, so dialogue stays readable over a bright set.
        const float top = y - style.size * 1.15f - h * 0.02f;
        const float bottom = y + style.size * 0.5f + h * 0.02f;
        for (int py = static_cast<int>(top); py < static_cast<int>(bottom); ++py) {
            for (int px = 0; px < img.width(); ++px) {
                blend(img, px, py, Color{0.0f, 0.0f, 0.0f, 1.0f}, 0.38f);
            }
        }
        if (!shot.speaker.empty()) {
            maz::render::TextStyle who{h * 0.028f, 0.11f, 0.12f};
            maz::render::fillPath(img,
                                  maz::render::textPathCentred(shot.speaker, w * 0.5f,
                                                               top - h * 0.012f, who),
                                  Color{key.r, key.g, key.b, 0.6f});
        }
    }

    for (const std::string& line : lines) {
        maz::render::fillPath(img, maz::render::textPathCentred(line, w * 0.5f, y, style),
                              Color{slug ? key.r : 0.96f, slug ? key.g : 0.95f, slug ? key.b : 0.92f,
                                    slug ? 0.72f : 0.97f});
        y += style.size * 1.35f;
    }
}

// One frame of the film at `time`, drawn from the reel and nothing else.
Image drawFrame(const maz::film::Reel& reel, double time, int width, int height) {
    const maz::film::Shot* shot = maz::film::shotAt(reel, time);
    if (shot == nullptr) {
        return Image(width, height, Color{0.0f, 0.0f, 0.0f, 1.0f});
    }
    const float w = static_cast<float>(width);
    const float h = static_cast<float>(height);

    const double progress = shot->duration > 0.0 ? (time - shot->start) / shot->duration : 0.0;
    const maz::film::CameraShot cam = maz::film::cameraFor(*shot, progress, time);
    const maz::film::Palette pal = maz::film::paletteFor(reel.genre, shot->time, shot->mood);
    const float lightX = lightXFor(shot->set, shot->time);

    // Frames are drawn in order, so remembering the last shot's wash is the whole cache that is
    // needed: one entry, hit for every frame of a shot after its first.
    static int cachedShot = -1;
    static int cachedW = 0;
    static int cachedH = 0;
    static Image cachedWash;
    if (cachedShot != shot->index || cachedW != width || cachedH != height) {
        cachedWash = buildWash(pal, *shot, lightX, width, height);
        cachedShot = shot->index;
        cachedW = width;
        cachedH = height;
    }
    Image img = cachedWash;

    drawSetPieces(img, pal, *shot, lightX, cam, w, h);

    // Which pose a beat puts a body in. The browser picks from POSES_BY_BEAT with the shot's own seed;
    // this picks the same way from the same table so a beat still reads on the body.
    static const char* kByBeat[][3] = {
        {"stand", "hands-in-pockets", "sit"},      // open
        {"turn-away", "reach", "stand"},           // spark
        {"walk", "point", "reach"},                // push
        {"stand", "turn-away", "hands-in-pockets"},// turn
        {"recoil", "slump", "head-in-hands"},      // crisis
        {"stand", "point", "reach"},               // choice
        {"stand", "hands-in-pockets", "sit"},      // after
    };
    static const char* kBeats[] = {"open", "spark", "push", "turn", "crisis", "choice", "after"};

    for (const Spot& spot : layoutFor(*shot)) {
        int beat = 0;
        for (int i = 0; i < 7; ++i) {
            if (shot->beat == kBeats[i]) {
                beat = i;
            }
        }
        const std::uint32_t pick = hashName(spot.name + shot->beat) + static_cast<std::uint32_t>(reel.seed);
        const maz::film::Pose* pose = spot.fixedPose.empty()
                                          ? maz::film::poseNamed(kByBeat[beat][pick % 3u])
                                          : maz::film::poseNamed(spot.fixedPose);
        const bool speaking = !shot->speaker.empty() && spot.name == shot->speaker;
        drawFigure(img, pal, maz::film::voiceFor(reel, spot.name), spot,
                   pose == nullptr ? maz::film::Pose{} : *pose, speaking, lightX, cam, w, h);
    }

    drawCaptions(img, pal, *shot, w, h);
    veil(img, Color{0.0f, 0.0f, 0.0f, 1.0f}, static_cast<float>(maz::film::fadeAt(reel, *shot, time)));
    return img;
}

bool writeBytes(const std::string& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(f);
}

std::string argAfter(int argc, char** argv, const std::string& flag, const std::string& fallback) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (flag == argv[i]) {
            return argv[i + 1];
        }
    }
    return fallback;
}

} // namespace

int main(int argc, char** argv) {
    const std::string reelPath = argAfter(argc, argv, "--reel", "");
    if (reelPath.empty()) {
        std::printf(
            "filmreel — draw a SCRIPT FORGE reel natively\n\n"
            "  --reel <file.reel.json>   the film to draw (required)\n"
            "  --out <dir>               write a frame sequence here (default: filmreel-out)\n"
            "  --contact <file>          instead, write one contact sheet of the whole film\n"
            "  --fps <n>                 frames per second (default 24)\n"
            "  --width <px>              frame width; height follows at 16:9 (default 1280)\n"
            "  --from <s> --to <s>       render only this stretch of the film\n"
            "  --frames <n>              stop after this many frames\n"
            "  --format qoi|ppm          frame format (default qoi, lossless)\n");
        return 2;
    }

    const maz::film::Reel reel = maz::film::loadReel(reelPath);
    if (!reel.valid) {
        std::printf("filmreel: %s\n", reel.error.c_str());
        return 1;
    }

    const int fps = std::atoi(argAfter(argc, argv, "--fps", "24").c_str());
    const int width = std::atoi(argAfter(argc, argv, "--width", "1280").c_str());
    const int height = width * 9 / 16;
    const std::string format = argAfter(argc, argv, "--format", "qoi");
    const std::string contact = argAfter(argc, argv, "--contact", "");
    if (fps < 1 || width < 64) {
        std::printf("filmreel: --fps must be at least 1 and --width at least 64\n");
        return 2;
    }

    std::printf("filmreel: \"%s\" — %s, %zu shots, %s at %dx%d\n", reel.title.c_str(),
                reel.genreLabel.c_str(), reel.shots.size(), maz::film::clock(reel.duration).c_str(),
                width, height);

    const auto began = std::chrono::steady_clock::now();

    // --- one sheet of the whole film, for looking at it ---
    if (!contact.empty()) {
        const int cols = 5;
        const int cellW = width / cols;
        const int cellH = cellW * 9 / 16;
        const int rows = (static_cast<int>(reel.shots.size()) + cols - 1) / cols;
        Image sheet(cellW * cols, cellH * rows, Color{0.0f, 0.0f, 0.0f, 1.0f});
        for (std::size_t i = 0; i < reel.shots.size(); ++i) {
            const maz::film::Shot& s = reel.shots[i];
            const Image cell = drawFrame(reel, s.start + s.duration * 0.5, cellW, cellH);
            const int ox = static_cast<int>(i) % cols * cellW;
            const int oy = static_cast<int>(i) / cols * cellH;
            for (int y = 0; y < cellH; ++y) {
                for (int x = 0; x < cellW; ++x) {
                    sheet.setPixel(ox + x, oy + y, cell.getPixel(x, y));
                }
            }
        }
        const bool ppm = contact.size() > 4 && contact.substr(contact.size() - 4) == ".ppm";
        const bool ok = writeBytes(contact, ppm ? maz::render::encodePnmP6(sheet)
                                                : maz::render::encodeQoi(sheet));
        std::printf("filmreel: %s %s (%d shots)\n", ok ? "wrote" : "FAILED to write", contact.c_str(),
                    static_cast<int>(reel.shots.size()));
        return ok ? 0 : 1;
    }

    // --- the frame sequence ---
    const std::string outDir = argAfter(argc, argv, "--out", "filmreel-out");
    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);
    if (ec) {
        std::printf("filmreel: could not make the output directory \"%s\": %s\n", outDir.c_str(),
                    ec.message().c_str());
        return 1;
    }

    const double from = std::atof(argAfter(argc, argv, "--from", "0").c_str());
    const double to = std::atof(argAfter(argc, argv, "--to", "0").c_str());
    const double endAt = to > from ? std::fmin(to, reel.duration) : reel.duration;
    const int cap = std::atoi(argAfter(argc, argv, "--frames", "0").c_str());

    const int firstFrame = static_cast<int>(from * fps);
    int total = static_cast<int>(std::ceil(endAt * fps)) - firstFrame;
    if (cap > 0 && cap < total) {
        total = cap;
    }
    if (total < 1) {
        std::printf("filmreel: nothing to render in that stretch\n");
        return 2;
    }

    for (int i = 0; i < total; ++i) {
        const double t = static_cast<double>(firstFrame + i) / static_cast<double>(fps);
        const Image frame = drawFrame(reel, t, width, height);
        char name[64];
        std::snprintf(name, sizeof(name), "/frame-%06d.%s", firstFrame + i, format.c_str());
        if (!writeBytes(outDir + name, format == "ppm" ? maz::render::encodePnmP6(frame)
                                                       : maz::render::encodeQoi(frame))) {
            std::printf("filmreel: could not write %s%s\n", outDir.c_str(), name);
            return 1;
        }
        if ((i + 1) % 25 == 0 || i + 1 == total) {
            std::printf("\r  %d / %d frames", i + 1, total);
            std::fflush(stdout);
        }
    }

    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
    const double filmSeconds = static_cast<double>(total) / static_cast<double>(fps);
    std::printf("\nfilmreel: %d frames of %s film in %.1fs — %.1fx realtime, %.0f frames/second\n",
                total, maz::film::clock(filmSeconds).c_str(), seconds,
                seconds > 0.0 ? filmSeconds / seconds : 0.0,
                seconds > 0.0 ? static_cast<double>(total) / seconds : 0.0);
    return 0;
}
