// Maz Engine — unit tests for the pure-logic modules (no GPU/window needed).
// A tiny dependency-free harness: CHECK/CHECK_NEAR record failures and the process exits non-zero
// if any check fails, so it plugs straight into ctest. Kept minimal to match the engine's no-extra-
// dependency philosophy.

#include "maz/ecs/World.hpp"
#include "maz/fx/Particles.hpp"
#include "maz/game/Collision.hpp"
#include "maz/game/Shake.hpp"
#include "maz/game/SpatialGrid.hpp"
#include "maz/math/Math.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int g_checks = 0;
int g_failures = 0;

void reportFail(const char* expr, const char* file, int line) {
    ++g_failures;
    std::printf("  FAIL: %s  (%s:%d)\n", expr, file, line);
}

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        ++g_checks;                                                                                \
        if (!(cond)) reportFail(#cond, __FILE__, __LINE__);                                        \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                                       \
    do {                                                                                           \
        ++g_checks;                                                                                \
        if (std::fabs((a) - (b)) > (eps)) reportFail(#a " ~= " #b, __FILE__, __LINE__);            \
    } while (0)

using namespace maz;

void testMath() {
    // Vulkan-correct perspective flips clip-space Y (proj[1][1] < 0).
    const math::mat4 proj = math::perspective(glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
    CHECK(proj[1][1] < 0.0f);

    // ortho2D: origin top-left, y down. (0,0) -> NDC top (-1), (0,h) -> NDC bottom (+1).
    const float w = 800.0f, h = 600.0f;
    const math::mat4 o = math::ortho2D(w, h);
    const math::vec4 topLeft = o * math::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    const math::vec4 botLeft = o * math::vec4(0.0f, h, 0.0f, 1.0f);
    CHECK_NEAR(topLeft.x, -1.0f, 1e-4f);
    CHECK_NEAR(topLeft.y, -1.0f, 1e-4f);
    CHECK_NEAR(botLeft.y, 1.0f, 1e-4f);

    // Basic vector identities.
    CHECK_NEAR(math::dot(math::vec3(1, 0, 0), math::vec3(0, 1, 0)), 0.0f, 1e-6f);
    const math::vec3 c = math::cross(math::vec3(1, 0, 0), math::vec3(0, 1, 0));
    CHECK_NEAR(c.z, 1.0f, 1e-6f);
    CHECK_NEAR(glm::length(math::normalize(math::vec3(3, 4, 0))), 1.0f, 1e-6f);
}

void testCollision() {
    using game::Aabb;
    const Aabb a = Aabb::fromCenterSize(math::vec3(0, 0, 0), math::vec3(2, 2, 2)); // [-1,1]^3
    CHECK_NEAR(a.min.x, -1.0f, 1e-6f);
    CHECK_NEAR(a.max.z, 1.0f, 1e-6f);

    const Aabb overlapping = Aabb::fromCenterSize(math::vec3(1.5f, 0, 0), math::vec3(2, 2, 2));
    const Aabb apart = Aabb::fromCenterSize(math::vec3(5, 0, 0), math::vec3(2, 2, 2));
    CHECK(a.overlaps(overlapping));
    CHECK(!a.overlaps(apart));

    // slideMove: a wall at x in [4,6]; moving +x with half-extent 1 into it should stop at x = 3.
    // (Resolution is discrete — it corrects overlap at the destination — so deltas are per-frame
    // sized; a delta that overshoots the wall entirely would tunnel, which real callers avoid.)
    std::vector<Aabb> solids = {Aabb::fromCenterSize(math::vec3(5, 0, 0), math::vec3(2, 100, 100))};
    const math::vec3 half(1, 1, 1);
    math::vec3 p = game::slideMove(math::vec3(0, 0, 0), math::vec3(5, 0, 0), half, solids);
    CHECK_NEAR(p.x, 3.0f, 1e-4f);
    // Sliding: moving diagonally into the wall should still advance in z (slide along it).
    math::vec3 q = game::slideMove(math::vec3(0, 0, 0), math::vec3(5, 0, 4), half, solids);
    CHECK_NEAR(q.x, 3.0f, 1e-4f);
    CHECK_NEAR(q.z, 4.0f, 1e-4f);
}

void testSpatialGrid() {
    using game::Aabb;
    std::vector<Aabb> solids;
    for (int i = 0; i < 10; ++i) {
        solids.push_back(
            Aabb::fromCenterSize(math::vec3(static_cast<float>(i) * 10.0f, 0, 0), math::vec3(2, 2, 2)));
    }
    game::SpatialGrid grid;
    grid.build(solids, 6.0f);
    CHECK(grid.cellCount() > 0);

    // A query box near solid #0 should gather it but not far ones.
    std::vector<Aabb> got;
    grid.gather(Aabb::fromCenterSize(math::vec3(0, 0, 0), math::vec3(2, 2, 2)), got);
    CHECK(!got.empty());
    bool sawNear = false, sawFar = false;
    for (const Aabb& s : got) {
        if (std::fabs(s.min.x - (-1.0f)) < 0.5f) sawNear = true;
        if (s.min.x > 80.0f) sawFar = true;
    }
    CHECK(sawNear);
    CHECK(!sawFar);

    // Grid-based slideMove must match the vector version against the same solids.
    const math::vec3 half(1, 1, 1);
    const math::vec3 start(-20, 0, 0), delta(30, 0, 0);
    math::vec3 viaVec = game::slideMove(start, delta, half, solids);
    math::vec3 viaGrid = game::slideMove(start, delta, half, grid);
    CHECK_NEAR(viaVec.x, viaGrid.x, 1e-4f);

    int occupied = 0;
    grid.forEachOccupiedCell([&](float, float, float, float) { ++occupied; });
    CHECK(occupied == static_cast<int>(grid.cellCount()));
}

struct Pos {
    float x, y;
};
struct Vel {
    float vx, vy;
};

void testEcs() {
    ecs::World w;
    const ecs::Entity a = w.create();
    const ecs::Entity b = w.create();
    CHECK(w.valid(a));
    CHECK(w.valid(b));
    CHECK(a != b);
    CHECK(w.size() == 2);

    w.add<Pos>(a, {1.0f, 2.0f});
    w.add<Vel>(a, {10.0f, 0.0f});
    w.add<Pos>(b, {5.0f, 5.0f});
    CHECK(w.has<Pos>(a));
    CHECK(w.has<Vel>(a));
    CHECK(!w.has<Vel>(b));
    CHECK_NEAR(w.get<Pos>(a)->x, 1.0f, 1e-6f);

    // each<Pos> visits both entities; view<Pos,Vel> only the one with both.
    int posCount = 0, bothCount = 0;
    w.each<Pos>([&](ecs::Entity, Pos&) { ++posCount; });
    w.view<Pos, Vel>([&](ecs::Entity, Pos& p, Vel& v) {
        p.x += v.vx; // integrate one step
        ++bothCount;
    });
    CHECK(posCount == 2);
    CHECK(bothCount == 1);
    CHECK_NEAR(w.get<Pos>(a)->x, 11.0f, 1e-6f);

    // destroy removes components and frees the id for reuse.
    w.destroy(a);
    CHECK(!w.valid(a));
    CHECK(!w.has<Pos>(a));
    CHECK(w.size() == 1);
    const ecs::Entity c = w.create();
    CHECK(c == a); // freed id reused
}

void testShake() {
    game::Shake s;
    CHECK_NEAR(s.trauma(), 0.0f, 1e-6f);
    CHECK_NEAR(glm::length(s.offset(1.0f)), 0.0f, 1e-6f); // no trauma => no shake
    s.addTrauma(0.5f);
    CHECK_NEAR(s.trauma(), 0.5f, 1e-6f);
    CHECK(glm::length(s.offset(1.0f)) > 0.0f); // now it shakes
    s.addTrauma(10.0f);
    CHECK_NEAR(s.trauma(), 1.0f, 1e-6f); // clamped to 1
    s.update(2.0f);
    CHECK_NEAR(s.trauma(), 0.0f, 1e-6f); // decayed to zero (large dt)
}

void testParticleAttractor() {
    fx::ParticleSystem ps(64);
    fx::BurstDesc d;
    d.count = 20;
    d.x = 0.0f;
    d.y = 0.0f;
    d.speedMin = d.speedMax = 100.0f; // all fly outward at fixed speed
    d.lifeMin = d.lifeMax = 5.0f;
    d.drag = 0.0f;
    ps.emit(d);
    ps.update(1.0f / 60.0f); // alive() is refreshed by update(), not emit()
    CHECK(ps.alive() == 20);
    // With a strong attractor at origin the particles are pulled back — after many steps they are
    // still alive (life 5s) and nothing crashes; the attractor force integrates cleanly.
    ps.setAttractor(0.0f, 0.0f, 4000.0f, 0.0f);
    for (int i = 0; i < 60; ++i) {
        ps.update(1.0f / 60.0f);
    }
    CHECK(ps.alive() == 20); // still alive (life 5s), attractor applied without crashing
    ps.clearAttractor();
    ps.clear();
    CHECK(ps.alive() == 0);
}

} // namespace

int main() {
    std::printf("maz unit tests\n");
    testMath();
    testCollision();
    testSpatialGrid();
    testEcs();
    testShake();
    testParticleAttractor();
    std::printf("%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
