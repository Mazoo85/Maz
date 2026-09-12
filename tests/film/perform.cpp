// tests/film/perform.cpp — the walk, checked against the thing everybody gets wrong.
//
// Everybody gets the same thing wrong: the feet slide. A character is moved forward at a speed and
// their legs are swung on a timer, and the two do not agree, so the planted foot skates along the
// ground. It is the single most recognisable failure in cheap character animation, and it is not a
// matter of taste — it is measurable, and it is measured here: while a foot is on the ground, its
// position IN THE WORLD must not change.
//
// The rest is the physiology a walk has to have and is easy to leave out: the pelvis rises and falls
// TWICE per stride and swings side to side ONCE (get that the wrong way round and the figure bounces
// like a hobby-horse); the hips and shoulders turn opposite ways; the arms swing opposite the legs;
// somebody is always in contact with the floor; and the leg solve is never asked for more than a leg
// can do.
#include "maz/film/Perform.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace film = maz::film;
namespace math = maz::math;

static std::vector<std::string> failures;

static void check(bool ok, const std::string& what) {
    if (!ok) {
        failures.push_back(what);
    }
}
static void near(float got, float want, float tol, const std::string& what) {
    if (!(std::fabs(got - want) <= tol)) {
        char buf[256];
        std::snprintf(buf, sizeof buf, "%s (got %.5f, wanted %.5f +/- %.5f)", what.c_str(),
                      static_cast<double>(got), static_cast<double>(want), static_cast<double>(tol));
        failures.push_back(buf);
    }
}

// Walk a character along +Z at a steady speed and hand back the frames.
struct Frame {
    film::Skeleton sk;
    float time = 0.0f;
    math::vec3 foot[2];
    bool planted[2] = {false, false};
};

static std::vector<Frame> walkFrames(const film::Build& b, float speed, int frames, float fps) {
    std::vector<Frame> out;
    out.reserve(static_cast<std::size_t>(frames));
    for (int i = 0; i < frames; ++i) {
        const float t = static_cast<float>(i) / fps;
        film::Motive mv;
        mv.walkSpeed = speed;
        mv.travelled = speed * t;
        mv.position = math::vec3(0.0f, 0.0f, speed * t);
        const film::Pose pose = film::performAt(b, mv, t);
        Frame f;
        f.time = t;
        f.sk = film::skeletonOf(b, pose);
        for (int s = 0; s < 2; ++s) {
            f.foot[s] = film::Skeleton::at(f.sk.ankle[s]);
            // Planted means the gait says this foot is down: the ankle is at its resting height.
            // Down means the gait has this foot in its stance, which the lift curve decides; the
            // ankle's own height also moves with the roll of the foot, so it cannot be the test.
            f.planted[s] = film::footAt(mv.travelled / (2.0f * film::kStep * b.height) +
                                            (s == film::kLeft ? 0.5f : 0.0f),
                                        film::kStep * b.height)
                               .planted;
        }
        out.push_back(f);
    }
    return out;
}

int main() {
    const film::Build b = film::adultMale();
    const float H = b.height;
    const float speed = 0.78f * H; // a normal walking pace, about 1.4 m/s for a man of average height

    // ------------------------------------------------------------------ 1. a planted foot does not
    //                                                                        move. this is the test.
    {
        const std::vector<Frame> fr = walkFrames(b, speed, 240, 60.0f);
        float worstSlip = 0.0f;
        int contiguous = 0;
        for (std::size_t i = 1; i < fr.size(); ++i) {
            for (int s = 0; s < 2; ++s) {
                if (fr[i].planted[s] && fr[i - 1].planted[s]) {
                    // HORIZONTALLY. The ankle rises and falls through a stance as the foot rolls from
                    // heel to toe, and that is right; what must not move is where the foot is on the
                    // floor.
                    const math::vec3 d = fr[i].foot[s] - fr[i - 1].foot[s];
                    const float slip = std::sqrt(d.x * d.x + d.z * d.z);
                    worstSlip = slip > worstSlip ? slip : worstSlip;
                    ++contiguous;
                }
            }
        }
        check(contiguous > 200, "there are plenty of frames with a foot down to check");
        // A tenth of a millimetre over four seconds of walking. Anything a viewer could see is a
        // hundred times this.
        near(worstSlip, 0.0f, 1e-4f, "a foot on the ground does not move while it is on the ground");
    }

    // ------------------------------------------------------------------ 2. somebody is always on the
    //                                                                        floor
    {
        const std::vector<Frame> fr = walkFrames(b, speed, 240, 60.0f);
        int airborne = 0;
        for (const Frame& f : fr) {
            if (!f.planted[0] && !f.planted[1]) {
                ++airborne;
            }
        }
        check(airborne == 0, "walking is not running: at least one foot is always down");

        // And the feet take turns, rather than both being down the whole time.
        int bothDown = 0;
        for (const Frame& f : fr) {
            if (f.planted[0] && f.planted[1]) {
                ++bothDown;
            }
        }
        const float share = static_cast<float>(bothDown) / static_cast<float>(fr.size());
        near(share, 0.24f, 0.06f,
             "both feet are down for about a quarter of the stride, which is what double support is");
    }

    // ------------------------------------------------------------------ 3. the pelvis rises twice and
    //                                                                        sways once per stride
    {
        const float stride = 2.0f * film::kStep * H;
        const int N = 600;
        std::vector<float> up(static_cast<std::size_t>(N));
        std::vector<float> side(static_cast<std::size_t>(N));
        for (int i = 0; i < N; ++i) {
            film::Motive mv;
            mv.walkSpeed = speed;
            mv.travelled = stride * 2.0f * static_cast<float>(i) / static_cast<float>(N); // two strides
            const film::Pose p = film::performAt(b, mv, mv.travelled / speed);
            up[static_cast<std::size_t>(i)] = p.hips.y;
            side[static_cast<std::size_t>(i)] = p.hips.x;
        }
        auto peaks = [](const std::vector<float>& v) {
            int n = 0;
            for (std::size_t i = 1; i + 1 < v.size(); ++i) {
                if (v[i] > v[i - 1] && v[i] >= v[i + 1]) {
                    ++n;
                }
            }
            return n;
        };
        check(peaks(up) == 4, "the hips rise and fall twice per stride, so four times in two strides");
        check(peaks(side) == 2, "and swing from side to side once per stride, so twice in two");

        float loY = 1e9f, hiY = -1e9f, loX = 1e9f, hiX = -1e9f;
        for (int i = 0; i < N; ++i) {
            const std::size_t k = static_cast<std::size_t>(i);
            loY = std::fmin(loY, up[k]);
            hiY = std::fmax(hiY, up[k]);
            loX = std::fmin(loX, side[k]);
            hiX = std::fmax(hiX, side[k]);
        }
        near((hiY - loY) / H, film::kPelvisBob, 0.004f, "the rise and fall is about four centimetres");
        near((hiX - loX) / H, 2.0f * film::kPelvisSway, 0.006f, "and so is the side to side");
    }

    // ------------------------------------------------------------------ 4. hips and shoulders turn
    //                                                                        opposite ways
    {
        bool everOpposed = false;
        bool everTogether = false;
        for (int i = 0; i < 60; ++i) {
            film::Motive mv;
            mv.walkSpeed = speed;
            mv.travelled = 2.0f * film::kStep * H * static_cast<float>(i) / 60.0f;
            const film::Pose p = film::performAt(b, mv, 0.0f);
            if (std::fabs(p.pelvisTwist) > 0.01f) {
                if (p.pelvisTwist * p.twist < 0.0f) {
                    everOpposed = true;
                } else {
                    everTogether = true;
                }
            }
        }
        check(everOpposed, "the hips and the ribcage turn against each other");
        check(!everTogether, "and never together, which is how a plank walks");
    }

    // ------------------------------------------------------------------ 5. arms swing opposite the
    //                                                                        legs
    {
        float agreement = 0.0f;
        for (int i = 0; i < 120; ++i) {
            film::Motive mv;
            mv.walkSpeed = speed;
            mv.travelled = 2.0f * film::kStep * H * static_cast<float>(i) / 60.0f;
            const film::Pose p = film::performAt(b, mv, 0.0f);
            // The right foot's forward position against the right arm's swing.
            agreement += p.ankle[film::kRight].z * p.arm[film::kRight].swing;
        }
        check(agreement < 0.0f, "the right arm goes back as the right leg comes forward");
    }

    // ------------------------------------------------------------------ 6. the legs are never asked
    //                                                                        for more than a leg can do
    {
        for (float sp : {0.35f * H, 0.62f * H, 0.78f * H, 0.95f * H}) {
            const std::vector<Frame> fr = walkFrames(b, sp, 180, 60.0f);
            int overreached = 0;
            for (const Frame& f : fr) {
                if (!f.sk.reached[0] || !f.sk.reached[1]) {
                    ++overreached;
                }
            }
            check(overreached == 0, "at every walking pace, both legs can reach where the gait puts them");
        }
        // And the same for the other two builds, whose legs are different lengths.
        for (const film::Build& other : {film::adultFemale(), film::child()}) {
            const std::vector<Frame> fr = walkFrames(other, 0.78f * other.height, 180, 60.0f);
            int overreached = 0;
            for (const Frame& f : fr) {
                if (!f.sk.reached[0] || !f.sk.reached[1]) {
                    ++overreached;
                }
            }
            check(overreached == 0, "and for a woman and a child, whose legs are not a man's length");
        }
    }

    // ------------------------------------------------------------------ 7. standing is alive, but not
    //                                                                        fidgeting
    {
        film::Motive mv;
        mv.seed = 7u;
        math::vec3 firstHead = film::performSkeleton(b, mv, 0.0f).eyes(b);
        float mostMoved = 0.0f;
        float everMoved = 0.0f;
        for (int i = 1; i <= 240; ++i) {
            const float t = static_cast<float>(i) / 24.0f; // ten seconds
            const math::vec3 head = film::performSkeleton(b, mv, t).eyes(b);
            const math::vec3 d = head - firstHead;
            const float moved = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
            mostMoved = moved > mostMoved ? moved : mostMoved;
            everMoved += moved;
        }
        check(everMoved > 0.0f, "somebody standing still is not a photograph");
        check(mostMoved > 0.004f, "they breathe, and their weight shifts");
        check(mostMoved < 0.09f, "but they are standing, not swaying about");

        // Two people standing together do not breathe in unison.
        film::Motive other = mv;
        other.seed = 41u;
        const math::vec3 a = film::performSkeleton(b, mv, 3.3f).eyes(b);
        const math::vec3 c = film::performSkeleton(b, other, 3.3f).eyes(b);
        check(std::fabs(a.y - c.y) > 1e-5f, "and two of them do not breathe in step");

        // Whichever leg has the weight, both feet stay on the floor.
        for (int i = 0; i <= 200; ++i) {
            const film::Pose p = film::performAt(b, mv, static_cast<float>(i) * 0.13f);
            near(p.ankle[0].y, b.m(b.yAnkle), 1e-5f, "a standing foot stays on the floor");
            near(p.ankle[1].y, b.m(b.yAnkle), 1e-5f, "both of them, flat, with no heel lifted");
        }
    }

    // ------------------------------------------------------------------ 8. speaking moves one hand,
    //                                                                        on the syllable
    {
        film::Motive quiet;
        quiet.seed = 3u;
        film::Motive talking = quiet;
        talking.speaking = 1.0f;
        talking.syllable = 0.5f; // the middle of a syllable, where the gesture peaks
        const film::Skeleton q = film::performSkeleton(b, quiet, 2.0f);
        const film::Skeleton t = film::performSkeleton(b, talking, 2.0f);
        auto moved = [&](int side) {
            const math::vec3 d = t.handAt(b, side) - q.handAt(b, side);
            return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        };
        check(moved(film::kRight) > 0.10f, "a speaker's leading hand comes up");
        check(moved(film::kRight) > moved(film::kLeft) * 2.0f,
              "and leads: two hands doing the same thing at once is semaphore");

        // The gesture is on the syllable clock, not a clock of its own.
        film::Motive between = talking;
        between.syllable = 0.0f;
        const float peak = (film::performSkeleton(b, talking, 2.0f).handAt(b, film::kRight) -
                            q.handAt(b, film::kRight))
                               .y;
        const float trough = (film::performSkeleton(b, between, 2.0f).handAt(b, film::kRight) -
                              q.handAt(b, film::kRight))
                                 .y;
        check(peak > trough, "the hand is higher in the middle of a syllable than at the edge of one");
    }

    // ------------------------------------------------------------------ 9. a big look takes the
    //                                                                        shoulders with it
    {
        film::Motive small;
        small.lookYaw = 0.5f;
        film::Motive big = small;
        big.lookYaw = 1.5f;
        const film::Pose ps = film::performAt(b, small, 0.0f);
        const film::Pose pb = film::performAt(b, big, 0.0f);
        film::Motive ahead;
        near(ps.twist, film::performAt(b, ahead, 0.0f).twist, 1e-6f,
             "a small look is done with the neck alone, and leaves the trunk exactly as it was");
        check(std::fabs(pb.twist) > 0.2f, "a big one takes the shoulders round too, or it is an owl");
        check(std::fabs(pb.headYaw) < std::fabs(big.lookYaw),
              "and the neck does not do all of a big look by itself");
    }

    // ------------------------------------------------------------------ 10. the same input gives the
    //                                                                         same performance
    {
        film::Motive mv;
        mv.seed = 99u;
        mv.walkSpeed = speed;
        mv.travelled = 3.77f;
        mv.speaking = 0.4f;
        mv.syllable = 0.3f;
        const film::Pose a = film::performAt(b, mv, 1.234f);
        const film::Pose c = film::performAt(b, mv, 1.234f);
        near(a.hips.y, c.hips.y, 0.0f, "a performance is the same performance every time it is asked for");
        near(a.arm[0].elbow, c.arm[0].elbow, 0.0f, "down to the last joint");
    }

    if (!failures.empty()) {
        for (const std::string& f : failures) {
            std::printf("FAIL: %s\n", f.c_str());
        }
        std::printf("%zu failed\n", failures.size());
        return 1;
    }
    std::printf("perform: all checks passed\n");
    return 0;
}
