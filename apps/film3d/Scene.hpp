#pragma once

#include "maz/film/Business.hpp"
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

    // ---- what they wear, and what their hair does -------------------------------------------------
    //
    // The silhouette is who somebody is at the distance a film watches people from — you know which
    // one is which across a car park, in the dark, long before you can read a face. So this comes off
    // the ROLE first, because the role is the one thing the film actually knows about them, and off
    // the name only where the role says nothing: a nurse is in scrubs with her hair up because that is
    // what you have to do to work a ward, and the stranger who turns up at midnight is in a coat.
    {
        auto is = [&](const char* word) { return mentions(who.role, word); };
        const bool medical = is("nurse") || is("doctor") || is("medic") || is("surgeon");
        const bool uniformed = is("officer") || is("guard") || is("soldier") || is("police") ||
                               is("driver") || is("porter");
        const bool outerwear = is("stranger") || is("detective") || is("priest") || is("inspector") ||
                               is("traveller") || is("keeper");
        const std::uint32_t roll = h >> 19;
        if (medical) {
            c.build.sleeve = 0.55f;   // scrubs: short sleeves, nothing that can catch on anything
            c.build.coatY = 0.0f;
            c.build.hairY = 0.0f;     // and hair up, which is not a style choice on a ward
            c.build.fringe = 0.25f;
        } else if (uniformed) {
            c.build.sleeve = 1.55f;
            c.build.coatY = 0.52f;    // a tunic, cut at the hip
            c.build.hairY = 0.0f;
            c.build.fringe = 0.15f;
        } else if (outerwear) {
            c.build.sleeve = 1.55f;
            c.build.coatY = 0.30f + static_cast<float>(roll % 9u) * 0.012f; // a coat, to the knee
            c.build.fringe = 0.55f;
        } else if (young) {
            c.build.sleeve = 0.85f;
            c.build.fringe = 0.95f;   // children have fringes, and it is most of what says child
            c.build.hairY = (roll & 1u) ? 0.0f : 0.70f;
        } else {
            // Nobody in particular, so spread them out: some in a jacket, some not, sleeves long or
            // short, hair up or down. A cast all dressed the same is a cast of extras.
            c.build.sleeve = (roll & 1u) ? 1.55f : 0.50f;
            c.build.coatY = (roll & 2u) ? 0.50f : 0.0f;
            c.build.fringe = 0.15f + static_cast<float>(roll % 7u) * 0.11f;
            c.build.hairY = 0.0f;
        }
        // Hair down is a woman's silhouette here more often than a man's, which is a generalisation
        // and is also what an audience reads; either way it is the LENGTH that separates two people
        // standing together, not the colour. It is decided last, and skips only the two roles where
        // hair up is not a style choice — a ward and a uniform both require it.
        const bool longHair = ((roll >> 4) & 3u) != 0u;
        if (!medical && !uniformed && c.build.heads > 7.0f && c.build.height < 1.72f && longHair) {
            c.build.hairY = 0.62f + static_cast<float>(roll % 11u) * 0.008f;
        }
        if (c.build.height < 1.72f && c.build.heads > 7.0f && ((roll >> 6) & 3u) == 0u) {
            c.build.skirtY = 0.38f + static_cast<float>(roll % 5u) * 0.03f;
        }
    }

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
    // Skin reflects somewhere between a third and two thirds of what falls on it. It used to be as
    // much as 0.97 here, which is paper, and it did not matter while the LIGHT carried the hour in its
    // own darkness — now that a daylight key is a daylight key, a face at 0.97 prints as a white oval.
    const float skinTone = 0.40f + static_cast<float>((h >> 17) % 30u) / 100.0f;
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
    maz::film::Face face;
    maz::film::Business business;
    bool speaking = false;
};

struct Scene {
    Stage stage;
    std::vector<Standing> people;
    Lens lens;
    maz::film::Palette palette;

    // Where the thing the story turns on is, this instant — on its plinth, or in somebody's hand.
    // Before this it was welded to the plinth for the whole film, which made it scenery rather than
    // the thing everybody in the film is arguing about.
    math::mat4 objectAt{1.0f};
    bool objectShown = true;
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
