// tests/render/screenambient.cpp — contact shading, worked out from the depth buffer.
//
// Where two surfaces meet, light gets trapped and it goes dark. Without that, a chair does not rest
// on a floor, it is pasted onto a photograph of one — which is exactly how this renderer's rooms
// looked.
//
// The engine already ships a per-vertex ambient-occlusion baker, and the first attempt used it. It
// does not work here, and the reason is worth keeping: the rooms are built from large boxes, so a
// wall has four corners and no vertices in between. Per-vertex occlusion can only darken the corners
// and smear the result across the whole wall, which is a muddy gradient rather than a shadow in a
// corner — and inside a closed room every vertex is occluded anyway, so it mostly just turns the
// lights down. Rendered and looked at, it was worse than nothing.
//
// This works in screen space instead, from the depth buffer that is already read back for the lens.
// It does not care how the geometry is tessellated, it costs the same on a box as on a statue, and
// it works on the PEOPLE too — which the baked version could never have done, because they are
// rebuilt every frame.
#include "maz/render/ScreenAmbient.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace render = maz::render;

static std::vector<std::string> failures;
static void check(bool ok, const std::string& what) {
    if (!ok) failures.push_back(what);
}
static void near(float got, float want, float tol, const std::string& what) {
    if (!(std::fabs(got - want) <= tol)) {
        char buf[240];
        std::snprintf(buf, sizeof buf, "%s (got %.5f, wanted %.5f +/- %.5f)", what.c_str(),
                      static_cast<double>(got), static_cast<double>(want), static_cast<double>(tol));
        failures.push_back(buf);
    }
}

static const float kNear = 0.04f, kFar = 220.0f;

// A depth-buffer reading for something this many metres away.
static float depthFor(float metres) {
    return (kFar - kNear * kFar / metres) / (kFar - kNear);
}

int main() {
    // ------------------------------------------------------------------ 1. a flat wall
    //
    // Nothing occludes anything on a flat surface, however close the camera is. This is the case that
    // matters most, because it is most of every frame: an occlusion term that dirties flat walls is
    // worse than none at all.
    {
        const int w = 64, h = 48;
        render::Image img(w, h, render::Color{0.6f, 0.6f, 0.6f, 1.0f});
        const render::Image before = img;
        std::vector<float> depth(static_cast<std::size_t>(w) * static_cast<std::size_t>(h),
                                 depthFor(3.0f));
        render::screenAmbientOcclusion(img, 0, h, depth, kNear, kFar, 0.35f, 0.8f);
        float worst = 0.0f;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                worst = std::fmax(worst, before.getPixel(x, y).r - img.getPixel(x, y).r);
            }
        }
        check(worst < 0.02f, "a flat wall is not dirtied by contact shading");
    }

    // A wall seen at an angle is still flat. A depth ramp is the case a naive "is my neighbour
    // nearer?" test gets wrong: every pixel on a receding floor has a nearer neighbour, so a floor
    // would darken to nothing along its whole length.
    {
        const int w = 64, h = 48;
        render::Image img(w, h, render::Color{0.6f, 0.6f, 0.6f, 1.0f});
        const render::Image before = img;
        std::vector<float> depth(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0.0f);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                // A floor running away from the camera: 1.5 m at the bottom, 9 m at the top.
                const float t = static_cast<float>(y) / static_cast<float>(h - 1);
                depth[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                      static_cast<std::size_t>(x)] = depthFor(9.0f - 7.5f * t);
            }
        }
        render::screenAmbientOcclusion(img, 0, h, depth, kNear, kFar, 0.35f, 0.8f);
        float worst = 0.0f;
        for (int y = 4; y < h - 4; ++y) {
            for (int x = 4; x < w - 4; ++x) {
                worst = std::fmax(worst, before.getPixel(x, y).r - img.getPixel(x, y).r);
            }
        }
        check(worst < 0.06f,
              "and neither is a floor running away from the camera, which a naive nearer-neighbour "
              "test would darken along its whole length");
    }

    // ------------------------------------------------------------------ 1b. and at a quarter too
    //
    // The pass works at one pixel in `step`, and the play tier asks for a quarter where the recording
    // tier asks for a half — 1.9 ms a frame against 6.9, which is the difference between the preview
    // showing the film that comes out and the preview showing a different one. A quarter has to find
    // the same corner; it is allowed to be softer about where exactly it starts.
    {
        const int w = 80, h = 60;
        render::Image img(w, h, render::Color{0.6f, 0.6f, 0.6f, 1.0f});
        render::Image quarter = img;
        std::vector<float> depth(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0.0f);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                depth[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                      static_cast<std::size_t>(x)] = depthFor(x < w / 2 ? 3.0f : 2.4f);
            }
        }
        render::screenAmbientOcclusion(img, 0, h, depth, kNear, kFar, 0.5f, 0.85f, 2);
        render::screenAmbientOcclusion(quarter, 0, h, depth, kNear, kFar, 0.5f, 0.85f, 4);

        const auto darkestNear = [&](const render::Image& in) {
            float d = 1.0f;
            for (int x = w / 2 - 8; x < w / 2; ++x) {
                d = std::fmin(d, in.getPixel(x, h / 2).r);
            }
            return d;
        };
        check(darkestNear(quarter) < 0.6f - 0.04f, "a quarter-resolution pass finds the corner too");
        check(std::fabs(darkestNear(quarter) - darkestNear(img)) < 0.12f,
              "and darkens it by much the same amount as the half-resolution one");
        // And the open wall is still left alone, which is the thing a coarser grid could most easily
        // get wrong: one working cell now covers sixteen pixels, so a corner smeared over four times
        // the distance would show up here as the open wall going dark as well.
        check(quarter.getPixel(4, h / 2).r > 0.55f,
              "while the open wall away from it is still not darkened at a quarter");

        // A step outside the range is clamped rather than dividing by nothing or indexing off the end.
        render::Image silly(w, h, render::Color{0.6f, 0.6f, 0.6f, 1.0f});
        render::screenAmbientOcclusion(silly, 0, h, depth, kNear, kFar, 0.5f, 0.85f, 0);
        render::screenAmbientOcclusion(silly, 0, h, depth, kNear, kFar, 0.5f, 0.85f, 99);
        check(silly.getPixel(4, h / 2).r > 0.0f, "an absurd step is clamped, not crashed on");
    }

    // ------------------------------------------------------------------ 2. a corner goes dark
    {
        const int w = 80, h = 60;
        render::Image img(w, h, render::Color{0.6f, 0.6f, 0.6f, 1.0f});
        const render::Image before = img;
        std::vector<float> depth(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0.0f);
        // Left half: a wall 3 m away. Right half: a wall 2.4 m away. They meet down the middle, and
        // the join is where light gets trapped.
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                depth[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                      static_cast<std::size_t>(x)] = depthFor(x < w / 2 ? 3.0f : 2.4f);
            }
        }
        render::screenAmbientOcclusion(img, 0, h, depth, kNear, kFar, 0.5f, 0.85f);

        // The far side of the join darkens: it is the surface being closed in on.
        //
        // Measured as the darkest pixel within a few of the join, rather than at one fixed offset. How
        // WIDE the band is depends on how far the ring reaches, which is a tuning decision that
        // changes; that the join darkens at all is the property. A first version probed exactly two
        // pixels in, and started failing the moment the reach was tightened — testing the tuning
        // instead of the behaviour.
        float darkest = 1.0f;
        for (int x = w / 2 - 6; x < w / 2; ++x) {
            darkest = std::fmin(darkest, img.getPixel(x, h / 2).r);
        }
        const float openFar = img.getPixel(4, h / 2).r;
        const float openNear = img.getPixel(w - 5, h / 2).r;
        check(darkest < openFar - 0.04f, "the inside of a corner goes dark");
        check(openFar > 0.55f, "while the open wall away from it does not");
        near(openNear, before.getPixel(w - 5, h / 2).r, 0.03f,
             "and the surface doing the occluding is left alone — it is not in anybody's corner");

        // It fades with distance from the join rather than stopping dead, or it reads as a drawn line.
        const float away = before.getPixel(w / 2 - 20, h / 2).r - img.getPixel(w / 2 - 20, h / 2).r;
        check((before.getPixel(w / 2 - 1, h / 2).r - darkest) > away,
              "and it is strongest at the join, fading away from it");
    }

    // ------------------------------------------------------------------ 2b. no haloes
    //
    // The artefact this technique is famous for. A figure standing against a far wall is NOT in a
    // corner with it — there are metres of air between them — but every sample taken across the
    // silhouette finds something much nearer, and the result is a dark rim drawn around every person
    // and every object in the film. It is the single most recognisable way for this to look wrong.
    //
    // Three of the guards in the shading exist for this one case, and this is the check that holds
    // them: mutation-testing found that without a foreground-against-background case, all three could
    // be deleted and every other check still passed.
    {
        const int w = 96, h = 72;
        render::Image img(w, h, render::Color{0.6f, 0.6f, 0.6f, 1.0f});
        const render::Image before = img;
        std::vector<float> depth(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0.0f);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                // A post 1.2 m away standing in the middle of a wall 14 m away.
                const bool post = x >= w / 2 - 6 && x < w / 2 + 6;
                depth[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                      static_cast<std::size_t>(x)] = depthFor(post ? 1.2f : 14.0f);
            }
        }
        render::screenAmbientOcclusion(img, 0, h, depth, kNear, kFar, 0.45f, 0.85f);

        float rim = 0.0f;
        for (int y = 6; y < h - 6; ++y) {
            for (int dx = 1; dx <= 6; ++dx) {
                rim = std::fmax(rim, before.getPixel(w / 2 - 6 - dx, y).r -
                                         img.getPixel(w / 2 - 6 - dx, y).r);
                rim = std::fmax(rim, before.getPixel(w / 2 + 5 + dx, y).r -
                                         img.getPixel(w / 2 + 5 + dx, y).r);
            }
        }
        check(rim < 0.05f,
              "a wall fourteen metres behind a post is not given a dark rim around it — the post is "
              "not in a corner with it, it is in front of it");

        // And the same against the sky, which is at the far plane and has nothing behind it at all.
        render::Image sky(w, h, render::Color{0.7f, 0.7f, 0.7f, 1.0f});
        const render::Image skyBefore = sky;
        std::vector<float> skyDepth(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 1.0f);
        for (int y = 0; y < h; ++y) {
            for (int x = w / 2 - 6; x < w / 2 + 6; ++x) {
                skyDepth[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                         static_cast<std::size_t>(x)] = depthFor(1.2f);
            }
        }
        render::screenAmbientOcclusion(sky, 0, h, skyDepth, kNear, kFar, 0.45f, 0.85f);
        float skyRim = 0.0f;
        for (int y = 6; y < h - 6; ++y) {
            for (int dx = 1; dx <= 6; ++dx) {
                skyRim = std::fmax(skyRim, skyBefore.getPixel(w / 2 - 6 - dx, y).r -
                                               sky.getPixel(w / 2 - 6 - dx, y).r);
            }
        }
        check(skyRim < 0.05f, "and neither is the sky behind it");
    }

    // ------------------------------------------------------------------ 3. knobs
    {
        const int w = 64, h = 48;
        auto shade = [&](float radius, float strength) {
            render::Image img(w, h, render::Color{0.6f, 0.6f, 0.6f, 1.0f});
            std::vector<float> depth(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0.0f);
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    depth[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                          static_cast<std::size_t>(x)] = depthFor(x < w / 2 ? 3.0f : 2.4f);
                }
            }
            render::screenAmbientOcclusion(img, 0, h, depth, kNear, kFar, radius, strength);
            float darkest = 1.0f;
            for (int x = w / 2 - 6; x < w / 2; ++x) {
                darkest = std::fmin(darkest, img.getPixel(x, h / 2).r);
            }
            return 0.6f - darkest;
        };
        check(shade(0.5f, 0.85f) > shade(0.5f, 0.3f), "a stronger setting darkens the corner more");
        check(shade(0.5f, 0.85f) > shade(0.12f, 0.85f),
              "and a wider reach finds more of what is closing in");
        near(shade(0.5f, 0.0f), 0.0f, 1e-5f, "and nothing at all turns it off");
    }

    // ------------------------------------------------------------------ 4. the impossible
    {
        // A picture with structure in it, so a refusal that is not really a refusal shows up. Reading
        // off the end of a short buffer is undefined rather than reliably harmless: whatever it finds
        // there, not one pixel of the picture may move.
        render::Image img(24, 16, render::Color{0.5f, 0.5f, 0.5f, 1.0f});
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 24; ++x) {
                const float v = ((x / 3 + y / 3) % 2 == 0) ? 0.3f : 0.8f;
                img.setPixel(x, y, render::Color{v, v, v, 1.0f});
            }
        }
        const render::Image before = img;
        std::vector<float> tooFew(6, 0.5f);
        render::screenAmbientOcclusion(img, 0, 16, tooFew, kNear, kFar, 0.4f, 0.8f);
        int moved = 0;
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 24; ++x) {
                if (img.getPixel(x, y).r != before.getPixel(x, y).r) {
                    ++moved;
                }
            }
        }
        check(moved == 0, "a depth buffer that does not fit the picture is refused, not read");

        std::vector<float> full(static_cast<std::size_t>(24) * 16u, 0.5f);
        render::screenAmbientOcclusion(img, 8, 8, full, kNear, kFar, 0.4f, 0.8f);
        near(img.getPixel(12, 8).r, before.getPixel(12, 8).r, 1e-6f, "and an empty band is left alone");

        // The sky is at the far plane and has nothing behind it. It must not come out grubby.
        render::Image sky(32, 24, render::Color{0.7f, 0.75f, 0.8f, 1.0f});
        std::vector<float> farAway(static_cast<std::size_t>(32) * 24u, 1.0f);
        render::screenAmbientOcclusion(sky, 0, 24, farAway, kNear, kFar, 0.4f, 0.8f);
        near(sky.getPixel(16, 12).r, 0.7f, 0.01f, "and the sky stays clean");
    }

    if (!failures.empty()) {
        for (const std::string& f : failures) {
            std::printf("FAIL: %s\n", f.c_str());
        }
        std::printf("%zu failed\n", failures.size());
        return 1;
    }
    std::printf("screen ambient: all checks passed\n");
    return 0;
}
