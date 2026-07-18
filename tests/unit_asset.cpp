// Unit tests for the composite asset: buildModel baking + .mazasset save/load round-trip. No GPU.

#include "maz/assets/CompositeAsset.hpp"

#include <cmath>
#include <cstdio>

using namespace maz;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

bool nearly(float a, float b) { return std::fabs(a - b) < 1e-4f; }

assets::Part makeBoxPart(const char* name, float y) {
    assets::Part p;
    p.name = name;
    p.kind = assets::PrimitiveKind::Box;
    p.params.size[0] = p.params.size[1] = p.params.size[2] = 1.0f;
    p.local.position = {0.0f, y, 0.0f};
    return p;
}

} // namespace

int main() {
    // buildModel merges parts: vertex count is the sum of the parts' primitives (two boxes = 48).
    {
        assets::AssetDoc doc;
        doc.parts.push_back(makeBoxPart("A", 0.0f));
        doc.parts.push_back(makeBoxPart("B", 5.0f));
        const assets::Model model = assets::buildModel(doc);
        check(model.vertexCount() == 48, "buildModel sums part vertices (2 boxes -> 48)");
        check(model.meshes.size() == 1, "buildModel produces a single merged mesh");
        // A part translated to y=+5 pushes the top of the bounds to 5.5 (box half-extent 0.5).
        check(nearly(model.bounds.max[1], 5.5f), "translated part extends the bounds");
        check(nearly(model.bounds.min[1], -0.5f), "untranslated part sets the lower bound");
    }

    // Round-trip: name (quoted/spaced), kind, part count, per-part kind/params/transform.
    {
        const char* tmp = "unit_asset_tmp.mazasset";
        assets::AssetDoc doc;
        doc.name = "Hero \"Proto\" Unit";
        doc.kind = assets::AssetKind::Character;

        assets::Part torso = makeBoxPart("Torso", 1.0f);
        torso.params.size[0] = 0.5f;
        torso.params.size[1] = 0.8f;
        torso.params.size[2] = 0.3f;
        torso.local.rotationEuler = {0.0f, 30.0f, 0.0f};
        torso.local.scale = {1.25f, 1.25f, 1.25f};

        assets::Part head;
        head.name = "Head";
        head.kind = assets::PrimitiveKind::Sphere;
        head.params.radius = 0.3f;
        head.params.segments = 20;
        head.params.rings = 10;
        head.local.position = {0.0f, 1.7f, 0.0f};

        doc.parts = {torso, head};

        std::string err;
        check(assets::saveAsset(tmp, doc, &err), "save asset");

        assets::AssetDoc r;
        check(assets::loadAsset(tmp, r, &err), err.empty() ? "load asset" : err.c_str());
        check(r.name == doc.name, "asset name round-trips (quotes + spaces)");
        check(r.kind == assets::AssetKind::Character, "asset kind round-trips");
        check(r.parts.size() == 2, "part count round-trips");
        if (r.parts.size() == 2) {
            check(r.parts[0].kind == assets::PrimitiveKind::Box, "part 0 is a box");
            check(nearly(r.parts[0].params.size[1], 0.8f), "part 0 size round-trips");
            check(nearly(r.parts[0].local.rotationEuler.y, 30.0f), "part 0 rotation round-trips");
            check(nearly(r.parts[0].local.scale.x, 1.25f), "part 0 scale round-trips");
            check(r.parts[1].kind == assets::PrimitiveKind::Sphere, "part 1 is a sphere");
            check(nearly(r.parts[1].params.radius, 0.3f), "part 1 radius round-trips");
            check(r.parts[1].params.segments == 20, "part 1 segments round-trip");
            check(r.parts[1].params.rings == 10, "part 1 rings round-trip");
            check(nearly(r.parts[1].local.position.y, 1.7f), "part 1 position round-trips");
        }
    }

    // Unknown lines are tolerated (forward-compat); a wrong header is rejected.
    {
        const char* tol = "unit_asset_tolerant.mazasset";
        std::FILE* f = std::fopen(tol, "w");
        if (f) {
            std::fputs("maz-asset 1\nname \"X\"\nfuture_key 1 2 3\npart\n  prim box\n", f);
            std::fclose(f);
        }
        assets::AssetDoc r;
        std::string err;
        check(assets::loadAsset(tol, r, &err), "tolerates unrecognized lines");
        check(r.parts.size() == 1, "part still parsed past an unknown line");

        const char* bad = "unit_asset_bad.mazasset";
        std::FILE* g = std::fopen(bad, "w");
        if (g) {
            std::fputs("this is not an asset\n", g);
            std::fclose(g);
        }
        assets::AssetDoc rb;
        std::string e2;
        check(!assets::loadAsset(bad, rb, &e2), "rejects a non-asset file");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
