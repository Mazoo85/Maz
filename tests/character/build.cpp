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

    std::printf("\n%s: %d failure(s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES", g_failures);
    return g_failures == 0 ? 0 : 1;
}
