#pragma once

#include "maz/io/Json.hpp"

#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// maz::film — a film reel as plain data, and the reader that brings one in from a file.
//
// A "reel" is the edit of a film with no pictures in it: an ordered list of timed SHOTS, each saying
// what the camera sees, for how long, in which set, at what hour, who is in frame, who is speaking and
// what they say. It is produced by SCRIPT FORGE (film/js/film-reel.js) and deliberately carries no DOM,
// no canvas and no audio, so that something other than a browser can draw it -- which is what this is
// for. The document format is named and versioned ("maz-film-reel", version 1) precisely so that a
// renderer reading it has a contract rather than whatever the writer happened to put on the object.
//
// The rule this file lives by: AGREE WITH THE BROWSER. A native renderer that cuts the film even one
// shot differently is not a second renderer of the same film, it is a second opinion about what the
// film is. Where the browser has a quirk (see shotAt), it is matched on purpose and noted.
//
// Pure std + io::Json. No GPU, no window, no engine dependencies -- readable and testable headlessly.
namespace maz::film {

inline constexpr const char* kReelFormat = "maz-film-reel";
inline constexpr int kReelVersion = 1;

// How a character sounds and where they stand. Derived from their name, so a character sounds and
// looks the same every time the same film is rebuilt.
struct Voice {
    std::string name;
    float pitch = 0.0f; // Hz, the character's own register
    float rate = 1.0f;  // how fast they speak, relative
    float hue = 0.0f;   // degrees, their silhouette's tint
    int side = 0;       // -1 stands left of frame, +1 right
};

struct Character {
    std::string name;
    std::string role;
    std::string part; // "lead" or "supporting"
};

// One shot: the camera is on this for `duration` seconds starting at `start`.
struct Shot {
    int index = 0;
    double start = 0.0;
    double duration = 0.0;

    std::string kind;    // title | establish | action | line | end
    std::string set;     // which of the buildable sets to draw
    std::string time;    // NIGHT | DAWN | DAY | DUSK -- the light, not the clock
    std::string framing; // wide | mid | close | two | ots | insert | low
    std::string camera;  // static | push | push-slow | pull | pan-l | pan-r | track-l | track-r |
                         // handheld | whip

    std::string caption;
    std::string subcaption;
    std::string parenthetical;
    std::string speaker; // empty when nobody is speaking
    int side = 0;        // which side the speaker is on

    std::vector<std::string> characters; // who is in frame
    float mood = 0.0f;                   // 0 calm .. 1 wound up; drives light, score and cutting
    int scene = 0;
    std::string beat; // open | spark | push | turn | crisis | choice | after | title | end

    double end() const { return start + duration; }
};

struct Reel {
    std::string title;
    std::string genre;
    std::string genreLabel;
    std::string object; // the thing the story turns on
    std::uint32_t seed = 0;
    double duration = 0.0;

    std::vector<Character> characters;
    std::map<std::string, Voice> voices;
    std::vector<Shot> shots;

    bool valid = false;  // false means nothing below it should be trusted
    std::string error;   // why, when it is not valid
};

namespace detail {

inline Reel reelError(std::string why) {
    Reel r;
    r.valid = false;
    r.error = std::move(why);
    return r;
}

inline std::vector<std::string> stringArray(const io::JsonValue& v) {
    std::vector<std::string> out;
    if (!v.isArray()) {
        return out;
    }
    out.reserve(v.size());
    for (const io::JsonValue& item : v.items()) {
        if (item.isString()) {
            out.push_back(item.asString());
        }
    }
    return out;
}

} // namespace detail

// Parse a reel document. Never throws; check `valid` and read `error` when it is false.
inline Reel parseReel(const std::string& text) {
    const io::JsonParseResult parsed = io::parseJson(text);
    if (!parsed.ok) {
        return detail::reelError("not valid JSON: " + parsed.error + " (line " +
                                 std::to_string(parsed.line) + ")");
    }
    const io::JsonValue& doc = parsed.value;
    if (!doc.isObject()) {
        return detail::reelError("the document is not a JSON object");
    }
    if (doc["format"].asString() != kReelFormat) {
        return detail::reelError("not a film reel: expected format \"" + std::string(kReelFormat) +
                                 "\", found \"" + doc["format"].asString() + "\"");
    }
    // Refusing a newer version is the point of having one: a renderer that guessed at a format it did
    // not know would draw a wrong film confidently, which is worse than drawing none.
    const int version = doc["version"].asInt(0);
    if (version != kReelVersion) {
        return detail::reelError("reel format version " + std::to_string(version) +
                                 " is not readable by this build (expected " +
                                 std::to_string(kReelVersion) + ")");
    }
    if (!doc["shots"].isArray() || doc["shots"].size() == 0) {
        return detail::reelError("the reel has no shots -- there is no film to draw");
    }

    Reel reel;
    reel.title = doc["title"].asString();
    reel.genre = doc["genre"].asString();
    reel.genreLabel = doc["genreLabel"].asString();
    reel.object = doc["object"].asString();
    reel.seed = static_cast<std::uint32_t>(doc["seed"].asNumber(0.0));
    reel.duration = doc["duration"].asNumber(0.0);

    for (const io::JsonValue& c : doc["characters"].items()) {
        Character ch;
        ch.name = c["name"].asString();
        ch.role = c["role"].asString();
        ch.part = c["part"].asString();
        reel.characters.push_back(std::move(ch));
    }

    if (doc["voices"].isObject()) {
        for (const auto& entry : doc["voices"].fields().items) {
            const io::JsonValue& v = entry.second;
            Voice voice;
            voice.name = v["name"].asString(entry.first);
            voice.pitch = v["pitch"].asFloat(0.0f);
            voice.rate = v["rate"].asFloat(1.0f);
            voice.hue = v["hue"].asFloat(0.0f);
            voice.side = v["side"].asInt(0);
            reel.voices.emplace(entry.first, std::move(voice));
        }
    }

    reel.shots.reserve(doc["shots"].size());
    for (const io::JsonValue& s : doc["shots"].items()) {
        Shot shot;
        shot.index = s["index"].asInt(static_cast<int>(reel.shots.size()));
        shot.start = s["start"].asNumber(0.0);
        shot.duration = s["duration"].asNumber(0.0);
        shot.kind = s["kind"].asString();
        shot.set = s["set"].asString();
        shot.time = s["time"].asString();
        shot.framing = s["framing"].asString();
        shot.camera = s["camera"].asString();
        shot.caption = s["caption"].asString();
        shot.subcaption = s["subcaption"].asString();
        shot.parenthetical = s["parenthetical"].asString();
        shot.speaker = s["speaker"].asString();
        shot.side = s["side"].asInt(0);
        shot.characters = detail::stringArray(s["characters"]);
        shot.mood = s["mood"].asFloat(0.0f);
        shot.scene = s["scene"].asInt(0);
        shot.beat = s["beat"].asString();
        reel.shots.push_back(std::move(shot));
    }

    // A duration the writer forgot is recoverable; a wrong one is not worth trusting over the shots.
    if (!(reel.duration > 0.0)) {
        reel.duration = reel.shots.back().end();
    }

    reel.valid = true;
    return reel;
}

// Read a reel document from disk.
inline Reel loadReel(const std::string& path) {
    std::string text;
    if (!io::readTextFile(path, text)) {
        return detail::reelError("could not read the reel file \"" + path + "\"");
    }
    return parseReel(text);
}

// Which shot is on screen at `time`. Null only for a reel with no shots.
//
// The final shot is returned for ANY time the film does not contain -- past the end, and before the
// beginning too. That is exactly what the browser's shotAt does (it falls out of the bottom of its
// loop), and it is matched rather than corrected so that both renderers answer the same question the
// same way. Nothing asks for a negative time; if anything ever does, it should do so identically.
inline const Shot* shotAt(const Reel& reel, double time) {
    if (reel.shots.empty()) {
        return nullptr;
    }
    for (const Shot& s : reel.shots) {
        if (time >= s.start && time < s.end()) {
            return &s;
        }
    }
    return &reel.shots.back();
}

// A character's voice, or null if the reel does not have one for that name.
inline const Voice* voiceFor(const Reel& reel, const std::string& name) {
    const auto it = reel.voices.find(name);
    return it == reel.voices.end() ? nullptr : &it->second;
}

// Seconds as a running time, the way a person writes one: 2:39.
inline std::string clock(double seconds) {
    long total = static_cast<long>(std::lround(seconds));
    if (total < 0) {
        total = 0;
    }
    const long minutes = total / 60;
    const long rest = total % 60;
    return std::to_string(minutes) + ":" + (rest < 10 ? "0" : "") + std::to_string(rest);
}

} // namespace maz::film
