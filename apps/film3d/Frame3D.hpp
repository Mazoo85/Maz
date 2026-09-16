#pragma once

#include "../filmreel/Frame.hpp" // the captions, the grain, the fades: one film, one set of titles
#include "Scene.hpp"

#include "maz/film/AirVolume.hpp"
#include "maz/render/DepthOfField.hpp"
#include "maz/render/MotionBlur.hpp"
#include "maz/film/Expression.hpp"

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

// How hard the renderer is allowed to work on this frame.
//
// One film, two jobs. Playing it back on a phone has to keep up with twelve frames a second; writing
// it out to a file can take as long as it likes. Rather than two renderers, one dial.
struct Look {
    // Whether the picture goes through a lens at the end, or comes off the rasteriser as a pinhole.
    // It is a separate knob from the supersample because it costs a fixed amount per frame rather
    // than a multiple of everything.
    int lens = 1;
    // How far the shutter opens, in hundredths of a frame interval. 50 is the 180-degree shutter a
    // film camera has had for a century and is what this wants; 0 is a stills camera, and a stills
    // camera at twelve frames a second is a slideshow. A separate knob from the lens because it
    // costs a different amount and only on the frames where the camera is actually moving.
    int shutter = 50;
    int supersample = 2;   // 1 plays, 2 is for keeps: the frame is drawn twice over and averaged
    int shadows = 2;       // 0 none, 1 a hard edge, 2 a soft one
};

// What does not change from one frame of a shot to the next.
//
// The room is rebuilt from scratch every frame, and a room is the most expensive thing in the scene:
// walls, furniture, fifteen sets' worth of dressing, several hundred triangles of it. Nothing about it
// changes while the camera is on one shot — a three-second shot at twelve frames a second rebuilt the
// same room thirty-six times. On a laptop that is waste; on a phone it is the difference between a
// film that plays and a film that stutters.
struct Cache {
    int shot = -1;
    Stage stage;
    bool held = false;

    // Where the camera was on the previous frame, so this one can be smeared from there to here.
    // It is kept rather than recomputed because recomputing it means running the whole camera
    // decision a second time, and because the cache is the one thing here that already knows what
    // happened a frame ago. `at` is when that camera was, and `lensShot` which shot it belonged to:
    // a camera from a different shot, or from a second ago, is not a camera this frame moved from,
    // and smearing between two of them would draw a whip pan across every cut in the film.
    Lens lens;
    double at = -1.0;
    int lensShot = -1;
};

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
    // THE AMBIENT IS THE OPPOSITE COLOUR TO THE KEY, and this is the one change that stopped the
    // pictures looking like they were printed on coloured paper.
    //
    // Every light in the frame used to come from the same place in the palette — the key from
    // pal.key, the ambient from pal.sky, the bounce from pal.ink, the air from pal.sky again — and a
    // horror palette is green in all four. A frame lit entirely in one hue has no colour information
    // in it at all: nothing is warm relative to anything else, so nothing reads as lit, it reads as
    // tinted. It is the difference between a scene lit green and a scene printed on green stock, and
    // the code already had a comment saying so about a narrower version of the same mistake.
    //
    // Film lights the other way round, always: a warm key against cool shadows or a cool key against
    // warm ones. So the ambient is worked out from the key itself — its complement about its own
    // brightness — rather than picked, which means it is right for any palette including ones that do
    // not exist yet, and it cannot drift out of agreement with the key because it IS the key.
    {
        const float kr = static_cast<float>(pal.key.r / 255.0);
        const float kg = static_cast<float>(pal.key.g / 255.0);
        const float kb = static_cast<float>(pal.key.b / 255.0);
        const float grey = 0.299f * kr + 0.587f * kg + 0.114f * kb;
        auto opposite = [&](float c) { return grey * 2.0f - c; };
        // Kept pale. The complement at full strength is a second coloured light fighting the first;
        // most of the way to white, it is air.
        auto pale = [&](float c) { return 0.58f + 0.42f * (c < 0.0f ? 0.0f : (c > 1.0f ? 1.0f : c)); };
        // And leaned a third of the way back toward the set's own sky, so a room still looks like the
        // room it is rather than like a colour theory exercise.
        auto skyward = [&](double c) { return static_cast<float>(c / 255.0); };
        auto mix = [](float a, float b2, float t) { return a + (b2 - a) * t; };
        s.ambientTint = Color{mix(pale(opposite(kr)), 0.55f + 0.45f * skyward(pal.sky.r), 0.32f),
                              mix(pale(opposite(kg)), 0.55f + 0.45f * skyward(pal.sky.g), 0.32f),
                              mix(pale(opposite(kb)), 0.55f + 0.45f * skyward(pal.sky.b), 0.32f),
                              1.0f};
    }

    // Night is a single hard source and almost no bounce; day is soft and comes from everywhere. The
    // difference between them is mostly the ambient, not the key.
    const bool night = shot.time == "NIGHT";
    const bool dusk = shot.time == "DUSK" || shot.time == "DAWN";
    // Night is one hard source and almost no bounce. The temptation is to lift the ambient until the
    // frame is comfortable to look at, and that is how a night scene turns into an overcast afternoon:
    // the ambient stays low and the EXPOSURE does the lifting, which keeps the shape of the light.
    s.ambient = night ? 0.105f : (dusk ? 0.185f : 0.300f);
    s.fill = night ? 0.075f : (dusk ? 0.130f : 0.205f);
    // And the KEY carries the hour too, which it did not have to before the set was built out of real
    // materials. A palette's colours are dark for a night scene, so when every surface was painted
    // out of the palette the darkness of the night was in the paint. Plaster is plaster at midnight —
    // it reflects three quarters of what falls on it whatever the time — so the night has to be in
    // the LIGHT now, which is where it was always supposed to be.
    const float hour = night ? 0.30f : (dusk ? 0.58f : 1.0f);
    s.key = Color{s.key.r * hour, s.key.g * hour, s.key.b * hour, 1.0f};
    // A wound-up scene gets a harder, lower key from further round the side: the light follows the
    // mood, which is the one thing about lighting an audience reads without being told.
    const float mood = static_cast<float>(shot.mood);
    const float side = 0.42f + 0.36f * mood;
    const float drift = 0.04f * static_cast<float>(std::sin(seconds * 0.21));
    s.keyDirection = math::normalize(
        math::vec3(-side + drift, -(0.86f - 0.30f * mood), 0.62f - 0.22f * mood));
    s.ambient *= 1.0f - 0.25f * mood;
    // What comes back UP off the floor: the floor's own colour, and much less of it than comes down.
    // Without this every surface in the frame gets the same ambient from every direction, which is
    // why a wall used to read as paper — there is no gradient anywhere in a plane lit that way, and a
    // real room has nothing but gradients.
    // What comes back UP off the floor carries the floor's colour, and the floor is the set's — so
    // this one IS the palette's, and it is the warm-or-cool counterweight to whatever the ambient
    // turned out to be. Between the three of them a surface facing up, a surface facing down and a
    // surface facing the lamp are three different colours, which is what makes a plane read as lit.
    s.bounceTint = Color{static_cast<float>(0.34 + 0.62 * pal.deep.r / 255.0),
                         static_cast<float>(0.32 + 0.62 * pal.deep.g / 255.0),
                         static_cast<float>(0.30 + 0.58 * pal.deep.b / 255.0), 1.0f};
    s.bounce = night ? 0.40f : 0.62f;
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
    // These were 1.30 / 1.18 / 1.05, and a night scene needed all of it: the set was painted out of
    // the palette, the palette is dark at night, and the picture came off the rasteriser almost black.
    // Now that the set is built out of real materials — plaster reflects three quarters of what falls
    // on it at midnight as well as at noon — the darkness is in the LIGHT instead, and the print has
    // to be pulled back down or a night horror film reads as a hospital at lunchtime, which is exactly
    // what it did for one round.
    const float base = night ? 0.78f : (dusk ? 0.94f : 1.05f);

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
                        double time, double progress, Cache* cache = nullptr) {
    Scene sc;
    sc.palette = maz::film::paletteFor(reel.genre, shot.time, shot.mood);
    if (cache != nullptr && cache->held && cache->shot == shot.index) {
        sc.stage = cache->stage;
    } else {
        sc.stage = maz::film::buildStage(shot, sc.palette, reel.seed);
        if (cache != nullptr) {
            cache->stage = sc.stage;
            cache->shot = shot.index;
            cache->held = true;
        }
    }

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

    // Where the object is, until somebody turns out to be holding it: on its plinth, where the room
    // put it.
    sc.objectAt = maz::film::detail::move(sc.stage.objectAt.x, sc.stage.objectAt.y,
                                          sc.stage.objectAt.z);
    bool heldByAnyone = false;

    const int total = static_cast<int>(present.size());
    for (int i = 0; i < total; ++i) {
        const Cast* who = present[static_cast<std::size_t>(i)];
        Standing st;
        st.who = who;
        st.speaking = !shot.speaker.empty() && shot.speaker == who->name;

        // What they are DOING with the room, as against where they are standing in it. Read out of
        // the reel: who is holding the thing the story turns on is recorded data, and who sits down
        // is read out of the prose, which is the only place the film ever says so.
        const maz::film::Business biz =
            maz::film::businessAt(reel, shot.index, who->name, progress);
        st.business = biz;

        Motive mv;
        mv.seed = hashName(who->name);
        mv.position = markFor(sc.stage, i, total);
        // They face the camera, turned a little toward whoever else is there. Squarer to the lens
        // than two people talking would really stand — which is what actors are directed to do, and
        // for the same reason: turned as far as life would turn them, the camera gets two profiles.
        const float toCentre = mv.position.x > 0.0f ? 0.30f : -0.30f;
        mv.facing = 3.14159265f + toCentre; // 0 faces +Z, away from the camera; turn them round
        mv.tension = static_cast<float>(shot.mood) * 0.7f;

        // Sitting moves somebody off their mark and onto the chair — and the camera goes with them,
        // because it is aimed at where people ARE rather than at where the marks were.
        if (biz.sit > 0.001f) {
            mv.sit = biz.sit;
            mv.seatY = sc.stage.seatAt.y;
            mv.position = math::vec3(sc.stage.seatAt.x, 0.0f, sc.stage.seatAt.z);
            mv.facing = sc.stage.seatFacing + toCentre * 0.5f;
        }
        mv.leanOn = biz.lean;

        // An action shot MOVES somebody: the flat film's "action" was a caption over a still picture,
        // and a character who never walks anywhere is the clearest thing missing from it.
        const bool walker =
            shot.kind == "action" && i == 0 && shot.duration > 1.6 && biz.sit <= 0.001f;
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

        // ---- the thing the story turns on ---------------------------------------------------------
        //
        // Whoever has it, has it IN THEIR HAND. And the two shots that matter — the one where they
        // take it and the one where they give it up — are played rather than cut around: the hand
        // goes to where the thing is and comes back with it, or goes out and leaves it there. The
        // reel knew who was holding it all along; the 3D renderer simply never asked.
        const int carryHand = (mv.seed & 2u) ? maz::film::kLeft : maz::film::kRight;
        if (biz.holding && (biz.takingIt || biz.givingItUp)) {
            const float u = static_cast<float>(progress);
            // Taking it: the hand is out at the object at the start of the shot and back by the end.
            // Giving it up: the other way round. Either way the reach is over well before the cut, so
            // the shot is not one long stretch.
            const float phase = biz.takingIt ? 1.0f - std::fmin(1.0f, u * 2.2f)
                                             : std::fmin(1.0f, std::fmax(0.0f, (u - 0.25f) * 2.2f));
            mv.reaching = true;
            mv.reachHand = carryHand;
            mv.reachAmount = phase;
            mv.reachTo = sc.stage.objectAt;
        }

        st.pose = maz::film::performAt(who->build, mv, static_cast<float>(time));
        st.skeleton = maz::film::skeletonOf(who->build, st.pose);
        if (biz.holding) {
            sc.objectAt = st.skeleton.wrist[carryHand] *
                          maz::film::detail::move(0.0f, -who->build.m(who->build.handLen) * 0.55f,
                                                  0.0f);
            sc.objectShown = true;
            heldByAnyone = true;
        }

        // The face. Everyone in the shot reacts to the beat the shot is on, not just whoever is
        // talking — a listener's face is half of why the cut to them exists.
        maz::film::Reacting react;
        react.beat = shot.beat;
        react.mood = shot.mood;
        react.speaking = st.speaking;
        react.syllable = mv.syllable;
        react.toward = lookYaw < 0.0f ? -1.0f : (lookYaw > 0.0f ? 1.0f : 0.0f);
        react.toward *= std::fmin(1.0f, std::fabs(lookYaw) / 0.8f);
        react.seconds = time;
        react.seed = mv.seed;
        st.face = maz::film::faceAt(react);
        sc.people.push_back(st);
    }

    // Nobody has it, so it is back where the room keeps it — except when the story has LOST it, which
    // is a state the object arc actually tracks, and an object that stays visibly on its plinth
    // through the scene where everyone is looking for it is a plot hole you can see.
    if (!heldByAnyone) {
        sc.objectShown = shot.objectBeat != "lost";
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
                         int width, int height, const Look& look, Cache* cache = nullptr) {
    Image img(width, height, Color{0.0f, 0.0f, 0.0f, 1.0f});
    const Shot* shot = maz::film::shotAt(reel, time);
    if (shot == nullptr || width < 8 || height < 8) {
        return img;
    }
    const float frameW = static_cast<float>(width);
    const float frameH = std::fmin(static_cast<float>(height), frameW / maz::film::kAspect);
    const float frameY = (static_cast<float>(height) - frameH) / 2.0f;
    const double progress = shot->duration > 0.0 ? (time - shot->start) / shot->duration : 0.0;

    Scene sc = stageScene(reel, *shot, cast, time, progress, cache);
    // Built once and used twice: the shadow pass and the picture see the same bodies, which they must,
    // and a body is a few thousand triangles to assemble.
    // How much of a figure to build, from how much of the frame it is going to fill.
    //
    // Every pixel here is worked out on the processor, so this is not an optimisation in the usual
    // sense of buying speed nobody asked for: it is the frame budget that everything else has to be
    // paid for out of. A head is three thousand triangles at full detail and is fifteen pixels tall
    // in a wide shot.
    //
    // It is worked out from the SHOT's framing rather than from the camera's position at this
    // instant, and deliberately: a push that halves the distance over five seconds would otherwise
    // walk the figure up through the detail levels while it played, and every step of that is a
    // visible change of shape. Fixed for the length of a shot, it cannot pop, because there is
    // nothing to pop between.
    const float acting = std::sqrt(math::dot(sc.lens.at - sc.lens.eye, sc.lens.at - sc.lens.eye));
    // How tall 1.8 metres is, in pixels, at the distance the camera is standing.
    const float tall = acting > 0.05f
                           ? 1.8f / acting / (2.0f * std::tan(sc.lens.fovY * 0.5f)) * frameH
                           : frameH;
    const float detail = tall > 300.0f ? 1.0f
                         : tall > 150.0f ? 0.70f
                         : tall > 70.0f  ? 0.48f
                                         : 0.32f;

    std::vector<maz::render::shapes::MeshData> bodies;
    bodies.reserve(sc.people.size() + 1);
    // The object goes in with the bodies rather than with the room, because it moves like one: it is
    // wherever the scene says it is, which may be on a plinth or may be in somebody's fist.
    if (sc.objectShown && !sc.stage.object.vertices.empty()) {
        bodies.push_back(maz::render::applyTransform(sc.stage.object, sc.objectAt));
    }
    for (const Standing& who : sc.people) {
        bodies.push_back(maz::film::buildBody(who.who->build, who.skeleton, who.face, detail));
    }

    Surface surf = surfaceFor(sc.palette, *shot, time);
    setAir(surf, sc.palette, *shot, sc.stage.indoors);
    // The room's own lamp, wherever this set keeps one. It carries the set's accent rather than the
    // key's colour, because the whole point of a practical is that it is a DIFFERENT light: two lamps
    // of the same colour are one lamp with a longer shadow.
    surf.lampAt = sc.stage.lampAt;
    surf.lampReach = sc.stage.lampReach;
    surf.lampStrength = sc.stage.lampStrength * (shot->time == "DAY" ? 0.55f : 1.0f);
    // The ACCENT, not the key. A practical the same colour as the key is one lamp with a longer
    // shadow; a practical in a different colour is a second light, and the eye reads the difference
    // between two lights long before it reads either of them.
    surf.lamp = Color{static_cast<float>(0.42 + 0.58 * sc.palette.accent.r / 255.0),
                      static_cast<float>(0.38 + 0.58 * sc.palette.accent.g / 255.0),
                      static_cast<float>(0.34 + 0.58 * sc.palette.accent.b / 255.0), 1.0f};

    // ---- what the key light cannot see ------------------------------------------------------------
    //
    // The map is fitted to the ACTING AREA rather than to the whole set. A corridor is twelve metres
    // long and a chapel six and a half tall, and a map stretched over all of that spends its
    // resolution on the far end of a room nobody is standing in, leaving the shadow under a foot —
    // the one everybody actually looks at — four texels wide.
    maz::render::ShadowMap shadows(look.shadows == 1 ? 512 : 1024);
    if (look.shadows > 0) {
        const float reach = std::fmin(sc.stage.halfWidth, 4.5f);
        const float ceiling = sc.stage.ceiling > 0.0f ? std::fmin(sc.stage.ceiling, 3.2f) : 2.6f;
        const math::vec3 lo(-reach, -0.15f, sc.stage.markLeft.z - 2.6f);
        const math::vec3 hi(reach, ceiling, sc.stage.markLeft.z + 3.4f);
        shadows.begin(maz::render::directionalLight(lo, hi, surf.keyDirection));
        shadows.add(sc.stage.props);
        for (const maz::render::shapes::MeshData& body : bodies) {
            shadows.add(body);
        }
        shadows.setSoftness(look.shadows >= 2 ? 3 : 1);
        // CONTACT HARDENING: how big the thing doing the lighting is.
        //
        // A shadow is sharp where the object touches the ground and opens out with distance, and how
        // fast it opens out is how WIDE the source is. Every shadow in here used to be equally soft
        // everywhere, which is the look of an object photographed separately and pasted onto a
        // photograph of a floor — the edge tightening at the feet is the cue that says the two things
        // are in the same room.
        //
        // Outdoors in daylight that source is the sun, half a degree across, and shadows stay crisp
        // several metres out. Anywhere else — indoors, or outside after dark — the light is a window
        // or a lamp, several degrees across, and an arm's shadow on a wall two metres behind it is a
        // grey suggestion. Three degrees is not measured off anything; it is what a room lit the way
        // a film lights a room looks like.
        const bool daylight = !sc.stage.indoors && shot->time != "NIGHT";
        shadows.setSourceSize(daylight ? 0.009f : 0.055f);
        surf.shadows = &shadows;
        // Not all of it. A shadow in a film is never black — there is always bounce finding its way
        // in — and taking the whole key away turns a figure's own shadow side into a hole.
        surf.shadowStrength = 0.84f;
    }

    // The picture is rendered inside the letterboxed window only, and at a multiple of its size so the
    // edges can be averaged down. There is no anti-aliasing in the rasteriser on purpose: a frame
    // rendered twice over and averaged is simpler, is exactly right, and costs only what it costs.
    const int S = look.supersample < 1 ? 1 : (look.supersample > 3 ? 3 : look.supersample);
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

    // ---- the weather ------------------------------------------------------------------------------
    //
    // Last, and blended, and casting nothing. It is drawn after everything else because it is
    // translucent and the rasteriser does not sort — and because rain that throws a shadow is a
    // thousand tiny holes punched in the set.
    //
    // In three dimensions the air is SOMEWHERE. Rain that falls between the camera and a face is the
    // shot; rain painted on top of the finished frame, which is what the flat renderer does and is
    // right for a painted film, is a screen saver.
    const maz::film::Weather weather =
        maz::film::weatherFor(reel.genre, shot->time, shot->set);
    if (maz::film::weatherHasParticles(weather)) {
        const maz::render::shapes::MeshData air = maz::film::airMesh(
            weather, sc.lens.eye, sc.lens.at, time, reel.seed,
            Color{static_cast<float>(sc.palette.key.r / 255.0),
                  static_cast<float>(sc.palette.key.g / 255.0),
                  static_cast<float>(sc.palette.key.b / 255.0), 1.0f},
            static_cast<float>(shot->mood));
        Surface wet = surf;
        wet.shadows = nullptr;
        wet.cull = maz::render::Cull::None; // a flake has no back: it is the same flake from behind
        wet.alpha = maz::film::airAlpha(weather);
        wet.emissive = maz::film::airGlow(weather);
        wet.fogMax *= 0.55f; // the air does not fade into itself as fast as the set fades into it
        raster.draw(big, air, math::mat4(1.0f), vp, wet);
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

    // ---- the lens ---------------------------------------------------------------------------------
    //
    // Everything up to here was drawn by a PINHOLE: every point in the world lands on exactly one
    // pixel however far away it is. Real glass focuses at one distance and turns everything else into
    // a small disc, and that is most of what separates a photograph from a diagram — a close-up with
    // the room sharp behind it is a snapshot, and the same close-up with the room fallen away is a
    // close-up.
    //
    // It focuses on WHATEVER THE CAMERA IS AIMED AT, which is not a choice so much as the definition:
    // lensFor already decided what the shot is of, so the focus distance falls out of it for free and
    // cannot disagree with the framing. How far open the lens is does depend on the framing, and the
    // reason is the same one that gives a close-up a long lens: shallow focus on a wide shot is a
    // mistake, and deep focus on a close-up throws away the only thing a close-up is for.
    //
    // Blurred BEFORE the print curve, because defocus is something that happens to light on its way to
    // the film and not to the photograph afterwards.
    // The depth buffer, read back at the frame's own resolution: one sample per finished pixel, taken
    // from the middle of the block that made it. Both the shutter and the lens need it — the shutter
    // to know how far the thing at each pixel moved, the lens to know how far away it is — so it is
    // read once here rather than twice below.
    const bool wantsShutter = look.shutter > 0 && cache != nullptr && cache->lensShot == shot->index &&
                              cache->at >= 0.0 && time > cache->at && time - cache->at < 0.35;
    std::vector<float> depth;
    if (look.lens > 0 || wantsShutter) {
        depth.assign(static_cast<std::size_t>(frameW) * static_cast<std::size_t>(frameH), 1.0f);
        for (int y = 0; y < static_cast<int>(frameH); ++y) {
            for (int x = 0; x < static_cast<int>(frameW); ++x) {
                depth[static_cast<std::size_t>(y) * static_cast<std::size_t>(frameW) +
                      static_cast<std::size_t>(x)] = raster.depthAt(x * S + S / 2, y * S + S / 2);
            }
        }
    }

    // ---- the shutter ------------------------------------------------------------------------------
    //
    // A rasteriser renders an instant. A film camera does not: its shutter is open for a fraction of
    // every frame, and whatever moved while it was open lands on the film as a smear. That smear is
    // not authenticity being simulated — it is the thing that joins one frame to the next, and
    // without it a pan at twelve frames a second is a sequence of separate photographs.
    //
    // Where the camera WAS comes out of the cache, which is the one thing here that already knows
    // what happened a frame ago. Applied before the lens, because the shutter is open while the light
    // is still on its way through the glass.
    if (wantsShutter) {
        const math::mat4 wasView =
            glm::lookAt(cache->lens.eye, cache->lens.at, math::vec3(0.0f, 1.0f, 0.0f));
        const math::mat4 wasVp =
            math::perspective(cache->lens.fovY, frameW / frameH, 0.04f, 220.0f) * wasView;
        maz::render::motionBlur(img, y0, y0 + static_cast<int>(frameH), depth, vp, wasVp,
                                static_cast<float>(look.shutter) / 100.0f);
    }
    if (cache != nullptr) {
        cache->lens = sc.lens;
        cache->at = time;
        cache->lensShot = shot->index;
    }

    if (look.lens > 0) {
        const float focus = std::sqrt(math::dot(sc.lens.at - sc.lens.eye, sc.lens.at - sc.lens.eye));
        float aperture = 0.0f;
        if (shot->framing == "close" || shot->framing == "ots" || shot->framing == "insert") {
            aperture = 1.70f;
        } else if (shot->framing == "mid" || shot->framing == "two" || shot->framing == "low") {
            aperture = 0.80f;
        } else {
            // And a wide gets NO lens at all, which is both the honest artistic answer and the one
            // that pays for the others. A wide shot is about the room: throwing the room out of
            // focus in it is throwing away the shot. At the aperture a wide would want, the furthest
            // thing in frame comes out five per cent soft, which nobody can see and everybody pays
            // six milliseconds a frame for.
            aperture = 0.0f;
        }
        maz::render::depthOfField(img, y0, y0 + static_cast<int>(frameH), depth, 0.04f, 220.0f, focus,
                                  aperture, look.lens);
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
