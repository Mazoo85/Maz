#pragma once

#include "maz/film/Noise.hpp"
#include "maz/film/Reel.hpp"

#include <string>
#include <vector>

// filmshowcase — the demo reel, written as data.
//
// A film made by SCRIPT FORGE is one story: one genre, one palette, a handful of sets, whatever
// weather that genre and hour imply. That is the right shape for a film and the wrong shape for a
// demonstration, because no single film visits all fifteen sets, all ten genre palettes, all seven
// framings or all six kinds of weather.
//
// So this is a MONTAGE: a short chapter per genre, chosen between them to cover every set once, every
// palette, every camera move and every weather kind. It is built out of the same maz::film::Reel type
// the browser writes, drawn by the same drawFrame the native renderer uses for a real film, and is
// therefore a demonstration of the renderer rather than a second renderer with demo features in it.
//
// Pure data + std. No GPU, no window, no drawing here at all.
namespace filmshowcase {

// One shot of the montage, flat enough to read the whole reel as a table.
struct Beat {
    const char* kind;     // title | establish | action | line | end
    const char* set;      // one of the fifteen
    const char* time;     // NIGHT | DAWN | DAY | DUSK
    const char* framing;  // wide | mid | close | two | ots | insert | low
    const char* camera;   // static | push | push-slow | pull | pan-l | pan-r | track-l | track-r |
                          // handheld | whip
    const char* caption;
    const char* subcaption;
    const char* speaker;      // "" when nobody speaks
    const char* parenthetical;
    const char* beat;         // open | spark | push | turn | crisis | choice | after | title | end
    double mood;              // 0 calm .. 1 wound up
    double duration;          // seconds
    int cast;                 // how many characters stand in frame: 0, 1 or 2
    int side;                 // which side the speaker is on: -1, 0, +1
    int scene;                // shots sharing a scene cut hard; a new scene dips through black
};

// A genre's stretch of the montage: its own palette, its own cast, its own seed.
struct Chapter {
    const char* genre;
    const char* genreLabel;
    const char* object; // the thing an insert shot holds on
    const char* lead;
    const char* second;
    std::uint32_t seed;
    std::vector<Beat> beats;
};

// Turn a chapter into a reel: start times accumulated, voices derived from the names exactly as
// SCRIPT FORGE derives them, so a showcase figure is tinted and stands like a real one.
inline maz::film::Reel reelFor(const Chapter& chapter) {
    maz::film::Reel reel;
    reel.title = "MAZ ENGINE";
    reel.genre = chapter.genre;
    reel.genreLabel = chapter.genreLabel;
    reel.object = chapter.object;
    reel.seed = chapter.seed;
    reel.valid = true;

    const char* names[2] = {chapter.lead, chapter.second};
    for (int i = 0; i < 2; ++i) {
        maz::film::Character c;
        c.name = names[i];
        c.role = i == 0 ? "lead" : "supporting";
        c.part = i == 0 ? "lead" : "supporting";
        reel.characters.push_back(c);

        // Same derivation as film-reel.js: everything about a voice comes from its name.
        maz::film::Rng rng(maz::film::hashText(std::string("voice:") + names[i]));
        maz::film::Voice v;
        v.name = names[i];
        v.pitch = static_cast<float>(100.0 + rng.next() * 140.0);
        v.rate = static_cast<float>(0.9 + rng.next() * 0.25);
        v.hue = static_cast<float>(rng.next() * 360.0);
        v.side = i == 0 ? -1 : 1;
        reel.voices[v.name] = v;
    }

    double t = 0.0;
    for (std::size_t i = 0; i < chapter.beats.size(); ++i) {
        const Beat& b = chapter.beats[i];
        maz::film::Shot s;
        s.index = static_cast<int>(i);
        s.start = t;
        s.duration = b.duration;
        s.kind = b.kind;
        s.set = b.set;
        s.time = b.time;
        s.framing = b.framing;
        s.camera = b.camera;
        s.caption = b.caption;
        s.subcaption = b.subcaption;
        s.parenthetical = b.parenthetical;
        s.speaker = b.speaker;
        s.side = b.side;
        s.mood = b.mood;
        s.scene = b.scene;
        s.beat = b.beat;
        for (int c = 0; c < b.cast && c < 2; ++c) {
            s.characters.push_back(names[c]);
        }
        reel.shots.push_back(s);
        t += b.duration;
    }
    reel.duration = t;
    return reel;
}

// ------------------------------------------------------------------ the montage itself
//
// Coverage, deliberately: all fifteen sets appear (three of them twice); all ten genre palettes; the
// weather kinds follow from genre+hour+outdoors (Air.hpp), so western by day gives shimmer, fantasy
// gives embers, horror outdoors gives fog, thriller at night outdoors gives rain, a night interior
// gives haze and a day interior gives dust; the framings and camera moves are spread so that no two
// neighbouring shots move the same way.
inline std::vector<Chapter> montage() {
    std::vector<Chapter> m;

    // DRAMA — the title, and the quiet end of the scale.
    m.push_back(Chapter{
        "drama", "Drama", "letter", "MARA", "JUNO", 0x5eedd00du,
        {
            {"title", "water", "DUSK", "wide", "push-slow", "MAZ ENGINE",
             "a film drawn in c++ \xC2\xB7 no browser", "", "", "title", 0.15, 2.5, 0, 0, 0},
            {"establish", "kitchen", "DAY", "wide", "pan-r",
             "int. kitchen \xE2\x80\x94 day \xC2\xB7 drama \xC2\xB7 dust", "", "", "", "open", 0.2, 1.25, 1, 0, 1},
            {"action", "street", "DAY", "low", "pull", "Fifteen sets, each built in three planes of depth.",
             "", "", "", "spark", 0.3, 1.35, 2, 0, 2},
        }});

    // HORROR — fog outdoors, haze in.
    m.push_back(Chapter{
        "horror", "Horror", "key", "SHAY", "SAM", 0x21a55a11u,
        {
            {"establish", "woods", "NIGHT", "wide", "push",
             "ext. woods \xE2\x80\x94 night \xC2\xB7 horror \xC2\xB7 fog", "", "", "", "turn", 0.7, 1.35, 1, 0, 0},
            {"line", "ward", "NIGHT", "mid", "handheld", "Something in here is still awake.", "",
             "SHAY", "(whispering)", "crisis", 0.85, 2.0, 1, -1, 1},
        }});

    // THRILLER — rain on the street, then a corridor.
    m.push_back(Chapter{
        "thriller", "Thriller", "phone", "VEN", "KAI", 0x7a11e0deu,
        {
            {"establish", "street", "NIGHT", "wide", "track-l",
             "ext. street \xE2\x80\x94 night \xC2\xB7 thriller \xC2\xB7 rain", "", "", "", "push", 0.6, 1.35, 2, 0, 0},
            {"action", "corridor", "NIGHT", "mid", "push",
             "A camera with intent: push, pull, track, whip.", "", "", "", "crisis", 0.9, 1.35, 2, 0, 1},
        }});

    // WESTERN — heat shimmer by day, and the object the story turns on.
    m.push_back(Chapter{
        "western", "Western", "watch", "ODELL", "RUE", 0x11dea7u,
        {
            {"establish", "field", "DAY", "wide", "pan-l",
             "ext. field \xE2\x80\x94 day \xC2\xB7 western \xC2\xB7 shimmer", "", "", "", "open", 0.35, 1.35, 1, 0, 0},
            {"insert", "field", "DAY", "insert", "push", "", "", "", "", "spark", 0.4, 1.0, 0, 0, 0},
        }});

    // FANTASY — embers, whatever the hour.
    m.push_back(Chapter{
        "fantasy", "Fantasy", "ring", "ILVA", "BRAN", 0xfa27a5eeu,
        {
            {"establish", "chapel", "DUSK", "wide", "push-slow",
             "int. chapel \xE2\x80\x94 dusk \xC2\xB7 fantasy \xC2\xB7 embers", "", "", "", "choice", 0.5, 1.35, 1, 0, 0},
            {"line", "chapel", "DUSK", "ots", "static", "Then we do it the hard way.", "", "ILVA", "",
             "choice", 0.55, 1.8, 2, -1, 1},
        }});

    // SCIFI — the cold end of the palettes.
    m.push_back(Chapter{
        "scifi", "Sci-fi", "disc", "NOOR", "AXEL", 0x5c1f1000u,
        {
            {"establish", "ship", "DUSK", "wide", "push",
             "int. ship \xE2\x80\x94 dusk \xC2\xB7 sci-fi \xC2\xB7 haze", "", "", "", "open", 0.4, 1.25, 1, 0, 0},
            {"action", "industrial", "DUSK", "low", "track-r", "Ten poses, one for every story beat.", "",
             "", "", "push", 0.6, 1.25, 2, 0, 1},
        }});

    // COMEDY — the bright end.
    m.push_back(Chapter{
        "comedy", "Comedy", "cup", "PIP", "DORA", 0xc0fedau,
        {
            {"establish", "office", "DAY", "wide", "static",
             "int. office \xE2\x80\x94 day \xC2\xB7 comedy \xC2\xB7 dust", "", "", "", "after", 0.25, 1.2, 2, 0, 0},
            {"line", "office", "DAY", "mid", "push-slow",
             "Nobody reads the manual. That's the manual.", "", "DORA", "", "after", 0.3, 1.8, 2, 1, 0},
        }});

    // HEIST — the bar at speed, then the car.
    m.push_back(Chapter{
        "heist", "Heist", "case", "MERCE", "TOBY", 0x4e15700du,
        {
            {"establish", "bar", "DAY", "two", "whip",
             "int. bar \xE2\x80\x94 day \xC2\xB7 heist \xC2\xB7 dust", "", "", "", "spark", 0.45, 1.2, 2, 0, 0},
            {"action", "vehicle", "DUSK", "close", "handheld", "Seven framings, from a wide to a face.", "",
             "", "", "crisis", 0.8, 1.25, 1, 0, 1},
        }});

    // MYSTERY — the lighthouse and its beam, then a room held still.
    m.push_back(Chapter{
        "mystery", "Mystery", "photograph", "ADA", "WREN", 0x115723fu,
        {
            {"establish", "lighthouse", "NIGHT", "wide", "pan-r",
             "ext. lighthouse \xE2\x80\x94 night \xC2\xB7 mystery", "", "", "", "turn", 0.55, 1.5, 1, 0, 0},
            {"establish", "room", "DUSK", "mid", "static",
             "int. room \xE2\x80\x94 dusk \xC2\xB7 mystery \xC2\xB7 haze", "", "", "", "turn", 0.55, 1.2, 1, 0, 1},
        }});

    // ROMANCE — the last palette, and the end card.
    m.push_back(Chapter{
        "romance", "Romance", "ribbon", "ELIN", "OTTO", 0x40a11ceu,
        {
            {"establish", "water", "DAWN", "wide", "pull",
             "ext. water \xE2\x80\x94 dawn \xC2\xB7 romance", "", "", "", "after", 0.3, 1.25, 2, 0, 0},
            {"end", "field", "DAWN", "wide", "push-slow", "MAZ ENGINE",
             "c++20 \xC2\xB7 sdl3 \xC2\xB7 vulkan \xE2\x80\x94 this reel drawn on the cpu", "", "", "end", 0.1,
             2.6, 0, 0, 1},
        }});

    return m;
}

} // namespace filmshowcase
