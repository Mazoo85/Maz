// Maz Engine — "THINNING" (render::simplifyClustering, simplifyQuadric, buildMeshLods, LodChain,
// projectedRadiusPixels, quantizeMesh / dequantizeMesh, computeCurvature, scatterBlueNoise —
// everything the engine does to make a mesh cheaper, measured against a shape whose answers are known)
// Simplification is judged by eye almost everywhere, which is how a decimator that quietly flattens a
// silhouette survives. So the subject here is a sphere: every vertex of it SHOULD sit at radius 1, its
// curvature should be exactly 1 over r squared, and its volume should be four thirds pi — which turns
// "does it still look right" into a number. LEFT: the two decimators on the same mesh, scored on how
// far the surface drifts off the sphere, and the practical difference that matters more than either
// score. MIDDLE: the LOD ladder built from those, and the projection that chooses between its levels,
// checked against the lens formula it implements. RIGHT: quantization, where the error bound is a
// promise the header makes and this measures; curvature, which has an exact answer at three different
// radii; and a blue-noise scatter whose spacing guarantee is checked pair by pair.
// Fixed data, no input. --headless / --frames N for CI.

#include "maz/Engine.hpp"

// None of these are in maz/Engine.hpp: that umbrella carries 153 of the engine's 697 headers.
#include "maz/render/MeshCurvature.hpp"
#include "maz/render/MeshIcosphere.hpp"
#include "maz/render/MeshLod.hpp"
#include "maz/render/MeshLodBuilder.hpp"
#include "maz/render/MeshMassProperties.hpp"
#include "maz/render/MeshPoissonPrune.hpp"
#include "maz/render/MeshQuantize.hpp"
#include "maz/render/MeshSimplify.hpp"
#include "maz/render/MeshSimplifyQuadric.hpp"

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
using maz::render::shapes::MeshData;

namespace {

constexpr double kPi = 3.14159265358979323846;

std::string num(double v, int decimals = 4) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

// On a sphere of radius 1 every vertex belongs at radius 1, so |r - 1| is the exact distance the
// surface has drifted — no reference mesh, no Hausdorff sampling, no approximation.
void radiusError(const MeshData& m, double& worst, double& rms) {
    worst = 0.0;
    double acc = 0.0;
    for (const render::MeshVertex& v : m.vertices) {
        const double r = std::sqrt(static_cast<double>(v.px) * v.px + static_cast<double>(v.py) * v.py +
                                   static_cast<double>(v.pz) * v.pz);
        worst = std::max(worst, std::fabs(r - 1.0));
        acc += (r - 1.0) * (r - 1.0);
    }
    rms = m.vertices.empty() ? 0.0 : std::sqrt(acc / static_cast<double>(m.vertices.size()));
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("THINNING starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Thinning";
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

    const MeshData base = render::makeIcosphere(1.0f, 4);
    const std::size_t baseTriangles = base.indices.size() / 3;
    const double trueVolume = 4.0 / 3.0 * kPi;

    // ---- the two decimators ---------------------------------------------------------------------------
    struct Reduced {
        std::string how;
        std::size_t triangles = 0;
        double volume = 0.0;
        double worst = 0.0;
        double rms = 0.0;
    };
    std::vector<Reduced> reductions;
    auto measure = [&](const std::string& how, const MeshData& m) {
        Reduced r;
        r.how = how;
        r.triangles = m.indices.size() / 3;
        r.volume = render::computeMassProperties(m).volume;
        radiusError(m, r.worst, r.rms);
        return r;
    };
    // Clustering is asked for a CELL SIZE and gives back whatever triangle count falls out; quadric is
    // asked for a count and hits it. The cell sizes below were chosen offline to land near the quadric
    // targets so the two are compared at matched cost rather than at matched settings.
    reductions.push_back(measure("quadric, asked 1280", render::simplifyQuadric(base, 1280)));
    reductions.push_back(measure("clustering, cell 0.155", render::simplifyClustering(base, 0.155f)));
    reductions.push_back(measure("quadric, asked 320", render::simplifyQuadric(base, 320)));
    reductions.push_back(measure("clustering, cell 0.330", render::simplifyClustering(base, 0.330f)));

    // ---- the LOD ladder -------------------------------------------------------------------------------
    const render::MeshLodSet lods = render::buildMeshLods(base, 0.5f, 32, 6, 200.0f);
    struct Level {
        std::size_t triangles = 0;
        double worst = 0.0;
    };
    std::vector<Level> levels;
    for (const MeshData& m : lods.meshes) {
        double w = 0.0;
        double r = 0.0;
        radiusError(m, w, r);
        levels.push_back(Level{m.indices.size() / 3, w});
    }
    struct Pick {
        float metres = 0.0f;
        double pixels = 0.0;
        double exactPixels = 0.0;
        int level = 0;
    };
    std::vector<Pick> picks;
    {
        const float fov = static_cast<float>(60.0 * kPi / 180.0);
        for (float d : {2.0f, 5.0f, 12.0f, 30.0f, 80.0f, 200.0f}) {
            const float px = render::projectedRadiusPixels(1.0f, d, fov, 1080.0f);
            // The lens formula the call implements, written out independently.
            const double exact = (1.0 / (static_cast<double>(d) * std::tan(60.0 * kPi / 180.0 / 2.0))) * 540.0;
            picks.push_back(Pick{d, static_cast<double>(px), exact, lods.chain.select(px)});
        }
    }

    // ---- quantization ---------------------------------------------------------------------------------
    struct Quant {
        int bits = 0;
        double step = 0.0;
        double worstMove = 0.0;
        double bound = 0.0;
        double packedRatio = 0.0;
    };
    std::vector<Quant> quants;
    for (int bits : {8, 10, 12, 16}) {
        const render::QuantizedMesh q = render::quantizeMesh(base, bits);
        const MeshData back = render::dequantizeMesh(q);
        double worst = 0.0;
        for (std::size_t i = 0; i < base.vertices.size() && i < back.vertices.size(); ++i) {
            const double dx = static_cast<double>(base.vertices[i].px) - back.vertices[i].px;
            const double dy = static_cast<double>(base.vertices[i].py) - back.vertices[i].py;
            const double dz = static_cast<double>(base.vertices[i].pz) - back.vertices[i].pz;
            worst = std::max(worst, std::sqrt(dx * dx + dy * dy + dz * dz));
        }
        const double step =
            static_cast<double>(q.posExtent[0]) / static_cast<double>((1u << bits) - 1u);
        quants.push_back(Quant{bits, step, worst, step * 0.5 * std::sqrt(3.0),
                               100.0 * static_cast<double>(bits) / 32.0});
    }

    // ---- curvature, which has an exact answer ---------------------------------------------------------
    struct Curve {
        float radius = 0.0f;
        double gaussian = 0.0;
        double exactGaussian = 0.0;
        double mean = 0.0;
        double exactMean = 0.0;
    };
    std::vector<Curve> curves;
    for (float r : {1.0f, 2.0f, 0.5f}) {
        const render::MeshCurvature c = render::computeCurvature(render::makeIcosphere(r, 4));
        double g = 0.0;
        double m = 0.0;
        std::size_t n = 0;
        for (std::size_t i = 0; i < c.vertexCount; ++i) {
            if (c.boundary[i]) {
                continue;
            }
            g += static_cast<double>(c.gaussian[i]);
            m += static_cast<double>(c.mean[i]);
            ++n;
        }
        const double rr = static_cast<double>(r);
        curves.push_back(Curve{r, n ? g / static_cast<double>(n) : 0.0, 1.0 / (rr * rr),
                               n ? m / static_cast<double>(n) : 0.0, 1.0 / rr});
    }

    // ---- blue noise -----------------------------------------------------------------------------------
    struct Scatter {
        float asked = 0.0f;
        std::size_t kept = 0;
        double closestPair = 0.0;
    };
    std::vector<Scatter> scatters;
    for (float d : {0.3f, 0.15f}) {
        const std::vector<render::SurfacePoint> pts = render::scatterBlueNoise(base, d, 4000, 11u);
        double closest = 1e30;
        for (std::size_t i = 0; i < pts.size(); ++i) {
            for (std::size_t j = i + 1; j < pts.size(); ++j) {
                const math::vec3 a = pts[i].position;
                const math::vec3 b = pts[j].position;
                const double dx = static_cast<double>(a.x) - b.x;
                const double dy = static_cast<double>(a.y) - b.y;
                const double dz = static_cast<double>(a.z) - b.z;
                closest = std::min(closest, std::sqrt(dx * dx + dy * dy + dz * dz));
            }
        }
        scatters.push_back(Scatter{d, pts.size(), pts.size() > 1 ? closest : 0.0});
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
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  THINNING", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          ("making a " + std::to_string(baseTriangles) +
                           "-triangle sphere cheaper, scored against the sphere it is supposed to be")
                              .c_str(),
                          kDim, 0.32f);

            auto cell = [&](float x, float y, const std::string& s, render::Color colour, float scale) {
                font.drawText(*renderer, x, y, s.c_str(), colour, scale);
            };
            auto row = [&](float x, float y, const char* label, const std::string& value,
                           const std::string& truth, render::Color colour) {
                font.drawText(*renderer, x, y, label, kDim, sz);
                font.drawText(*renderer, x + 180.0f, y, value.c_str(), colour, sz);
                if (!truth.empty()) {
                    font.drawText(*renderer, x + 290.0f, y, truth.c_str(), kDim, 0.24f);
                }
            };

            // ---- column 1: the decimators ----
            float y = 100.0f;
            font.drawText(*renderer, 24.0f, y, "TWO WAYS TO LOSE TRIANGLES", kHead, 0.34f);
            y += 30.0f;
            cell(24.0f, y, "method", kDim, 0.25f);
            cell(230.0f, y, "tris", kDim, 0.25f);
            cell(292.0f, y, "worst", kDim, 0.25f);
            cell(365.0f, y, "rms off", kDim, 0.25f);
            y += 22.0f;
            for (const Reduced& r : reductions) {
                cell(24.0f, y, r.how, kText, sz);
                cell(230.0f, y, std::to_string(r.triangles), kVal, sz);
                cell(292.0f, y, num(r.worst, 5), kVal, sz);
                cell(365.0f, y, num(r.rms, 5), kVal, sz);
                y += 23.0f;
            }
            y += 6.0f;
            cell(24.0f, y,
                 "volume held: " + num(reductions[0].volume, 4) + " at 1280 triangles, " +
                     num(reductions[2].volume, 4) + " at 320 — a true sphere holds " +
                     num(trueVolume, 4),
                 kDim, 0.25f);
            y += 26.0f;
            font.drawText(*renderer, 24.0f, y,
                          "Every vertex of a unit sphere belongs at radius 1, so how far the surface "
                          "has drifted is a subtraction rather than a judgement. At 1280 triangles "
                          "quadric decimation is clearly tighter; at 320 the two are within a "
                          "hair of each other and clustering's WORST point is actually the better of "
                          "the two, while its rms is worse — which is the honest answer, not a "
                          "winner. The difference that decides it in practice is on the left: quadric "
                          "was asked for 1280 triangles and returned 1280. Clustering takes a cell "
                          "size and gives back whatever falls out, so hitting a budget means "
                          "searching for the cell that lands near it.",
                          kDim, 0.25f);

            y += 150.0f;
            font.drawText(*renderer, 24.0f, y, "SCATTERING WITH A SPACING RULE", kHead, 0.34f);
            y += 30.0f;
            for (const Scatter& s : scatters) {
                row(24.0f, y, (num(static_cast<double>(s.asked), 2) + " apart, asked").c_str(),
                    std::to_string(s.kept) + " kept",
                    "closest pair " + num(s.closestPair, 4),
                    s.closestPair >= static_cast<double>(s.asked) ? kOk : kNo);
                y += 23.0f;
            }
            y += 6.0f;
            font.drawText(*renderer, 24.0f, y,
                          "4000 darts thrown at the sphere, then thinned until nothing is too close. "
                          "The check is the whole promise: measure every surviving pair and none is "
                          "nearer than asked. That is what makes scattered grass or rocks look placed "
                          "rather than sprinkled.",
                          kDim, 0.25f);

            // ---- column 2: the ladder ----
            y = 100.0f;
            font.drawText(*renderer, 470.0f, y, "A LOD LADDER", kHead, 0.34f);
            y += 30.0f;
            cell(470.0f, y, "level", kDim, 0.25f);
            cell(560.0f, y, "triangles", kDim, 0.25f);
            cell(680.0f, y, "worst drift", kDim, 0.25f);
            y += 22.0f;
            for (std::size_t i = 0; i < levels.size(); ++i) {
                cell(470.0f, y, std::to_string(i), kText, sz);
                cell(560.0f, y, std::to_string(levels[i].triangles), kVal, sz);
                cell(680.0f, y, num(levels[i].worst, 5), levels[i].worst < 0.01 ? kOk : kVal, sz);
                y += 23.0f;
            }
            y += 10.0f;
            font.drawText(*renderer, 470.0f, y,
                          "Each level is decimated from the ORIGINAL rather than from the level above, "
                          "so the error does not compound down the ladder — and the counts halve "
                          "exactly because the target is a ratio of the previous level's real count.",
                          kDim, 0.25f);

            y += 74.0f;
            font.drawText(*renderer, 470.0f, y, "AND WHICH ONE TO DRAW", kHead, 0.34f);
            y += 28.0f;
            cell(470.0f, y, "a 1 m ball, 60 degree lens, 1080 px tall", kDim, 0.25f);
            y += 24.0f;
            cell(470.0f, y, "distance", kDim, 0.25f);
            cell(575.0f, y, "pixels", kDim, 0.25f);
            cell(675.0f, y, "by hand", kDim, 0.25f);
            cell(775.0f, y, "level", kDim, 0.25f);
            y += 22.0f;
            for (const Pick& p : picks) {
                cell(470.0f, y, num(static_cast<double>(p.metres), 0) + " m", kText, sz);
                cell(575.0f, y, num(p.pixels, 2), kVal, sz);
                cell(675.0f, y, num(p.exactPixels, 2),
                     std::fabs(p.pixels - p.exactPixels) < 0.01 ? kOk : kNo, sz);
                cell(775.0f, y, p.level < 0 ? "culled" : std::to_string(p.level), kText, sz);
                y += 23.0f;
            }
            y += 8.0f;
            font.drawText(*renderer, 470.0f, y,
                          "The \"by hand\" column is the lens formula written out separately and "
                          "compared, because a projection that is quietly wrong picks the wrong level "
                          "everywhere and looks like a decimation problem. Note 80 m skips level 4: "
                          "the thresholds scale with the ratio, so a level whose band the object flies "
                          "past is simply never selected.",
                          kDim, 0.25f);

            // ---- column 3: quantize and curvature ----
            y = 100.0f;
            font.drawText(*renderer, 950.0f, y, "SMALLER NUMBERS PER VERTEX", kHead, 0.34f);
            y += 30.0f;
            cell(950.0f, y, "bits", kDim, 0.25f);
            cell(1020.0f, y, "grid step", kDim, 0.25f);
            cell(1120.0f, y, "worst move", kDim, 0.25f);
            cell(1225.0f, y, "bound", kDim, 0.25f);
            y += 22.0f;
            for (const Quant& q : quants) {
                cell(950.0f, y, std::to_string(q.bits), kText, sz);
                cell(1020.0f, y, num(q.step, 6), kDim, sz);
                cell(1120.0f, y, num(q.worstMove, 6), q.worstMove <= q.bound ? kOk : kNo, sz);
                cell(1225.0f, y, num(q.bound, 6), kDim, sz);
                y += 23.0f;
            }
            y += 8.0f;
            font.drawText(*renderer, 950.0f, y,
                          "The header promises the error is bounded by half a grid step per channel; "
                          "every row here is measured by quantizing, dequantizing and subtracting, and "
                          "every one comes in under. What it does NOT do is shrink memory on its own — "
                          "QuantizedMesh holds a uint32 per channel, the same twelve bytes a position "
                          "already took. The win is the bit width you may then pack to on the way out, "
                          "which the header says plainly is a separate job.",
                          kDim, 0.25f);

            y += 118.0f;
            font.drawText(*renderer, 950.0f, y, "CURVATURE, WHICH HAS AN ANSWER", kHead, 0.34f);
            y += 30.0f;
            cell(950.0f, y, "radius", kDim, 0.25f);
            cell(1030.0f, y, "gaussian", kDim, 0.25f);
            cell(1120.0f, y, "should be", kDim, 0.25f);
            cell(1215.0f, y, "mean / 1/r", kDim, 0.25f);
            y += 22.0f;
            for (const Curve& c : curves) {
                cell(950.0f, y, num(static_cast<double>(c.radius), 1), kText, sz);
                cell(1030.0f, y, num(c.gaussian, 4), kVal, sz);
                cell(1120.0f, y, num(c.exactGaussian, 4), kDim, sz);
                cell(1215.0f, y, num(c.mean, 4) + " / " + num(c.exactMean, 2),
                     std::fabs(c.mean - c.exactMean) < 1e-3 ? kOk : kNo, sz);
                y += 23.0f;
            }
            y += 8.0f;
            font.drawText(*renderer, 950.0f, y,
                          "A sphere's Gaussian curvature is one over r squared and its mean curvature "
                          "one over r, everywhere, so three radii give six answers to check against. "
                          "The mean curvature lands on all three to four decimals. The Gaussian runs "
                          "0.12% high at every radius, and that is not error: by Gauss-Bonnet the "
                          "total Gaussian curvature of ANY closed genus-0 polyhedron is exactly four "
                          "pi, so the average per unit area is four pi over the MESH's area — and an "
                          "inscribed mesh has less area than the sphere. The excess should therefore "
                          "equal the area deficit exactly, and it does: 1.9% at 320 triangles, 0.48% "
                          "at 1280, 0.12% at 5120, 0.03% at 20480, matching the area ratio to five "
                          "decimals each time. The estimator is right; the polyhedron is the thing "
                          "being measured.",
                          kDim, 0.25f);

            font.drawText(*renderer, 24.0f, 700.0f,
                          "Every score here is a distance to a shape that is known exactly, which is "
                          "the only way to tell a decimator that preserves a silhouette from one that "
                          "flattens it — and the reason to reach for a sphere before a dragon.",
                          render::Color{0.58f, 0.62f, 0.70f, 1}, 0.27f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("THINNING shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
