// Maz Engine — "HITBOX" (math's intersection suite: RayEllipsoid, RayCylinder, RayCone,
// RayCapsule, RayTorus, RayPlanar, SegmentDistance, SweptSphere, SweptAabb, GjkDistance, Epa,
// MinkowskiSum)
// Twelve modules answering three questions a game asks thousands of times a frame: does this ray
// hit that shape, how far apart are these two things, and where will this moving thing be when it
// stops. All three have exact answers, so every ray here is aimed at a place where the arithmetic
// is a whole number and the error column is the whole point. LEFT: a ray against six analytic
// solids, including the cases that must MISS — the surprising half of a hit test. MIDDLE: the
// distance between two segments, and the capsule overlap built on it. RIGHT: motion resolved
// ahead of time, and two convex shapes in the plane measured apart, measured overlapping, and
// added together.
// Fixed data, no input. --headless / --frames N for CI.

#include "maz/Engine.hpp"

// None of these are in maz/Engine.hpp: that umbrella carries 153 of the engine's 699 headers.
#include "maz/math/Epa.hpp"
#include "maz/math/GjkDistance.hpp"
#include "maz/math/MinkowskiSum.hpp"
#include "maz/math/RayCapsule.hpp"
#include "maz/math/RayCone.hpp"
#include "maz/math/RayCylinder.hpp"
#include "maz/math/RayEllipsoid.hpp"
#include "maz/math/RayPlanar.hpp"
#include "maz/math/RayTorus.hpp"
#include "maz/math/SegmentDistance.hpp"
#include "maz/math/SweptAabb.hpp"
#include "maz/math/SweptSphere.hpp"

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
    std::snprintf(buf, sizeof(buf), "%.0e", v);
    return buf;
}

std::string vec(const math::vec3& v) {
    return num(static_cast<double>(v.x), 3) + " " + num(static_cast<double>(v.y), 3) + " " +
           num(static_cast<double>(v.z), 3);
}

// One ray query and the answer geometry says it should have given.
struct Shot {
    std::string shape;
    std::string aim;
    bool hit = false;
    bool shouldHit = false;
    float t = 0.0f;
    float exact = 0.0f;
    math::vec3 point{0.0f};
    math::vec3 normal{0.0f};
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("HITBOX starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Hitbox";
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

    // ---- a ray against six analytic solids -------------------------------------------------------
    std::vector<Shot> shots;
    {
        using math::vec3;
        const vec3 minusX(-1, 0, 0), minusY(0, -1, 0), minusZ(0, 0, -1);
        auto add = [&](const char* shape, const char* aim, bool shouldHit, float exact, bool hit,
                       float t, const vec3& p, const vec3& n) {
            shots.push_back(Shot{shape, aim, hit, shouldHit, t, exact, p, n});
        };

        // An ellipsoid with radii (2,1,1) at the origin. Down the long axis the surface is at x=2,
        // so a ray starting at x=5 travels exactly 3; down the short axis, exactly 4.
        {
            const vec3 radii(2, 1, 1);
            const auto a = math::rayIntersectsEllipsoid(vec3(5, 0, 0), minusX, vec3(0), radii);
            add("ellipsoid", "down its long axis", true, 3.0f, a.hit, a.t, a.point, a.normal);
            const auto b = math::rayIntersectsEllipsoid(vec3(0, 5, 0), minusY, vec3(0), radii);
            add("ellipsoid", "down its short axis", true, 4.0f, b.hit, b.t, b.point, b.normal);
            const auto c = math::rayIntersectsEllipsoid(vec3(5, 3, 0), minusX, vec3(0), radii);
            add("ellipsoid", "passing above it", false, 0.0f, c.hit, c.t, c.point, c.normal);
            const auto d = math::rayIntersectsEllipsoid(vec3(5, 0, 0), vec3(1, 0, 0), vec3(0), radii);
            add("ellipsoid", "aimed away from it", false, 0.0f, d.hit, d.t, d.point, d.normal);
        }
        // A cylinder of radius 1 and height 2 standing on y = -1.
        {
            const vec3 base(0, -1, 0), axis(0, 1, 0);
            const auto a = math::rayIntersectsCylinder(vec3(5, 0.5f, 0), minusX, base, axis, 1.0f, 2.0f);
            add("cylinder", "at the side wall", true, 4.0f, a.hit, a.t, a.point, a.normal);
            const auto b = math::rayIntersectsCylinder(vec3(0, 5, 0), minusY, base, axis, 1.0f, 2.0f);
            add("cylinder", "straight onto the cap", true, 4.0f, b.hit, b.t, b.point, b.normal);
            const auto c = math::rayIntersectsCylinder(vec3(5, 1.5f, 0), minusX, base, axis, 1.0f, 2.0f);
            add("cylinder", "above its top", false, 0.0f, c.hit, c.t, c.point, c.normal);
        }
        // A cone with its tip at the origin opening upward at 45 degrees: at height y its radius is
        // also y, so the surface at y=1 sits at x=1 and a ray from x=5 travels exactly 4.
        {
            const float halfAngle = static_cast<float>(3.14159265358979323846 / 4.0);
            const auto a = math::rayIntersectsCone(vec3(5, 1, 0), minusX, vec3(0), vec3(0, 1, 0),
                                                   halfAngle, 2.0f);
            add("cone", "where its radius is 1", true, 4.0f, a.hit, a.t, a.point, a.normal);
            const auto b = math::rayIntersectsCone(vec3(5, 0, 0), minusX, vec3(0), vec3(0, 1, 0),
                                                   halfAngle, 2.0f);
            add("cone", "straight at the tip", true, 5.0f, b.hit, b.t, b.point, b.normal);
        }
        // A capsule of radius 1 whose spine runs from (0,-1,0) to (0,1,0).
        {
            const vec3 a0(0, -1, 0), a1(0, 1, 0);
            const auto a = math::rayIntersectsCapsule(vec3(5, 0, 0), minusX, a0, a1, 1.0f);
            add("capsule", "at the barrel", true, 4.0f, a.hit, a.t, a.point, a.normal);
            // Above the spine the capsule is the sphere at (0,1,0): at y=1.5 its surface is at
            // x = sqrt(1 - 0.25), so the ray travels 5 - 0.8660254.
            const float exact = 5.0f - std::sqrt(0.75f);
            const auto b = math::rayIntersectsCapsule(vec3(5, 1.5f, 0), minusX, a0, a1, 1.0f);
            add("capsule", "at the rounded cap", true, exact, b.hit, b.t, b.point, b.normal);
            const auto c = math::rayIntersectsCapsule(vec3(5, 2.5f, 0), minusX, a0, a1, 1.0f);
            add("capsule", "just past the cap", false, 0.0f, c.hit, c.t, c.point, c.normal);
        }
        // A torus lying in the xy plane, major radius 2 and minor 0.5: its outer rim is at x=2.5
        // and the top of its tube, above the ring at x=2, is at z=0.5.
        {
            const vec3 axis(0, 0, 1);
            const auto a = math::rayIntersectsTorus(vec3(5, 0, 0), minusX, vec3(0), axis, 2.0f, 0.5f);
            add("torus", "at the outer rim", true, 2.5f, a.hit, a.t, a.point, a.normal);
            const auto b = math::rayIntersectsTorus(vec3(0, 0, 5), minusZ, vec3(0), axis, 2.0f, 0.5f);
            add("torus", "through the hole", false, 0.0f, b.hit, b.t, b.point, b.normal);
            const auto c = math::rayIntersectsTorus(vec3(2, 0, 5), minusZ, vec3(0), axis, 2.0f, 0.5f);
            add("torus", "onto the top of the tube", true, 4.5f, c.hit, c.t, c.point, c.normal);
        }
        // Flat shapes in the plane z = 0, all five units below a ray pointing down.
        {
            const vec3 n(0, 0, 1), u(1, 0, 0), v(0, 1, 0);
            const auto a = math::rayIntersectsDisk(vec3(0.5f, 0, 5), minusZ, vec3(0), n, 1.0f);
            add("disk", "inside radius 1", true, 5.0f, a.hit, a.t, a.point, a.normal);
            const auto b = math::rayIntersectsDisk(vec3(1.5f, 0, 5), minusZ, vec3(0), n, 1.0f);
            add("disk", "outside radius 1", false, 0.0f, b.hit, b.t, b.point, b.normal);
            const auto c = math::rayIntersectsAnnulus(vec3(0.5f, 0, 5), minusZ, vec3(0), n, 0.8f, 1.0f);
            add("annulus", "in its hole", false, 0.0f, c.hit, c.t, c.point, c.normal);
            const auto d = math::rayIntersectsAnnulus(vec3(0.9f, 0, 5), minusZ, vec3(0), n, 0.8f, 1.0f);
            add("annulus", "on the ring", true, 5.0f, d.hit, d.t, d.point, d.normal);
            const auto e = math::rayIntersectsRect(vec3(0.9f, 0.9f, 5), minusZ, vec3(0), u, v, 1.0f, 1.0f);
            add("rect", "inside the corner", true, 5.0f, e.hit, e.t, e.point, e.normal);
            const auto f = math::rayIntersectsRect(vec3(1.1f, 0.9f, 5), minusZ, vec3(0), u, v, 1.0f, 1.0f);
            add("rect", "just past the corner", false, 0.0f, f.hit, f.t, f.point, f.normal);
        }
    }

    // ---- how far apart are two segments ----------------------------------------------------------
    struct Pair {
        std::string what;
        float distance = 0.0f;
        float exact = 0.0f;
        float s = 0.0f, t = 0.0f;
        math::vec3 a{0.0f}, b{0.0f};
    };
    std::vector<Pair> pairs;
    struct OverlapRow {
        float gap = 0.0f;
        bool overlap = false;
    };
    std::vector<OverlapRow> overlaps;
    {
        using math::vec3;
        {   // Perpendicular and skew: A runs along x through the origin, B along y one unit above.
            const auto c = math::closestBetweenSegments(vec3(0, 0, 0), vec3(1, 0, 0),
                                                        vec3(0.5f, -1, 1), vec3(0.5f, 1, 1));
            pairs.push_back(Pair{"skew, crossing 1 above", c.distance, 1.0f, c.s, c.t, c.pointA, c.pointB});
        }
        {   // Parallel: every pair of points is the same distance apart, so the answer is the offset.
            const auto c = math::closestBetweenSegments(vec3(0, 0, 0), vec3(1, 0, 0),
                                                        vec3(0, 3, 0), vec3(1, 3, 0));
            pairs.push_back(Pair{"parallel, offset by 3", c.distance, 3.0f, c.s, c.t, c.pointA, c.pointB});
        }
        {   // Collinear and disjoint: the closest points are the facing endpoints.
            const auto c = math::closestBetweenSegments(vec3(0, 0, 0), vec3(1, 0, 0),
                                                        vec3(3, 0, 0), vec3(4, 0, 0));
            pairs.push_back(Pair{"collinear, end to end", c.distance, 2.0f, c.s, c.t, c.pointA, c.pointB});
        }
        for (float gap : {0.9f, 1.0f, 1.1f}) {
            overlaps.push_back(OverlapRow{
                gap, math::capsulesOverlap(math::vec3(0, 0, 0), math::vec3(1, 0, 0), 0.5f,
                                           math::vec3(0, gap, 0), math::vec3(1, gap, 0), 0.5f)});
        }
    }

    // ---- where will it be when it hits -----------------------------------------------------------
    struct Sweep {
        std::string what;
        bool hit = false;
        float t = 0.0f;
        float exact = 0.0f;
        std::string detail;
    };
    std::vector<Sweep> sweeps;
    {
        using math::vec3;
        const auto fast = math::sweepSpherePlane(vec3(0, 5, 0), 1.0f, vec3(0, -10, 0), vec3(0, 1, 0), 0.0f);
        sweeps.push_back(Sweep{"a radius-1 sphere at y=5, moving -10", fast.hit, fast.t, 0.4f,
                               "contact at " + vec(fast.point)});
        const auto slow = math::sweepSpherePlane(vec3(0, 5, 0), 1.0f, vec3(0, -3, 0), vec3(0, 1, 0), 0.0f);
        sweeps.push_back(Sweep{"the same sphere, moving only -3", slow.hit, slow.t, 0.0f,
                               "it does not reach the plane this step"});
        const auto away = math::sweepSpherePlane(vec3(0, 5, 0), 1.0f, vec3(0, 10, 0), vec3(0, 1, 0), 0.0f);
        sweeps.push_back(Sweep{"moving away from the plane", away.hit, away.t, 0.0f, "no contact"});
        const math::Aabb3 mover{vec3(-0.5f, -0.5f, -0.5f), vec3(0.5f, 0.5f, 0.5f)};
        const math::Aabb3 wall{vec3(3, -5, -5), vec3(4, 5, 5)};
        const auto box = math::sweptAabbAabb(mover, vec3(10, 0, 0), wall);
        sweeps.push_back(Sweep{"a unit box thrown 10 at a wall 3 away", box.hit, box.t, 0.25f,
                               "the face it hits: " + vec(box.normal)});
    }

    // ---- two convex shapes in the plane ----------------------------------------------------------
    struct Convex {
        float centres = 0.0f;
        bool intersecting = false;
        float gjk = 0.0f, gjkExact = 0.0f;
        float depth = 0.0f, depthExact = 0.0f;
        math::vec2 normal{0.0f, 0.0f};
    };
    std::vector<Convex> convex;
    std::size_t sumPoints = 0;
    float sumWidth = 0.0f, sumHeight = 0.0f;
    {
        auto square = [](float cx, float half) {
            return std::vector<math::vec2>{{cx - half, -half},
                                           {cx + half, -half},
                                           {cx + half, half},
                                           {cx - half, half}};
        };
        const std::vector<math::vec2> a = square(0.0f, 1.0f);
        for (float cx : {3.0f, 2.0f, 1.5f}) {
            const std::vector<math::vec2> b = square(cx, 1.0f);
            const math::GjkResult g = math::gjkDistance(a, b);
            const math::EpaResult e = math::epaPenetration(a, b);
            convex.push_back(Convex{cx, g.intersecting, g.distance, std::max(0.0f, cx - 2.0f),
                                    e.depth, std::max(0.0f, 2.0f - cx), e.normal});
        }
        const std::vector<math::vec2> sum = math::minkowskiSumConvex(a, square(0.0f, 0.5f));
        float mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f;
        for (const math::vec2& p : sum) {
            mnx = std::min(mnx, p.x);
            mxx = std::max(mxx, p.x);
            mny = std::min(mny, p.y);
            mxy = std::max(mxy, p.y);
        }
        sumPoints = sum.size();
        sumWidth = mxx - mnx;
        sumHeight = mxy - mny;
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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  HITBOX", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "twelve intersection modules, every ray aimed at a place where the right "
                          "answer is a whole number — so the error column is the result",
                          kDim, 0.32f);

            // ---- column 1 ----------------------------------------------------------------
            float y = 98.0f;
            font.drawText(*renderer, 24.0f, y, "DOES THIS RAY HIT THAT SHAPE", kHead, 0.34f);
            y += 28.0f;
            cell(24.0f, y, "shape", kDim, 0.24f);
            cell(130.0f, y, "aimed", kDim, 0.24f);
            cell(370.0f, y, "result", kDim, 0.24f);
            cell(450.0f, y, "distance", kDim, 0.24f);
            cell(550.0f, y, "exact", kDim, 0.24f);
            cell(640.0f, y, "off by", kDim, 0.24f);
            cell(720.0f, y, "surface normal", kDim, 0.24f);
            y += 20.0f;
            for (const Shot& s : shots) {
                const bool right = s.hit == s.shouldHit;
                cell(24.0f, y, s.shape, kText, sz);
                cell(130.0f, y, s.aim, kText, sz);
                cell(370.0f, y, s.hit ? "hit" : "miss", right ? (s.hit ? kOk : kDim) : kBad, sz);
                if (s.shouldHit) {
                    const double off = std::fabs(static_cast<double>(s.t) - s.exact);
                    cell(450.0f, y, num(static_cast<double>(s.t)), kVal, sz);
                    cell(550.0f, y, num(static_cast<double>(s.exact)), kDim, sz);
                    cell(640.0f, y, off == 0.0 ? "exact" : sci(off), off < 1e-5 ? kOk : kBad, sz);
                    cell(720.0f, y, vec(s.normal), kVal, 0.23f);
                }
                y += 21.0f;
            }
            y += 8.0f;
            font.drawText(*renderer, 24.0f, y,
                          "Eight of the twenty-one rows are misses, and they are the half of a hit "
                          "test that goes wrong quietly. A ray aimed AWAY from a shape it is "
                          "standing inside the line of must not report the hit behind it. A "
                          "cylinder is not its infinite tube — a ray above its top has to miss "
                          "even though it crosses the axis. A torus has a hole, and a rectangle "
                          "has corners a disk of the same reach would have covered. Getting those "
                          "right is what separates an intersection routine from an equation.",
                          kDim, 0.25f);

            y += 96.0f;
            font.drawText(*renderer, 24.0f, y, "AND WHERE EXACTLY", kHead, 0.34f);
            y += 28.0f;
            font.drawText(*renderer, 24.0f, y,
                          "The normals are the other half. On the cone at (1,1,0) it is "
                          "0.707 -0.707 0: the surface climbs away from the axis, so its outward "
                          "normal tilts DOWN, which is the sign everyone gets backwards. At the "
                          "capsule's cap the normal is 0.866 0.500 0 — pointing out of the sphere "
                          "that caps it, not out of the barrel. And a cone has no normal at its "
                          "tip at all; asked for one, this returns the axis rather than a zero "
                          "vector, which is a choice worth knowing about before you shade with it.",
                          kDim, 0.25f);

            // ---- column 2 ----------------------------------------------------------------
            y = 98.0f;
            font.drawText(*renderer, 1150.0f, y, "HOW FAR APART ARE TWO SEGMENTS", kHead, 0.34f);
            y += 28.0f;
            cell(1150.0f, y, "arrangement", kDim, 0.24f);
            cell(1400.0f, y, "distance", kDim, 0.24f);
            cell(1500.0f, y, "exact", kDim, 0.24f);
            cell(1580.0f, y, "along A", kDim, 0.24f);
            cell(1660.0f, y, "along B", kDim, 0.24f);
            y += 20.0f;
            for (const Pair& p : pairs) {
                const double off = std::fabs(static_cast<double>(p.distance) - p.exact);
                cell(1150.0f, y, p.what, kText, sz);
                cell(1400.0f, y, num(static_cast<double>(p.distance)), off < 1e-5 ? kOk : kBad, sz);
                cell(1500.0f, y, num(static_cast<double>(p.exact), 1), kDim, sz);
                cell(1580.0f, y, num(static_cast<double>(p.s), 2), kVal, sz);
                cell(1660.0f, y, num(static_cast<double>(p.t), 2), kVal, sz);
                y += 21.0f;
            }
            y += 6.0f;
            for (const Pair& p : pairs) {
                cell(1170.0f, y, p.what + ": closest at " + vec(p.a) + "  and  " + vec(p.b), kDim, 0.23f);
                y += 19.0f;
            }
            y += 8.0f;
            for (const OverlapRow& o : overlaps) {
                cell(1150.0f, y,
                     "two radius-0.5 capsules, spines " + num(static_cast<double>(o.gap), 1) +
                         " apart:",
                     kText, sz);
                cell(1440.0f, y, o.overlap ? "overlap" : "clear", o.overlap ? kBad : kOk, sz);
                y += 21.0f;
            }
            y += 6.0f;
            font.drawText(*renderer, 1150.0f, y,
                          "The middle row is the one to remember. Two radii of 0.5 reach exactly "
                          "1.0, so spines exactly 1.0 apart are touching and nothing more — and "
                          "the test says overlap, because it compares with <=. That is the "
                          "conservative answer for a collision query and the wrong one for a "
                          "resolver: push two shapes apart until this returns false and it never "
                          "will, because it cannot. Treat a zero-depth contact as already resolved.",
                          kDim, 0.25f);

            y += 84.0f;
            font.drawText(*renderer, 1150.0f, y, "WHERE WILL IT BE WHEN IT HITS", kHead, 0.34f);
            y += 28.0f;
            for (const Sweep& s : sweeps) {
                cell(1150.0f, y, s.what, kText, sz);
                if (s.hit) {
                    const double off = std::fabs(static_cast<double>(s.t) - s.exact);
                    cell(1480.0f, y, "at " + num(static_cast<double>(s.t)) + " of the step",
                         off < 1e-5 ? kOk : kBad, sz);
                    cell(1680.0f, y, s.detail, kVal, 0.23f);
                } else {
                    cell(1480.0f, y, s.detail, kDim, sz);
                }
                y += 21.0f;
            }
            y += 6.0f;
            font.drawText(*renderer, 1150.0f, y,
                          "A sphere of radius 1 whose centre starts at 5 touches the ground when "
                          "its centre reaches 1, which is four of the ten units it was going to "
                          "travel: 0.4 of the step, found before it moves rather than after it has "
                          "gone through. The box is the same idea in three axes at once and lands "
                          "on 0.25 with the face it struck. This is what stops a fast object "
                          "teleporting through a thin wall between two frames.",
                          kDim, 0.25f);

            y += 84.0f;
            font.drawText(*renderer, 1150.0f, y, "TWO CONVEX SHAPES, IN THE PLANE", kHead, 0.34f);
            y += 28.0f;
            cell(1150.0f, y, "centres apart", kDim, 0.24f);
            cell(1300.0f, y, "gjk says", kDim, 0.24f);
            cell(1410.0f, y, "gap", kDim, 0.24f);
            cell(1490.0f, y, "exact", kDim, 0.24f);
            cell(1570.0f, y, "epa depth", kDim, 0.24f);
            cell(1680.0f, y, "exact", kDim, 0.24f);
            cell(1760.0f, y, "push direction", kDim, 0.24f);
            y += 20.0f;
            for (const Convex& c : convex) {
                cell(1150.0f, y, num(static_cast<double>(c.centres), 1), kText, sz);
                cell(1300.0f, y, c.intersecting ? "touching" : "apart", kVal, sz);
                cell(1410.0f, y, num(static_cast<double>(c.gjk)),
                     std::fabs(c.gjk - c.gjkExact) < 1e-5f ? kOk : kBad, sz);
                cell(1490.0f, y, num(static_cast<double>(c.gjkExact), 2), kDim, sz);
                cell(1570.0f, y, num(static_cast<double>(c.depth)),
                     std::fabs(c.depth - c.depthExact) < 1e-5f ? kOk : kBad, sz);
                cell(1680.0f, y, num(static_cast<double>(c.depthExact), 2), kDim, sz);
                cell(1760.0f, y,
                     num(static_cast<double>(c.normal.x), 2) + " " +
                         num(static_cast<double>(c.normal.y), 2),
                     kVal, sz);
                y += 21.0f;
            }
            y += 6.0f;
            cell(1150.0f, y,
                 "adding a side-2 square to a side-1 square gives " + std::to_string(sumPoints) +
                     " points spanning " + num(static_cast<double>(sumWidth), 2) + " by " +
                     num(static_cast<double>(sumHeight), 2),
                 std::fabs(sumWidth - 3.0f) < 1e-5f ? kOk : kBad, 0.25f);
            y += 24.0f;
            font.drawText(*renderer, 1150.0f, y,
                          "GJK and EPA are the same idea read from either side of zero. GJK walks "
                          "toward the origin from outside the Minkowski difference and reports how "
                          "far it got short; EPA expands outward from inside it and reports how "
                          "deep it is. Neither ever builds that difference — which is the point, "
                          "because for two squares it is a third square, and for two of anything "
                          "else it is a shape nobody wants to enumerate. The bottom row is the "
                          "handover: squares 1.5 apart overlap by exactly 0.5, pushed along -x.",
                          kDim, 0.25f);

            font.drawText(*renderer, 24.0f, 1000.0f,
                          "Nothing here is drawn, nothing is approximated, and nothing needs a "
                          "frame to have happened. Every row is a closed-form answer to a question "
                          "asked before anything moves — which is the only way a collision system "
                          "is ever right at speed.",
                          render::Color{0.58f, 0.62f, 0.70f, 1}, 0.27f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("HITBOX shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
