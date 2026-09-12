#pragma once

#include "maz/film/Palette.hpp"
#include "maz/film/Reel.hpp"
#include "maz/math/Math.hpp"
#include "maz/render/MeshMerge.hpp"
#include "maz/render/MeshTransform.hpp"
#include "maz/render/Shapes.hpp"
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
    float halfWidth = 5.0f;  // metres either side of centre
    float depth = 7.0f;      // metres from the front of the stage to the back wall
    float ceiling = 2.9f;    // metres; 0 means outdoors, with no ceiling and no walls
    bool indoors = true;
    // The marks the actors stand on, left of frame and right of frame.
    math::vec3 markLeft{-0.62f, 0.0f, 0.0f};
    math::vec3 markRight{0.62f, 0.0f, 0.0f};
    // Where the thing the story turns on sits when nobody is holding it.
    math::vec3 objectAt{0.0f, 0.9f, 0.6f};
};

namespace stagedetail {

inline render::Color rgb(const Rgb& c, double scale = 1.0) {
    return render::Color{static_cast<float>(c.r * scale / 255.0), static_cast<float>(c.g * scale / 255.0),
                         static_cast<float>(c.b * scale / 255.0), 1.0f};
}

inline void add(render::shapes::MeshData& into, const render::shapes::MeshData& part) {
    into = render::mergeMeshes(into, part);
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
        add(m, box(halfW * 2.0f, height, 0.12f, math::vec3(0.0f, height * 0.5f, z), c));
        return m;
    }
    const float leftW = (doorX - dw * 0.5f) + halfW;
    const float rightW = halfW - (doorX + dw * 0.5f);
    add(m, box(leftW, height, 0.12f, math::vec3(-halfW + leftW * 0.5f, height * 0.5f, z), c));
    add(m, box(rightW, height, 0.12f, math::vec3(halfW - rightW * 0.5f, height * 0.5f, z), c));
    add(m, box(dw, height - dh, 0.12f, math::vec3(doorX, dh + (height - dh) * 0.5f, z), c));
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

// Build the room this shot happens in.
inline Stage buildStage(const Shot& shot, const Palette& pal, std::uint32_t seed) {
    using namespace stagedetail;
    Stage st;
    Dice dice(seed * 2654435761u + static_cast<std::uint32_t>(shot.scene) * 40503u + 17u);

    // Walls and floor are pulled apart in value on purpose. A room built out of one shade of the
    // palette photographs as a single dark mass with a person standing in front of it; the eye needs
    // the floor, the walls and the ceiling to be three different values before it will read a room.
    const render::Color floorC = rgb(pal.deep, 0.95);
    const render::Color wallC = rgb(pal.deep, 1.55);
    const render::Color inkC = rgb(pal.ink, 1.0);
    const render::Color accentC = rgb(pal.accent, 1.0);
    const render::Color skyC = rgb(pal.sky, 1.0);

    const std::string& set = shot.set;
    const bool outdoors = set == "woods" || set == "field" || set == "street" || set == "water" ||
                          set == "lighthouse";
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
    } else {
        st.halfWidth = 9.0f;
        st.depth = 22.0f;
        st.ceiling = 0.0f;
    }

    // ---- floor, walls, ceiling -------------------------------------------------------------------
    {
        const float w = st.halfWidth * 2.0f + 0.4f;
        add(st.mesh, box(w, 0.12f, st.depth + 4.0f, math::vec3(0.0f, -0.06f, st.depth * 0.5f - 1.5f),
                         floorC));
        if (st.indoors) {
            const float h = st.ceiling;
            add(st.mesh, wallWithDoor(st.halfWidth, h, st.depth, dice.range(-0.5f, 0.5f) * st.halfWidth,
                                      wallC, set != "chapel"));
            add(st.mesh, box(0.12f, h, st.depth + 3.0f,
                             math::vec3(-st.halfWidth, h * 0.5f, st.depth * 0.5f - 1.0f), wallC));
            add(st.mesh, box(0.12f, h, st.depth + 3.0f,
                             math::vec3(st.halfWidth, h * 0.5f, st.depth * 0.5f - 1.0f), wallC));
            add(st.mesh, box(st.halfWidth * 2.0f, 0.10f, st.depth + 3.0f,
                             math::vec3(0.0f, h, st.depth * 0.5f - 1.0f), rgb(pal.shadow, 1.6)));
            // A skirting line, which is most of what tells you a wall is a wall and not a backdrop.
            add(st.mesh, box(st.halfWidth * 2.0f, 0.11f, 0.02f, math::vec3(0.0f, 0.055f, st.depth - 0.07f),
                             inkC));
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
            for (int i = 0; i < 14; ++i) {
                const float x = dice.range(-st.halfWidth * 2.2f, st.halfWidth * 2.2f);
                const float z = dice.range(st.depth * 0.45f, st.depth + 12.0f);
                if (set == "woods") {
                    add(st.mesh, post(dice.range(0.10f, 0.26f), dice.range(5.0f, 11.0f), x, z, inkC, 7));
                } else if (set == "street") {
                    add(st.mesh, stand(dice.range(3.0f, 7.0f), dice.range(4.0f, 9.0f),
                                       dice.range(3.0f, 6.0f), x * 1.6f, z + 6.0f, inkC));
                } else {
                    add(st.mesh, stand(dice.range(0.6f, 1.6f), dice.range(0.3f, 1.1f),
                                       dice.range(0.6f, 1.6f), x, z, inkC));
                }
            }
            if (set == "lighthouse") {
                add(st.mesh, post(1.7f, 16.0f, dice.range(-3.0f, 3.0f), st.depth + 6.0f, wallC, 14));
            }
        }
    }

    // ---- what is in it ---------------------------------------------------------------------------
    if (set == "office") {
        add(st.mesh, table(1.55f, 0.78f, dice.range(-0.5f, 0.5f), 2.3f, rgb(pal.deep, 1.35), inkC));
        add(st.mesh, chair(0.1f, 3.1f, 0.15f, inkC));
        add(st.mesh, stand(0.45f, 1.85f, 0.42f, -st.halfWidth + 0.5f, 3.4f, inkC)); // a filing cabinet
    } else if (set == "kitchen" || set == "bar") {
        const float counterH = set == "bar" ? 1.06f : 0.92f;
        add(st.mesh, stand(st.halfWidth * 1.3f, counterH, 0.62f, 0.0f, 2.6f, rgb(pal.deep, 1.30f)));
        add(st.mesh, box(st.halfWidth * 1.34f, 0.05f, 0.70f, math::vec3(0.0f, counterH, 2.6f), inkC));
        for (int i = 0; i < 3; ++i) {
            add(st.mesh, post(0.19f, 0.74f, -1.3f + static_cast<float>(i) * 1.3f, 1.75f, inkC, 9));
        }
    } else if (set == "ward") {
        for (int i = 0; i < 2; ++i) {
            const float x = i == 0 ? -1.8f : 1.8f;
            add(st.mesh, stand(0.95f, 0.60f, 2.05f, x, 3.2f, rgb(pal.sky, 0.9)));
            add(st.mesh, post(0.035f, 1.75f, x + 0.62f, 2.5f, inkC, 7)); // a drip stand
        }
    } else if (set == "chapel") {
        for (int i = 0; i < 5; ++i) {
            const float z = 2.2f + static_cast<float>(i) * 1.15f;
            add(st.mesh, stand(st.halfWidth * 1.1f, 0.44f, 0.36f, 0.0f, z, inkC));
            add(st.mesh, box(st.halfWidth * 1.1f, 0.55f, 0.07f, math::vec3(0.0f, 0.72f, z - 0.17f), inkC));
        }
        add(st.mesh, box(0.10f, 1.9f, 0.10f, math::vec3(0.0f, 3.6f, st.depth - 0.2f), accentC));
        add(st.mesh, box(0.85f, 0.10f, 0.10f, math::vec3(0.0f, 4.05f, st.depth - 0.2f), accentC));
    } else if (set == "ship" || set == "industrial") {
        for (int i = 0; i < 4; ++i) {
            const float z = 1.4f + static_cast<float>(i) * 1.8f;
            add(st.mesh, post(0.13f, st.ceiling, -st.halfWidth + 0.35f, z, inkC, 8));
            add(st.mesh, post(0.13f, st.ceiling, st.halfWidth - 0.35f, z, inkC, 8));
            add(st.mesh, box(0.16f, 0.16f, st.halfWidth * 2.0f,
                             math::vec3(0.0f, st.ceiling - 0.35f, z), inkC));
        }
        add(st.mesh, stand(0.8f, 1.1f, 0.8f, dice.range(-1.2f, 1.2f), 4.2f, rgb(pal.deep, 1.4)));
    } else if (set == "corridor") {
        for (int i = 0; i < 6; ++i) {
            const float z = 1.1f + static_cast<float>(i) * 1.85f;
            // Doors down both sides, which is what a corridor IS, and what makes its length read.
            add(st.mesh, box(0.06f, 2.05f, 0.92f, math::vec3(-st.halfWidth + 0.09f, 1.025f, z), inkC));
            add(st.mesh, box(0.06f, 2.05f, 0.92f, math::vec3(st.halfWidth - 0.09f, 1.025f, z), inkC));
            add(st.mesh, box(0.55f, 0.05f, 0.16f, math::vec3(0.0f, st.ceiling - 0.06f, z + 0.5f),
                             rgb(pal.key, 1.0)));
        }
    } else if (set == "room") {
        add(st.mesh, stand(1.85f, 0.72f, 0.85f, dice.range(-1.0f, 1.0f), 3.4f, rgb(pal.deep, 1.3)));
        add(st.mesh, table(1.05f, 0.55f, dice.range(-0.8f, 0.8f), 1.9f, rgb(pal.deep, 1.35), inkC));
    } else if (set == "vehicle") {
        add(st.mesh, stand(0.52f, 0.95f, 0.55f, -0.55f, 1.1f, inkC));
        add(st.mesh, stand(0.52f, 0.95f, 0.55f, 0.55f, 1.1f, inkC));
        add(st.mesh, box(st.halfWidth * 1.9f, 0.9f, 0.08f, math::vec3(0.0f, 1.45f, 2.6f), skyC));
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
        add(st.mesh, stand(0.46f, 0.72f, 0.40f, where.x, where.z, rgb(pal.deep, 1.45))); // a plinth
        add(st.mesh, box(0.54f, 0.035f, 0.48f, math::vec3(where.x, 0.72f, where.z), rgb(pal.deep, 1.9)));
        add(st.mesh, box(0.026f, 0.011f, 0.150f, math::vec3(where.x, 0.749f, where.z), objC));
        add(st.mesh, box(0.058f, 0.011f, 0.058f,
                         math::vec3(where.x, 0.749f, where.z - 0.076f), objC));
        add(st.mesh, box(0.020f, 0.011f, 0.026f,
                         math::vec3(where.x + 0.023f, 0.749f, where.z + 0.055f), objC));
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
