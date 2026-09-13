// tests/film/face.cpp — a face, checked as a face.
//
// A reaction shot is half of film grammar: the reason to cut to somebody is to watch them take
// something in. Before there was a face there was nothing to watch, and the cut to it was a cut to
// nothing. So this checks the two halves of having one — that the head is BUILT like a head, and that
// what it is doing comes from the story rather than from a random number.
//
// Most of what is in here is here because it went wrong. Three of these checks would each have caught,
// in a second, a bug that took an afternoon of rendering pictures and staring at them:
//
//   * The head was lofted INSIDE OUT. Every triangle faced inward, so the renderer culled the front of
//     the face and drew the inside of the back of the skull instead — which is smooth, has no chin,
//     and still looks enough like a head that it survived being looked at, enlarged, for hours. The
//     shading was wrong, the nose was invisible, and no amount of adjusting the nose was going to fix
//     it. `outward` is that check.
//   * Features were laid onto the depth of the face measured DOWN THE MIDDLE, at whatever height they
//     sat. A head is a ball: at the eyes the surface is already a tenth of a head further back than it
//     is at the nose, so the eyes stood out of the face like golf balls and the cheekbones like two
//     flying saucers. `nothing floats` is that check.
//   * The neck was aimed at the middle of the head, and limb() finishes with a rounded cap standing a
//     whole radius past the point it is given — five centimetres on a neck. It arrived as a dome
//     behind the mouth, and the figure grew a muzzle. `nothing floats` catches that one too.
#include "maz/film/Actor.hpp"
#include "maz/film/Expression.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace film = maz::film;
namespace math = maz::math;
namespace render = maz::render;

// The signed volume a closed surface encloses — the divergence theorem, in four lines. It comes out
// positive when the triangles are wound outward and negative when they are not, so it is the one test
// an inside-out mesh cannot pass by happening to look right.
static double enclosedVolume(const render::shapes::MeshData& m) {
    double v = 0.0;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const auto& a = m.vertices[m.indices[t + 0]];
        const auto& b = m.vertices[m.indices[t + 1]];
        const auto& c = m.vertices[m.indices[t + 2]];
        v += static_cast<double>(a.px * (b.py * c.pz - c.py * b.pz) -
                                 a.py * (b.px * c.pz - c.px * b.pz) +
                                 a.pz * (b.px * c.py - c.px * b.py)) /
             6.0;
    }
    return v;
}

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

// How much of the head's geometry moved between two faces. The point of measuring the MESH rather than
// the numbers is that it proves the numbers are actually used: an expression that changes a field
// nothing reads is not an expression.
static float meshMoved(const render::shapes::MeshData& a, const render::shapes::MeshData& b) {
    if (a.vertices.size() != b.vertices.size()) {
        return 1e9f; // a different mesh entirely, which is a change by any measure
    }
    float worst = 0.0f;
    for (std::size_t i = 0; i < a.vertices.size(); ++i) {
        const float dx = a.vertices[i].px - b.vertices[i].px;
        const float dy = a.vertices[i].py - b.vertices[i].py;
        const float dz = a.vertices[i].pz - b.vertices[i].pz;
        const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (d > worst) {
            worst = d;
        }
    }
    return worst;
}

int main() {
    const film::Build b = film::adultMale();
    const float hw = b.m(b.headHalfW());
    const float hh = b.m(b.headHalfH());
    const float hd = b.m(b.headHalfD());

    // ------------------------------------------------------------------ 1. the head is a head
    //
    // Four almost equal parts from the crown of the hair to the chin, which is the one rule about a
    // face that everybody can see broken without being able to say what is wrong. The first draft had
    // the nose a third of a head too high and a mouth right behind it, and no amount of work on either
    // of them mattered while that was true.
    {
        const film::Skeleton sk = film::skeletonOf(b, film::restPose(b));
        near(sk.crown(b).y / b.height, 1.000f, 0.002f, "the crown of the hair is the top of a person");

        // The face, section by section, widest at the cheekbones and narrowing both ways.
        const float atBrow = film::detail::headAt(0.22f).rw;
        const float atCheek = film::detail::headAt(-0.12f).rw;
        const float atJaw = film::detail::headAt(-0.46f).rw;
        const float atChin = film::detail::headAt(-0.88f).rw;
        check(atCheek >= atBrow && atCheek >= atJaw, "a face is widest at the cheekbones");
        check(atJaw > atChin * 1.5f, "and narrows a long way from the jaw to the chin");
        check(atChin > 0.2f && atChin < 0.6f, "but the chin is a chin, not a point and not a shelf");

        // The brow stands proud and the eye sockets behind it. That difference is where a face gets
        // its shadows: without it the light lies flat across everything and the head is an egg.
        const float brow = film::detail::headFrontZ(hw, hh, hd, 0.0f, hh * 0.22f);
        const float socket = film::detail::headFrontZ(hw, hh, hd, hw * 0.375f, hh * 0.07f);
        const float cheek = film::detail::headFrontZ(hw, hh, hd, hw * 0.48f, -hh * 0.12f);
        check(brow > socket, "the brow stands in front of the eye sockets");
        check(cheek > socket, "and so does the cheekbone, because the socket is a hollow between them");

        // And the face falls away from the middle. This is the whole of the bug that floated every
        // feature off the face: measured down the middle the head is at its deepest, and a feature put
        // at that depth but out at the eyes or the cheekbones sits a centimetre in front of the skin.
        const float middle = film::detail::headFrontZ(hw, hh, hd, 0.0f, hh * 0.07f);
        check(middle - socket > hd * 0.05f,
              "and the face is measurably further back at the eyes than down the middle of it");

        // The nose. It is not a separate piece — it is the head's own surface coming forward down the
        // middle — so it is measured as the depth in the middle against the depth just beside it.
        const float bridge = film::detail::headFrontZ(hw, hh, hd, 0.0f, -hh * 0.25f);
        const float beside = film::detail::headFrontZ(hw, hh, hd, hw * 0.30f, -hh * 0.25f);
        const float underIt = film::detail::headFrontZ(hw, hh, hd, 0.0f, -hh * 0.60f) -
                              film::detail::headFrontZ(hw, hh, hd, hw * 0.30f, -hh * 0.60f);
        near((bridge - beside) * 1000.0f, 20.0f, 8.0f, "the nose stands about 20mm off the face");
        check(underIt < (bridge - beside) * 0.6f, "and it STOPS above the mouth rather than running on");
    }

    // ------------------------------------------------------------------ 2. outward
    //
    // Every triangle of the head faces out of it. Wound the other way the whole head is inside out:
    // the renderer culls the surface facing the camera and draws the far one instead, so a close-up is
    // the inside of the back of somebody's skull. It is the single most expensive mistake available
    // here, because the result still looks like a head.
    {
        const render::shapes::MeshData head = film::detail::skullShell(hw, hh, hd, 0.0f, b.skin);
        check(head.vertices.size() > 500, "the head has enough surface to be a head");

        // The signed volume of a closed surface is positive when the triangles are wound outward and
        // negative when they are not — the divergence theorem, one line of arithmetic, and it cannot
        // be fooled by a head that happens to look right.
        const double volume = enclosedVolume(head);
        check(volume > 0.0, "the head is wound outward, not inside out");
        // A head is about three litres, and a wrongly scaled one would pass the sign test alone.
        check(volume > 0.0015 && volume < 0.0060, "and it holds about three litres, as a head does");

        // The same for the hair, which is lofted the same way and got it wrong the same way.
        const render::shapes::MeshData hair = film::detail::hairCap(hw, hh, hd, hh, b.hair);
        const double hairVolume = enclosedVolume(hair);
        check(hairVolume > 0.0, "and so is the hair");
    }

    // ------------------------------------------------------------------ 3. nothing floats
    //
    // Every piece of the head is ON the face. A feature laid onto the depth of the face measured down
    // the middle stands off it everywhere else, and a neck aimed at the middle of the head arrives in
    // front of the mouth. Both happened; both look exactly like what they are.
    {
        const film::Skeleton sk = film::skeletonOf(b, film::restPose(b));
        const render::shapes::MeshData body = film::buildBody(b, sk, film::Face());
        const math::mat4 intoHead = math::inverse(sk.head);

        float worst = 0.0f;
        float worstY = 0.0f;
        int floating = 0;
        for (const auto& v : body.vertices) {
            const math::vec4 local = intoHead * math::vec4(v.px, v.py, v.pz, 1.0f);
            // Only the SKIN of the face. Hair — which here means the hair and the eyebrows, since
            // they are the same colour and the same stuff — sits on top of the face by design, and
            // outside the head's own width at a given height is the neck, which is wider than a chin
            // and meant to be. The end of the nose is allowed to overhang, because that is what the
            // underside of a nose does and it is where the nostrils live.
            const bool isHair = std::fabs(v.r - b.hair.r) < 0.01f &&
                                std::fabs(v.g - b.hair.g) < 0.01f &&
                                std::fabs(v.b - b.hair.b) < 0.01f;
            const float wide = film::detail::headAt(local.y / hh).rw * hw;
            const bool nose =
                std::fabs(local.x) < hw * 0.24f && local.y > -hh * 0.46f && local.y < -hh * 0.18f;
            if (isHair || local.y < -hh * 0.85f || local.y > hh * 0.40f ||
                std::fabs(local.x) > wide * 0.90f || nose) {
                continue;
            }
            const float surface = film::detail::headFrontZ(hw, hh, hd, local.x, local.y);
            const float proud = local.z - surface;
            if (proud > worst) {
                worst = proud;
                worstY = local.y / hh;
            }
            if (proud > hd * 0.13f) {
                ++floating;
            }
        }
        char buf[200];
        std::snprintf(buf, sizeof buf,
                      "nothing on the face stands more than an eighth of a head-depth off it "
                      "(worst %.1f mm at y=%.2f, %d vertices past the line)",
                      static_cast<double>(worst * 1000.0f), static_cast<double>(worstY), floating);
        check(floating == 0, buf);
    }

    // ------------------------------------------------------------------ 4. the face does something
    {
        const film::Skeleton sk = film::skeletonOf(b, film::restPose(b));
        const film::Face calm = film::faceForBeat("open", 0.2);
        const film::Face bad = film::faceForBeat("crisis", 0.9);

        check(bad.browLift < calm.browLift - 0.3f, "a crisis lowers the brows a long way");
        check(bad.smile < calm.smile - 0.3f, "and takes the mouth down with them");
        check(bad.squint > calm.squint + 0.2f, "and narrows the eyes");
        check(film::faceForBeat("spark", 0.8).browLift > 0.4f, "a spark lifts them instead");
        check(film::faceForBeat("choice", 0.9).browTilt > 0.3f,
              "and a choice knits the inner ends up, which is the shape of not wanting to");

        // The same beat played harder is the same expression, further. A performance that does not
        // build is a slideshow of stock faces.
        check(film::faceForBeat("crisis", 0.9).browLift < film::faceForBeat("crisis", 0.2).browLift,
              "and the same beat played harder goes further, rather than becoming a different face");

        // It reaches the geometry. Every check above could pass with a Face that buildBody ignores.
        const float moved = meshMoved(film::buildBody(b, sk, calm), film::buildBody(b, sk, bad));
        check(moved > hh * 0.02f, "and the head that is built from it is a different head");
    }

    // ------------------------------------------------------------------ 5. talking
    {
        film::Reacting r;
        r.beat = "push";
        r.mood = 0.5;
        r.speaking = true;

        r.syllable = 0.5f;
        const float mid = film::faceAt(r).mouthOpen;
        r.syllable = 0.02f;
        const float start = film::faceAt(r).mouthOpen;
        r.speaking = false;
        const float shut = film::faceAt(r).mouthOpen;

        check(mid > 0.5f, "a mouth opens in the middle of a syllable");
        check(start < mid * 0.5f, "and is nearly closed at the edges of one");
        near(shut, 0.0f, 1e-6f, "and somebody who is not talking does not move their mouth at all");

        // The jaw goes with it. A dark hole that grows in a face that is otherwise still reads as a
        // hole; a jaw that swings down reads as somebody talking, which is the entire point.
        const render::shapes::MeshData shutJaw = film::detail::skullShell(hw, hh, hd, 0.0f, b.skin);
        const render::shapes::MeshData openJaw =
            film::detail::skullShell(hw, hh, hd, hh * 0.085f, b.skin);
        float shutLow = 1e9f, openLow = 1e9f, shutTop = -1e9f, openTop = -1e9f;
        for (const auto& v : shutJaw.vertices) {
            shutLow = std::min(shutLow, v.py);
            shutTop = std::max(shutTop, v.py);
        }
        for (const auto& v : openJaw.vertices) {
            openLow = std::min(openLow, v.py);
            openTop = std::max(openTop, v.py);
        }
        check(openLow < shutLow - hh * 0.04f, "and the chin swings down when it does");
        near(openTop, shutTop, 1e-5f,
             "while the top of the head stays exactly where it was — a jaw hinges, it does not slide "
             "the whole skull down");
    }

    // ------------------------------------------------------------------ 6. blinking
    {
        // Not everybody at once. Two people listening to the same line who blink in unison are two
        // puppets on one string, and it is the kind of thing an audience notices without noticing.
        int together = 0, apart = 0;
        for (int i = 0; i < 600; ++i) {
            const double t = static_cast<double>(i) * 0.05;
            const bool a = film::blinkAmount(t, 11u) > 0.3f;
            const bool c = film::blinkAmount(t, 7331u) > 0.3f;
            if (a && c) ++together;
            if (a != c) ++apart;
        }
        check(apart > 0, "two people blink at different moments");
        check(together * 4 < apart, "and hardly ever at the same one");

        // A blink is quick. One that lasts half a second is somebody falling asleep.
        int shut = 0;
        for (int i = 0; i < 4000; ++i) {
            if (film::blinkAmount(static_cast<double>(i) * 0.01, 11u) > 0.5f) ++shut;
        }
        check(shut > 0, "a blink happens");
        check(shut < 4000 / 12, "and it is quick — an eye is open far more than it is shut");

        // And it goes down fast and comes back up slower, which is what an eyelid does and is most of
        // what makes a blink read as a blink rather than as a dropped frame.
        int down = 0, up = 0;
        float peak = 0.0f;
        double peakAt = 0.0;
        for (int i = 0; i < 4000; ++i) {
            const double t = static_cast<double>(i) * 0.005;
            const float v = film::blinkAmount(t, 11u);
            if (v > peak) {
                peak = v;
                peakAt = t;
            }
        }
        // Only the ONE blink around that peak. Counted across the whole run the two halves come out
        // of different blinks and the answer means nothing.
        for (int i = -60; i <= 60; ++i) {
            const double t = peakAt + static_cast<double>(i) * 0.005;
            if (film::blinkAmount(t, 11u) > 0.05f) {
                (i < 0 ? down : up)++;
            }
        }
        check(down > 0 && up > down, "and the lid drops faster than it lifts, as a lid does");

        // And it CLOSES the eye rather than deleting it. The first attempt drew an eyelid as its own
        // sphere coming down over the eyeball, which is how a lid works and is not how one can be
        // drawn at this size: a sphere big enough to cover the eye is bigger than the eye, so it stood
        // proud of the face, and the result was two pale eggs parked on the forehead.
        const film::Skeleton sk = film::skeletonOf(b, film::restPose(b));
        film::Face open2, shut2;
        shut2.blink = 1.0f;
        const render::shapes::MeshData openEye = film::buildBody(b, sk, open2);
        const render::shapes::MeshData shutEye = film::buildBody(b, sk, shut2);
        check(openEye.vertices.size() == shutEye.vertices.size(),
              "a shut eye is the same eye, closed — not a different piece of geometry");
        check(meshMoved(openEye, shutEye) > hh * 0.01f, "and closing it actually moves it");

        // Specifically: the white of the eye gets SHORTER. Something moving is not the same as the eye
        // shutting, and the whites are the part that has to disappear.
        auto whiteHeight = [&](const render::shapes::MeshData& head) {
            float lo = 1e9f, hi = -1e9f;
            for (const auto& v : head.vertices) {
                if (std::fabs(v.r - 0.78f) < 0.01f && std::fabs(v.g - 0.77f) < 0.01f &&
                    std::fabs(v.b - 0.74f) < 0.01f) {
                    lo = std::min(lo, v.py);
                    hi = std::max(hi, v.py);
                }
            }
            return hi > lo ? hi - lo : 0.0f;
        };
        const float openTall = whiteHeight(openEye);
        const float shutTall = whiteHeight(shutEye);
        check(openTall > hh * 0.05f, "an open eye has some white showing");
        check(shutTall < openTall * 0.35f, "and a shut one has almost none");
    }

    // ------------------------------------------------------------------ 7. in range, always
    {
        const char* beats[] = {"open", "spark", "push", "turn", "crisis", "choice", "after", "nonsense"};
        int bad = 0;
        for (const char* beat : beats) {
            for (int mi = 0; mi <= 10; ++mi) {
                for (int si = 0; si <= 4; ++si) {
                    for (int speak = 0; speak < 2; ++speak) {
                        film::Reacting r;
                        r.beat = beat;
                        r.mood = static_cast<double>(mi) * 0.1;
                        r.speaking = speak != 0;
                        r.syllable = static_cast<float>(si) * 0.25f;
                        r.seconds = static_cast<double>(mi) * 1.7;
                        r.seed = static_cast<std::uint32_t>(mi * 31 + si);
                        r.toward = static_cast<float>(si) - 2.0f; // deliberately out of range
                        const film::Face f = film::faceAt(r);
                        if (f.browLift < -1.0f || f.browLift > 1.0f) ++bad;
                        if (f.browTilt < -1.0f || f.browTilt > 1.0f) ++bad;
                        if (f.smile < -1.0f || f.smile > 1.0f) ++bad;
                        if (f.squint < -0.3f || f.squint > 1.0f) ++bad;
                        if (f.mouthOpen < 0.0f || f.mouthOpen > 1.0f) ++bad;
                        if (f.blink < 0.0f || f.blink > 1.0f) ++bad;
                        if (f.gaze < -1.0f || f.gaze > 1.0f) ++bad;
                    }
                }
            }
        }
        check(bad == 0, "nothing a face is asked to do puts a brow through the top of the head");
    }

    if (!failures.empty()) {
        for (const std::string& f : failures) {
            std::printf("FAIL: %s\n", f.c_str());
        }
        std::printf("%zu failed\n", failures.size());
        return 1;
    }
    std::printf("face: all checks passed\n");
    return 0;
}
