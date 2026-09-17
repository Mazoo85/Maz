#pragma once

#include "maz/film/Palette.hpp"
#include "maz/film/Reel.hpp"
#include "maz/math/Math.hpp"
#include "maz/render/MeshMerge.hpp"
#include "maz/render/MeshTransform.hpp"
#include "maz/render/MeshRefine.hpp"
#include "maz/render/Shapes.hpp"
#include "maz/render/SurfaceGrain.hpp"
#include "maz/render/Shapes3D.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

// maz::film STAGE — a place with a floor, walls and things in it, in three dimensions.
//
// The flat renderer drew a set as three painted planes at three parallax rates, which is a very old
// and very good trick and is still a painting: nothing in it can be walked behind, the light cannot
// fall across it, and moving the camera does not reveal anything, because there is nothing there to
// reveal. This builds the same fifteen places as ROOMS — real floors, real walls, props with a back to
// them — so that a character can stand behind a table, a camera can move around them, and the key
// light can throw one side of the room into shade.
//
// The rooms are BUILT TO SCALE, in metres, against the bodies that stand in them. A door is 2.05m, a
// table 0.74m, a chair seat 0.45m, a counter 0.92m, a corridor 2.4m wide. That is not fussiness: an
// actor is 1.78m tall and every one of those numbers is read against them, so a table at "about right"
// puts a grown adult's hip at their ribs and quietly wrecks every shot in the room.
namespace maz::film {

// How big the room is and where a camera may stand in it. Z runs away from the camera's usual
// position, X across, Y up from the floor.
struct Stage {
    render::shapes::MeshData mesh;
    // What CASTS a shadow: the furniture, the trees, the plinth — everything in the room, but not the
    // room. A key light in a film is a conceit; a real lamp outside a closed box with a ceiling on it
    // lights nothing inside, and the first frame rendered with shadows switched on was a room with a
    // lid, correctly pitch dark. So the shell is lit and receives, and does not cast.
    render::shapes::MeshData props;
    float halfWidth = 5.0f;  // metres either side of centre
    float depth = 7.0f;      // metres from the front of the stage to the back wall
    float ceiling = 2.9f;    // metres; 0 means outdoors, with no ceiling and no walls
    bool indoors = true;
    // The marks the actors stand on, left of frame and right of frame.
    math::vec3 markLeft{-0.62f, 0.0f, 0.0f};
    math::vec3 markRight{0.62f, 0.0f, 0.0f};
    // Where the thing the story turns on sits when nobody is holding it.
    math::vec3 objectAt{0.0f, 0.9f, 0.6f};
    // And the thing itself, modelled about its own origin so it can be picked up. It is deliberately
    // NOT part of the room: an object welded to its plinth is an object nobody in the film ever
    // touches, and the whole point of it is that somebody does.
    render::shapes::MeshData object;

    // Somewhere to sit. Every buildable set has one, because "sits down" is a stage direction the
    // screenplay writes for any of them and a character who cannot sit just stands there instead —
    // and it is placed just off the acting area rather than against the back wall, so somebody who
    // sits is still inside the shot the framing was built for.
    math::vec3 seatAt{-0.95f, 0.45f, 1.55f}; // x, the height of the seat surface, z
    float seatFacing = 3.14159265f;          // which way somebody sitting on it faces

    // THE PRACTICAL: a lamp that is IN the room, as against the key, which comes from infinitely far
    // away and therefore lights a whole wall to exactly one value however big the wall is. That
    // evenness is what a flat surface reads as — there is no gradient anywhere in it, and a real room
    // has nothing but gradients. Where it goes depends on the room: a strip light over a corridor, a
    // window in a ward, a fire in a chapel, the moon behind the trees.
    math::vec3 lampAt{0.0f, 2.4f, 2.0f};
    float lampStrength = 0.0f; // 0 means this set has no practical, which is allowed
    float lampReach = 3.4f;    // metres to half brightness
};

namespace stagedetail {

inline render::Color rgb(const Rgb& c, double scale = 1.0) {
    return render::Color{static_cast<float>(c.r * scale / 255.0), static_cast<float>(c.g * scale / 255.0),
                         static_cast<float>(c.b * scale / 255.0), 1.0f};
}

inline void add(render::shapes::MeshData& into, const render::shapes::MeshData& part) {
    into = render::mergeMeshes(into, part);
}

// A MATERIAL: a real colour — plaster, lino, wood, steel — leaned part of the way toward the film's
// own palette, rather than the palette over again at a different brightness.
//
// A set built out of one palette colour at four brightnesses photographs as one thing. There is no
// colour information in it anywhere: nothing is warm relative to anything else, so the frame reads as
// TINTED rather than as lit. And no amount of work on the lighting fixes it, because light multiplies
// the surface colour — a green wall lit pink is a green wall. Two rounds of effort went into the
// lighting before that was obvious, which is the reason this comment is as long as it is.
//
// `lean` is how far toward the palette to go: 0 leaves the material as it is, 1 is the palette neat.
// Around a third keeps a horror set green enough that you know it is a horror set, while leaving a
// wooden chair browner than the plaster behind it.
inline render::Color material(float r, float g, float b, const Rgb& toward, float lean, float value) {
    const float tr = static_cast<float>(toward.r / 255.0);
    const float tg = static_cast<float>(toward.g / 255.0);
    const float tb = static_cast<float>(toward.b / 255.0);
    // Leaned toward the palette's HUE but not its brightness: the palette colours are already dark for
    // a night scene, and multiplying a dark material by a dark palette gives black.
    const float tLum = 0.299f * tr + 0.587f * tg + 0.114f * tb;
    const float norm = tLum > 0.04f ? 1.0f / tLum : 1.0f;
    auto mixed = [&](float m, float t) {
        const float leaned = m + (m * t * norm - m) * lean;
        const float out = leaned * value;
        return out < 0.0f ? 0.0f : (out > 1.0f ? 1.0f : out);
    };
    return render::Color{mixed(r, tr), mixed(g, tg), mixed(b, tb), 1.0f};
}


// A box given by its FULL size and the position of its centre. makeBox takes a full edge length and
// the scale multiplies it, so the numbers written here are the numbers you would measure.
inline render::shapes::MeshData box(float sx, float sy, float sz, const math::vec3& at,
                                    const render::Color& c) {
    return render::applyTransform(
        render::applyTransform(render::shapes::makeBox(1.0f, c),
                               render::scaleMatrix(math::vec3(sx, sy, sz))),
        render::translationMatrix(at));
}

// The same box, with somewhere to put detail on it.
//
// A wall is one enormous quad, and a quad has four corners. Anything worked out per vertex — a
// colour variation, a bake, dirt at the skirting — can only be linear across the whole wall, which
// means it cannot be seen. So the surfaces big enough to read as flat get cut into a grid first.
//
// A THIRD of a metre is where this landed. Coarser and the variation reads as a few large stains;
// finer and the triangle count climbs with nothing to show for it, and the count is not free — every
// triangle is rasterised twice, once for the picture and once into the shadow map.
inline render::shapes::MeshData surface(float sx, float sy, float sz, const math::vec3& at,
                                        const render::Color& c, float cell = 99.0f) {
    render::shapes::MeshData m = box(sx, sy, sz, at, c);
    render::refineMesh(m, cell);
    return m;
}

// A box standing ON the floor at `at`, which is how furniture is actually described.
inline render::shapes::MeshData stand(float sx, float sy, float sz, float x, float z,
                                      const render::Color& c) {
    return box(sx, sy, sz, math::vec3(x, sy * 0.5f, z), c);
}

inline render::shapes::MeshData post(float radius, float height, float x, float z,
                                     const render::Color& c, int sides = 10) {
    return render::applyTransform(render::shapes::makeCylinder(radius, height, sides, c),
                                  render::translationMatrix(math::vec3(x, height * 0.5f, z)));
}

// A table: a top and four legs, at the height a table is.
inline render::shapes::MeshData table(float w, float d, float x, float z, const render::Color& top,
                                      const render::Color& leg) {
    const float h = 0.74f;
    render::shapes::MeshData m = box(w, 0.045f, d, math::vec3(x, h, z), top);
    const float lx = w * 0.5f - 0.06f;
    const float lz = d * 0.5f - 0.06f;
    for (int i = 0; i < 4; ++i) {
        const float sx = (i & 1) ? 1.0f : -1.0f;
        const float sz = (i & 2) ? 1.0f : -1.0f;
        add(m, stand(0.055f, h - 0.045f, 0.055f, x + sx * lx, z + sz * lz, leg));
    }
    return m;
}

inline render::shapes::MeshData chair(float x, float z, float facing, const render::Color& c) {
    render::shapes::MeshData m = box(0.44f, 0.04f, 0.44f, math::vec3(0.0f, 0.45f, 0.0f), c);
    add(m, box(0.44f, 0.50f, 0.04f, math::vec3(0.0f, 0.70f, -0.20f), c));
    for (int i = 0; i < 4; ++i) {
        const float sx = (i & 1) ? 0.18f : -0.18f;
        const float sz = (i & 2) ? 0.18f : -0.18f;
        add(m, stand(0.035f, 0.43f, 0.035f, sx, sz, c));
    }
    return render::applyTransform(
        m, render::translationMatrix(math::vec3(x, 0.0f, z)) *
               glm::rotate(math::mat4(1.0f), facing, math::vec3(0.0f, 1.0f, 0.0f)));
}

// A doorway cut into the back wall: the wall is built as four pieces around the hole, because a hole
// is the one thing a box cannot have and a room without one is a box you are sealed inside.
inline render::shapes::MeshData wallWithDoor(float halfW, float height, float z, float doorX,
                                             const render::Color& c, bool withDoor) {
    const float dw = 0.92f;  // a door is 920mm wide
    const float dh = 2.05f;  // and 2.05m tall
    render::shapes::MeshData m;
    if (!withDoor) {
        add(m, surface(halfW * 2.0f, height, 0.12f, math::vec3(0.0f, height * 0.5f, z), c));
        return m;
    }
    const float leftW = (doorX - dw * 0.5f) + halfW;
    const float rightW = halfW - (doorX + dw * 0.5f);
    add(m, surface(leftW, height, 0.12f, math::vec3(-halfW + leftW * 0.5f, height * 0.5f, z), c));
    add(m, surface(rightW, height, 0.12f, math::vec3(halfW - rightW * 0.5f, height * 0.5f, z), c));
    add(m, surface(dw, height - dh, 0.12f, math::vec3(doorX, dh + (height - dh) * 0.5f, z), c));
    return m;
}

// A deterministic dice from the shot, so the same film dresses the same room the same way twice.
struct Dice {
    std::uint32_t s;
    explicit Dice(std::uint32_t seed) : s(seed ? seed : 0x9E3779B9u) {}
    std::uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float unit() { return static_cast<float>(next() % 100000u) / 100000.0f; }
    float range(float lo, float hi) { return lo + (hi - lo) * unit(); }
    bool chance(float p) { return unit() < p; }
};

} // namespace stagedetail

// Into the picture AND into the list of things that cast.
inline void addProp(Stage& st, const render::shapes::MeshData& part) {
    st.mesh = render::mergeMeshes(st.mesh, part);
    st.props = render::mergeMeshes(st.props, part);
}

// Build the room this shot happens in.
inline Stage buildStage(const Shot& shot, const Palette& pal, std::uint32_t seed) {
    using namespace stagedetail;
    Stage st;
    Dice dice(seed * 2654435761u + static_cast<std::uint32_t>(shot.scene) * 40503u + 17u);

    // Walls and floor are pulled apart in VALUE and in HUE. A room built out of one shade of the
    // palette photographs as a single dark mass with a person standing in front of it; the eye needs
    // the floor, the walls and the ceiling to be three different values AND three different colours
    // before it will read a room. The value half of that was already here; the colour half is what
    // material() is for, and it is the difference between a lit set and a tinted one.
    const float lean = 0.46f;  // the shell carries the genre: a horror room is a green room
    const float pLean = 0.26f; // the props carry less of it, so a wooden chair stays wooden
    const render::Color floorC = material(0.42f, 0.39f, 0.36f, pal.deep, lean, 1.00f);
    const render::Color wallC = material(0.76f, 0.75f, 0.72f, pal.deep, lean, 0.92f);
    const render::Color ceilC = material(0.82f, 0.82f, 0.80f, pal.deep, lean * 0.7f, 0.78f);
    const render::Color woodC = material(0.40f, 0.32f, 0.25f, pal.deep, pLean, 1.00f);
    const render::Color steelC = material(0.52f, 0.55f, 0.59f, pal.sky, pLean, 1.00f);
    const render::Color clothC = material(0.52f, 0.50f, 0.50f, pal.accent, 0.20f, 0.96f);
    const render::Color inkC = rgb(pal.ink, 1.0);
    const render::Color accentC = rgb(pal.accent, 1.0);
    const render::Color skyC = rgb(pal.sky, 1.0);

    const std::string& set = shot.set;
    // Inside or out. The SHOT knows, because the script wrote it into the slug line, and that is the
    // answer whenever it is there. The list of set names below is only the fallback for a reel old
    // enough not to carry it — and it is worth keeping the reason: this renderer had "lighthouse"
    // down as outdoors while the writer had it down as INT., so a film set in a lamp room was drawn
    // as an open field under a sky, for as long as the two lists were allowed to disagree.
    const bool guessOutdoors = set == "woods" || set == "field" || set == "street" ||
                               set == "water" || set == "lighthouse";
    const bool outdoors = shot.interior >= 0 ? (shot.interior == 0) : guessOutdoors;
    st.indoors = !outdoors;

    // ---- the shape of the place ------------------------------------------------------------------
    if (set == "corridor") {
        st.halfWidth = 1.2f; // a corridor is 2.4m across, and that is why it feels like one
        st.depth = 12.0f;
        st.ceiling = 2.6f;
    } else if (set == "ward" || set == "office" || set == "kitchen" || set == "room") {
        st.halfWidth = 3.2f;
        st.depth = 6.5f;
        st.ceiling = 2.75f;
    } else if (set == "bar" || set == "chapel") {
        st.halfWidth = 4.2f;
        st.depth = 9.0f;
        st.ceiling = set == "chapel" ? 6.5f : 3.1f;
    } else if (set == "ship" || set == "industrial" || set == "vehicle") {
        st.halfWidth = 2.6f;
        st.depth = 7.5f;
        st.ceiling = set == "vehicle" ? 1.95f : 3.4f;
    } else if (set == "lighthouse") {
        // A lamp room: small, tall for its floor, and the one interior in the list that nobody had
        // given a shape to, because this renderer had it filed as an outdoor set. With the slug line
        // now deciding, it turns up indoors and needs a room.
        st.halfWidth = 2.2f;
        st.depth = 4.4f;
        st.ceiling = 3.2f;
    } else {
        st.halfWidth = 9.0f;
        st.depth = 22.0f;
        st.ceiling = 0.0f;
    }

    // An interior with no ceiling is a contradiction, and the way it fails is not a crash: the walls
    // are built to height zero and the room comes out as a floor floating in the sky. Any set that
    // arrives indoors without a shape of its own gets an ordinary room rather than that.
    if (st.indoors && st.ceiling <= 0.01f) {
        st.halfWidth = 3.2f;
        st.depth = 6.5f;
        st.ceiling = 2.75f;
    }

    // ---- floor, walls, ceiling -------------------------------------------------------------------
    {
        const float w = st.halfWidth * 2.0f + 0.4f;
        add(st.mesh, surface(w, 0.12f, st.depth + 4.0f,
                             math::vec3(0.0f, -0.06f, st.depth * 0.5f - 1.5f), floorC));
        if (st.indoors) {
            const float h = st.ceiling;
            add(st.mesh, wallWithDoor(st.halfWidth, h, st.depth, dice.range(-0.5f, 0.5f) * st.halfWidth,
                                      wallC, set != "chapel"));
            add(st.mesh, surface(0.12f, h, st.depth + 3.0f,
                                 math::vec3(-st.halfWidth, h * 0.5f, st.depth * 0.5f - 1.0f), wallC));
            add(st.mesh, surface(0.12f, h, st.depth + 3.0f,
                                 math::vec3(st.halfWidth, h * 0.5f, st.depth * 0.5f - 1.0f), wallC));
            add(st.mesh, surface(st.halfWidth * 2.0f, 0.10f, st.depth + 3.0f,
                                 math::vec3(0.0f, h, st.depth * 0.5f - 1.0f), ceilC));
            // A skirting line, which is most of what tells you a wall is a wall and not a backdrop.
            add(st.mesh, box(st.halfWidth * 2.0f, 0.11f, 0.02f, math::vec3(0.0f, 0.055f, st.depth - 0.07f),
                             inkC));
            // And the same down both SIDE walls, which is where it was missing and where it matters
            // more. The back wall is a long way off and usually half behind somebody; the side walls
            // are the two biggest objects in almost every frame, and they were unbroken planes of one
            // colour from the floor to the ceiling. A line running away from the camera at a constant
            // height is also the strongest perspective cue a room has — it is the thing that says how
            // long the room is.
            const float inner = st.halfWidth - 0.06f;   // the face of the wall, not its middle
            const float runZ = st.depth + 3.0f;
            const float runAt = st.depth * 0.5f - 1.0f;
            for (int side = 0; side < 2; ++side) {
                const float sx = side == 0 ? -1.0f : 1.0f;
                add(st.mesh, box(0.03f, 0.11f, runZ,
                                 math::vec3(sx * (inner - 0.015f), 0.055f, runAt), inkC));
                // A rail two thirds of the way up, where a room that has one has one. Not in a
                // vehicle, which has no walls to speak of, and not in a chapel, where the walls are
                // meant to go up uninterrupted.
                if (h > 2.4f && set != "vehicle" && set != "chapel") {
                    add(st.mesh, box(0.028f, 0.035f, runZ,
                                     math::vec3(sx * (inner - 0.014f), h * 0.70f, runAt), woodC));
                }
            }

            // Things ON the walls: notices, pictures, a window, a panel — whatever the room would
            // have. They are flat and they are small, and they do more for a picture than another
            // piece of furniture would, because they break the one surface that has nothing else
            // happening on it. They stand a centimetre proud of the wall and they cast, so each one
            // also throws a small shadow, which is what stops them reading as paint.
            {
                const int each = st.depth > 8.0f ? 4 : 3;
                for (int side = 0; side < 2; ++side) {
                    const float sx = side == 0 ? -1.0f : 1.0f;
                    for (int i = 0; i < each; ++i) {
                        // Every roll in its own statement, for the reason set out below: two rolls
                        // inside one call are evaluated in whichever order the compiler likes, and
                        // GCC and Clang disagree, so the same film dresses itself differently
                        // depending on which one built the renderer.
                        const float rz = dice.range(0.0f, 1.0f);
                        const float rh = dice.range(0.0f, 1.0f);
                        const float rw = dice.range(0.0f, 1.0f);
                        const float rk = dice.range(0.0f, 1.0f);
                        const float lit = dice.range(0.0f, 1.0f);
                        const float z = 0.6f + (st.depth - 1.4f) *
                                                   (static_cast<float>(i) + rz * 0.7f) /
                                                   static_cast<float>(each);
                        const float wide = 0.26f + rw * 0.40f;
                        const float tall = 0.22f + rh * 0.42f;
                        const float at = h * 0.38f + rk * h * 0.22f;   // below the rail, not through it
                        // The first attempt made these out of the palette's darkest ink, and they came
                        // out as flat black rectangles — holes punched in the wall rather than things
                        // hanging on it. They are dressing, so they get a real material like everything
                        // else in the room: a dark board that still takes the light. One in five is the
                        // key colour instead, and that one is what the eye lands on.
                        const render::Color c = lit < 0.20f ? rgb(pal.key, 0.85) : woodC;
                        addProp(st, box(0.045f, tall, wide,
                                        math::vec3(sx * (inner - 0.022f), at, z), c));
                    }
                }
            }
            // A practical: a lamp, a window, a lit panel — something in shot that is ITSELF bright.
            // A night interior with no visible source is a room somebody forgot to light, and no
            // amount of exposure makes it look deliberate.
            if (set != "corridor") {
                const float px = dice.chance(0.5f) ? -st.halfWidth + 0.18f : st.halfWidth - 0.18f;
                const float py = st.ceiling * 0.62f;
                add(st.mesh, box(0.09f, 0.95f, 0.62f, math::vec3(px, py, st.depth * 0.45f),
                                 rgb(pal.key, 1.0)));
            }
        } else {
            // Outdoors: a far plane standing in for the sky, and a horizon of low shapes so the ground
            // does not simply stop.
            // Standing ON the horizon, not through it: a backdrop that continues below the ground
            // is geometry nobody can see and a surface the fog has to be trusted to hide.
            add(st.mesh, box(st.halfWidth * 6.0f, 44.0f, 0.5f,
                             math::vec3(0.0f, 22.0f, st.depth + 18.0f), skyC));
            // Every roll into its own named variable, in its own statement, before anything is built
            // with it.
            //
            // Two dice rolls inside one call — stand(dice.range(..), dice.range(..), ..) — look
            // harmless and are not: C++ does not say which argument is evaluated first, and GCC and
            // Clang genuinely choose differently. The rolls come out in a different ORDER, so the same
            // film with the same seed grows a different skyline depending on which compiler built the
            // renderer. It is not a rounding difference that can be waved away; the buildings are
            // somewhere else. This was invisible until the renderer was compiled a second way and the
            // two were compared, which is the whole reason that test exists.
            for (int i = 0; i < 14; ++i) {
                const float x = dice.range(-st.halfWidth * 2.2f, st.halfWidth * 2.2f);
                const float z = dice.range(st.depth * 0.45f, st.depth + 12.0f);
                const float a = dice.range(0.0f, 1.0f);
                const float b = dice.range(0.0f, 1.0f);
                const float c = dice.range(0.0f, 1.0f);
                auto spread = [](float lo, float hi, float t) { return lo + (hi - lo) * t; };
                if (set == "woods") {
                    addProp(st, post(spread(0.10f, 0.26f, a), spread(5.0f, 11.0f, b), x, z, inkC, 7));
                } else if (set == "street") {
                    addProp(st, stand(spread(3.0f, 7.0f, a), spread(4.0f, 9.0f, b),
                                      spread(3.0f, 6.0f, c), x * 1.6f, z + 6.0f, inkC));
                } else {
                    addProp(st, stand(spread(0.6f, 1.6f, a), spread(0.3f, 1.1f, b),
                                      spread(0.6f, 1.6f, c), x, z, inkC));
                }
            }
            if (set == "lighthouse") {
                addProp(st, post(1.7f, 16.0f, dice.range(-3.0f, 3.0f), st.depth + 6.0f, wallC, 14));
            }
        }
    }

    // ---- what is in it ---------------------------------------------------------------------------
    //
    // From here down, everything is dressing rather than architecture, so it is collected separately
    // and handed to the shadow pass. `into` writes to both.
    //
    // Every flat top that furniture puts into the room is written down as it goes in, and a pass at
    // the end puts things ON them. A room with a bare table in it is a showroom; the difference
    // between a set and a room is the half-dozen small objects nobody placed on purpose. Recording
    // the tops rather than hand-placing clutter per set means a set that gains a table gains its
    // clutter for free, and nothing can be scattered onto a surface that is not there.
    struct Top {
        float x, y, z, halfW, halfD;
    };
    std::vector<Top> tops;
    auto topped = [&tops](float x, float y, float z, float halfW, float halfD) {
        tops.push_back(Top{x, y, z, halfW, halfD});
    };
    if (set == "office") {
        const float deskX = dice.range(-0.5f, 0.5f);
        addProp(st, table(1.55f, 0.78f, deskX, 2.3f, woodC, steelC));
        topped(deskX, 0.7625f, 2.3f, 0.70f, 0.32f);
        addProp(st, chair(0.1f, 3.1f, 0.15f, woodC));
        addProp(st, stand(0.45f, 1.85f, 0.42f, -st.halfWidth + 0.5f, 3.4f, steelC)); // a filing cabinet
    } else if (set == "kitchen" || set == "bar") {
        const float counterH = set == "bar" ? 1.06f : 0.92f;
        addProp(st, stand(st.halfWidth * 1.3f, counterH, 0.62f, 0.0f, 2.6f, woodC));
        addProp(st, box(st.halfWidth * 1.34f, 0.05f, 0.70f, math::vec3(0.0f, counterH, 2.6f), steelC));
        topped(0.0f, counterH + 0.025f, 2.6f, st.halfWidth * 0.60f, 0.26f);
        for (int i = 0; i < 3; ++i) {
            addProp(st, post(0.19f, 0.74f, -1.3f + static_cast<float>(i) * 1.3f, 1.75f, steelC, 9));
        }
    } else if (set == "ward") {
        for (int i = 0; i < 2; ++i) {
            const float x = i == 0 ? -1.8f : 1.8f;
            addProp(st, stand(0.95f, 0.60f, 2.05f, x, 3.2f, clothC));      // a bed, made up
            topped(x, 0.60f, 3.2f, 0.38f, 0.80f);
            addProp(st, post(0.035f, 1.75f, x + 0.62f, 2.5f, steelC, 7)); // a drip stand
        }
    } else if (set == "chapel") {
        for (int i = 0; i < 5; ++i) {
            const float z = 2.2f + static_cast<float>(i) * 1.15f;
            addProp(st, stand(st.halfWidth * 1.1f, 0.44f, 0.36f, 0.0f, z, woodC));
            addProp(st, box(st.halfWidth * 1.1f, 0.55f, 0.07f, math::vec3(0.0f, 0.72f, z - 0.17f), woodC));
        }
        addProp(st, box(0.10f, 1.9f, 0.10f, math::vec3(0.0f, 3.6f, st.depth - 0.2f), accentC));
        addProp(st, box(0.85f, 0.10f, 0.10f, math::vec3(0.0f, 4.05f, st.depth - 0.2f), accentC));
    } else if (set == "ship" || set == "industrial") {
        for (int i = 0; i < 4; ++i) {
            const float z = 1.4f + static_cast<float>(i) * 1.8f;
            addProp(st, post(0.13f, st.ceiling, -st.halfWidth + 0.35f, z, steelC, 8));
            addProp(st, post(0.13f, st.ceiling, st.halfWidth - 0.35f, z, steelC, 8));
            addProp(st, box(0.16f, 0.16f, st.halfWidth * 2.0f,
                             math::vec3(0.0f, st.ceiling - 0.35f, z), steelC));
        }
        const float crateX = dice.range(-1.2f, 1.2f);
        addProp(st, stand(0.8f, 1.1f, 0.8f, crateX, 4.2f, steelC));
        topped(crateX, 1.1f, 4.2f, 0.32f, 0.32f);
    } else if (set == "corridor") {
        for (int i = 0; i < 6; ++i) {
            const float z = 1.1f + static_cast<float>(i) * 1.85f;
            // Doors down both sides, which is what a corridor IS, and what makes its length read.
            addProp(st, box(0.06f, 2.05f, 0.92f, math::vec3(-st.halfWidth + 0.09f, 1.025f, z), woodC));
            addProp(st, box(0.06f, 2.05f, 0.92f, math::vec3(st.halfWidth - 0.09f, 1.025f, z), woodC));
            addProp(st, box(0.55f, 0.05f, 0.16f, math::vec3(0.0f, st.ceiling - 0.06f, z + 0.5f),
                             rgb(pal.key, 1.0)));
        }
    } else if (set == "room") {
        const float sideX = dice.range(-1.0f, 1.0f);
        addProp(st, stand(1.85f, 0.72f, 0.85f, sideX, 3.4f, woodC));
        topped(sideX, 0.72f, 3.4f, 0.80f, 0.34f);
        const float lowX = dice.range(-0.8f, 0.8f);
        addProp(st, table(1.05f, 0.55f, lowX, 1.9f, woodC, steelC));
        topped(lowX, 0.7625f, 1.9f, 0.44f, 0.20f);
    } else if (set == "vehicle") {
        // Behind the marks, not on them. Seats at z = 1.1 sat exactly where the two characters stand,
        // so they were inside the furniture — which nobody could see until the seats started casting a
        // shadow and put both actors in the dark.
        addProp(st, stand(0.52f, 0.95f, 0.55f, -0.55f, 2.15f, clothC));
        addProp(st, stand(0.52f, 0.95f, 0.55f, 0.55f, 2.15f, clothC));
        addProp(st, box(st.halfWidth * 1.9f, 0.9f, 0.08f, math::vec3(0.0f, 1.45f, 2.6f), skyC));
    }

    // ---- and the things left lying on them --------------------------------------------------------
    //
    // Small, dull and slightly askew. Nothing here is meant to be looked at — the point is that in a
    // close-up there is something behind the actor's shoulder with an edge on it, and in a wide shot
    // the tops are not empty planes. They cast, which is where most of the effect actually comes
    // from: half a dozen little shadows on a table read as a used table.
    for (std::size_t t = 0; t < tops.size(); ++t) {
        const Top& tp = tops[t];
        const float many = dice.range(0.0f, 1.0f);
        const int count = 1 + static_cast<int>(many * 2.99f);          // one, two or three
        for (int i = 0; i < count; ++i) {
            // One roll per statement, each into its own name — see the note on the skyline below.
            const float ox = dice.range(-0.72f, 0.72f);
            const float oz = dice.range(-0.62f, 0.62f);
            const float sw = dice.range(0.0f, 1.0f);
            const float sh = dice.range(0.0f, 1.0f);
            const float sd = dice.range(0.0f, 1.0f);
            const float which = dice.range(0.0f, 1.0f);
            const float wide = 0.05f + sw * 0.14f;
            const float tall = 0.04f + sh * 0.20f;
            const float deep = 0.05f + sd * 0.12f;
            const render::Color c = which < 0.30f ? steelC : (which < 0.70f ? woodC : clothC);
            addProp(st, box(wide, tall, deep,
                            math::vec3(tp.x + ox * tp.halfW, tp.y + tall * 0.5f,
                                       tp.z + oz * tp.halfD),
                            c));
        }
    }

    // ---- the thing the story turns on -------------------------------------------------------------
    //
    // It has to be somewhere. An insert is a shot of an object, and before this the insert framing
    // pointed at a patch of empty floor.
    {
        // Bright and a size you can see. An insert is a shot whose entire job is to make one small
        // thing unmissable, so the object is the brightest thing in the room by some distance — which
        // is also how a props department would do it.
        const render::Color objC{
            static_cast<float>(0.45 + 0.55 * pal.accent.r / 255.0),
            static_cast<float>(0.45 + 0.55 * pal.accent.g / 255.0),
            static_cast<float>(0.45 + 0.55 * pal.accent.b / 255.0), 1.0f};
        const math::vec3 where(0.0f, 0.0f, 1.9f);
        addProp(st, stand(0.46f, 0.72f, 0.40f, where.x, where.z, rgb(pal.deep, 1.45))); // a plinth
        addProp(st, box(0.54f, 0.035f, 0.48f, math::vec3(where.x, 0.72f, where.z), rgb(pal.deep, 1.9)));
        // The object itself is built about the ORIGIN and left loose, so whoever has it can carry it.
        add(st.object, box(0.026f, 0.011f, 0.150f, math::vec3(0.0f, 0.0f, 0.0f), objC));
        add(st.object, box(0.058f, 0.011f, 0.058f, math::vec3(0.0f, 0.0f, -0.076f), objC));
        add(st.object, box(0.020f, 0.011f, 0.026f, math::vec3(0.023f, 0.0f, 0.055f), objC));
    }

    // ---- somewhere to sit ------------------------------------------------------------------------
    //
    // What it is depends on the room — a chair in an office, a stool at a bar, the edge of a bed on a
    // ward, a low wall outside — but there is always one, and it is always in roughly the same place
    // relative to the acting area, because that is where the camera is already pointed.
    {
        float seatH = 0.45f;
        if (set == "bar" || set == "kitchen") {
            seatH = 0.72f; // a stool
        } else if (set == "ward") {
            seatH = 0.58f; // the edge of a bed
        } else if (set == "chapel") {
            seatH = 0.44f; // a pew
        } else if (outdoors) {
            seatH = 0.40f; // a step, a low wall, a fallen trunk
        }
        const float sx = st.halfWidth < 1.6f ? -st.halfWidth * 0.52f : -1.02f;
        const float sz = 1.58f;
        st.seatAt = math::vec3(sx, seatH, sz);
        st.seatFacing = 3.14159265f;
        const render::Color seatC = outdoors ? material(0.50f, 0.49f, 0.46f, pal.deep, 0.40f, 1.0f)
                                             : woodC;
        if (outdoors) {
            // Outdoors it is a low wall or the end of a fallen trunk — a chair standing in a wood is
            // funnier than anything else in the film.
            addProp(st, box(1.45f, seatH, 0.42f, math::vec3(sx, seatH * 0.5f, sz), seatC));
        } else if (seatH > 0.60f) {
            addProp(st, post(0.17f, seatH, sx, sz, seatC, 10));
            addProp(st, box(0.40f, 0.045f, 0.40f, math::vec3(sx, seatH, sz), seatC));
        } else {
            addProp(st, box(0.48f, 0.055f, 0.46f, math::vec3(sx, seatH, sz), seatC));
            for (int i = 0; i < 4; ++i) {
                const float lx = sx + ((i & 1) ? 0.19f : -0.19f);
                const float lz = sz + ((i & 2) ? 0.18f : -0.18f);
                addProp(st, stand(0.045f, seatH - 0.03f, 0.045f, lx, lz, seatC));
            }
            if (set != "chapel") {
                // A back, which is what makes a chair read as a chair from across a room. Low enough
                // that it does not stand up behind a seated character's head like a post.
                addProp(st, box(0.48f, 0.38f, 0.05f,
                                 math::vec3(sx, seatH + 0.21f, sz + 0.205f), seatC));
            }
        }
    }

    // ---- the practical ----------------------------------------------------------------------------
    //
    // One lamp, in the room, with a falloff. What it is depends on what the room has: a corridor has
    // strip lights, a ward has a window, a chapel has candles, a bar has something over the counter.
    // Outdoors it is the sky itself coming through whatever is overhead — lower, wider and much
    // weaker, because a practical outdoors is a practical you can see the source of.
    {
        if (set == "corridor") {
            st.lampAt = math::vec3(0.0f, st.ceiling - 0.25f, 2.6f);
            st.lampStrength = 0.55f;
            st.lampReach = 3.0f;
        } else if (set == "ward" || set == "room" || set == "office") {
            st.lampAt = math::vec3(st.halfWidth * 0.72f, 1.55f, 2.9f); // a window, off to one side
            st.lampStrength = 0.42f;
            st.lampReach = 4.0f;
        } else if (set == "chapel") {
            st.lampAt = math::vec3(0.0f, 2.1f, st.depth - 0.6f);
            st.lampStrength = 0.60f;
            st.lampReach = 4.5f;
        } else if (set == "bar" || set == "kitchen") {
            st.lampAt = math::vec3(0.0f, 1.9f, 2.4f);
            st.lampStrength = 0.50f;
            st.lampReach = 2.8f;
        } else if (set == "ship" || set == "industrial" || set == "vehicle") {
            st.lampAt = math::vec3(-st.halfWidth * 0.6f, st.ceiling - 0.4f, 2.0f);
            st.lampStrength = 0.45f;
            st.lampReach = 3.2f;
        } else {
            // Outdoors: whatever is up there, a long way off and not much of it.
            st.lampAt = math::vec3(-2.4f, 5.5f, 5.0f);
            st.lampStrength = 0.22f;
            st.lampReach = 9.0f;
        }
    }

    // ---- the marks ------------------------------------------------------------------------------
    //
    // Two characters in a room stand a conversational distance apart — about 1.2m between them — and
    // both a little forward of centre so the room has depth BEHIND them. Standing them on the back
    // wall is what flattens a 3D set back into the painted one.
    const float apart = st.halfWidth < 2.0f ? 0.52f : 0.64f;
    st.markLeft = math::vec3(-apart, 0.0f, 1.05f);
    st.markRight = math::vec3(apart, 0.0f, 1.05f);
    st.objectAt = math::vec3(0.0f, 0.755f, 1.9f);

    // ---- and the set is worn ---------------------------------------------------------------------
    //
    // Last, over everything the set is made of at once, because it is sampled by where a vertex IS.
    // Two boxes that meet — a wall and its skirting, a counter and the floor under it — have to agree
    // about the dirt in the seam between them, and doing this per box would put a hard line down
    // every one of those joins.
    //
    // It runs over the props as well as the walls. A prop is small enough to have no vertices inside
    // it, so what it gets is not a texture across it but a single nudge of its own — which is exactly
    // what was wanted there too: five identical crates lit identically read as five copies of one
    // crate, and five crates a few per cent apart read as five crates.
    //
    // The held object is deliberately left out. It is the one thing in the film the audience is
    // asked to look at and recognise across a cut, and a fleck of dirt on it is a fleck of doubt.
    //
    // The seed is the film's. Two films get different walls; the same film gets the same wall in
    // every shot it appears in, which is the part that would be glaring if it were wrong.
    {
        render::Grain g;
        g.seed = seed * 2654435761u + 17u;
        // The close scale is 0.80 and not something finer, and that is not taste. The vertices are a
        // third of a metre apart, so a variation finer than two thirds of a metre has nowhere to be
        // recorded and averages itself away — asking for 0.38 here, which is what this had first,
        // simply threw that octave in the bin. Nothing about the picture said so; the numbers did.
        g.broad = 2.4f;
        g.close = 0.80f;
        // And the amount is modest on purpose. Pushed to three and a half times this it was still
        // barely visible at the size a film plays at, which is the honest measure of what vertex
        // colour can do on triangles this size: it varies one prop against the next and it puts dirt
        // in the corner where the wall meets the floor, and it does not make a surface look like a
        // material. That job belongs to the per-pixel grain in the rasteriser.
        g.amount = 0.12f;
        // The dirt low down is NOT done here any more. Per vertex it needed the walls cut into a
        // grid to land on at all; per pixel, in the rasteriser, it costs a subtraction and is sharp
        // at any distance. What is left here is the part vertex colour is actually good at: a box
        // with no vertices inside it gets one nudge of its own, so five identical crates stop being
        // five copies of one crate.
        g.low = 0.0f;
        render::grainMesh(st.mesh, g);
    }
    return st;
}

// ------------------------------------------------------------------------------------ the camera
//
// Where the camera goes for a framing, and what it does over the length of the shot.
//
// The framings are the ones the reel already speaks in, and each carries its own FOCAL LENGTH, because
// that is the half of framing that is not distance. A close-up shot on a wide lens from close up is not
// the same picture as a close-up shot on a long lens from further back: the first bends a face outward
// and pushes the room away behind it, and it is the single most common way an amateur close-up goes
// wrong. So the closer the shot, the longer the lens — a wide is about a 35mm, a mid a 50, a close-up
// an 85 — and the camera steps back to make up the difference, exactly as it would on a set.
struct Lens {
    math::vec3 eye{0.0f, 1.55f, -4.0f};
    math::vec3 at{0.0f, 1.35f, 0.0f};
    float fovY = 0.58f;
};

// One person, as the camera needs to know them: where they stand and how high up their eyes and chest
// are, which is all a lens has to be aimed with.
struct Subject {
    math::vec3 stand{0.0f, 0.0f, 0.0f};
    float eyeY = 1.66f;
    float chestY = 1.28f;
};

inline Lens lensFor(const Shot& shot, const Stage& st, const Subject& a, const Subject& b,
                    bool twoPeople, float progress, float seconds) {
    Lens L;
    const float t = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    const std::string& f = shot.framing;

    // Distance and focal length together. The pair is the framing; neither alone is.
    float dist = 4.2f;
    float fov = 0.58f;
    math::vec3 look = math::vec3((a.stand.x + b.stand.x) * 0.5f, a.chestY,
                                 (a.stand.z + b.stand.z) * 0.5f);
    float eyeHeight = 1.55f;

    if (f == "wide") {
        dist = 5.6f;
        fov = 0.62f;                  // about a 35mm
        look.y = a.chestY * 0.80f;
    } else if (f == "mid") {
        dist = 2.75f;
        fov = 0.50f;                  // 50mm
        look.y = a.chestY + 0.16f;
    } else if (f == "close") {
        dist = 1.35f;
        fov = 0.36f;                  // 85mm: a face on a long lens, which is how a face is shot
        look = math::vec3(a.stand.x, a.eyeY - 0.03f, a.stand.z);
        eyeHeight = a.eyeY;
    } else if (f == "two") {
        dist = 3.5f;
        fov = 0.66f;
        look.y = a.chestY + 0.10f;
    } else if (f == "ots") {
        // Over the shoulder: the camera stands behind and outside the NEAR person's shoulder, looking
        // past them at the other. The near one is meant to be a dark mass at the edge of frame, so the
        // camera is close to them and their far side is what is in focus.
        const math::vec3 nearAt = b.stand;
        const math::vec3 farAt = a.stand;
        math::vec3 toFar = farAt - nearAt;
        const float len = std::sqrt(toFar.x * toFar.x + toFar.z * toFar.z);
        toFar = len > 1e-4f ? math::vec3(toFar.x / len, 0.0f, toFar.z / len)
                            : math::vec3(1.0f, 0.0f, 0.0f);
        const math::vec3 side(-toFar.z, 0.0f, toFar.x);
        L.eye = nearAt - toFar * 0.95f + side * 0.46f + math::vec3(0.0f, b.eyeY + 0.06f, 0.0f);
        L.at = math::vec3(farAt.x, a.eyeY - 0.06f, farAt.z);
        L.fovY = 0.46f;
        return L;
    } else if (f == "insert") {
        dist = 0.58f;
        fov = 0.48f;
        look = st.objectAt;
        eyeHeight = st.objectAt.y + 0.18f;
    } else if (f == "low") {
        dist = 3.0f;
        fov = 0.62f;
        eyeHeight = 0.52f;            // on the floor, looking up: the shot that makes somebody loom
        look.y = a.chestY + 0.25f;
    }
    if (!twoPeople) {
        look.x = a.stand.x;
        look.z = a.stand.z;
    }

    // ---- the move ---------------------------------------------------------------------------------
    //
    // A move is a change over the length of the shot, so it is expressed against `progress` rather than
    // against the clock: a shot that is cut shorter moves less, not faster.
    float slideEye = 0.0f;
    float slideLook = 0.0f;
    float shake = 0.0f;
    if (shot.camera == "push") {
        dist *= 1.0f - 0.16f * t;
    } else if (shot.camera == "push-slow") {
        dist *= 1.0f - 0.07f * t;
    } else if (shot.camera == "pull") {
        dist *= 1.0f + 0.20f * t;
    } else if (shot.camera == "pan-l" || shot.camera == "pan-r") {
        slideLook = (shot.camera == "pan-l" ? -1.0f : 1.0f) * 0.85f * (t - 0.5f);
    } else if (shot.camera == "track-l" || shot.camera == "track-r") {
        const float d = (shot.camera == "track-l" ? -1.0f : 1.0f) * 1.15f * (t - 0.5f);
        slideEye = d;
        slideLook = d * 0.35f;
    } else if (shot.camera == "handheld") {
        shake = 1.0f;
    } else if (shot.camera == "whip") {
        const float u = t < 0.5f ? 0.0f : (t - 0.5f) * 2.0f;
        slideEye = 2.4f * u * u;
        slideLook = 3.0f * u * u;
    }

    L.eye = math::vec3(look.x + slideEye, eyeHeight, look.z - dist);
    L.at = math::vec3(look.x + slideLook, look.y, look.z);
    L.fovY = fov;

    if (shake > 0.0f) {
        // Handheld: a low, irregular drift rather than a jitter. Two frequencies that do not divide
        // into each other, or it reads as a mechanical wobble instead of a person holding a camera.
        const float w = seconds;
        L.eye.x += 0.035f * (std::sin(w * 2.3f) + 0.6f * std::sin(w * 5.7f));
        L.eye.y += 0.022f * (std::sin(w * 1.9f + 1.1f) + 0.5f * std::sin(w * 4.3f));
        L.at.x += 0.045f * std::sin(w * 1.7f + 0.4f);
        L.at.y += 0.030f * std::sin(w * 2.9f + 2.0f);
    }

    // A film set has no fourth wall: the camera stands where it would be, which for a wide shot is
    // outside the room entirely, looking in between the side walls. Clamping it to the inside of the
    // room turns every wide into a mid — which is what the first contact sheet showed, a film with no
    // wide shots in it at all.
    const float frontWall = -9.0f;
    if (L.eye.z < frontWall) {
        L.eye.z = frontWall;
    }
    if (L.eye.y < 0.22f) {
        L.eye.y = 0.22f;
    }
    if (st.ceiling > 0.0f && L.eye.y > st.ceiling - 0.15f) {
        L.eye.y = st.ceiling - 0.15f;
    }
    return L;
}

} // namespace maz::film
