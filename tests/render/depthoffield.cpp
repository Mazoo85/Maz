// tests/render/depthoffield.cpp — the one thing a lens does that a pinhole does not.
//
// A rasteriser is a pinhole camera: every point in the world lands on exactly one pixel however far
// away it is, so everything is in focus and the picture reads as a diagram. Real glass focuses at one
// distance and turns everything else into a small disc. Most of what these checks are about is that
// the disc is the right size in the right places — and in particular that the FOREGROUND goes soft
// faster than the background, which is the asymmetry that makes a face separate from a wall.
#include "maz/render/DepthOfField.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace render = maz::render;

static std::vector<std::string> failures;

static void check(bool ok, const std::string& what) {
    if (!ok) {
        failures.push_back(what);
    }
}

static void near(float got, float want, float tol, const std::string& what) {
    if (!(std::fabs(got - want) <= tol)) {
        char buf[220];
        std::snprintf(buf, sizeof buf, "%s (got %.5f, wanted %.5f +/- %.5f)", what.c_str(),
                      static_cast<double>(got), static_cast<double>(want), static_cast<double>(tol));
        failures.push_back(buf);
    }
}

int main() {
    const float nearP = 0.04f;
    const float farP = 220.0f;

    // ------------------------------------------------------------------ 1. depth back into metres
    //
    // A depth buffer does not store distance. It stores z/w after the projection, which is crammed up
    // against the far plane — over half the range is spent on the first metre. Blurring by that number
    // directly is a lens that treats everything past ten metres as the same distance, which is not
    // what glass does.
    {
        near(render::viewDistance(0.0f, nearP, farP), nearP, 0.001f,
             "depth 0 is the near plane, in metres");
        check(render::viewDistance(0.9999f, nearP, farP) > farP * 0.5f,
              "and depth 1 is somewhere out near the far plane");
        float last = 0.0f;
        bool climbs = true;
        for (int i = 0; i <= 100; ++i) {
            const float d = render::viewDistance(static_cast<float>(i) * 0.0099f, nearP, farP);
            if (d < last) {
                climbs = false;
            }
            last = d;
        }
        check(climbs, "and further along the buffer is always further away");

        // The non-linearity, stated as a number so it cannot quietly go away: the first half of the
        // depth range is the first few centimetres.
        check(render::viewDistance(0.5f, nearP, farP) < 0.25f,
              "half the depth buffer is spent on the first quarter of a metre, which is why this "
              "conversion has to exist at all");
    }

    // ------------------------------------------------------------------ 2. the circle of confusion
    {
        const float focus = 2.5f;
        near(render::blurAmount(focus, focus, 1.0f), 0.0f, 1e-5f,
             "a point at the focus distance is sharp");
        check(render::blurAmount(focus * 1.4f, focus, 1.0f) > 0.0f, "one behind it is not");
        check(render::blurAmount(focus * 0.6f, focus, 1.0f) > 0.0f, "and neither is one in front");

        // The asymmetry, which is the whole reason it is written as the difference of the reciprocals
        // rather than as a distance. The foreground falls away fast and the background slowly: it is
        // why a face a metre in front of a wall is separated from it, and why two trees fifty metres
        // away are not separated from each other.
        const float ahead = render::blurAmount(focus - 1.0f, focus, 1.0f);
        const float behind = render::blurAmount(focus + 1.0f, focus, 1.0f);
        check(ahead > behind * 1.5f, "and the foreground goes soft much faster than the background");

        check(render::blurAmount(200.0f, focus, 4.0f) <= 1.0f, "nothing goes softer than the softest");
        near(render::blurAmount(50.0f, focus, 0.0f), 0.0f, 1e-6f, "and a pinhole blurs nothing");
    }

    // ------------------------------------------------------------------ 3. the picture
    {
        const int w = 96, h = 64;
        const float focus = 3.0f;
        // Half the frame at the focus distance, half of it a long way behind.
        auto depthFor = [&](float metres) {
            // Invert viewDistance, so the test speaks in metres and the code is handed depths.
            return (farP - nearP * farP / metres) / (farP - nearP);
        };

        render::Image img(w, h, render::Color{0.0f, 0.0f, 0.0f, 1.0f});
        std::vector<float> depth(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0.0f);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const bool right = x >= w / 2;
                // A hard black-and-white checker, so softening is unmistakable.
                const float v = ((x / 4 + y / 4) % 2 == 0) ? 1.0f : 0.0f;
                img.setPixel(x, y, render::Color{v, v, v, 1.0f});
                depth[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                      static_cast<std::size_t>(x)] = depthFor(right ? 40.0f : focus);
            }
        }
        const render::Image before = img;
        render::depthOfField(img, 0, h, depth, nearP, farP, focus, 2.0f, 2);

        // The half at the focus distance is untouched, to the last bit. A lens that softens what it
        // is focused on is not a lens, it is a smear.
        int movedInFocus = 0;
        float softestOut = 0.0f;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w / 2 - 8; ++x) {
                if (std::fabs(img.getPixel(x, y).r - before.getPixel(x, y).r) > 0.002f) {
                    ++movedInFocus;
                }
            }
        }
        // And the far half is not: a hard checker that comes through a lens unchanged has not been
        // through one.
        int greys = 0;
        for (int y = 8; y < h - 8; ++y) {
            for (int x = w / 2 + 8; x < w - 8; ++x) {
                const float v = img.getPixel(x, y).r;
                if (v > 0.15f && v < 0.85f) {
                    ++greys;
                }
                softestOut = std::max(softestOut, std::fabs(v - before.getPixel(x, y).r));
            }
        }
        check(movedInFocus == 0, "what the lens is focused on comes through it untouched");
        check(greys > 200, "and what is forty metres behind comes through soft");
        check(softestOut > 0.2f, "by a long way, not by a rounding error");

        // Nothing is invented and nothing is lost: a blur moves light about, it does not make it.
        double sumBefore = 0.0, sumAfter = 0.0;
        for (int y = 0; y < h; ++y) {
            for (int x = w / 2 + 6; x < w - 6; ++x) {
                sumBefore += static_cast<double>(before.getPixel(x, y).r);
                sumAfter += static_cast<double>(img.getPixel(x, y).r);
            }
        }
        check(std::fabs(sumAfter - sumBefore) < sumBefore * 0.12,
              "and the same amount of light comes out as went in");
    }

    // ------------------------------------------------------------------ 4. it can be turned off
    {
        const int w = 40, h = 32;
        render::Image img(w, h, render::Color{0.0f, 0.0f, 0.0f, 1.0f});
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                img.setPixel(x, y, render::Color{x % 2 ? 1.0f : 0.0f, 0.0f, 0.0f, 1.0f});
            }
        }
        const render::Image before = img;
        std::vector<float> depth(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0.99f);
        render::depthOfField(img, 0, h, depth, nearP, farP, 2.0f, 0.0f, 2);
        int moved = 0;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (img.getPixel(x, y).r != before.getPixel(x, y).r) {
                    ++moved;
                }
            }
        }
        check(moved == 0, "a pinhole leaves the picture exactly as it found it");

        // And it refuses the impossible rather than reading off the end of anything: a depth buffer
        // that is too small for the picture, a picture with no rows in it.
        std::vector<float> tooFew(4, 0.5f);
        render::depthOfField(img, 0, h, tooFew, nearP, farP, 2.0f, 1.5f, 2);
        int movedAgain = 0;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (img.getPixel(x, y).r != before.getPixel(x, y).r) {
                    ++movedAgain;
                }
            }
        }
        check(movedAgain == 0, "and a depth buffer that does not fit the picture is refused, not read");

        // A picture too small to blur is left alone rather than walked off the end of. The blur reads
        // a radius either side of every pixel, so a two-row frame has nowhere to read from.
        render::Image sliver(6, 2, render::Color{0.4f, 0.4f, 0.4f, 1.0f});
        sliver.setPixel(3, 1, render::Color{1.0f, 1.0f, 1.0f, 1.0f});
        std::vector<float> thin(12, 0.9f);
        render::depthOfField(sliver, 0, 2, thin, nearP, farP, 2.0f, 2.0f, 2);
        near(sliver.getPixel(3, 1).r, 1.0f, 1e-5f, "and a frame two rows high is left alone");
        near(sliver.getPixel(0, 0).r, 0.4f, 1e-5f, "every pixel of it");
    }

    // ------------------------------------------------------------------ 5. one level and two
    {
        const int w = 80, h = 48;
        auto build = [&](int levels) {
            render::Image img(w, h, render::Color{0.0f, 0.0f, 0.0f, 1.0f});
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    const float v = (x == w / 2) ? 1.0f : 0.0f; // one bright line
                    img.setPixel(x, y, render::Color{v, v, v, 1.0f});
                }
            }
            std::vector<float> depth(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0.0f);
            for (float& d : depth) {
                d = (220.0f - 0.04f * 220.0f / 60.0f) / (220.0f - 0.04f);
            }
            render::depthOfField(img, 0, h, depth, nearP, farP, 2.0f, 3.0f, levels);
            return img;
        };
        // Both spread the line out; two levels spread it further, which is the whole of what the
        // second level is for.
        auto spread = [&](const render::Image& im) {
            int lit = 0;
            for (int x = 0; x < w; ++x) {
                if (im.getPixel(x, h / 2).r > 0.0005f) {
                    ++lit;
                }
            }
            return lit;
        };
        const int one = spread(build(1));
        const int two = spread(build(2));
        check(one > 3, "one level of lens spreads a bright line out");
        check(two > one, "and two levels spread it further, which is what the second level is for");
    }

    if (!failures.empty()) {
        for (const std::string& f : failures) {
            std::printf("FAIL: %s\n", f.c_str());
        }
        std::printf("%zu failed\n", failures.size());
        return 1;
    }
    std::printf("depth of field: all checks passed\n");
    return 0;
}
