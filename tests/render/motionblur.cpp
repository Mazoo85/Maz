// tests/render/motionblur.cpp — why a film at twelve frames a second does not look like a slideshow.
//
// A camera renders an INSTANT. A film camera does not: its shutter is open for a fraction of the
// frame and everything that moved while it was open is smeared across the picture, and that smear is
// what joins one frame to the next. Without it, at twelve frames a second, a pan is a sequence of
// separate photographs — the effect people call strobing, and the single most obvious thing wrong
// with a rendered pan.
//
// What is done here is the standard camera-motion version: for each pixel, work out where the thing
// standing at it USED to be on screen one frame ago, and gather along the line between. It is exact
// for anything that did not move on its own and wrong for anything that did, which is a real limit
// and is stated rather than hidden — see the note at the end of this file.
#include "maz/render/MotionBlur.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace render = maz::render;
namespace math = maz::math;

static std::vector<std::string> failures;

static void check(bool ok, const std::string& what) {
    if (!ok) {
        failures.push_back(what);
    }
}

static void near(float got, float want, float tol, const std::string& what) {
    if (!(std::fabs(got - want) <= tol)) {
        char buf[240];
        std::snprintf(buf, sizeof buf, "%s (got %.5f, wanted %.5f +/- %.5f)", what.c_str(),
                      static_cast<double>(got), static_cast<double>(want), static_cast<double>(tol));
        failures.push_back(buf);
    }
}

// A camera looking down -z from `eye`, turned by `yaw` radians.
static math::mat4 cameraAt(const math::vec3& eye, float yaw, float aspect) {
    const math::vec3 forward(std::sin(yaw), 0.0f, -std::cos(yaw));
    const math::mat4 view = glm::lookAt(eye, eye + forward, math::vec3(0.0f, 1.0f, 0.0f));
    return math::perspective(0.9f, aspect, 0.1f, 120.0f) * view;
}

// The depth a point `metres` away lands on, for that projection.
static float depthAt(float metres) {
    const float nearP = 0.1f, farP = 120.0f;
    return (farP - nearP * farP / metres) / (farP - nearP);
}

int main() {
    const int w = 120, h = 80;
    const float aspect = static_cast<float>(w) / static_cast<float>(h);

    // ------------------------------------------------------------------ 1. how far a pixel moved
    {
        const math::mat4 still = cameraAt(math::vec3(0.0f, 1.6f, 0.0f), 0.0f, aspect);

        // A camera that did not move smears nothing. This is the case that runs on most frames of
        // most films — a locked-off shot — so it had better cost nothing and do nothing.
        {
            const math::vec2 v = render::cameraVelocity(still, still, 60.0f, 40.0f, depthAt(5.0f),
                                                        w, h);
            near(v.x, 0.0f, 1e-3f, "a camera that did not move moves nothing sideways");
            near(v.y, 0.0f, 1e-3f, "and nothing up or down");
        }

        // A PAN, and the direction matters and is easy to get backwards. The camera was turned left
        // and is now straight ahead, so it panned RIGHT — and a camera panning right drags the
        // picture LEFT across the frame, the way the scenery goes backwards past a train window. So
        // the velocity is negative: the thing standing at this pixel was over to the right of here a
        // frame ago, and is travelling leftward. A streak pointing the other way is a shot that looks
        // like it is panning the wrong way, which is worse than no streak at all.
        {
            const math::mat4 was = cameraAt(math::vec3(0.0f, 1.6f, 0.0f), -0.05f, aspect);
            const math::vec2 v =
                render::cameraVelocity(still, was, 60.0f, 40.0f, depthAt(5.0f), w, h);
            check(v.x < -1.0f, "a camera panning right drags the picture leftward across the frame");
            check(std::fabs(v.y) < std::fabs(v.x) * 0.2f, "and barely at all up or down");

            // And the other way round is the other way round, which a sign error fixed by negating
            // everything would not survive.
            const math::mat4 wasRight = cameraAt(math::vec3(0.0f, 1.6f, 0.0f), 0.05f, aspect);
            const math::vec2 back =
                render::cameraVelocity(still, wasRight, 60.0f, 40.0f, depthAt(5.0f), w, h);
            check(back.x > 1.0f, "and panning left drags it rightward");
        }

        // PARALLAX, which is the reason this is worth doing per pixel rather than once per frame.
        // Under a pure TURN everything moves the same amount whatever its distance; under a TRACK,
        // near things move further than far things. A blur that ignored depth would be right for one
        // and wrong for the other, and a track is exactly the move where the smear carries the
        // feeling of speed.
        {
            const math::mat4 wasTurned = cameraAt(math::vec3(0.0f, 1.6f, 0.0f), -0.05f, aspect);
            const float nearTurn =
                render::cameraVelocity(still, wasTurned, 60.0f, 40.0f, depthAt(2.0f), w, h).x;
            const float farTurn =
                render::cameraVelocity(still, wasTurned, 60.0f, 40.0f, depthAt(40.0f), w, h).x;
            check(std::fabs(nearTurn - farTurn) < std::fabs(nearTurn) * 0.25f,
                  "under a pan, near and far move across the frame together");

            const math::mat4 wasOver = cameraAt(math::vec3(-0.25f, 1.6f, 0.0f), 0.0f, aspect);
            const float nearTrack =
                render::cameraVelocity(still, wasOver, 60.0f, 40.0f, depthAt(2.0f), w, h).x;
            const float farTrack =
                render::cameraVelocity(still, wasOver, 60.0f, 40.0f, depthAt(40.0f), w, h).x;
            check(std::fabs(nearTrack) > std::fabs(farTrack) * 3.0f,
                  "but under a track the near thing moves much further than the far one, which is "
                  "the whole reason this is worked out per pixel");
        }

        // The far plane is not somewhere the arithmetic falls over. A sky pixel is at depth 1, and
        // reading one must give an answer rather than an infinity that smears the whole frame.
        {
            const math::mat4 was = cameraAt(math::vec3(0.0f, 1.6f, 0.0f), -0.05f, aspect);
            const math::vec2 v = render::cameraVelocity(still, was, 60.0f, 40.0f, 1.0f, w, h);
            check(std::isfinite(v.x) && std::isfinite(v.y), "a pixel at the far plane has a finite answer");
        }
    }

    // ------------------------------------------------------------------ 2. the picture
    {
        const math::mat4 now = cameraAt(math::vec3(0.0f, 1.6f, 0.0f), 0.0f, aspect);
        const math::mat4 panned = cameraAt(math::vec3(0.0f, 1.6f, 0.0f), -0.06f, aspect);

        auto build = [&]() {
            render::Image img(w, h, render::Color{0.0f, 0.0f, 0.0f, 1.0f});
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    // One bright vertical line and one bright horizontal line, so a smear along one
                    // axis is unmistakable from a smear along the other.
                    const float v = (x == w / 2 || y == h / 2) ? 1.0f : 0.0f;
                    img.setPixel(x, y, render::Color{v, v, v, 1.0f});
                }
            }
            return img;
        };
        std::vector<float> depth(static_cast<std::size_t>(w) * static_cast<std::size_t>(h),
                                 depthAt(5.0f));

        // A still camera leaves the frame exactly as it found it — to the last bit, not nearly.
        {
            render::Image img = build();
            const render::Image before = img;
            render::motionBlur(img, 0, h, depth, now, now, 0.5f);
            int moved = 0;
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    if (img.getPixel(x, y).r != before.getPixel(x, y).r) {
                        ++moved;
                    }
                }
            }
            check(moved == 0, "a locked-off camera comes through the shutter untouched");
        }

        // And a shutter held closed does nothing however much the camera moved.
        {
            render::Image img = build();
            const render::Image before = img;
            render::motionBlur(img, 0, h, depth, now, panned, 0.0f);
            int moved = 0;
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    if (img.getPixel(x, y).r != before.getPixel(x, y).r) {
                        ++moved;
                    }
                }
            }
            check(moved == 0, "and a shutter that never opens blurs nothing");
        }

        // A pan smears ACROSS and not down.
        {
            render::Image img = build();
            const render::Image before = img;
            render::motionBlur(img, 0, h, depth, now, panned, 0.55f);

            // The vertical line is spread sideways, where before it was exactly one pixel wide.
            int spread = 0, wasWide = 0;
            for (int x = 0; x < w; ++x) {
                if (img.getPixel(x, 10).r > 0.02f) {
                    ++spread;
                }
                if (before.getPixel(x, 10).r > 0.02f) {
                    ++wasWide;
                }
            }
            check(wasWide == 1, "the line started one pixel wide");
            check(spread >= 3, "and a pan spreads it across the frame");

            // And it spread the way the camera dragged it — to the LEFT of where the line was, not
            // the right. This is the check that a streak trailing the wrong way fails.
            int onLeft = 0, onRight = 0;
            for (int x = 1; x <= 4; ++x) {
                if (img.getPixel(w / 2 - x, 10).r > 0.02f) {
                    ++onLeft;
                }
                if (img.getPixel(w / 2 + x, 10).r > 0.02f) {
                    ++onRight;
                }
            }
            check(onLeft > onRight, "on the side it was dragged towards, not the other one");

            // The horizontal line is NOT spread up and down: a column crossing it lights the same
            // number of rows as before. A blur that went the wrong way, or every way, fails here.
            int rowsBefore = 0, rowsAfter = 0;
            for (int y = 0; y < h; ++y) {
                if (before.getPixel(10, y).r > 0.02f) {
                    ++rowsBefore;
                }
                if (img.getPixel(10, y).r > 0.02f) {
                    ++rowsAfter;
                }
            }
            check(rowsAfter <= rowsBefore + 1,
                  "and does not spread a horizontal one up and down, which a blur going the wrong "
                  "way would");

            // Light is moved about, not made. A shutter is an average.
            double was = 0.0, is = 0.0;
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    was += static_cast<double>(before.getPixel(x, y).r);
                    is += static_cast<double>(img.getPixel(x, y).r);
                }
            }
            check(is < was * 1.05 && is > was * 0.55,
                  "and the shutter moves light about rather than making it");
        }

        // A longer shutter smears further. It is a knob, not a switch.
        {
            auto reach = [&](float shutter) {
                render::Image img = build();
                render::motionBlur(img, 0, h, depth, now, panned, shutter);
                int lit = 0;
                for (int x = 0; x < w; ++x) {
                    if (img.getPixel(x, 10).r > 0.02f) {
                        ++lit;
                    }
                }
                return lit;
            };
            const int shortOpen = reach(0.25f);
            const int longOpen = reach(1.0f);
            check(longOpen > shortOpen,
                  "a shutter held open longer smears further than one barely open");
        }
    }

    // ------------------------------------------------------------------ 2b. off the edge of the frame
    //
    // A streak near the edge of the picture wants to gather from outside it. There are two paths
    // through the gather — a fast one for streaks that land wholly inside, which walks a constant
    // offset with no bounds checking at all, and a careful one that clamps — and the fast one is only
    // safe because of the test that chooses between them. Forcing every pixel down the fast path
    // reads off the end of the buffer, and the checks above did not notice: they looked at the middle
    // of the frame and at light being conserved, and a wild read happens to land on other rows of the
    // same picture often enough to pass both.
    //
    // What catches it is the one thing a gather can never do: an average of samples taken from the
    // picture cannot be brighter than the brightest thing in the picture, or darker than the darkest.
    // A read from outside the buffer is not a sample of the picture, and says so.
    {
        const int bw = 96, bh = 64;
        const float ba = static_cast<float>(bw) / static_cast<float>(bh);
        const math::mat4 now = cameraAt(math::vec3(0.0f, 1.6f, 0.0f), 0.0f, ba);
        // A whip: the picture is dragged a long way, so streaks at every edge run off it.
        const math::mat4 was = cameraAt(math::vec3(-0.9f, 1.6f, 0.0f), -0.30f, ba);

        render::Image img(bw, bh, render::Color{0.0f, 0.0f, 0.0f, 1.0f});
        for (int y = 0; y < bh; ++y) {
            for (int x = 0; x < bw; ++x) {
                const float v = ((x / 3 + y / 3) % 2 == 0) ? 0.25f : 0.75f;
                img.setPixel(x, y, render::Color{v, v, v, 1.0f});
            }
        }
        std::vector<float> depth(static_cast<std::size_t>(bw) * static_cast<std::size_t>(bh), 0.0f);
        for (int y = 0; y < bh; ++y) {
            for (int x = 0; x < bw; ++x) {
                // Varied, so the near pixels move much further than the far ones and the streaks are
                // every length at once.
                depth[static_cast<std::size_t>(y) * static_cast<std::size_t>(bw) +
                      static_cast<std::size_t>(x)] = depthAt(1.0f + static_cast<float>(x) * 0.6f);
            }
        }
        render::motionBlur(img, 0, bh, depth, now, was, 0.9f);

        int outOfRange = 0, notFinite = 0;
        for (int y = 0; y < bh; ++y) {
            for (int x = 0; x < bw; ++x) {
                const float v = img.getPixel(x, y).r;
                if (!std::isfinite(v)) {
                    ++notFinite;
                } else if (v < 0.25f - 1e-4f || v > 0.75f + 1e-4f) {
                    ++outOfRange;
                }
            }
        }
        check(notFinite == 0, "a streak that runs off the edge of the frame gathers real numbers");
        check(outOfRange == 0,
              "and gathers them from inside the picture — nothing comes out brighter than the "
              "brightest thing in it, which is what reading off the end of the buffer looks like");
    }

    // ------------------------------------------------------------------ 3. it refuses the impossible
    {
        render::Image img(20, 12, render::Color{0.3f, 0.3f, 0.3f, 1.0f});
        img.setPixel(10, 6, render::Color{1.0f, 1.0f, 1.0f, 1.0f});
        const render::Image before = img;
        const math::mat4 a = cameraAt(math::vec3(0.0f, 1.6f, 0.0f), 0.0f, 1.6f);
        const math::mat4 b = cameraAt(math::vec3(0.0f, 1.6f, 0.0f), -0.2f, 1.6f);

        // A depth buffer that does not fit the picture is refused rather than read off the end of.
        std::vector<float> tooFew(6, 0.5f);
        render::motionBlur(img, 0, 12, tooFew, a, b, 0.5f);
        near(img.getPixel(10, 6).r, before.getPixel(10, 6).r, 1e-6f,
             "a depth buffer that does not fit the picture is refused, not read");

        // A band with nothing in it is left alone rather than walked off the end of.
        std::vector<float> full(static_cast<std::size_t>(20) * 12u, 0.5f);
        render::motionBlur(img, 6, 6, full, a, b, 0.5f);
        near(img.getPixel(10, 6).r, before.getPixel(10, 6).r, 1e-6f, "and an empty band is left alone");

        // A shutter allowed only one sample. The samples are spread across `taps - 1` gaps, so one
        // tap is a division by zero and a frame full of NaN — which does not throw, does not warn,
        // and paints black. Asking for fewer than two is refused instead.
        render::motionBlur(img, 0, 12, full, a, b, 0.5f, 1);
        check(std::isfinite(img.getPixel(10, 6).r) && std::isfinite(img.getPixel(0, 0).r),
              "a shutter asked for a single sample is refused rather than filling the frame with "
              "nothing-at-all");
        near(img.getPixel(10, 6).r, before.getPixel(10, 6).r, 1e-6f,
             "and leaves the picture as it found it");
    }

    if (!failures.empty()) {
        for (const std::string& f : failures) {
            std::printf("FAIL: %s\n", f.c_str());
        }
        std::printf("%zu failed\n", failures.size());
        return 1;
    }
    std::printf("motion blur: all checks passed\n");
    return 0;
}
