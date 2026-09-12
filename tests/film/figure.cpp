// tests/film/figure.cpp — verifies the ported figure (film Figure.hpp).
//
// A character in these films is a silhouette driven by ten joint angles. The ten poses, their limits
// and the body geometry are ported from film/js/film-figures.js, and the properties that matter are
// the ones the browser's own tests found the hard way:
//   * every named pose is inside POSE_LIMITS -- a blend must never make a shape a human cannot hold;
//   * a foot is ON the ground at every pose. A bent leg is shorter end to end than a straight one, so
//     without a correction the figure hangs in the air; the browser hit exactly this;
//   * the head must not punch a hole in the torso. Both are sub-paths of ONE filled path, so if their
//     winding directions disagree the nonzero rule cuts the head out. That is invisible in the
//     geometry and obvious in the picture, so it is asserted on rendered pixels;
//   * the body scales cleanly with height, and the same pose always gives the same path.
#include "maz/film/Figure.hpp"
#include "maz/io/Json.hpp"
#include "maz/render/PathFill.hpp"

#include <cmath>
#include <cstdio>
#include <string>

using maz::film::Pose;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static std::string g_bodyFixture = "tests/film/body-fixture.json";

int main(int argc, char** argv) {
    if (argc > 1) {
        g_bodyFixture = argv[1];
    }
    const auto& names = maz::film::poseNames();
    CHECK(names.size() == 10u, "all ten poses are there");

    // --- 1. Every named pose is inside the limits. ---
    {
        bool ok = true;
        std::string bad;
        for (const std::string& n : names) {
            const Pose* p = maz::film::poseNamed(n);
            if (p == nullptr || !maz::film::withinLimits(*p)) {
                ok = false;
                if (bad.empty()) bad = n;
            }
        }
        CHECK(ok, ("every named pose is inside POSE_LIMITS (first bad: " + bad + ")").c_str());
        CHECK(maz::film::poseNamed("no-such-pose") == nullptr, "an unknown pose name gives nothing");
    }

    // --- 2. Clamping brings any pose inside, however absurd. ---
    {
        Pose wild;
        wild.head = 40.0; wild.torso = -40.0; wild.armL = 99.0; wild.foreL = -99.0;
        wild.armR = 99.0; wild.foreR = -99.0; wild.legL = 12.0; wild.shinL = 12.0;
        wild.legR = -12.0; wild.shinR = -12.0;
        CHECK(!maz::film::withinLimits(wild), "an absurd pose is recognised as out of limits");
        CHECK(maz::film::withinLimits(maz::film::clampToLimits(wild)), "clamping brings it inside");
        // Clamping something already legal must not move it.
        const Pose* stand = maz::film::poseNamed("stand");
        const Pose same = maz::film::clampToLimits(*stand);
        CHECK(same.head == stand->head && same.armL == stand->armL && same.legR == stand->legR,
              "clamping a legal pose leaves it exactly alone");
    }

    // --- 3. A foot is on the ground in every pose -- never hanging, never floating. ---
    {
        bool planted = true;
        std::string bad;
        for (const std::string& n : names) {
            const Pose* p = maz::film::poseNamed(n);
            const double foot = maz::film::lowestFootY(200.0, *p);
            // y grows downward and the ground is y = 0, so the lowest foot must be at or below it.
            if (foot < -0.001) {
                planted = false;
                if (bad.empty()) bad = n + " (foot at " + std::to_string(foot) + ")";
            }
        }
        CHECK(planted, ("a foot is on the ground in every pose; first floating: " + bad).c_str());
    }

    // --- 4. The body is the browser's body, joint for joint. ---
    //
    // Ported geometry is worth nothing if it is only plausible. The fixture holds the exact path the
    // browser's drawFigure emits for all ten poses at height 300 -- captured by handing it a canvas
    // that records instead of drawing -- and this demands the same corners in the same order.
    {
        const maz::render::Path stand = maz::film::bodyPath(300.0, *maz::film::poseNamed("stand"));
        CHECK(!stand.empty(), "the body path is not empty");
        CHECK(stand.contours().size() == 10u,
              "the body is nine limb segments plus one head, as one path");

        std::string bodyText;
        if (!maz::io::readTextFile(g_bodyFixture, bodyText)) {
            std::printf("FAIL: could not read the body fixture at %s\n", g_bodyFixture.c_str());
            ++g_fail;
        } else {
            const maz::io::JsonParseResult bodyDoc = maz::io::parseJson(bodyText);
            CHECK(bodyDoc.ok, "the body fixture is valid JSON");
            const double h = bodyDoc.value["height"].asNumber(300.0);
            int wrong = 0;
            double worst = 0.0;
            std::string firstBad;
            for (const std::string& n : names) {
                const maz::render::Path mine = maz::film::bodyPath(h, *maz::film::poseNamed(n));
                const maz::io::JsonValue& ops = bodyDoc.value["poses"][n];
                // Nine segments of M,L,L,L,Z then one ellipse: 46 operations in all.
                std::size_t op = 0;
                for (std::size_t c = 0; c < 9; ++c) {
                    for (std::size_t k = 0; k < 4; ++k) {
                        const maz::io::JsonValue& o = ops[op + k];
                        const double bx = o[static_cast<std::size_t>(1)].asNumber(0.0);
                        const double by = o[static_cast<std::size_t>(2)].asNumber(0.0);
                        const double dx = std::fabs(static_cast<double>(mine.contours()[c][k].x) - bx);
                        const double dy = std::fabs(static_cast<double>(mine.contours()[c][k].y) - by);
                        worst = std::fmax(worst, std::fmax(dx, dy));
                        if (dx > 1e-3 || dy > 1e-3) {
                            ++wrong;
                            if (firstBad.empty()) {
                                firstBad = n + " contour " + std::to_string(c) + " point " +
                                           std::to_string(k);
                            }
                        }
                    }
                    op += 5; // four corners plus the closePath
                }
                // The tenth sub-path is the head ellipse; check its centre and radii.
                const maz::io::JsonValue& e = ops[op];
                const maz::math::vec2 head = maz::film::headCentre(h, *maz::film::poseNamed(n));
                if (std::fabs(static_cast<double>(head.x) - e[static_cast<std::size_t>(1)].asNumber()) > 1e-3 ||
                    std::fabs(static_cast<double>(head.y) - e[static_cast<std::size_t>(2)].asNumber()) > 1e-3) {
                    ++wrong;
                    if (firstBad.empty()) firstBad = n + " head centre";
                }
                if (std::fabs(h * 0.052 - e[static_cast<std::size_t>(3)].asNumber()) > 1e-6 ||
                    std::fabs(h * 0.062 - e[static_cast<std::size_t>(4)].asNumber()) > 1e-6) {
                    ++wrong;
                    if (firstBad.empty()) firstBad = n + " head size";
                }
            }
            if (wrong != 0) {
                std::printf("FAIL: %d body points differ from the browser (first: %s)\n", wrong,
                            firstBad.c_str());
                ++g_fail;
            } else {
                std::printf("  (body geometry matches the browser to %.2g of a pixel)\n", worst);
            }
        }

        maz::math::vec2 lo{}, hi{};
        CHECK(stand.bounds(lo, hi), "the body path has bounds");
        CHECK(hi.y > -1.0f && hi.y < 9.0f, "the feet sit on the ground line");
        CHECK(hi.x - lo.x < 300.0f, "a standing figure is narrower than it is tall");
    }

    // --- 5. The body scales with height. ---
    {
        const Pose* p = maz::film::poseNamed("reach");
        const maz::render::Path a = maz::film::bodyPath(100.0, *p);
        const maz::render::Path b = maz::film::bodyPath(300.0, *p);
        maz::math::vec2 alo{}, ahi{}, blo{}, bhi{};
        a.bounds(alo, ahi);
        b.bounds(blo, bhi);
        const float scaleY = (bhi.y - blo.y) / (ahi.y - alo.y);
        CHECK(std::fabs(scaleY - 3.0f) < 0.02f, "tripling the height triples the figure");
    }

    // --- 6. The same pose always gives the same path. ---
    {
        const Pose* p = maz::film::poseNamed("point");
        const maz::render::Path a = maz::film::bodyPath(180.0, *p);
        const maz::render::Path b = maz::film::bodyPath(180.0, *p);
        bool same = a.contours().size() == b.contours().size();
        for (std::size_t i = 0; same && i < a.contours().size(); ++i) {
            same = a.contours()[i].size() == b.contours()[i].size();
            for (std::size_t j = 0; same && j < a.contours()[i].size(); ++j) {
                same = a.contours()[i][j].x == b.contours()[i][j].x &&
                       a.contours()[i][j].y == b.contours()[i][j].y;
            }
        }
        CHECK(same, "the same pose and height always give the same path");
    }

    // --- 7. The head is solid, not a hole. ---
    //
    // The head and the limbs are sub-paths of ONE filled path. Under the nonzero rule two overlapping
    // sub-paths wound the SAME way stay filled, and two wound OPPOSITE ways cancel to a hole. Nothing
    // about the geometry says which happened; only the pixels do.
    {
        bool solidEverywhere = true;
        std::string bad;
        for (const std::string& n : names) {
            const Pose* p = maz::film::poseNamed(n);
            maz::render::Image img(400, 400, maz::render::Color{0.0f, 0.0f, 0.0f, 1.0f});
            const maz::render::Path body = maz::film::bodyPath(300.0, *p);
            // Draw with the feet at y = 360 so the whole figure lands on the image.
            maz::render::Path moved = body;
            moved.translate(200.0f, 360.0f);
            maz::render::fillPath(img, moved, maz::render::Color{1.0f, 1.0f, 1.0f, 1.0f});

            const maz::math::vec2 head = maz::film::headCentre(300.0, *p);
            const int hx = static_cast<int>(std::lround(head.x + 200.0f));
            const int hy = static_cast<int>(std::lround(head.y + 360.0f));
            if (img.getPixel(hx, hy).r < 0.9f) {
                solidEverywhere = false;
                if (bad.empty()) bad = n;
            }
        }
        CHECK(solidEverywhere,
              ("the head is filled, not a hole, in every pose (first hollow: " + bad + ")").c_str());
    }

    // --- 8. Two different poses really are different shapes. ---
    {
        const maz::render::Path a = maz::film::bodyPath(200.0, *maz::film::poseNamed("stand"));
        const maz::render::Path b = maz::film::bodyPath(200.0, *maz::film::poseNamed("head-in-hands"));
        maz::math::vec2 alo{}, ahi{}, blo{}, bhi{};
        a.bounds(alo, ahi);
        b.bounds(blo, bhi);
        CHECK(std::fabs((ahi.x - alo.x) - (bhi.x - blo.x)) > 1.0f,
              "standing and holding your head are not the same silhouette");
    }

    if (g_fail == 0) {
        std::printf("film figure: all checks passed (%zu poses)\n", names.size());
    }
    return g_fail == 0 ? 0 : 1;
}
