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

#include <algorithm>

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
        // The femoral head, not the crotch. Halfway up is the crotch; the joint the leg swings from
        // is higher, and the difference is a leg that can reach the end of a stride.
        near(frac(sk.hip[film::kRight]), 0.530f, 0.002f, "the hip joint is at 0.53, inside the pelvis");
        near(frac(sk.knee[film::kRight]), 0.285f, 0.003f,
             "the knee is BELOW halfway down the leg, at 0.285");
        near(frac(sk.ankle[film::kRight]), 0.039f, 0.002f, "the ankle is 0.039 above the floor");
        near(sk.crown(b).y / H, 1.000f, 0.002f, "the crown is the top of the figure");

        // The drawing rule for arm length: with the arms hanging, the fingertips reach mid-thigh.
        const float midThigh = (0.480f + 0.285f) * 0.5f; // between the crotch and the knee
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

    // ------------------------------------------------------------------ 4b. the body is not inside out
    {
        // Every piece of the body is a closed surface, so the signed volume of its triangles — the
        // divergence-theorem sum — is POSITIVE when they are wound outward and negative when they are
        // not. Nothing else here would notice: a limb built inside out has the same silhouette, and
        // the renderer simply draws its far surface instead of its near one, which on a smooth tube
        // looks very nearly the same. It was wrong for a long time and what finally showed it was
        // shadows, where inside-out geometry records the near surface of everything and puts every lit
        // surface into its own shadow.
        auto signedVolume = [](const maz::render::shapes::MeshData& m) {
            double v = 0.0;
            for (std::size_t i = 0; i + 2 < m.indices.size(); i += 3) {
                const maz::render::MeshVertex& a = m.vertices[m.indices[i]];
                const maz::render::MeshVertex& b2 = m.vertices[m.indices[i + 1]];
                const maz::render::MeshVertex& c = m.vertices[m.indices[i + 2]];
                v += (static_cast<double>(a.px) *
                          (static_cast<double>(b2.py) * static_cast<double>(c.pz) -
                           static_cast<double>(b2.pz) * static_cast<double>(c.py)) -
                      static_cast<double>(a.py) *
                          (static_cast<double>(b2.px) * static_cast<double>(c.pz) -
                           static_cast<double>(b2.pz) * static_cast<double>(c.px)) +
                      static_cast<double>(a.pz) *
                          (static_cast<double>(b2.px) * static_cast<double>(c.py) -
                           static_cast<double>(b2.py) * static_cast<double>(c.px))) /
                     6.0;
            }
            return v;
        };
        const film::Build b = film::adultMale();
        // Wound outward the whole body comes to about 0.04 cubic metres; wound inward it collapses to
        // 0.002, because the inverted pieces subtract instead of adding. (Neither is a person's actual
        // volume — the lofts are open at their ends and the pieces overlap — so this is a check on the
        // WINDING, not a measurement of the man.) Plain "> 0" would pass either way: the head, the
        // hands and the shoes are spheres and boxes and are wound correctly whatever the lofts do.
        const double bodyVolume = signedVolume(film::buildActor(b, film::restPose(b)));
        check(bodyVolume > 0.020, "the body is wound outward, not inside out");
        check(bodyVolume < 0.120, "and is a body's worth of volume, not a runaway one");

        // And the vertex normals agree: a lofted limb's normals point AWAY from its own axis.
        const math::vec3 top(0.0f, 0.5f, 0.0f);
        const math::vec3 bottom(0.0f, -0.5f, 0.0f);
        const maz::render::shapes::MeshData one =
            film::detail::limb(top, bottom, 0.08f, 0.06f, maz::render::Color{1.0f, 1.0f, 1.0f, 1.0f});
        check(signedVolume(one) > 0.0, "and so is a limb on its own");
        double outward = 0.0;
        int counted = 0;
        for (const maz::render::MeshVertex& v : one.vertices) {
            const math::vec3 away(v.px, 0.0f, v.pz); // straight out from the limb's axis
            const float len = std::sqrt(away.x * away.x + away.z * away.z);
            if (len < 1e-3f) {
                continue;                            // on the axis, at a cap tip: says nothing
            }
            outward += static_cast<double>(math::dot(math::vec3(v.nx, v.ny, v.nz), away / len));
            ++counted;
        }
        check(counted > 40 && outward > 0.0, "with its normals pointing out of it, not into it");
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
        const film::BodyPose rest = film::restPose(b);
        const film::Skeleton before = film::skeletonOf(b, rest);

        film::BodyPose bent = rest;
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
        film::BodyPose backwards = rest;
        backwards.arm[film::kRight].elbow = -1.2f;
        const film::Skeleton hyper = film::skeletonOf(b, backwards);
        near(dist(hyper.handAt(b, film::kRight), film::skeletonOf(b, [&] {
                     film::BodyPose z = rest;
                     z.arm[film::kRight].elbow = 0.0f;
                     return z;
                 }()).handAt(b, film::kRight)),
             0.0f, 1e-5f, "an elbow will not bend backwards, however hard it is asked to");

        // The head turns the head, not the chest.
        film::BodyPose looking = rest;
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
                film::BodyPose p = film::restPose(b);
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
        film::BodyPose crouch = film::restPose(b);
        crouch.hips.y = b.m(b.yPelvis) - 0.20f;
        const film::Skeleton sk = film::skeletonOf(b, crouch);
        check(film::Skeleton::at(sk.knee[film::kRight]).z > 0.02f,
              "crouching bends the knee forward, not backward");
    }

    // ------------------------------------------------------------------ 8. facing turns the whole
    //                                                                        person, once
    {
        const film::Build b = film::adultMale();
        film::BodyPose p = film::restPose(b);
        const math::vec3 straightOn = film::skeletonOf(b, p).eyes(b);
        p.facing = 3.14159265f;
        const film::Skeleton turned = film::skeletonOf(b, p);
        near(turned.eyes(b).z, -straightOn.z, 1e-4f, "turned around, the eyes look the other way");
        near(turned.eyes(b).y, straightOn.y, 1e-4f, "at the same height");
        check(film::Skeleton::at(turned.shoulder[film::kRight]).x > 0.0f,
              "and their right shoulder has come round to the other side");

        // Standing somewhere else moves everything by exactly that much.
        film::BodyPose moved = film::restPose(b);
        moved.position = math::vec3(2.0f, 0.0f, -3.0f);
        const film::Skeleton there = film::skeletonOf(b, moved);
        near(there.crown(b).x, straightOn.x + 2.0f, 1e-4f, "standing two metres over puts them there");
        near(there.crown(b).z, film::skeletonOf(b, film::restPose(b)).crown(b).z - 3.0f, 1e-4f,
             "and three metres back");
    }

    // ------------------------------------------------------------------ what they are wearing
    //
    // The silhouette is who somebody is at the distance a film watches people from. Each of these
    // checks that a garment changes the OUTLINE — a coat that only changes a colour is a colour.
    {
        // How much cloth there is, and where. Measured off the mesh rather than off the numbers that
        // built it, so a field that nothing reads cannot pass.
        struct Cut {
            float wideLow = 0.0f;  // widest cloth between the knee and the hip, as a fraction of height
            float lowestTop = 1e9f; // how far down the body the top's own colour reaches
            float wideArm = 0.0f;  // widest top-coloured cloth at forearm height: the sleeve
            float atHem = 0.0f;     // and the garment's own width, near the hem
            float aboveHem = 0.0f;  // and a hand's width higher up
        };
        auto cutOf = [](const film::Build& b) {
            const maz::render::shapes::MeshData m = film::buildActor(b, film::restPose(b));
            Cut c;
            for (const auto& v : m.vertices) {
                const bool isTop = std::fabs(v.r - b.top.r) < 0.005f &&
                                   std::fabs(v.g - b.top.g) < 0.005f &&
                                   std::fabs(v.b - b.top.b) < 0.005f;
                const bool isLeg = std::fabs(v.r - b.legwear.r) < 0.005f &&
                                   std::fabs(v.g - b.legwear.g) < 0.005f &&
                                   std::fabs(v.b - b.legwear.b) < 0.005f;
                const float y = v.py / b.height;
                const float x = std::fabs(v.px) / b.height;
                if ((isTop || isLeg) && y > 0.30f && y < 0.46f) {
                    c.wideLow = std::max(c.wideLow, x);
                }
                // Two bands, both BELOW the hands, so what is measured is the garment and not an arm.
                if (isTop && y > 0.32f && y < 0.40f) {
                    c.atHem = std::max(c.atHem, x);
                }
                if (isTop && y > 0.44f && y < 0.52f) {
                    c.aboveHem = std::max(c.aboveHem, x);
                }
                if (isTop) {
                    c.lowestTop = std::min(c.lowestTop, y);
                    // Between the elbow and the wrist the torso is long finished, so anything in the
                    // top's colour out here is a sleeve and nothing else.
                    if (y > 0.56f && y < 0.66f) {
                        c.wideArm = std::max(c.wideArm, x);
                    }
                }
            }
            return c;
        };

        const film::Build plain = film::adultMale();
        film::Build coat = plain;
        coat.coatY = 0.33f;
        const Cut bare = cutOf(plain);
        const Cut wrapped = cutOf(coat);
        check(wrapped.lowestTop < bare.lowestTop - 0.10f,
              "a coat puts cloth a long way below where a shirt stops");
        check(wrapped.wideLow > bare.wideLow * 1.15f,
              "and it is wider down there than the legs are");
        // A hem is always wider than the waist above it. A garment that does not open out is a tube of
        // paint, and it is the flare that makes a coat read as a coat from behind.
        check(wrapped.atHem > wrapped.aboveHem * 1.06f, "and it flares on the way down, as a hem does");

        const film::Build trousers = film::adultFemale();
        film::Build skirt = trousers;
        skirt.skirtY = 0.40f;
        check(cutOf(skirt).wideLow > cutOf(trousers).wideLow * 1.2f, "and so does a skirt");

        // And it is not inside out. This is the mistake that cost an afternoon on the head — every
        // triangle facing inward, so the renderer culls the surface facing the camera and draws the
        // far one instead — and a lofted garment is built exactly the same way, so it is exactly as
        // easy to get wrong. The signed volume of a surface comes out negative when it happens.
        {
            const maz::render::shapes::MeshData cloth = film::detail::hanging(
                math::mat4(1.0f), 0.0f, -0.60f, 0.16f, 0.11f, 0.24f, 0.17f, plain.top);
            double v = 0.0;
            for (std::size_t t = 0; t + 2 < cloth.indices.size(); t += 3) {
                const auto& p0 = cloth.vertices[cloth.indices[t + 0]];
                const auto& p1 = cloth.vertices[cloth.indices[t + 1]];
                const auto& p2 = cloth.vertices[cloth.indices[t + 2]];
                v += static_cast<double>(p0.px * (p1.py * p2.pz - p2.py * p1.pz) -
                                         p0.py * (p1.px * p2.pz - p2.px * p1.pz) +
                                         p0.pz * (p1.px * p2.py - p2.px * p1.py)) /
                     6.0;
            }
            check(v > 0.0, "and a coat is wound the right way out, like everything else that is lofted");
        }

        film::Build sleeves = plain;
        sleeves.sleeve = 1.55f;
        check(cutOf(sleeves).wideArm > cutOf(plain).wideArm * 1.5f,
              "a long sleeve puts cloth down at the forearm; a short one leaves it bare");
    }

    // ------------------------------------------------------------------ and their hair
    {
        auto hairBelow = [](const film::Build& b, float y) {
            const maz::render::shapes::MeshData m = film::buildActor(b, film::restPose(b));
            int n = 0;
            for (const auto& v : m.vertices) {
                if (std::fabs(v.r - b.hair.r) < 0.005f && std::fabs(v.g - b.hair.g) < 0.005f &&
                    std::fabs(v.b - b.hair.b) < 0.005f && v.py < y) {
                    ++n;
                }
            }
            return n;
        };
        const film::Build cropped = film::adultFemale();
        film::Build flowing = cropped;
        flowing.hairY = 0.62f;
        const float chin = (1.0f - 1.0f / cropped.heads) * cropped.height;
        check(hairBelow(cropped, chin) == 0, "short hair stops at the head");
        check(hairBelow(flowing, chin) > 20, "and long hair carries on down past the jaw");

        // And it falls BEHIND the face. The first version bridged the fall onto the cap, which meant
        // the surface had to get from a hairline that is high at the forehead and low at the nape down
        // to a level ring — and the only way across the front is a sheet straight down the face. It
        // looked exactly like somebody wearing their hair over their eyes.
        //
        // The window is the face proper: below the brow, inside the width of the features. Above it is
        // the forehead, where a fringe is meant to be, and out at the temples hair comes forward
        // because that is what hair does.
        const film::Skeleton sk = film::skeletonOf(flowing, film::restPose(flowing));
        const math::vec3 head = film::Skeleton::at(sk.head);
        const float hw = flowing.m(flowing.headHalfW());
        const float hh = flowing.m(flowing.headHalfH());
        const float hd = flowing.m(flowing.headHalfD());
        const maz::render::shapes::MeshData m = film::buildActor(flowing, film::restPose(flowing));
        int overTheFace = 0;
        float worst = -1e9f;
        for (const auto& v : m.vertices) {
            if (!(std::fabs(v.r - flowing.hair.r) < 0.005f &&
                  std::fabs(v.g - flowing.hair.g) < 0.005f &&
                  std::fabs(v.b - flowing.hair.b) < 0.005f)) {
                continue;
            }
            const float lx = v.px - head.x;
            const float ly = v.py - head.y;
            const float lz = v.pz - head.z;
            if (ly < hh * 0.10f && ly > -hh * 0.85f && std::fabs(lx) < hw * 0.45f) {
                const float proud = lz - film::detail::headFrontZ(hw, hh, hd, lx, ly);
                worst = std::max(worst, proud);
                if (proud > -hd * 0.05f) {
                    ++overTheFace;
                }
            }
        }
        char buf[160];
        std::snprintf(buf, sizeof buf,
                      "and never in front of the face it is meant to frame (%d strands over it, "
                      "nearest %.0f mm)",
                      overTheFace, static_cast<double>(worst * 1000.0f));
        check(overTheFace == 0, buf);
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
