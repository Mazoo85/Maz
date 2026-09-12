#pragma once

#include "maz/film/Canvas.hpp"
#include "maz/film/Noise.hpp"
#include "maz/film/Palette.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <vector>

// maz::film -- how this film has treated this room.
//
// Fifteen sets drawn identically every time is the one thing you cannot un-notice once you have seen
// two films. film/js/set-dress.js is the browser's answer: a layer of dressing seeded from the story,
// drawn over whatever the set painter drew, so one layer varies all fifteen at once.
//
// This is that layer in C++, and the reason it exists as a port rather than as a second design is the
// rule the whole native renderer lives by: AGREE WITH THE BROWSER. A room the browser dresses as
// abandoned and the engine dresses as kept is not the same film rendered twice.
//
// The dial is condition -- kept, worn, abandoned -- because what makes a room look like a different
// room is not where the furniture is, it is how the room has been treated. Genre leans on it and the
// seed settles it.
//
// WHERE IT MAY DRAW is the whole safety story, and it is the same story here as there: set painters
// put their furniture in the middle and figures stand between x=250 and x=660 of a 1000-unit stage, so
// dressing keeps to the outer bands and the floor line. A prop's box is a promise -- x leaves room for
// w, and every shape draws inside {x, y, w, h} -- because a crate placed inside the left band and
// fifty units wide reaches into the acting area and grows out of somebody's chest.
namespace maz::film {

inline constexpr float kStageW = 1000.0f;
inline constexpr float kStageH = 420.0f;

inline constexpr float kDressLeftLo = 10.0f;
inline constexpr float kDressLeftHi = 200.0f;
inline constexpr float kDressRightLo = 770.0f;
inline constexpr float kDressRightHi = 975.0f;
inline constexpr float kDressFloorY = 352.0f;
inline constexpr float kDressWallLo = 60.0f;
inline constexpr float kDressWallHi = 240.0f;

enum class Condition { Kept, Worn, Abandoned };

inline const char* conditionName(Condition c) {
    switch (c) {
        case Condition::Kept: return "kept";
        case Condition::Worn: return "worn";
        case Condition::Abandoned: return "abandoned";
    }
    return "worn";
}

enum class PropKind { Crate, Stack, Plank, Hanging, Post, Rock, Scrub };

inline const char* propName(PropKind k) {
    switch (k) {
        case PropKind::Crate: return "crate";
        case PropKind::Stack: return "stack";
        case PropKind::Plank: return "plank";
        case PropKind::Hanging: return "hanging";
        case PropKind::Post: return "post";
        case PropKind::Rock: return "rock";
        case PropKind::Scrub: return "scrub";
    }
    return "crate";
}

struct Prop {
    PropKind kind = PropKind::Crate;
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;
};

struct Mark {
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;
    double alpha = 0.0;
};

struct Lamp {
    bool lit = false;
    double x = 0.0;
    double y = 0.0;
    double r = 0.0;
};

struct Dressing {
    Condition condition = Condition::Worn;
    bool outdoors = false;
    std::vector<Prop> props;
    std::vector<Mark> marks;
    Lamp lamp;
};

namespace detail {

// How each genre treats a room: weights over kept, worn, abandoned.
inline const std::map<std::string, std::array<int, 3>>& dressWeights() {
    static const std::map<std::string, std::array<int, 3>> kWeights = {
        {"horror", {0, 1, 4}},   {"thriller", {1, 3, 3}}, {"mystery", {1, 3, 2}},
        {"scifi", {3, 3, 2}},    {"western", {1, 3, 3}},  {"heist", {2, 4, 1}},
        {"drama", {2, 4, 1}},    {"fantasy", {2, 3, 2}},  {"romance", {4, 3, 1}},
        {"comedy", {4, 3, 1}}
    };
    return kWeights;
}

// A crate in a field is a mistake, not a mood.
inline bool dressOutdoors(const std::string& setKey) {
    return setKey == "woods" || setKey == "street" || setKey == "field" || setKey == "water";
}

struct Amount {
    int propsLo;
    int propsHi;
    int marksLo;
    int marksHi;
    double lamp;
};

inline Amount amountFor(Condition c) {
    switch (c) {
        case Condition::Kept: return {1, 2, 0, 0, 0.8};
        case Condition::Worn: return {2, 4, 1, 2, 0.45};
        case Condition::Abandoned: return {3, 5, 2, 3, 0.05};
    }
    return {2, 4, 1, 2, 0.45};
}

inline double between(Rng& rng, double lo, double hi) { return lo + rng.next() * (hi - lo); }

// The browser writes Math.floor(lo + rng() * (hi - lo + 1)); floor on a value that cannot be negative
// here, so std::floor matches.
inline int intBetween(Rng& rng, int lo, int hi) {
    return static_cast<int>(std::floor(static_cast<double>(lo) +
                                       rng.next() * static_cast<double>(hi - lo + 1)));
}

} // namespace detail

inline Condition conditionFor(const std::string& genre, std::uint32_t seed) {
    const auto& table = detail::dressWeights();
    const auto it = table.find(genre);
    const std::array<int, 3>& weights = it == table.end() ? table.at("drama") : it->second;
    const int total = weights[0] + weights[1] + weights[2];
    Rng rng((hashText("dress:condition") ^ seed) & 0xFFFFFFFFu);
    const double roll = rng.next() * static_cast<double>(total);
    double acc = 0.0;
    const Condition order[3] = {Condition::Kept, Condition::Worn, Condition::Abandoned};
    for (int i = 0; i < 3; ++i) {
        acc += static_cast<double>(weights[static_cast<std::size_t>(i)]);
        if (roll < acc) {
            return order[i];
        }
    }
    return Condition::Worn;
}

// Everything this film puts in this set. Pure data -- no canvas -- so the decisions can be checked
// against the browser's without drawing any of them.
inline Dressing dressingFor(const std::string& setKey, const std::string& genre, std::uint32_t seed) {
    Dressing out;
    out.condition = conditionFor(genre, seed);
    out.outdoors = detail::dressOutdoors(setKey);
    const detail::Amount amount = detail::amountFor(out.condition);

    static const PropKind kIndoor[4] = {PropKind::Crate, PropKind::Stack, PropKind::Plank,
                                        PropKind::Hanging};
    static const PropKind kOutdoor[4] = {PropKind::Post, PropKind::Rock, PropKind::Scrub,
                                         PropKind::Plank};
    const PropKind* pool = out.outdoors ? kOutdoor : kIndoor;

    // Keyed on the set as well as the film, so the two rooms of one film are dressed differently from
    // each other -- the bug one level up from the one this fixes.
    Rng rng((hashText("dress:" + setKey) ^ seed) & 0xFFFFFFFFu);

    const int count = detail::intBetween(rng, amount.propsLo, amount.propsHi);
    for (int i = 0; i < count; ++i) {
        const PropKind kind = pool[static_cast<std::size_t>(rng.next() * 4.0) % 4u];
        // Alternate sides so a room is not all piled into one corner.
        const double bandLo = (i % 2 == 0) ? kDressLeftLo : kDressRightLo;
        const double bandHi = (i % 2 == 0) ? kDressLeftHi : kDressRightHi;
        double w = 0.0;
        double h = 0.0;
        if (kind == PropKind::Hanging) {
            w = detail::between(rng, 10.0, 26.0);
            h = detail::between(rng, 90.0, 230.0);
        } else {
            w = detail::between(rng, 26.0, 64.0);
            h = detail::between(rng, 18.0, 52.0);
        }
        Prop prop;
        prop.kind = kind;
        prop.w = w;
        prop.h = h;
        prop.x = detail::between(rng, bandLo, std::max(bandLo, bandHi - w));
        prop.y = kind == PropKind::Hanging ? 0.0 : (kDressFloorY - h);
        out.props.push_back(prop);
    }

    const int markCount = out.outdoors ? 0 : detail::intBetween(rng, amount.marksLo, amount.marksHi);
    for (int m = 0; m < markCount; ++m) {
        const double bandLo = (m % 2 == 0) ? kDressLeftLo : kDressRightLo;
        const double bandHi = (m % 2 == 0) ? kDressLeftHi : kDressRightHi;
        Mark mark;
        mark.w = detail::between(rng, 30.0, 90.0);
        mark.h = detail::between(rng, 40.0, 120.0);
        mark.x = detail::between(rng, bandLo, std::max(bandLo, bandHi - mark.w));
        mark.y = detail::between(rng, kDressWallLo, kDressWallHi);
        mark.alpha = detail::between(rng, 0.1, 0.26);
        out.marks.push_back(mark);
    }

    if (!out.outdoors && rng.next() < amount.lamp) {
        const bool left = rng.next() < 0.5;
        const double bandLo = left ? kDressLeftLo : kDressRightLo;
        const double bandHi = left ? kDressLeftHi : kDressRightHi;
        out.lamp.lit = true;
        out.lamp.x = detail::between(rng, bandLo + 30.0, bandHi - 30.0);
        out.lamp.y = detail::between(rng, 150.0, 250.0);
        out.lamp.r = detail::between(rng, 40.0, 90.0);
    }
    return out;
}

namespace detail {

inline void drawProp(Canvas& c, const Palette& pal, const Prop& prop) {
    const float x = static_cast<float>(prop.x);
    const float y = static_cast<float>(prop.y);
    const float w = static_cast<float>(prop.w);
    const float h = static_cast<float>(prop.h);

    if (prop.kind == PropKind::Hanging) {
        c.setFill(pal.ink, 0.85f);
        c.fillRect(x, y, w, h);
        return;
    }
    if (prop.kind == PropKind::Rock || prop.kind == PropKind::Scrub) {
        c.setFill(pal.ink, prop.kind == PropKind::Scrub ? 0.7f : 0.9f);
        c.beginPath();
        // The browser draws the TOP half of the ellipse (Math.PI to 0), which is what makes a rock sit
        // on the floor rather than sink through it.
        c.arcTo(x + w * 0.5f, y + h, w * 0.5f, 3.14159265f, 6.28318531f);
        c.closePath();
        c.fill();
        return;
    }
    if (prop.kind == PropKind::Post) {
        // Tall and thin, rising out of the top of its box -- height is free, width is not.
        c.setFill(pal.ink, 0.9f);
        c.fillRect(x + w * 0.3f, y - h * 1.4f, w * 0.3f, h * 2.4f);
        return;
    }
    if (prop.kind == PropKind::Plank) {
        // Leaning, which is what tells you nobody put it away. A quad rather than a rotation, so the
        // lean is bounded by the box's own width instead of by however far a rotation threw it.
        const float bx = x;
        const float by = y + h;
        const float topX = x + w * 0.62f;
        const float topY = y - h * 1.1f;
        const float t = w * 0.3f;
        c.setFill(pal.ink, 0.88f);
        c.beginPath();
        c.moveTo(bx, by);
        c.lineTo(bx + t, by);
        c.lineTo(topX + t, topY);
        c.lineTo(topX, topY);
        c.closePath();
        c.fill();
        return;
    }
    // crate, and stack: the same box, once or twice.
    c.setFill(pal.ink, 0.92f);
    c.fillRect(x, y, w, h);
    if (prop.kind == PropKind::Stack) {
        c.setFill(pal.ink, 0.8f);
        c.fillRect(x + w * 0.15f, y - h * 0.72f, w * 0.7f, h * 0.72f);
    }
}

} // namespace detail

// Draw the dressing onto the mid plane, after the set painter and before the actors.
inline void drawDressing(Canvas& c, const Palette& pal, const Dressing& dressing) {
    // Damp, soot, the shape something used to hang in: the cheapest signal that a room has a history.
    for (const Mark& mark : dressing.marks) {
        c.setFill(pal.shadow, static_cast<float>(mark.alpha));
        c.beginPath();
        c.ellipse(static_cast<float>(mark.x + mark.w * 0.5), static_cast<float>(mark.y),
                  static_cast<float>(mark.w * 0.5), static_cast<float>(mark.h * 0.5));
        c.fill();
    }

    if (dressing.lamp.lit) {
        const float lx = static_cast<float>(dressing.lamp.x);
        const float ly = static_cast<float>(dressing.lamp.y);
        const float lr = static_cast<float>(dressing.lamp.r);
        Gradient glow = Canvas::radialGradient(lx, ly, 2.0f, lx, ly, lr);
        glow.addStop(0.0f, pal.key.toColor(0.4f));
        glow.addStop(1.0f, pal.key.toColor(0.0f));
        c.setFillGradient(glow);
        c.fillRect(lx - lr, ly - lr, lr * 2.0f, lr * 2.0f);
        c.setFill(pal.key, 0.75f);
        c.fillRect(lx - 7.0f, ly - 7.0f, 14.0f, 14.0f);
    }

    for (const Prop& prop : dressing.props) {
        detail::drawProp(c, pal, prop);
    }
}

} // namespace maz::film
