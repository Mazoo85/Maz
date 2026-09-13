// tests/film/business.cpp — what a character is doing with the room, read out of the film.
//
// Two sources of truth, and the tests are different because the sources are:
//
//   THE OBJECT is recorded data. The reel says who is holding the thing the story turns on, shot by
//   shot, and the tests are exact — this person has it, that person does not, and the shot where it
//   changed hands is the shot where it changed hands.
//
//   SITTING AND LEANING are read out of the prose, because the prose is the only place the film ever
//   says anybody did them. So the tests are about the two ways reading prose goes wrong: finding a
//   word inside another word, and attributing to one character something the other one did.
#include "maz/film/Business.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace film = maz::film;

static std::vector<std::string> failures;

static void check(bool ok, const std::string& what) {
    if (!ok) {
        failures.push_back(what);
    }
}

// A reel built by hand, so the tests say what they mean rather than depending on which film the
// screenplay generator happens to write today.
static film::Reel makeReel() {
    film::Reel r;
    r.valid = true;
    r.object = "key";
    r.characters.push_back({"SHAY", "night nurse", "lead"});
    r.characters.push_back({"SAM", "stranger", "supporting"});

    auto shot = [&](int scene, const char* caption, const char* speaker, const char* holder,
                    std::vector<std::string> who) {
        film::Shot s;
        s.index = static_cast<int>(r.shots.size());
        s.start = static_cast<double>(s.index) * 4.0;
        s.duration = 4.0;
        s.scene = scene;
        s.caption = caption;
        s.speaker = speaker;
        s.holdingBy = holder;
        s.characters = std::move(who);
        r.shots.push_back(s);
    };

    shot(1, "SHAY comes in without knocking.", "", "", {"SHAY"});                    // 0
    shot(1, "SHAY picks the key up off the table.", "", "SHAY", {"SHAY"});           // 1
    shot(1, "Ten minutes. In, out, done.", "SHAY", "SHAY", {"SHAY", "SAM"});         // 2
    shot(1, "SHAY sits down because standing has stopped working.", "", "SHAY",
         {"SHAY", "SAM"});                                                           // 3
    shot(1, "I am not angry.", "SHAY", "SHAY", {"SHAY", "SAM"});                     // 4
    shot(1, "SAM leans on the door frame and waits.", "", "SHAY", {"SHAY", "SAM"});  // 5
    shot(1, "SHAY puts the key down. SAM says nothing.", "", "", {"SHAY", "SAM"});   // 6
    // A line with nobody's name in it, in a shot only one of them is in. It is the shape of sentence
    // that used to put the whole cast on the floor, because nothing in it says who it is NOT about.
    shot(1, "Sits down, finally, and looks at the wall.", "", "", {"SHAY"});          // 7
    shot(2, "Outside, the ordinary world carries on being ordinary.", "", "",
         {"SHAY"});                                                                  // 8
    return r;
}

int main() {
    const film::Reel reel = makeReel();

    // ------------------------------------------------------------------ 1. the object
    {
        check(!film::businessAt(reel, 0, "SHAY", 0.5).holding, "nobody starts with it");
        check(film::businessAt(reel, 1, "SHAY", 0.5).holding, "and then SHAY has it");
        check(!film::businessAt(reel, 1, "SAM", 0.5).holding, "and SAM does not");
        check(film::businessAt(reel, 4, "SHAY", 0.5).holding, "and still has it three shots later");
        check(!film::businessAt(reel, 6, "SHAY", 0.5).holding, "and then puts it down");

        // The two shots that are worth playing rather than cutting around.
        check(film::businessAt(reel, 1, "SHAY", 0.5).takingIt,
              "the shot where it arrives in a hand knows that it does");
        check(!film::businessAt(reel, 2, "SHAY", 0.5).takingIt,
              "and the next shot does not think it happens all over again");
        check(film::businessAt(reel, 5, "SHAY", 0.5).givingItUp,
              "and the shot where it leaves knows that too");
        check(!film::businessAt(reel, 4, "SHAY", 0.5).givingItUp,
              "one shot before, they are still holding on to it");
    }

    // ------------------------------------------------------------------ 2. sitting
    {
        // In the shot it happens in, it HAPPENS. Cutting to somebody already seated throws away the
        // beat — sitting down because standing has stopped working IS the shot.
        check(film::businessAt(reel, 3, "SHAY", 0.0).sit < 0.1f, "at the top of the shot they stand");
        check(film::businessAt(reel, 3, "SHAY", 1.0).sit > 0.9f, "and by the end of it they are down");
        const float middle = film::businessAt(reel, 3, "SHAY", 0.5).sit;
        check(middle > 0.2f && middle < 0.8f, "and in the middle they are halfway down");

        // And they STAY down. Somebody who sat in one shot is still sitting in the next, or they are
        // bobbing up and down at every cut.
        check(film::businessAt(reel, 4, "SHAY", 0.5).sit > 0.9f, "they are still sitting a shot later");
        check(film::businessAt(reel, 6, "SHAY", 0.5).sit > 0.9f, "and three shots later");

        // But not for ever: a cut to a different scene is a cut to somewhere else, and everybody is
        // standing up when we get there.
        check(film::businessAt(reel, 8, "SHAY", 0.5).sit < 0.1f,
              "and standing again in the next scene, because it is a different room");

        // It is about the person the line is about. A line that says one character sat down must not
        // sit the whole cast down — which is exactly what it did until it was made to check.
        check(film::businessAt(reel, 4, "SAM", 0.5).sit < 0.1f, "SAM, who never sat down, is standing");
        // Including when the line names nobody at all, in a shot the other one is not even in.
        check(film::businessAt(reel, 7, "SHAY", 1.0).sit > 0.9f,
              "an unattributed line in a one-hander is about the one person in it");
        check(film::businessAt(reel, 7, "SAM", 1.0).sit < 0.1f,
              "and not about somebody who is not in the shot");
    }

    // ------------------------------------------------------------------ 3. leaning
    {
        check(film::businessAt(reel, 5, "SAM", 0.5).lean != 0.0f, "SAM leans on the door frame");
        check(film::businessAt(reel, 5, "SHAY", 0.5).lean == 0.0f, "and SHAY, who is sitting, does not");
        // Leaning is not a state. Somebody who leaned on a wall in one shot and is welded to it for
        // the rest of the scene has not been blocked, they have been parked.
        check(film::businessAt(reel, 6, "SAM", 0.5).lean == 0.0f,
              "and in the next shot, which is about SAM too, SAM is standing up again");
    }

    // ------------------------------------------------------------------ 4. reading prose
    {
        // Whole words. This is the failure that makes string matching a bad idea when it is done
        // badly: "sat" lives inside "satisfied", "sit" inside "visiting", "rests" inside "arrests".
        check(film::saysSitting("SHAY sits down"), "\"sits\" is somebody sitting");
        check(film::saysSitting("SHAY slumps against the desk"), "and so is \"slumps\"");
        check(!film::saysSitting("SHAY looks satisfied"), "but \"satisfied\" is not");
        check(!film::saysSitting("a visiting hour that nobody keeps"), "and neither is \"visiting\"");
        check(!film::saysLeaning("the arrests were never reported"), "nor \"arrests\" leaning");

        // And the sentence the film actually writes, which says both words at once and means only one
        // of them. "Standing has stopped working" is not somebody standing up.
        const std::string real = "SHAY sits down in the middle of it because standing has stopped working.";
        check(film::saysSitting(real), "the line the film really writes reads as sitting down");
        check(!film::saysStanding(real), "and not, at the same time, as standing up");
        check(film::saysStanding("SHAY stands and walks out"), "while standing up reads as standing up");
        // A line that says both means the later of them, and the later of them is always sitting
        // down: somebody who stands up and then sits is sitting when the shot ends.
        check(!film::saysStanding("SHAY stands, thinks better of it, and sits back down"),
              "and a line that says both is a line about sitting down");
    }

    // ------------------------------------------------------------------ 5. nothing out of range
    {
        // A shot index that is not a shot, a name that is not in the film, an empty reel: none of them
        // may read off the end of anything.
        const film::Reel empty;
        check(film::businessAt(empty, 0, "SHAY", 0.5).sit == 0.0f, "an empty reel does nothing");
        check(film::businessAt(reel, -1, "SHAY", 0.5).sit == 0.0f, "nor does a shot before the first");
        check(film::businessAt(reel, 900, "SHAY", 0.5).sit == 0.0f, "nor one past the last");
        check(!film::businessAt(reel, 3, "NOBODY", 0.5).holding, "and a stranger holds nothing");
        const float clamped = film::businessAt(reel, 3, "SHAY", 7.0).sit;
        check(clamped >= 0.0f && clamped <= 1.0f, "and a shot played past its end stays in range");
    }

    if (!failures.empty()) {
        for (const std::string& f : failures) {
            std::printf("FAIL: %s\n", f.c_str());
        }
        std::printf("%zu failed\n", failures.size());
        return 1;
    }
    std::printf("business: all checks passed\n");
    return 0;
}
