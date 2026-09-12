// Maz Engine — "MESH DOCTOR" (render::analyzeMesh, summarizeTopology, analyzeWatertight,
// analyzeDegenerate, analyzeValence, analyzeWinding, connectedComponentLabels /
// splitConnectedComponents, analyzeTriangleQuality, weldVertices — the whole mesh-health panel a DCC
// tool or a 3D-print slicer shows before it lets you do anything)
// One sphere, back from an exporter in the state meshes usually arrive in, run past nine diagnostics.
// LEFT: as imported. Almost every check reports the mesh is fine — manifold, consistently wound, genus
// 0 — and it is unusable, because no two of its triangles share a vertex, so there is nothing for those
// checks to disagree about. That is the lesson of the left column: a connectivity report on an unwelded
// mesh is a report about nothing. MIDDLE: weld it, and the real damage surfaces — two holes, six open
// edges, one triangle wound backwards — then makeWindingConsistent fixes the one it can, and the
// diagnosis names the two rims it cannot. RIGHT: splitConnectedComponents pulls a detached part off a
// merged export, and the triangle-shape scores put the icosphere beside the UV sphere, which is where
// the pole slivers everyone warns about turn into numbers. Fixed data, no input.
// --headless / --frames N for CI.

#include "maz/Engine.hpp"

// None of these are in maz/Engine.hpp: that umbrella carries 153 of the engine's 692 headers.
#include "maz/render/MeshComponents.hpp"
#include "maz/render/MeshDegenerate.hpp"
#include "maz/render/MeshIcosphere.hpp"
#include "maz/render/MeshStats.hpp"
#include "maz/render/MeshTopologySummary.hpp"
#include "maz/render/MeshValence.hpp"
#include "maz/render/MeshWatertight.hpp"
#include "maz/render/MeshWeld.hpp"
#include "maz/render/MeshWinding.hpp"
#include "maz/render/Shapes.hpp"
#include "maz/render/TriangleQuality.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;
using maz::render::shapes::MeshData;

namespace {

std::string num(double v, int decimals = 2) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

std::string sci(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.1e", v);
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

// Rebuild a drawable mesh from welded positions. The sphere's normals are its positions, which is why
// the icosphere needs no normal pass at all.
MeshData fromWeld(const render::WeldedMesh& w) {
    MeshData m;
    m.vertices.reserve(w.positions.size());
    for (const math::vec3& p : w.positions) {
        m.vertices.push_back(render::MeshVertex{p.x, p.y, p.z, p.x, p.y, p.z, 1.0f, 1.0f, 1.0f, 0.0f,
                                                0.0f});
    }
    m.indices = w.indices;
    return m;
}

// The patient: what a sphere looks like after an exporter wrote every triangle's corners separately,
// dropped two faces, and reversed one. All three are ordinary import damage; the first is the one that
// hides the other two.
MeshData badImport(const MeshData& src, std::size_t dropA, std::size_t dropB, std::size_t flip) {
    MeshData out;
    const std::size_t triN = src.indices.size() / 3;
    for (std::size_t t = 0; t < triN; ++t) {
        if (t == dropA || t == dropB) {
            continue;
        }
        std::uint32_t a = src.indices[t * 3 + 0];
        std::uint32_t b = src.indices[t * 3 + 1];
        std::uint32_t c = src.indices[t * 3 + 2];
        if (t == flip) {
            const std::uint32_t tmp = b;
            b = c;
            c = tmp;
        }
        const auto base = static_cast<std::uint32_t>(out.vertices.size());
        out.vertices.push_back(src.vertices[a]);
        out.vertices.push_back(src.vertices[b]);
        out.vertices.push_back(src.vertices[c]);
        out.indices.push_back(base);
        out.indices.push_back(base + 1);
        out.indices.push_back(base + 2);
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("MESH DOCTOR starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Mesh Doctor";
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

    // ---- the patient ---------------------------------------------------------------------------------
    // Triangles 40 and 41 of this icosphere share exactly one vertex and no edge, so dropping both opens
    // two separate holes that meet at a single corner — the shape a hole report is most likely to get
    // wrong, and the reason this pair was chosen rather than two neighbours.
    const MeshData clean = render::makeIcosphere(1.0f, 2);
    const MeshData imported = badImport(clean, 40, 41, 100);

    const render::WeldedMesh weld = render::weldVertices(positionsOf(imported), imported.indices, 1e-5f);
    const MeshData welded = fromWeld(weld);
    const MeshData repaired = render::makeWindingConsistent(welded);

    // ---- the panel, run three times ------------------------------------------------------------------
    const render::MeshStats impStats = render::analyzeMesh(imported);
    const render::TopologySummary impTopo = render::summarizeTopology(imported);
    const render::WindingReport impWind = render::analyzeWinding(imported);
    const render::ValenceReport impVal = render::analyzeValence(imported);

    const render::MeshStats weldStats = render::analyzeMesh(welded);
    const render::TopologySummary weldTopo = render::summarizeTopology(welded);
    const render::WatertightReport weldWater = render::analyzeWatertight(welded);
    const render::WindingReport weldWind = render::analyzeWinding(welded);
    const render::ValenceReport weldVal = render::analyzeValence(welded);

    const render::WindingReport fixedWind = render::analyzeWinding(repaired);

    // ---- a merged export, pulled back apart ----------------------------------------------------------
    MeshData merged = clean;
    {
        const MeshData small = render::makeIcosphere(0.3f, 1);
        const auto base = static_cast<std::uint32_t>(merged.vertices.size());
        for (render::MeshVertex v : small.vertices) {
            v.px += 3.0f;
            merged.vertices.push_back(v);
        }
        for (std::uint32_t i : small.indices) {
            merged.indices.push_back(base + i);
        }
    }
    std::uint32_t mergedIslands = 0;
    render::connectedComponentLabels(merged, mergedIslands);
    const std::vector<MeshData> parts = render::splitConnectedComponents(merged);
    std::vector<render::MeshStats> partStats;
    partStats.reserve(parts.size());
    for (const MeshData& p : parts) {
        partStats.push_back(render::analyzeMesh(p));
    }

    // ---- triangle shape: the two ways to build a sphere -----------------------------------------------
    // Matched roughly on triangle count (320 against 400) so the comparison is about SHAPE, not density.
    const MeshData uv = render::shapes::makeSphere(1.0f, 10, 20, render::Color{1, 1, 1, 1});
    const render::TriangleQualityStats icoQ = render::analyzeTriangleQuality(clean);
    const render::TriangleQualityStats uvQ = render::analyzeTriangleQuality(uv);
    const render::DegenerateReport icoD = render::analyzeDegenerate(clean);
    const render::DegenerateReport uvD = render::analyzeDegenerate(uv);

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

            const float sz = 0.29f;
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  MESH DOCTOR", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "one sphere, nine diagnostics, and the reason you weld before you believe any "
                          "of them",
                          kDim, 0.34f);

            auto row = [&](float x, float y, const char* label, const std::string& value,
                           render::Color colour) {
                font.drawText(*renderer, x, y, label, kDim, sz);
                font.drawText(*renderer, x + 225.0f, y, value.c_str(), colour, sz);
            };

            // ---- column 1: as imported ----
            float y = 106.0f;
            font.drawText(*renderer, 24.0f, y, "AS IMPORTED", kHead, 0.35f);
            y += 34.0f;
            row(24.0f, y, "vertices", std::to_string(impStats.vertexCount), kNo);
            y += 25.0f;
            row(24.0f, y, "triangles", std::to_string(impStats.triangleCount), kText);
            y += 25.0f;
            row(24.0f, y, "islands", std::to_string(impTopo.componentCount), kNo);
            y += 25.0f;
            row(24.0f, y, "surface area", num(impStats.surfaceArea, 3), kVal);
            y += 32.0f;

            font.drawText(*renderer, 24.0f, y, "WHAT THE CHECKS SAY", kHead, 0.35f);
            y += 34.0f;
            row(24.0f, y, "manifold", impTopo.manifold ? "yes" : "no", impTopo.manifold ? kOk : kNo);
            y += 25.0f;
            row(24.0f, y, "winding", impWind.consistent ? "consistent" : "disagrees",
                impWind.consistent ? kOk : kNo);
            y += 25.0f;
            row(24.0f, y, "non-manifold edges", std::to_string(impTopo.nonManifoldEdgeCount), kOk);
            y += 25.0f;
            row(24.0f, y, "mean valence", num(static_cast<double>(impVal.meanValence), 2), kNo);
            y += 30.0f;
            font.drawText(*renderer, 24.0f, y,
                          ("Three of those four are green and the mesh is rubble. Every check that "
                           "compares a triangle with its NEIGHBOURS passes here because it has none: " +
                           std::to_string(impStats.vertexCount) + " vertices hold " +
                           std::to_string(weldStats.vertexCount) +
                           " distinct positions, so this is " + std::to_string(impTopo.componentCount) +
                           " loose triangles that happen to touch. Valence 2 at every vertex is the "
                           "tell — a corner of a closed surface should have five or six.")
                              .c_str(),
                          kDim, 0.26f);

            // ---- column 2: welded, then rewound ----
            y = 106.0f;
            font.drawText(*renderer, 500.0f, y, "AFTER WELD", kHead, 0.35f);
            y += 34.0f;
            row(500.0f, y, "positions kept", std::to_string(weld.positions.size()), kOk);
            y += 25.0f;
            row(500.0f, y, "collapsed triangles", std::to_string(weld.removedTriangles), kOk);
            y += 25.0f;
            row(500.0f, y, "islands", std::to_string(weldTopo.componentCount), kOk);
            y += 25.0f;
            row(500.0f, y, "mean valence", num(static_cast<double>(weldVal.meanValence), 2), kOk);
            y += 32.0f;

            font.drawText(*renderer, 500.0f, y, "THE DAMAGE, NOW VISIBLE", kHead, 0.35f);
            y += 34.0f;
            row(500.0f, y, "watertight", weldWater.watertight ? "yes" : "no",
                weldWater.watertight ? kOk : kNo);
            y += 25.0f;
            row(500.0f, y, "holes", std::to_string(weldWater.holeCount), kNo);
            y += 25.0f;
            row(500.0f, y, "open edges", std::to_string(weldWater.boundaryEdgeCount), kNo);
            y += 25.0f;
            row(500.0f, y, "largest rim", num(weldWater.largestHolePerimeter, 3), kVal);
            y += 25.0f;
            row(500.0f, y, "wound backwards", std::to_string(weldWind.flippedCount) + " triangle", kNo);
            y += 25.0f;
            row(500.0f, y, "disagreeing edges", std::to_string(weldWind.inconsistentEdges), kNo);
            y += 32.0f;

            font.drawText(*renderer, 500.0f, y, "AFTER makeWindingConsistent", kHead, 0.35f);
            y += 34.0f;
            row(500.0f, y, "winding", fixedWind.consistent ? "consistent" : "disagrees",
                fixedWind.consistent ? kOk : kNo);
            y += 25.0f;
            row(500.0f, y, "still to patch", std::to_string(weldWater.holeCount) + " holes", kNo);
            y += 30.0f;
            font.drawText(*renderer, 500.0f, y,
                          "Welding moved no vertex — the surface area is unchanged to four decimals. "
                          "All it did was let a shared corner be one vertex, and with that the two "
                          "missing faces and the reversed one are findable. The two rims meet at a "
                          "single corner, the shape a hole walk keyed by vertex reports as one hole.",
                          kDim, 0.26f);

            // ---- column 3: loose parts, and triangle shape ----
            y = 106.0f;
            font.drawText(*renderer, 990.0f, y, "A MERGED EXPORT, SPLIT", kHead, 0.35f);
            y += 34.0f;
            row(990.0f, y, "islands found", std::to_string(mergedIslands), kVal);
            y += 25.0f;
            for (std::size_t i = 0; i < partStats.size(); ++i) {
                const render::MeshStats& s = partStats[i];
                row(990.0f, y, i == 0 ? "part 1" : "part 2",
                    std::to_string(s.triangleCount) + " tris, area " + num(s.surfaceArea, 2), kText);
                y += 25.0f;
                font.drawText(*renderer, 1010.0f, y,
                              ("centred at " + num(static_cast<double>(s.boundsCenter().x), 1) + ", " +
                               num(static_cast<double>(s.boundsCenter().y), 1) + ", " +
                               num(static_cast<double>(s.boundsCenter().z), 1))
                                  .c_str(),
                              kDim, 0.26f);
                y += 24.0f;
            }
            y += 12.0f;

            font.drawText(*renderer, 990.0f, y, "TRIANGLE SHAPE", kHead, 0.35f);
            y += 34.0f;
            font.drawText(*renderer, 990.0f, y, "icosphere / UV sphere, matched on count", kDim, 0.26f);
            y += 28.0f;
            row(990.0f, y, "triangles",
                std::to_string(icoQ.triangleCount) + "  /  " + std::to_string(uvQ.triangleCount), kText);
            y += 25.0f;
            row(990.0f, y, "worst quality",
                num(static_cast<double>(icoQ.minQuality), 3) + "  /  " + sci(uvQ.minQuality), kVal);
            y += 25.0f;
            row(990.0f, y, "average quality",
                num(static_cast<double>(icoQ.avgQuality), 3) + "  /  " +
                    num(static_cast<double>(uvQ.avgQuality), 3),
                kVal);
            y += 25.0f;
            row(990.0f, y, "smallest angle",
                num(static_cast<double>(icoQ.minAngleOverall), 1) + "  /  " +
                    num(static_cast<double>(uvQ.minAngleOverall), 1),
                kVal);
            y += 25.0f;
            row(990.0f, y, "zero-area",
                std::to_string(icoD.zeroArea.size()) + "  /  " + std::to_string(uvD.zeroArea.size()),
                uvD.zeroArea.empty() ? kOk : kNo);
            y += 25.0f;
            row(990.0f, y, "caps / needles",
                std::to_string(icoD.caps.size() + icoD.needles.size()) + "  /  " +
                    std::to_string(uvD.caps.size() + uvD.needles.size()),
                uvD.caps.empty() ? kOk : kNo);
            y += 30.0f;
            font.drawText(*renderer, 990.0f, y,
                          ("Both poles of the UV sphere collapse, and they do it differently. The north "
                           "pole's " +
                           std::to_string(uvD.zeroArea.size()) +
                           " triangles are exactly zero-area, because sin(0) is 0. The south pole's " +
                           std::to_string(uvD.caps.size() + uvD.needles.size()) +
                           " are not: float sin(pi) is -8.7e-08, so that pole is a ring 1e-07 wide and "
                           "its triangles measure " +
                           sci(static_cast<double>(uvQ.minQuality)) +
                           " on the shape score while passing any zero-area test. The classifier names "
                           "them caps and needles; the score sees them as slivers. Neither check alone "
                           "finds both poles.")
                              .c_str(),
                          kDim, 0.26f);

            font.drawText(*renderer, 24.0f, 690.0f,
                          "Nothing here needs a GPU: every number on this screen is computed from "
                          "positions and indices on the CPU, which is why the same nine calls run in a "
                          "unit test, in an importer, and in a build step that refuses a mesh no "
                          "printer could seal.",
                          render::Color{0.58f, 0.62f, 0.70f, 1}, 0.28f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("MESH DOCTOR shutting down (renderer %s)",
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
