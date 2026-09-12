// tests/film/reel.cpp — verifies the native reel reader (film Reel.hpp).
//
// A "reel" is a film as plain data: an ordered list of timed shots saying what the camera sees, for
// how long, who is in it and what they say. The browser writes it; this reads it. The two must agree
// exactly, because a native renderer that cuts the film even one shot differently is not rendering the
// same film. So the ground truth here is a real 40-shot film exported from the browser and checked in
// as a fixture, and the checks are about agreement rather than plausibility:
//   * the fixture's known title, seed, genre, shot count and duration come back unchanged;
//   * shotAt returns the right shot at every boundary, just inside it, and just past it;
//   * the shots are contiguous and sum to the stated duration;
//   * every spoken shot names a speaker who has a voice;
//   * out-of-range times behave exactly as the browser's shotAt does, quirk included;
//   * malformed input is rejected with a reason rather than crashing or half-parsing.
#include "maz/film/Reel.hpp"

#include <cmath>
#include <cstdio>
#include <string>

using maz::film::Reel;
using maz::film::Shot;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

int main(int argc, char** argv) {
    const std::string fixture = argc > 1 ? argv[1] : "tests/film/reel-fixture.json";

    const Reel reel = maz::film::loadReel(fixture);
    if (!reel.valid) {
        std::printf("FAIL: could not load the fixture reel at %s (%s)\n", fixture.c_str(),
                    reel.error.c_str());
        return 1;
    }

    // --- 1. The film that was exported is the film that came back. ---
    CHECK(reel.title == "DREW AND THE MESSAGE", "the fixture's title survives the trip");
    CHECK(reel.genre == "drama", "the fixture's genre survives");
    CHECK(reel.seed == 20260912u, "the seed survives — it is what makes the film repeatable");
    CHECK(reel.shots.size() == 40u, "all forty shots come back");
    CHECK(std::fabs(reel.duration - 159.27) < 0.01, "the duration comes back");
    CHECK(reel.characters.size() == 2u, "both characters come back");
    CHECK(reel.voices.size() == 2u, "both voices come back");

    // --- 2. Shots are contiguous and add up to the whole film. ---
    {
        double at = 0.0;
        bool contiguous = true;
        for (const Shot& s : reel.shots) {
            if (std::fabs(s.start - at) > 1e-9) {
                contiguous = false;
            }
            at += s.duration;
        }
        CHECK(contiguous, "every shot starts where the last one ended");
        CHECK(std::fabs(at - reel.duration) < 1e-9, "the shots add up to the stated duration");
    }

    // --- 3. shotAt lands on the right shot at every boundary. ---
    {
        bool atStart = true, insideOk = true, pastOk = true;
        for (std::size_t i = 0; i < reel.shots.size(); ++i) {
            const Shot& s = reel.shots[i];
            const Shot* a = maz::film::shotAt(reel, s.start);
            if (a == nullptr || a->index != s.index) {
                atStart = false;
            }
            const Shot* b = maz::film::shotAt(reel, s.start + s.duration * 0.5);
            if (b == nullptr || b->index != s.index) {
                insideOk = false;
            }
            // The instant a shot ends belongs to the NEXT shot, not to it.
            if (i + 1 < reel.shots.size()) {
                const Shot* c = maz::film::shotAt(reel, s.start + s.duration);
                if (c == nullptr || c->index != s.index + 1) {
                    pastOk = false;
                }
            }
        }
        CHECK(atStart, "shotAt(start of a shot) is that shot");
        CHECK(insideOk, "shotAt(middle of a shot) is that shot");
        CHECK(pastOk, "the instant a shot ends belongs to the next shot");
    }

    // --- 4. Out of range, exactly as the browser behaves. ---
    {
        const Shot* last = &reel.shots.back();
        const Shot* past = maz::film::shotAt(reel, reel.duration);
        const Shot* way = maz::film::shotAt(reel, reel.duration + 1000.0);
        // The browser's shotAt falls through its loop and returns the final shot for ANY time it does
        // not contain -- including a negative one. Matched deliberately: a native renderer that
        // disagreed here would be a second opinion about what the film is, not a second renderer.
        const Shot* before = maz::film::shotAt(reel, -1.0);
        CHECK(past == last, "a time at the very end gives the last shot");
        CHECK(way == last, "a time past the end gives the last shot");
        CHECK(before == last, "a negative time gives the last shot, as the browser does");
    }

    // --- 5. The film's shape: a title first, an end card last. ---
    CHECK(reel.shots.front().kind == "title", "a film opens on its title card");
    CHECK(reel.shots.back().kind == "end", "a film closes on its end card");
    CHECK(reel.shots.back().caption == "THE END", "the end card says so");

    // --- 6. Every spoken shot names a speaker who has a voice. ---
    {
        int spoken = 0;
        bool voiced = true, framed = true;
        for (const Shot& s : reel.shots) {
            if (s.kind != "line") {
                continue;
            }
            ++spoken;
            if (s.speaker.empty() || maz::film::voiceFor(reel, s.speaker) == nullptr) {
                voiced = false;
            }
            if (s.characters.empty()) {
                framed = false;
            }
        }
        CHECK(spoken > 0, "the fixture film has spoken shots");
        CHECK(voiced, "every spoken shot names a speaker who has a voice");
        CHECK(framed, "every spoken shot says who is in frame");
    }

    // --- 7. A voice carries what is needed to draw and sound a character. ---
    {
        const maz::film::Voice* v = maz::film::voiceFor(reel, reel.characters.front().name);
        CHECK(v != nullptr, "the lead has a voice");
        if (v != nullptr) {
            CHECK(v->pitch > 0.0f, "a voice has a pitch");
            CHECK(v->side == -1 || v->side == 1, "a voice says which side of frame they stand on");
        }
        CHECK(maz::film::voiceFor(reel, "NOBODY") == nullptr, "an unknown name has no voice");
    }

    // --- 8. Moods and framings are the values the renderer knows how to draw. ---
    {
        bool known = true;
        for (const Shot& s : reel.shots) {
            if (s.mood < 0.0f || s.mood > 1.0f) {
                known = false;
            }
            if (s.duration <= 0.0) {
                known = false;
            }
            if (s.set.empty() || s.framing.empty() || s.camera.empty() || s.time.empty()) {
                known = false;
            }
        }
        CHECK(known, "every shot has a set, a framing, a camera, an hour and a positive duration");
    }

    // --- 9. Malformed input is refused with a reason, not a crash or a half-parse. ---
    {
        const Reel notJson = maz::film::parseReel("this is not json at all");
        CHECK(!notJson.valid && !notJson.error.empty(), "text that is not JSON is refused with a reason");

        const Reel wrongKind = maz::film::parseReel("{\"format\":\"something-else\",\"version\":1}");
        CHECK(!wrongKind.valid, "a JSON document that is not a reel is refused");

        const Reel future = maz::film::parseReel(
            "{\"format\":\"maz-film-reel\",\"version\":99,\"shots\":[]}");
        CHECK(!future.valid, "a reel from a newer format version is refused rather than guessed at");

        const Reel noShots = maz::film::parseReel(
            "{\"format\":\"maz-film-reel\",\"version\":1,\"shots\":[]}");
        CHECK(!noShots.valid, "a reel with no shots is refused — there is no film to draw");

        const Reel missing = maz::film::loadReel("no/such/file.json");
        CHECK(!missing.valid && !missing.error.empty(), "a missing file is refused with a reason");
    }

    // --- 10. Reading is pure: the same text twice gives the same reel. ---
    {
        const Reel a = maz::film::loadReel(fixture);
        const Reel b = maz::film::loadReel(fixture);
        bool same = a.shots.size() == b.shots.size() && a.title == b.title &&
                    std::fabs(a.duration - b.duration) < 1e-12;
        for (std::size_t i = 0; same && i < a.shots.size(); ++i) {
            same = a.shots[i].start == b.shots[i].start && a.shots[i].caption == b.shots[i].caption &&
                   a.shots[i].set == b.shots[i].set;
        }
        CHECK(same, "reading the same reel twice gives the same reel");
    }

    // --- 11. The running time reads the way a person writes one. ---
    CHECK(maz::film::clock(0.0) == "0:00", "zero reads 0:00");
    CHECK(maz::film::clock(9.4) == "0:09", "nine seconds reads 0:09");
    CHECK(maz::film::clock(65.0) == "1:05", "sixty-five seconds reads 1:05");
    CHECK(maz::film::clock(159.27) == "2:39", "the fixture film runs 2:39");
    CHECK(maz::film::clock(-5.0) == "0:00", "a negative time reads 0:00 rather than a minus sign");

    if (g_fail == 0) {
        std::printf("film reel: all checks passed (%zu shots, %s)\n", reel.shots.size(),
                    maz::film::clock(reel.duration).c_str());
    }
    return g_fail == 0 ? 0 : 1;
}
