// Maz Engine — unit tests for the pure-logic modules (no GPU/window needed).
// A tiny dependency-free harness: CHECK/CHECK_NEAR record failures and the process exits non-zero
// if any check fails, so it plugs straight into ctest. Kept minimal to match the engine's no-extra-
// dependency philosophy.

#include "maz/anim/AnimClip.hpp"
#include "maz/anim/Animator.hpp"
#include "maz/anim/Skeleton.hpp"
#include "maz/anim/SpriteAnim.hpp"
#include "maz/anim/Tween.hpp"
#include "maz/core/CVars.hpp"
#include "maz/core/Events.hpp"
#include "maz/core/Jobs.hpp"
#include "maz/core/Resources.hpp"
#include "maz/core/SceneStack.hpp"
#include "maz/ecs/World.hpp"
#include "maz/fx/Particles.hpp"
#include "maz/game/BehaviorTree.hpp"
#include "maz/game/Collision.hpp"
#include "maz/game/NavGrid.hpp"
#include "maz/game/Physics2D.hpp"
#include "maz/game/Shake.hpp"
#include "maz/game/SpatialGrid.hpp"
#include "maz/game/StateMachine.hpp"
#include "maz/game/Steering.hpp"
#include "maz/io/Config.hpp"
#include "maz/io/Json.hpp"
#include "maz/io/Serialize.hpp"
#include "maz/ui/UI.hpp"
#include "maz/math/Math.hpp"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <string>
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

void testRaycast() {
    using game::Aabb;
    const Aabb box = Aabb::fromCenterSize(math::vec3(5, 0, 0), math::vec3(2, 2, 2)); // [4,6]x[-1,1]^2
    // Ray from origin along +x hits the near face at x=4 (t=4).
    game::RayHit h = game::raycastAabb(math::vec3(0, 0, 0), math::vec3(1, 0, 0), box);
    CHECK(h.hit);
    CHECK_NEAR(h.t, 4.0f, 1e-4f);
    CHECK_NEAR(h.point.x, 4.0f, 1e-4f);
    // Pointing away (-x) misses.
    CHECK(!game::raycastAabb(math::vec3(0, 0, 0), math::vec3(-1, 0, 0), box).hit);
    // Parallel and offset in y misses.
    CHECK(!game::raycastAabb(math::vec3(0, 5, 0), math::vec3(1, 0, 0), box).hit);
    // maxDist shorter than the box excludes it.
    CHECK(!game::raycastAabb(math::vec3(0, 0, 0), math::vec3(1, 0, 0), box, 3.0f).hit);
    // Ray starting inside the box hits at t=0.
    game::RayHit inside = game::raycastAabb(math::vec3(5, 0, 0), math::vec3(1, 0, 0), box);
    CHECK(inside.hit);
    CHECK_NEAR(inside.t, 0.0f, 1e-4f);

    // List raycast returns the nearest box and its index.
    std::vector<Aabb> boxes = {
        Aabb::fromCenterSize(math::vec3(20, 0, 0), math::vec3(2, 2, 2)), // far  (index 0)
        Aabb::fromCenterSize(math::vec3(8, 0, 0), math::vec3(2, 2, 2)),  // near (index 1)
    };
    game::RayHit nearest = game::raycast(math::vec3(0, 0, 0), math::vec3(1, 0, 0), boxes);
    CHECK(nearest.hit);
    CHECK(nearest.index == 1);        // the closer box
    CHECK_NEAR(nearest.t, 7.0f, 1e-4f); // near face of box at [7,9]
    CHECK(!game::raycast(math::vec3(0, 0, 0), math::vec3(0, 1, 0), boxes).hit); // up misses both
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

void testNavGrid() {
    // Open 10x10 grid: straight-line diagonal path from a corner to the opposite corner.
    game::NavGrid grid(10, 10, 1.0f);
    std::vector<game::NavGrid::Cell> path;
    CHECK(grid.findPath({0, 0}, {9, 9}, path));
    CHECK(!path.empty());
    CHECK(path.front() == (game::NavGrid::Cell{0, 0}));
    CHECK(path.back() == (game::NavGrid::Cell{9, 9}));
    // On an open grid the shortest 8-connected route is a pure diagonal: 10 cells (start..goal).
    CHECK(path.size() == 10);

    // A vertical wall spanning x=5 for z in [0..8] leaves a one-cell gap at z=9, so a path must
    // exist but be longer than the blocked straight line.
    game::NavGrid walled(12, 12, 1.0f);
    for (int z = 0; z <= 8; ++z) {
        walled.setBlocked(5, z, true);
    }
    std::vector<game::NavGrid::Cell> around;
    CHECK(walled.findPath({2, 4}, {9, 4}, around));
    CHECK(!around.empty());
    CHECK(around.back() == (game::NavGrid::Cell{9, 4}));
    for (const auto& c : around) {
        CHECK(!(c.x == 5 && c.z <= 8)); // never steps onto the wall
    }

    // Fully sealing the wall (all 12 rows at x=5) makes the goal unreachable.
    game::NavGrid sealed(12, 12, 1.0f);
    for (int z = 0; z < 12; ++z) {
        sealed.setBlocked(5, z, true);
    }
    std::vector<game::NavGrid::Cell> none;
    CHECK(!sealed.findPath({2, 4}, {9, 4}, none));
    CHECK(none.empty());

    // Blocked endpoints fail cleanly.
    game::NavGrid g2(5, 5, 1.0f);
    g2.setBlocked(4, 4, true);
    std::vector<game::NavGrid::Cell> p2;
    CHECK(!g2.findPath({0, 0}, {4, 4}, p2));

    // World<->cell round-trips: a point maps to a cell whose center is within half a cell of it.
    game::NavGrid wg(8, 8, 2.0f, math::vec3(-8.0f, 0.0f, -8.0f));
    const math::vec3 probe(1.0f, 0.0f, -3.0f);
    const game::NavGrid::Cell wc = wg.worldToCell(probe);
    const math::vec3 center = wg.cellToWorld(wc);
    CHECK(std::fabs(center.x - probe.x) <= 1.0f);
    CHECK(std::fabs(center.z - probe.z) <= 1.0f);

    // No corner-cutting: a single solid at (1,0) forbids the (0,0)->(1,1) diagonal (it would squeeze
    // past the corner), forcing a 3-cell orthogonal detour via (0,1) instead of the 2-cell diagonal.
    game::NavGrid corner(4, 4, 1.0f);
    corner.setBlocked(1, 0, true);
    std::vector<game::NavGrid::Cell> cp;
    CHECK(corner.findPath({0, 0}, {1, 1}, cp));
    CHECK(cp.size() == 3);
}

void testSteering() {
    using game::Agent;

    // limit(): long vectors are capped to the max magnitude, short ones pass through.
    CHECK_NEAR(glm::length(game::limit(math::vec3(10, 0, 0), 3.0f)), 3.0f, 1e-5f);
    CHECK_NEAR(glm::length(game::limit(math::vec3(1, 0, 0), 3.0f)), 1.0f, 1e-5f);

    // seek force points toward the target; flee points away.
    Agent a;
    a.pos = math::vec3(0, 0, 0);
    a.vel = math::vec3(0, 0, 0);
    const math::vec3 target(10, 0, 0);
    const math::vec3 s = game::seek(a, target);
    CHECK(s.x > 0.0f);
    CHECK(math::dot(s, target - a.pos) > 0.0f);
    const math::vec3 f = game::flee(a, target);
    CHECK(f.x < 0.0f);

    // Steering force never exceeds maxForce.
    CHECK(glm::length(s) <= a.maxForce + 1e-4f);

    // arrive: outside slowRadius wants full speed; well inside it wants less. Compare desired speeds
    // by looking at (force + vel) which reconstructs the desired velocity (vel is zero here).
    Agent b;
    b.pos = math::vec3(0, 0, 0);
    const float slow = 5.0f;
    const math::vec3 far = game::arrive(b, math::vec3(100, 0, 0), slow);   // desired speed = maxSpeed
    const math::vec3 near = game::arrive(b, math::vec3(1, 0, 0), slow);    // desired speed ~ maxSpeed/5
    CHECK(glm::length(far) > glm::length(near));

    // separation pushes away from a close neighbor.
    Agent c;
    c.pos = math::vec3(0, 0, 0);
    std::vector<math::vec3> neighbors = {math::vec3(0, 0, 0), math::vec3(0.5f, 0, 0)};
    const math::vec3 sep = game::separation(c, neighbors, 2.0f);
    CHECK(sep.x < 0.0f); // pushed in -x, away from the neighbor at +x

    // integrate caps speed at maxSpeed even under a huge shove.
    Agent d;
    d.maxSpeed = 5.0f;
    game::integrate(d, math::vec3(1000, 0, 0), 1.0f / 60.0f);
    CHECK(glm::length(d.vel) <= d.maxSpeed + 1e-4f);

    // Full sim: a seeking agent converges on a static target.
    Agent e;
    e.pos = math::vec3(-20, 0, 0);
    const math::vec3 goal(0, 0, 0);
    for (int i = 0; i < 600; ++i) {
        game::integrate(e, game::arrive(e, goal, 3.0f), 1.0f / 60.0f);
    }
    CHECK(glm::length(e.pos - goal) < 1.0f);

    // followPath advances its waypoint index as the agent reaches each node.
    Agent p;
    p.pos = math::vec3(0, 0, 0);
    std::vector<math::vec3> wps = {math::vec3(0, 0, 0), math::vec3(5, 0, 0), math::vec3(10, 0, 0)};
    uint32_t idx = 0;
    game::followPath(p, wps, idx, 1.0f, 2.0f); // starts on wp0 (within 1.0) -> advances to wp1
    CHECK(idx == 1);
    uint32_t idx2 = 0;
    Agent q;
    q.pos = math::vec3(100, 0, 0); // far from wp0: no advance
    game::followPath(q, wps, idx2, 1.0f, 2.0f);
    CHECK(idx2 == 0);
}

void testTween() {
    using anim::Ease;

    // Every curve pins its endpoints: ease(type,0)==0 and ease(type,1)==1 (within tolerance;
    // Back/Elastic overshoot in the middle but still land on the endpoints).
    const Ease all[] = {Ease::Linear,     Ease::QuadIn,   Ease::QuadOut,   Ease::QuadInOut,
                        Ease::CubicIn,    Ease::CubicOut, Ease::CubicInOut, Ease::SineIn,
                        Ease::SineOut,    Ease::SineInOut, Ease::ExpoOut,   Ease::CircOut,
                        Ease::BackOut,    Ease::ElasticOut, Ease::BounceOut};
    for (Ease e : all) {
        CHECK_NEAR(anim::ease(e, 0.0f), 0.0f, 1e-3f);
        CHECK_NEAR(anim::ease(e, 1.0f), 1.0f, 1e-3f);
    }

    // Known curve values.
    CHECK_NEAR(anim::ease(Ease::Linear, 0.5f), 0.5f, 1e-6f);
    CHECK_NEAR(anim::ease(Ease::QuadIn, 0.5f), 0.25f, 1e-6f);
    CHECK_NEAR(anim::ease(Ease::QuadOut, 0.5f), 0.75f, 1e-6f);
    CHECK_NEAR(anim::ease(Ease::SineInOut, 0.5f), 0.5f, 1e-6f);

    // Input clamping: t below 0 / above 1 behaves like the endpoints.
    CHECK_NEAR(anim::ease(Ease::CubicInOut, -3.0f), 0.0f, 1e-6f);
    CHECK_NEAR(anim::ease(Ease::CubicInOut, 5.0f), 1.0f, 1e-6f);

    // BackOut overshoots above 1 before settling (anticipation), proving it's not clamped mid-curve.
    bool overshot = false;
    for (float t = 0.6f; t < 0.95f; t += 0.01f) {
        if (anim::ease(Ease::BackOut, t) > 1.0f) overshot = true;
    }
    CHECK(overshot);

    // mix helpers.
    CHECK_NEAR(anim::mix(0.0f, 10.0f, 0.25f), 2.5f, 1e-6f);
    const math::vec3 vm = anim::mix(math::vec3(0, 0, 0), math::vec3(4, 8, 0), 0.5f);
    CHECK_NEAR(vm.x, 2.0f, 1e-6f);
    CHECK_NEAR(vm.y, 4.0f, 1e-6f);

    // Tween Once: reaches the end and finishes, clamped there.
    anim::Tween once;
    once.duration = 1.0f;
    once.loop = anim::Loop::Once;
    once.update(0.5f);
    CHECK_NEAR(once.progress(), 0.5f, 1e-5f);
    CHECK(!once.finished);
    once.update(1.0f); // overshoot
    CHECK(once.finished);
    CHECK_NEAR(once.progress(), 1.0f, 1e-5f);
    CHECK_NEAR(once.sample(10.0f, 20.0f), 20.0f, 1e-4f);

    // Tween Repeat: wraps back to the start.
    anim::Tween rep;
    rep.duration = 1.0f;
    rep.loop = anim::Loop::Repeat;
    rep.update(1.5f);
    CHECK(!rep.finished);
    CHECK_NEAR(rep.progress(), 0.5f, 1e-5f);

    // Tween PingPong: reverses direction after each cycle.
    anim::Tween pp;
    pp.duration = 1.0f;
    pp.loop = anim::Loop::PingPong;
    pp.update(1.5f); // one full cycle + half back
    CHECK(pp.reversing);
    CHECK_NEAR(pp.progress(), 0.5f, 1e-5f);
    pp.update(1.0f); // cross back through 0 and forward again
    CHECK(!pp.reversing);
    CHECK_NEAR(pp.progress(), 0.5f, 1e-5f);

    // Zero-duration tween is treated as instantly complete.
    anim::Tween zero;
    zero.duration = 0.0f;
    zero.update(0.016f);
    CHECK_NEAR(zero.progress(), 1.0f, 1e-6f);
}

void testUI() {
    // Rect hit-testing (half-open on the far edges).
    ui::Rect r{10, 20, 100, 40};
    CHECK(r.contains(10, 20));
    CHECK(r.contains(50, 40));
    CHECK(!r.contains(9, 20));
    CHECK(!r.contains(110, 20)); // x+w is outside
    CHECK(!r.contains(50, 60));  // y+h is outside

    // Slider value mapping + clamping.
    ui::Rect track{40, 0, 100, 20};
    CHECK_NEAR(ui::sliderValueFromX(track, 40, 0, 100), 0.0f, 1e-4f);
    CHECK_NEAR(ui::sliderValueFromX(track, 90, 0, 100), 50.0f, 1e-4f);
    CHECK_NEAR(ui::sliderValueFromX(track, 140, 0, 100), 100.0f, 1e-4f);
    CHECK_NEAR(ui::sliderValueFromX(track, -50, 0, 100), 0.0f, 1e-4f);   // clamped low
    CHECK_NEAR(ui::sliderValueFromX(track, 9999, 0, 100), 100.0f, 1e-4f); // clamped high

    // Button interaction state machine on a renderer-less context (draw calls no-op).
    ui::Context ui;
    const ui::Rect btn{40, 40, 100, 30};

    // Hover only, no press -> no click.
    ui.begin(50, 50, false);
    CHECK(!ui.button(1, btn, "x"));
    ui.end();
    // Press inside (button-down edge) -> captures active, not yet a click.
    ui.begin(50, 50, true);
    CHECK(!ui.button(1, btn, "x"));
    ui.end();
    // Release inside -> click fires.
    ui.begin(50, 50, false);
    CHECK(ui.button(1, btn, "x"));
    ui.end();

    // Press inside then release OUTSIDE -> no click (press/release must land on the same widget).
    ui.begin(50, 50, true);
    CHECK(!ui.button(1, btn, "x"));
    ui.end();
    ui.begin(500, 500, false);
    CHECK(!ui.button(1, btn, "x"));
    ui.end();

    // Toggle flips its bound value on a completed click.
    bool flag = false;
    ui.begin(50, 50, true);
    ui.toggle(2, btn, "opt", flag);
    ui.end();
    ui.begin(50, 50, false);
    const bool changed = ui.toggle(2, btn, "opt", flag);
    ui.end();
    CHECK(changed);
    CHECK(flag);

    // Slider: pressing at the track midpoint sets the value to the midpoint of the range.
    float vol = 0.0f;
    ui.begin(90, 10, true);
    const bool moved = ui.slider(3, track, vol, 0.0f, 100.0f);
    ui.end();
    CHECK(moved);
    CHECK_NEAR(vol, 50.0f, 1e-3f);
}

void testSerialize() {
    struct Pod {
        int32_t a;
        float b;
        uint8_t c;
    };

    // Round-trip mixed scalars, a POD struct, a string, and a vector through the codec.
    io::ByteWriter w;
    w.writeHeader(0x1234ABCD, 3);
    w.write<int32_t>(-42);
    w.write<float>(3.5f);
    w.write(Pod{7, 1.25f, 200});
    w.writeString("hello maz");
    w.writeVector(std::vector<uint32_t>{10, 20, 30});

    io::ByteReader r(w.data());
    CHECK(r.readHeader(0x1234ABCD, 3));
    CHECK(r.read<int32_t>() == -42);
    CHECK_NEAR(r.read<float>(), 3.5f, 1e-6f);
    const Pod p = r.read<Pod>();
    CHECK(p.a == 7);
    CHECK_NEAR(p.b, 1.25f, 1e-6f);
    CHECK(p.c == 200);
    CHECK(r.readString() == std::string("hello maz"));
    const std::vector<uint32_t> v = r.readVector<uint32_t>();
    CHECK(v.size() == 3);
    CHECK(v[0] == 10 && v[1] == 20 && v[2] == 30);
    CHECK(r.ok());
    CHECK(r.remaining() == 0);

    // Wrong magic / version is rejected cleanly.
    io::ByteReader bad(w.data());
    CHECK(!bad.readHeader(0xDEADBEEF, 3));
    CHECK(!bad.ok());
    io::ByteReader badVer(w.data());
    CHECK(!badVer.readHeader(0x1234ABCD, 99));

    // Truncated input: reading past the end sets not-ok instead of reading garbage/crashing.
    io::ByteWriter w2;
    w2.write<uint64_t>(0x1122334455667788ull);
    std::vector<uint8_t> trunc(w2.data().begin(), w2.data().begin() + 3); // only 3 of 8 bytes
    io::ByteReader tr(trunc);
    tr.read<uint64_t>();
    CHECK(!tr.ok());

    // A truncated length-prefixed string doesn't over-read.
    io::ByteWriter w3;
    w3.writeString("abcdef");
    std::vector<uint8_t> ts(w3.data().begin(), w3.data().begin() + 5); // length + 1 byte only
    io::ByteReader tsr(ts);
    tsr.readString();
    CHECK(!tsr.ok());
}

void testJson() {
    using io::JsonValue;

    // Parse a representative document exercising every type + nesting.
    const char* doc = R"({
        "name": "level-1",
        "gravity": -9.8,
        "count": 3,
        "enabled": true,
        "empty": null,
        "tags": ["start", "boss", "dark"],
        "player": { "hp": 100, "pos": [4, 5.5] }
    })";
    auto r = io::parseJson(doc);
    CHECK(r.ok);
    CHECK(r.error.empty());
    const JsonValue& j = r.value;
    CHECK(j.isObject());

    // Typed reads with chained lookups + defaults on missing keys never crash.
    CHECK(j["name"].asString() == std::string("level-1"));
    CHECK_NEAR(j["gravity"].asFloat(), -9.8f, 1e-5f);
    CHECK(j["count"].asInt() == 3);
    CHECK(j["enabled"].asBool() == true);
    CHECK(j["empty"].isNull());
    CHECK(j["missing"].asInt(42) == 42);           // absent key -> default
    CHECK(j["missing"]["deep"].asInt(7) == 7);     // chained missing -> default, no crash

    // Arrays: size, indexing, out-of-range returns null sentinel.
    CHECK(j["tags"].isArray());
    CHECK(j["tags"].size() == 3);
    CHECK(j["tags"][0].asString() == std::string("start"));
    CHECK(j["tags"][2].asString() == std::string("dark"));
    CHECK(j["tags"][9].isNull());

    // Nested object + number array.
    CHECK(j["player"]["hp"].asInt() == 100);
    CHECK_NEAR(j["player"]["pos"][1].asFloat(), 5.5f, 1e-5f);

    // Insertion order is preserved through the object.
    const auto& fields = j.fields();
    CHECK(fields.items.size() == 7);
    CHECK(fields.items[0].first == std::string("name"));
    CHECK(fields.items[1].first == std::string("gravity"));

    // Round-trip: parse -> dump -> parse yields the same values.
    std::string dumped = j.dump();
    auto r2 = io::parseJson(dumped);
    CHECK(r2.ok);
    CHECK(r2.value["player"]["hp"].asInt() == 100);
    CHECK(r2.value["tags"].size() == 3);

    // Integers dump without a spurious ".0"; the dumped form is compact.
    CHECK(JsonValue(3).dump() == std::string("3"));
    CHECK(JsonValue(-42).dump() == std::string("-42"));
    CHECK(JsonValue(true).dump() == std::string("true"));
    CHECK(JsonValue(nullptr).dump() == std::string("null"));
    CHECK(JsonValue("hi").dump() == std::string("\"hi\""));

    // String escaping round-trips control chars, quotes, and backslashes.
    JsonValue s(std::string("a\"b\\c\nd\te"));
    auto rs = io::parseJson(s.dump());
    CHECK(rs.ok);
    CHECK(rs.value.asString() == std::string("a\"b\\c\nd\te"));

    // \u escape decodes to UTF-8.
    auto ru = io::parseJson("\"\\u00e9\"");  // é
    CHECK(ru.ok);
    CHECK(ru.value.asString().size() == 2);  // two UTF-8 bytes

    // Building a document in code, then dumping + reparsing.
    JsonValue built = JsonValue::object();
    built.set("id", 7);
    built.set("names", JsonValue::array());
    built.fields()["names"].push_back(JsonValue("a"));
    built.fields()["names"].push_back(JsonValue("b"));
    auto rb = io::parseJson(built.dump());
    CHECK(rb.ok);
    CHECK(rb.value["id"].asInt() == 7);
    CHECK(rb.value["names"].size() == 2);
    CHECK(rb.value["names"][1].asString() == std::string("b"));

    // Malformed inputs fail cleanly (ok == false, non-empty error, line/col set) — never throw.
    const char* bad[] = {
        "{",                    // unterminated object
        "[1, 2,",               // unterminated array
        "{\"a\": }",            // missing value
        "{\"a\" 1}",            // missing colon
        "truue",                // bad literal
        "\"unterminated",       // unterminated string
        "[1 2]",                // missing comma
        "{a: 1}",               // unquoted key
        "",                     // empty input
        "123 456",              // trailing tokens
    };
    for (const char* b : bad) {
        auto rbad = io::parseJson(b);
        CHECK(!rbad.ok);
        CHECK(!rbad.error.empty());
        CHECK(rbad.value.isNull());
    }

    // Pretty-print produces newlines + indentation and still round-trips.
    std::string pretty = j.dump(2);
    CHECK(pretty.find('\n') != std::string::npos);
    auto rp = io::parseJson(pretty);
    CHECK(rp.ok);
    CHECK(rp.value["name"].asString() == std::string("level-1"));

    // A level document (tile rows + pickups) survives a parse -> dump -> parse round-trip intact.
    const char* levelDoc = R"({
        "name": "ARENA",
        "tileSize": 44,
        "tiles": ["111", "1 1", "111"],
        "pickups": [ {"x": 1, "y": 1} ]
    })";
    auto lr = io::parseJson(levelDoc);
    CHECK(lr.ok);
    CHECK(lr.value["tiles"].size() == 3);
    CHECK(lr.value["tiles"][0].asString() == std::string("111"));
    CHECK(lr.value["pickups"][0]["x"].asInt() == 1);
    auto lr2 = io::parseJson(lr.value.dump());
    CHECK(lr2.ok);
    CHECK(lr2.value["tileSize"].asInt() == 44);
    CHECK(lr2.value["tiles"].size() == 3);
    CHECK(lr2.value["pickups"].size() == 1);

    // File IO: write a document to a temp path, read it back, and verify equality of values.
    const std::string path = "maz_json_roundtrip_test.json";
    CHECK(io::writeJsonFile(path, lr.value, 2));
    auto fr = io::parseJsonFile(path);
    CHECK(fr.ok);
    CHECK(fr.value["name"].asString() == std::string("ARENA"));
    CHECK(fr.value["tiles"].size() == 3);
    std::remove(path.c_str());

    // A missing file fails cleanly (not-ok, non-empty error) rather than throwing.
    auto missing = io::parseJsonFile("definitely_not_a_real_file_12345.json");
    CHECK(!missing.ok);
    CHECK(!missing.error.empty());
}

void testCVars() {
    core::CVarRegistry reg;

    // Registration + typed defaults.
    reg.registerBool("debug.wireframe", false, "draw wireframe");
    reg.registerInt("scene.count", 8, "entity count");
    reg.registerFloat("render.exposure", 1.0f, "HDR exposure");
    reg.registerString("app.title", "MAZ", "window title");
    CHECK(reg.has("scene.count"));
    CHECK(!reg.has("nope"));
    CHECK(reg.entries().size() == 4);
    CHECK(reg.getBool("debug.wireframe") == false);
    CHECK(reg.getInt("scene.count") == 8);
    CHECK_NEAR(reg.getFloat("render.exposure"), 1.0f, 1e-6f);
    CHECK(reg.getString("app.title") == std::string("MAZ"));

    // Re-registering keeps the current (possibly modified) value rather than resetting.
    reg.setInt("scene.count", 15);
    reg.registerInt("scene.count", 8, "entity count");
    CHECK(reg.getInt("scene.count") == 15);

    // Range clamp on numeric setters.
    reg.setRange("render.exposure", 0.2, 3.0);
    reg.setFloat("render.exposure", 9.0f);
    CHECK_NEAR(reg.getFloat("render.exposure"), 3.0f, 1e-6f);
    reg.setFloat("render.exposure", -1.0f);
    CHECK_NEAR(reg.getFloat("render.exposure"), 0.2f, 1e-6f);
    reg.setRange("scene.count", 1, 10);
    reg.setInt("scene.count", 100);
    CHECK(reg.getInt("scene.count") == 10);

    // setFromString coercion per type (+ clamping) and failure on garbage.
    CHECK(reg.setFromString("debug.wireframe", "true"));
    CHECK(reg.getBool("debug.wireframe") == true);
    CHECK(reg.setFromString("debug.wireframe", "off"));
    CHECK(reg.getBool("debug.wireframe") == false);
    CHECK(!reg.setFromString("debug.wireframe", "maybe")); // unparsable bool
    CHECK(reg.setFromString("scene.count", "5"));
    CHECK(reg.getInt("scene.count") == 5);
    CHECK(!reg.setFromString("scene.count", "abc"));       // unparsable int
    CHECK(!reg.setFromString("unknown.name", "1"));        // unknown cvar
    CHECK(reg.setFromString("app.title", "HELLO"));
    CHECK(reg.getString("app.title") == std::string("HELLO"));

    // Command-line style assignments.
    const int applied = reg.applyAssignments({"scene.count=7", "render.exposure=1.5", "bad", "x=y"});
    CHECK(applied == 2); // two known cvars set; "bad" (no '=') and "x=y" (unknown) skipped
    CHECK(reg.getInt("scene.count") == 7);

    // --- JSON bridge (io::Config) ---------------------------------------------------------------
    // loadConfig applies matching keys, coerces types, ignores unknowns, respects clamps.
    const char* cfgDoc = R"({
        "debug.wireframe": true,
        "scene.count": 999,
        "render.exposure": 2.25,
        "app.title": "CONFIGURED",
        "unknown.key": 42
    })";
    auto cfg = io::parseJson(cfgDoc);
    CHECK(cfg.ok);
    const int n = io::loadConfig(reg, cfg.value);
    CHECK(n == 4); // four known keys applied, unknown.key ignored
    CHECK(reg.getBool("debug.wireframe") == true);
    CHECK(reg.getInt("scene.count") == 10); // clamped to [1,10]
    CHECK_NEAR(reg.getFloat("render.exposure"), 2.25f, 1e-5f);
    CHECK(reg.getString("app.title") == std::string("CONFIGURED"));

    // configToJson round-trips the whole registry back out (order preserved).
    io::JsonValue dumped = io::configToJson(reg);
    CHECK(dumped.isObject());
    CHECK(dumped.fields().items.size() == 4);
    CHECK(dumped["scene.count"].asInt() == 10);
    CHECK(dumped["app.title"].asString() == std::string("CONFIGURED"));

    // A fresh registry loading that dump reproduces the same values.
    core::CVarRegistry reg2;
    reg2.registerBool("debug.wireframe", false);
    reg2.registerInt("scene.count", 0);
    reg2.registerFloat("render.exposure", 0.0f);
    reg2.registerString("app.title", "");
    io::loadConfig(reg2, dumped);
    CHECK(reg2.getInt("scene.count") == 10);
    CHECK(reg2.getBool("debug.wireframe") == true);
    CHECK(reg2.getString("app.title") == std::string("CONFIGURED"));

    // File round-trip through the io::Config convenience helpers.
    const std::string path = "maz_cvars_roundtrip_test.json";
    CHECK(io::saveConfigFile(reg, path));
    core::CVarRegistry reg3;
    reg3.registerInt("scene.count", 0);
    reg3.registerString("app.title", "");
    const int loaded = io::loadConfigFile(reg3, path);
    CHECK(loaded == 2);
    CHECK(reg3.getInt("scene.count") == 10);
    std::remove(path.c_str());
}

void testStateMachine() {
    enum class S { Patrol, Chase, Return };

    // Track enter/exit/update side effects and drive transitions with plain flags.
    int patrolEnters = 0, chaseEnters = 0, patrolExits = 0;
    float patrolUpdateTime = 0.0f;
    bool seePlayer = false;
    bool lostPlayer = false;
    bool backHome = false;

    game::StateMachine<S> fsm;
    fsm.addState(
        S::Patrol, [&](float dt) { patrolUpdateTime += dt; }, [&]() { ++patrolEnters; },
        [&]() { ++patrolExits; });
    fsm.addState(S::Chase, {}, [&]() { ++chaseEnters; });
    fsm.addState(S::Return);
    fsm.addTransition(S::Patrol, S::Chase, [&]() { return seePlayer; });
    fsm.addTransition(S::Chase, S::Return, [&]() { return lostPlayer; });
    fsm.addTransition(S::Return, S::Patrol, [&]() { return backHome; });

    fsm.start(S::Patrol);
    CHECK(fsm.isIn(S::Patrol));
    CHECK(patrolEnters == 1);

    // No trigger: stays in Patrol, its onUpdate accumulates dt.
    fsm.update(0.5f);
    CHECK(fsm.isIn(S::Patrol));
    CHECK_NEAR(patrolUpdateTime, 0.5f, 1e-5f);
    CHECK(fsm.transitionCount() == 0);

    // Trigger Patrol -> Chase: exit Patrol, enter Chase.
    seePlayer = true;
    fsm.update(0.5f);
    CHECK(fsm.isIn(S::Chase));
    CHECK(patrolExits == 1);
    CHECK(chaseEnters == 1);
    CHECK(fsm.transitionCount() == 1);
    // Patrol's onUpdate must not run once we've left it.
    CHECK_NEAR(patrolUpdateTime, 0.5f, 1e-5f);

    // Chase -> Return -> Patrol across updates.
    lostPlayer = true;
    fsm.update(0.1f);
    CHECK(fsm.isIn(S::Return));
    backHome = true;
    fsm.update(0.1f);
    CHECK(fsm.isIn(S::Patrol));
    CHECK(patrolEnters == 2);
    CHECK(fsm.transitionCount() == 3);

    // Any-transition fires from any state and takes priority.
    enum class G { A, B, Dead };
    bool dead = false;
    game::StateMachine<G> g;
    g.addState(G::A);
    g.addState(G::B);
    g.addState(G::Dead);
    g.addTransition(G::A, G::B, [&]() { return true; }); // would fire, but...
    g.addAnyTransition(G::Dead, [&]() { return dead; });  // ...any-transition checked first
    g.start(G::A);
    dead = true;
    g.update(0.0f);
    CHECK(g.isIn(G::Dead));
}

void testSpriteAnim() {
    // gridFrames: a 4x1 strip yields 4 evenly-split columns spanning full height.
    const auto strip = anim::gridFrames(4, 1, 0, 4);
    CHECK(strip.size() == 4);
    CHECK_NEAR(strip[0].u0, 0.0f, 1e-6f);
    CHECK_NEAR(strip[0].u1, 0.25f, 1e-6f);
    CHECK_NEAR(strip[1].u0, 0.25f, 1e-6f);
    CHECK_NEAR(strip[3].u1, 1.0f, 1e-6f);
    CHECK_NEAR(strip[0].v0, 0.0f, 1e-6f);
    CHECK_NEAR(strip[0].v1, 1.0f, 1e-6f);

    // Row-major indexing on a 2x2 sheet: cell 3 is the bottom-right quadrant.
    const auto grid = anim::gridFrames(2, 2, 0, 4);
    CHECK_NEAR(grid[3].u0, 0.5f, 1e-6f);
    CHECK_NEAR(grid[3].v0, 0.5f, 1e-6f);
    CHECK_NEAR(grid[3].u1, 1.0f, 1e-6f);

    // Looping clip at 10 fps: each 0.1s advances one frame; wraps after the last.
    anim::SpriteAnim a;
    a.play(anim::gridFrames(4, 1, 0, 4), 10.0f, true);
    CHECK(a.index() == 0);
    a.update(0.1f);
    CHECK(a.index() == 1);
    a.update(0.25f); // +2.5 frames -> lands on frame 3 (1 + 2)
    CHECK(a.index() == 3);
    a.update(0.1f); // wrap back to 0
    CHECK(a.index() == 0);
    CHECK(!a.finished());
    // frame() matches the strip UV for the current index.
    a.update(0.1f);
    CHECK_NEAR(a.frame().u0, 0.25f, 1e-6f);

    // One-shot clip: clamps on the last frame and reports finished.
    anim::SpriteAnim once;
    once.play(anim::gridFrames(3, 1, 0, 3), 10.0f, false);
    once.update(1.0f); // way past the end
    CHECK(once.index() == 2);
    CHECK(once.finished());
    once.reset();
    CHECK(once.index() == 0);
    CHECK(!once.finished());

    // A single-frame clip never advances or finishes (nothing to animate).
    anim::SpriteAnim one;
    one.play(anim::gridFrames(1, 1, 0, 1), 10.0f, false);
    one.update(5.0f);
    CHECK(one.index() == 0);
    CHECK(!one.finished());
}

void testEventBus() {
    struct Damage {
        int amount;
    };
    struct Healed {
        int amount;
    };

    core::EventBus bus;

    // Multiple subscribers of one type all fire, in subscription order, with the payload.
    int total = 0;
    int calls = 0;
    int lastSeen = 0;
    bus.subscribe<Damage>([&](const Damage& d) { total += d.amount; ++calls; lastSeen = d.amount; });
    bus.subscribe<Damage>([&](const Damage& d) { total += d.amount * 10; });
    CHECK(bus.subscriberCount<Damage>() == 2);

    bus.emit(Damage{5});
    CHECK(calls == 1);
    CHECK(lastSeen == 5);
    CHECK(total == 55); // 5 + 50

    // A different event type is isolated — emitting Healed doesn't call Damage handlers.
    int healed = 0;
    bus.subscribe<Healed>([&](const Healed& h) { healed += h.amount; });
    bus.emit(Healed{7});
    CHECK(healed == 7);
    CHECK(calls == 1); // unchanged

    // Unsubscribe stops delivery to that one handler only.
    core::EventBus::Token t = bus.subscribe<Healed>([&](const Healed& h) { healed += h.amount * 100; });
    CHECK(bus.subscriberCount<Healed>() == 2);
    bus.unsubscribe(t);
    CHECK(bus.subscriberCount<Healed>() == 1);
    bus.emit(Healed{2});
    CHECK(healed == 9); // only the first handler ran (7 + 2), not the *100 one

    // Emitting a type with no subscribers is a harmless no-op.
    struct Unheard {
        int x;
    };
    bus.emit(Unheard{1});

    // Re-entrancy: a handler that unsubscribes itself mid-dispatch is safe (snapshotted list).
    core::EventBus bus2;
    int fired = 0;
    core::EventBus::Token self = 0;
    self = bus2.subscribe<Damage>([&](const Damage&) {
        ++fired;
        bus2.unsubscribe(self); // remove self during dispatch
    });
    bus2.emit(Damage{1});
    bus2.emit(Damage{1});
    CHECK(fired == 1); // only the first emit reaches it
    CHECK(bus2.subscriberCount<Damage>() == 0);

    // Stale/invalid tokens unsubscribe cleanly.
    bus2.unsubscribe(999999);
    bus2.unsubscribe(core::EventBus::kInvalidToken);
}

void testJobs() {
    core::JobSystem js;
    CHECK(js.workerCount() >= 1);

    // parallelFor writes to distinct indices -> deterministic, race-free result.
    const size_t N = 10000;
    std::vector<uint64_t> out(N, 0);
    js.parallelFor(0, N, [&](size_t i) { out[i] = static_cast<uint64_t>(i) * i; });
    bool allOk = true;
    for (size_t i = 0; i < N; ++i) {
        if (out[i] != static_cast<uint64_t>(i) * i) {
            allOk = false;
            break;
        }
    }
    CHECK(allOk);

    // Every index is visited exactly once (atomic tally).
    std::vector<std::atomic<int>> visits(N);
    for (auto& v : visits) {
        v.store(0);
    }
    js.parallelFor(0, N, [&](size_t i) { visits[i].fetch_add(1); });
    bool onceEach = true;
    for (size_t i = 0; i < N; ++i) {
        if (visits[i].load() != 1) {
            onceEach = false;
            break;
        }
    }
    CHECK(onceEach);

    // parallelRanges tiles [0,N) exactly once (chunks are contiguous and cover everything).
    std::vector<std::atomic<int>> rangeVisits(N);
    for (auto& v : rangeVisits) {
        v.store(0);
    }
    js.parallelRanges(0, N, [&](size_t a, size_t b) {
        for (size_t i = a; i < b; ++i) {
            rangeVisits[i].fetch_add(1);
        }
    });
    bool rangeOk = true;
    for (size_t i = 0; i < N; ++i) {
        if (rangeVisits[i].load() != 1) {
            rangeOk = false;
            break;
        }
    }
    CHECK(rangeOk);

    // submit() returns a future carrying the task's result.
    std::vector<std::future<int>> futs;
    for (int i = 0; i < 20; ++i) {
        futs.push_back(js.submit([i] { return i * 2; }));
    }
    int sum = 0;
    for (auto& f : futs) {
        sum += f.get();
    }
    CHECK(sum == 2 * (19 * 20 / 2)); // 2 * sum(0..19) = 380

    // Empty / inverted ranges are safe no-ops.
    int touched = 0;
    js.parallelFor(5, 5, [&](size_t) { ++touched; });
    js.parallelFor(10, 3, [&](size_t) { ++touched; });
    CHECK(touched == 0);

    // A pool with an explicit single worker still runs everything correctly.
    core::JobSystem single(1);
    CHECK(single.workerCount() == 1);
    std::atomic<int> counter{0};
    single.parallelFor(0, 500, [&](size_t) { counter.fetch_add(1); });
    CHECK(counter.load() == 500);
}

void testResourceCache() {
    core::ResourceCache<std::string, int> cache;
    int loaderCalls = 0;
    auto loadValue = [&](int v) {
        return [&, v] { ++loaderCalls; return v; };
    };

    // First acquire loads; second acquire of the same key reuses (loader not called again).
    int& a = cache.acquire("tex.png", loadValue(42));
    CHECK(a == 42);
    CHECK(loaderCalls == 1);
    CHECK(cache.loads() == 1);
    CHECK(cache.hits() == 0);
    CHECK(cache.refCount("tex.png") == 1);

    int& a2 = cache.acquire("tex.png", loadValue(999)); // loader value ignored — cached
    CHECK(a2 == 42);
    CHECK(&a == &a2);          // same instance
    CHECK(loaderCalls == 1);   // not reloaded
    CHECK(cache.hits() == 1);
    CHECK(cache.refCount("tex.png") == 2);

    // A different key is a separate load.
    cache.acquire("mesh.obj", loadValue(7));
    CHECK(cache.loads() == 2);
    CHECK(cache.size() == 2);

    // Release drops refs; the entry survives until the count hits zero, then evicts (with callback).
    int evicted = -1;
    CHECK(!cache.release("tex.png", [&](int& v) { evicted = v; })); // refs 2 -> 1, not evicted
    CHECK(cache.refCount("tex.png") == 1);
    CHECK(evicted == -1);
    CHECK(cache.release("tex.png", [&](int& v) { evicted = v; })); // refs 1 -> 0, evicted
    CHECK(evicted == 42);
    CHECK(!cache.contains("tex.png"));
    CHECK(cache.size() == 1);

    // Releasing an unknown / already-evicted key is a harmless false.
    CHECK(!cache.release("tex.png"));
    CHECK(!cache.release("does-not-exist"));

    // find() reflects presence without touching refcounts.
    CHECK(cache.find("mesh.obj") != nullptr);
    CHECK(*cache.find("mesh.obj") == 7);
    CHECK(cache.find("tex.png") == nullptr);
    CHECK(cache.refCount("mesh.obj") == 1);

    // Dedup under heavy reuse: 100 acquires across 5 keys => 5 loads, 95 hits.
    core::ResourceCache<int, int> pool;
    int builds = 0;
    for (int i = 0; i < 100; ++i) {
        pool.acquire(i % 5, [&] { ++builds; return 0; });
    }
    CHECK(builds == 5);
    CHECK(pool.loads() == 5);
    CHECK(pool.hits() == 95);
    CHECK(pool.size() == 5);
    CHECK(pool.refCount(0) == 20);

    // clear() runs the evict callback for every remaining entry.
    int clears = 0;
    pool.clear([&](int&) { ++clears; });
    CHECK(clears == 5);
    CHECK(pool.size() == 0);
}

void testSkeleton() {
    // Two joints: root at origin, child one unit up (local translate (0,1,0)).
    std::vector<anim::Joint> joints(2);
    joints[0].parent = -1;
    joints[0].localBind = math::mat4(1.0f);
    joints[1].parent = 0;
    joints[1].localBind = glm::translate(math::mat4(1.0f), math::vec3(0, 1, 0));
    anim::Skeleton skel(joints);
    CHECK(skel.jointCount() == 2);
    CHECK(skel.parent(1) == 0);

    // Global bind of the child places it at (0,1,0).
    const math::vec4 childBindPos = skel.globalBind()[1] * math::vec4(0, 0, 0, 1);
    CHECK_NEAR(childBindPos.x, 0.0f, 1e-5f);
    CHECK_NEAR(childBindPos.y, 1.0f, 1e-5f);

    // At rest (local == localBind), every skinning matrix is the identity (no deformation).
    std::vector<math::mat4> skin;
    skel.computeSkinning(skel.restLocals(), skin);
    for (size_t i = 0; i < skel.jointCount(); ++i) {
        const math::vec4 p = skin[i] * math::vec4(0.3f, 0.7f, -0.2f, 1.0f);
        CHECK_NEAR(p.x, 0.3f, 1e-5f);
        CHECK_NEAR(p.y, 0.7f, 1e-5f);
        CHECK_NEAR(p.z, -0.2f, 1e-5f);
    }

    // computeGlobals chains transforms: translate root by (5,0,0), keep child local (0,1,0) ->
    // child global maps the origin to (5,1,0).
    std::vector<math::mat4> locals = skel.restLocals();
    locals[0] = glm::translate(math::mat4(1.0f), math::vec3(5, 0, 0));
    std::vector<math::mat4> globals;
    skel.computeGlobals(locals, globals);
    const math::vec4 childPos = globals[1] * math::vec4(0, 0, 0, 1);
    CHECK_NEAR(childPos.x, 5.0f, 1e-5f);
    CHECK_NEAR(childPos.y, 1.0f, 1e-5f);

    // Rigidly rotating the root 90 deg about Z moves a root-bound vertex from (1,0,0) to (0,1,0).
    std::vector<math::mat4> rot = skel.restLocals();
    rot[0] = glm::rotate(math::mat4(1.0f), glm::radians(90.0f), math::vec3(0, 0, 1));
    std::vector<math::mat4> skin2;
    skel.computeSkinning(rot, skin2);
    const math::vec4 v = skin2[0] * math::vec4(1, 0, 0, 1);
    CHECK_NEAR(v.x, 0.0f, 1e-5f);
    CHECK_NEAR(v.y, 1.0f, 1e-5f);
    // And a child-bound vertex at the child's bind position (0,1,0) rotates about the root to (-1,0,0).
    const math::vec4 cv = skin2[1] * math::vec4(0, 1, 0, 1);
    CHECK_NEAR(cv.x, -1.0f, 1e-5f);
    CHECK_NEAR(cv.y, 0.0f, 1e-5f);
}

void testAnimClip() {
    using anim::Key;

    // vec3 track: keys at t=0 -> (0,0,0), t=1 -> (10,0,0). Linear interp + endpoint clamping.
    std::vector<Key<math::vec3>> vt = {{0.0f, math::vec3(0, 0, 0)}, {1.0f, math::vec3(10, 0, 0)}};
    CHECK_NEAR(anim::sampleVec3(vt, 0.5f, math::vec3(0)).x, 5.0f, 1e-5f);
    CHECK_NEAR(anim::sampleVec3(vt, -1.0f, math::vec3(0)).x, 0.0f, 1e-5f);  // clamp low
    CHECK_NEAR(anim::sampleVec3(vt, 9.0f, math::vec3(0)).x, 10.0f, 1e-5f);  // clamp high
    // Empty track -> fallback.
    CHECK_NEAR(anim::sampleVec3({}, 0.5f, math::vec3(3, 0, 0)).x, 3.0f, 1e-6f);

    // quat track: identity at 0 -> rotZ90 at 1. Slerp midpoint is rotZ45.
    const math::quat qid(1, 0, 0, 0);
    const math::quat q90 = glm::angleAxis(glm::radians(90.0f), math::vec3(0, 0, 1));
    std::vector<Key<math::quat>> qt = {{0.0f, qid}, {1.0f, q90}};
    {
        const math::quat mid = anim::sampleQuat(qt, 0.5f, qid);
        const math::vec3 r = mid * math::vec3(1, 0, 0); // rotate +x by 45 deg
        CHECK_NEAR(r.x, std::cos(glm::radians(45.0f)), 1e-4f);
        CHECK_NEAR(r.y, std::sin(glm::radians(45.0f)), 1e-4f);
    }
    {
        const math::quat end = anim::sampleQuat(qt, 2.0f, qid); // clamp to last
        const math::vec3 r = end * math::vec3(1, 0, 0);
        CHECK_NEAR(r.x, 0.0f, 1e-4f);
        CHECK_NEAR(r.y, 1.0f, 1e-4f);
    }

    // JointPose::matrix at identity leaves a point unchanged.
    {
        anim::JointPose jp;
        const math::vec4 p = jp.matrix() * math::vec4(2, 3, 4, 1);
        CHECK_NEAR(p.x, 2.0f, 1e-5f);
        CHECK_NEAR(p.y, 3.0f, 1e-5f);
        CHECK_NEAR(p.z, 4.0f, 1e-5f);
    }

    // Clip sampling: one joint, translation keyed; looping wraps the time.
    anim::AnimClip clip;
    clip.duration = 2.0f;
    clip.loop = true;
    clip.tracks.resize(1);
    clip.tracks[0].translation = {{0.0f, math::vec3(0, 0, 0)}, {2.0f, math::vec3(0, 8, 0)}};
    std::vector<anim::JointPose> rest(1);
    std::vector<anim::JointPose> out;
    clip.sample(1.0f, rest, out);
    CHECK_NEAR(out[0].translation.y, 4.0f, 1e-5f);
    clip.sample(2.5f, rest, out); // wraps to t=0.5 -> y=2
    CHECK_NEAR(out[0].translation.y, 2.0f, 1e-5f);

    // A joint with no keys falls back to the provided rest pose.
    rest[0].translation = math::vec3(1, 2, 3);
    anim::AnimClip empty;
    empty.duration = 1.0f;
    empty.tracks.resize(1);
    empty.sample(0.5f, rest, out);
    CHECK_NEAR(out[0].translation.x, 1.0f, 1e-6f);
    CHECK_NEAR(out[0].translation.z, 3.0f, 1e-6f);

    // blendPoses: lerp translation, slerp rotation.
    std::vector<anim::JointPose> a(1), b(1);
    a[0].translation = math::vec3(0, 0, 0);
    b[0].translation = math::vec3(10, 0, 0);
    a[0].rotation = qid;
    b[0].rotation = q90;
    std::vector<anim::JointPose> blended;
    anim::blendPoses(a, b, 0.25f, blended);
    CHECK_NEAR(blended[0].translation.x, 2.5f, 1e-5f);
    {
        const math::vec3 r = blended[0].rotation * math::vec3(1, 0, 0); // 22.5 deg
        CHECK_NEAR(r.x, std::cos(glm::radians(22.5f)), 1e-4f);
        CHECK_NEAR(r.y, std::sin(glm::radians(22.5f)), 1e-4f);
    }
    // Blend weight clamps.
    anim::blendPoses(a, b, 2.0f, blended);
    CHECK_NEAR(blended[0].translation.x, 10.0f, 1e-5f);
}

void testPhysics2D() {
    using game::Body2D;

    // Head-on elastic collision of equal-mass circles: momentum is conserved and they separate.
    game::PhysicsWorld2D w;
    Body2D a;
    a.pos = math::vec2(0, 0);
    a.vel = math::vec2(2, 0);
    a.radius = 0.5f;
    a.restitution = 1.0f;
    Body2D b;
    b.pos = math::vec2(0.9f, 0); // overlapping (dist 0.9 < r 1.0)
    b.vel = math::vec2(-2, 0);
    b.radius = 0.5f;
    b.restitution = 1.0f;
    w.add(a);
    w.add(b);
    w.step(1.0f / 60.0f);
    CHECK(w.bodies[0].vel.x < 0.0f); // 'a' bounced back left
    CHECK(w.bodies[1].vel.x > 0.0f); // 'b' bounced back right
    // Equal mass => sum of velocities (proportional to momentum) is conserved at ~0.
    CHECK_NEAR(w.bodies[0].vel.x + w.bodies[1].vel.x, 0.0f, 1e-3f);

    // A static body (invMass 0) is unmoved by an impact.
    Body2D dyn;
    dyn.pos = math::vec2(0, 0);
    dyn.vel = math::vec2(5, 0);
    dyn.radius = 0.5f;
    Body2D wall;
    wall.pos = math::vec2(0.9f, 0);
    wall.vel = math::vec2(0, 0);
    wall.radius = 0.5f;
    wall.invMass = 0.0f;
    game::collideCircles(dyn, wall);
    CHECK_NEAR(wall.pos.x, 0.9f, 1e-6f); // static didn't move
    CHECK_NEAR(wall.vel.x, 0.0f, 1e-6f);
    CHECK(dyn.vel.x < 0.0f); // dynamic bounced off

    // Positional correction pushes overlapping bodies apart (distance increases toward r).
    game::PhysicsWorld2D w2;
    Body2D c1;
    c1.pos = math::vec2(0, 0);
    c1.radius = 0.5f;
    Body2D c2;
    c2.pos = math::vec2(0.5f, 0); // heavy overlap (dist 0.5 < r 1.0), both at rest
    c2.radius = 0.5f;
    w2.add(c1);
    w2.add(c2);
    const float before = glm::length(w2.bodies[1].pos - w2.bodies[0].pos);
    for (int i = 0; i < 20; ++i) {
        w2.step(1.0f / 60.0f);
    }
    const float after = glm::length(w2.bodies[1].pos - w2.bodies[0].pos);
    CHECK(after > before);
    CHECK(after <= 1.0f + 1e-3f); // never over-separates past the contact distance

    // Wall bounce reflects velocity by restitution.
    Body2D ball;
    ball.pos = math::vec2(5, 9.9f);
    ball.vel = math::vec2(0, 5); // moving down (+y)
    ball.radius = 0.5f;
    ball.restitution = 0.5f;
    game::Bounds2D bnd{0, 0, 10, 10};
    game::collideBounds(ball, bnd);
    CHECK(ball.vel.y < 0.0f);                   // now moving up
    CHECK_NEAR(ball.vel.y, -2.5f, 1e-4f);        // 5 * 0.5
    CHECK_NEAR(ball.pos.y, 9.5f, 1e-4f);         // clamped to floor - radius

    // Gravity + inelastic floor: a ball settles and never sinks through the floor.
    game::PhysicsWorld2D w3;
    w3.gravity = math::vec2(0, 30.0f);
    w3.bounds = game::Bounds2D{0, 0, 10, 10};
    w3.hasBounds = true;
    Body2D drop;
    drop.pos = math::vec2(5, 2);
    drop.radius = 0.5f;
    drop.restitution = 0.0f;
    w3.add(drop);
    for (int i = 0; i < 300; ++i) {
        w3.step(1.0f / 60.0f);
        CHECK(w3.bodies[0].pos.y + w3.bodies[0].radius <= 10.0f + 1e-3f); // never below the floor
    }
    CHECK(w3.bodies[0].pos.y + w3.bodies[0].radius > 10.0f - 0.05f); // came to rest ON the floor

    // --- Box shapes -------------------------------------------------------------------------------
    // Box-box overlap resolves along the axis of least penetration (here, X).
    Body2D bx1;
    bx1.shape = Body2D::Box;
    bx1.half = math::vec2(1.0f, 1.0f);
    bx1.pos = math::vec2(0, 0);
    Body2D bx2;
    bx2.shape = Body2D::Box;
    bx2.half = math::vec2(1.0f, 1.0f);
    bx2.pos = math::vec2(1.6f, 0.1f); // overlap 0.4 in x, 1.9 in y -> separates on x
    game::PhysicsWorld2D wb;
    wb.add(bx1);
    wb.add(bx2);
    const float sepBefore = wb.bodies[1].pos.x - wb.bodies[0].pos.x;
    for (int i = 0; i < 20; ++i) {
        wb.step(1.0f / 60.0f);
    }
    const float sepAfter = wb.bodies[1].pos.x - wb.bodies[0].pos.x;
    CHECK(sepAfter > sepBefore);                 // pushed apart
    CHECK(sepAfter <= 2.0f + 1e-2f);             // to (but not past) contact distance in x
    CHECK_NEAR(wb.bodies[0].pos.y, 0.0f, 1e-2f); // barely moved in y (least-penetration axis was x)

    // A dynamic box settles ON a static box platform (soft-constraint solver: assert final rest,
    // which proves it neither sank through nor bounced off). Platform top is at y = 9.5.
    game::PhysicsWorld2D wp;
    wp.gravity = math::vec2(0, 30.0f);
    Body2D ground;
    ground.shape = Body2D::Box;
    ground.half = math::vec2(6.0f, 0.5f);
    ground.pos = math::vec2(0, 10.0f);
    ground.invMass = 0.0f; // static
    Body2D crate;
    crate.shape = Body2D::Box;
    crate.half = math::vec2(0.5f, 0.5f);
    crate.pos = math::vec2(0, 8.0f); // gentle drop
    crate.restitution = 0.0f;
    wp.add(ground);
    wp.add(crate);
    for (int i = 0; i < 400; ++i) {
        wp.step(1.0f / 60.0f);
    }
    const float crateTop = wp.bodies[1].pos.y + wp.bodies[1].half.y;
    CHECK(crateTop > 9.3f && crateTop < 9.7f);   // resting on the platform top (~9.5)
    CHECK_NEAR(wp.bodies[0].pos.y, 10.0f, 1e-6f); // static platform never moved

    // Circle vs box: a ball falls onto a static box and rests on top.
    game::PhysicsWorld2D wcb;
    wcb.gravity = math::vec2(0, 30.0f);
    Body2D plat;
    plat.shape = Body2D::Box;
    plat.half = math::vec2(4.0f, 0.5f);
    plat.pos = math::vec2(0, 10.0f);
    plat.invMass = 0.0f;
    Body2D ball2;
    ball2.shape = Body2D::Circle;
    ball2.radius = 0.5f;
    ball2.pos = math::vec2(0, 8.0f); // gentle drop
    ball2.restitution = 0.0f;
    wcb.add(plat);
    wcb.add(ball2);
    for (int i = 0; i < 400; ++i) {
        wcb.step(1.0f / 60.0f);
    }
    const float ballTop = wcb.bodies[1].pos.y + wcb.bodies[1].radius;
    CHECK(ballTop > 9.3f && ballTop < 9.7f); // ball rests on the box top (~9.5)

    // Friction slows a box sliding along a static floor (vs frictionless, which keeps its speed).
    auto slideVx = [](float mu) {
        game::PhysicsWorld2D pw;
        pw.gravity = math::vec2(0, 30.0f);
        Body2D fl;
        fl.shape = Body2D::Box;
        fl.half = math::vec2(20.0f, 0.5f);
        fl.pos = math::vec2(0, 10.0f);
        fl.invMass = 0.0f;
        fl.friction = mu;
        Body2D box;
        box.shape = Body2D::Box;
        box.half = math::vec2(0.5f, 0.5f);
        box.pos = math::vec2(0, 9.0f);
        box.vel = math::vec2(6.0f, 0.0f); // sliding right
        box.restitution = 0.0f;
        box.friction = mu;
        pw.add(fl);
        pw.add(box);
        for (int i = 0; i < 120; ++i) {
            pw.step(1.0f / 60.0f);
        }
        return pw.bodies[1].vel.x;
    };
    const float vxFric = slideVx(0.8f);
    const float vxNone = slideVx(0.0f);
    CHECK(vxFric < vxNone); // friction removed horizontal speed
    CHECK(vxFric < 6.0f);   // and slowed it below the launch speed
    CHECK(vxNone > 5.9f);   // frictionless keeps ~all of it
}

void testAnimator() {
    // Two single-key (constant) clips with distinct joint translations.
    auto makeConst = [](math::vec3 t) {
        anim::AnimClip c;
        c.duration = 1.0f;
        c.loop = true;
        c.tracks.resize(1);
        c.tracks[0].translation = {{0.0f, t}};
        return c;
    };
    anim::Animator anim;
    anim.setRestPose(std::vector<anim::JointPose>(1));
    const int a = anim.addClip("A", makeConst(math::vec3(10, 0, 0)));
    const int b = anim.addClip("B", makeConst(math::vec3(0, 10, 0)));
    CHECK(anim.clipCount() == 2);
    CHECK(anim.findClip("B") == b);
    CHECK(anim.findClip("nope") == -1);

    // With nothing playing, the pose is the rest pose.
    anim.update(0.1f);
    CHECK_NEAR(anim.pose()[0].translation.x, 0.0f, 1e-6f);

    // First clip snaps in (no cross-fade): pose == A immediately.
    anim.play(a, 0.5f);
    CHECK(anim.currentClip() == a);
    CHECK(!anim.isFading());
    CHECK_NEAR(anim.pose()[0].translation.x, 10.0f, 1e-5f);
    CHECK_NEAR(anim.pose()[0].translation.y, 0.0f, 1e-5f);

    // Cross-fade A -> B over 1 second. At the start the pose is still A; halfway it's the 50/50
    // blend; at the end it's fully B.
    anim.play(b, 1.0f);
    CHECK(anim.isFading());
    CHECK_NEAR(anim.pose()[0].translation.x, 10.0f, 1e-4f); // fadeT 0 -> still A
    CHECK_NEAR(anim.pose()[0].translation.y, 0.0f, 1e-4f);
    anim.update(0.5f); // halfway
    CHECK_NEAR(anim.pose()[0].translation.x, 5.0f, 1e-3f);
    CHECK_NEAR(anim.pose()[0].translation.y, 5.0f, 1e-3f);
    anim.update(0.5f); // fade complete
    CHECK(!anim.isFading());
    CHECK_NEAR(anim.pose()[0].translation.x, 0.0f, 1e-4f);
    CHECK_NEAR(anim.pose()[0].translation.y, 10.0f, 1e-4f);
    CHECK_NEAR(anim.fadeProgress(), 1.0f, 1e-5f);

    // Re-playing the current clip is a no-op (doesn't restart a fade).
    anim.play(b, 1.0f);
    CHECK(!anim.isFading());
}

void testBehaviorTree() {
    using namespace maz::game::bt;
    auto ok = [] { return Status::Success; };
    auto no = [] { return Status::Failure; };
    auto run = [] { return Status::Running; };

    // Sequence: AND semantics, short-circuits at the first non-Success.
    CHECK(sequence(action(ok), action(ok))->tick() == Status::Success);
    CHECK(sequence(action(ok), action(no))->tick() == Status::Failure);
    CHECK(sequence(action(ok), action(run))->tick() == Status::Running);

    // Selector: OR / priority fallback, short-circuits at the first non-Failure.
    CHECK(selector(action(no), action(ok))->tick() == Status::Success);
    CHECK(selector(action(no), action(no))->tick() == Status::Failure);
    CHECK(selector(action(run), action(ok))->tick() == Status::Running);

    // Inverter flips success/failure, passes running.
    CHECK(inverter(action(ok))->tick() == Status::Failure);
    CHECK(inverter(action(no))->tick() == Status::Success);
    CHECK(inverter(action(run))->tick() == Status::Running);

    // Condition maps a predicate to success/failure.
    CHECK(condition([] { return true; })->tick() == Status::Success);
    CHECK(condition([] { return false; })->tick() == Status::Failure);

    // Short-circuit: a Sequence that fails early doesn't tick later children.
    int firstTicks = 0, secondTicks = 0;
    auto seq = sequence(action([&] { ++firstTicks; return Status::Failure; }),
                        action([&] { ++secondTicks; return Status::Success; }));
    seq->tick();
    CHECK(firstTicks == 1);
    CHECK(secondTicks == 0); // never reached

    // Short-circuit: a Selector that succeeds early doesn't tick later children.
    int aTicks = 0, bTicks = 0;
    auto sel = selector(action([&] { ++aTicks; return Status::Success; }),
                        action([&] { ++bTicks; return Status::Success; }));
    sel->tick();
    CHECK(aTicks == 1);
    CHECK(bTicks == 0);

    // Reactive priority: a gated high-priority branch pre-empts the fallback the instant its
    // condition flips, because the memoryless selector re-evaluates from the top every tick.
    bool alarm = false;
    const char* acted = "";
    BehaviorTree tree(selector(
        sequence(condition([&] { return alarm; }), action([&] { acted = "flee"; return Status::Running; })),
        action([&] { acted = "patrol"; return Status::Running; })));
    tree.tick();
    CHECK(std::string(acted) == "patrol"); // alarm off -> fallback
    alarm = true;
    tree.tick();
    CHECK(std::string(acted) == "flee"); // alarm on -> high-priority branch pre-empts
    alarm = false;
    tree.tick();
    CHECK(std::string(acted) == "patrol"); // and back again
}

// A scene that logs its lifecycle + update calls into a shared vector for assertions.
struct LogScene : core::Scene {
    std::vector<std::string>* log = nullptr;
    std::string name;
    bool blockU = true;
    bool blockR = true;
    core::SceneStack* stack = nullptr; // optional: for self-mutation tests
    int pushCountdown = -1;            // if >=0, push a child scene when it hits 0 during update

    void onEnter() override { log->push_back(name + ":enter"); }
    void onExit() override { log->push_back(name + ":exit"); }
    void onPause() override { log->push_back(name + ":pause"); }
    void onResume() override { log->push_back(name + ":resume"); }
    void update(float) override {
        log->push_back(name + ":update");
        if (pushCountdown == 0 && stack) {
            auto child = std::make_unique<LogScene>();
            child->log = log;
            child->name = name + "-child";
            stack->push(std::move(child));
        }
        if (pushCountdown >= 0) {
            --pushCountdown;
        }
    }
    bool blocksUpdate() const override { return blockU; }
    bool blocksRender() const override { return blockR; }
};

void testSceneStack() {
    auto make = [](std::vector<std::string>& log, const char* n) {
        auto s = std::make_unique<LogScene>();
        s->log = &log;
        s->name = n;
        return s;
    };

    // push / pause / resume / pop lifecycle.
    {
        std::vector<std::string> log;
        core::SceneStack stack;
        stack.push(make(log, "A"));
        CHECK(stack.size() == 1);
        CHECK(log.size() == 1 && log[0] == "A:enter");
        stack.push(make(log, "B")); // A pauses, B enters
        CHECK(stack.size() == 2);
        CHECK(log[1] == "A:pause");
        CHECK(log[2] == "B:enter");
        CHECK(stack.top() != nullptr);
        stack.pop(); // B exits, A resumes
        CHECK(stack.size() == 1);
        CHECK(log[3] == "B:exit");
        CHECK(log[4] == "A:resume");
    }

    // replace: old exits, new enters, no pause/resume.
    {
        std::vector<std::string> log;
        core::SceneStack stack;
        stack.push(make(log, "A"));
        stack.replace(make(log, "C"));
        CHECK(stack.size() == 1);
        CHECK(log[1] == "A:exit");
        CHECK(log[2] == "C:enter");
    }

    // update propagation: a non-blocking overlay lets the scene below update too; a blocking one
    // stops it.
    {
        std::vector<std::string> log;
        core::SceneStack stack;
        stack.push(make(log, "A"));
        auto overlay = make(log, "B");
        overlay->blockU = false; // transparent, non-modal
        stack.push(std::move(overlay));
        log.clear();
        stack.update(0.016f);
        // Top (B) updates first, then A (because B doesn't block).
        CHECK(log.size() == 2);
        CHECK(log[0] == "B:update");
        CHECK(log[1] == "A:update");
    }
    {
        std::vector<std::string> log;
        core::SceneStack stack;
        stack.push(make(log, "A"));
        stack.push(make(log, "B")); // B blocks by default
        log.clear();
        stack.update(0.016f);
        CHECK(log.size() == 1); // only B updates
        CHECK(log[0] == "B:update");
    }

    // Deferred mutation: a scene pushing another during update applies AFTER the pass (no
    // invalidation), so the child's enter happens once and the parent finished its update.
    {
        std::vector<std::string> log;
        core::SceneStack stack;
        auto a = make(log, "A");
        a->stack = &stack;
        a->pushCountdown = 0; // push a child on the next update
        stack.push(std::move(a));
        log.clear();
        stack.update(0.016f);
        CHECK(stack.size() == 2);
        // A updated, then the deferred child push applied (A pauses, child enters).
        CHECK(log[0] == "A:update");
        CHECK(log[1] == "A:pause");
        CHECK(log[2] == "A-child:enter");
    }

    // clear() exits every scene, top-down.
    {
        std::vector<std::string> log;
        core::SceneStack stack;
        stack.push(make(log, "A"));
        stack.push(make(log, "B"));
        log.clear();
        stack.clear();
        CHECK(stack.empty());
        CHECK(log.size() == 2);
        CHECK(log[0] == "B:exit");
        CHECK(log[1] == "A:exit");
    }
}

} // namespace

int main() {
    std::printf("maz unit tests\n");
    testMath();
    testCollision();
    testRaycast();
    testSpatialGrid();
    testNavGrid();
    testPhysics2D();
    testBehaviorTree();
    testSteering();
    testStateMachine();
    testSpriteAnim();
    testSkeleton();
    testAnimClip();
    testAnimator();
    testEventBus();
    testJobs();
    testResourceCache();
    testSceneStack();
    testTween();
    testUI();
    testSerialize();
    testJson();
    testCVars();
    testEcs();
    testShake();
    testParticleAttractor();
    std::printf("%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
