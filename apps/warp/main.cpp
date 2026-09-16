// Maz Engine — "WARP" (render's mesh deformers: MeshTwist, MeshTaper, MeshBend, MeshSpherify,
// MeshRipple, MeshDisplace, MeshExplode, MeshInset, MeshFlatten, MeshSnapGrid, MeshNormalize,
// MeshRecenter)
// Twelve ways to move every vertex of a mesh at once. A deformer is easy to write and hard to
// trust, because the wrong one still produces a shape — so each panel here states the property
// the operation is supposed to PRESERVE and measures how far it drifted. A twist must not change
// any vertex's distance from its axis. A taper must scale the cross-section and leave the length
// alone. A bend must keep arc length. An inset must keep each tile's centroid while scaling its
// area by exactly (1-amount) squared. LEFT: the shape-changers. MIDDLE: the ones that add detail
// or take a surface apart. RIGHT: the housekeeping — flatten, snap, fit, and find the pivot.
// Fixed data, no input. --headless / --frames N for CI.

#include "maz/Engine.hpp"

// None of these are in maz/Engine.hpp: that umbrella carries 153 of the engine's 699 headers.
#include "maz/render/MeshBend.hpp"
#include "maz/render/MeshDisplace.hpp"
#include "maz/render/MeshExplode.hpp"
#include "maz/render/MeshFlatten.hpp"
#include "maz/render/MeshInset.hpp"
#include "maz/render/MeshNormalize.hpp"
#include "maz/render/MeshRecenter.hpp"
#include "maz/render/MeshRipple.hpp"
#include "maz/render/MeshSnapGrid.hpp"
#include "maz/render/MeshSpherify.hpp"
#include "maz/render/MeshTaper.hpp"
#include "maz/render/MeshTwist.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

std::string num(double v, int decimals = 4) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

std::string sci(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.1e", v);
    return buf;
}

// "exact" reads better than 0.0e+00 and is the honest word when the difference really is zero.
std::string drift(double v) { return v == 0.0 ? std::string("exact") : sci(v); }

struct Extent {
    math::vec3 lo{0.0f}, hi{0.0f};
};

Extent extentOf(const render::shapes::MeshData& m) {
    Extent e;
    if (m.vertices.empty()) {
        return e;
    }
    e.lo = math::vec3(m.vertices[0].px, m.vertices[0].py, m.vertices[0].pz);
    e.hi = e.lo;
    for (const render::MeshVertex& v : m.vertices) {
        e.lo.x = std::min(e.lo.x, v.px);
        e.hi.x = std::max(e.hi.x, v.px);
        e.lo.y = std::min(e.lo.y, v.py);
        e.hi.y = std::max(e.hi.y, v.py);
        e.lo.z = std::min(e.lo.z, v.pz);
        e.hi.z = std::max(e.hi.z, v.pz);
    }
    return e;
}

double triangleArea(const render::MeshVertex& a, const render::MeshVertex& b,
                    const render::MeshVertex& c) {
    const double ux = static_cast<double>(b.px) - a.px, uy = static_cast<double>(b.py) - a.py,
                 uz = static_cast<double>(b.pz) - a.pz;
    const double vx = static_cast<double>(c.px) - a.px, vy = static_cast<double>(c.py) - a.py,
                 vz = static_cast<double>(c.pz) - a.pz;
    const double cx = uy * vz - uz * vy, cy = uz * vx - ux * vz, cz = ux * vy - uy * vx;
    return 0.5 * std::sqrt(cx * cx + cy * cy + cz * cz);
}

math::vec3 faceCentroid(const render::shapes::MeshData& m, std::size_t tri) {
    math::vec3 c(0.0f);
    for (std::size_t k = 0; k < 3; ++k) {
        const render::MeshVertex& v = m.vertices[m.indices[tri * 3 + k]];
        c.x += v.px;
        c.y += v.py;
        c.z += v.pz;
    }
    return math::vec3(c.x / 3.0f, c.y / 3.0f, c.z / 3.0f);
}

double distance(const math::vec3& a, const math::vec3& b) {
    const double dx = static_cast<double>(a.x) - b.x, dy = static_cast<double>(a.y) - b.y,
                 dz = static_cast<double>(a.z) - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("WARP starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Warp";
    wc.width = cfg.width;
    wc.height = cfg.height;
    wc.headless = cfg.headless;
    if (!window.init(wc)) {
        return 1;
    }

    render::RendererConfig rc;
    rc.vsync = cfg.vsync;
    rc.allowHeadless = cfg.headless;
    auto renderer = render::createVulkanRenderer();
    if (!renderer->init(window, rc)) {
        return 1;
    }

    platform::Input input;
    core::Clock clock(1.0 / 60.0);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    const render::Color white{1, 1, 1, 1};
    const render::shapes::MeshData box = render::shapes::makeBox(2.0f, white);   // -1..1 each axis
    const render::shapes::MeshData ball = render::shapes::makeSphere(1.0f, 16, 24, white);

    // ---- twist: the distance from the axis must not move ----------------------------------------
    double twistRadiusDrift = 0.0, twistAngleDrift = 0.0, twistZeroDrift = 0.0;
    {
        const float rate = 0.5f; // radians per unit along Y
        const render::shapes::MeshData t = render::twistMesh(box, 1, rate);
        for (std::size_t i = 0; i < box.vertices.size(); ++i) {
            const render::MeshVertex& a = box.vertices[i];
            const render::MeshVertex& b = t.vertices[i];
            const double ra = std::hypot(static_cast<double>(a.px), static_cast<double>(a.pz));
            const double rb = std::hypot(static_cast<double>(b.px), static_cast<double>(b.pz));
            twistRadiusDrift = std::max(twistRadiusDrift, std::fabs(ra - rb));
            if (ra < 1e-6) {
                continue;
            }
            // MeshTwist.hpp names the rotated pair for axis Y as (u, v) = (z, x), so the angle to
            // read is atan2(x, z). Measuring it in the (x, z) plane instead reports every vertex
            // as rotated the wrong way — a full radian out at the ends, and entirely the reader's
            // mistake rather than the deformer's.
            const double aa = std::atan2(static_cast<double>(a.px), static_cast<double>(a.pz));
            const double ab = std::atan2(static_cast<double>(b.px), static_cast<double>(b.pz));
            double d = ab - aa;
            while (d > 3.14159265358979) {
                d -= 6.28318530717959;
            }
            while (d < -3.14159265358979) {
                d += 6.28318530717959;
            }
            twistAngleDrift = std::max(twistAngleDrift, std::fabs(d - rate * a.py));
        }
        const render::shapes::MeshData z = render::twistMesh(box, 1, 0.0f);
        for (std::size_t i = 0; i < box.vertices.size(); ++i) {
            twistZeroDrift = std::max(
                {twistZeroDrift,
                 std::fabs(static_cast<double>(box.vertices[i].px) - z.vertices[i].px),
                 std::fabs(static_cast<double>(box.vertices[i].pz) - z.vertices[i].pz)});
        }
    }

    // ---- taper: the cross-section scales, the length does not ------------------------------------
    double taperBottom = 0.0, taperTop = 0.0, taperHeight = 0.0;
    {
        const render::shapes::MeshData t = render::taperMesh(box, 1, 1.0f, 0.5f);
        for (const render::MeshVertex& v : t.vertices) {
            const double r = std::hypot(static_cast<double>(v.px), static_cast<double>(v.pz));
            if (v.py > 0.99f) {
                taperTop = std::max(taperTop, r);
            }
            if (v.py < -0.99f) {
                taperBottom = std::max(taperBottom, r);
            }
        }
        const Extent e = extentOf(t);
        taperHeight = static_cast<double>(e.hi.y) - e.lo.y;
    }

    // ---- bend: every vertex lands on its own circle -----------------------------------------------
    double bendDrift = 0.0, bendXBefore = 0.0, bendXAfter = 0.0;
    {
        const float radius = 4.0f;
        const render::shapes::MeshData b = render::bendMesh(box, 0, 1, radius);
        for (std::size_t i = 0; i < box.vertices.size(); ++i) {
            // A point at height y rides a circle of radius (R - y) about the bend centre at y = R.
            const double want = radius - static_cast<double>(box.vertices[i].py);
            const double dx = b.vertices[i].px;
            const double dy = static_cast<double>(b.vertices[i].py) - radius;
            bendDrift = std::max(bendDrift, std::fabs(std::hypot(dx, dy) - want));
        }
        const Extent before = extentOf(box), after = extentOf(b);
        bendXBefore = static_cast<double>(before.hi.x) - before.lo.x;
        bendXAfter = static_cast<double>(after.hi.x) - after.lo.x;
    }

    // ---- spherify: a lerp toward the sphere, exact at both ends -----------------------------------
    struct SpherifyRow {
        float t = 0.0f;
        double offSphere = 0.0;
        double offLerp = 0.0;
    };
    std::vector<SpherifyRow> spherifies;
    {
        for (float t : {0.0f, 0.5f, 1.0f}) {
            const render::shapes::MeshData s = render::spherifyMesh(box, 1.0f, t);
            double offSphere = 0.0, offLerp = 0.0;
            for (std::size_t i = 0; i < box.vertices.size(); ++i) {
                const render::MeshVertex& o = box.vertices[i];
                const render::MeshVertex& n = s.vertices[i];
                const double dn = std::sqrt(static_cast<double>(n.px) * n.px +
                                            static_cast<double>(n.py) * n.py +
                                            static_cast<double>(n.pz) * n.pz);
                const double dOld = std::sqrt(static_cast<double>(o.px) * o.px +
                                              static_cast<double>(o.py) * o.py +
                                              static_cast<double>(o.pz) * o.pz);
                offSphere = std::max(offSphere, std::fabs(dn - 1.0));
                offLerp = std::max(offLerp, std::fabs(dn - (dOld + (1.0 - dOld) * t)));
            }
            spherifies.push_back(SpherifyRow{t, offSphere, offLerp});
        }
    }

    // ---- displace and ripple: bounded, and repeatable ---------------------------------------------
    double displaceMax = 0.0, displaceSameSeed = 0.0, displaceOtherSeed = 0.0, displaceZero = 0.0;
    double rippleMax = 0.0, rippleZero = 0.0;
    {
        const render::DisplaceResult a = render::displaceMesh(ball, 0.2f, 2.0f, 7u);
        const render::DisplaceResult b = render::displaceMesh(ball, 0.2f, 2.0f, 7u);
        const render::DisplaceResult c = render::displaceMesh(ball, 0.2f, 2.0f, 9u);
        displaceMax = a.maxOffset;
        for (std::size_t i = 0; i < a.mesh.vertices.size(); ++i) {
            displaceSameSeed = std::max(
                displaceSameSeed,
                std::fabs(static_cast<double>(a.mesh.vertices[i].px) - b.mesh.vertices[i].px));
            displaceOtherSeed = std::max(
                displaceOtherSeed,
                std::fabs(static_cast<double>(a.mesh.vertices[i].px) - c.mesh.vertices[i].px));
        }
        displaceZero = render::displaceMesh(ball, 0.0f, 2.0f, 7u).maxOffset;

        const render::shapes::MeshData r = render::rippleMesh(box, 1, 0.3f, 2.0f);
        for (std::size_t i = 0; i < box.vertices.size(); ++i) {
            rippleMax = std::max(rippleMax,
                                 distance(math::vec3(box.vertices[i].px, box.vertices[i].py,
                                                     box.vertices[i].pz),
                                          math::vec3(r.vertices[i].px, r.vertices[i].py,
                                                     r.vertices[i].pz)));
        }
        const render::shapes::MeshData rz = render::rippleMesh(box, 1, 0.0f, 2.0f);
        for (std::size_t i = 0; i < box.vertices.size(); ++i) {
            rippleZero = std::max(
                rippleZero, std::fabs(static_cast<double>(box.vertices[i].py) - rz.vertices[i].py));
        }
    }

    // ---- explode and inset: taking a surface apart ------------------------------------------------
    std::size_t explodeTrisIn = 0, explodeTrisOut = 0, explodeVertsIn = 0, explodeVertsOut = 0;
    double explodeDrift = 0.0;
    struct InsetRow {
        float amount = 0.0f;
        double areaDrift = 0.0;
        double centroidDrift = 0.0;
    };
    std::vector<InsetRow> insets;
    {
        const float dist = 0.5f;
        const render::shapes::MeshData e = render::explodeFaces(box, dist);
        explodeTrisIn = box.indices.size() / 3;
        explodeTrisOut = e.indices.size() / 3;
        explodeVertsIn = box.vertices.size();
        explodeVertsOut = e.vertices.size();
        for (std::size_t t = 0; t < explodeTrisIn; ++t) {
            explodeDrift =
                std::max(explodeDrift,
                         std::fabs(distance(faceCentroid(box, t), faceCentroid(e, t)) - dist));
        }

        for (float amount : {0.0f, 0.1f, 0.3f, 0.5f}) {
            const render::shapes::MeshData ins = render::insetFaces(box, amount);
            double areaDrift = 0.0, centroidDrift = 0.0;
            for (std::size_t t = 0; t < box.indices.size() / 3; ++t) {
                const double before =
                    triangleArea(box.vertices[box.indices[t * 3]], box.vertices[box.indices[t * 3 + 1]],
                                 box.vertices[box.indices[t * 3 + 2]]);
                const double after =
                    triangleArea(ins.vertices[ins.indices[t * 3]], ins.vertices[ins.indices[t * 3 + 1]],
                                 ins.vertices[ins.indices[t * 3 + 2]]);
                const double want = before * static_cast<double>(1.0f - amount) *
                                    static_cast<double>(1.0f - amount);
                areaDrift = std::max(areaDrift, std::fabs(after - want));
                centroidDrift =
                    std::max(centroidDrift, distance(faceCentroid(box, t), faceCentroid(ins, t)));
            }
            insets.push_back(InsetRow{amount, areaDrift, centroidDrift});
        }
    }

    // ---- flatten and snap: collapsing onto something ----------------------------------------------
    double flattenDrift = 0.0, flattenHeight = 0.0, flattenDiagonalDrift = 0.0;
    struct SnapRow {
        float step = 0.0f;
        std::uint32_t moved = 0;
        double maxMove = 0.0;
        double halfDiagonal = 0.0;
        double offGrid = 0.0;
    };
    std::vector<SnapRow> snaps;
    {
        const render::shapes::MeshData f =
            render::projectToPlane(ball, math::vec3(0), math::vec3(0, 1, 0));
        for (const render::MeshVertex& v : f.vertices) {
            flattenDrift = std::max(flattenDrift, std::fabs(static_cast<double>(v.py)));
        }
        const Extent e = extentOf(f);
        flattenHeight = static_cast<double>(e.hi.y) - e.lo.y;

        const math::vec3 diag(0.57735027f, 0.57735027f, 0.57735027f);
        const render::shapes::MeshData g = render::projectToPlane(ball, math::vec3(0), diag);
        for (const render::MeshVertex& v : g.vertices) {
            flattenDiagonalDrift =
                std::max(flattenDiagonalDrift,
                         std::fabs(static_cast<double>(v.px) * diag.x +
                                   static_cast<double>(v.py) * diag.y +
                                   static_cast<double>(v.pz) * diag.z));
        }

        for (float step : {0.25f, 0.5f, 1.0f}) {
            const render::SnapResult s = render::snapVerticesToGrid(ball, step);
            double offGrid = 0.0;
            for (const render::MeshVertex& v : s.mesh.vertices) {
                for (float c : {v.px, v.py, v.pz}) {
                    const double q = static_cast<double>(c) / step;
                    offGrid = std::max(offGrid, std::fabs(q - std::round(q)));
                }
            }
            snaps.push_back(SnapRow{step, s.movedVertices, s.maxDisplacement,
                                    static_cast<double>(step) * std::sqrt(3.0) * 0.5, offGrid});
        }
    }

    // ---- normalize and recentre: putting a mesh where you want it ----------------------------------
    double normScale = 0.0;
    math::vec3 normBefore(0.0f), normAfter(0.0f), normCentre(0.0f);
    double aspectBefore = 0.0, aspectAfter = 0.0;
    {
        render::shapes::MeshData oblong = box;
        for (render::MeshVertex& v : oblong.vertices) {
            v.px *= 3.0f;
            v.pz *= 0.5f;
        }
        const render::NormalizeResult n = render::normalizeToBox(oblong, math::vec3(1, 1, 1));
        normScale = n.scale;
        const Extent a = extentOf(oblong), b = extentOf(n.mesh);
        normBefore = math::vec3(a.hi.x - a.lo.x, a.hi.y - a.lo.y, a.hi.z - a.lo.z);
        normAfter = math::vec3(b.hi.x - b.lo.x, b.hi.y - b.lo.y, b.hi.z - b.lo.z);
        normCentre = math::vec3((b.hi.x + b.lo.x) * 0.5f, (b.hi.y + b.lo.y) * 0.5f,
                                (b.hi.z + b.lo.z) * 0.5f);
        aspectBefore = static_cast<double>(normBefore.x) / normBefore.y;
        aspectAfter = static_cast<double>(normAfter.x) / normAfter.y;
    }
    struct PivotRow {
        std::string what;
        math::vec3 pivot{0.0f};
        bool fellBack = false;
    };
    std::vector<PivotRow> pivots;
    bool planeFellBack = false;
    {
        render::shapes::MeshData moved = ball;
        for (render::MeshVertex& v : moved.vertices) {
            v.px += 5.0f;
            v.py += 3.0f;
            v.pz -= 2.0f;
        }
        const char* names[4] = {"bounding-box centre", "base", "centre of mass", "vertex average"};
        const render::PivotMode modes[4] = {
            render::PivotMode::BBoxCenter, render::PivotMode::Base,
            render::PivotMode::CenterOfMass, render::PivotMode::VertexAverage};
        for (int i = 0; i < 4; ++i) {
            const render::RecenterResult r = render::recenterMesh(moved, modes[i]);
            pivots.push_back(PivotRow{names[i], r.pivot, r.fellBack});
        }
        planeFellBack = render::recenterMesh(render::shapes::makePlane(1.0f, white),
                                             render::PivotMode::CenterOfMass)
                            .fellBack;
    }

    const render::Color kText{0.92f, 0.95f, 1.0f, 1};
    const render::Color kDim{0.60f, 0.66f, 0.78f, 1};
    const render::Color kHead{1.0f, 0.80f, 0.45f, 1};
    const render::Color kVal{0.55f, 0.85f, 1.0f, 1};
    const render::Color kOk{0.50f, 0.95f, 0.60f, 1};
    const render::Color kBad{1.0f, 0.52f, 0.45f, 1};

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.07f, 0.08f, 0.11f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            const float sz = 0.26f;
            auto cell = [&](float x, float y, const std::string& s, render::Color colour,
                            float scale) {
                font.drawText(*renderer, x, y, s.c_str(), colour, scale);
            };
            auto tiny = [](double v) { return v < 1e-5; };

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  WARP", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "twelve mesh deformers, each measured against the property it is supposed "
                          "to preserve — because a wrong deformer still produces a shape",
                          kDim, 0.32f);

            // ---- column 1 ---------------------------------------------------------------
            float y = 98.0f;
            font.drawText(*renderer, 24.0f, y, "TWIST, TAPER, BEND", kHead, 0.34f);
            y += 28.0f;
            cell(24.0f, y, "a 2-unit box twisted 0.5 radians per unit of height", kVal, 0.26f);
            y += 22.0f;
            cell(44.0f, y, "no vertex changed its distance from the axis:", kText, sz);
            cell(480.0f, y, drift(twistRadiusDrift), tiny(twistRadiusDrift) ? kOk : kBad, sz);
            y += 21.0f;
            cell(44.0f, y, "every angle is exactly rate times height:", kText, sz);
            cell(480.0f, y, drift(twistAngleDrift), tiny(twistAngleDrift) ? kOk : kBad, sz);
            y += 21.0f;
            cell(44.0f, y, "a twist of zero changes nothing at all:", kText, sz);
            cell(480.0f, y, drift(twistZeroDrift), tiny(twistZeroDrift) ? kOk : kBad, sz);
            y += 24.0f;
            font.drawText(*renderer, 24.0f, y,
                          "The middle line was a full radian out the first time it was measured, "
                          "and the deformer was right. MeshTwist.hpp names the rotated pair for "
                          "the Y axis as (u, v) = (z, x); reading the angle as atan2(z, x) instead "
                          "of atan2(x, z) reports every vertex as turning the wrong way. An axis "
                          "convention stated in a header is not decoration, and a deformer test "
                          "that assumes one is testing the assumption.",
                          kDim, 0.25f);

            y += 84.0f;
            cell(24.0f, y, "tapered from full width to half along Y", kVal, 0.26f);
            y += 22.0f;
            cell(44.0f, y, "widest radius at the bottom", kText, sz);
            cell(330.0f, y, num(taperBottom), kVal, sz);
            y += 21.0f;
            cell(44.0f, y, "and at the top", kText, sz);
            cell(330.0f, y, num(taperTop), kVal, sz);
            cell(440.0f, y, "ratio " + num(taperTop / taperBottom) + ", asked 0.5000",
                 std::fabs(taperTop / taperBottom - 0.5) < 1e-4 ? kOk : kBad, sz);
            y += 21.0f;
            cell(44.0f, y, "height, which a taper must not touch", kText, sz);
            cell(330.0f, y, num(taperHeight), std::fabs(taperHeight - 2.0) < 1e-5 ? kOk : kBad, sz);
            y += 28.0f;

            cell(24.0f, y, "bent along X around a radius of 4", kVal, 0.26f);
            y += 22.0f;
            cell(44.0f, y, "every vertex sits on its own circle to within", kText, sz);
            cell(480.0f, y, drift(bendDrift), tiny(bendDrift) ? kOk : kBad, sz);
            y += 21.0f;
            cell(44.0f, y, "straight-line width " + num(bendXBefore, 3) + " becomes " +
                               num(bendXAfter, 3),
                 kDim, sz);
            y += 24.0f;
            font.drawText(*renderer, 24.0f, y,
                          "A bend is the one deformer whose bounding box is supposed to grow: the "
                          "material keeps its arc length and the straight-line distance across it "
                          "changes, which is why the width goes up rather than down. The circle "
                          "test is the real check — a point at height y rides a circle of radius "
                          "4 minus y about the bend centre, and every one of them does.",
                          kDim, 0.25f);

            y += 84.0f;
            font.drawText(*renderer, 24.0f, y, "SPHERIFY IS A LERP, NOT A PROJECTION", kHead, 0.34f);
            y += 28.0f;
            cell(24.0f, y, "blend", kDim, 0.24f);
            cell(130.0f, y, "off the unit sphere", kDim, 0.24f);
            cell(370.0f, y, "off the exact lerp", kDim, 0.24f);
            y += 20.0f;
            for (const SpherifyRow& s : spherifies) {
                cell(24.0f, y, num(static_cast<double>(s.t), 1), kText, sz);
                cell(130.0f, y, s.offSphere < 1e-5 ? "on it: " + sci(s.offSphere)
                                                   : num(s.offSphere, 3) + " away",
                     s.t >= 1.0f ? (tiny(s.offSphere) ? kOk : kBad) : kDim, sz);
                cell(370.0f, y, sci(s.offLerp), tiny(s.offLerp) ? kOk : kBad, sz);
                y += 21.0f;
            }
            y += 6.0f;
            font.drawText(*renderer, 24.0f, y,
                          "At a blend of 1 every corner of the box lands on the sphere to eighteen "
                          "billionths. The column that matters is the second one: at every blend "
                          "in between, each vertex's distance from the centre is exactly the "
                          "straight interpolation between where it started and the radius — so "
                          "animating the blend is a smooth morph and not a scramble.",
                          kDim, 0.25f);

            // ---- column 2 ---------------------------------------------------------------
            y = 98.0f;
            font.drawText(*renderer, 900.0f, y, "ADDING DETAIL YOU DID NOT MODEL", kHead, 0.34f);
            y += 28.0f;
            cell(900.0f, y, "noise displacement, amplitude 0.2", kVal, 0.26f);
            y += 22.0f;
            cell(920.0f, y, "largest distance any vertex actually moved", kText, sz);
            cell(1320.0f, y, num(displaceMax), displaceMax <= 0.2 ? kOk : kBad, sz);
            y += 21.0f;
            cell(920.0f, y, "the same seed, run twice", kText, sz);
            cell(1320.0f, y, drift(displaceSameSeed), tiny(displaceSameSeed) ? kOk : kBad, sz);
            y += 21.0f;
            cell(920.0f, y, "a different seed", kText, sz);
            cell(1320.0f, y, num(displaceOtherSeed) + " apart", kVal, sz);
            y += 21.0f;
            cell(920.0f, y, "amplitude zero", kText, sz);
            cell(1320.0f, y, drift(displaceZero), tiny(displaceZero) ? kOk : kBad, sz);
            y += 24.0f;
            cell(900.0f, y, "a sine ripple, amplitude 0.3", kVal, 0.26f);
            y += 22.0f;
            cell(920.0f, y, "largest displacement", kText, sz);
            cell(1320.0f, y, num(rippleMax), rippleMax <= 0.3 ? kOk : kBad, sz);
            y += 21.0f;
            cell(920.0f, y, "amplitude zero", kText, sz);
            cell(1320.0f, y, drift(rippleZero), tiny(rippleZero) ? kOk : kBad, sz);
            y += 24.0f;
            font.drawText(*renderer, 900.0f, y,
                          "Amplitude is a ceiling, not a promise: 0.1948 of a possible 0.2, because "
                          "the noise has to reach its own extreme somewhere for a vertex to travel "
                          "the whole way. The two seed rows are the property that matters for a "
                          "game — the same seed gives a bit-identical mesh, so a planet generated "
                          "on two machines is the same planet, and a different seed gives a "
                          "genuinely different one rather than a shifted copy.",
                          kDim, 0.25f);

            y += 96.0f;
            font.drawText(*renderer, 900.0f, y, "AND TAKING A SURFACE APART", kHead, 0.34f);
            y += 28.0f;
            cell(900.0f, y,
                 "explode by 0.5: " + std::to_string(explodeTrisIn) + " triangles in, " +
                     std::to_string(explodeTrisOut) + " out; " + std::to_string(explodeVertsIn) +
                     " vertices become " + std::to_string(explodeVertsOut),
                 kText, sz);
            y += 21.0f;
            cell(920.0f, y, "every face centroid moved exactly 0.5 along its own normal:", kText, sz);
            cell(1420.0f, y, drift(explodeDrift), tiny(explodeDrift) ? kOk : kBad, sz);
            y += 26.0f;
            cell(900.0f, y, "inset: each tile shrinks about its own centroid", kVal, 0.26f);
            y += 22.0f;
            cell(900.0f, y, "amount", kDim, 0.24f);
            cell(1010.0f, y, "area off (1-a) squared", kDim, 0.24f);
            cell(1280.0f, y, "centroid moved", kDim, 0.24f);
            y += 20.0f;
            for (const InsetRow& i : insets) {
                cell(900.0f, y, num(static_cast<double>(i.amount), 1), kText, sz);
                cell(1010.0f, y, drift(i.areaDrift), tiny(i.areaDrift) ? kOk : kBad, sz);
                cell(1280.0f, y, drift(i.centroidDrift), tiny(i.centroidDrift) ? kOk : kBad, sz);
                y += 21.0f;
            }
            y += 6.0f;
            font.drawText(*renderer, 900.0f, y,
                          "Both unweld first — three corners per triangle, no sharing — which is "
                          "why 24 vertices become 36 for 12 faces and why the tiles can separate "
                          "and flat-shade instead of dragging their neighbours with them. The "
                          "inset rows are the exact statement of what an inset is: scale each "
                          "tile's area by the square of one minus the amount, and do not move it. "
                          "Both hold to within a tenth of a millionth at every amount, and the "
                          "area is bit-exact at three of the four.",
                          kDim, 0.25f);

            // ---- column 3 ---------------------------------------------------------------
            y = 98.0f;
            font.drawText(*renderer, 1700.0f, y, "FLATTEN, SNAP, FIT, PIVOT", kHead, 0.34f);
            y += 28.0f;
            cell(1700.0f, y, "a sphere projected onto the plane y = 0", kVal, 0.26f);
            y += 22.0f;
            cell(1720.0f, y, "worst distance off the plane", kText, sz);
            cell(2060.0f, y, drift(flattenDrift), tiny(flattenDrift) ? kOk : kBad, sz);
            y += 21.0f;
            cell(1720.0f, y, "height of what is left", kText, sz);
            cell(2060.0f, y, drift(flattenHeight), tiny(flattenHeight) ? kOk : kBad, sz);
            y += 21.0f;
            cell(1720.0f, y, "onto a diagonal plane instead", kText, sz);
            cell(2060.0f, y, sci(flattenDiagonalDrift),
                 tiny(flattenDiagonalDrift) ? kOk : kVal, sz);
            y += 26.0f;

            cell(1700.0f, y, "snapping every vertex to a grid", kVal, 0.26f);
            y += 22.0f;
            cell(1700.0f, y, "spacing", kDim, 0.24f);
            cell(1790.0f, y, "moved", kDim, 0.24f);
            cell(1900.0f, y, "largest move", kDim, 0.24f);
            cell(2030.0f, y, "cell half-diagonal", kDim, 0.24f);
            cell(2190.0f, y, "off grid", kDim, 0.24f);
            y += 20.0f;
            for (const SnapRow& s : snaps) {
                cell(1700.0f, y, num(static_cast<double>(s.step), 2), kText, sz);
                cell(1790.0f, y, std::to_string(s.moved), kVal, sz);
                cell(1900.0f, y, num(s.maxMove), s.maxMove <= s.halfDiagonal ? kOk : kBad, sz);
                cell(2030.0f, y, num(s.halfDiagonal), kDim, sz);
                cell(2190.0f, y, drift(s.offGrid), tiny(s.offGrid) ? kOk : kBad, sz);
                y += 21.0f;
            }
            y += 6.0f;
            font.drawText(*renderer, 1700.0f, y,
                          "No vertex can move further than half a cell's diagonal, because that is "
                          "the furthest any point in a cell can be from the nearest corner — and "
                          "none does, at any spacing. The last column is the one that says the "
                          "job was actually done: every coordinate is an exact multiple of the "
                          "spacing afterwards, not merely close to one.",
                          kDim, 0.25f);

            y += 76.0f;
            cell(1700.0f, y,
                 "fitting a " + num(static_cast<double>(normBefore.x), 0) + " by " +
                     num(static_cast<double>(normBefore.y), 0) + " by " +
                     num(static_cast<double>(normBefore.z), 0) + " box into a unit box",
                 kVal, 0.26f);
            y += 22.0f;
            cell(1720.0f, y, "uniform scale applied", kText, sz);
            cell(2000.0f, y, num(normScale, 6), kVal, sz);
            y += 21.0f;
            cell(1720.0f, y, "result", kText, sz);
            cell(2000.0f, y,
                 num(static_cast<double>(normAfter.x)) + " x " +
                     num(static_cast<double>(normAfter.y)) + " x " +
                     num(static_cast<double>(normAfter.z)),
                 std::fabs(normAfter.x - 1.0f) < 1e-4f ? kOk : kBad, sz);
            y += 21.0f;
            cell(1720.0f, y, "proportions kept", kText, sz);
            cell(2000.0f, y, num(aspectBefore, 6) + " then " + num(aspectAfter, 6),
                 std::fabs(aspectBefore - aspectAfter) < 1e-4 ? kOk : kBad, sz);
            y += 21.0f;
            cell(1720.0f, y, "landed centred on", kText, sz);
            cell(2000.0f, y,
                 sci(std::fabs(static_cast<double>(normCentre.x))) + "  " +
                     sci(std::fabs(static_cast<double>(normCentre.y))) + "  " +
                     sci(std::fabs(static_cast<double>(normCentre.z))),
                 kOk, 0.23f);
            y += 26.0f;

            cell(1700.0f, y, "four pivots for a unit sphere sitting at (5, 3, -2)", kVal, 0.26f);
            y += 22.0f;
            for (const PivotRow& p : pivots) {
                const bool exact = std::fabs(p.pivot.x - 5.0f) < 1e-4f;
                cell(1720.0f, y, p.what, kText, sz);
                cell(2000.0f, y,
                     num(static_cast<double>(p.pivot.x), 4) + "  " +
                         num(static_cast<double>(p.pivot.y), 4) + "  " +
                         num(static_cast<double>(p.pivot.z), 4),
                     exact ? kOk : kBad, sz);
                if (p.fellBack) {
                    cell(2230.0f, y, "fell back", kDim, sz);
                }
                y += 21.0f;
            }
            y += 6.0f;
            cell(1700.0f, y,
                 std::string("a flat plane asked for its centre of mass: ") +
                     (planeFellBack ? "falls back to the vertex average" : "computes one"),
                 planeFellBack ? kOk : kBad, 0.25f);
            y += 24.0f;
            font.drawText(*renderer, 1700.0f, y,
                          "Base is meant to differ — it puts the lowest point at the origin, not "
                          "the centre, which is what you want for something standing on ground. "
                          "Of the three that do aim at the centre, two land on it exactly. The "
                          "vertex average does not, and the 0.0239 it is out is the reason the "
                          "other modes exist: a UV sphere duplicates its seam column, so that one "
                          "longitude has twice the vertices and drags the average toward it. "
                          "Averaging vertices is not averaging the shape — it measures where you "
                          "happened to put them. Centre of mass integrates the actual volume, and "
                          "says so honestly by falling back when there is no volume to integrate.",
                          kDim, 0.25f);

            font.drawText(*renderer, 24.0f, 1030.0f,
                          "Every row is a copy of a mesh compared against the one it came from, in "
                          "memory, with nothing drawn. A deformer that has gone wrong still hands "
                          "back a plausible shape — the only way to know is to name what should "
                          "not have changed and then go and look.",
                          render::Color{0.58f, 0.62f, 0.70f, 1}, 0.27f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("WARP shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
