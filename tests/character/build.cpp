// Unit test for the SENTINEL procedural character (apps/character/character.hpp) — pure CPU, no GPU.
// Verifies the code-authored humanoid produces valid, non-empty, roughly symmetric geometry with the
// expected footing/height, so a regression in the mesh-primitive or merge/transform ops is caught.

#include "character.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace maz::render;

namespace {
int g_failures = 0;
void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) ++g_failures;
}

struct Bounds {
    float lo[3] = {1e9f, 1e9f, 1e9f};
    float hi[3] = {-1e9f, -1e9f, -1e9f};
    bool finite = true;
};

Bounds boundsOf(const shapes::MeshData& g) {
    Bounds b;
    for (const auto& v : g.vertices) {
        const float p[3] = {v.px, v.py, v.pz};
        for (int i = 0; i < 3; ++i) {
            if (!std::isfinite(p[i])) b.finite = false;
            b.lo[i] = std::min(b.lo[i], p[i]);
            b.hi[i] = std::max(b.hi[i], p[i]);
        }
    }
    return b;
}
} // namespace

int main() {
    const character::CharacterModel m = character::buildCharacter();

    // Each material group has geometry, and every index references a real vertex.
    auto valid = [](const shapes::MeshData& g, const char* name) {
        bool ok = !g.vertices.empty() && !g.indices.empty() && g.indices.size() % 3 == 0;
        for (uint32_t i : g.indices) ok = ok && (i < g.vertices.size());
        std::printf("[%s] %s group: %zu verts / %zu tris\n", ok ? "PASS" : "FAIL", name,
                    g.vertices.size(), g.indices.size() / 3);
        return ok;
    };
    check(valid(m.body, "body"), "body group valid");
    check(valid(m.dark, "dark"), "dark group valid");
    check(valid(m.glow, "glow"), "glow group valid");

    const Bounds bb = boundsOf(m.body);
    const Bounds db = boundsOf(m.dark);
    const Bounds gb = boundsOf(m.glow);
    check(bb.finite && db.finite && gb.finite, "no NaN/inf vertices");

    // Overall figure bounds (union of the three groups).
    float lo[3], hi[3];
    for (int i = 0; i < 3; ++i) {
        lo[i] = std::min({bb.lo[i], db.lo[i], gb.lo[i]});
        hi[i] = std::max({bb.hi[i], db.hi[i], gb.hi[i]});
    }
    // Feet sit on the ground plane (min y ~ 0), and the figure is roughly human-proportioned.
    check(lo[1] > -0.05f && lo[1] < 0.05f, "feet rest on y=0 ground");
    check(hi[1] > 1.9f && hi[1] < 2.4f, "overall height ~2m");
    // Left/right symmetry about x=0 (the mirrored limbs).
    check(std::fabs(lo[0] + hi[0]) < 0.02f, "figure is x-symmetric");
    // The character faces +Z, so it is deeper toward +Z than -Z (visor/feet reach forward).
    check(hi[2] > 0.1f, "features reach forward (+Z)");

    // Total triangle budget is sane (a code-authored character, not a scanned mesh).
    const size_t tris = (m.body.indices.size() + m.dark.indices.size() + m.glow.indices.size()) / 3;
    check(tris > 2000 && tris < 40000, "triangle count in expected range");

    // Rig/animation: every animation frame and theme must yield the SAME vertex/index counts as the
    // rest pose (so the viewer can stream frames into one dynamic mesh via updateMesh), and each frame
    // must be valid (indices in range, finite positions).
    auto counts = [](const character::CharacterModel& c) {
        return std::vector<std::size_t>{c.body.vertices.size(), c.body.indices.size(),
                                        c.dark.vertices.size(), c.dark.indices.size(),
                                        c.glow.vertices.size(), c.glow.indices.size()};
    };
    const std::vector<std::size_t> ref = counts(m);
    bool invariant = true, framesValid = true;
    const character::Anim anims[] = {character::Anim::Idle, character::Anim::Wave,
                                     character::Anim::PowerUp, character::Anim::APose,
                                     character::Anim::Action};
    for (character::Anim a : anims) {
        for (float t = 0.0f; t < 6.3f; t += 0.7f) {
            for (int th = 0; th < character::kThemeCount; ++th) {
                const character::CharacterModel f =
                    character::buildCharacter(character::animate(a, t), static_cast<character::Theme>(th));
                invariant = invariant && (counts(f) == ref);
                for (const shapes::MeshData* g : {&f.body, &f.dark, &f.glow}) {
                    for (uint32_t i : g->indices) framesValid = framesValid && (i < g->vertices.size());
                    framesValid = framesValid && boundsOf(*g).finite;
                }
            }
        }
    }
    check(invariant, "vertex/index counts invariant across all anims/times/themes");
    check(framesValid, "every animated frame is valid (indices in range, finite)");

    std::printf("\n%s: %d failure(s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES", g_failures);
    return g_failures == 0 ? 0 : 1;
}
