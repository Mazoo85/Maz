// tests/film/actor.cpp — a person, checked against the proportions of a person.
//
// The old figures were flat shapes sized by eye, and "by eye" is exactly how a figure ends up with the
// head of a child on the legs of a basketball player without anyone noticing. So the body is built to
// the figure-drawing canon, and the canon is what this checks: a landmark that drifts by a couple of
// per cent is invisible in one still and unmistakable the moment two characters stand together.
//
// It also checks the two things that are not proportions and matter just as much: that a joint moves
// only what hangs off it, and that the leg solve never stretches a bone to reach the floor.
#include "maz/film/Actor.hpp"

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
        std::snprintf(buf, sizeof buf, "%s (got %.4f, wanted %.4f +/- %.4f)", what.c_str(),
                      static_cast<double>(got), static_cast<double>(want), static_cast<double>(tol));
        failures.push_back(buf);
    }
}

static float dist(const math::vec3& a, const math::vec3& b) {
    const math::vec3 d = a - b;
    return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

int main() {
    // ------------------------------------------------------------------ 1. the canon, in the skeleton
    {
        const film::Build b = film::adultMale();
        const film::Skeleton sk = film::skeletonOf(b, film::restPose(b));
        const float H = b.height;
        auto frac = [&](const math::mat4& m) { return film::Skeleton::at(m).y / H; };

        near(frac(sk.shoulder[film::kRight]), 0.815f, 0.002f, "the shoulder is at 0.815 of the height");
        near(frac(sk.elbow[film::kRight]), 0.625f, 0.004f, "the elbow is level with the navel, 0.625");
        near(frac(sk.wrist[film::kRight]), 0.485f, 0.006f, "the wrist is level with the crotch, 0.485");
        near(frac(sk.hip[film::kRight]), 0.500f, 0.002f, "the hip joint is halfway up");
        near(frac(sk.knee[film::kRight]), 0.285f, 0.003f,
             "the knee is BELOW halfway down the leg, at 0.285");
        near(frac(sk.ankle[film::kRight]), 0.045f, 0.002f, "the ankle is 0.045 above the floor");
        near(sk.crown(b).y / H, 1.000f, 0.002f, "the crown is the top of the figure");

        // The drawing rule for arm length: with the arms hanging, the fingertips reach mid-thigh.
        const float midThigh = (0.500f + 0.285f) * 0.5f;
        near(sk.fingertipAt(b, film::kRight).y / H, midThigh, 0.035f,
             "the fingertips reach mid-thigh with the arms down");

        // Seven and a half heads, and the eyes halfway up the head rather than near the top.
        near(b.headHalfH() * 2.0f, 1.0f / 7.5f, 0.001f, "an adult is seven and a half heads tall");
        const float chin = sk.crown(b).y - b.m(b.headHalfH() * 2.0f);
        near((sk.eyes(b).y - chin) / (sk.crown(b).y - chin), 0.57f, 0.06f,
             "the eyes sit around the middle of the head, not up by the hairline");
    }

    // ------------------------------------------------------------------ 2. a child is not a small
    //                                                                        adult
    {
        const film::Build kid = film::child();
        const film::Build man = film::adultMale();
        check(kid.height < man.height, "a child is shorter");
        check(kid.headHalfH() * 2.0f > man.headHalfH() * 2.0f * 1.15f,
              "and their head is a BIGGER share of them, which is the whole difference");
        const film::Skeleton sk = film::skeletonOf(kid, film::restPose(kid));
        near(sk.crown(kid).y / kid.height, 1.0f, 0.003f, "the child's crown is still the top of them");
    }

    // ------------------------------------------------------------------ 3. a woman's build differs
    //                                                                        where a woman's build differs
    {
        const film::Build w = film::adultFemale();
        const film::Build man = film::adultMale();
        check(w.yokeHalfW < man.yokeHalfW, "narrower across the shoulders");
        check(w.pelvisHalfW > man.pelvisHalfW, "wider at the hip");
        check(w.pelvisHalfW / w.waistHalfW > man.pelvisHalfW / man.waistHalfW,
              "and a greater difference between waist and hip, which is what reads at a distance");
        // The proportions still hold: the canon is not a male canon.
        const film::Skeleton sk = film::skeletonOf(w, film::restPose(w));
        near(film::Skeleton::at(sk.shoulder[film::kRight]).y / w.height, 0.815f, 0.002f,
             "a woman's shoulder is at 0.815 of her height too");
    }

    // ------------------------------------------------------------------ 4. the soles are on the floor
    //                                                                        and the crown is the top
    {
        for (const film::Build& b : {film::adultMale(), film::adultFemale(), film::child()}) {
            const maz::render::shapes::MeshData mesh = film::buildActor(b, film::restPose(b));
            check(!mesh.vertices.empty(), "the body has a surface");
            float lo = 1e9f;
            float hi = -1e9f;
            for (const maz::render::MeshVertex& v : mesh.vertices) {
                lo = v.py < lo ? v.py : lo;
                hi = v.py > hi ? v.py : hi;
            }
            near(lo, 0.0f, 0.004f, "nothing of the figure is below the floor, and nothing floats");
            near(hi, b.height, 0.006f, "and the top of the figure is its stated height");
        }
    }

    // ------------------------------------------------------------------ 5. left and right match
    {
        const film::Build b = film::adultMale();
        const film::Skeleton sk = film::skeletonOf(b, film::restPose(b));
        for (int j = 0; j < 3; ++j) {
            const math::mat4* pair[3][2] = {{&sk.shoulder[0], &sk.shoulder[1]},
                                            {&sk.elbow[0], &sk.elbow[1]},
                                            {&sk.knee[0], &sk.knee[1]}};
            const math::vec3 r = film::Skeleton::at(*pair[j][0]);
            const math::vec3 l = film::Skeleton::at(*pair[j][1]);
            near(r.x, -l.x, 1e-4f, "standing at rest, the two sides mirror each other");
            near(r.y, l.y, 1e-4f, "at the same height");
            near(r.z, l.z, 1e-4f, "and at the same depth");
        }
        // And the character's right really is at negative x when they face +Z.
        check(film::Skeleton::at(sk.shoulder[film::kRight]).x < 0.0f,
              "someone facing you has their right hand on your left");
    }

    // ------------------------------------------------------------------ 6. a joint moves what hangs
    //                                                                        off it, and nothing else
    {
        const film::Build b = film::adultMale();
        const film::Pose rest = film::restPose(b);
        const film::Skeleton before = film::skeletonOf(b, rest);

        film::Pose bent = rest;
        bent.arm[film::kRight].elbow = 1.2f;
        const film::Skeleton after = film::skeletonOf(b, bent);

        check(dist(after.handAt(b, film::kRight), before.handAt(b, film::kRight)) > 0.15f,
              "bending the right elbow moves the right hand a long way");
        check(film::Skeleton::at(after.wrist[film::kRight]).z >
                  film::Skeleton::at(before.wrist[film::kRight]).z + 0.1f,
              "and it moves it FORWARD, because that is the only way an elbow bends");
        near(dist(after.handAt(b, film::kLeft), before.handAt(b, film::kLeft)), 0.0f, 1e-5f,
             "the other hand has not moved");
        near(dist(film::Skeleton::at(after.ankle[film::kRight]),
                  film::Skeleton::at(before.ankle[film::kRight])),
             0.0f, 1e-5f, "nor has either foot");
        near(dist(after.crown(b), before.crown(b)), 0.0f, 1e-5f, "nor the head");

        // An elbow that bends backwards is the most alarming thing a figure can do, so it cannot.
        film::Pose backwards = rest;
        backwards.arm[film::kRight].elbow = -1.2f;
        const film::Skeleton hyper = film::skeletonOf(b, backwards);
        near(dist(hyper.handAt(b, film::kRight), film::skeletonOf(b, [&] {
                     film::Pose z = rest;
                     z.arm[film::kRight].elbow = 0.0f;
                     return z;
                 }()).handAt(b, film::kRight)),
             0.0f, 1e-5f, "an elbow will not bend backwards, however hard it is asked to");

        // The head turns the head, not the chest.
        film::Pose looking = rest;
        looking.headYaw = 0.6f;
        const film::Skeleton turned = film::skeletonOf(b, looking);
        check(dist(turned.eyes(b), before.eyes(b)) > 0.01f, "turning the head moves the eyes");
        near(dist(film::Skeleton::at(turned.shoulder[film::kRight]),
                  film::Skeleton::at(before.shoulder[film::kRight])),
             0.0f, 1e-5f, "and leaves the shoulders where they were");
    }

    // ------------------------------------------------------------------ 7. the legs solve, and the
    //                                                                        bones keep their length
    {
        const film::Build b = film::adultMale();
        const float thigh = b.m(b.yHip - b.yKnee);
        const float shin = b.m(b.yKnee - b.yAnkle);

        // Walk one foot forward, back, and lift the hips up and down — the bones must not stretch.
        for (int step = -6; step <= 6; ++step) {
            for (int drop = 0; drop <= 4; ++drop) {
                film::Pose p = film::restPose(b);
                p.ankle[film::kRight].z = static_cast<float>(step) * 0.06f;
                p.hips.y = b.m(b.yPelvis) - static_cast<float>(drop) * 0.035f;
                const film::Skeleton sk = film::skeletonOf(b, p);
                const math::vec3 hip = film::Skeleton::at(sk.hip[film::kRight]);
                const math::vec3 knee = film::Skeleton::at(sk.knee[film::kRight]);
                const math::vec3 ankle = film::Skeleton::at(sk.ankle[film::kRight]);
                near(dist(hip, knee), thigh, 1e-3f, "the thigh keeps its length whatever the foot does");
                near(dist(knee, ankle), shin, 1e-3f, "and so does the shin");
                if (sk.reached[film::kRight]) {
                    near(ankle.z, p.ankle[film::kRight].z, 1e-4f,
                         "and a foot the leg CAN reach is exactly where it was put");
                } else {
                    // Out of reach: the foot falls short of the target rather than the leg growing.
                    check(std::fabs(ankle.z) < std::fabs(p.ankle[film::kRight].z) + 1e-4f,
                          "a foot the leg cannot reach falls short rather than stretching the leg");
                }
            }
        }

        // A bent knee points FORWARD. A knee that solves to the other side is the single most
        // recognisable way a walk goes wrong.
        film::Pose crouch = film::restPose(b);
        crouch.hips.y = b.m(b.yPelvis) - 0.20f;
        const film::Skeleton sk = film::skeletonOf(b, crouch);
        check(film::Skeleton::at(sk.knee[film::kRight]).z > 0.02f,
              "crouching bends the knee forward, not backward");
    }

    // ------------------------------------------------------------------ 8. facing turns the whole
    //                                                                        person, once
    {
        const film::Build b = film::adultMale();
        film::Pose p = film::restPose(b);
        const math::vec3 straightOn = film::skeletonOf(b, p).eyes(b);
        p.facing = 3.14159265f;
        const film::Skeleton turned = film::skeletonOf(b, p);
        near(turned.eyes(b).z, -straightOn.z, 1e-4f, "turned around, the eyes look the other way");
        near(turned.eyes(b).y, straightOn.y, 1e-4f, "at the same height");
        check(film::Skeleton::at(turned.shoulder[film::kRight]).x > 0.0f,
              "and their right shoulder has come round to the other side");

        // Standing somewhere else moves everything by exactly that much.
        film::Pose moved = film::restPose(b);
        moved.position = math::vec3(2.0f, 0.0f, -3.0f);
        const film::Skeleton there = film::skeletonOf(b, moved);
        near(there.crown(b).x, straightOn.x + 2.0f, 1e-4f, "standing two metres over puts them there");
        near(there.crown(b).z, film::skeletonOf(b, film::restPose(b)).crown(b).z - 3.0f, 1e-4f,
             "and three metres back");
    }

    if (!failures.empty()) {
        for (const std::string& f : failures) {
            std::printf("FAIL: %s\n", f.c_str());
        }
        std::printf("%zu failed\n", failures.size());
        return 1;
    }
    std::printf("actor: all checks passed\n");
    return 0;
}
