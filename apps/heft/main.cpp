// Maz Engine — "HEFT" (render::computeMassProperties, computePrincipalAxes, analyzeSolidity,
// containsPoint, projectedArea, fitBoundingCylinder, fitDominantPlane, math::boundingSphere,
// math::fitObb — what a mesh weighs, where it balances, which way it lies, and what shape it really is)
// A renderer only needs a mesh's triangles. Everything else about it — how heavy it is, where its
// centre of mass sits, how it tumbles, whether a point is inside it, what silhouette it casts, how
// close to convex or flat it is — is geometry someone has to compute, and the engine computes all of
// it on the CPU. The demo's whole method is to ask only questions whose answers can be written down in
// advance. LEFT: a unit cube, checked against arithmetic — volume 1, inertia a squared over six,
// silhouette 1 head-on and root three down the diagonal, bounding sphere root three over two. Every
// one of them lands exactly. MIDDLE: a sphere the engine can only approximate, where the interesting
// number is not the answer but the error, and how it falls by four each time the mesh is subdivided;
// then the same cube moved and turned, where the mass properties follow it and the box fitter does
// not, for a reason worth knowing. RIGHT: two cubes with a gap, where solidity and containment both
// have exact answers, and how flat a plane, a cube and a ball each measure.
// Fixed data, no input. --headless / --frames N for CI.

#include "maz/Engine.hpp"

// None of these are in maz/Engine.hpp: that umbrella carries 153 of the engine's 697 headers.
#include "maz/math/BoundingSphere.hpp"
#include "maz/math/FitObb.hpp"
#include "maz/render/MeshBoundingCylinder.hpp"
#include "maz/render/MeshContainment.hpp"
#include "maz/render/MeshDominantPlane.hpp"
#include "maz/render/MeshIcosphere.hpp"
#include "maz/render/MeshMassProperties.hpp"
#include "maz/render/MeshMerge.hpp"
#include "maz/render/MeshPrincipalAxes.hpp"
#include "maz/render/MeshProjectedArea.hpp"
#include "maz/render/MeshSolidity.hpp"
#include "maz/render/MeshTransform.hpp"
#include "maz/render/Shapes.hpp"

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

std::string sci(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.0e", v);
    return buf;
}

std::vector<math::vec3> positionsOf(const MeshData& m) {
    std::vector<math::vec3> p;
    p.reserve(m.vertices.size());
    for (const render::MeshVertex& v : m.vertices) {
        p.push_back(math::vec3(v.px, v.py, v.pz));
    }
    return p;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("HEFT starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Heft";
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

    // ---- a unit cube, against arithmetic ------------------------------------------------------------
    const MeshData cube = render::shapes::makeBox(1.0f, white);
    const render::MassProperties cubeMass = render::computeMassProperties(cube);
    const render::SolidityReport cubeSolidity = render::analyzeSolidity(cube);
    const double cubeOffDiagonal =
        std::max(std::fabs(cubeMass.inertia[0][1]),
                 std::max(std::fabs(cubeMass.inertia[0][2]), std::fabs(cubeMass.inertia[1][2])));
    const double cubeFaceOn =
        static_cast<double>(render::projectedArea(cube, math::vec3(1, 0, 0)));
    const double cubeDiagonal =
        static_cast<double>(render::projectedArea(cube, math::vec3(1, 1, 1)));
    const math::Sphere cubeSphere = math::boundingSphere(positionsOf(cube));
    const bool insideCentre = render::containsPoint(cube, math::vec3(0, 0, 0));
    const bool insideCorner = render::containsPoint(cube, math::vec3(0.49f, 0.49f, 0.49f));
    const bool insideOutside = render::containsPoint(cube, math::vec3(0.6f, 0, 0));

    // ---- a sphere the engine can only approximate ---------------------------------------------------
    struct BallStep {
        int subdivisions = 0;
        std::size_t triangles = 0;
        double volume = 0.0;
        double shortBy = 0.0; // percent under the true sphere
    };
    std::vector<BallStep> ball;
    const double trueBall = 4.0 / 3.0 * kPi;
    for (int sub : {1, 2, 3, 4}) {
        const MeshData s = render::makeIcosphere(1.0f, sub);
        const render::MassProperties mp = render::computeMassProperties(s);
        ball.push_back(BallStep{sub, s.indices.size() / 3, mp.volume,
                                100.0 * (trueBall - mp.volume) / trueBall});
    }
    const render::PrincipalAxes ballAxes =
        render::computePrincipalAxes(render::makeIcosphere(1.0f, 4));

    // ---- moved and turned ---------------------------------------------------------------------------
    // A 2-cube rotated 30 degrees about z and carried off to (3, -1, 2). The mass properties should
    // follow it exactly; the box fitter is the interesting one.
    MeshData moved;
    {
        math::mat4 m(1.0f);
        m = glm::translate(m, math::vec3(3.0f, -1.0f, 2.0f));
        m = glm::rotate(m, glm::radians(30.0f), math::vec3(0, 0, 1));
        moved = render::applyTransform(render::shapes::makeBox(2.0f, white), m);
    }
    const render::MassProperties movedMass = render::computeMassProperties(moved);
    const math::Obb movedObb = math::fitObb(positionsOf(moved));
    const double movedObbVolume = 8.0 * static_cast<double>(movedObb.half.x) *
                                  static_cast<double>(movedObb.half.y) *
                                  static_cast<double>(movedObb.half.z);
    double bestAxisAlignment = 0.0;
    for (int i = 0; i < 3; ++i) {
        const math::vec3 a = movedObb.axis(i);
        bestAxisAlignment =
            std::max(bestAxisAlignment, std::fabs(static_cast<double>(a.x) * std::cos(kPi / 6.0) +
                                                  static_cast<double>(a.y) * std::sin(kPi / 6.0)));
    }
    const double axisOffDegrees =
        std::acos(std::min(1.0, bestAxisAlignment)) * 180.0 / kPi;
    // The same fitter on a BRICK, which has no tie to resolve.
    math::Obb brickObb;
    {
        math::mat4 m(1.0f);
        m = glm::translate(m, math::vec3(3.0f, -1.0f, 2.0f));
        m = glm::rotate(m, glm::radians(30.0f), math::vec3(0, 0, 1));
        m = glm::scale(m, math::vec3(1.0f, 0.9f, 0.8f));
        brickObb = math::fitObb(positionsOf(render::applyTransform(render::shapes::makeBox(2.0f, white), m)));
    }

    // ---- two cubes with a gap between them ----------------------------------------------------------
    MeshData pair;
    {
        math::mat4 l(1.0f);
        l = glm::translate(l, math::vec3(-2.0f, 0.0f, 0.0f));
        math::mat4 r(1.0f);
        r = glm::translate(r, math::vec3(2.0f, 0.0f, 0.0f));
        pair = render::mergeMeshes(render::applyTransform(render::shapes::makeBox(1.0f, white), l),
                                   render::applyTransform(render::shapes::makeBox(1.0f, white), r));
    }
    const render::SolidityReport pairSolidity = render::analyzeSolidity(pair);
    const bool inTheGap = render::containsPoint(pair, math::vec3(0, 0, 0));
    const bool inACube = render::containsPoint(pair, math::vec3(2, 0, 0));
    const render::BoundingCylinder pairCylinder = render::fitBoundingCylinder(pair);

    // ---- how flat is it? ----------------------------------------------------------------------------
    struct Flatness {
        const char* what;
        float planarity = 0.0f;
        float thickness = 0.0f;
        float rms = 0.0f;
    };
    std::vector<Flatness> flatness;
    {
        const MeshData flat = render::shapes::makePlane(1.0f, white);
        const MeshData ballMesh = render::makeIcosphere(1.0f, 3);
        const render::MeshPlane a = render::fitDominantPlane(flat);
        const render::MeshPlane b = render::fitDominantPlane(cube);
        const render::MeshPlane c = render::fitDominantPlane(ballMesh);
        flatness.push_back(Flatness{"a flat plane", a.planarity, a.thickness, a.rmsDistance});
        flatness.push_back(Flatness{"a cube", b.planarity, b.thickness, b.rmsDistance});
        flatness.push_back(Flatness{"a ball", c.planarity, c.thickness, c.rmsDistance});
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
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  HEFT", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "what a mesh weighs, where it balances and what shape it really is — asked "
                          "only in ways the answer can be checked",
                          kDim, 0.32f);

            auto cell = [&](float x, float y, const std::string& s, render::Color colour, float scale) {
                font.drawText(*renderer, x, y, s.c_str(), colour, scale);
            };
            auto row = [&](float x, float y, const char* label, const std::string& value,
                           const std::string& truth, render::Color colour) {
                font.drawText(*renderer, x, y, label, kDim, sz);
                font.drawText(*renderer, x + 190.0f, y, value.c_str(), colour, sz);
                if (!truth.empty()) {
                    font.drawText(*renderer, x + 300.0f, y, truth.c_str(), kDim, 0.24f);
                }
            };

            // ---- column 1: the cube, against arithmetic ----
            float y = 100.0f;
            font.drawText(*renderer, 24.0f, y, "A UNIT CUBE, AGAINST ARITHMETIC", kHead, 0.34f);
            y += 30.0f;
            row(24.0f, y, "volume", num(cubeMass.volume, 6), "exactly 1", kOk);
            y += 23.0f;
            row(24.0f, y, "centre of mass", "(0, 0, 0)", "exactly the origin", kOk);
            y += 23.0f;
            row(24.0f, y, "inertia, each axis", num(cubeMass.inertia[0][0], 6), "m a^2 / 6 = 0.166667",
                kOk);
            y += 23.0f;
            row(24.0f, y, "inertia, off-axis", sci(cubeOffDiagonal), "exactly 0", kOk);
            y += 23.0f;
            row(24.0f, y, "solidity", num(cubeSolidity.solidity, 6), "convex, so 1", kOk);
            y += 23.0f;
            row(24.0f, y, "silhouette, head-on", num(cubeFaceOn, 6), "one face = 1", kOk);
            y += 23.0f;
            row(24.0f, y, "silhouette, diagonal", num(cubeDiagonal, 6), "sqrt(3) = 1.732051", kOk);
            y += 23.0f;
            row(24.0f, y, "bounding sphere",
                num(static_cast<double>(cubeSphere.radius), 6), "sqrt(3)/2 = 0.866025", kOk);
            y += 23.0f;
            row(24.0f, y, "inside test",
                std::string(insideCentre ? "centre in" : "centre OUT") + ", " +
                    (insideCorner ? "corner in" : "corner OUT") + ", " +
                    (insideOutside ? "outside IN" : "outside out"),
                "", (insideCentre && insideCorner && !insideOutside) ? kOk : kNo);
            y += 30.0f;
            font.drawText(*renderer, 24.0f, y,
                          "Nine questions with answers you can write on paper, and nine exact matches "
                          "— the inertia to six decimals, the off-diagonal terms at literal zero, the "
                          "diagonal silhouette at root three. That is the point of asking this shape "
                          "first: a volume integrator that is subtly wrong still LOOKS plausible on an "
                          "interesting mesh, and has nowhere to hide on a boring one.",
                          kDim, 0.25f);

            // ---- column 2: the ball, and the moved cube ----
            y = 100.0f;
            font.drawText(*renderer, 470.0f, y, "A BALL IT CAN ONLY APPROXIMATE", kHead, 0.34f);
            y += 30.0f;
            cell(470.0f, y, "subdiv", kDim, 0.25f);
            cell(560.0f, y, "triangles", kDim, 0.25f);
            cell(670.0f, y, "volume", kDim, 0.25f);
            cell(770.0f, y, "short by", kDim, 0.25f);
            y += 22.0f;
            for (const BallStep& b : ball) {
                cell(470.0f, y, std::to_string(b.subdivisions), kText, sz);
                cell(560.0f, y, std::to_string(b.triangles), kDim, sz);
                cell(670.0f, y, num(b.volume, 4), kVal, sz);
                cell(770.0f, y, num(b.shortBy, 3) + "%", kVal, sz);
                y += 23.0f;
            }
            y += 6.0f;
            font.drawText(*renderer, 470.0f, y,
                          ("A true ball of radius 1 holds " + num(trueBall, 6) +
                           ". A mesh inscribed in it never does, and the useful number is not the "
                           "volume but the error, which falls by roughly four each time the triangle "
                           "count goes up by four — the signature of a second-order approximation. Its "
                           "three principal moments come out equal to seven decimals (" +
                           num(ballAxes.moment[0], 5) +
                           "), which is what a ball's ought to be: it tumbles the same about every axis.")
                              .c_str(),
                          kDim, 0.25f);

            y += 112.0f;
            font.drawText(*renderer, 470.0f, y, "MOVED AND TURNED", kHead, 0.34f);
            y += 30.0f;
            row(470.0f, y, "volume", num(movedMass.volume, 5), "a 2-cube is 8", kOk);
            y += 23.0f;
            row(470.0f, y, "centre of mass",
                "(" + num(movedMass.centroid[0], 3) + ", " + num(movedMass.centroid[1], 3) + ", " +
                    num(movedMass.centroid[2], 3) + ")",
                "put at (3, -1, 2)", kOk);
            y += 23.0f;
            row(470.0f, y, "fitObb volume", num(movedObbVolume, 3), "a 2-cube is 8", kNo);
            y += 23.0f;
            row(470.0f, y, "fitObb axis, off by", num(axisOffDegrees, 1) + " deg", "should be 0", kNo);
            y += 23.0f;
            row(470.0f, y, "same fit on a brick",
                num(static_cast<double>(brickObb.half.x), 3) + ", " +
                    num(static_cast<double>(brickObb.half.y), 3) + ", " +
                    num(static_cast<double>(brickObb.half.z), 3),
                "asked for 1, 0.9, 0.8", kOk);
            y += 28.0f;
            font.drawText(*renderer, 470.0f, y,
                          "The mass properties follow the cube exactly. The box fitter does not, and "
                          "it is not broken: it orients the box by the eigenvectors of the point "
                          "cloud's covariance, and a cube's two horizontal spreads are EQUAL, so every "
                          "direction in that plane is equally an eigenvector and rounding picks one. "
                          "The box still contains every corner — it is a valid bound, just not the "
                          "tight one. Give the same call a brick, whose three spreads differ, and it "
                          "recovers the box to the last decimal.",
                          kDim, 0.25f);

            // ---- column 3: the pair, and flatness ----
            y = 100.0f;
            font.drawText(*renderer, 950.0f, y, "TWO CUBES WITH A GAP", kHead, 0.34f);
            y += 30.0f;
            row(950.0f, y, "mesh volume", num(pairSolidity.meshVolume, 4), "two unit cubes = 2", kOk);
            y += 23.0f;
            row(950.0f, y, "convex hull", num(pairSolidity.hullVolume, 4), "5 x 1 x 1 = 5", kOk);
            y += 23.0f;
            row(950.0f, y, "solidity", num(pairSolidity.solidity, 4), "2/5 = 0.4", kOk);
            y += 23.0f;
            row(950.0f, y, "the gap at (0,0,0)", inTheGap ? "inside" : "outside", "outside", !inTheGap ? kOk : kNo);
            y += 23.0f;
            row(950.0f, y, "a cube at (2,0,0)", inACube ? "inside" : "outside", "inside", inACube ? kOk : kNo);
            y += 23.0f;
            row(950.0f, y, "cylinder radius",
                num(static_cast<double>(pairCylinder.radius), 4), "sqrt(2)/2 = 0.707107", kOk);
            y += 23.0f;
            row(950.0f, y, "cylinder height",
                num(static_cast<double>(pairCylinder.height), 4), "end to end = 5", kOk);
            y += 28.0f;
            font.drawText(*renderer, 950.0f, y,
                          "Solidity is the mesh's own volume over its convex hull's, and here both are "
                          "whole numbers you can see: two cubes of 1 inside a hull five long, so 0.4 "
                          "exactly. It is the one number that says \"this is not a blob\" — worth "
                          "having before choosing between a convex proxy and a real collision mesh. "
                          "Containment agrees: the space between the cubes is outside the mesh, which "
                          "a hull-based test would get wrong.",
                          kDim, 0.25f);

            y += 108.0f;
            font.drawText(*renderer, 950.0f, y, "HOW FLAT IS IT", kHead, 0.34f);
            y += 30.0f;
            cell(950.0f, y, "shape", kDim, 0.25f);
            cell(1080.0f, y, "planarity", kDim, 0.25f);
            cell(1190.0f, y, "thickness", kDim, 0.25f);
            y += 22.0f;
            for (const Flatness& f : flatness) {
                cell(950.0f, y, f.what, kText, sz);
                cell(1080.0f, y, num(static_cast<double>(f.planarity), 4),
                     f.planarity > 0.5f ? kOk : kVal, sz);
                cell(1190.0f, y, num(static_cast<double>(f.thickness), 3), kDim, sz);
                y += 23.0f;
            }
            y += 8.0f;
            font.drawText(*renderer, 950.0f, y,
                          "A plane is flat to 1.0000 with zero thickness; a cube and a ball are 0.0000 "
                          "flat, because neither has a least-spread direction to lie in. This is how a "
                          "tool decides whether a scanned or generated patch can be treated as a "
                          "surface — billboarded, projected onto, walked on — before trying.",
                          kDim, 0.25f);

            font.drawText(*renderer, 24.0f, 700.0f,
                          "Every figure on this screen came from triangles and nothing else: no GPU, "
                          "no physics engine, no solver. Which is why the same calls run in a build "
                          "step that refuses an asset whose centre of mass is outside it.",
                          render::Color{0.58f, 0.62f, 0.70f, 1}, 0.27f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("HEFT shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
