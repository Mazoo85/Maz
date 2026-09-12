// tests/film/showcase.cpp — the demo reel is only a demonstration if it still shows everything.
//
// apps/filmshowcase/Montage.hpp claims, in a comment, that it covers all fifteen sets, all ten genre
// palettes, all seven framings, all ten camera moves and all seven kinds of weather. A comment cannot
// hold that true: add a set to Sets.hpp and the claim quietly becomes false, and the only symptom is a
// demo that no longer shows the thing that was added. So this test counts.
#include "Montage.hpp"

#include "maz/film/Air.hpp"
#include "maz/film/Sets.hpp"

#include <algorithm>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

namespace {

int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

// What is missing from `have`, out of `want`, as a readable list.
std::string missingFrom(const std::vector<std::string>& want, const std::set<std::string>& have) {
    std::string out;
    for (const std::string& w : want) {
        if (have.count(w) == 0) {
            out += (out.empty() ? "" : ", ") + w;
        }
    }
    return out;
}

} // namespace

int main() {
    const std::vector<filmshowcase::Chapter> montage = filmshowcase::montage();

    std::set<std::string> sets, genres, framings, cameras, weathers, kinds, beats;
    int shots = 0;
    double seconds = 0.0;
    for (const filmshowcase::Chapter& c : montage) {
        genres.insert(c.genre);
        for (const filmshowcase::Beat& b : c.beats) {
            ++shots;
            seconds += b.duration;
            sets.insert(b.set);
            framings.insert(b.framing);
            cameras.insert(b.camera);
            kinds.insert(b.kind);
            beats.insert(b.beat);
            weathers.insert(maz::film::weatherName(maz::film::weatherFor(c.genre, b.time, b.set)));
        }
    }

    // --- 1. Every buildable set appears. This is the one that breaks when the engine grows. ---
    {
        std::vector<std::string> want;
        for (const maz::film::SetPainter& s : maz::film::sets()) {
            want.push_back(s.name);
        }
        const std::string missing = missingFrom(want, sets);
        CHECK(missing.empty(), ("every buildable set is in the demo reel (missing: " + missing +
                                ")").c_str());
    }

    // --- 2. Every genre palette, so the colour range is all of it and not one mood. ---
    {
        const std::vector<std::string> want{"drama",   "thriller", "horror", "comedy", "romance",
                                            "scifi",   "mystery",  "fantasy", "heist", "western"};
        const std::string missing = missingFrom(want, genres);
        CHECK(missing.empty(), ("every genre appears (missing: " + missing + ")").c_str());
    }

    // --- 3. Every framing and every camera move. ---
    {
        const std::vector<std::string> wantFraming{"wide", "mid", "close", "two", "ots", "insert",
                                                   "low"};
        const std::string mf = missingFrom(wantFraming, framings);
        CHECK(mf.empty(), ("every framing appears (missing: " + mf + ")").c_str());

        const std::vector<std::string> wantCamera{"static", "push",     "push-slow", "pull",
                                                  "pan-l",  "pan-r",    "track-l",   "track-r",
                                                  "handheld", "whip"};
        const std::string mc = missingFrom(wantCamera, cameras);
        CHECK(mc.empty(), ("every camera move appears (missing: " + mc + ")").c_str());
    }

    // --- 4. Every kind of weather, which is not chosen directly: it follows from genre, hour and
    //        whether the set is outdoors, so covering it means choosing those three well. ---
    {
        const std::vector<std::string> want{"none",    "rain",  "dust", "fog",
                                            "haze",    "shimmer", "embers"};
        const std::string missing = missingFrom(want, weathers);
        CHECK(missing.empty(), ("every kind of weather appears (missing: " + missing + ")").c_str());
    }

    // --- 5. Every kind of caption the renderer can draw, since each is laid out differently. ---
    {
        const std::vector<std::string> want{"title", "establish", "action", "line", "end"};
        const std::string missing = missingFrom(want, kinds);
        CHECK(missing.empty(), ("every caption kind appears (missing: " + missing + ")").c_str());
    }

    // --- 6. The reels a chapter turns into are valid, timed end to end, and carry their cast. ---
    {
        bool ok = true;
        std::string why;
        for (const filmshowcase::Chapter& c : montage) {
            const maz::film::Reel reel = filmshowcase::reelFor(c);
            if (!reel.valid || reel.shots.size() != c.beats.size()) {
                ok = false;
                why = std::string(c.genre) + ": wrong shot count";
                break;
            }
            double t = 0.0;
            for (std::size_t i = 0; i < reel.shots.size(); ++i) {
                const maz::film::Shot& s = reel.shots[i];
                if (s.start != t || s.index != static_cast<int>(i)) {
                    ok = false;
                    why = std::string(c.genre) + ": shot " + std::to_string(i) + " starts wrong";
                    break;
                }
                if (s.characters.size() != static_cast<std::size_t>(c.beats[i].cast)) {
                    ok = false;
                    why = std::string(c.genre) + ": shot " + std::to_string(i) + " has the wrong cast";
                    break;
                }
                // A speaking shot must have the speaker in frame, or the line comes from nobody.
                if (!s.speaker.empty() &&
                    std::find(s.characters.begin(), s.characters.end(), s.speaker) ==
                        s.characters.end()) {
                    ok = false;
                    why = std::string(c.genre) + ": " + s.speaker + " speaks from off frame";
                    break;
                }
                t += s.duration;
            }
            if (!ok) break;
            if (reel.duration != t) {
                ok = false;
                why = std::string(c.genre) + ": duration does not match the shots";
                break;
            }
            // Every shot must find itself, at its start and just before its end.
            for (const maz::film::Shot& s : reel.shots) {
                const maz::film::Shot* a = maz::film::shotAt(reel, s.start);
                const maz::film::Shot* b = maz::film::shotAt(reel, s.end() - 0.001);
                if (a == nullptr || b == nullptr || a->index != s.index || b->index != s.index) {
                    ok = false;
                    why = std::string(c.genre) + ": shot " + std::to_string(s.index) +
                          " is not on screen during itself";
                    break;
                }
            }
            if (!ok) break;
        }
        CHECK(ok, ("every chapter makes a reel that plays (" + why + ")").c_str());
    }

    // --- 7. It stays a demo reel and not a film: short, and none of it dead air. ---
    {
        CHECK(seconds > 20.0 && seconds < 60.0,
              ("the reel is between 20 and 60 seconds (" + std::to_string(seconds) + ")").c_str());
        bool eachLongEnough = true;
        for (const filmshowcase::Chapter& c : montage) {
            for (const filmshowcase::Beat& b : c.beats) {
                // Under a second at 12fps is fewer than 12 frames, and the fades at a scene change
                // take 0.45s of that: a shot that short is a flash, not a shot.
                eachLongEnough = eachLongEnough && b.duration >= 1.0;
            }
        }
        CHECK(eachLongEnough, "no shot is shorter than a second");
    }

    if (g_fail == 0) {
        std::printf("film showcase: all checks passed (%zu chapters, %d shots, %.1fs, %zu sets, "
                    "%zu weather kinds)\n",
                    montage.size(), shots, seconds, sets.size(), weathers.size());
    }
    return g_fail == 0 ? 0 : 1;
}
