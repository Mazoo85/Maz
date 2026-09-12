#pragma once

#include "maz/film/Perform.hpp"
#include "maz/film/Stage.hpp"
#include "maz/render/SoftRaster.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// film3d — a reel, staged in three dimensions and photographed.
//
// The flat renderer paints a shot: three planes, figures drawn between them, and a camera that is a
// scale and an offset. This STAGES one — a room with a floor and walls, bodies standing on marks in
// it, and a camera that is somewhere in the room with a focal length. Everything that then looks
// right looks right because it IS right: a figure is behind the table because they are standing behind
// the table, and one side of a face is dark because the key is on the other side of it.
//
// It reads the same .reel.json the flat renderer reads, and makes the same cuts at the same moments,
// because a film rendered two ways has to be one film.
namespace film3d {

namespace math = maz::math;

using maz::film::Build;
using maz::film::Lens;
using maz::film::Motive;
using maz::film::Reel;
using maz::film::Shot;
using maz::film::Skeleton;
using maz::film::Stage;
using maz::film::Subject;

// ------------------------------------------------------------------------------------ casting
//
// Who a character is, physically. Derived from their name and role so the same film casts the same
// people every time, and two characters in the same film are never the same person.
struct Cast {
    Build build;
    std::string name;
    int side = -1; // -1 stands left of frame, +1 right
};

inline std::uint32_t hashName(const std::string& s) {
    std::uint32_t h = 2166136261u;
    for (char ch : s) {
        h ^= static_cast<std::uint32_t>(static_cast<unsigned char>(ch));
        h *= 16777619u;
    }
    return h;
}

inline bool mentions(const std::string& hay, const char* needle) {
    std::string low;
    for (char ch : hay) {
        low += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return low.find(needle) != std::string::npos;
}

inline Cast castFor(const maz::film::Character& who, const maz::film::Voice* voice) {
    const std::uint32_t h = hashName(who.name);
    Cast c;
    c.name = who.name;
    c.side = voice != nullptr && voice->side != 0 ? voice->side : ((h & 1u) ? 1 : -1);

    const bool young = mentions(who.role, "child") || mentions(who.role, "boy") ||
                       mentions(who.role, "girl") || mentions(who.role, "kid") ||
                       mentions(who.role, "daughter") || mentions(who.role, "son");
    if (young) {
        c.build = maz::film::child();
    } else if ((h >> 3) & 1u) {
        c.build = maz::film::adultFemale();
    } else {
        c.build = maz::film::adultMale();
    }
    // Height varies by a few centimetres either way: a cast in which everyone is exactly the average
    // height of their build is a cast of clones, and it shows the moment two of them share a frame.
    c.build.height *= 0.955f + static_cast<float>((h >> 7) % 90u) / 1000.0f;

    // Their clothes, off their own name, in colours that stay separable in a dark room.
    const float hue = static_cast<float>((h >> 11) % 360u);
    auto fromHue = [](float deg, float sat, float val) {
        const float hh = deg / 60.0f;
        const int i = static_cast<int>(hh) % 6;
        const float ff = hh - std::floor(hh);
        const float p = val * (1.0f - sat);
        const float q = val * (1.0f - sat * ff);
        const float t = val * (1.0f - sat * (1.0f - ff));
        switch (i) {
            case 0: return maz::render::Color{val, t, p, 1.0f};
            case 1: return maz::render::Color{q, val, p, 1.0f};
            case 2: return maz::render::Color{p, val, t, 1.0f};
            case 3: return maz::render::Color{p, q, val, 1.0f};
            case 4: return maz::render::Color{t, p, val, 1.0f};
            default: return maz::render::Color{val, p, q, 1.0f};
        }
    };
    c.build.top = fromHue(hue, 0.30f, 0.42f);
    c.build.legwear = fromHue(hue + 18.0f, 0.16f, 0.22f);
    const float skinTone = 0.52f + static_cast<float>((h >> 17) % 45u) / 100.0f;
    c.build.skin = maz::render::Color{skinTone, skinTone * 0.80f, skinTone * 0.67f, 1.0f};
    const bool fair = ((h >> 23) & 3u) == 0u;
    c.build.hair = fair ? maz::render::Color{0.44f, 0.34f, 0.20f, 1.0f}
                        : maz::render::Color{0.13f, 0.10f, 0.09f, 1.0f};
    return c;
}

// ------------------------------------------------------------------------------------ the scene
//
// Everything the camera will see at one instant: the room, and everybody in it, already posed.
struct Standing {
    const Cast* who = nullptr;
    Skeleton skeleton;
    maz::film::BodyPose pose;
    bool speaking = false;
};

struct Scene {
    Stage stage;
    std::vector<Standing> people;
    Lens lens;
    maz::film::Palette palette;
};

// Who is where. At most two people are placed on marks; anyone else stands further back, because a
// third person in a two-hander stands upstage, and because two marks is what a room has.
inline math::vec3 markFor(const Stage& st, int slot, int total) {
    if (total <= 1) {
        return math::vec3(0.0f, 0.0f, st.markLeft.z);
    }
    if (slot == 0) {
        return st.markLeft;
    }
    if (slot == 1) {
        return st.markRight;
    }
    const float x = (slot % 2 == 0 ? -1.0f : 1.0f) * (1.5f + 0.4f * static_cast<float>(slot));
    return math::vec3(x, 0.0f, st.markLeft.z + 1.6f);
}

} // namespace film3d
