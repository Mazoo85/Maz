#pragma once

#include "maz/film/Reel.hpp"

#include <cstddef>
#include <string>

// maz::film BUSINESS — what a character is doing with the room.
//
// "Business" is the word a theatre uses for it: not the lines and not the blocking, but the small
// physical things an actor does while the scene happens. Somebody sits down. Somebody leans on the
// door frame. Somebody picks the thing up, and later puts it down again. It is what separates a person
// in a room from a person standing in front of a painting of one, and until now the 3D renderer had
// none of it: two people stood on their marks for the length of the film and talked.
//
// Everything here is read out of the reel, which means two different sources of truth and it is worth
// being straight about the difference:
//
//   THE OBJECT is real data. The reel says who is holding the thing the story turns on, shot by shot,
//   because the screenplay tracked it while it was being written. When that changes, somebody picked
//   it up or put it down, and the exact shot it happened in is known.
//
//   SITTING AND LEANING are read out of the PROSE, because the prose is the only place the film says
//   anybody did them. That is string matching and there is no dressing it up. What makes it work at
//   all is that the prose is not arbitrary: it comes from the same generator every time, out of a
//   small and known vocabulary, so the handful of words below is most of what it can ever say. A word
//   it does not know costs nothing — the character stands, which is what they did before.
namespace maz::film {

// What one character is doing with the room in one shot.
struct Business {
    float sit = 0.0f;  // 0 standing, 1 seated, between them the act of sitting down
    float lean = 0.0f; // signed: leaning on something to one side or the other, 0 not leaning
    bool holding = false;    // the thing the story turns on is in their hand
    bool takingIt = false;   // and this is the shot where it arrives there
    bool givingItUp = false; // or the shot where it leaves
};

namespace bizdetail {

inline std::string lowered(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
    }
    return out;
}

// Whole words only. Without that, "sits" is found inside nothing in particular but "sat" is found
// inside "sATisfied" and half the film ends up on the floor.
inline bool hasWord(const std::string& hay, const std::string& word) {
    std::size_t at = 0;
    while ((at = hay.find(word, at)) != std::string::npos) {
        const bool startOk = at == 0 || !((hay[at - 1] >= 'a' && hay[at - 1] <= 'z'));
        const std::size_t after = at + word.size();
        const bool endOk = after >= hay.size() || !(hay[after] >= 'a' && hay[after] <= 'z');
        if (startOk && endOk) {
            return true;
        }
        at = after;
    }
    return false;
}

inline bool anyWord(const std::string& hay, const char* const* words, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        if (hasWord(hay, words[i])) {
            return true;
        }
    }
    return false;
}

} // namespace bizdetail

// The vocabulary. Deliberately short: these are the words the screenplay writes, and a longer list
// guessing at synonyms it never uses would only find them by accident inside other words.
inline bool saysSitting(const std::string& line) {
    static const char* const words[] = {"sits", "sit",     "sat",   "sitting", "slumps",
                                        "sinks", "settles", "kneels"};
    return bizdetail::anyWord(bizdetail::lowered(line), words, sizeof(words) / sizeof(words[0]));
}

inline bool saysStanding(const std::string& line) {
    static const char* const words[] = {"stands", "rises", "gets up", "straightens", "walks", "leaves",
                                        "goes"};
    const std::string low = bizdetail::lowered(line);
    // "standing has stopped working" is not somebody standing up, and the film really does write
    // sentences like that. A line that says both is a line about sitting down.
    if (saysSitting(low)) {
        return false;
    }
    return bizdetail::anyWord(low, words, sizeof(words) / sizeof(words[0]));
}

inline bool saysLeaning(const std::string& line) {
    static const char* const words[] = {"leans", "leaning", "props", "slouches", "rests"};
    return bizdetail::anyWord(bizdetail::lowered(line), words, sizeof(words) / sizeof(words[0]));
}

namespace bizdetail {

// Everything the shot says out loud: the action line, the line under it, and the direction in
// brackets before a line of dialogue.
inline std::string proseOf(const Shot& s) {
    return s.caption + " " + s.subcaption + " " + s.parenthetical;
}

// Whether this shot's prose is ABOUT this character. With one person on the set it always is; with
// two, a line that names one of them is about that one, and a line that names neither is about
// whoever is speaking.
inline bool aboutThem(const Shot& s, const std::string& who) {
    // Somebody who is not in the shot is not doing anything in it. Without this, a line that says one
    // character sits down sits everybody in the film down, because there is nothing in the sentence
    // that says it is not about them either.
    bool inIt = s.characters.empty();
    for (const std::string& c : s.characters) {
        if (c == who) {
            inIt = true;
        }
    }
    if (!inIt) {
        return false;
    }
    if (s.characters.size() < 2) {
        return true;
    }
    const std::string low = lowered(proseOf(s));
    const std::string name = lowered(who);
    if (!name.empty() && low.find(name) != std::string::npos) {
        return true;
    }
    for (const std::string& other : s.characters) {
        if (other != who && !other.empty() && low.find(lowered(other)) != std::string::npos) {
            return false; // it is about the other one
        }
    }
    return s.speaker == who;
}

} // namespace bizdetail

// What `who` is doing in shot `index`. `progress` is 0..1 through that shot, so the act of sitting
// down can be shown happening rather than cut to already done.
inline Business businessAt(const Reel& reel, int index, const std::string& who, double progress) {
    Business biz;
    if (index < 0 || static_cast<std::size_t>(index) >= reel.shots.size()) {
        return biz;
    }
    const std::size_t here = static_cast<std::size_t>(index);
    const Shot& shot = reel.shots[here];

    // ---- the object ------------------------------------------------------------------------------
    //
    // Real data: the reel tracked who had it while the screenplay was being written.
    biz.holding = !who.empty() && shot.holdingBy == who;
    const std::string before = here > 0 ? reel.shots[here - 1].holdingBy : std::string();
    const std::string after =
        here + 1 < reel.shots.size() ? reel.shots[here + 1].holdingBy : shot.holdingBy;
    biz.takingIt = biz.holding && before != who;
    biz.givingItUp = biz.holding && after != who;

    // ---- sitting ---------------------------------------------------------------------------------
    //
    // Sitting down is a state, not an event: somebody who sat in the third shot of a scene is still
    // sitting in the fourth. So the search runs BACKWARD through the scene for the last thing said
    // about this character, and stops at the edge of the scene — a cut to a different room stands
    // everybody up, which is both true and the only sane default.
    for (std::size_t i = here + 1; i-- > 0;) {
        const Shot& past = reel.shots[i];
        if (past.scene != shot.scene) {
            break;
        }
        if (!bizdetail::aboutThem(past, who)) {
            continue;
        }
        const std::string prose = bizdetail::proseOf(past);
        if (saysStanding(prose)) {
            break;
        }
        if (saysSitting(prose)) {
            // In the shot it happens in, it HAPPENS; in every shot after it, it has happened.
            const double t = progress < 0.0 ? 0.0 : (progress > 1.0 ? 1.0 : progress);
            biz.sit = i == here ? static_cast<float>(t) : 1.0f;
            break;
        }
    }

    // ---- leaning ---------------------------------------------------------------------------------
    //
    // Not a state. Leaning is something somebody is doing in a shot, and a character who leaned once
    // and then stayed welded to the wall for the rest of the scene is a character nobody blocked.
    if (biz.sit <= 0.0f && bizdetail::aboutThem(shot, who) && saysLeaning(bizdetail::proseOf(shot))) {
        // Toward the nearer wall, which is the side of frame they are already standing on.
        biz.lean = shot.side < 0 ? -1.0f : 1.0f;
    }
    return biz;
}

} // namespace maz::film
