#pragma once

#include "../filmreel/Frame.hpp" // the captions, the grain, the fades: one film, one set of titles
#include "Scene.hpp"

#include "maz/render/Tonemap.hpp"

#include <vector>

#include <algorithm>
#include <cmath>

// film3d — one frame of the film, photographed.
namespace film3d {

using maz::render::Color;
using maz::render::Image;
using maz::render::SoftRaster;
using maz::render::Surface;

// How the key light falls in this room at this hour. The palette already decided what colour the light
// is; this decides where it comes FROM, which is the half of lighting the flat renderer could not have.
inline Surface surfaceFor(const maz::film::Palette& pal, const Shot& shot, double seconds) {
    Surface s;
    // The palette's key is a strong, saturated colour — it is chosen to stand for a genre at a glance.
    // Used neat as a light it prints the whole frame in one hue. A real lamp is far closer to white
    // than its "colour" suggests, so the key is pulled most of the way back to white and the ambient
    // takes the room's own colour instead. That is what gives a frame a warm side and a cool side
    // rather than a single tint.
    auto toward = [](double c, float amount) {
        const float v = static_cast<float>(c / 255.0);
        return 1.0f - (1.0f - v) * amount;
    };
    s.key = Color{toward(pal.key.r, 0.55f), toward(pal.key.g, 0.55f), toward(pal.key.b, 0.55f), 1.0f};
    s.ambientTint = Color{static_cast<float>(0.68 + 0.32 * pal.sky.r / 255.0),
                          static_cast<float>(0.68 + 0.32 * pal.sky.g / 255.0),
                          static_cast<float>(0.72 + 0.28 * pal.sky.b / 255.0), 1.0f};

    // Night is a single hard source and almost no bounce; day is soft and comes from everywhere. The
    // difference between them is mostly the ambient, not the key.
    const bool night = shot.time == "NIGHT";
    const bool dusk = shot.time == "DUSK" || shot.time == "DAWN";
    // Night is one hard source and almost no bounce. The temptation is to lift the ambient until the
    // frame is comfortable to look at, and that is how a night scene turns into an overcast afternoon:
    // the ambient stays low and the EXPOSURE does the lifting, which keeps the shape of the light.
    s.ambient = night ? 0.105f : (dusk ? 0.185f : 0.300f);
    s.fill = night ? 0.075f : (dusk ? 0.130f : 0.205f);
    // A wound-up scene gets a harder, lower key from further round the side: the light follows the
    // mood, which is the one thing about lighting an audience reads without being told.
    const float mood = static_cast<float>(shot.mood);
    const float side = 0.42f + 0.36f * mood;
    const float drift = 0.04f * static_cast<float>(std::sin(seconds * 0.21));
    s.keyDirection = math::normalize(
        math::vec3(-side + drift, -(0.86f - 0.30f * mood), 0.62f - 0.22f * mood));
    s.ambient *= 1.0f - 0.25f * mood;
    return s;
}

// The air in this place: its colour, and how far you can see through it.
inline void setAir(Surface& s, const maz::film::Palette& pal, const Shot& shot, bool indoors) {
    // Air at night is not the same colour as air at noon, and the palette's sky is written for a
    // painted backdrop rather than for something the whole frame is going to be mixed toward.
    const bool night = shot.time == "NIGHT";
    const bool dusk = shot.time == "DUSK" || shot.time == "DAWN";
    const double dim = night ? 0.40 : (dusk ? 0.70 : 1.0);
    // And pulled part of the way toward its own grey. A genre colour used neat as the colour of the
    // AIR tints every distance in the frame at once, and the picture comes out printed on coloured
    // stock rather than lit in a colour.
    const double grey = (pal.sky.r * 0.299 + pal.sky.g * 0.587 + pal.sky.b * 0.114);
    auto air = [&](double c) { return static_cast<float>((c + (grey - c) * 0.34) * dim / 255.0); };
    s.fog = Color{air(pal.sky.r), air(pal.sky.g), air(pal.sky.b), 1.0f};
    if (indoors) {
        // A room has air in it too, and a little of it is what pushes a back wall back. More than a
        // little and the room fills with smoke.
        s.fogStart = 3.5f;
        s.fogEnd = 18.0f;
        s.fogMax = 0.22f;
    } else {
        s.fogStart = 3.0f;
        s.fogEnd = 34.0f;
        s.fogMax = 0.90f;   // outdoors it goes almost all the way, and takes the horizon with it
    }
    // A wound-up scene closes in: less depth of air, a nearer world.
    const float mood = static_cast<float>(shot.mood);
    s.fogEnd *= 1.0f - 0.28f * mood;
}

// How much light reaches the film, and the curve it is printed through.
//
// The first pass of this looked correct and was unwatchable: a night scene in a dark room, shaded
// honestly, is very nearly black, because a dark surface lit by a dim light IS very nearly black. Real
// photography solves it the way it has always solved it — open the aperture and print through a curve
// with a lifted toe and a shoulder — so a night scene reads as night rather than as a fault. The
// engine already has the curve (ACES, the same one its GPU tonemap pass uses); this is the aperture.
inline float exposureFor(const Shot& shot) {
    const bool night = shot.time == "NIGHT";
    const bool dusk = shot.time == "DUSK" || shot.time == "DAWN";
    const float base = night ? 1.30f : (dusk ? 1.18f : 1.05f);

    // An automatic exposure was tried here — open the aperture in proportion to how dark the set's
    // own palette is — and it was taken out again. It was written to fix a frame in the demo reel that
    // looked black, and the frame turned out to be a FADE between two chapters, which is black on
    // purpose. Applied to a night horror film, which is what most of the testing was done against, it
    // opened the picture up until it read as an overcast afternoon. The lesson is the one worth
    // keeping: check what a frame IS before deciding it is wrong.
    return base * (1.0f + 0.12f * static_cast<float>(shot.mood));
}

// The picture, printed. Exposure first, then the filmic curve, then a small lift in the blacks so the
// shadows hold a colour rather than crushing to nothing — which on a set built out of one dark palette
// is the difference between a room and a hole.
inline void printImage(Image& img, int y0, int y1, float exposure) {
    for (int y = y0; y < y1; ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const Color c = img.getPixel(x, y);
            Color out;
            out.r = maz::render::acesFilmic(c.r * exposure) * 0.955f + 0.028f;
            out.g = maz::render::acesFilmic(c.g * exposure) * 0.955f + 0.030f;
            out.b = maz::render::acesFilmic(c.b * exposure) * 0.955f + 0.034f;
            out.a = 1.0f;
            img.setPixel(x, y, out);
        }
    }
}

// Everybody's position, pose and skeleton for this instant.
inline Scene stageScene(const Reel& reel, const Shot& shot, const std::map<std::string, Cast>& cast,
                        double time, double progress) {
    Scene sc;
    sc.palette = maz::film::paletteFor(reel.genre, shot.time, shot.mood);
    sc.stage = maz::film::buildStage(shot, sc.palette, reel.seed);

    // Who is in this shot, in a settled order: left of frame first, so the same two people do not
    // swap sides between cuts. Crossing the line is the one edit rule that confuses an audience even
    // when they cannot say why.
    std::vector<const Cast*> present;
    for (const std::string& name : shot.characters) {
        const auto it = cast.find(name);
        if (it != cast.end()) {
            present.push_back(&it->second);
        }
    }
    std::stable_sort(present.begin(), present.end(),
                     [](const Cast* a, const Cast* b) { return a->side < b->side; });

    const int total = static_cast<int>(present.size());
    for (int i = 0; i < total; ++i) {
        const Cast* who = present[static_cast<std::size_t>(i)];
        Standing st;
        st.who = who;
        st.speaking = !shot.speaker.empty() && shot.speaker == who->name;

        Motive mv;
        mv.seed = hashName(who->name);
        mv.position = markFor(sc.stage, i, total);
        // They face the camera, turned a little toward whoever else is there. Squarer to the lens
        // than two people talking would really stand — which is what actors are directed to do, and
        // for the same reason: turned as far as life would turn them, the camera gets two profiles.
        const float toCentre = mv.position.x > 0.0f ? 0.30f : -0.30f;
        mv.facing = 3.14159265f + toCentre; // 0 faces +Z, away from the camera; turn them round
        mv.tension = static_cast<float>(shot.mood) * 0.7f;

        // An action shot MOVES somebody: the flat film's "action" was a caption over a still picture,
        // and a character who never walks anywhere is the clearest thing missing from it.
        const bool walker = shot.kind == "action" && i == 0 && shot.duration > 1.6;
        if (walker) {
            const float span = 1.45f;
            const float u = static_cast<float>(progress);
            const float eased = u * u * (3.0f - 2.0f * u); // start and stop, rather than snap into motion
            const float travelled = eased * span;
            mv.position.x += (who->side < 0 ? 1.0f : -1.0f) * travelled;
            mv.travelled = travelled;
            // Speed from the derivative of the ease, so the gait matches what the body is doing.
            const float dEase = 6.0f * u * (1.0f - u);
            mv.walkSpeed = dEase * span / static_cast<float>(shot.duration);
            mv.facing = who->side < 0 ? 1.5707963f : -1.5707963f;
        }

        // They look at whoever is speaking, or at the other person, or at the object.
        float lookYaw = 0.0f;
        if (total > 1) {
            const int other = i == 0 ? 1 : 0;
            const math::vec3 theirs = markFor(sc.stage, other, total);
            const math::vec3 d = theirs - mv.position;
            const float want = std::atan2(d.x, d.z);
            lookYaw = want - mv.facing;
            while (lookYaw > 3.14159265f) {
                lookYaw -= 6.28318531f;
            }
            while (lookYaw < -3.14159265f) {
                lookYaw += 6.28318531f;
            }
            lookYaw = std::fmax(-1.5f, std::fmin(1.5f, lookYaw));
        }
        // And they cheat the look too. A head turned the full way to the other person puts the face
        // in profile in every two-shot in the film; half of it reads as looking at them and still
        // shows the camera a face.
        mv.lookYaw = lookYaw * 0.45f;
        mv.lookPitch = -0.04f;

        if (st.speaking) {
            mv.speaking = 0.75f + 0.25f * static_cast<float>(shot.mood);
            // The hands move on the syllable clock the flat renderer already counts, so the gesture
            // lands on the voice in both renderers rather than on a timer of its own.
            const int syllables = maz::film::syllablesFor(shot.caption);
            const double gap = shot.duration > 0.0 && syllables > 0
                                   ? shot.duration * 0.82 / static_cast<double>(syllables)
                                   : 0.0;
            const double into = time - shot.start;
            mv.syllable = gap > 0.0 ? static_cast<float>(std::fmod(into, gap) / gap) : 0.0f;
            mv.gestureHand = (mv.seed & 1u) ? maz::film::kLeft : maz::film::kRight;
        }

        st.pose = maz::film::performAt(who->build, mv, static_cast<float>(time));
        st.skeleton = maz::film::skeletonOf(who->build, st.pose);
        sc.people.push_back(st);
    }

    // ---- the camera -------------------------------------------------------------------------------
    // With nobody in frame — a slug line, an insert, a shot of a room — the camera still has to be
    // aimed at something, and the something is the acting area. Aimed at the origin instead, as it
    // was at first, it looks at the floor three metres in front of the set and photographs a wedge of
    // nothing, which is what most of the first contact sheet's empty panels were.
    Subject a;
    Subject b;
    a.stand = math::vec3((sc.stage.markLeft.x + sc.stage.markRight.x) * 0.5f, 0.0f,
                         sc.stage.markLeft.z);
    b.stand = a.stand;
    if (!sc.people.empty()) {
        // The subject of the shot is whoever is speaking; if nobody is, it is whoever is on the side
        // the reel says the shot is on.
        std::size_t lead = 0;
        for (std::size_t i = 0; i < sc.people.size(); ++i) {
            if (sc.people[i].speaking) {
                lead = i;
            }
        }
        const Standing& s0 = sc.people[lead];
        a.stand = s0.pose.position;
        a.eyeY = s0.skeleton.eyes(s0.who->build).y;
        a.chestY = maz::film::Skeleton::at(s0.skeleton.chest).y;
        const std::size_t other = sc.people.size() > 1 ? (lead == 0 ? 1u : 0u) : lead;
        const Standing& s1 = sc.people[other];
        b.stand = s1.pose.position;
        b.eyeY = s1.skeleton.eyes(s1.who->build).y;
        b.chestY = maz::film::Skeleton::at(s1.skeleton.chest).y;
    }
    sc.lens = maz::film::lensFor(shot, sc.stage, a, b, sc.people.size() > 1,
                                 static_cast<float>(progress), static_cast<float>(time));
    return sc;
}

// One frame: the room, the people, then the film's own titles over the top.
inline Image drawFrame3D(const Reel& reel, const std::map<std::string, Cast>& cast, double time,
                         int width, int height, int supersample) {
    Image img(width, height, Color{0.0f, 0.0f, 0.0f, 1.0f});
    const Shot* shot = maz::film::shotAt(reel, time);
    if (shot == nullptr || width < 8 || height < 8) {
        return img;
    }
    const float frameW = static_cast<float>(width);
    const float frameH = std::fmin(static_cast<float>(height), frameW / maz::film::kAspect);
    const float frameY = (static_cast<float>(height) - frameH) / 2.0f;
    const double progress = shot->duration > 0.0 ? (time - shot->start) / shot->duration : 0.0;

    const Scene sc = stageScene(reel, *shot, cast, time, progress);
    // Built once and used twice: the shadow pass and the picture see the same bodies, which they must,
    // and a body is a few thousand triangles to assemble.
    std::vector<maz::render::shapes::MeshData> bodies;
    bodies.reserve(sc.people.size());
    for (const Standing& who : sc.people) {
        bodies.push_back(maz::film::buildBody(who.who->build, who.skeleton));
    }

    Surface surf = surfaceFor(sc.palette, *shot, time);
    setAir(surf, sc.palette, *shot, sc.stage.indoors);

    // ---- what the key light cannot see ------------------------------------------------------------
    //
    // The map is fitted to the ACTING AREA rather than to the whole set. A corridor is twelve metres
    // long and a chapel six and a half tall, and a map stretched over all of that spends its
    // resolution on the far end of a room nobody is standing in, leaving the shadow under a foot —
    // the one everybody actually looks at — four texels wide.
    maz::render::ShadowMap shadows(1024);
    {
        const float reach = std::fmin(sc.stage.halfWidth, 4.5f);
        const float ceiling = sc.stage.ceiling > 0.0f ? std::fmin(sc.stage.ceiling, 3.2f) : 2.6f;
        const math::vec3 lo(-reach, -0.15f, sc.stage.markLeft.z - 2.6f);
        const math::vec3 hi(reach, ceiling, sc.stage.markLeft.z + 3.4f);
        shadows.begin(maz::render::directionalLight(lo, hi, surf.keyDirection));
        shadows.add(sc.stage.props);
        for (const maz::render::shapes::MeshData& body : bodies) {
            shadows.add(body);
        }
        surf.shadows = &shadows;
        // Not all of it. A shadow in a film is never black — there is always bounce finding its way
        // in — and taking the whole key away turns a figure's own shadow side into a hole.
        surf.shadowStrength = 0.84f;
    }

    // The picture is rendered inside the letterboxed window only, and at a multiple of its size so the
    // edges can be averaged down. There is no anti-aliasing in the rasteriser on purpose: a frame
    // rendered twice over and averaged is simpler, is exactly right, and costs only what it costs.
    const int S = supersample < 1 ? 1 : (supersample > 3 ? 3 : supersample);
    const int pw = static_cast<int>(frameW) * S;
    const int ph = static_cast<int>(frameH) * S;
    Image big(pw, ph, Color{static_cast<float>(sc.palette.sky.r / 255.0),
                            static_cast<float>(sc.palette.sky.g / 255.0),
                            static_cast<float>(sc.palette.sky.b / 255.0), 1.0f});
    SoftRaster raster(pw, ph);
    raster.clearDepth();

    const math::mat4 view = glm::lookAt(sc.lens.eye, sc.lens.at, math::vec3(0.0f, 1.0f, 0.0f));
    const math::mat4 vp =
        math::perspective(sc.lens.fovY, frameW / frameH, 0.04f, 220.0f) * view;

    raster.draw(big, sc.stage.mesh, math::mat4(1.0f), vp, surf);
    for (const maz::render::shapes::MeshData& body : bodies) {
        raster.draw(big, body, math::mat4(1.0f), vp, surf);
    }

    // Down into the frame, averaging each block of S x S.
    const int x0 = 0;
    const int y0 = static_cast<int>(frameY);
    const float inv = 1.0f / static_cast<float>(S * S);
    for (int y = 0; y < static_cast<int>(frameH); ++y) {
        for (int x = 0; x < static_cast<int>(frameW); ++x) {
            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;
            for (int j = 0; j < S; ++j) {
                for (int i = 0; i < S; ++i) {
                    const Color c = big.getPixel(x * S + i, y * S + j);
                    r += c.r;
                    g += c.g;
                    b += c.b;
                }
            }
            img.setPixel(x0 + x, y0 + y, Color{r * inv, g * inv, b * inv, 1.0f});
        }
    }

    printImage(img, y0, y0 + static_cast<int>(frameH), exposureFor(*shot));

    // The film's own titles, grain, letterbox and fades — the same ones the flat renderer draws,
    // because the captions belong to the film and not to the renderer.
    filmreel::drawCaptions(img, sc.palette, *shot, frameW, frameH, frameY, progress);
    maz::film::drawGrain(img, time, static_cast<int>(frameY), static_cast<int>(frameY + frameH));
    for (int y = 0; y < static_cast<int>(frameY) && y < img.height(); ++y) {
        img.blendSpan(0, img.width(), y, Color{0.0f, 0.0f, 0.0f, 1.0f}, 1.0f);
    }
    for (int y = static_cast<int>(frameY + frameH); y < img.height(); ++y) {
        if (y >= 0) {
            img.blendSpan(0, img.width(), y, Color{0.0f, 0.0f, 0.0f, 1.0f}, 1.0f);
        }
    }
    const float fade = static_cast<float>(maz::film::fadeAt(reel, *shot, time));
    if (fade > 0.0f) {
        for (int y = 0; y < img.height(); ++y) {
            img.blendSpan(0, img.width(), y, Color{0.0f, 0.0f, 0.0f, 1.0f}, fade);
        }
    }
    return img;
}

// The cast, once, for the whole film.
inline std::map<std::string, Cast> castReel(const Reel& reel) {
    std::map<std::string, Cast> out;
    for (const maz::film::Character& who : reel.characters) {
        const auto v = reel.voices.find(who.name);
        out[who.name] = castFor(who, v == reel.voices.end() ? nullptr : &v->second);
    }
    return out;
}

} // namespace film3d
