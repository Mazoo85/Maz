// Surface grain: vary a flat colour by where it is in the world, the same way every time.

#include "maz/render/MeshRefine.hpp"
#include "maz/render/Shapes.hpp"
#include "maz/render/SurfaceGrain.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

using maz::render::shapes::MeshData;

} // namespace

int main() {
    using maz::render::grainAt;
    using maz::render::grainMesh;
    using maz::render::Grain;
    using maz::render::Color;

    // ---- the noise itself --------------------------------------------------------------------------
    {
        float lowest = 2.0f;
        float highest = -2.0f;
        double sum = 0.0;
        int samples = 0;
        for (int i = 0; i < 4000; ++i) {
            const float t = static_cast<float>(i) * 0.031f;
            const float v = grainAt(t, t * 0.7f + 1.3f, t * 1.9f - 4.0f, 0.5f, 7u);
            lowest = std::min(lowest, v);
            highest = std::max(highest, v);
            sum += static_cast<double>(v);
            ++samples;
        }
        check(lowest >= -1.0f && highest <= 1.0f, "the noise stays inside minus one to one");
        check(lowest < -0.4f && highest > 0.4f, "and actually uses the range rather than hugging zero");
        check(std::fabs(sum / samples) < 0.05, "and averages out to nothing, so it does not shift the "
                                               "brightness of everything it touches");
    }

    // ---- it is smooth, not static ------------------------------------------------------------------
    //
    // Noise that jumps from sample to sample is television static, and a wall covered in it looks
    // like a wall covered in static. Within a cell the value has to move gently.
    {
        float worstStep = 0.0f;
        for (int i = 0; i < 2000; ++i) {
            const float x = static_cast<float>(i) * 0.002f;
            const float a = grainAt(x, 3.0f, -1.0f, 0.5f, 11u);
            const float b = grainAt(x + 0.002f, 3.0f, -1.0f, 0.5f, 11u);
            worstStep = std::max(worstStep, std::fabs(a - b));
        }
        // Two millimetres apart, against a cell half a metre across: a quarter of a per cent of the
        // way through a cell cannot legitimately change the value much at all.
        check(worstStep < 0.05f, "two points two millimetres apart get nearly the same value");

        // And no crease where one cell meets the next. Smoothness of the VALUE is not enough —
        // straight lines between cells are continuous too, and still leave a hard line along every
        // boundary because the slope jumps there. What has to match across a boundary is the slope.
        // With a cell of half a metre the boundaries sit at every multiple of 0.5.
        float worstKink = 0.0f;
        for (int i = -6; i <= 6; ++i) {
            const float at = static_cast<float>(i) * 0.5f;
            const float h = 0.004f;
            const float before = (grainAt(at - h, 3.0f, -1.0f, 0.5f, 11u) -
                                  grainAt(at - 2.0f * h, 3.0f, -1.0f, 0.5f, 11u)) / h;
            const float after = (grainAt(at + 2.0f * h, 3.0f, -1.0f, 0.5f, 11u) -
                                 grainAt(at + h, 3.0f, -1.0f, 0.5f, 11u)) / h;
            worstKink = std::max(worstKink, std::fabs(before - after));
        }
        check(worstKink < 0.35f, "and the slope carries across a cell boundary, so there is no crease "
                                 "ruled along every half metre");
    }

    // ---- and it is not the same everywhere ---------------------------------------------------------
    {
        const float a = grainAt(0.0f, 0.0f, 0.0f, 0.5f, 3u);
        const float b = grainAt(5.0f, 0.0f, 0.0f, 0.5f, 3u);
        const float c = grainAt(0.0f, 0.0f, 0.0f, 0.5f, 4u);
        check(std::fabs(a - b) > 0.02f, "two places five metres apart get different values");
        check(std::fabs(a - c) > 0.02f, "and a different seed gives a different field");
    }

    // ---- the same answer every time ----------------------------------------------------------------
    {
        bool same = true;
        for (int i = 0; i < 500; ++i) {
            const float x = static_cast<float>(i) * 0.37f - 40.0f;
            if (grainAt(x, x * 0.3f, x * -0.8f, 0.6f, 21u) !=
                grainAt(x, x * 0.3f, x * -0.8f, 0.6f, 21u)) {
                same = false;
            }
        }
        check(same, "asking twice gives exactly the same number");
        check(grainAt(1.0f, 2.0f, 3.0f, 0.0f, 1u) == 0.0f, "a cell size of zero is refused, not divided by");
    }

    // ---- on a mesh ---------------------------------------------------------------------------------
    {
        MeshData m = maz::render::shapes::makeBox(1.0f, Color{0.5f, 0.5f, 0.5f, 1.0f});
        for (auto& v : m.vertices) {
            v.px *= 6.0f;
            v.py *= 3.0f;
            v.pz *= 0.12f;
        }
        maz::render::refineMesh(m, 0.4f);

        MeshData flat = m;
        Grain g;
        g.amount = 0.12f;
        g.seed = 5u;
        grainMesh(m, g);

        float lowest = 10.0f;
        float highest = -10.0f;
        float worstHue = 0.0f;
        for (std::size_t i = 0; i < m.vertices.size(); ++i) {
            const float k = m.vertices[i].r / flat.vertices[i].r;
            lowest = std::min(lowest, k);
            highest = std::max(highest, k);
            // Every channel moved by the same factor: the palette must come through untouched.
            worstHue = std::max(worstHue, std::fabs(m.vertices[i].g / flat.vertices[i].g - k));
            worstHue = std::max(worstHue, std::fabs(m.vertices[i].b / flat.vertices[i].b - k));
        }
        check(highest - lowest > 0.04f, "a refined wall comes out with real variation across it");
        check(lowest > 1.0f - 0.13f && highest < 1.0f + 0.13f,
              "and stays inside the swing that was asked for");
        check(worstHue < 1e-5f, "and the hue is untouched — brightness only, so the palette survives");
    }

    // ---- an UNREFINED wall is the reason MeshRefine exists -----------------------------------------
    //
    // This is the whole argument for the pair of them in one check. The same grain on the same wall,
    // with and without somewhere to land: four corners can only give a wall a linear ramp, and a
    // linear ramp across six metres is not texture, it is a gradient.
    {
        MeshData coarse = maz::render::shapes::makeBox(1.0f, Color{0.5f, 0.5f, 0.5f, 1.0f});
        for (auto& v : coarse.vertices) {
            v.px *= 6.0f;
            v.py *= 3.0f;
            v.pz *= 0.12f;
        }
        MeshData fine = coarse;
        maz::render::refineMesh(fine, 0.4f);

        Grain g;
        g.amount = 0.12f;
        g.seed = 9u;
        grainMesh(coarse, g);
        grainMesh(fine, g);

        check(coarse.vertices.size() == 24u, "an unrefined box has twenty-four vertices to work with");
        check(fine.vertices.size() > 400u, "and a refined one has hundreds");

        // How much the brightness changes between neighbours in the middle of the big face is the
        // thing that reads as texture. On the coarse mesh there ARE no neighbours in the middle.
        const auto spread = [](const MeshData& mesh) {
            float lo = 10.0f;
            float hi = -10.0f;
            for (const auto& v : mesh.vertices) {
                if (std::fabs(v.pz - 0.06f) < 1e-4f) { // one big face only
                    lo = std::min(lo, v.r);
                    hi = std::max(hi, v.r);
                }
            }
            return hi - lo;
        };
        check(spread(fine) > spread(coarse) * 1.5f,
              "and the refined wall varies far more across one face than the flat one can");
    }

    // ---- dirt at the bottom ------------------------------------------------------------------------
    {
        MeshData m = maz::render::shapes::makeBox(1.0f, Color{0.6f, 0.6f, 0.6f, 1.0f});
        for (auto& v : m.vertices) {
            v.px *= 4.0f;
            v.py = (v.py + 0.5f) * 3.0f; // standing on the floor, not centred on it
            v.pz *= 0.12f;
        }
        maz::render::refineMesh(m, 0.3f);
        Grain g;
        g.amount = 0.0f; // dirt alone, so nothing else can account for the difference
        g.low = 0.25f;
        g.lowOver = 1.0f;
        grainMesh(m, g);

        float atFloor = 1.0f;
        float upHigh = 0.0f;
        for (const auto& v : m.vertices) {
            if (v.py < 0.02f) {
                atFloor = std::min(atFloor, v.r);
            }
            if (v.py > 2.0f) {
                upHigh = std::max(upHigh, v.r);
            }
        }
        check(atFloor < 0.6f * 0.8f, "the bottom of a wall is dirty");
        check(std::fabs(upHigh - 0.6f) < 1e-5f, "and the top of it is not touched at all");

        // And it is CONCENTRATED at the bottom rather than spread evenly up to the fade height.
        // That is the difference between a scuff line along a skirting board and a wall that has
        // simply been painted as a gradient, and it is also what keeps the dirt away from anybody's
        // face. Halfway up the fade, well under a third of the darkening should be left.
        float midway = -1.0f;
        float atBottom = 0.0f;
        for (const auto& v : m.vertices) {
            if (std::fabs(v.py - 0.5f) < 0.1f) { // the refined grid has no vertex exactly at 0.5
                midway = std::max(midway, 0.6f - v.r);
            }
            if (v.py < 0.02f) {
                atBottom = std::max(atBottom, 0.6f - v.r);
            }
        }
        check(midway >= 0.0f && atBottom > 0.0f, "there are vertices to measure the fade at");
        check(midway < atBottom * 0.34f,
              "and halfway up the fade two thirds of the darkening is already gone — a scuff along "
              "the skirting, not a wall painted as a gradient");
    }

    // ---- refusals ----------------------------------------------------------------------------------
    {
        MeshData m = maz::render::shapes::makeBox(2.0f, Color{0.4f, 0.4f, 0.4f, 1.0f});
        const MeshData before = m;
        Grain off;
        off.amount = 0.0f;
        off.low = 0.0f;
        grainMesh(m, off);
        bool untouched = true;
        for (std::size_t i = 0; i < m.vertices.size(); ++i) {
            if (m.vertices[i].r != before.vertices[i].r) {
                untouched = false;
            }
        }
        check(untouched, "a grain of nothing changes nothing");

        MeshData empty;
        Grain g;
        grainMesh(empty, g);
        check(empty.vertices.empty(), "an empty mesh comes back empty");

        // A swing wider than the colour itself must not come out negative and print as a hole.
        MeshData dark = maz::render::shapes::makeBox(2.0f, Color{0.02f, 0.02f, 0.02f, 1.0f});
        maz::render::refineMesh(dark, 0.3f);
        Grain wild;
        wild.amount = 3.0f;
        wild.low = 2.0f;
        grainMesh(dark, wild);
        float darkest = 1.0f;
        for (const auto& v : dark.vertices) {
            darkest = std::min(darkest, std::min(v.r, std::min(v.g, v.b)));
        }
        check(darkest >= 0.0f, "an absurd amount clamps at black rather than going through it");
    }

    if (failures == 0) {
        std::printf("surface grain: all checks passed\n");
    }
    return failures == 0 ? 0 : 1;
}
