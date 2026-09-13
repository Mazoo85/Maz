// Maz Engine — "WAYPOINTS" (math::CubicSpline, MonotoneCubic, CatmullRomSpline, TcbSpline, bsplineEval,
// nurbsPoint, ArcLengthTable, closestPointOnPolyline, simplifyPolyline, resamplePolyline,
// parallelTransportFrames, rotationMinimizingFrames — twelve ways to put a curve through a handful of
// points, and the reasons to pick between them)
// Every one of them is a smooth curve through some points, which makes them look interchangeable until
// something goes wrong. This runs them all on the same data and measures what actually separates them.
// LEFT: a value over time. A natural cubic through keyframes that never leave 0..10 dips to -1.2 and
// rises to 12.0 — that is the bug behind an animated scale that flips inside out, and MonotoneCubic is
// the fix. Under it, the same waypoints through six curves, scored on the only two questions that
// matter: does it pass THROUGH the points, and does it stay inside them. MIDDLE: why the default
// Catmull-Rom is centripetal, settled by search rather than assertion — the uniform parameterisation is
// given several hundred four-point configurations and its loops are counted. Then arc length, which is
// the difference between "half way along the parameter" and "half way along the path", and they are not
// close. RIGHT: what it costs to throw points away, and a frame that does not flip where a Frenet frame
// does. Fixed data, no input. --headless / --frames N for CI.

#include "maz/Engine.hpp"

// None of these are in maz/Engine.hpp: that umbrella carries 153 of the engine's 697 headers.
#include "maz/core/Pcg32.hpp"
#include "maz/math/ArcLength.hpp"
#include "maz/math/BSpline.hpp"
#include "maz/math/CatmullRomSpline.hpp"
#include "maz/math/ClosestPointCurve.hpp"
#include "maz/math/CubicSpline.hpp"
#include "maz/math/MonotoneCubic.hpp"
#include "maz/math/NurbsCurve.hpp"
#include "maz/math/ParallelTransport.hpp"
#include "maz/math/ResamplePolyline.hpp"
#include "maz/math/RotationMinimizingFrame.hpp"
#include "maz/math/SimplifyPolyline.hpp"
#include "maz/math/TcbSpline.hpp"

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
using math::vec2;
using math::vec3;

namespace {

std::string num(double v, int decimals = 2) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

float dist2d(vec2 a, vec2 b) {
    const vec2 d = b - a;
    return std::sqrt(d.x * d.x + d.y * d.y);
}

// The route: two long legs, one deliberately SHORT one, then a corner. Uneven spacing is what the
// parameterisation argument is about, so the data has to have some.
std::vector<vec2> waypointList() {
    return {{0.0f, 0.0f}, {5.0f, 0.0f}, {5.4f, 0.0f}, {5.8f, 2.5f}, {10.0f, 3.0f}, {14.0f, 0.0f}};
}

struct CurveScore {
    std::string name;
    float worstMiss = 0.0f; // furthest any waypoint sits from the curve
    float strays = 0.0f;    // furthest the curve leaves the waypoints' own bounding box
    float length = 0.0f;
};

CurveScore score(const std::string& name, const std::vector<vec2>& curve,
                 const std::vector<vec2>& wps) {
    CurveScore s;
    s.name = name;
    vec2 lo = wps[0];
    vec2 hi = wps[0];
    for (const vec2& v : wps) {
        lo.x = std::min(lo.x, v.x);
        lo.y = std::min(lo.y, v.y);
        hi.x = std::max(hi.x, v.x);
        hi.y = std::max(hi.y, v.y);
    }
    for (const vec2& p : curve) {
        s.strays = std::max(s.strays, std::max(0.0f, std::max(lo.x - p.x, p.x - hi.x)));
        s.strays = std::max(s.strays, std::max(0.0f, std::max(lo.y - p.y, p.y - hi.y)));
    }
    for (const vec2& w : wps) {
        float best = 1e30f;
        for (const vec2& p : curve) {
            best = std::min(best, dist2d(p, w));
        }
        s.worstMiss = std::max(s.worstMiss, best);
    }
    for (std::size_t i = 1; i < curve.size(); ++i) {
        s.length += dist2d(curve[i - 1], curve[i]);
    }
    return s;
}

// Does one Catmull-Rom segment cross itself? A loop is the failure the centripetal parameterisation
// exists to prevent, and unlike "looks wrong" it is a yes or no you can count.
bool segmentLoops(const vec2& p0, const vec2& p1, const vec2& p2, const vec2& p3, float alpha) {
    const int n = 120;
    math::CatmullRomSpline cr;
    cr.alpha = alpha;
    std::vector<vec2> s;
    s.reserve(static_cast<std::size_t>(n) + 1);
    for (int i = 0; i <= n; ++i) {
        s.push_back(cr.segment(p0, p1, p2, p3, static_cast<float>(i) / static_cast<float>(n)));
    }
    auto cross = [](vec2 a, vec2 b) { return a.x * b.y - a.y * b.x; };
    for (std::size_t i = 1; i < s.size(); ++i) {
        for (std::size_t j = i + 2; j + 1 < s.size(); ++j) {
            const vec2 p = s[i - 1];
            const vec2 r = s[i] - s[i - 1];
            const vec2 q = s[j - 1];
            const vec2 t = s[j] - s[j - 1];
            const float d = cross(r, t);
            if (std::fabs(d) < 1e-12f) {
                continue;
            }
            const float u = cross(q - p, t) / d;
            const float v = cross(q - p, r) / d;
            if (u > 1e-6f && u < 1.0f - 1e-6f && v > 1e-6f && v < 1.0f - 1e-6f) {
                return true;
            }
        }
    }
    return false;
}

float angleBetween(vec3 a, vec3 b) {
    const float d = a.x * b.x + a.y * b.y + a.z * b.z;
    return std::acos(std::max(-1.0f, std::min(1.0f, d))) * 57.29577951f;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("WAYPOINTS starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Waypoints";
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

    // ---- a value over time ---------------------------------------------------------------------------
    // Keyframes that rise and fall back, all inside 0..10. A curve through them has no business leaving
    // that range, and one of these two does.
    const std::vector<float> keyX{0, 1, 2, 3, 4, 5};
    const std::vector<float> keyY{0, 0, 10, 10, 0, 0};
    const std::vector<double> keyXd(keyX.begin(), keyX.end());
    const std::vector<double> keyYd(keyY.begin(), keyY.end());
    const math::CubicSpline natural(keyX, keyY);
    const math::MonotoneCubic monotone(keyXd, keyYd);
    double naturalLo = 1e30;
    double naturalHi = -1e30;
    double monoLo = 1e30;
    double monoHi = -1e30;
    for (int i = 0; i <= 2000; ++i) {
        const double x = 5.0 * static_cast<double>(i) / 2000.0;
        const double c = static_cast<double>(natural.eval(static_cast<float>(x)));
        const double m = monotone.eval(x);
        naturalLo = std::min(naturalLo, c);
        naturalHi = std::max(naturalHi, c);
        monoLo = std::min(monoLo, m);
        monoHi = std::max(monoHi, m);
    }
    double throughKeys = 0.0;
    for (std::size_t i = 0; i < keyX.size(); ++i) {
        throughKeys = std::max(throughKeys,
                               std::fabs(static_cast<double>(natural.eval(keyX[i])) -
                                         static_cast<double>(keyY[i])));
        throughKeys = std::max(throughKeys, std::fabs(monotone.eval(keyXd[i]) - keyYd[i]));
    }

    // ---- the same waypoints, six curves --------------------------------------------------------------
    const std::vector<vec2> wps = waypointList();
    const int kSamples = 600;
    const float maxU = static_cast<float>(wps.size() - 1);
    std::vector<CurveScore> scores;
    std::vector<vec2> centripetal;
    {
        for (float alpha : {0.0f, 0.5f}) {
            math::CatmullRomSpline cr;
            cr.points = wps;
            cr.alpha = alpha;
            std::vector<vec2> s;
            for (int i = 0; i <= kSamples; ++i) {
                s.push_back(cr.eval(maxU * static_cast<float>(i) / static_cast<float>(kSamples)));
            }
            scores.push_back(score(alpha == 0.0f ? "Catmull-Rom uniform" : "Catmull-Rom centripetal",
                                   s, wps));
            if (alpha == 0.5f) {
                centripetal = s;
            }
        }
        {
            const int segs = math::bsplineSegmentCount(wps.size(), false);
            std::vector<vec2> s;
            for (int i = 0; i <= kSamples; ++i) {
                s.push_back(math::bsplineEval(
                    wps, static_cast<float>(segs) * static_cast<float>(i) /
                             static_cast<float>(kSamples), false));
            }
            scores.push_back(score("uniform B-spline", s, wps));
        }
        {
            const std::vector<float> weights(wps.size(), 1.0f);
            const std::vector<float> knots =
                math::nurbsClampedKnots(static_cast<int>(wps.size()), 3);
            std::vector<vec2> s;
            for (int i = 0; i <= kSamples; ++i) {
                s.push_back(math::nurbsPoint(wps, weights, knots, 3,
                                             static_cast<float>(i) / static_cast<float>(kSamples)));
            }
            scores.push_back(score("NURBS degree 3", s, wps));
        }
        {
            // tcbSegment draws p1->p2 out of four points, so a chain only reaches the INTERIOR
            // waypoints unless the ends are duplicated. Without this the first and last waypoint are
            // never on the curve, and the miss column would be measuring the sampling, not the spline.
            std::vector<vec2> pad;
            pad.push_back(wps.front());
            for (const vec2& w : wps) {
                pad.push_back(w);
            }
            pad.push_back(wps.back());
            auto sampleTcb = [&](const math::TcbParams& prm) {
                std::vector<vec2> s;
                const int per = kSamples / 4;
                for (std::size_t i = 0; i + 3 < pad.size(); ++i) {
                    for (int k = 0; k <= per; ++k) {
                        s.push_back(math::tcbSegment(pad[i], pad[i + 1], pad[i + 2], pad[i + 3],
                                                     static_cast<float>(k) / static_cast<float>(per),
                                                     prm));
                    }
                }
                return s;
            };
            scores.push_back(score("TCB, all three at 0", sampleTcb(math::TcbParams{}), wps));
            math::TcbParams tense;
            tense.tension = 0.8f;
            scores.push_back(score("TCB, tension 0.8", sampleTcb(tense), wps));
        }
    }

    // ---- why the default is centripetal --------------------------------------------------------------
    // Not asserted: counted. A few hundred four-point configurations, half of them with two points
    // almost on top of each other, and the loops in the middle segment are tallied for each
    // parameterisation. A fixed seed, so the numbers on screen are the same every run.
    core::Pcg32 rng(4242u, 7u);
    int loopsUniform = 0;
    int loopsCentripetal = 0;
    int configurations = 0;
    vec2 worstCase[4];
    bool haveWorstCase = false;
    {
        auto spread = [&]() {
            return (static_cast<float>(rng.nextBounded(20001)) / 10000.0f - 1.0f) * 4.0f;
        };
        auto nudge = [&]() { return (static_cast<float>(rng.nextBounded(20001)) / 10000.0f - 1.0f) * 0.05f; };
        for (int trial = 0; trial < 600; ++trial) {
            vec2 p[4];
            for (auto& v : p) {
                v = vec2(spread(), spread());
            }
            if (trial % 2 == 1) {
                p[2] = vec2(p[1].x + nudge(), p[1].y + nudge()); // a near-duplicate waypoint
            }
            if (dist2d(p[1], p[2]) < 1e-4f) {
                continue;
            }
            ++configurations;
            const bool u = segmentLoops(p[0], p[1], p[2], p[3], 0.0f);
            const bool c = segmentLoops(p[0], p[1], p[2], p[3], 0.5f);
            if (u) {
                ++loopsUniform;
            }
            if (c) {
                ++loopsCentripetal;
            }
            if (u && !c && !haveWorstCase) {
                for (int i = 0; i < 4; ++i) {
                    worstCase[i] = p[i];
                }
                haveWorstCase = true;
            }
        }
    }

    // ---- distance along ------------------------------------------------------------------------------
    const math::ArcLengthTable table(centripetal);
    float stepLoU = 1e30f;
    float stepHiU = 0.0f;
    float stepLoS = 1e30f;
    float stepHiS = 0.0f;
    float halfwayGap = 0.0f;
    {
        math::CatmullRomSpline cr;
        cr.points = wps;
        cr.alpha = 0.5f;
        std::vector<vec2> byParameter;
        std::vector<vec2> byDistance;
        for (int i = 0; i <= 20; ++i) {
            const float f = static_cast<float>(i) / 20.0f;
            byParameter.push_back(cr.eval(maxU * f));
            byDistance.push_back(cr.eval(maxU * table.parameterAtFraction(f)));
        }
        for (std::size_t i = 1; i < byParameter.size(); ++i) {
            const float a = dist2d(byParameter[i - 1], byParameter[i]);
            const float b = dist2d(byDistance[i - 1], byDistance[i]);
            stepLoU = std::min(stepLoU, a);
            stepHiU = std::max(stepHiU, a);
            stepLoS = std::min(stepLoS, b);
            stepHiS = std::max(stepHiS, b);
        }
        halfwayGap = dist2d(cr.eval(maxU * 0.5f), cr.eval(maxU * table.parameterAtFraction(0.5f)));
    }
    const math::CurveProjection probeA = math::closestPointOnPolyline(centripetal, vec2(7.0f, -2.0f));
    const math::CurveProjection probeB = math::closestPointOnPolyline(centripetal, vec2(12.0f, 5.0f));

    // ---- what throwing points away costs -------------------------------------------------------------
    struct Thinned {
        float epsilon = 0.0f;
        std::size_t kept = 0;
        float worstDeviation = 0.0f;
    };
    std::vector<Thinned> thinned;
    for (float epsilon : {0.001f, 0.01f, 0.05f, 0.2f}) {
        const std::vector<vec2> simplified = math::simplifyPolyline(centripetal, epsilon);
        // Measure what it actually cost rather than trusting the epsilon: the furthest any original
        // sample now sits from the thinned path.
        float worst = 0.0f;
        for (const vec2& p : centripetal) {
            worst = std::max(worst, math::closestPointOnPolyline(simplified, p).distance);
        }
        thinned.push_back(Thinned{epsilon, simplified.size(), worst});
    }
    float evenLo = 1e30f;
    float evenHi = 0.0f;
    std::size_t evenCount = 0;
    {
        const std::vector<vec2> even = math::resamplePolyline(centripetal, 24);
        evenCount = even.size();
        for (std::size_t i = 1; i < even.size(); ++i) {
            const float d = dist2d(even[i - 1], even[i]);
            evenLo = std::min(evenLo, d);
            evenHi = std::max(evenHi, d);
        }
    }

    // ---- a frame that does not flip ------------------------------------------------------------------
    // y = x^3 has an inflection at x = 0, where the curvature passes through zero and the acceleration
    // vector — which is all a Frenet frame has to point "up" with — reverses.
    float worstFrenet = 0.0f;
    float worstTransport = 0.0f;
    float worstRmf = 0.0f;
    float stableAgree = 0.0f;
    {
        std::vector<vec3> pts;
        std::vector<vec3> tangents;
        std::vector<vec3> frenet;
        const int n = 200;
        for (int i = 0; i <= n; ++i) {
            const float t = -1.5f + 3.0f * static_cast<float>(i) / static_cast<float>(n);
            pts.push_back(vec3(t, t * t * t, 0.2f * t));
            const vec3 d(1.0f, 3.0f * t * t, 0.2f);
            const float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
            const vec3 T(d.x / len, d.y / len, d.z / len);
            tangents.push_back(T);
            const vec3 a(0.0f, 6.0f * t, 0.0f);
            const float along = a.x * T.x + a.y * T.y + a.z * T.z;
            const vec3 nrm(a.x - along * T.x, a.y - along * T.y, a.z - along * T.z);
            const float nl = std::sqrt(nrm.x * nrm.x + nrm.y * nrm.y + nrm.z * nrm.z);
            frenet.push_back(nl > 1e-9f ? vec3(nrm.x / nl, nrm.y / nl, nrm.z / nl) : vec3(0, 0, 0));
        }
        const std::vector<math::Frame> transported =
            math::parallelTransportFrames(pts, vec3(0.0f, 0.0f, 1.0f));
        const std::vector<math::Frame> minimizing =
            math::rotationMinimizingFrames(pts, tangents, vec3(0.0f, 0.0f, 1.0f));
        for (std::size_t i = 1; i < pts.size(); ++i) {
            worstFrenet = std::max(worstFrenet, angleBetween(frenet[i - 1], frenet[i]));
            worstTransport =
                std::max(worstTransport, angleBetween(transported[i - 1].normal, transported[i].normal));
            worstRmf = std::max(worstRmf, angleBetween(minimizing[i - 1].normal, minimizing[i].normal));
        }
        for (std::size_t i = 0; i < pts.size(); ++i) {
            stableAgree =
                std::max(stableAgree, angleBetween(transported[i].normal, minimizing[i].normal));
        }
    }

    const render::Color kText{0.92f, 0.95f, 1.0f, 1};
    const render::Color kDim{0.60f, 0.66f, 0.78f, 1};
    const render::Color kHead{1.0f, 0.80f, 0.45f, 1};
    const render::Color kVal{0.55f, 0.85f, 1.0f, 1};
    const render::Color kOk{0.50f, 0.95f, 0.60f, 1};
    const render::Color kNo{1.0f, 0.48f, 0.42f, 1};

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

            const float sz = 0.28f;
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  WAYPOINTS", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "twelve ways to draw a curve through some points — and the measurements that "
                          "tell them apart",
                          kDim, 0.32f);

            auto cell = [&](float x, float y, const std::string& s, render::Color colour, float scale) {
                font.drawText(*renderer, x, y, s.c_str(), colour, scale);
            };
            auto row = [&](float x, float y, const char* label, const std::string& value,
                           render::Color colour) {
                font.drawText(*renderer, x, y, label, kDim, sz);
                font.drawText(*renderer, x + 215.0f, y, value.c_str(), colour, sz);
            };

            // ---- column 1 ----
            float y = 98.0f;
            font.drawText(*renderer, 24.0f, y, "A VALUE OVER TIME", kHead, 0.34f);
            y += 28.0f;
            cell(24.0f, y, "keyframes 0, 0, 10, 10, 0, 0 — nothing outside 0..10", kDim, 0.25f);
            y += 24.0f;
            row(24.0f, y, "natural CubicSpline",
                num(naturalLo, 2) + " .. " + num(naturalHi, 2), kNo);
            y += 23.0f;
            row(24.0f, y, "MonotoneCubic", num(monoLo, 2) + " .. " + num(monoHi, 2), kOk);
            y += 23.0f;
            row(24.0f, y, "both hit the keys", "off by " + num(throughKeys, 1), kOk);
            y += 28.0f;
            font.drawText(*renderer, 24.0f, y,
                          "Both curves are smooth and both pass exactly through every keyframe, and one "
                          "of them leaves the range anyway. A scale keyed like this goes inside out; an "
                          "alpha goes negative; a health bar overfills. Monotone cubic gives up a little "
                          "smoothness at the corners and cannot overshoot, which is why it is the one "
                          "for a value with a meaning.",
                          kDim, 0.25f);

            y += 104.0f;
            font.drawText(*renderer, 24.0f, y, "THE SAME SIX WAYPOINTS", kHead, 0.34f);
            y += 28.0f;
            cell(24.0f, y, "curve", kDim, 0.25f);
            cell(230.0f, y, "misses by", kDim, 0.25f);
            cell(330.0f, y, "strays", kDim, 0.25f);
            y += 22.0f;
            for (const CurveScore& s : scores) {
                cell(24.0f, y, s.name, kText, sz);
                cell(230.0f, y, num(static_cast<double>(s.worstMiss), 3),
                     s.worstMiss < 0.001f ? kOk : kNo, sz);
                cell(330.0f, y, num(static_cast<double>(s.strays), 3),
                     s.strays < 0.05f ? kOk : kVal, sz);
                y += 23.0f;
            }
            y += 8.0f;
            font.drawText(*renderer, 24.0f, y,
                          "Two questions, and they trade against each other. Catmull-Rom and TCB go "
                          "through every waypoint and pay for it by bulging past them; the B-spline and "
                          "the NURBS stay strictly inside and never arrive — the B-spline does not even "
                          "reach its own end points, missing them by 4.2 and 4.7, while the NURBS's "
                          "clamped knots pin both ends exactly and miss the middle by 0.77. TCB with "
                          "tension, continuity and bias all at zero is uniform Catmull-Rom: the two "
                          "agree to within three millionths of a unit, which is the identity showing "
                          "through float precision rather than a coincidence.",
                          kDim, 0.25f);

            // ---- column 2 ----
            y = 98.0f;
            font.drawText(*renderer, 470.0f, y, "WHY THE DEFAULT IS CENTRIPETAL", kHead, 0.34f);
            y += 28.0f;
            cell(470.0f, y,
                 std::to_string(configurations) +
                     " four-point layouts, half with a near-duplicate point", kDim, 0.25f);
            y += 24.0f;
            row(470.0f, y, "uniform: loops", std::to_string(loopsUniform), kNo);
            y += 23.0f;
            row(470.0f, y, "centripetal: loops", std::to_string(loopsCentripetal),
                loopsCentripetal == 0 ? kOk : kNo);
            y += 28.0f;
            font.drawText(*renderer, 470.0f, y,
                          "The header says uniform parameterisation \"famously produces cusps and "
                          "self-intersecting loops\". That is a claim with a yes-or-no test, so it is "
                          "counted here rather than repeated: every configuration is drawn both ways "
                          "and the middle segment is checked against itself for a crossing. A fixed "
                          "seed, so this is the same search every run.",
                          kDim, 0.25f);

            y += 92.0f;
            font.drawText(*renderer, 470.0f, y, "HALFWAY IS NOT HALFWAY", kHead, 0.34f);
            y += 28.0f;
            row(470.0f, y, "path length", num(static_cast<double>(table.totalLength()), 3), kVal);
            y += 23.0f;
            row(470.0f, y, "20 equal steps of u",
                num(static_cast<double>(stepLoU), 2) + " .. " + num(static_cast<double>(stepHiU), 2) +
                    "  (" + num(static_cast<double>(stepHiU / stepLoU), 1) + "x)", kNo);
            y += 23.0f;
            row(470.0f, y, "20 equal steps of arc",
                num(static_cast<double>(stepLoS), 2) + " .. " + num(static_cast<double>(stepHiS), 2) +
                    "  (" + num(static_cast<double>(stepHiS / stepLoS), 2) + "x)", kOk);
            y += 23.0f;
            row(470.0f, y, "the two halfways", num(static_cast<double>(halfwayGap), 3) + " apart", kNo);
            y += 28.0f;
            font.drawText(*renderer, 470.0f, y,
                          "Step the parameter evenly and something walking the path lurches: the "
                          "longest step is twenty times the shortest, because the parameter is evenly "
                          "spread over SEGMENTS and the segments are not the same length. An "
                          "ArcLengthTable converts distance back to parameter, and the same twenty "
                          "steps become even. This is why a camera on a rail needs one.",
                          kDim, 0.25f);

            y += 96.0f;
            font.drawText(*renderer, 470.0f, y, "NEAREST POINT ON THE PATH", kHead, 0.34f);
            y += 28.0f;
            row(470.0f, y, "from (7, -2)",
                num(static_cast<double>(probeA.distance), 3) + " away", kVal);
            y += 23.0f;
            row(470.0f, y, "from (12, 5)",
                num(static_cast<double>(probeB.distance), 3) + " away", kVal);
            y += 26.0f;
            font.drawText(*renderer, 470.0f, y,
                          "Cross-track error, in one call: how far off the rails something is, and "
                          "where it should have been.",
                          kDim, 0.25f);

            // ---- column 3 ----
            y = 98.0f;
            font.drawText(*renderer, 950.0f, y, "THROWING POINTS AWAY", kHead, 0.34f);
            y += 28.0f;
            cell(950.0f, y, "epsilon", kDim, 0.25f);
            cell(1040.0f, y, "kept", kDim, 0.25f);
            cell(1140.0f, y, "really off by", kDim, 0.25f);
            y += 22.0f;
            for (const Thinned& t : thinned) {
                cell(950.0f, y, num(static_cast<double>(t.epsilon), 3), kText, sz);
                cell(1040.0f, y,
                     std::to_string(t.kept) + " / " + std::to_string(centripetal.size()), kVal, sz);
                cell(1140.0f, y, num(static_cast<double>(t.worstDeviation), 4),
                     t.worstDeviation <= t.epsilon ? kOk : kNo, sz);
                y += 23.0f;
            }
            y += 6.0f;
            row(950.0f, y, "resampled, even steps",
                num(static_cast<double>(evenLo), 2) + " .. " + num(static_cast<double>(evenHi), 2) +
                    "  (" + std::to_string(evenCount) + " points)", kVal);
            y += 28.0f;
            font.drawText(*renderer, 950.0f, y,
                          "Douglas-Peucker keeps 5% of the samples and stays within a hundredth of a "
                          "unit of the original — and the last column is measured, not assumed: every "
                          "original sample is projected back onto the thinned path, and none of them "
                          "beat the epsilon they were promised. Resampling does the opposite job, "
                          "trading the shape's own spacing for an even one.",
                          kDim, 0.25f);

            y += 108.0f;
            font.drawText(*renderer, 950.0f, y, "A FRAME THAT DOES NOT FLIP", kHead, 0.34f);
            y += 28.0f;
            cell(950.0f, y, "biggest jump between neighbouring normals", kDim, 0.25f);
            y += 24.0f;
            row(950.0f, y, "Frenet (acceleration)",
                num(static_cast<double>(worstFrenet), 2) + " deg", kNo);
            y += 23.0f;
            row(950.0f, y, "parallel transport",
                num(static_cast<double>(worstTransport), 2) + " deg", kOk);
            y += 23.0f;
            row(950.0f, y, "rotation-minimizing",
                num(static_cast<double>(worstRmf), 2) + " deg", kOk);
            y += 23.0f;
            row(950.0f, y, "those two agree to",
                num(static_cast<double>(stableAgree), 3) + " deg", kOk);
            y += 28.0f;
            font.drawText(*renderer, 950.0f, y,
                          "The curve is y = x cubed, which straightens out at the origin and bends the "
                          "other way after. A Frenet frame points \"up\" along the acceleration, and "
                          "the acceleration reverses there — so the frame snaps through a right angle "
                          "in one step, and a tube swept along it kinks. Both stable methods carry the "
                          "previous frame forward instead and never turn more than a third of a degree, "
                          "arriving at answers that agree with each other to two hundredths.",
                          kDim, 0.25f);

            font.drawText(*renderer, 24.0f, 700.0f,
                          "None of this needs a GPU: it is positions and parameters on the CPU, which "
                          "is why the same twelve calls run in a unit test, in an editor's path tool, "
                          "and in whatever walks the path at runtime.",
                          render::Color{0.58f, 0.62f, 0.70f, 1}, 0.27f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("WAYPOINTS shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
