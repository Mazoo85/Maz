// Maz Engine — unit tests for the pure-logic modules (no GPU/window needed).
// A tiny dependency-free harness: CHECK/CHECK_NEAR record failures and the process exits non-zero
// if any check fails, so it plugs straight into ctest. Kept minimal to match the engine's no-extra-
// dependency philosophy.

#include "maz/anim/AdditiveBlend.hpp"
#include "maz/anim/AnimClip.hpp"
#include "maz/anim/AnimStateMachine.hpp"
#include "maz/anim/Animator.hpp"
#include "maz/anim/BlendSpace.hpp"
#include "maz/anim/BlendTree.hpp"
#include "maz/anim/IK.hpp"
#include "maz/audio/Dsp.hpp"
#include "maz/audio/Envelope.hpp"
#include "maz/audio/Spatial2D.hpp"
#include "maz/audio/Spatial3D.hpp"
#include "maz/audio/Wav.hpp"
#include "maz/anim/Skeleton.hpp"
#include "maz/anim/SpriteAnim.hpp"
#include "maz/anim/Timeline.hpp"
#include "maz/anim/TriggerTrack.hpp"
#include "maz/anim/Tween.hpp"
#include "maz/anim/TweenPlayer.hpp"
#include "maz/core/CVars.hpp"
#include "maz/core/Events.hpp"
#include "maz/core/Jobs.hpp"
#include "maz/core/Noise.hpp"
#include "maz/core/Profiler.hpp"
#include "maz/core/Random.hpp"
#include "maz/core/Resources.hpp"
#include "maz/core/Scheduler.hpp"
#include "maz/core/SceneStack.hpp"
#include "maz/core/Signal.hpp"
#include "maz/core/StringId.hpp"
#include "maz/ecs/World.hpp"
#include "maz/fx/ParticleEmitter.hpp"
#include "maz/fx/Particles.hpp"
#include "maz/game/Area2D.hpp"
#include "maz/game/AutoTile.hpp"
#include "maz/game/Avoidance.hpp"
#include "maz/game/BehaviorTree.hpp"
#include "maz/game/CameraController2D.hpp"
#include "maz/game/Collision.hpp"
#include "maz/game/CollisionLayers.hpp"
#include "maz/game/FlowField.hpp"
#include "maz/game/Goap.hpp"
#include "maz/game/NavGrid.hpp"
#include "maz/game/NavMesh.hpp"
#include "maz/game/NormalLight2D.hpp"
#include "maz/game/Parallax.hpp"
#include "maz/game/Physics2D.hpp"
#include "maz/game/PhysicsQuery2D.hpp"
#include "maz/game/Shake.hpp"
#include "maz/game/SoftShadow2D.hpp"
#include "maz/game/SpatialGrid.hpp"
#include "maz/game/StateMachine.hpp"
#include "maz/game/Steering.hpp"
#include "maz/game/TileSet.hpp"
#include "maz/game/Visibility2D.hpp"
#include "maz/input/ActionMap.hpp"
#include "maz/io/Config.hpp"
#include "maz/io/Json.hpp"
#include "maz/io/Localization.hpp"
#include "maz/io/SceneSerializer.hpp"
#include "maz/io/Serialize.hpp"
#include "maz/ui/Container.hpp"
#include "maz/ui/Layout.hpp"
#include "maz/ui/StyleBox.hpp"
#include "maz/ui/TextInput.hpp"
#include "maz/ui/TextLayout.hpp"
#include "maz/ui/Theme.hpp"
#include "maz/ui/Tree.hpp"
#include "maz/ui/UI.hpp"
#include "maz/io/PrefabText.hpp"
#include "maz/math/Math.hpp"
#include "maz/render/Grid3D.hpp"
#include "maz/render/Line2D.hpp"
#include "maz/render/Shapes3D.hpp"
#include "maz/scene/Prefab.hpp"
#include "maz/scene/TransformGraph.hpp"

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

void testNavMesh() {
    using math::vec2;
    auto square = [](float x0, float y0, float x1, float y1) {
        return std::vector<vec2>{{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}}; // CCW
    };

    // Point location + same-cell path.
    {
        game::NavMesh nm;
        nm.addPolygon(square(0, 0, 10, 10));
        nm.build();
        CHECK(nm.cellAt(vec2{5, 5}) == 0);
        CHECK(nm.cellAt(vec2{50, 50}) == game::NavMesh::kNone);
        auto p = nm.findPath(vec2{2, 2}, vec2{8, 8});
        CHECK(p.size() == 2); // start + goal, no bends inside one convex cell
        CHECK_NEAR(p.front().x, 2.0f, 1e-3f);
        CHECK_NEAR(p.back().y, 8.0f, 1e-3f);
    }

    // Straight corridor: two side-by-side cells; a horizontal path needs no corner.
    {
        game::NavMesh nm;
        nm.addPolygon(square(0, 0, 10, 10));
        nm.addPolygon(square(10, 0, 20, 10)); // shares edge x=10
        nm.build();
        auto p = nm.findPath(vec2{2, 5}, vec2{18, 5});
        CHECK(p.size() == 2); // straight across the portal, no bend
        CHECK_NEAR(p.back().x, 18.0f, 1e-3f);
    }

    // L-shaped corridor: the path must hug the reflex corner at (10,10).
    {
        game::NavMesh nm;
        nm.addPolygon(square(0, 0, 10, 10));    // cell 0 (bottom-left)
        nm.addPolygon(square(0, 10, 10, 20));   // cell 1 (top-left)
        nm.addPolygon(square(10, 10, 20, 20));  // cell 2 (top-right)
        nm.build();
        auto p = nm.findPath(vec2{5, 2}, vec2{18, 15});
        CHECK(p.size() == 3); // start, corner, goal
        CHECK_NEAR(p.front().x, 5.0f, 1e-3f);
        CHECK_NEAR(p.front().y, 2.0f, 1e-3f);
        CHECK_NEAR(p[1].x, 10.0f, 1e-3f); // hugs the inner corner
        CHECK_NEAR(p[1].y, 10.0f, 1e-3f);
        CHECK_NEAR(p.back().x, 18.0f, 1e-3f);
        CHECK_NEAR(p.back().y, 15.0f, 1e-3f);
    }

    // Disconnected cells: no path.
    {
        game::NavMesh nm;
        nm.addPolygon(square(0, 0, 10, 10));
        nm.addPolygon(square(100, 100, 110, 110)); // isolated
        nm.build();
        auto p = nm.findPath(vec2{5, 5}, vec2{105, 105});
        CHECK(p.empty());
    }

    // A point outside the mesh yields no path.
    {
        game::NavMesh nm;
        nm.addPolygon(square(0, 0, 10, 10));
        nm.build();
        CHECK(nm.findPath(vec2{5, 5}, vec2{50, 50}).empty());
    }
}

void testVisibility2D() {
    using math::vec2;
    const vec2 bmin{0, 0}, bmax{100, 100};

    // raySegment: horizontal ray from the origin hits a vertical wall at x=5.
    {
        const float t = game::Visibility2D::raySegment(vec2{0, 0}, vec2{1, 0}, vec2{5, -5}, vec2{5, 5});
        CHECK_NEAR(t, 5.0f, 1e-3f);
        // A ray pointing away from the wall misses.
        const float miss = game::Visibility2D::raySegment(vec2{0, 0}, vec2{-1, 0}, vec2{5, -5}, vec2{5, 5});
        CHECK(miss < 0.0f);
    }

    // Empty room: the light sees the whole box, so any interior point is inside the polygon.
    {
        auto poly = game::Visibility2D::compute(vec2{50, 50}, {}, bmin, bmax);
        CHECK(poly.size() >= 4);
        CHECK(game::Visibility2D::contains(poly, vec2{10, 10}));
        CHECK(game::Visibility2D::contains(poly, vec2{90, 90}));
        CHECK(game::Visibility2D::contains(poly, vec2{50, 5}));
        // A point outside the room is not lit.
        CHECK(!game::Visibility2D::contains(poly, vec2{150, 50}));
    }

    // A wall casts a shadow: light on the left, a vertical occluder in the middle.
    {
        std::vector<game::Segment2> occ{game::Segment2{vec2{50, 40}, vec2{50, 60}}};
        auto poly = game::Visibility2D::compute(vec2{10, 50}, occ, bmin, bmax);
        // In front of the wall (between light and wall) is lit.
        CHECK(game::Visibility2D::contains(poly, vec2{30, 50}));
        // Directly behind the wall is in shadow.
        CHECK(!game::Visibility2D::contains(poly, vec2{90, 50}));
        // Above the wall's span, the light still reaches the far corner.
        CHECK(game::Visibility2D::contains(poly, vec2{90, 90}));
    }
}

void testSoftShadow2D() {
    using math::vec2;

    // segmentsIntersect: a proper crossing is detected; a non-crossing pair is not.
    CHECK(game::segmentsIntersect(vec2{-1, 0}, vec2{1, 0}, vec2{0, -1}, vec2{0, 1}));
    CHECK(!game::segmentsIntersect(vec2{-1, 0}, vec2{1, 0}, vec2{-1, 1}, vec2{1, 1}));
    // Shared-endpoint / graze does not count as a blocking crossing.
    CHECK(!game::segmentsIntersect(vec2{0, 0}, vec2{1, 0}, vec2{1, 0}, vec2{1, 1}));

    // diskSamples: count + all within radius + deterministic + degenerate cases.
    {
        auto pts = game::diskSamples(vec2{10, 5}, 4.0f, 24);
        CHECK(pts.size() == 24);
        for (const vec2& p : pts) {
            const float dx = p.x - 10.0f, dy = p.y - 5.0f;
            CHECK(std::sqrt(dx * dx + dy * dy) <= 4.0f + 1e-3f);
        }
        // Deterministic: same call gives the same first point.
        auto pts2 = game::diskSamples(vec2{10, 5}, 4.0f, 24);
        CHECK_NEAR(pts[0].x, pts2[0].x, 1e-6f);
        CHECK_NEAR(pts[0].y, pts2[0].y, 1e-6f);
        // Degenerate: 1 sample (or radius 0) is a single centre point (point light).
        auto one = game::diskSamples(vec2{3, 7}, 4.0f, 1);
        CHECK(one.size() == 1);
        CHECK_NEAR(one[0].x, 3.0f, 1e-6f);
        auto zeroR = game::diskSamples(vec2{3, 7}, 0.0f, 16);
        CHECK(zeroR.size() == 1);
    }

    // softVisibility: no occluders -> fully lit.
    CHECK_NEAR(game::softVisibility(vec2{0, 20}, vec2{0, 0}, 4.0f, {}, 16), 1.0f, 1e-6f);

    // A wall spanning the whole width between the point and the light -> full umbra (0).
    {
        std::vector<game::Segment2> occ{game::Segment2{vec2{-50, 6}, vec2{50, 6}}};
        const float v = game::softVisibility(vec2{0, 20}, vec2{0, 0}, 4.0f, occ, 24);
        CHECK_NEAR(v, 0.0f, 1e-6f);
    }

    // A wall covering only one side of the light -> penumbra: strictly between 0 and 1.
    {
        std::vector<game::Segment2> occ{game::Segment2{vec2{0, 6}, vec2{60, 6}}};
        const float v = game::softVisibility(vec2{3, 20}, vec2{0, 0}, 30.0f, occ, 48);
        CHECK(v > 0.0f);
        CHECK(v < 1.0f);
    }
}

void testSpatial3D() {
    using audio::Attenuation3D;
    using audio::computeSpatialMix;
    using audio::dopplerPitch;
    using audio::equalPowerPan;
    using audio::Listener3D;
    using audio::panPosition;
    using audio::Source3D;
    using audio::SpatialConfig;
    using audio::SpatialMix;
    using math::vec3;

    // Attenuation: 1 at/inside ref; models fall off past it; None is flat.
    {
        CHECK_NEAR(audio::attenuation3D(0.5f, 1.0f, 100.0f, 1.0f, Attenuation3D::Inverse), 1.0f, 1e-5f);
        CHECK_NEAR(audio::attenuation3D(1.0f, 1.0f, 100.0f, 1.0f, Attenuation3D::Inverse), 1.0f, 1e-5f);
        // Inverse at d=3, ref=1, rolloff=1: 1/(1+1*2) = 1/3.
        CHECK_NEAR(audio::attenuation3D(3.0f, 1.0f, 100.0f, 1.0f, Attenuation3D::Inverse), 1.0f / 3.0f,
                   1e-5f);
        // InverseSquare at d=3, ref=1: 1/(1+1*4) = 1/5.
        CHECK_NEAR(audio::attenuation3D(3.0f, 1.0f, 100.0f, 1.0f, Attenuation3D::InverseSquare),
                   1.0f / 5.0f, 1e-5f);
        // Linear midway with rolloff 1 -> 0.5.
        CHECK_NEAR(audio::attenuation3D(50.5f, 1.0f, 100.0f, 1.0f, Attenuation3D::Linear), 0.5f, 1e-3f);
        // None is always 1; clamps beyond max for the falloff models.
        CHECK_NEAR(audio::attenuation3D(999.0f, 1.0f, 100.0f, 1.0f, Attenuation3D::None), 1.0f, 1e-5f);
        const float atMax = audio::attenuation3D(100.0f, 1.0f, 100.0f, 1.0f, Attenuation3D::Inverse);
        CHECK_NEAR(audio::attenuation3D(999.0f, 1.0f, 100.0f, 1.0f, Attenuation3D::Inverse), atMax,
                   1e-5f);
    }

    // Panning: a listener at origin facing -z with up +y has right = forward x up = +x.
    {
        Listener3D l;
        l.pos = vec3(0, 0, 0);
        l.forward = vec3(0, 0, -1);
        l.up = vec3(0, 1, 0);
        CHECK_NEAR(panPosition(l, vec3(5, 0, 0)), 1.0f, 1e-5f);   // straight right
        CHECK_NEAR(panPosition(l, vec3(-5, 0, 0)), -1.0f, 1e-5f); // straight left
        CHECK_NEAR(panPosition(l, vec3(0, 0, -5)), 0.0f, 1e-5f);  // straight ahead -> centred
        CHECK_NEAR(panPosition(l, vec3(0, 0, 0)), 0.0f, 1e-5f);   // on top of listener -> centred
    }

    // Constant-power pan split: centre is equal + power-preserving; extremes are hard channels.
    {
        float lft = 0.0f, rgt = 0.0f;
        equalPowerPan(0.0f, 1.0f, lft, rgt);
        CHECK_NEAR(lft, 0.70710678f, 1e-4f);
        CHECK_NEAR(rgt, 0.70710678f, 1e-4f);
        CHECK_NEAR(lft * lft + rgt * rgt, 1.0f, 1e-4f); // constant power
        equalPowerPan(-1.0f, 1.0f, lft, rgt);
        CHECK_NEAR(lft, 1.0f, 1e-4f);
        CHECK_NEAR(rgt, 0.0f, 1e-4f);
        equalPowerPan(1.0f, 1.0f, lft, rgt);
        CHECK_NEAR(lft, 0.0f, 1e-4f);
        CHECK_NEAR(rgt, 1.0f, 1e-4f);
    }

    // Doppler: static -> 1; source approaching -> pitch up; receding -> pitch down; listener approaching
    // -> pitch up. Source at (0,0,-10), listener at origin, so dHat (source->listener) = +z.
    {
        Listener3D l;
        l.pos = vec3(0, 0, 0);
        Source3D s;
        s.pos = vec3(0, 0, -10);
        const float c = 343.0f;

        CHECK_NEAR(dopplerPitch(l, s, c), 1.0f, 1e-5f); // both static

        s.velocity = vec3(0, 0, 34.3f); // moving +z = toward the listener
        CHECK(dopplerPitch(l, s, c) > 1.0f);
        // vS = +34.3 -> ratio = c/(c-34.3) = 343/308.7.
        CHECK_NEAR(dopplerPitch(l, s, c), 343.0f / (343.0f - 34.3f), 1e-4f);

        s.velocity = vec3(0, 0, -34.3f); // moving -z = away
        CHECK(dopplerPitch(l, s, c) < 1.0f);

        s.velocity = vec3(0, 0, 0);
        l.velocity = vec3(0, 0, -34.3f); // listener moving -z = toward the source
        // vL = dot(vel, dHat=+z) = -34.3 -> ratio = (c+34.3)/c.
        CHECK_NEAR(dopplerPitch(l, s, c), (343.0f + 34.3f) / 343.0f, 1e-4f);
        CHECK(dopplerPitch(l, s, c) > 1.0f);
    }

    // computeSpatialMix ties it together: a source to the right is louder in the right channel, and
    // farther sources are quieter overall.
    {
        Listener3D l;
        l.forward = vec3(0, 0, -1);
        l.up = vec3(0, 1, 0);
        SpatialConfig cfg;
        cfg.model = Attenuation3D::Inverse;
        cfg.refDistance = 1.0f;
        cfg.maxDistance = 100.0f;

        SpatialMix near = computeSpatialMix(l, Source3D{vec3(2, 0, 0), vec3(0, 0, 0)}, cfg);
        CHECK(near.right > near.left); // to the right -> louder right
        CHECK_NEAR(near.pan, 1.0f, 1e-4f);

        SpatialMix far = computeSpatialMix(l, Source3D{vec3(20, 0, 0), vec3(0, 0, 0)}, cfg);
        CHECK(far.right < near.right); // farther -> quieter
        CHECK_NEAR(near.pitch, 1.0f, 1e-5f); // no motion -> no doppler
    }
}

void testWav() {
    using audio::WavData;

    // 16-bit mono round-trips: encode float samples, decode, and get them back within quantization.
    {
        WavData in;
        in.sampleRate = 8000;
        in.channels = 1;
        in.samples = {0.0f, 0.5f, -0.5f, 1.0f, -1.0f};
        const std::vector<std::uint8_t> bytes = audio::encodeWav(in);
        // A valid RIFF/WAVE header.
        CHECK(bytes.size() >= 44);
        CHECK(bytes[0] == 'R' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == 'F');
        CHECK(bytes[8] == 'W' && bytes[9] == 'A' && bytes[10] == 'V' && bytes[11] == 'E');

        WavData out;
        CHECK(audio::decodeWav(bytes, out));
        CHECK(out.sampleRate == 8000);
        CHECK(out.channels == 1);
        CHECK(out.frameCount() == 5);
        CHECK_NEAR(out.samples[0], 0.0f, 1e-4f);
        CHECK_NEAR(out.samples[1], 0.5f, 1e-4f);
        CHECK_NEAR(out.samples[2], -0.5f, 1e-4f);
        CHECK_NEAR(out.samples[3], 1.0f, 1e-3f);  // 32767/32768
        CHECK_NEAR(out.samples[4], -1.0f, 1e-4f);
    }

    // Stereo interleaving is preserved through a round-trip.
    {
        WavData in;
        in.sampleRate = 44100;
        in.channels = 2;
        in.samples = {0.25f, -0.25f, 0.75f, -0.75f}; // 2 frames: (L,R),(L,R)
        WavData out;
        CHECK(audio::decodeWav(audio::encodeWav(in), out));
        CHECK(out.channels == 2);
        CHECK(out.frameCount() == 2);
        CHECK_NEAR(out.samples[0], 0.25f, 1e-3f);
        CHECK_NEAR(out.samples[1], -0.25f, 1e-3f);
        CHECK_NEAR(out.samples[3], -0.75f, 1e-3f);
    }

    // A hand-built 8-bit unsigned PCM stream decodes (128 = silence, 255 = +1, 0 = -1).
    {
        std::vector<std::uint8_t> b;
        auto tag = [&](const char* t) {
            for (int i = 0; i < 4; ++i) {
                b.push_back(static_cast<std::uint8_t>(t[i]));
            }
        };
        auto u32 = [&](std::uint32_t v) {
            for (int i = 0; i < 4; ++i) {
                b.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
            }
        };
        auto u16 = [&](std::uint16_t v) {
            b.push_back(static_cast<std::uint8_t>(v & 0xFF));
            b.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
        };
        const std::uint8_t pcm[3] = {128, 255, 0};
        tag("RIFF");
        u32(36u + 3u);
        tag("WAVE");
        tag("fmt ");
        u32(16u);
        u16(1u); // PCM
        u16(1u); // mono
        u32(22050u);
        u32(22050u); // byteRate
        u16(1u);     // blockAlign
        u16(8u);     // bits
        tag("data");
        u32(3u);
        for (std::uint8_t s : pcm) {
            b.push_back(s);
        }

        WavData out;
        CHECK(audio::decodeWav(b, out));
        CHECK(out.sampleRate == 22050);
        CHECK(out.channels == 1);
        CHECK(out.frameCount() == 3);
        CHECK_NEAR(out.samples[0], 0.0f, 1e-3f);   // 128 -> 0
        CHECK_NEAR(out.samples[1], 0.9922f, 2e-3f); // 255 -> ~+1
        CHECK_NEAR(out.samples[2], -1.0f, 1e-3f);   // 0 -> -1
    }

    // Malformed / too-short streams fail cleanly.
    {
        WavData out;
        CHECK(!audio::decodeWav(nullptr, 0, out));
        const std::vector<std::uint8_t> junk = {'N', 'O', 'P', 'E'};
        CHECK(!audio::decodeWav(junk, out));
    }
}

void testParticleEmitter() {
    using fx::Curve;
    using fx::EmitShape;
    using fx::Emitter;
    using fx::Gradient;
    using fx::ParticleState;
    using fx::sampleOffset;
    using fx::simulate;
    using math::vec2;

    // Curve: linear ramp 1 -> 0, clamped at the ends, empty -> 0.
    {
        Curve c;
        c.addPoint(1.0f, 0.0f); // add out of order to exercise sorted insert
        c.addPoint(0.0f, 1.0f);
        CHECK(c.size() == 2);
        CHECK_NEAR(c.sample(0.0f), 1.0f, 1e-5f);
        CHECK_NEAR(c.sample(1.0f), 0.0f, 1e-5f);
        CHECK_NEAR(c.sample(0.25f), 0.75f, 1e-5f);
        CHECK_NEAR(c.sample(-1.0f), 1.0f, 1e-5f); // clamp low
        CHECK_NEAR(c.sample(2.0f), 0.0f, 1e-5f);  // clamp high
        Curve empty;
        CHECK_NEAR(empty.sample(0.5f), 0.0f, 1e-5f);
    }

    // Gradient: red -> blue, midpoint is the average; empty -> white.
    {
        Gradient g;
        g.addStop(0.0f, render::Color{1, 0, 0, 1});
        g.addStop(1.0f, render::Color{0, 0, 1, 1});
        const render::Color mid = g.sample(0.5f);
        CHECK_NEAR(mid.r, 0.5f, 1e-5f);
        CHECK_NEAR(mid.b, 0.5f, 1e-5f);
        CHECK_NEAR(g.sample(-1.0f).r, 1.0f, 1e-5f); // clamp low -> red
        CHECK_NEAR(g.sample(2.0f).b, 1.0f, 1e-5f);  // clamp high -> blue
        Gradient empty;
        CHECK_NEAR(empty.sample(0.3f).g, 1.0f, 1e-5f); // white
    }

    // Emission shapes: Point is always the origin; Rect stays within its half-extents; Circle within
    // its radius; Ring within [inner, outer].
    {
        EmitShape pt;
        CHECK_NEAR(sampleOffset(pt, 0.3f, 0.7f).x, 0.0f, 1e-6f);
        CHECK_NEAR(sampleOffset(pt, 0.3f, 0.7f).y, 0.0f, 1e-6f);

        EmitShape rect;
        rect.type = EmitShape::Rect;
        rect.half = vec2(10.0f, 4.0f);
        for (int i = 0; i <= 10; ++i) {
            const float u = static_cast<float>(i) / 10.0f;
            const vec2 o = sampleOffset(rect, u, 1.0f - u);
            CHECK(std::fabs(o.x) <= 10.0f + 1e-4f);
            CHECK(std::fabs(o.y) <= 4.0f + 1e-4f);
        }

        EmitShape circ;
        circ.type = EmitShape::Circle;
        circ.radius = 12.0f;
        for (int i = 0; i <= 10; ++i) {
            const float u = static_cast<float>(i) / 10.0f;
            const vec2 o = sampleOffset(circ, u, u);
            CHECK(std::sqrt(o.x * o.x + o.y * o.y) <= 12.0f + 1e-4f);
        }

        EmitShape ring;
        ring.type = EmitShape::Ring;
        ring.radius = 20.0f;
        ring.innerRadius = 10.0f;
        for (int i = 0; i <= 10; ++i) {
            const float u = static_cast<float>(i) / 10.0f;
            const vec2 o = sampleOffset(ring, u, 0.5f);
            const float r = std::sqrt(o.x * o.x + o.y * o.y);
            CHECK(r >= 10.0f - 1e-4f);
            CHECK(r <= 20.0f + 1e-4f);
        }
    }

    // simulate: a single particle, fixed life/speed/direction so the motion is exact.
    {
        Emitter e;
        e.position = vec2(100.0f, 100.0f);
        e.count = 1;               // i=0, frac=0 -> birth 0
        e.explosiveness = 1.0f;    // born at t=0
        e.lifeMin = e.lifeMax = 1.0f;
        e.shape.type = EmitShape::Point;
        e.direction = vec2(1.0f, 0.0f); // +x
        e.spread = 0.0f;
        e.speedMin = e.speedMax = 100.0f;
        e.gravity = vec2(0.0f, 0.0f);
        e.sizeBase = 4.0f;

        auto at0 = simulate(e, 7u, 0.0f);
        CHECK(at0.size() == 1);
        CHECK_NEAR(at0[0].pos.x, 100.0f, 1e-3f); // at origin at age 0
        CHECK_NEAR(at0[0].pos.y, 100.0f, 1e-3f);

        auto at05 = simulate(e, 7u, 0.5f); // moved +50 in x at 100px/s for 0.5s
        CHECK(at05.size() == 1);
        CHECK_NEAR(at05[0].pos.x, 150.0f, 1e-3f);
        CHECK_NEAR(at05[0].pos.y, 100.0f, 1e-3f);

        // Past its lifetime -> not alive.
        auto atDead = simulate(e, 7u, 1.01f);
        CHECK(atDead.empty());

        // Gravity adds 0.5*g*t^2: with g=(0,200) at t=0.5 -> +25 in y.
        Emitter g = e;
        g.gravity = vec2(0.0f, 200.0f);
        auto gs = simulate(g, 7u, 0.5f);
        CHECK(gs.size() == 1);
        CHECK_NEAR(gs[0].pos.y, 100.0f + 25.0f, 1e-3f);
        CHECK_NEAR(gs[0].pos.x, 150.0f, 1e-3f);
    }

    // Determinism + burst count: an explosive burst has all `count` particles alive right after t=0.
    {
        Emitter e;
        e.count = 50;
        e.explosiveness = 1.0f; // all born at 0
        e.lifeMin = e.lifeMax = 2.0f;
        auto a = simulate(e, 123u, 0.01f);
        auto b = simulate(e, 123u, 0.01f);
        CHECK(a.size() == 50);
        CHECK(a.size() == b.size());
        bool identical = true;
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (std::fabs(a[i].pos.x - b[i].pos.x) > 1e-6f ||
                std::fabs(a[i].pos.y - b[i].pos.y) > 1e-6f) {
                identical = false;
            }
        }
        CHECK(identical); // same seed + time -> identical result
        // A different seed generally moves the particles.
        auto c = simulate(e, 999u, 0.01f);
        CHECK(c.size() == 50);
    }
}

void testTileSet() {
    using game::collectSolids;
    using game::dropY;
    using game::solidAt;
    using game::TileDef;
    using game::Tilemap;
    using game::TileSet;
    using math::vec2;

    // A 4x3 map, 10px tiles. Row 2 (bottom) = ground (id 1, Full). One ledge (id 2, Box bottom-half) at
    // (1,1). Everything else empty (id 0, undefined in the set).
    Tilemap map;
    map.resize(4, 3, 0);
    map.setTileSize(10.0f);
    map.set(0, 2, 1);
    map.set(1, 2, 1);
    map.set(2, 2, 1);
    map.set(3, 2, 1);
    map.set(1, 1, 2); // a ledge floating one row above the ground

    TileSet set;
    TileDef ground;
    ground.collision = TileDef::Full;
    ground.atlasX = 0;
    ground.atlasY = 1;
    set.define(1, ground);
    TileDef ledge;
    ledge.collision = TileDef::Box;
    ledge.boxMin = vec2(0.0f, 0.5f); // bottom half of the cell solid; its TOP surface is mid-cell
    ledge.boxMax = vec2(1.0f, 1.0f);
    ledge.atlasX = 2;
    ledge.atlasY = 0;
    set.define(2, ledge);

    // Lookups.
    CHECK(set.size() == 2);
    CHECK(set.get(1) != nullptr);
    CHECK(set.get(9) == nullptr);       // undefined id
    CHECK(set.isSolid(1));
    CHECK(set.isSolid(2));
    CHECK(!set.isSolid(0));             // empty tile: not in the set -> not solid
    CHECK(set.get(1)->atlasY == 1);    // atlas source carried through

    // collectSolids: 4 ground + 1 ledge = 5 boxes; the ledge's box is the bottom half of its cell.
    auto solids = collectSolids(map, set);
    CHECK(solids.size() == 5);
    const game::TileBox* lb = nullptr;
    for (const auto& b : solids) {
        if (b.id == 2) {
            lb = &b;
        }
    }
    CHECK(lb != nullptr);
    // Cell (1,1) spans world x[10,20] y[10,20]; bottom-half box -> y[15,20], full x.
    CHECK_NEAR(lb->min.x, 10.0f, 1e-4f);
    CHECK_NEAR(lb->min.y, 15.0f, 1e-4f);
    CHECK_NEAR(lb->max.x, 20.0f, 1e-4f);
    CHECK_NEAR(lb->max.y, 20.0f, 1e-4f);

    // solidAt: a full ground cell is solid anywhere inside; the ledge cell is solid only in its bottom
    // half; empty cells and out-of-bounds read as not solid.
    CHECK(solidAt(map, set, vec2(5.0f, 25.0f)));    // inside ground cell (0,2)
    CHECK(solidAt(map, set, vec2(15.0f, 18.0f)));   // inside ledge's solid bottom half
    CHECK(!solidAt(map, set, vec2(15.0f, 12.0f)));  // ledge cell but in its empty TOP half
    CHECK(!solidAt(map, set, vec2(5.0f, 5.0f)));    // empty cell (0,0)
    CHECK(!solidAt(map, set, vec2(-5.0f, 5.0f)));   // out of bounds

    // dropY: a point falling down column x rests on the first solid top surface. Column 0 (only ground)
    // rests on the ground top (y=20). Column 1 (ledge above ground) rests on the ledge top (y=15).
    CHECK_NEAR(dropY(map, set, 5.0f, 0.0f, 999.0f), 20.0f, 1e-4f);   // ground top of cell (0,2)
    CHECK_NEAR(dropY(map, set, 15.0f, 0.0f, 999.0f), 15.0f, 1e-4f);  // ledge top (mid-cell) at (1,1)
    // A column with no solids below returns maxY (column 2 above the ground still hits ground at 20).
    CHECK_NEAR(dropY(map, set, 25.0f, 0.0f, 999.0f), 20.0f, 1e-4f);  // column 2: only ground -> 20
    CHECK_NEAR(dropY(map, set, 55.0f, 0.0f, 999.0f), 999.0f, 1e-4f); // x out of range -> maxY
}

void testCollisionLayers() {
    using game::CollisionObject2D;
    using game::detects;
    using game::interact;
    using game::layerBit;
    using game::layerMask;
    using game::LayerRegistry;

    // Bits & mask building.
    CHECK(layerBit(0) == 1u);
    CHECK(layerBit(3) == 8u);
    CHECK(layerBit(31) == 0x80000000u);
    CHECK(layerBit(-1) == 0u);  // out of range -> empty
    CHECK(layerBit(32) == 0u);
    CHECK(layerMask({0, 1, 4}) == (1u | 2u | 16u));

    // Directional detect: observer's mask vs target's layer.
    CHECK(detects(layerBit(2), layerBit(2)));          // scanning the layer it lives in
    CHECK(!detects(layerBit(2), layerBit(3)));         // different bit -> no
    CHECK(detects(layerMask({1, 2}), layerBit(2)));    // mask covering several layers
    CHECK(!detects(0u, layerBit(0)));                  // scans nothing

    // Symmetric interact: either side scanning the other pairs them.
    // A scans B's layer, B scans nothing back -> still interact (Godot pairs on either direction).
    CHECK(interact(layerBit(0), layerBit(1), layerBit(1), 0u));
    CHECK(interact(layerBit(1), 0u, layerBit(0), layerBit(1))); // B scans A
    CHECK(!interact(layerBit(0), layerBit(0), layerBit(1), layerBit(1))); // neither scans the other
    CHECK(interact(layerBit(0), layerBit(0), layerBit(0), layerBit(0)));  // both on/scan layer 0

    // CollisionObject2D bit editing + queries.
    {
        CollisionObject2D player;
        player.layer = 0;
        player.mask = 0;
        player.setLayerBit(0, true);  // player lives on layer 0
        player.setMaskBit(1, true);   // player scans layer 1 (enemies)
        player.setMaskBit(2, true);   // and layer 2 (pickups)
        CHECK(player.layerHas(0));
        CHECK(!player.layerHas(1));
        CHECK(player.maskHas(1));
        CHECK(player.maskHas(2));
        CHECK(!player.maskHas(0));
        player.setMaskBit(2, false); // stop scanning pickups
        CHECK(!player.maskHas(2));

        CollisionObject2D enemy;
        enemy.layer = layerBit(1); // enemy lives on layer 1
        enemy.mask = layerBit(0);  // enemy scans layer 0 (player)
        CHECK(player.detects(enemy));      // player.mask(1) & enemy.layer(1) -> yes
        CHECK(enemy.detects(player));      // enemy.mask(0) & player.layer(0) -> yes
        CHECK(player.interactsWith(enemy));

        CollisionObject2D pickup;
        pickup.layer = layerBit(2); // pickups on layer 2
        pickup.mask = 0;            // pickups scan nothing
        CHECK(!player.detects(pickup));      // player no longer scans layer 2
        CHECK(!pickup.detects(player));      // pickup scans nothing
        CHECK(!player.interactsWith(pickup)); // neither scans the other -> no pairing
    }

    // Named-layer registry: insertion-ordered bit assignment, lookup, combined masks, overflow.
    {
        LayerRegistry reg;
        const int p = reg.add("player");
        const int e = reg.add("enemy");
        const int k = reg.add("pickup");
        CHECK(p == 0);
        CHECK(e == 1);
        CHECK(k == 2);
        CHECK(reg.add("player") == 0); // re-adding returns the existing index
        CHECK(reg.size() == 3);
        CHECK(reg.index("enemy") == 1);
        CHECK(reg.index("missing") == -1);
        CHECK(reg.bit("pickup") == layerBit(2));
        CHECK(reg.bit("missing") == 0u);
        CHECK(reg.mask({"player", "pickup"}) == (layerBit(0) | layerBit(2)));

        // Fill to 32 then overflow.
        LayerRegistry full;
        for (int i = 0; i < 32; ++i) {
            CHECK(full.add("l" + std::to_string(i)) == i);
        }
        CHECK(full.add("overflow") == -1);
        CHECK(full.size() == 32);
    }
}

void testArea2D() {
    using game::Area2D;
    using math::vec2;

    // --- Overlap geometry. ---
    {
        Area2D c;
        c.shape = Area2D::Circle;
        c.pos = vec2(0.0f, 0.0f);
        c.radius = 2.0f;

        Area2D c2;
        c2.shape = Area2D::Circle;
        c2.radius = 1.0f;
        c2.pos = vec2(2.5f, 0.0f); // centres 2.5 apart, radii sum 3 -> overlap
        CHECK(game::overlaps(c, c2));
        c2.pos = vec2(3.5f, 0.0f); // 3.5 > 3 -> no
        CHECK(!game::overlaps(c, c2));
        c2.pos = vec2(3.0f, 0.0f); // exactly touching -> not overlapping (strict)
        CHECK(!game::overlaps(c, c2));

        // Circle vs box.
        Area2D b;
        b.shape = Area2D::Box;
        b.half = vec2(1.0f, 1.0f);
        b.pos = vec2(2.5f, 0.0f); // box spans x[1.5,3.5]; circle reaches x=2 -> overlap
        CHECK(game::overlaps(c, b));
        b.pos = vec2(4.5f, 0.0f); // box spans x[3.5,5.5]; circle reaches 2 -> no
        CHECK(!game::overlaps(c, b));
        // Corner case: box just off the circle's corner.
        b.pos = vec2(3.0f, 3.0f); // nearest corner (2,2), dist sqrt8 ~2.83 > 2 -> no
        CHECK(!game::overlaps(c, b));
        b.pos = vec2(2.2f, 2.2f); // nearest corner (1.2,1.2), dist ~1.70 < 2 -> yes
        CHECK(game::overlaps(c, b));

        // Box vs box (AABB).
        Area2D b1, b2;
        b1.shape = Area2D::Box;
        b1.half = vec2(1.0f, 1.0f);
        b1.pos = vec2(0.0f, 0.0f);
        b2.shape = Area2D::Box;
        b2.half = vec2(1.0f, 1.0f);
        b2.pos = vec2(1.5f, 0.0f); // overlap in x by 0.5
        CHECK(game::overlaps(b1, b2));
        b2.pos = vec2(2.5f, 0.0f);
        CHECK(!game::overlaps(b1, b2));
    }

    // --- containsPoint. ---
    {
        Area2D c;
        c.shape = Area2D::Circle;
        c.radius = 2.0f;
        CHECK(c.containsPoint(vec2(1.0f, 1.0f)));   // dist sqrt2 < 2
        CHECK(!c.containsPoint(vec2(2.0f, 2.0f)));  // dist sqrt8 > 2

        Area2D b;
        b.shape = Area2D::Box;
        b.half = vec2(3.0f, 1.0f);
        CHECK(b.containsPoint(vec2(2.9f, 0.9f)));
        CHECK(!b.containsPoint(vec2(3.1f, 0.0f)));
    }

    // --- AreaMonitor enter/exit diffing across frames. ---
    {
        game::AreaMonitor mon;
        std::vector<int> entered, exited;

        mon.update({1, 2}, entered, exited); // first frame: 1,2 enter
        CHECK(entered.size() == 2 && entered[0] == 1 && entered[1] == 2);
        CHECK(exited.empty());
        CHECK(mon.contains(1) && mon.contains(2) && !mon.contains(3));

        mon.update({2, 3}, entered, exited); // 3 enters, 1 exits, 2 stays
        CHECK(entered.size() == 1 && entered[0] == 3);
        CHECK(exited.size() == 1 && exited[0] == 1);
        CHECK(mon.members().size() == 2); // {2,3}
        CHECK(mon.contains(2) && mon.contains(3) && !mon.contains(1));

        mon.update({2, 3}, entered, exited); // no change
        CHECK(entered.empty() && exited.empty());

        mon.update({}, entered, exited); // everyone leaves
        CHECK(entered.empty());
        CHECK(exited.size() == 2 && exited[0] == 2 && exited[1] == 3);
        CHECK(mon.members().empty());

        // Duplicate ids in the input are de-duplicated.
        mon.update({5, 5, 5}, entered, exited);
        CHECK(entered.size() == 1 && entered[0] == 5);
        CHECK(mon.members().size() == 1);
    }
}

void testAvoidance() {
    using math::vec2;
    const float radius = 0.5f, maxSpeed = 2.0f;

    // No neighbours -> the preferred velocity is returned exactly.
    {
        const vec2 pref(2.0f, 0.0f);
        auto v = game::rvoVelocity(vec2(0, 0), vec2(0, 0), pref, radius, maxSpeed, {});
        CHECK_NEAR(v.x, pref.x, 1e-4f);
        CHECK_NEAR(v.y, pref.y, 1e-4f);
    }

    // A neighbour dead ahead on a head-on course: the chosen velocity must steer aside (nonzero lateral
    // component) rather than drive straight into it.
    {
        std::vector<game::AvoidNeighbor> nb = {{vec2(4.0f, 0.0f), vec2(-2.0f, 0.0f), radius}};
        const vec2 pref(2.0f, 0.0f); // straight at the neighbour
        auto v = game::rvoVelocity(vec2(0, 0), vec2(2, 0), pref, radius, maxSpeed, nb);
        CHECK(std::fabs(v.y) > 0.1f); // deviated sideways to avoid
    }

    // Two agents crossing head-on, both running RVO each step, never overlap and both make progress.
    {
        const float dt = 1.0f / 30.0f;
        vec2 pa(0, 0), pb(10, 0);
        vec2 va(0, 0), vb(0, 0);
        const vec2 ga(10, 0), gb(0, 0);
        float minGap = 1e9f;
        for (int i = 0; i < 240; ++i) {
            auto pref = [&](vec2 p, vec2 g) {
                vec2 d = g - p;
                float l = std::sqrt(d.x * d.x + d.y * d.y);
                return l > 1e-4f ? d / l * std::min(maxSpeed, l / dt) : vec2(0, 0);
            };
            std::vector<game::AvoidNeighbor> na = {{pb, vb, radius}};
            std::vector<game::AvoidNeighbor> nbr = {{pa, va, radius}};
            va = game::rvoVelocity(pa, va, pref(pa, ga), radius, maxSpeed, na);
            vb = game::rvoVelocity(pb, vb, pref(pb, gb), radius, maxSpeed, nbr);
            pa += va * dt;
            pb += vb * dt;
            const vec2 d = pb - pa;
            minGap = std::min(minGap, std::sqrt(d.x * d.x + d.y * d.y));
        }
        CHECK(minGap > 2.0f * radius - 0.15f); // never (meaningfully) overlapped
        CHECK(pa.x > 7.0f);                    // A still crossed to the far side
        CHECK(pb.x < 3.0f);                    // B likewise
    }
}

void testAutoTile() {
    // Cellular cave generation is deterministic for a seed, enclosed by walls, and mixed (not uniform).
    {
        const int w = 48, h = 32;
        auto a = game::CellularCave::generate(w, h, 12345);
        auto b = game::CellularCave::generate(w, h, 12345);
        auto c = game::CellularCave::generate(w, h, 99999);
        CHECK(a == b);   // same seed -> identical cave (reproducible)
        CHECK(!(a == c)); // different seed -> different cave

        // The whole border is solid.
        bool borderSolid = true;
        for (int x = 0; x < w; ++x) {
            if (a[game::CellularCave::idx(w, x, 0)] == 0) borderSolid = false;
            if (a[game::CellularCave::idx(w, x, h - 1)] == 0) borderSolid = false;
        }
        for (int y = 0; y < h; ++y) {
            if (a[game::CellularCave::idx(w, 0, y)] == 0) borderSolid = false;
            if (a[game::CellularCave::idx(w, w - 1, y)] == 0) borderSolid = false;
        }
        CHECK(borderSolid);

        // It carved SOME floor and kept SOME wall (a smoothed cave, not all one thing).
        int walls = 0, floors = 0;
        for (uint8_t v : a) {
            (v ? walls : floors)++;
        }
        CHECK(walls > 0);
        CHECK(floors > 0);
    }

    // autotileMask4: fully-surrounded -> 0x0F; isolated -> 0; open on one side clears that bit;
    // out-of-bounds counts as solid.
    {
        const int w = 3, h = 3;
        std::vector<uint8_t> g(9, 1); // all solid
        CHECK(game::autotileMask4(g, w, h, 1, 1) == 0x0F);

        std::vector<uint8_t> iso(9, 0);
        iso[game::CellularCave::idx(w, 1, 1)] = 1; // a lone solid cell surrounded by floor
        CHECK(game::autotileMask4(iso, w, h, 1, 1) == 0x00);

        std::vector<uint8_t> openE(9, 1);
        openE[game::CellularCave::idx(w, 2, 1)] = 0;      // east neighbour is floor
        CHECK((game::autotileMask4(openE, w, h, 1, 1) & 0x2) == 0); // E bit cleared
        CHECK((game::autotileMask4(openE, w, h, 1, 1) & 0x1) != 0); // N still set

        // A corner cell: its off-map N and W sides read as solid (enclosed), only interior sides vary.
        std::vector<uint8_t> gg(9, 1);
        gg[game::CellularCave::idx(w, 1, 0)] = 0; // the cell to the east of corner (0,0) is floor...
        // corner (0,0): N and W are out of bounds (solid), E=(1,0)=floor, S=(0,1)=solid
        CHECK((game::autotileMask4(gg, w, h, 0, 0) & 0x2) == 0); // E open
        CHECK((game::autotileMask4(gg, w, h, 0, 0) & 0x1) != 0); // N (out of bounds) solid
        CHECK((game::autotileMask4(gg, w, h, 0, 0) & 0x8) != 0); // W (out of bounds) solid
        CHECK(game::autotileIndex4(0x0F) == 15);
    }
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

void testTweenPlayer() {
    using anim::TweenPlayer;

    // Sequential: x goes 0->100 (1s), then 100->0 (1s). Linear so values are exact.
    {
        float x = -1.0f;
        TweenPlayer tp;
        tp.appendProperty([&](float v) { x = v; }, 0.0f, 100.0f, 1.0f)
            .appendProperty([&](float v) { x = v; }, 100.0f, 0.0f, 1.0f);
        CHECK(tp.stepCount() == 2);
        CHECK_NEAR(tp.totalDuration(), 2.0f, 1e-5f);
        tp.update(0.5f);
        CHECK_NEAR(x, 50.0f, 1e-4f); // half through step 1
        tp.update(0.5f);
        CHECK_NEAR(x, 100.0f, 1e-4f); // step 1 complete
        tp.update(0.5f);
        CHECK_NEAR(x, 50.0f, 1e-4f); // half through step 2 (100 -> 0)
        CHECK(!tp.finished());
        tp.update(0.5f);
        CHECK_NEAR(x, 0.0f, 1e-4f);
        CHECK(tp.finished());
    }

    // Parallel: within ONE step, x:0->10 and y:0->20 animate together over 1s.
    {
        float x = 0.0f, y = 0.0f;
        TweenPlayer tp;
        tp.appendProperty([&](float v) { x = v; }, 0.0f, 10.0f, 1.0f)
            .parallelProperty([&](float v) { y = v; }, 0.0f, 20.0f, 1.0f);
        CHECK(tp.stepCount() == 1); // one group, two tweeners
        tp.update(0.5f);
        CHECK_NEAR(x, 5.0f, 1e-4f);
        CHECK_NEAR(y, 10.0f, 1e-4f);
    }

    // Interval delays the next property tween.
    {
        float x = 7.0f;
        TweenPlayer tp;
        tp.appendInterval(1.0f).appendProperty([&](float v) { x = v; }, 0.0f, 10.0f, 1.0f);
        tp.update(0.5f);
        CHECK_NEAR(x, 7.0f, 1e-4f); // still waiting
        tp.update(1.0f);           // 1.5s total: 0.5s into the property
        CHECK_NEAR(x, 5.0f, 1e-4f);
    }

    // Callback fires once, sequenced; loops replay it.
    {
        int hits = 0;
        float x = 0.0f;
        TweenPlayer tp;
        tp.appendProperty([&](float v) { x = v; }, 0.0f, 1.0f, 1.0f)
            .appendCallback([&]() { ++hits; })
            .setLoops(2);
        tp.update(1.0f); // finishes property + fires callback (loop 1), wraps to start of loop 2
        CHECK(hits == 1);
        tp.update(1.0f); // finishes loop 2's property + callback, then done
        CHECK(hits == 2);
        CHECK(tp.finished());
    }

    // Easing is applied: QuadOut is past the halfway value at t=0.5 (fast start, slow end).
    {
        float x = 0.0f;
        TweenPlayer tp;
        tp.appendProperty([&](float v) { x = v; }, 0.0f, 100.0f, 1.0f, anim::Ease::QuadOut);
        tp.update(0.5f);
        CHECK(x > 50.0f); // ease-out is ahead of linear mid-way
        tp.update(0.5f);
        CHECK_NEAR(x, 100.0f, 1e-4f);
    }

    // Infinite loop (loops <= 0) never finishes.
    {
        float x = 0.0f;
        TweenPlayer tp;
        tp.appendProperty([&](float v) { x = v; }, 0.0f, 1.0f, 1.0f).setLoops(0);
        for (int i = 0; i < 50; ++i) {
            tp.update(1.0f);
        }
        CHECK(!tp.finished());
    }
}

void testTriggerTrack() {
    // --- TriggerTrack.collectRange: half-open forward sweep, fire-once semantics. ---
    {
        anim::TriggerTrack tt;
        tt.add(1.0f, 10);
        tt.add(2.0f, 20);
        tt.add(2.0f, 21); // two markers at the same time both fire, in insertion order
        tt.add(3.0f, 30);
        CHECK(tt.size() == 4);
        CHECK_NEAR(tt.endTime(), 3.0f, 1e-6f);

        std::vector<int> out;
        tt.collectRange(0.0f, 2.0f, false, out); // [0,2): fires only the marker at 1.0
        CHECK(out.size() == 1);
        CHECK(out[0] == 10);

        out.clear();
        tt.collectRange(2.0f, 3.0f, false, out); // [2,3): both markers at 2.0, not the one at 3.0
        CHECK(out.size() == 2);
        CHECK(out[0] == 20);
        CHECK(out[1] == 21);

        out.clear();
        tt.collectRange(2.0f, 3.0f, true, out); // inclusive end: also fires the marker at 3.0
        CHECK(out.size() == 3);
        CHECK(out[2] == 30);
    }

    // --- MethodTimeline (Once): each marker fires exactly once as the head sweeps, end marker included. ---
    {
        anim::MethodTimeline mt;
        mt.length = 4.0f;
        mt.loop = anim::Loop::Once;
        mt.track.add(1.0f, 1);
        mt.track.add(2.5f, 2);
        mt.track.add(4.0f, 3); // exactly at the clip end

        std::vector<int> fired;
        // Step in small increments across the whole clip.
        for (int i = 0; i < 100; ++i) {
            mt.update(0.05f, fired); // 100 * 0.05 = 5s, past the 4s end
        }
        CHECK(mt.finished);
        CHECK(fired.size() == 3);
        CHECK(fired[0] == 1);
        CHECK(fired[1] == 2);
        CHECK(fired[2] == 3); // the end-of-clip marker fired (inclusive terminal segment)

        // No further fires after finishing.
        std::vector<int> more;
        mt.update(1.0f, more);
        CHECK(more.empty());
    }

    // --- A single big step across several markers fires them all, in order. ---
    {
        anim::MethodTimeline mt;
        mt.length = 10.0f;
        mt.loop = anim::Loop::Once;
        mt.track.add(1.0f, 1);
        mt.track.add(3.0f, 2);
        mt.track.add(7.0f, 3);
        std::vector<int> fired;
        mt.update(8.0f, fired); // one jump from 0 to 8 crosses markers at 1,3,7
        CHECK(fired.size() == 3);
        CHECK(fired[0] == 1 && fired[1] == 2 && fired[2] == 3);
    }

    // --- MethodTimeline (Repeat): markers fire once per loop pass, wrap handled, no double-fire. ---
    {
        anim::MethodTimeline mt;
        mt.length = 4.0f;
        mt.loop = anim::Loop::Repeat;
        mt.track.add(0.0f, 0); // downbeat at the loop start
        mt.track.add(2.0f, 1);
        // A marker exactly at length would be the next loop's 0 -> must not double-fire.

        std::vector<int> fired;
        // Run for 9 seconds = 2 full loops (8s) + 1s into the third.
        for (int i = 0; i < 180; ++i) {
            mt.update(0.05f, fired);
        }
        int c0 = 0, c1 = 0;
        for (int id : fired) {
            (id == 0 ? c0 : c1)++;
        }
        // Downbeat (t=0): loops start at 0,4,8 -> 3 times within [0,9). Marker at 2: 2,6 -> 2 times
        // (next at 10 > 9). (The very first update fires t=0 as the head leaves 0.)
        CHECK(c0 == 3);
        CHECK(c1 == 2);
    }

    // --- Degenerate: zero-length clip fires nothing and never hangs. ---
    {
        anim::MethodTimeline mt;
        mt.length = 0.0f; // and no markers -> endTime 0
        std::vector<int> fired;
        mt.update(1.0f, fired);
        CHECK(fired.empty());
        CHECK(!mt.finished); // len<=0 is a no-op, not a completion
    }
}

void testTimeline() {
    using anim::Ease;

    // ---- Track sampling ----
    {
        anim::Track tr;
        tr.add(0.0f, 0.0f);
        tr.add(1.0f, 10.0f);
        tr.add(3.0f, 30.0f);
        // Hold before the first key and after the last (no extrapolation).
        CHECK_NEAR(tr.sample(-1.0f), 0.0f, 1e-5f);
        CHECK_NEAR(tr.sample(5.0f), 30.0f, 1e-5f);
        // Exact keys.
        CHECK_NEAR(tr.sample(0.0f), 0.0f, 1e-5f);
        CHECK_NEAR(tr.sample(1.0f), 10.0f, 1e-5f);
        CHECK_NEAR(tr.sample(3.0f), 30.0f, 1e-5f);
        // Linear midpoints inside each segment.
        CHECK_NEAR(tr.sample(0.5f), 5.0f, 1e-5f);   // halfway 0->10
        CHECK_NEAR(tr.sample(2.0f), 20.0f, 1e-5f);  // halfway 10->30 over [1,3]
    }

    // Out-of-order insertion stays sorted and samples correctly.
    {
        anim::Track tr;
        tr.add(2.0f, 20.0f);
        tr.add(0.0f, 0.0f);
        tr.add(1.0f, 10.0f);
        CHECK(tr.keys.size() == 3);
        CHECK_NEAR(tr.keys[0].time, 0.0f, 1e-6f);
        CHECK_NEAR(tr.keys[1].time, 1.0f, 1e-6f);
        CHECK_NEAR(tr.keys[2].time, 2.0f, 1e-6f);
        CHECK_NEAR(tr.sample(1.5f), 15.0f, 1e-5f);
    }

    // Per-segment easing: QuadIn on [0,1] gives ease(0.5)=0.25 -> value 2.5 for a 0..10 segment.
    {
        anim::Track tr;
        tr.add(0.0f, 0.0f, Ease::QuadIn);
        tr.add(1.0f, 10.0f);
        CHECK_NEAR(tr.sample(0.5f), 2.5f, 1e-4f);
    }

    // ---- Timeline: multi-track, duration, playback ----
    {
        anim::Timeline tl;
        tl.track("x").add(0.0f, 0.0f);
        tl.track("x").add(2.0f, 100.0f);
        tl.track("y").add(0.0f, 50.0f);
        tl.track("y").add(2.0f, 50.0f); // constant track
        // Auto length = longest track end.
        CHECK_NEAR(tl.length(), 2.0f, 1e-5f);
        // Tracks are independent.
        CHECK_NEAR(tl.valueAt("x", 1.0f), 50.0f, 1e-5f);
        CHECK_NEAR(tl.valueAt("y", 1.0f), 50.0f, 1e-5f);
        // Unknown track -> 0.
        CHECK_NEAR(tl.valueAt("nope", 1.0f), 0.0f, 1e-6f);

        // Playhead advance + value() at the playhead.
        tl.loop = anim::Loop::Once;
        tl.update(0.5f);
        CHECK_NEAR(tl.value("x"), 25.0f, 1e-5f);
        tl.update(10.0f); // overshoot -> clamps + finishes
        CHECK(tl.finished);
        CHECK_NEAR(tl.value("x"), 100.0f, 1e-5f);
    }

    // Repeat wraps the playhead.
    {
        anim::Timeline tl;
        tl.track("v").add(0.0f, 0.0f);
        tl.track("v").add(1.0f, 10.0f);
        tl.loop = anim::Loop::Repeat;
        tl.update(1.5f);
        CHECK(!tl.finished);
        CHECK_NEAR(tl.playhead(), 0.5f, 1e-5f);
        CHECK_NEAR(tl.value("v"), 5.0f, 1e-5f);
    }

    // PingPong reflects the query time on the way back.
    {
        anim::Timeline tl;
        tl.track("v").add(0.0f, 0.0f);
        tl.track("v").add(1.0f, 10.0f);
        tl.loop = anim::Loop::PingPong;
        tl.update(1.5f); // one cycle forward + half back -> playhead reflects to 0.5
        CHECK(tl.reversing);
        CHECK_NEAR(tl.playhead(), 0.5f, 1e-5f);
        CHECK_NEAR(tl.value("v"), 5.0f, 1e-5f);
    }

    // Explicit duration overrides auto length.
    {
        anim::Timeline tl;
        tl.track("v").add(0.0f, 0.0f);
        tl.track("v").add(1.0f, 10.0f);
        tl.duration = 4.0f;
        CHECK_NEAR(tl.length(), 4.0f, 1e-5f);
    }
}

void testLayout() {
    using ui::LayoutNode;
    using ui::Rect;
    const Rect screen{0, 0, 1000, 600};

    // Anchor: fill with a margin insets the child on all sides.
    {
        LayoutNode root(LayoutNode::Mode::Anchor);
        LayoutNode child;
        child.fill(20.0f);
        root.add(&child);
        root.layout(screen);
        CHECK_NEAR(child.rect.x, 20.0f, 1e-3f);
        CHECK_NEAR(child.rect.y, 20.0f, 1e-3f);
        CHECK_NEAR(child.rect.w, 960.0f, 1e-3f); // 1000 - 40
        CHECK_NEAR(child.rect.h, 560.0f, 1e-3f); // 600 - 40
    }

    // Anchor: a fixed-size box pinned to the center via (0.5,0.5,0.5,0.5) anchors + offsets.
    {
        LayoutNode root(LayoutNode::Mode::Anchor);
        LayoutNode box;
        box.setAnchors(0.5f, 0.5f, 0.5f, 0.5f);
        box.setOffsets(-100.0f, -50.0f, 100.0f, 50.0f); // 200x100 centered
        root.add(&box);
        root.layout(screen);
        CHECK_NEAR(box.rect.w, 200.0f, 1e-3f);
        CHECK_NEAR(box.rect.h, 100.0f, 1e-3f);
        CHECK_NEAR(box.rect.centerX(), 500.0f, 1e-3f);
        CHECK_NEAR(box.rect.centerY(), 300.0f, 1e-3f);
    }

    // Anchor: responsiveness — the SAME anchored top bar stretches to whatever width the root has.
    {
        LayoutNode root(LayoutNode::Mode::Anchor);
        LayoutNode bar;
        bar.anchorTop(48.0f);
        root.add(&bar);
        root.layout(Rect{0, 0, 1000, 600});
        CHECK_NEAR(bar.rect.w, 1000.0f, 1e-3f);
        CHECK_NEAR(bar.rect.h, 48.0f, 1e-3f);
        root.layout(Rect{0, 0, 400, 300}); // shrink the window
        CHECK_NEAR(bar.rect.w, 400.0f, 1e-3f); // still full width
        CHECK_NEAR(bar.rect.h, 48.0f, 1e-3f);  // still fixed height
    }

    // HBox: two fixed 100-wide + one expander, spacing 10, in a 1000-wide row.
    {
        LayoutNode row(LayoutNode::Mode::HBox);
        row.spacing = 10.0f;
        LayoutNode a, b, c;
        a.minW = 100.0f;
        b.expand = true;
        c.minW = 100.0f;
        row.add(&a).add(&b).add(&c);
        row.layout(Rect{0, 0, 1000, 80});
        // leftover = 1000 - 20(spacing) - 200(fixed) = 780 -> to the single expander.
        CHECK_NEAR(a.rect.x, 0.0f, 1e-3f);
        CHECK_NEAR(a.rect.w, 100.0f, 1e-3f);
        CHECK_NEAR(b.rect.x, 110.0f, 1e-3f); // 100 + 10 spacing
        CHECK_NEAR(b.rect.w, 780.0f, 1e-3f);
        CHECK_NEAR(c.rect.x, 900.0f, 1e-3f); // 110 + 780 + 10
        CHECK_NEAR(c.rect.w, 100.0f, 1e-3f);
        CHECK_NEAR(a.rect.h, 80.0f, 1e-3f); // fill cross axis
    }

    // VBox: two equal expanders share the height with spacing.
    {
        LayoutNode col(LayoutNode::Mode::VBox);
        col.spacing = 20.0f;
        LayoutNode a, b;
        a.expand = true;
        b.expand = true;
        col.add(&a).add(&b);
        col.layout(Rect{0, 0, 200, 500});
        // leftover = 500 - 20 = 480, split -> 240 each.
        CHECK_NEAR(a.rect.h, 240.0f, 1e-3f);
        CHECK_NEAR(a.rect.y, 0.0f, 1e-3f);
        CHECK_NEAR(b.rect.y, 260.0f, 1e-3f); // 240 + 20
        CHECK_NEAR(b.rect.h, 240.0f, 1e-3f);
        CHECK_NEAR(a.rect.w, 200.0f, 1e-3f); // fill cross axis
    }

    // Center: a fixed-size child is centered within the parent.
    {
        LayoutNode root(LayoutNode::Mode::Center);
        LayoutNode dlg;
        dlg.minW = 300.0f;
        dlg.minH = 200.0f;
        root.add(&dlg);
        root.layout(Rect{0, 0, 1000, 600});
        CHECK_NEAR(dlg.rect.x, 350.0f, 1e-3f); // (1000-300)/2
        CHECK_NEAR(dlg.rect.y, 200.0f, 1e-3f); // (600-200)/2
    }

    // Padding on a container insets the arranged area.
    {
        LayoutNode row(LayoutNode::Mode::HBox);
        row.pad = 15.0f;
        LayoutNode a;
        a.expand = true;
        row.add(&a);
        row.layout(Rect{0, 0, 200, 100});
        CHECK_NEAR(a.rect.x, 15.0f, 1e-3f);
        CHECK_NEAR(a.rect.w, 170.0f, 1e-3f); // 200 - 30
        CHECK_NEAR(a.rect.y, 15.0f, 1e-3f);
        CHECK_NEAR(a.rect.h, 70.0f, 1e-3f);  // 100 - 30
    }

    // Nesting: a VBox inside the expanding cell of an HBox lays its children out within that cell.
    {
        LayoutNode row(LayoutNode::Mode::HBox);
        LayoutNode side;
        side.minW = 200.0f;
        LayoutNode content(LayoutNode::Mode::VBox);
        content.expand = true;
        content.spacing = 0.0f;
        LayoutNode top, bottom;
        top.expand = true;
        bottom.expand = true;
        content.add(&top).add(&bottom);
        row.add(&side).add(&content);
        row.layout(Rect{0, 0, 1000, 400});
        // content cell = x[200..1000], the VBox splits its 400 height into two 200s.
        CHECK_NEAR(content.rect.x, 200.0f, 1e-3f);
        CHECK_NEAR(content.rect.w, 800.0f, 1e-3f);
        CHECK_NEAR(top.rect.x, 200.0f, 1e-3f);   // inherits the content cell's x
        CHECK_NEAR(top.rect.h, 200.0f, 1e-3f);
        CHECK_NEAR(bottom.rect.y, 200.0f, 1e-3f);
    }
}

void testUiContainer() {
    using ui::Control;
    using ui::Rect;
    using ui::SizeFlag;

    // HBox: two fixed 40-wide (Fill) + one Expand, spacing 10, in a 200-wide area.
    {
        Control a, b, c;
        a.minW = 40.0f;
        b.minW = 40.0f;
        b.hFlag = SizeFlag::Expand;
        c.minW = 40.0f;
        std::vector<Control*> kids{&a, &b, &c};
        ui::hbox(Rect{0, 0, 200, 100}, kids, 10.0f);
        // totalMin = 120 + 20 sep = 140, leftover 60 -> the single expander.
        CHECK_NEAR(a.rect.x, 0.0f, 1e-3f);
        CHECK_NEAR(a.rect.w, 40.0f, 1e-3f);
        CHECK_NEAR(b.rect.x, 50.0f, 1e-3f); // 40 + 10
        CHECK_NEAR(b.rect.w, 100.0f, 1e-3f); // 40 + 60 leftover
        CHECK_NEAR(c.rect.x, 160.0f, 1e-3f); // 50 + 100 + 10
        CHECK_NEAR(c.rect.w, 40.0f, 1e-3f);
        CHECK_NEAR(a.rect.h, 100.0f, 1e-3f); // vertical Fill fills the area height
    }

    // HBox expand by STRETCH RATIO: two expanders 1:3 split the whole 200 (no min), sep 0.
    {
        Control a, b;
        a.hFlag = SizeFlag::Expand;
        a.stretch = 1.0f;
        b.hFlag = SizeFlag::Expand;
        b.stretch = 3.0f;
        std::vector<Control*> kids{&a, &b};
        ui::hbox(Rect{0, 0, 200, 50}, kids, 0.0f);
        CHECK_NEAR(a.rect.w, 50.0f, 1e-3f);  // 200 * 1/4
        CHECK_NEAR(b.rect.w, 150.0f, 1e-3f); // 200 * 3/4
        CHECK_NEAR(b.rect.x, 50.0f, 1e-3f);
    }

    // Cross-axis ShrinkCenter: a child keeps its min height, centered vertically in the row.
    {
        Control a;
        a.minW = 30.0f;
        a.minH = 20.0f;
        a.vFlag = SizeFlag::ShrinkCenter;
        std::vector<Control*> kids{&a};
        ui::hbox(Rect{0, 0, 100, 100}, kids, 0.0f);
        CHECK_NEAR(a.rect.y, 40.0f, 1e-3f); // (100-20)/2
        CHECK_NEAR(a.rect.h, 20.0f, 1e-3f);
    }

    // VBox: fixed header + expanding body + fixed footer, spacing 0, in a 300-tall column.
    {
        Control head, body, foot;
        head.minH = 40.0f;
        body.vFlag = SizeFlag::Expand;
        foot.minH = 30.0f;
        std::vector<Control*> kids{&head, &body, &foot};
        ui::vbox(Rect{0, 0, 120, 300}, kids, 0.0f);
        CHECK_NEAR(head.rect.y, 0.0f, 1e-3f);
        CHECK_NEAR(head.rect.h, 40.0f, 1e-3f);
        CHECK_NEAR(body.rect.y, 40.0f, 1e-3f);
        CHECK_NEAR(body.rect.h, 230.0f, 1e-3f); // 300 - 40 - 30
        CHECK_NEAR(foot.rect.y, 270.0f, 1e-3f);
        CHECK_NEAR(body.rect.w, 120.0f, 1e-3f); // horizontal Fill
    }

    // Grid: 4 Fill children in 2 columns, each min 30x20, no sep, area exactly fits.
    {
        Control g[4];
        std::vector<Control*> kids;
        for (int i = 0; i < 4; ++i) {
            g[i].minW = 30.0f;
            g[i].minH = 20.0f;
            kids.push_back(&g[i]);
        }
        ui::grid(Rect{0, 0, 60, 40}, kids, 2, 0.0f, 0.0f);
        CHECK_NEAR(g[0].rect.x, 0.0f, 1e-3f);
        CHECK_NEAR(g[0].rect.y, 0.0f, 1e-3f);
        CHECK_NEAR(g[1].rect.x, 30.0f, 1e-3f);
        CHECK_NEAR(g[1].rect.y, 0.0f, 1e-3f);
        CHECK_NEAR(g[2].rect.x, 0.0f, 1e-3f);
        CHECK_NEAR(g[2].rect.y, 20.0f, 1e-3f);
        CHECK_NEAR(g[3].rect.x, 30.0f, 1e-3f);
        CHECK_NEAR(g[3].rect.y, 20.0f, 1e-3f);
        CHECK_NEAR(g[3].rect.w, 30.0f, 1e-3f);
    }

    // Grid with an expanding column: column 1 (holding an h-Expand child) absorbs the extra width.
    {
        Control g[2];
        g[0].minW = 20.0f;
        g[0].minH = 10.0f;
        g[1].minW = 20.0f;
        g[1].minH = 10.0f;
        g[1].hFlag = SizeFlag::Expand;
        std::vector<Control*> kids{&g[0], &g[1]};
        ui::grid(Rect{0, 0, 100, 10}, kids, 2, 0.0f, 0.0f);
        CHECK_NEAR(g[0].rect.w, 20.0f, 1e-3f);  // fixed column
        CHECK_NEAR(g[1].rect.x, 20.0f, 1e-3f);
        CHECK_NEAR(g[1].rect.w, 80.0f, 1e-3f);  // 20 + 60 leftover
    }

    // Margin: per-side insets; the child fills what remains.
    {
        Control child;
        ui::margin(Rect{0, 0, 100, 100}, child, 10.0f, 20.0f, 30.0f, 40.0f);
        CHECK_NEAR(child.rect.x, 10.0f, 1e-3f);
        CHECK_NEAR(child.rect.y, 20.0f, 1e-3f);
        CHECK_NEAR(child.rect.w, 60.0f, 1e-3f); // 100 - 10 - 30
        CHECK_NEAR(child.rect.h, 40.0f, 1e-3f); // 100 - 20 - 40
    }

    // Center: the child sits at its min size in the middle of the area.
    {
        Control child;
        child.minW = 20.0f;
        child.minH = 10.0f;
        ui::center(Rect{0, 0, 100, 100}, child);
        CHECK_NEAR(child.rect.x, 40.0f, 1e-3f);
        CHECK_NEAR(child.rect.y, 45.0f, 1e-3f);
        CHECK_NEAR(child.rect.w, 20.0f, 1e-3f);
    }

    // Bottom-up min size: an HBox reports summed widths + separators, tallest height.
    {
        Control a, b;
        a.minW = 40.0f;
        a.minH = 20.0f;
        b.minW = 60.0f;
        b.minH = 30.0f;
        std::vector<Control*> kids{&a, &b};
        float w = 0.0f, h = 0.0f;
        ui::hboxMinSize(kids, 10.0f, w, h);
        CHECK_NEAR(w, 110.0f, 1e-3f); // 40 + 60 + 10 sep
        CHECK_NEAR(h, 30.0f, 1e-3f);  // tallest
        ui::vboxMinSize(kids, 10.0f, w, h);
        CHECK_NEAR(w, 60.0f, 1e-3f);  // widest
        CHECK_NEAR(h, 60.0f, 1e-3f);  // 20 + 30 + 10 sep
        ui::gridMinSize(kids, 2, 5.0f, 5.0f, w, h);
        CHECK_NEAR(w, 105.0f, 1e-3f); // 40 + 60 + 5 hsep (1 row)
        CHECK_NEAR(h, 30.0f, 1e-3f);  // single row, tallest
    }
}

void testStyleBox() {
    using ui::Border;
    using ui::Patch9;
    using ui::Rect;

    const Rect src{0, 0, 64, 64};
    const Border border{16, 16, 16, 16};

    // A 200x100 destination: nine cells laid out with fixed 16px corners + stretched middle.
    {
        const auto p = ui::ninePatch(Rect{10, 20, 200, 100}, border, src);
        // 3x3 in reading order.
        CHECK(p[0].cell == Patch9::TopLeft);
        CHECK(p[4].cell == Patch9::Center);
        CHECK(p[8].cell == Patch9::BottomRight);

        // Corners are exactly the border size in the destination, pinned to the corners.
        CHECK_NEAR(p[0].dst.x, 10.0f, 1e-4f);
        CHECK_NEAR(p[0].dst.y, 20.0f, 1e-4f);
        CHECK_NEAR(p[0].dst.w, 16.0f, 1e-4f);
        CHECK_NEAR(p[0].dst.h, 16.0f, 1e-4f);
        CHECK_NEAR(p[2].dst.x, 10.0f + 200.0f - 16.0f, 1e-4f); // top-right pinned to the right edge
        CHECK_NEAR(p[8].dst.x, 10.0f + 200.0f - 16.0f, 1e-4f);
        CHECK_NEAR(p[8].dst.y, 20.0f + 100.0f - 16.0f, 1e-4f);

        // Center + edges absorb the stretch: center is (w-32) x (h-32).
        CHECK_NEAR(p[4].dst.w, 200.0f - 32.0f, 1e-4f);
        CHECK_NEAR(p[4].dst.h, 100.0f - 32.0f, 1e-4f);
        CHECK_NEAR(p[1].dst.w, 200.0f - 32.0f, 1e-4f); // top edge stretches horizontally
        CHECK_NEAR(p[1].dst.h, 16.0f, 1e-4f);          // but keeps the border height
        CHECK_NEAR(p[3].dst.h, 100.0f - 32.0f, 1e-4f); // left edge stretches vertically
        CHECK_NEAR(p[3].dst.w, 16.0f, 1e-4f);

        // The nine dst cells tile the destination exactly (no gaps/overlap): sum of areas == dst area.
        float area = 0.0f;
        for (const auto& q : p) {
            area += q.dst.w * q.dst.h;
        }
        CHECK_NEAR(area, 200.0f * 100.0f, 1e-2f);

        // Source regions tile the source exactly too.
        float sarea = 0.0f;
        for (const auto& q : p) {
            sarea += q.src.w * q.src.h;
        }
        CHECK_NEAR(sarea, 64.0f * 64.0f, 1e-2f);
    }

    // Corners keep their size no matter the destination size (the whole point of a nine-patch).
    {
        const auto small = ui::ninePatch(Rect{0, 0, 60, 40}, border, src);
        const auto big = ui::ninePatch(Rect{0, 0, 500, 380}, border, src);
        CHECK_NEAR(small[0].dst.w, big[0].dst.w, 1e-4f); // 16 in both
        CHECK_NEAR(small[0].dst.h, big[0].dst.h, 1e-4f);
        CHECK_NEAR(big[8].dst.w, 16.0f, 1e-4f);
        // The center grows with the destination.
        CHECK(big[4].dst.w > small[4].dst.w);
    }

    // Degenerate: a destination smaller than the borders clamps middles to zero (never negative).
    {
        const auto p = ui::ninePatch(Rect{0, 0, 20, 20}, border, src);
        CHECK_NEAR(p[4].dst.w, 0.0f, 1e-4f); // center collapses
        CHECK_NEAR(p[4].dst.h, 0.0f, 1e-4f);
        for (const auto& q : p) {
            CHECK(q.dst.w >= 0.0f);
            CHECK(q.dst.h >= 0.0f);
        }
    }
}

void testTheme() {
    using ui::Corners;
    using ui::Rect;

    // roundedRectPolygon geometry.
    {
        const Rect box{10.0f, 20.0f, 200.0f, 100.0f};
        // Zero radius -> the four sharp corners, exactly (one point per corner).
        const auto sharp = ui::roundedRectPolygon(box, Corners{0.0f}, 6);
        CHECK(sharp.size() == 4);
        // Corner order: TL, TR, BR, BL.
        CHECK_NEAR(sharp[0].x, 10.0f, 1e-4f);
        CHECK_NEAR(sharp[0].y, 20.0f, 1e-4f);
        CHECK_NEAR(sharp[1].x, 210.0f, 1e-4f);
        CHECK_NEAR(sharp[1].y, 20.0f, 1e-4f);
        CHECK_NEAR(sharp[2].x, 210.0f, 1e-4f);
        CHECK_NEAR(sharp[2].y, 120.0f, 1e-4f);
        CHECK_NEAR(sharp[3].x, 10.0f, 1e-4f);
        CHECK_NEAR(sharp[3].y, 120.0f, 1e-4f);

        // Rounded: seg segments per corner -> 4*(seg+1) vertices, all inside the box bounds.
        const int seg = 6;
        const auto round = ui::roundedRectPolygon(box, Corners{12.0f}, seg);
        CHECK(round.size() == static_cast<std::size_t>(4 * (seg + 1)));
        for (const auto& p : round) {
            CHECK(p.x >= box.x - 1e-3f);
            CHECK(p.x <= box.right() + 1e-3f);
            CHECK(p.y >= box.y - 1e-3f);
            CHECK(p.y <= box.bottom() + 1e-3f);
        }
        // With a 12px radius no vertex sits in the very corner (that area is rounded away): the
        // top-left corner point (10,20) must not be present.
        bool hasSharpTL = false;
        for (const auto& p : round) {
            if (std::fabs(p.x - 10.0f) < 1e-3f && std::fabs(p.y - 20.0f) < 1e-3f) hasSharpTL = true;
        }
        CHECK(!hasSharpTL);

        // Radius clamps to half the shorter side (100/2 = 50): a huge radius yields a stadium, still
        // within bounds, still 4*(seg+1) verts.
        const auto clamped = ui::roundedRectPolygon(box, Corners{999.0f}, seg);
        CHECK(clamped.size() == static_cast<std::size_t>(4 * (seg + 1)));
        for (const auto& p : clamped) {
            CHECK(p.y >= box.y - 1e-3f);
            CHECK(p.y <= box.bottom() + 1e-3f);
        }
    }

    // StyleBoxFlat content rect insets by the margins.
    {
        ui::StyleBoxFlat s;
        s.contentMargin = ui::Border{8.0f, 6.0f, 8.0f, 10.0f};
        const Rect c = s.contentRect(Rect{0.0f, 0.0f, 100.0f, 100.0f});
        CHECK_NEAR(c.x, 8.0f, 1e-4f);
        CHECK_NEAR(c.y, 6.0f, 1e-4f);
        CHECK_NEAR(c.w, 100.0f - 16.0f, 1e-4f);
        CHECK_NEAR(c.h, 100.0f - 16.0f, 1e-4f);
    }

    // Theme registry: set/get, has, default fallback, and type/state resolution.
    {
        ui::Theme theme;
        ui::StyleBoxFlat normal;
        normal.bg = render::Color{0.2f, 0.3f, 0.4f, 1.0f};
        normal.radius = Corners{6.0f};
        ui::StyleBoxFlat hover;
        hover.bg = render::Color{0.4f, 0.5f, 0.6f, 1.0f};

        theme.setStyleBox("Button/normal", normal);
        theme.setStyleBox("Button/hover", hover);
        CHECK(theme.styleCount() == 2);
        CHECK(theme.hasStyleBox("Button/normal"));
        CHECK(!theme.hasStyleBox("Button/pressed"));

        // Exact key hit.
        CHECK_NEAR(theme.styleBox("Button/hover").bg.g, 0.5f, 1e-4f);
        // type/state resolution: pressed missing -> falls back to Button/normal.
        CHECK_NEAR(theme.styleBox("Button", "pressed").bg.b, 0.4f, 1e-4f);
        // type/state hit when present.
        CHECK_NEAR(theme.styleBox("Button", "hover").bg.b, 0.6f, 1e-4f);

        // Unknown type with no normal -> default style (the struct default bg).
        ui::StyleBoxFlat dfl;
        dfl.bg = render::Color{0.9f, 0.1f, 0.1f, 1.0f};
        theme.setDefaultStyleBox(dfl);
        CHECK_NEAR(theme.styleBox("Panel", "normal").bg.r, 0.9f, 1e-4f);
        CHECK_NEAR(theme.styleBox("missing-key").bg.r, 0.9f, 1e-4f);

        // Colors with fallback.
        theme.setColor("Button/font", render::Color{1.0f, 1.0f, 1.0f, 1.0f});
        CHECK(theme.hasColor("Button/font"));
        CHECK_NEAR(theme.color("Button/font").r, 1.0f, 1e-4f);
        CHECK_NEAR(theme.color("nope", render::Color{0.5f, 0.0f, 0.0f, 1.0f}).r, 0.5f, 1e-4f);
    }
}

void testTree() {
    // Build: A (A1, A2), B, C (C1 (C1a)).
    ui::Tree tree;
    ui::TreeItem& a = tree.add("A", 1);
    a.addChild("A1", 11);
    a.addChild("A2", 12);
    tree.add("B", 2);
    ui::TreeItem& c = tree.add("C", 3);
    ui::TreeItem& c1 = c.addChild("C1", 31);
    c1.addChild("C1a", 311);

    // Fully expanded: 7 rows depth-first with the right depths + hasChildren flags.
    {
        auto rows = tree.visibleRows();
        CHECK(rows.size() == 7);
        CHECK(tree.visibleCount() == 7);
        CHECK(rows[0].item->text == "A" && rows[0].depth == 0 && rows[0].hasChildren);
        CHECK(rows[1].item->text == "A1" && rows[1].depth == 1 && !rows[1].hasChildren);
        CHECK(rows[2].item->text == "A2" && rows[2].depth == 1);
        CHECK(rows[3].item->text == "B" && rows[3].depth == 0 && !rows[3].hasChildren);
        CHECK(rows[4].item->text == "C" && rows[4].depth == 0 && rows[4].hasChildren);
        CHECK(rows[5].item->text == "C1" && rows[5].depth == 1 && rows[5].hasChildren);
        CHECK(rows[6].item->text == "C1a" && rows[6].depth == 2 && !rows[6].hasChildren);
        // ids ride along on the items.
        CHECK(rows[6].item->id == 311);
    }

    // Collapse A: its two children vanish; A stays and is flagged collapsed+hasChildren.
    {
        a.collapsed = true;
        auto rows = tree.visibleRows();
        CHECK(rows.size() == 5); // A, B, C, C1, C1a
        CHECK(tree.visibleCount() == 5);
        CHECK(rows[0].item->text == "A" && rows[0].collapsed && rows[0].hasChildren);
        CHECK(rows[1].item->text == "B");
        CHECK(rows[2].item->text == "C");
        a.collapsed = false;
    }

    // Collapse a deeper branch (C1): hides only C1a, not C1 itself.
    {
        c1.collapsed = true;
        auto rows = tree.visibleRows();
        CHECK(rows.size() == 6); // A, A1, A2, B, C, C1
        CHECK(rows.back().item->text == "C1" && rows.back().collapsed);
        c1.collapsed = false;
    }

    // Collapsing a leaf (no children) changes nothing visible.
    {
        ui::TreeItem& a1ref = *a.children[0];
        a1ref.collapsed = true;
        CHECK(tree.visibleRows().size() == 7);
        a1ref.collapsed = false;
    }

    // Collapsing the top folder C hides its whole subtree (C1 + C1a).
    {
        c.collapsed = true;
        auto rows = tree.visibleRows();
        // A(0) A1(1) A2(2) B(3) C(4) -> 5 rows; C present but its subtree (C1, C1a) hidden.
        CHECK(rows.size() == 5);
        CHECK(rows.back().item->text == "C" && rows.back().collapsed);
        c.collapsed = false;
    }

    // Empty tree: no rows.
    {
        ui::Tree empty;
        CHECK(empty.visibleRows().empty());
        CHECK(empty.visibleCount() == 0);
    }
}

void testTextLayout() {
    using ui::TextAlign;
    using ui::TextLayout;
    // A synthetic measurer: every character (spaces included) is exactly 10px wide, so line widths are
    // perfectly predictable — text.size() * 10.
    auto measure = [](std::string_view s) { return static_cast<float>(s.size()) * 10.0f; };

    // Greedy wrap: with maxWidth 50, "aa bb" (5 chars = 50) just fits; adding " cc" (80) overflows.
    {
        const TextLayout t = ui::layoutText("aa bb cc", 50.0f, measure, 16.0f);
        CHECK(t.lines.size() == 2);
        CHECK(t.lines[0].text == "aa bb");
        CHECK(t.lines[1].text == "cc");
        CHECK_NEAR(t.lines[0].width, 50.0f, 1e-4f);
        CHECK_NEAR(t.height, 32.0f, 1e-4f); // 2 lines * 16
        CHECK_NEAR(t.width, 50.0f, 1e-4f);  // widest line
    }

    // Explicit '\n' is a hard break independent of width.
    {
        const TextLayout t = ui::layoutText("aa\nbb", 1000.0f, measure, 20.0f);
        CHECK(t.lines.size() == 2);
        CHECK(t.lines[0].text == "aa");
        CHECK(t.lines[1].text == "bb");
    }

    // A blank line (consecutive newlines) is preserved.
    {
        const TextLayout t = ui::layoutText("aa\n\nbb", 1000.0f, measure, 10.0f);
        CHECK(t.lines.size() == 3);
        CHECK(t.lines[1].text.empty());
    }

    // A single word wider than maxWidth goes on its own line (no mid-word break).
    {
        const TextLayout t = ui::layoutText("tiny enormouslylongword end", 60.0f, measure, 10.0f);
        // "tiny"(40) fits; "tiny enormouslylongword" overflows -> break; the long word alone; then "end".
        CHECK(t.lines.size() == 3);
        CHECK(t.lines[0].text == "tiny");
        CHECK(t.lines[1].text == "enormouslylongword");
        CHECK(t.lines[2].text == "end");
    }

    // Alignment: for a line of width W in a box of maxWidth M, left x=0, center x=(M-W)/2, right x=M-W.
    {
        const float M = 100.0f;
        const TextLayout left = ui::layoutText("abc", M, measure, 10.0f, TextAlign::Left);
        const TextLayout center = ui::layoutText("abc", M, measure, 10.0f, TextAlign::Center);
        const TextLayout right = ui::layoutText("abc", M, measure, 10.0f, TextAlign::Right);
        const float W = 30.0f; // "abc" = 3 * 10
        CHECK_NEAR(left.lines[0].x, 0.0f, 1e-4f);
        CHECK_NEAR(center.lines[0].x, (M - W) * 0.5f, 1e-4f);
        CHECK_NEAR(right.lines[0].x, M - W, 1e-4f);
    }

    // Line y offsets stack by lineHeight.
    {
        const TextLayout t = ui::layoutText("a\nb\nc", 1000.0f, measure, 24.0f);
        CHECK_NEAR(t.lines[0].y, 0.0f, 1e-4f);
        CHECK_NEAR(t.lines[1].y, 24.0f, 1e-4f);
        CHECK_NEAR(t.lines[2].y, 48.0f, 1e-4f);
    }

    // Empty input yields a single empty line (a paragraph with no words).
    {
        const TextLayout t = ui::layoutText("", 100.0f, measure, 10.0f);
        CHECK(t.lines.size() == 1);
        CHECK(t.lines[0].text.empty());
    }
}

void testTextInput() {
    // TextField editing: construct at end, move, insert, backspace, delete, home/end, max length.
    {
        ui::TextField f("hello");
        CHECK(f.text() == "hello");
        CHECK(f.caret() == 5); // caret starts at the end

        f.moveLeft();
        f.moveLeft(); // caret at 3 (between 'l' and 'l')
        CHECK(f.caret() == 3);
        f.insert('X'); // "helXlo", caret 4
        CHECK(f.text() == "helXlo");
        CHECK(f.caret() == 4);
        f.backspace(); // remove the 'X' -> "hello", caret 3
        CHECK(f.text() == "hello");
        CHECK(f.caret() == 3);
        f.del(); // remove the char at caret ('l') -> "helo", caret 3
        CHECK(f.text() == "helo");
        CHECK(f.caret() == 3);

        f.home();
        CHECK(f.caret() == 0);
        f.backspace(); // nothing before the start -> no-op
        CHECK(f.text() == "helo");
        f.end();
        CHECK(f.caret() == 4);
        f.del(); // nothing after the end -> no-op
        CHECK(f.text() == "helo");

        f.insert('\n'); // control chars are ignored
        f.insert('\t');
        CHECK(f.text() == "helo");

        ui::TextField g;
        g.setMaxLength(3);
        g.insert("abcdef"); // only the first 3 fit
        CHECK(g.text() == "abc");
        CHECK(g.caret() == 3);
    }

    // FocusChain: tab traversal wraps, focus() jumps, empty -> kNone.
    {
        ui::FocusChain fc;
        CHECK(fc.focused() == ui::FocusChain::kNone);
        fc.add(10);
        fc.add(20);
        fc.add(30);
        CHECK(fc.focused() == 10); // first added grabs focus
        fc.next();
        CHECK(fc.focused() == 20);
        fc.next();
        CHECK(fc.focused() == 30);
        fc.next();
        CHECK(fc.focused() == 10); // wrap forward
        fc.prev();
        CHECK(fc.focused() == 30); // wrap backward
        fc.focus(20);
        CHECK(fc.focused() == 20);
        CHECK(fc.isFocused(20));
        fc.focus(999); // unknown id -> no change
        CHECK(fc.focused() == 20);
    }

    // Context widget: clicking a field grabs focus and typed input lands only in the focused field.
    {
        ui::Context ui; // no renderer bound -> pure interaction logic (as other UI tests do)
        ui::TextField a("A"), b("B");
        ui::FocusChain focus;
        focus.add(1);
        focus.add(2); // field 1 focused initially
        const ui::Rect ra{0, 0, 100, 30}, rb{0, 40, 100, 30};

        // Type "x" with field 1 focused -> goes into a.
        {
            ui::TextEditInput e;
            e.typed = "x";
            ui.begin(500, 500, false); // pointer away, not clicking
            ui.textField(1, ra, a, focus, e);
            ui.textField(2, rb, b, focus, ui::TextEditInput{});
            ui.end();
            CHECK(a.text() == "Ax");
            CHECK(b.text() == "B");
        }
        // Click field 2 (press) -> focus moves; then next frame typing lands in b.
        {
            ui.begin(50, 55, true); // press inside rb
            ui.textField(1, ra, a, focus, ui::TextEditInput{});
            ui.textField(2, rb, b, focus, ui::TextEditInput{});
            ui.end();
            CHECK(focus.isFocused(2));

            ui::TextEditInput e;
            e.typed = "y";
            ui.begin(50, 55, false);
            ui.textField(1, ra, a, focus, ui::TextEditInput{});
            ui.textField(2, rb, b, focus, e);
            ui.end();
            CHECK(a.text() == "Ax");
            CHECK(b.text() == "By");
        }
    }
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

void testProfiler() {
    core::Profiler prof(0.5);

    // A synthetic frame with nested zones fed explicit microsecond timestamps (deterministic).
    //   frame [0..16000]
    //     update [0..6000]
    //       physics [0..3500]
    //       ai      [3500..6000]
    //     render [6000..15000]
    //       shadow [6000..8000]
    //       opaque [8000..14000]
    prof.beginFrame();
    prof.begin("frame", 0);
    prof.begin("update", 0);
    prof.begin("physics", 0);
    prof.end(3500);
    prof.begin("ai", 3500);
    prof.end(6000);
    prof.end(6000); // update
    prof.begin("render", 6000);
    prof.begin("shadow", 6000);
    prof.end(8000);
    prof.begin("opaque", 8000);
    prof.end(14000);
    prof.end(15000); // render
    prof.end(16000); // frame
    prof.endFrame();

    // Inclusive times.
    CHECK(prof.find("frame") != nullptr);
    CHECK(prof.find("frame")->inclusiveUs == 16000);
    CHECK(prof.find("update")->inclusiveUs == 6000);
    CHECK(prof.find("physics")->inclusiveUs == 3500);
    CHECK(prof.find("ai")->inclusiveUs == 2500);
    CHECK(prof.find("render")->inclusiveUs == 9000);
    CHECK(prof.find("shadow")->inclusiveUs == 2000);
    CHECK(prof.find("opaque")->inclusiveUs == 6000);

    // Self time = inclusive minus direct children.
    CHECK(prof.find("frame")->selfUs == 1000);   // 16000 - (6000 update + 9000 render)
    CHECK(prof.find("update")->selfUs == 0);      // 6000 - (3500 + 2500)
    CHECK(prof.find("render")->selfUs == 1000);   // 9000 - (2000 + 6000)
    CHECK(prof.find("physics")->selfUs == 3500);  // leaf: self == inclusive
    CHECK(prof.find("opaque")->selfUs == 6000);

    // Depth reflects nesting.
    CHECK(prof.find("frame")->depth == 0);
    CHECK(prof.find("update")->depth == 1);
    CHECK(prof.find("physics")->depth == 2);

    // Calls counted; leaf called once.
    CHECK(prof.find("physics")->calls == 1);

    // A zone entered multiple times in one frame aggregates its calls + inclusive time.
    prof.beginFrame();
    prof.begin("loop", 0);
    prof.end(100);
    prof.begin("loop", 100);
    prof.end(300);
    prof.begin("loop", 300);
    prof.end(350);
    prof.endFrame();
    CHECK(prof.find("loop")->calls == 3);
    CHECK(prof.find("loop")->inclusiveUs == 350); // 100 + 200 + 50

    // Per-frame accumulators reset: zones from the first frame read zero this frame.
    CHECK(prof.find("frame")->inclusiveUs == 0);
    CHECK(prof.find("frame")->calls == 0);

    // EMA smoothing converges toward a steady per-frame value. Feed the same 4ms zone repeatedly.
    core::Profiler ema(0.5);
    for (int i = 0; i < 12; ++i) {
        ema.beginFrame();
        ema.begin("z", 0);
        ema.end(4000); // 4 ms
        ema.endFrame();
    }
    CHECK_NEAR(ema.find("z")->smoothedMs, 4.0, 0.05);

    // Unbalanced end() (more ends than begins) is ignored rather than crashing.
    core::Profiler safe;
    safe.beginFrame();
    safe.end(10); // no open zone
    safe.endFrame();
    CHECK(safe.zones().empty());
}

void testNoise() {
    core::Noise n(1234);

    // Determinism: same seed -> identical field; a different seed diverges somewhere.
    {
        core::Noise a(7), b(7), c(8);
        bool sameAB = true, diffAC = false;
        for (int i = 0; i < 50; ++i) {
            const float x = static_cast<float>(i) * 0.37f, y = static_cast<float>(i) * 0.19f;
            if (a.noise2(x, y) != b.noise2(x, y)) sameAB = false;
            if (c.noise2(x, y) != a.noise2(x, y)) diffAC = true;
        }
        CHECK(sameAB);
        CHECK(diffAC);
    }

    // Perlin noise is exactly 0 at integer lattice points.
    for (int i = -3; i <= 3; ++i) {
        for (int j = -3; j <= 3; ++j) {
            CHECK_NEAR(n.noise2(static_cast<float>(i), static_cast<float>(j)), 0.0f, 1e-5f);
        }
    }

    // Output stays bounded (~[-1,1]); scan a dense grid.
    {
        float maxAbs = 0.0f;
        for (int i = 0; i < 400; ++i) {
            const float x = static_cast<float>(i) * 0.113f;
            for (int j = 0; j < 200; ++j) {
                const float y = static_cast<float>(j) * 0.091f;
                const float v = n.noise2(x, y);
                const float a = v < 0.0f ? -v : v;
                if (a > maxAbs) maxAbs = a;
            }
        }
        CHECK(maxAbs <= 1.0001f);
        CHECK(maxAbs > 0.4f); // it actually varies (not a flat zero field)
    }

    // Continuity: a tiny step in x changes the value only slightly (smooth, not white noise).
    {
        bool smooth = true;
        for (int i = 0; i < 100; ++i) {
            const float x = static_cast<float>(i) * 0.21f + 0.05f;
            const float y = static_cast<float>(i) * 0.13f + 0.05f;
            const float d = n.noise2(x + 0.01f, y) - n.noise2(x, y);
            if ((d < 0.0f ? -d : d) > 0.1f) smooth = false; // small input step -> small output step
        }
        CHECK(smooth);
    }

    // fbm stays bounded and is reproducible.
    {
        core::Noise a(55), b(55);
        float maxAbs = 0.0f;
        bool same = true;
        for (int i = 0; i < 300; ++i) {
            const float x = static_cast<float>(i) * 0.07f, y = static_cast<float>(i) * 0.05f;
            const float va = a.fbm2(x, y, 5);
            const float vb = b.fbm2(x, y, 5);
            if (va != vb) same = false;
            const float ab = va < 0.0f ? -va : va;
            if (ab > maxAbs) maxAbs = ab;
        }
        CHECK(same);
        CHECK(maxAbs <= 1.0001f);
    }

    // fbm with a single octave equals plain noise2 (normalization is a no-op at 1 octave).
    {
        core::Noise a(3);
        for (int i = 0; i < 20; ++i) {
            const float x = static_cast<float>(i) * 0.3f + 0.1f, y = static_cast<float>(i) * 0.2f;
            CHECK_NEAR(a.fbm2(x, y, 1), a.noise2(x, y), 1e-5f);
        }
    }
}

void testRandom() {
    // Reproducibility: the same seed yields the same stream; a different seed diverges.
    {
        core::Random a(12345), b(12345), c(99999);
        bool sameAB = true, diffAC = false;
        for (int i = 0; i < 32; ++i) {
            const uint64_t va = a.nextU64();
            if (va != b.nextU64()) sameAB = false;
            if (va != c.nextU64()) diffAC = true;
        }
        CHECK(sameAB);
        CHECK(diffAC);
    }

    // re-seed rewinds the stream.
    {
        core::Random r(7);
        const uint64_t first = r.nextU64();
        r.nextU64();
        r.nextU64();
        r.seed(7);
        CHECK(r.nextU64() == first);
    }

    // nextFloat() stays in [0, 1).
    {
        core::Random r(1);
        for (int i = 0; i < 1000; ++i) {
            const float f = r.nextFloat();
            CHECK(f >= 0.0f && f < 1.0f);
        }
    }

    // Inclusive int range: never out of bounds, and both endpoints are reachable.
    {
        core::Random r(2);
        bool hitLo = false, hitHi = false;
        bool inBounds = true;
        for (int i = 0; i < 5000; ++i) {
            const int v = r.range(3, 7);
            if (v < 3 || v > 7) inBounds = false;
            if (v == 3) hitLo = true;
            if (v == 7) hitHi = true;
        }
        CHECK(inBounds);
        CHECK(hitLo);
        CHECK(hitHi);
        CHECK(r.range(5, 5) == 5);   // degenerate range
        CHECK(r.range(9, 2) == 9);   // hi < lo -> lo
    }

    // Float range stays within [lo, hi).
    {
        core::Random r(3);
        for (int i = 0; i < 1000; ++i) {
            const float v = r.range(-2.0f, 5.0f);
            CHECK(v >= -2.0f && v < 5.0f);
        }
    }

    // chance() extremes are deterministic.
    {
        core::Random r(4);
        for (int i = 0; i < 100; ++i) {
            CHECK(!r.chance(0.0f));
            CHECK(r.chance(1.0f));
        }
    }

    // weighted(): zero-weight entries are never chosen; a heavy weight dominates.
    {
        core::Random r(5);
        std::vector<float> w{0.0f, 1.0f, 9.0f};
        int counts[3] = {0, 0, 0};
        for (int i = 0; i < 10000; ++i) counts[r.weighted(w)]++;
        CHECK(counts[0] == 0);            // zero weight never picked
        CHECK(counts[2] > counts[1]);     // 9:1 -> index 2 dominates
        CHECK(counts[1] > 0);             // but index 1 still appears
    }

    // shuffle(): the result is a permutation (same multiset) and deterministic for a fixed seed.
    {
        std::vector<int> base;
        for (int i = 0; i < 50; ++i) base.push_back(i);
        std::vector<int> a = base, b = base;
        core::Random ra(42), rb(42);
        ra.shuffle(a);
        rb.shuffle(b);
        CHECK(a == b); // deterministic
        // permutation check: sum and presence.
        long sum = 0;
        std::vector<char> seen(50, 0);
        for (int v : a) {
            sum += v;
            if (v >= 0 && v < 50) seen[static_cast<size_t>(v)] = 1;
        }
        CHECK(sum == 49 * 50 / 2);
        bool all = true;
        for (char s : seen)
            if (!s) all = false;
        CHECK(all);
        // extremely likely to differ from the identity for 50 elements.
        CHECK(a != base);
    }

    // pick() returns an element that is in the container.
    {
        core::Random r(6);
        std::vector<int> v{10, 20, 30, 40};
        for (int i = 0; i < 100; ++i) {
            const int p = r.pick(v);
            CHECK(p == 10 || p == 20 || p == 30 || p == 40);
        }
    }
}

void testScheduler() {
    // after(): fires exactly once, at (not before) the delay.
    {
        core::Scheduler s;
        int fires = 0;
        s.after(1.0, [&] { ++fires; });
        s.update(0.5);
        CHECK(fires == 0);       // not yet
        s.update(0.4);
        CHECK(fires == 0);       // 0.9 < 1.0
        s.update(0.2);
        CHECK(fires == 1);       // crossed 1.0
        s.update(5.0);
        CHECK(fires == 1);       // one-shot: never again
        CHECK(s.count() == 0);
    }

    // every() with a finite repeat count fires exactly that many times.
    {
        core::Scheduler s;
        int fires = 0;
        s.every(1.0, [&] { ++fires; }, 3);
        for (int i = 0; i < 10; ++i) s.update(1.0);
        CHECK(fires == 3);
        CHECK(s.count() == 0);
    }

    // every() forever keeps firing; a big dt catches up across multiple intervals.
    {
        core::Scheduler s;
        int fires = 0;
        s.every(0.5, [&] { ++fires; }, -1);
        s.update(2.0); // spans four 0.5s intervals
        CHECK(fires == 4);
        CHECK(s.count() == 1); // still live
    }

    // cancel() stops a pending timer before it fires.
    {
        core::Scheduler s;
        int fires = 0;
        auto h = s.after(1.0, [&] { ++fires; });
        CHECK(s.cancel(h));
        s.update(2.0);
        CHECK(fires == 0);
        CHECK(!s.cancel(h)); // already gone
    }

    // Multiple timers fire; a callback may schedule another without disturbing the current update.
    {
        core::Scheduler s;
        int a = 0, b = 0;
        s.after(0.5, [&] {
            ++a;
            s.after(0.5, [&] { ++b; }); // scheduled mid-update; fires on a later tick
        });
        s.update(1.0);
        CHECK(a == 1);
        CHECK(b == 0); // the newly-added timer waits for the next update
        s.update(0.6);
        CHECK(b == 1);
    }
}

void testSequence() {
    // wait -> call: the call fires only after the wait elapses.
    {
        core::Sequence seq;
        int calls = 0;
        seq.wait(1.0).call([&] { ++calls; });
        seq.update(0.5);
        CHECK(calls == 0);
        seq.update(0.6); // crosses 1.0, then the Call step runs immediately
        CHECK(calls == 1);
        CHECK(seq.done());
    }

    // span drives progress 0..1 and ends at exactly 1.0.
    {
        core::Sequence seq;
        float last = -1.0f;
        seq.span(2.0, [&](float p) { last = p; });
        seq.update(0.5);
        CHECK_NEAR(last, 0.25f, 1e-4f);
        seq.update(0.5);
        CHECK_NEAR(last, 0.5f, 1e-4f);
        seq.update(2.0); // overshoot -> clamps to 1.0 and finishes
        CHECK_NEAR(last, 1.0f, 1e-4f);
        CHECK(seq.done());
    }

    // Leftover time carries from one step into the next within a single update.
    {
        core::Sequence seq;
        int calls = 0;
        seq.wait(1.0).call([&] { ++calls; }).wait(1.0).call([&] { ++calls; });
        seq.update(2.5); // covers wait, call, wait, call all at once
        CHECK(calls == 2);
        CHECK(seq.done());
    }

    // loop() restarts after the last step.
    {
        core::Sequence seq;
        int calls = 0;
        seq.wait(1.0).call([&] { ++calls; }).loop();
        for (int i = 0; i < 5; ++i) seq.update(1.0);
        CHECK(calls >= 4);   // fires each loop
        CHECK(!seq.done());  // a looping sequence never reports done
    }

    // reset() rewinds to the start.
    {
        core::Sequence seq;
        int calls = 0;
        seq.wait(1.0).call([&] { ++calls; });
        seq.update(1.0);
        CHECK(calls == 1 && seq.done());
        seq.reset();
        CHECK(!seq.done());
        seq.update(1.0);
        CHECK(calls == 2);
    }
}

void testCameraController() {
    using game::CameraController2D;

    // No deadzone: snap() puts the focus exactly on the target.
    {
        CameraController2D cam;
        cam.setSmoothing(0.0f);
        cam.follow({100.0f, 50.0f});
        cam.snap();
        CHECK_NEAR(cam.position().x, 100.0f, 1e-4f);
        CHECK_NEAR(cam.position().y, 50.0f, 1e-4f);
    }

    // Deadzone: a target inside the box does NOT move the camera; outside moves it to the box edge.
    {
        CameraController2D cam;
        cam.setDeadzone({40.0f, 30.0f});
        // Start focused at origin; target 20 to the right is inside the 40 half-width box.
        cam.follow({20.0f, 0.0f});
        cam.snap();
        CHECK_NEAR(cam.position().x, 0.0f, 1e-4f); // unchanged
        // Target 100 to the right is outside; camera moves so target sits on the +40 edge => center 60.
        cam.follow({100.0f, 0.0f});
        cam.snap();
        CHECK_NEAR(cam.position().x, 60.0f, 1e-4f);
    }

    // Smoothing eases toward the desired focus (frame-rate-independent), never overshooting.
    {
        CameraController2D cam;
        cam.setSmoothing(10.0f);
        cam.follow({100.0f, 0.0f});
        float prev = 0.0f;
        for (int i = 0; i < 5; ++i) {
            cam.update(1.0f / 60.0f);
            const float x = cam.position().x;
            CHECK(x > prev);       // moving toward target
            CHECK(x < 100.0f + 1e-3f); // never past it
            prev = x;
        }
        // A very long step effectively arrives.
        cam.update(100.0f);
        CHECK_NEAR(cam.position().x, 100.0f, 1e-2f);
    }

    // World bounds clamp so the view rectangle stays inside the level.
    {
        CameraController2D cam;
        cam.setViewport(320.0f, 180.0f, 1.0f); // viewHalf = (160, 90)
        cam.setBounds({0.0f, 0.0f}, {1000.0f, 1000.0f});
        cam.setSmoothing(0.0f);
        // Target at the origin corner: center clamps to (160, 90).
        cam.follow({0.0f, 0.0f});
        cam.snap();
        CHECK_NEAR(cam.position().x, 160.0f, 1e-4f);
        CHECK_NEAR(cam.position().y, 90.0f, 1e-4f);
        // Far corner clamps to (1000-160, 1000-90) = (840, 910).
        cam.follow({5000.0f, 5000.0f});
        cam.snap();
        CHECK_NEAR(cam.position().x, 840.0f, 1e-4f);
        CHECK_NEAR(cam.position().y, 910.0f, 1e-4f);
        // Interior target passes through unclamped.
        cam.follow({500.0f, 500.0f});
        cam.snap();
        CHECK_NEAR(cam.position().x, 500.0f, 1e-4f);
    }

    // World smaller than the view on an axis: that axis is centered on the world.
    {
        CameraController2D cam;
        cam.setViewport(320.0f, 180.0f, 1.0f); // viewHalf.x = 160
        cam.setBounds({0.0f, 0.0f}, {100.0f, 2000.0f}); // world width 100 < view width 320
        cam.setSmoothing(0.0f);
        cam.follow({999.0f, 500.0f});
        cam.snap();
        CHECK_NEAR(cam.position().x, 50.0f, 1e-4f); // centered on [0,100]
        CHECK_NEAR(cam.position().y, 500.0f, 1e-4f); // y large enough, clamped normally
    }

    // Shake offset shifts the reported center but not the follow position.
    {
        CameraController2D cam;
        cam.setSmoothing(0.0f);
        cam.follow({200.0f, 200.0f});
        cam.snap();
        cam.setShakeOffset({8.0f, -4.0f});
        CHECK_NEAR(cam.position().x, 200.0f, 1e-4f); // follow center unaffected
        CHECK_NEAR(cam.center().x, 208.0f, 1e-4f);   // camera center includes shake
        CHECK_NEAR(cam.center().y, 196.0f, 1e-4f);
    }

    // worldToScreen: the camera center maps to the screen center; offsets scale by zoom.
    {
        CameraController2D cam;
        cam.setViewport(800.0f, 600.0f, 2.0f);
        cam.setSmoothing(0.0f);
        cam.follow({100.0f, 100.0f});
        cam.snap();
        math::vec2 s = cam.worldToScreen({100.0f, 100.0f}, 800.0f, 600.0f);
        CHECK_NEAR(s.x, 400.0f, 1e-3f);
        CHECK_NEAR(s.y, 300.0f, 1e-3f);
        math::vec2 s2 = cam.worldToScreen({110.0f, 100.0f}, 800.0f, 600.0f);
        CHECK_NEAR(s2.x, 420.0f, 1e-3f); // +10 world * zoom 2 = +20 px
    }
}

void testTransformGraph() {
    using scene::Transform2D;
    using scene::TransformGraph;
    const float PI = 3.14159265358979f;

    TransformGraph g;

    // Root at (100,100); child offset (10,0) in local space.
    TransformGraph::Node root = g.create(TransformGraph::kInvalid, Transform2D{{100.0f, 100.0f}, 0.0f, {1, 1}});
    TransformGraph::Node child = g.create(root, Transform2D{{10.0f, 0.0f}, 0.0f, {1, 1}});
    g.update();
    CHECK_NEAR(g.worldPosition(child).x, 110.0f, 1e-4f);
    CHECK_NEAR(g.worldPosition(child).y, 100.0f, 1e-4f);

    // Rotate the root 90 degrees: the child's offset sweeps to +Y.
    g.local(root).rotation = PI * 0.5f;
    g.update();
    CHECK_NEAR(g.worldPosition(child).x, 100.0f, 1e-3f);
    CHECK_NEAR(g.worldPosition(child).y, 110.0f, 1e-3f);
    CHECK_NEAR(g.worldRotation(child), PI * 0.5f, 1e-4f); // rotation inherited (sum)

    // Scale on the root scales the child's offset (2x -> offset 20).
    g.local(root).rotation = 0.0f;
    g.local(root).scale = {2.0f, 2.0f};
    g.update();
    CHECK_NEAR(g.worldPosition(child).x, 120.0f, 1e-4f);
    CHECK_NEAR(g.worldScale(child).x, 2.0f, 1e-4f);
    g.local(root).scale = {1.0f, 1.0f};

    // Nested grandchild: root -> child (10,0) -> grand (5,0). Rotate child 90 -> grand offset sweeps.
    TransformGraph::Node grand = g.create(child, Transform2D{{5.0f, 0.0f}, 0.0f, {1, 1}});
    g.local(root).rotation = 0.0f;
    g.local(child).rotation = PI * 0.5f;
    g.update();
    // child world pos = (110,100); grand = child + rotate((5,0), 90) = (110, 105).
    CHECK_NEAR(g.worldPosition(grand).x, 110.0f, 1e-3f);
    CHECK_NEAR(g.worldPosition(grand).y, 105.0f, 1e-3f);
    g.local(child).rotation = 0.0f;

    // localToWorld maps a point in a node's local space through its world transform.
    g.update();
    math::vec2 w = g.localToWorld(child, math::vec2{0.0f, 0.0f});
    CHECK_NEAR(w.x, 110.0f, 1e-4f); // node origin == its world position
    CHECK_NEAR(w.y, 100.0f, 1e-4f);
    math::vec2 w2 = g.localToWorld(child, math::vec2{3.0f, 0.0f});
    CHECK_NEAR(w2.x, 113.0f, 1e-4f);

    // Reparent grand under root directly; its world position now derives from root only.
    g.setParent(grand, root);
    g.local(grand).position = {7.0f, 0.0f};
    g.update();
    CHECK_NEAR(g.worldPosition(grand).x, 107.0f, 1e-4f);

    // Creation order independent of parent order: create parent AFTER child index-wise via reparent.
    TransformGraph g2;
    TransformGraph::Node a = g2.create(); // will be child
    TransformGraph::Node b = g2.create(TransformGraph::kInvalid, Transform2D{{50.0f, 0.0f}, 0.0f, {1, 1}});
    g2.setParent(a, b);
    g2.local(a).position = {5.0f, 0.0f};
    g2.update();
    CHECK_NEAR(g2.worldPosition(a).x, 55.0f, 1e-4f); // resolved even though child has lower index

    CHECK(g.size() == 3);
}

void testGrid3D() {
    using math::vec3;
    using math::vec4;
    using render::buildGrid;
    using render::GridSpec;
    using render::Line3;

    // A 2-division grid + axes: (2*2+1) lines each way = 5*2 = 10 grid lines, + 3 gizmo axes = 13.
    {
        GridSpec s;
        s.divisions = 2;
        s.spacing = 1.0f;
        s.gizmoAxes = true;
        s.axisLength = 3.0f;
        const std::vector<Line3> g = buildGrid(s);
        CHECK(g.size() == 13);

        // The grid lines span +/- 2 (ext = divisions*spacing). Measure only the 10 grid lines — the
        // last 3 lines are the gizmo axes, whose +X leg reaches x=3 and would pollute the span.
        float minX = 1e9f, maxX = -1e9f;
        for (std::size_t i = 0; i + 3 < g.size(); ++i) {
            minX = std::min({minX, g[i].a.x, g[i].b.x});
            maxX = std::max({maxX, g[i].a.x, g[i].b.x});
        }
        CHECK_NEAR(minX, -2.0f, 1e-4f);
        CHECK_NEAR(maxX, 2.0f, 1e-4f);

        // The center line parallel to X (z=0, x from -2..2) carries the axisColor, not the minor color.
        bool foundCenter = false;
        for (const Line3& l : g) {
            if (std::fabs(l.a.z) < 1e-5f && std::fabs(l.b.z) < 1e-5f && l.a.y == 0.0f &&
                std::fabs(l.a.x + 2.0f) < 1e-5f) {
                foundCenter = true;
                CHECK_NEAR(l.color.x, s.axisColor.x, 1e-5f);
            }
        }
        CHECK(foundCenter);

        // RGB gizmo axes: +X is red, +Y green, +Z blue, each from the origin.
        const Line3& xAxis = g[g.size() - 3];
        const Line3& yAxis = g[g.size() - 2];
        const Line3& zAxis = g[g.size() - 1];
        CHECK_NEAR(xAxis.b.x, 3.0f, 1e-5f);
        CHECK(xAxis.color.x > xAxis.color.y && xAxis.color.x > xAxis.color.z); // red dominant
        CHECK_NEAR(yAxis.b.y, 3.0f, 1e-5f);
        CHECK(yAxis.color.y > yAxis.color.x && yAxis.color.y > yAxis.color.z); // green dominant
        CHECK_NEAR(zAxis.b.z, 3.0f, 1e-5f);
        CHECK(zAxis.color.z > zAxis.color.x && zAxis.color.z > zAxis.color.y); // blue dominant
    }

    // gizmoAxes off → only the grid lines.
    {
        GridSpec s;
        s.divisions = 3;
        s.gizmoAxes = false;
        CHECK(buildGrid(s).size() == static_cast<std::size_t>((2 * 3 + 1) * 2));
    }

    // A wire box has 12 edges bounded exactly by [min, max].
    {
        const std::vector<Line3> box =
            render::buildWireBox(vec3(-1, 0, -2), vec3(1, 3, 2), vec4(1, 1, 1, 1));
        CHECK(box.size() == 12);
        float mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f;
        for (const Line3& l : box) {
            mnx = std::min({mnx, l.a.x, l.b.x});
            mxx = std::max({mxx, l.a.x, l.b.x});
            mny = std::min({mny, l.a.y, l.b.y});
            mxy = std::max({mxy, l.a.y, l.b.y});
        }
        CHECK_NEAR(mnx, -1.0f, 1e-4f);
        CHECK_NEAR(mxx, 1.0f, 1e-4f);
        CHECK_NEAR(mny, 0.0f, 1e-4f);
        CHECK_NEAR(mxy, 3.0f, 1e-4f);
    }
}

void testShapes3D() {
    namespace sh = render::shapes;
    const render::Color white{1, 1, 1, 1};

    // Bounds helper over a MeshData's vertices.
    auto bounds = [](const sh::MeshData& m, math::vec3& mn, math::vec3& mx) {
        mn = math::vec3(1e9f);
        mx = math::vec3(-1e9f);
        for (const render::MeshVertex& v : m.vertices) {
            mn.x = std::min(mn.x, v.px);
            mn.y = std::min(mn.y, v.py);
            mn.z = std::min(mn.z, v.pz);
            mx.x = std::max(mx.x, v.px);
            mx.y = std::max(mx.y, v.py);
            mx.z = std::max(mx.z, v.pz);
        }
    };
    // All normals unit length?
    auto normalsUnit = [](const sh::MeshData& m) {
        for (const render::MeshVertex& v : m.vertices) {
            const float len = std::sqrt(v.nx * v.nx + v.ny * v.ny + v.nz * v.nz);
            if (std::fabs(len - 1.0f) > 1e-3f) {
                return false;
            }
        }
        return true;
    };
    // Every index in range and triangle-count a multiple of 3?
    auto indicesValid = [](const sh::MeshData& m) {
        if (m.indices.size() % 3 != 0) {
            return false;
        }
        const uint32_t n = static_cast<uint32_t>(m.vertices.size());
        for (uint32_t idx : m.indices) {
            if (idx >= n) {
                return false;
            }
        }
        return true;
    };

    // Cylinder: radius 2, height 6 -> spans x,z in [-2,2], y in [-3,3].
    {
        const sh::MeshData m = sh::makeCylinder(2.0f, 6.0f, 24, white);
        CHECK(!m.vertices.empty());
        CHECK(indicesValid(m));
        CHECK(normalsUnit(m));
        math::vec3 mn, mx;
        bounds(m, mn, mx);
        CHECK_NEAR(mn.y, -3.0f, 1e-4f);
        CHECK_NEAR(mx.y, 3.0f, 1e-4f);
        CHECK_NEAR(mx.x, 2.0f, 1e-3f);
        CHECK_NEAR(mn.x, -2.0f, 1e-3f);
    }

    // Cone: base radius 1.5, height 4 -> y in [-2,2], apex at +2 (a single highest vertex at x=z=0).
    {
        const sh::MeshData m = sh::makeCone(1.5f, 4.0f, 20, white);
        CHECK(indicesValid(m));
        CHECK(normalsUnit(m));
        math::vec3 mn, mx;
        bounds(m, mn, mx);
        CHECK_NEAR(mx.y, 2.0f, 1e-4f);
        CHECK_NEAR(mn.y, -2.0f, 1e-4f);
        CHECK_NEAR(mx.x, 1.5f, 1e-3f);
    }

    // Torus: major 3, minor 1 -> outer radius 4, inner radius 2; y in [-1,1].
    {
        const sh::MeshData m = sh::makeTorus(3.0f, 1.0f, 24, 12, white);
        CHECK(indicesValid(m));
        CHECK(normalsUnit(m));
        math::vec3 mn, mx;
        bounds(m, mn, mx);
        CHECK_NEAR(mx.x, 4.0f, 1e-3f);   // major + minor
        CHECK_NEAR(mn.x, -4.0f, 1e-3f);
        CHECK_NEAR(mx.y, 1.0f, 1e-3f);   // minor radius
        CHECK_NEAR(mn.y, -1.0f, 1e-3f);
    }

    // Capsule: radius 1, cyl height 4 -> total height 6 (y in [-3,3]); x,z span the radius.
    {
        const sh::MeshData m = sh::makeCapsule(1.0f, 4.0f, 20, 6, white);
        CHECK(indicesValid(m));
        CHECK(normalsUnit(m));
        math::vec3 mn, mx;
        bounds(m, mn, mx);
        CHECK_NEAR(mx.y, 3.0f, 1e-3f);   // cyl half (2) + hemisphere radius (1)
        CHECK_NEAR(mn.y, -3.0f, 1e-3f);
        CHECK_NEAR(mx.x, 1.0f, 1e-3f);
    }

    // Degenerate segment counts are clamped, not crashy.
    {
        const sh::MeshData m = sh::makeCylinder(1.0f, 1.0f, 1, white);
        CHECK(m.vertices.size() > 0);
        CHECK(indicesValid(m));
    }
}

void testPolyline() {
    using math::vec2;
    using render::buildPolyline;
    using render::CapMode;
    using render::JointMode;
    using render::PolylineStyle;

    auto bbox = [](const std::vector<vec2>& v, vec2& mn, vec2& mx) {
        mn = vec2(1e9f, 1e9f);
        mx = vec2(-1e9f, -1e9f);
        for (const vec2& p : v) {
            mn.x = std::min(mn.x, p.x);
            mn.y = std::min(mn.y, p.y);
            mx.x = std::max(mx.x, p.x);
            mx.y = std::max(mx.y, p.y);
        }
    };

    // A single horizontal segment, width 4 (half 2), no caps → one rectangle = 2 triangles = 6 verts.
    {
        PolylineStyle s;
        s.width = 4.0f;
        s.cap = CapMode::None;
        const std::vector<vec2> tris = buildPolyline({vec2(0, 0), vec2(10, 0)}, s);
        CHECK(tris.size() == 6);
        vec2 mn, mx;
        bbox(tris, mn, mx);
        CHECK_NEAR(mn.x, 0.0f, 1e-4f);
        CHECK_NEAR(mx.x, 10.0f, 1e-4f);
        CHECK_NEAR(mn.y, -2.0f, 1e-4f);
        CHECK_NEAR(mx.y, 2.0f, 1e-4f);
    }

    // Box caps extend the ribbon half a width past each end (x: -2 … 12).
    {
        PolylineStyle s;
        s.width = 4.0f;
        s.cap = CapMode::Box;
        const std::vector<vec2> tris = buildPolyline({vec2(0, 0), vec2(10, 0)}, s);
        vec2 mn, mx;
        bbox(tris, mn, mx);
        CHECK_NEAR(mn.x, -2.0f, 1e-4f);
        CHECK_NEAR(mx.x, 12.0f, 1e-4f);
    }

    // Round caps add a fan, so more triangles than box, but stay within the half-width radius.
    {
        PolylineStyle s;
        s.width = 4.0f;
        s.cap = CapMode::Round;
        const std::vector<vec2> tris = buildPolyline({vec2(0, 0), vec2(10, 0)}, s);
        CHECK(tris.size() > 6);
        vec2 mn, mx;
        bbox(tris, mn, mx);
        CHECK_NEAR(mn.x, -2.0f, 1e-3f); // furthest cap point reaches -radius
        CHECK_NEAR(mx.x, 12.0f, 1e-3f);
    }

    // A right-angle bevel joint: 2 segment rectangles (4 tris) + 1 bevel triangle = 5 tris = 15 verts.
    {
        PolylineStyle s;
        s.width = 4.0f;
        s.joint = JointMode::Bevel;
        s.cap = CapMode::None;
        const std::vector<vec2> tris = buildPolyline({vec2(0, 0), vec2(10, 0), vec2(10, 10)}, s);
        CHECK(tris.size() == 15);
    }

    // Miter joint on a 90° corner (within the limit) reaches the outer apex at (12, -2).
    {
        PolylineStyle s;
        s.width = 4.0f;
        s.joint = JointMode::Miter;
        s.cap = CapMode::None;
        const std::vector<vec2> tris = buildPolyline({vec2(0, 0), vec2(10, 0), vec2(10, 10)}, s);
        float best = 1e9f;
        for (const vec2& p : tris) {
            best = std::min(best, std::fabs(p.x - 12.0f) + std::fabs(p.y + 2.0f));
        }
        CHECK(best < 1e-3f); // an emitted vertex sits at the miter apex
    }

    // A closed square loop: 4 segment rectangles (8 tris) + 4 bevel joints (4 tris) = 12 tris = 36 verts.
    {
        PolylineStyle s;
        s.width = 2.0f;
        s.joint = JointMode::Bevel;
        s.closed = true;
        const std::vector<vec2> tris =
            buildPolyline({vec2(0, 0), vec2(10, 0), vec2(10, 10), vec2(0, 10)}, s);
        CHECK(tris.size() == 36);
    }

    // Degenerate input (fewer than 2 points) yields nothing.
    {
        PolylineStyle s;
        CHECK(buildPolyline({vec2(1, 1)}, s).empty());
        CHECK(buildPolyline({}, s).empty());
    }
}

void testPrefab() {
    using scene::Prefab;
    using scene::PrefabNode;
    using scene::PropValue;

    // Build a "turret" template: root chassis (hp, pos) with a child "gun" (dmg).
    Prefab turret;
    turret.root.name = "chassis";
    scene::setProp(turret.root.props, "hp", PropValue::makeInt(100));
    turret.root.props.emplace_back("pos", PropValue::makeVec2(math::vec2(0.0f, 0.0f)));
    PrefabNode gun;
    gun.name = "gun";
    scene::setProp(gun.props, "dmg", PropValue::makeFloat(10.0f));
    turret.root.children.push_back(gun);

    CHECK(scene::nodeCount(turret.root) == 2);

    // Instantiate with no overrides → defaults carried through.
    {
        PrefabNode inst = scene::instantiate(turret);
        CHECK(inst.name == "chassis");
        CHECK(scene::getInt(inst.props, "hp") == 100);
        PrefabNode* g = scene::findNode(inst, "gun");
        CHECK(g != nullptr);
        CHECK_NEAR(scene::getFloat(g->props, "dmg"), 10.0f, 1e-5f);
    }

    // Override the root's hp + a child's dmg; the other keys keep their defaults.
    {
        scene::OverrideMap ov;
        ov.push_back({"", {{"hp", PropValue::makeInt(250)}}});
        ov.push_back({"gun", {{"dmg", PropValue::makeFloat(25.0f)}}});
        PrefabNode inst = scene::instantiate(turret, ov);
        CHECK(scene::getInt(inst.props, "hp") == 250);
        CHECK_NEAR(scene::getVec2(inst.props, "pos").x, 0.0f, 1e-5f); // untouched default
        CHECK_NEAR(scene::getFloat(scene::findNode(inst, "gun")->props, "dmg"), 25.0f, 1e-5f);
    }

    // An override may ADD a key not present in the template.
    {
        scene::OverrideMap ov;
        ov.push_back({"", {{"tint", PropValue::makeColor(math::vec4(1, 0, 0, 1))}}});
        PrefabNode inst = scene::instantiate(turret, ov);
        const PropValue* t = scene::findProp(inst.props, "tint");
        CHECK(t != nullptr);
        CHECK(t->type == PropValue::Type::Color);
        CHECK_NEAR(t->color.x, 1.0f, 1e-5f);
    }

    // Instances are INDEPENDENT: overriding one never mutates the template or a sibling instance.
    {
        scene::OverrideMap ov;
        ov.push_back({"", {{"hp", PropValue::makeInt(1)}}});
        PrefabNode a = scene::instantiate(turret, ov);
        PrefabNode b = scene::instantiate(turret); // no overrides
        CHECK(scene::getInt(a.props, "hp") == 1);
        CHECK(scene::getInt(b.props, "hp") == 100);       // sibling unaffected
        CHECK(scene::getInt(turret.root.props, "hp") == 100); // template unaffected
    }

    // Unknown override paths are ignored (no crash, no effect).
    {
        scene::OverrideMap ov;
        ov.push_back({"does/not/exist", {{"x", PropValue::makeFloat(9.0f)}}});
        PrefabNode inst = scene::instantiate(turret, ov);
        CHECK(scene::getInt(inst.props, "hp") == 100);
        CHECK(scene::findNode(inst, "missing") == nullptr);
    }
}

void testPrefabText() {
    using scene::Prefab;
    using scene::PrefabNode;
    using scene::PropValue;

    // Build a small tree: chassis (int hp, vec2 pos) → turret (color) → barrel (float len).
    Prefab pf;
    pf.root.name = "chassis";
    scene::setProp(pf.root.props, "hp", PropValue::makeInt(100));
    scene::setProp(pf.root.props, "pos", PropValue::makeVec2(math::vec2(3.0f, 4.0f)));
    PrefabNode turret;
    turret.name = "turret";
    scene::setProp(turret.props, "color", PropValue::makeColor(math::vec4(0.5f, 0.25f, 0.75f, 1.0f)));
    PrefabNode barrel;
    barrel.name = "barrel";
    scene::setProp(barrel.props, "len", PropValue::makeFloat(46.0f));
    turret.children.push_back(barrel);
    pf.root.children.push_back(turret);

    const std::string text = io::savePrefabText(pf);

    // The text has the expected node sections + typed property lines.
    CHECK(text.find("[node name=\"chassis\"]") != std::string::npos);
    CHECK(text.find("hp = int 100") != std::string::npos);
    CHECK(text.find("[node name=\"turret\" parent=\".\"]") != std::string::npos);
    CHECK(text.find("[node name=\"barrel\" parent=\"turret\"]") != std::string::npos);
    CHECK(text.find("len = float 46") != std::string::npos);

    // Parse it back into an identical tree.
    Prefab loaded;
    CHECK(io::loadPrefabText(text, loaded));
    CHECK(loaded.root.name == "chassis");
    CHECK(scene::nodeCount(loaded.root) == 3);
    CHECK(scene::getInt(loaded.root.props, "hp") == 100);
    CHECK_NEAR(scene::getVec2(loaded.root.props, "pos").x, 3.0f, 1e-4f);
    PrefabNode* lt = scene::findNode(loaded.root, "turret");
    CHECK(lt != nullptr);
    CHECK_NEAR(scene::getColor(lt->props, "color").z, 0.75f, 1e-4f);
    PrefabNode* lb = scene::findNode(loaded.root, "turret/barrel");
    CHECK(lb != nullptr);
    CHECK_NEAR(scene::getFloat(lb->props, "len"), 46.0f, 1e-4f);

    // Round-trip is idempotent: re-serializing the loaded tree yields identical text.
    CHECK(io::savePrefabText(loaded) == text);

    // Every property type survives a round-trip.
    {
        Prefab one;
        one.root.name = "n";
        scene::setProp(one.root.props, "f", PropValue::makeFloat(1.5f));
        scene::setProp(one.root.props, "i", PropValue::makeInt(-7));
        scene::setProp(one.root.props, "b", PropValue::makeBool(true));
        scene::setProp(one.root.props, "v", PropValue::makeVec2(math::vec2(2.0f, -3.0f)));
        scene::setProp(one.root.props, "c", PropValue::makeColor(math::vec4(0.1f, 0.2f, 0.3f, 0.4f)));
        scene::setProp(one.root.props, "t", PropValue::makeText("hello world"));
        Prefab back;
        CHECK(io::loadPrefabText(io::savePrefabText(one), back));
        CHECK_NEAR(scene::getFloat(back.root.props, "f"), 1.5f, 1e-4f);
        CHECK(scene::getInt(back.root.props, "i") == -7);
        CHECK(scene::getBool(back.root.props, "b") == true);
        CHECK_NEAR(scene::getVec2(back.root.props, "v").y, -3.0f, 1e-4f);
        CHECK_NEAR(scene::getColor(back.root.props, "c").w, 0.4f, 1e-4f);
        const PropValue* tp = scene::findProp(back.root.props, "t");
        CHECK(tp != nullptr && tp->text == "hello world");
    }

    // Empty text yields no root (load fails cleanly).
    {
        Prefab empty;
        CHECK(!io::loadPrefabText("", empty));
    }
}

void testLocalization() {
    // --- CSV parser ---
    // Basic grid.
    {
        const auto rows = io::parseCsv("a,b,c\nd,e,f");
        CHECK(rows.size() == 2);
        CHECK(rows[0].size() == 3);
        CHECK(rows[0][0] == "a" && rows[0][2] == "c");
        CHECK(rows[1][1] == "e");
    }
    // Quoted field keeps an embedded delimiter.
    {
        const auto rows = io::parseCsv("\"x,y\",z");
        CHECK(rows.size() == 1);
        CHECK(rows[0].size() == 2);
        CHECK(rows[0][0] == "x,y");
        CHECK(rows[0][1] == "z");
    }
    // Escaped quotes ("" -> ").
    {
        const auto rows = io::parseCsv("\"she said \"\"hi\"\"\",z");
        CHECK(rows[0][0] == "she said \"hi\"");
        CHECK(rows[0][1] == "z");
    }
    // Empty fields are preserved.
    {
        const auto rows = io::parseCsv("a,,c");
        CHECK(rows[0].size() == 3);
        CHECK(rows[0][1].empty());
    }
    // CRLF endings + a trailing newline does not add a blank row.
    {
        const auto rows = io::parseCsv("a,b\r\nc,d\r\n");
        CHECK(rows.size() == 2);
        CHECK(rows[1][0] == "c" && rows[1][1] == "d");
    }
    // A quoted field may contain a newline.
    {
        const auto rows = io::parseCsv("\"line1\nline2\",z");
        CHECK(rows.size() == 1);
        CHECK(rows[0][0] == "line1\nline2");
    }

    // --- TranslationTable ---
    {
        const char* csv =
            "keys,en,es,fr\n"
            "GREET,Hello,Hola,Bonjour\n"
            "BYE,Goodbye,Adios,\n"      // fr cell empty -> falls back to source (en)
            "NEW_GAME,New Game,Nuevo Juego,Nouvelle Partie\n";
        io::TranslationTable t;
        CHECK(t.loadCsv(csv));
        CHECK(t.count() == 3);
        CHECK(t.locales().size() == 3);
        CHECK(t.locales()[0] == "en");

        // Default active locale is the first (en).
        CHECK(t.locale() == "en");
        CHECK(t.tr("GREET") == "Hello");

        // Switch locale.
        t.setLocale("es");
        CHECK(t.locale() == "es");
        CHECK(t.tr("GREET") == "Hola");
        CHECK(t.tr("NEW_GAME") == "Nuevo Juego");

        // Empty fr cell falls back to the source language (en).
        CHECK(t.tr("BYE", "fr") == "Goodbye");
        // A named-locale lookup independent of the active one.
        CHECK(t.tr("GREET", "fr") == "Bonjour");

        // Unknown key returns the key itself (visible missing-string marker).
        CHECK(t.tr("MISSING") == "MISSING");
        CHECK(!t.hasKey("MISSING"));
        CHECK(t.hasKey("GREET"));

        // Unknown locale keeps the current active locale.
        t.setLocale("de");
        CHECK(t.locale() == "es"); // unchanged
    }

    // loadCsv rejects a header without any locale column.
    {
        io::TranslationTable t;
        CHECK(!t.loadCsv("keys\n"));
        CHECK(!t.loadCsv(""));
    }
}

void testActionMap() {
    using input::Device;
    input::ActionMap map;

    // "Jump" bound to two sources (a key and a pad button) — down if EITHER is down.
    map.bindButton("Jump", Device::Key, 44);       // e.g. space
    map.bindButton("Jump", Device::PadButton, 0);  // e.g. pad A
    // "Fire" bound to a mouse button.
    map.bindButton("Fire", Device::MouseButton, 1);
    // "MoveX" from an A/D key pair plus an analog pad axis 0.
    map.bindAxisPair("MoveX", Device::Key, 4 /*A*/, Device::Key, 7 /*D*/);
    map.bindAxisAnalog("MoveX", 0, 1.0f);
    // "AimY" from an inverted analog axis only.
    map.bindAxisAnalog("AimY", 1, -1.0f);

    CHECK(map.hasButton("Jump"));
    CHECK(map.hasAxis("MoveX"));
    CHECK(!map.hasButton("Nope"));

    // A tiny synthetic input state we can mutate between frames.
    struct State {
        bool key[256] = {};
        bool pad[16] = {};
        bool mouse[8] = {};
        float axis[4] = {};
    } st;
    auto down = [&](Device d, int code) {
        switch (d) {
        case Device::Key: return st.key[code];
        case Device::PadButton: return st.pad[code];
        case Device::MouseButton: return st.mouse[code];
        }
        return false;
    };
    auto analog = [&](int ax) { return st.axis[ax]; };

    // Frame 1: nothing down.
    map.update(down, analog);
    CHECK(!map.held("Jump"));
    CHECK(!map.pressed("Jump"));
    CHECK_NEAR(map.axis("MoveX"), 0.0f, 1e-6f);

    // Frame 2: press space -> Jump held AND pressed (edge this frame).
    st.key[44] = true;
    map.update(down, analog);
    CHECK(map.held("Jump"));
    CHECK(map.pressed("Jump"));
    CHECK(!map.released("Jump"));

    // Frame 3: still held -> held true, pressed false (no new edge).
    map.update(down, analog);
    CHECK(map.held("Jump"));
    CHECK(!map.pressed("Jump"));

    // Frame 4: release space -> released edge, no longer held.
    st.key[44] = false;
    map.update(down, analog);
    CHECK(!map.held("Jump"));
    CHECK(map.released("Jump"));

    // Alternate source: pad A alone drives Jump held.
    st.pad[0] = true;
    map.update(down, analog);
    CHECK(map.held("Jump"));
    CHECK(map.pressed("Jump")); // edge from up->down via the pad
    st.pad[0] = false;

    // Axis from keys: D -> +1, A -> -1, both -> 0.
    st.key[7] = true; // D
    map.update(down, analog);
    CHECK_NEAR(map.axis("MoveX"), 1.0f, 1e-6f);
    st.key[4] = true; // A too
    map.update(down, analog);
    CHECK_NEAR(map.axis("MoveX"), 0.0f, 1e-6f);
    st.key[7] = false;
    map.update(down, analog);
    CHECK_NEAR(map.axis("MoveX"), -1.0f, 1e-6f);
    st.key[4] = false;

    // Analog contribution adds to keys and clamps to [-1, 1].
    st.axis[0] = 0.5f;
    map.update(down, analog);
    CHECK_NEAR(map.axis("MoveX"), 0.5f, 1e-6f);
    st.key[7] = true;          // D (+1) plus analog 0.5 -> clamp to 1.0
    map.update(down, analog);
    CHECK_NEAR(map.axis("MoveX"), 1.0f, 1e-6f);
    st.key[7] = false;
    st.axis[0] = 0.0f;

    // Inverted analog axis.
    st.axis[1] = 0.8f;
    map.update(down, analog);
    CHECK_NEAR(map.axis("AimY"), -0.8f, 1e-6f);

    // Unknown actions read as neutral.
    CHECK(!map.held("Ghost"));
    CHECK_NEAR(map.axis("Ghost"), 0.0f, 1e-6f);
}

void testSceneSerializer() {
    struct Transform {
        float x, y;
    };
    struct Health {
        int hp;
    };
    struct Tag {
        std::string name;
    };

    io::SceneSerializer s;
    s.component<Transform>(
        "Transform",
        [](const Transform& t) {
            io::JsonValue j;
            j.set("x", t.x);
            j.set("y", t.y);
            return j;
        },
        [](const io::JsonValue& j) {
            return Transform{j["x"].asFloat(), j["y"].asFloat()};
        });
    s.component<Health>(
        "Health",
        [](const Health& h) {
            io::JsonValue j;
            j.set("hp", h.hp);
            return j;
        },
        [](const io::JsonValue& j) { return Health{j["hp"].asInt()}; });
    s.component<Tag>(
        "Tag",
        [](const Tag& t) {
            io::JsonValue j;
            j.set("name", t.name);
            return j;
        },
        [](const io::JsonValue& j) { return Tag{j["name"].asString()}; });

    ecs::World w;
    ecs::Entity a = w.create();
    w.add<Transform>(a, {1.0f, 2.0f});
    w.add<Health>(a, {100});
    ecs::Entity b = w.create();
    w.add<Transform>(b, {3.5f, -4.0f});
    w.add<Tag>(b, {"boss"});
    ecs::Entity c = w.create();
    w.add<Health>(c, {50});

    // Save the world; three entities carry registered components.
    io::JsonValue doc = s.saveWorld(w);
    CHECK(doc["entities"].size() == 3);
    // Entities are emitted in ascending-id order.
    CHECK(doc["entities"][0]["id"].asInt() < doc["entities"][1]["id"].asInt());

    // Load into a fresh world; counts match.
    ecs::World w2;
    CHECK(s.loadWorld(doc, w2) == 3);
    CHECK(w2.size() == 3);

    // Component values survived the round-trip.
    int transforms = 0, healths = 0, tags = 0, sumHp = 0;
    float sumX = 0.0f;
    std::string tagName;
    w2.each<Transform>([&](ecs::Entity, Transform& t) {
        ++transforms;
        sumX += t.x;
    });
    w2.each<Health>([&](ecs::Entity, Health& h) {
        ++healths;
        sumHp += h.hp;
    });
    w2.each<Tag>([&](ecs::Entity, Tag& t) {
        ++tags;
        tagName = t.name;
    });
    CHECK(transforms == 2);
    CHECK(healths == 2);
    CHECK(tags == 1);
    CHECK_NEAR(sumX, 4.5f, 1e-5f); // 1.0 + 3.5
    CHECK(sumHp == 150);           // 100 + 50
    CHECK(tagName == std::string("boss"));

    // Round-trip through JSON text (dump -> parse -> load) is stable.
    auto reparsed = io::parseJson(doc.dump());
    CHECK(reparsed.ok);
    ecs::World w3;
    CHECK(s.loadWorld(reparsed.value, w3) == 3);
    CHECK(w3.size() == 3);

    // File round-trip.
    const std::string path = "maz_scene_roundtrip_test.json";
    CHECK(s.saveWorldFile(w, path));
    ecs::World w4;
    CHECK(s.loadWorldFile(path, w4) == 3);
    std::remove(path.c_str());

    // An unknown component type in the document is skipped on load (no crash); known ones still load.
    const char* doc2 =
        R"({ "entities": [ { "id": 1, "components": { "Unknown": {"z": 5}, "Health": {"hp": 7} } } ] })";
    auto p2 = io::parseJson(doc2);
    CHECK(p2.ok);
    ecs::World w5;
    CHECK(s.loadWorld(p2.value, w5) == 1);
    int h5 = 0;
    w5.each<Health>([&](ecs::Entity, Health& h) { h5 = h.hp; });
    CHECK(h5 == 7);
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

void testSignal() {
    using core::ConnectionId;

    // Basic connect + emit passes the argument through; a second emit accumulates.
    {
        core::Signal<int> changed;
        int sum = 0;
        changed.connect([&](int v) { sum += v; });
        changed.emit(5);
        CHECK(sum == 5);
        changed.emit(3);
        CHECK(sum == 8);
        CHECK(changed.connectionCount() == 1);
    }

    // Multiple handlers all fire, in connection order.
    {
        core::Signal<> pinged;
        std::string order;
        pinged.connect([&]() { order += "a"; });
        pinged.connect([&]() { order += "b"; });
        pinged.emit();
        CHECK(order == "ab");
        CHECK(pinged.connectionCount() == 2);
    }

    // Disconnect stops a handler; isConnected reflects it.
    {
        core::Signal<> s;
        int n = 0;
        const ConnectionId id = s.connect([&]() { ++n; });
        CHECK(s.isConnected(id));
        s.emit();
        CHECK(n == 1);
        CHECK(s.disconnect(id));
        CHECK(!s.isConnected(id));
        s.emit();
        CHECK(n == 1); // no longer called
        CHECK(!s.disconnect(id)); // already gone
    }

    // One-shot fires exactly once, then auto-disconnects.
    {
        core::Signal<> s;
        int n = 0;
        s.connectOnce([&]() { ++n; });
        CHECK(s.connectionCount() == 1);
        s.emit();
        s.emit();
        CHECK(n == 1);
        CHECK(s.connectionCount() == 0);
    }

    // Deferred: the call is queued on emit and only runs at flushDeferred, with the emit args preserved.
    {
        core::Signal<int> s;
        int got = -1;
        s.connectDeferred([&](int v) { got = v; });
        s.emit(7);
        CHECK(got == -1);            // not called yet
        CHECK(s.pendingDeferred() == 1);
        s.flushDeferred();
        CHECK(got == 7);             // args carried through
        CHECK(s.pendingDeferred() == 0);
    }

    // Deferred + one-shot: queued once, runs once at flush, then gone.
    {
        core::Signal<> s;
        int n = 0;
        s.connectDeferredOnce([&]() { ++n; });
        s.emit();
        s.emit();                    // one-shot already disconnected → only one queued
        CHECK(s.pendingDeferred() == 1);
        s.flushDeferred();
        CHECK(n == 1);
        CHECK(s.connectionCount() == 0);
    }

    // A handler may disconnect itself during dispatch without corrupting the emit.
    {
        core::Signal<> s;
        int n = 0;
        ConnectionId self = core::kInvalidConnection;
        self = s.connect([&]() {
            ++n;
            s.disconnect(self);
        });
        s.connect([&]() { ++n; }); // a second handler must still run this round
        s.emit();
        CHECK(n == 2);             // both ran on the first emit
        s.emit();
        CHECK(n == 3);             // only the survivor ran on the second
    }

    // disconnectAll clears everything.
    {
        core::Signal<> s;
        s.connect([]() {});
        s.connect([]() {});
        CHECK(s.connectionCount() == 2);
        s.disconnectAll();
        CHECK(s.connectionCount() == 0);
    }
}

void testStringId() {
    using core::StringId;
    using core::StringTable;

    // Interning the SAME text twice yields the SAME id; different text yields different ids.
    {
        StringTable t;
        const StringId a = t.intern("player");
        const StringId b = t.intern("enemy");
        const StringId a2 = t.intern("player");
        CHECK(a.valid());
        CHECK(a == a2);        // deduplicated
        CHECK(a != b);         // distinct strings
        CHECK(t.size() == 2);  // only two unique entries despite three interns
    }

    // str() reverses an id back to its text.
    {
        StringTable t;
        const StringId id = t.intern("jump");
        CHECK(t.str(id) == "jump");
        // An invalid id reverses to the empty string.
        CHECK(t.str(StringId{core::kInvalidStringId}).empty());
    }

    // find() looks up WITHOUT inserting; contains() reflects membership.
    {
        StringTable t;
        t.intern("alpha");
        CHECK(t.find("alpha").valid());
        CHECK(!t.find("beta").valid()); // absent
        CHECK(t.size() == 1);           // find() did not grow the table
        CHECK(t.contains("alpha"));
        CHECK(!t.contains("beta"));
    }

    // Ids are dense insertion indices, stable across more interns.
    {
        StringTable t;
        const StringId a = t.intern("a");
        const StringId b = t.intern("b");
        const StringId c = t.intern("c");
        CHECK(a.value == 0u);
        CHECK(b.value == 1u);
        CHECK(c.value == 2u);
        CHECK(t.intern("a").value == 0u); // re-intern returns the original id
    }

    // The FNV-1a hash is deterministic and matches the id's stored hash; different strings differ.
    {
        // Known FNV-1a-32 of "hello" is 0x4F9F2CAB.
        CHECK(core::fnv1a32("hello") == 0x4F9F2CABu);
        CHECK(core::fnv1a32("") == 0x811C9DC5u); // empty -> offset basis
        StringTable t;
        const StringId h = t.intern("hello");
        CHECK(t.hash(h) == core::fnv1a32("hello"));
        CHECK(core::fnv1a32("hello") != core::fnv1a32("world"));
    }

    // The empty string is a legitimate, interned value.
    {
        StringTable t;
        const StringId e = t.intern("");
        CHECK(e.valid());
        CHECK(t.str(e).empty());
        CHECK(t.intern("") == e); // still dedups
        CHECK(t.size() == 1);
    }

    // StringId works as an unordered_map key (hash specialization).
    {
        StringTable t;
        std::unordered_map<StringId, int> counts;
        counts[t.intern("x")] += 3;
        counts[t.intern("x")] += 4; // same id -> same bucket
        counts[t.intern("y")] += 1;
        CHECK(counts.size() == 2);
        CHECK(counts[t.find("x")] == 7);
    }

    // clear() empties the table.
    {
        StringTable t;
        t.intern("one");
        t.intern("two");
        t.clear();
        CHECK(t.empty());
        CHECK(t.size() == 0);
        CHECK(!t.contains("one"));
    }
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

void testAdditiveBlend() {
    using anim::JointPose;

    // A zero delta (additive == reference) leaves the base untouched at ANY weight.
    {
        JointPose base;
        base.translation = math::vec3(5.0f, 1.0f, -2.0f);
        base.rotation = glm::normalize(glm::angleAxis(0.7f, math::vec3(0, 0, 1)));
        base.scale = math::vec3(2.0f, 2.0f, 2.0f);
        JointPose reference; // identity-ish
        reference.translation = math::vec3(3.0f, 3.0f, 3.0f);
        JointPose additive = reference; // no difference
        const JointPose out = anim::additiveBlendJoint(base, additive, reference, 1.0f);
        CHECK_NEAR(out.translation.x, base.translation.x, 1e-5f);
        CHECK_NEAR(out.translation.y, base.translation.y, 1e-5f);
        CHECK_NEAR(out.scale.x, base.scale.x, 1e-5f);
        // Rotation unchanged (dot of quats ~ 1).
        CHECK(std::fabs(glm::dot(out.rotation, base.rotation)) > 0.9999f);
    }

    // Weight 0 returns the base exactly even with a non-trivial delta.
    {
        JointPose base;
        base.translation = math::vec3(1.0f, 0.0f, 0.0f);
        JointPose reference; // zero translation
        JointPose additive;
        additive.translation = math::vec3(0.0f, 4.0f, 0.0f);
        const JointPose out = anim::additiveBlendJoint(base, additive, reference, 0.0f);
        CHECK_NEAR(out.translation.x, 1.0f, 1e-5f);
        CHECK_NEAR(out.translation.y, 0.0f, 1e-5f);
    }

    // Translation delta adds on top of the base, scaled by weight.
    {
        JointPose base;
        base.translation = math::vec3(1.0f, 0.0f, 0.0f);
        JointPose reference; // 0
        JointPose additive;
        additive.translation = math::vec3(0.0f, 2.0f, 0.0f); // delta = (0,2,0)
        const JointPose full = anim::additiveBlendJoint(base, additive, reference, 1.0f);
        CHECK_NEAR(full.translation.x, 1.0f, 1e-5f);
        CHECK_NEAR(full.translation.y, 2.0f, 1e-5f);
        const JointPose half = anim::additiveBlendJoint(base, additive, reference, 0.5f);
        CHECK_NEAR(half.translation.y, 1.0f, 1e-5f);
    }

    // Rotation delta: reference identity, additive = +90 deg about Z, base identity. At full weight the
    // result rotates +X onto +Y; at half weight ~ +45 deg.
    {
        JointPose base; // identity rotation
        JointPose reference; // identity
        JointPose additive;
        additive.rotation = glm::normalize(glm::angleAxis(glm::radians(90.0f), math::vec3(0, 0, 1)));
        const JointPose full = anim::additiveBlendJoint(base, additive, reference, 1.0f);
        const math::vec3 vFull = full.rotation * math::vec3(1, 0, 0);
        CHECK_NEAR(vFull.x, 0.0f, 1e-4f);
        CHECK_NEAR(vFull.y, 1.0f, 1e-4f);
        const JointPose half = anim::additiveBlendJoint(base, additive, reference, 0.5f);
        const math::vec3 vHalf = half.rotation * math::vec3(1, 0, 0);
        CHECK_NEAR(vHalf.x, std::cos(glm::radians(45.0f)), 1e-3f);
        CHECK_NEAR(vHalf.y, std::sin(glm::radians(45.0f)), 1e-3f);
    }

    // Scale delta is a RATIO applied multiplicatively: base 2, ref 1, additive 3 -> delta 3;
    // full weight -> 2*3 = 6; half weight -> 2 * lerp(1,3,0.5) = 2*2 = 4.
    {
        JointPose base;
        base.scale = math::vec3(2.0f, 2.0f, 2.0f);
        JointPose reference;
        reference.scale = math::vec3(1.0f, 1.0f, 1.0f);
        JointPose additive;
        additive.scale = math::vec3(3.0f, 3.0f, 3.0f);
        const JointPose full = anim::additiveBlendJoint(base, additive, reference, 1.0f);
        CHECK_NEAR(full.scale.x, 6.0f, 1e-4f);
        const JointPose half = anim::additiveBlendJoint(base, additive, reference, 0.5f);
        CHECK_NEAR(half.scale.x, 4.0f, 1e-4f);
    }

    // Rotation delta relative to a NON-identity reference: reference = +30 about Z, additive = +90 about
    // Z, so the delta is +60. Applied to an identity base at full weight -> +60 rotation of +X.
    {
        JointPose base; // identity
        JointPose reference;
        reference.rotation = glm::normalize(glm::angleAxis(glm::radians(30.0f), math::vec3(0, 0, 1)));
        JointPose additive;
        additive.rotation = glm::normalize(glm::angleAxis(glm::radians(90.0f), math::vec3(0, 0, 1)));
        const JointPose out = anim::additiveBlendJoint(base, additive, reference, 1.0f);
        const math::vec3 v = out.rotation * math::vec3(1, 0, 0);
        CHECK_NEAR(v.x, std::cos(glm::radians(60.0f)), 1e-3f);
        CHECK_NEAR(v.y, std::sin(glm::radians(60.0f)), 1e-3f);
    }

    // Whole-pose additive: only the joint that differs in the additive clip changes; a joint equal to
    // the reference is left at its base value.
    {
        std::vector<JointPose> base(2), reference(2), additive(2);
        base[0].translation = math::vec3(0.0f, 0.0f, 0.0f);
        base[1].translation = math::vec3(10.0f, 0.0f, 0.0f);
        // reference: both zero-translation. additive: joint 0 moves, joint 1 identical to reference.
        additive[0].translation = math::vec3(0.0f, 5.0f, 0.0f);
        std::vector<JointPose> out;
        anim::additiveBlend(base, additive, reference, 1.0f, out);
        CHECK(out.size() == 2);
        CHECK_NEAR(out[0].translation.y, 5.0f, 1e-5f); // joint 0 got the layer
        CHECK_NEAR(out[1].translation.x, 10.0f, 1e-5f); // joint 1 untouched (base preserved)
        CHECK_NEAR(out[1].translation.y, 0.0f, 1e-5f);
    }
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

void testPhysics2DRotation() {
    using game::Body2D;

    // enableRotation derives the inverse moment of inertia from shape + mass.
    {
        Body2D box;
        box.shape = Body2D::Box;
        box.half = math::vec2(2.0f, 1.0f); // 4x2 box
        box.invMass = 1.0f / 3.0f;         // mass 3
        box.enableRotation();
        // I = m(w^2+h^2)/12 = 3*(16+4)/12 = 5 -> invInertia = 0.2
        CHECK_NEAR(box.invInertia, 0.2f, 1e-5f);

        Body2D disc;
        disc.shape = Body2D::Circle;
        disc.radius = 2.0f;
        disc.invMass = 1.0f / 4.0f; // mass 4
        disc.enableRotation();
        // I = 0.5*m*r^2 = 0.5*4*4 = 8 -> invInertia = 0.125
        CHECK_NEAR(disc.invInertia, 0.125f, 1e-5f);

        Body2D wall; // static stays rotation-locked
        wall.invMass = 0.0f;
        wall.enableRotation();
        CHECK(wall.invInertia == 0.0f);
    }

    // A free-spinning body with no contacts advances its angle by angularVel*dt each step.
    {
        game::PhysicsWorld2D w; // no gravity, no other bodies
        Body2D b;
        b.shape = Body2D::Box;
        b.half = math::vec2(5.0f, 5.0f);
        b.invMass = 1.0f;
        b.enableRotation();
        b.angularVel = 2.0f; // rad/s
        w.add(b);
        const float dt = 1.0f / 60.0f;
        for (int i = 0; i < 60; ++i) {
            w.step(dt);
        }
        CHECK_NEAR(w.bodies[0].angle, 2.0f * 60.0f * dt, 1e-3f); // ~2 rad after 1 s
        CHECK_NEAR(w.bodies[0].angularVel, 2.0f, 1e-4f);         // undamped: spin unchanged
    }

    // Angular damping bleeds off spin.
    {
        game::PhysicsWorld2D w;
        Body2D b;
        b.shape = Body2D::Box;
        b.half = math::vec2(5.0f, 5.0f);
        b.invMass = 1.0f;
        b.enableRotation();
        b.angularVel = 5.0f;
        b.angularDamping = 3.0f;
        w.add(b);
        for (int i = 0; i < 120; ++i) {
            w.step(1.0f / 60.0f);
        }
        CHECK(std::fabs(w.bodies[0].angularVel) < 5.0f);  // decayed
        CHECK(std::fabs(w.bodies[0].angularVel) < 0.75f); // substantially
    }

    // A tilted box dropped onto a static floor picks up spin (torque from the off-center corner
    // contact), then damps down and settles resting above the floor — real rotational dynamics.
    {
        game::PhysicsWorld2D w;
        w.gravity = math::vec2(0.0f, 800.0f); // +y down

        Body2D floor;
        floor.shape = Body2D::Box;
        floor.pos = math::vec2(100.0f, 200.0f);
        floor.half = math::vec2(120.0f, 10.0f); // top surface at y=190
        floor.invMass = 0.0f;                   // static
        floor.friction = 0.7f;
        w.add(floor);

        Body2D box;
        box.shape = Body2D::Box;
        box.pos = math::vec2(100.0f, 120.0f);
        box.half = math::vec2(12.0f, 12.0f);
        box.invMass = 1.0f / 5.0f;
        box.restitution = 0.0f;
        box.friction = 0.7f;
        box.angle = 0.4f; // tilted, so it lands on a corner
        box.linearDamping = 0.6f;
        box.angularDamping = 2.0f;
        box.enableRotation();
        w.add(box);

        float maxSpin = 0.0f;
        for (int i = 0; i < 400; ++i) {
            w.step(1.0f / 60.0f, 8);
            maxSpin = std::max(maxSpin, std::fabs(w.bodies[1].angularVel));
        }
        const Body2D& r = w.bodies[1];
        // "At rest" is measured by the settled pose, not the instantaneous spin: a single-contact
        // solver leaves a tiny angular limit-cycle between the two bottom corners even once the box
        // is visually still, so we assert orientation + height rather than |angularVel|.
        CHECK(maxSpin > 0.05f);            // it actually rotated on the way down (corner contact -> torque)
        CHECK(std::fabs(r.angle) < 0.05f); // toppled flat from its initial 0.4 rad tilt
        CHECK(r.pos.y < 181.0f);           // rests on top of the floor (top at y=190, half-height 12)
        CHECK(r.pos.y > 175.0f);           // and did not sink into it
    }
}

void testPhysics2DJoints() {
    using game::Body2D;
    using game::Joint2D;

    // Pin joint as a pendulum: a body anchored (at an offset point) to a fixed world point orbits that
    // point at a constant radius = the anchor offset, no matter how it swings.
    {
        game::PhysicsWorld2D w;
        w.gravity = math::vec2(0.0f, 800.0f); // +y down

        Body2D bob;
        bob.shape = Body2D::Box;
        bob.pos = math::vec2(0.0f, 100.0f); // 100 below the anchor
        bob.half = math::vec2(8.0f, 8.0f);
        bob.invMass = 1.0f;
        bob.vel = math::vec2(200.0f, 0.0f); // shove sideways so it swings
        bob.enableRotation();
        const uint32_t id = w.add(bob);

        Joint2D pin;
        pin.type = Joint2D::Pin;
        pin.a = static_cast<int>(id);
        pin.b = -1;                            // anchor to a fixed world point
        pin.localA = math::vec2(0.0f, -100.0f); // the point 100 "above" the body's centre
        pin.anchorB = math::vec2(0.0f, 0.0f);   // world anchor
        w.addJoint(pin);

        float maxX = 0.0f;
        bool radiusHeld = true;
        for (int i = 0; i < 300; ++i) {
            w.step(1.0f / 60.0f, 10);
            const math::vec2 p = w.bodies[0].pos;
            const float r = std::sqrt(p.x * p.x + p.y * p.y);
            if (r < 88.0f || r > 112.0f) {
                radiusHeld = false; // constraint kept the bob ~100 from the anchor
            }
            maxX = std::max(maxX, std::fabs(p.x));
        }
        CHECK(radiusHeld);     // the pin held the pendulum arm length throughout
        CHECK(maxX > 30.0f);   // and it actually swung sideways
    }

    // Damped spring pulls a stretched body back to its rest length and settles there.
    {
        game::PhysicsWorld2D w; // no gravity

        Body2D m;
        m.shape = Body2D::Circle;
        m.radius = 5.0f;
        m.pos = math::vec2(0.0f, 80.0f); // stretched: 80 from the anchor, rest is 50
        m.invMass = 1.0f;
        const uint32_t id = w.add(m);

        Joint2D spring;
        spring.type = Joint2D::Spring;
        spring.a = static_cast<int>(id);
        spring.b = -1;
        spring.localA = math::vec2(0.0f, 0.0f);
        spring.anchorB = math::vec2(0.0f, 0.0f);
        spring.restLength = 50.0f;
        spring.stiffness = 30.0f;
        spring.damping = 8.0f;
        w.addJoint(spring);

        for (int i = 0; i < 400; ++i) {
            w.step(1.0f / 60.0f, 8);
        }
        // Settles at the rest length along the y axis (equilibrium at (0,50)).
        CHECK_NEAR(w.bodies[0].pos.y, 50.0f, 4.0f);
        CHECK(std::fabs(w.bodies[0].pos.x) < 4.0f);
        CHECK(std::fabs(w.bodies[0].vel.y) < 5.0f); // and comes to rest
    }
}

void testPhysics2DGroove() {
    using game::Body2D;
    using game::Joint2D;

    // A slider starting OFF a horizontal groove (y=0 line through the origin) is pulled onto the line,
    // while its position ALONG the groove is left free (unchanged).
    {
        game::PhysicsWorld2D w; // no gravity

        Body2D rail; // static groove body at the origin
        rail.pos = math::vec2(0.0f, 0.0f);
        rail.invMass = 0.0f;
        const uint32_t rid = w.add(rail);

        Body2D slider;
        slider.shape = Body2D::Box;
        slider.half = math::vec2(4.0f, 4.0f);
        slider.pos = math::vec2(30.0f, 20.0f); // 20 above the groove line, x=30 along it
        slider.invMass = 1.0f;
        const uint32_t sid = w.add(slider);

        Joint2D g;
        g.type = Joint2D::Groove;
        g.a = static_cast<int>(rid);
        g.b = static_cast<int>(sid);
        g.localA = math::vec2(0.0f, 0.0f); // groove passes through the rail's origin
        g.axis = math::vec2(1.0f, 0.0f);   // horizontal groove
        g.anchorB = math::vec2(0.0f, 0.0f); // slider anchored at its centre
        w.addJoint(g);

        for (int i = 0; i < 400; ++i) {
            w.step(1.0f / 60.0f, 8);
        }
        CHECK(std::fabs(w.bodies[1].pos.y) < 0.2f);        // pulled onto the groove line
        CHECK_NEAR(w.bodies[1].pos.x, 30.0f, 0.5f);        // free along the groove: x unchanged
    }

    // The groove holds the slider on a TILTED line under gravity: the along-axis component of gravity
    // slides it, but the perpendicular offset from the line stays ~0 the whole time.
    {
        game::PhysicsWorld2D w;
        w.gravity = math::vec2(0.0f, 600.0f); // +y down

        Body2D rail;
        rail.pos = math::vec2(0.0f, 0.0f);
        rail.invMass = 0.0f;
        const uint32_t rid = w.add(rail);

        const math::vec2 axis = math::vec2(1.0f, 1.0f) / std::sqrt(2.0f);

        Body2D slider;
        slider.shape = Body2D::Box;
        slider.half = math::vec2(4.0f, 4.0f);
        slider.pos = axis * 20.0f; // start on the line, clear of the rail reference body
        slider.invMass = 1.0f;
        const uint32_t sid = w.add(slider);

        // A 45-degree groove direction (need not be unit).
        Joint2D g;
        g.type = Joint2D::Groove;
        g.a = static_cast<int>(rid);
        g.b = static_cast<int>(sid);
        g.localA = math::vec2(0.0f, 0.0f);
        g.axis = math::vec2(1.0f, 1.0f); // down-right diagonal
        g.anchorB = math::vec2(0.0f, 0.0f);
        w.addJoint(g);

        const math::vec2 perp(-axis.y, axis.x);
        float maxOff = 0.0f, maxAlong = 0.0f;
        for (int i = 0; i < 200; ++i) {
            w.step(1.0f / 60.0f, 8);
            const math::vec2 p = w.bodies[1].pos;
            maxOff = std::max(maxOff, std::fabs(glm::dot(p, perp)));   // off-line drift
            maxAlong = std::max(maxAlong, std::fabs(glm::dot(p, axis))); // travel along the rail
        }
        CHECK(maxOff < 1.0f);    // stayed on the groove line
        CHECK(maxAlong > 40.0f); // and actually slid down the incline (started at along=20)
    }
}

void testPhysicsQuery2D() {
    using game::QueryShape2D;
    using game::RayHit2D;

    // Ray straight at a circle: hits the near surface, correct distance + outward normal.
    {
        std::vector<QueryShape2D> shapes;
        QueryShape2D c;
        c.kind = QueryShape2D::Circle;
        c.pos = math::vec2(5.0f, 0.0f);
        c.radius = 1.0f;
        c.id = 7;
        shapes.push_back(c);

        const RayHit2D h = game::queryRay(math::vec2(0.0f, 0.0f), math::vec2(1.0f, 0.0f), shapes);
        CHECK(h.hit);
        CHECK_NEAR(h.t, 4.0f, 1e-4f);
        CHECK_NEAR(h.point.x, 4.0f, 1e-4f);
        CHECK_NEAR(h.point.y, 0.0f, 1e-4f);
        CHECK_NEAR(h.normal.x, -1.0f, 1e-4f); // points back toward the ray origin
        CHECK_NEAR(h.normal.y, 0.0f, 1e-4f);
        CHECK(h.index == 0);
        CHECK(h.id == 7);
    }

    // A ray that misses the circle entirely.
    {
        std::vector<QueryShape2D> shapes;
        QueryShape2D c;
        c.pos = math::vec2(5.0f, 0.0f);
        c.radius = 1.0f;
        shapes.push_back(c);
        const RayHit2D h = game::queryRay(math::vec2(0.0f, 0.0f), math::vec2(0.0f, 1.0f), shapes);
        CHECK(!h.hit);
    }

    // A dir that need not be normalized still yields distance in world units.
    {
        std::vector<QueryShape2D> shapes;
        QueryShape2D c;
        c.pos = math::vec2(5.0f, 0.0f);
        c.radius = 1.0f;
        shapes.push_back(c);
        const RayHit2D h = game::queryRay(math::vec2(0.0f, 0.0f), math::vec2(10.0f, 0.0f), shapes);
        CHECK(h.hit);
        CHECK_NEAR(h.t, 4.0f, 1e-4f);
    }

    // Ray at an axis-aligned box: enters the -X face.
    {
        std::vector<QueryShape2D> shapes;
        QueryShape2D b;
        b.kind = QueryShape2D::Box;
        b.pos = math::vec2(5.0f, 0.0f);
        b.half = math::vec2(1.0f, 1.0f);
        b.angle = 0.0f;
        shapes.push_back(b);
        const RayHit2D h = game::queryRay(math::vec2(0.0f, 0.0f), math::vec2(1.0f, 0.0f), shapes);
        CHECK(h.hit);
        CHECK_NEAR(h.t, 4.0f, 1e-4f);
        CHECK_NEAR(h.normal.x, -1.0f, 1e-4f);
        CHECK_NEAR(h.normal.y, 0.0f, 1e-4f);
    }

    // A box rotated 45 degrees presents a corner toward the ray: the ray along +X hits the diamond at
    // x = 5 - halfDiagonal (half=1 → diagonal reach sqrt(2) ~ 1.4142), with a normal pointing back -X-ish.
    {
        std::vector<QueryShape2D> shapes;
        QueryShape2D b;
        b.kind = QueryShape2D::Box;
        b.pos = math::vec2(5.0f, 0.0f);
        b.half = math::vec2(1.0f, 1.0f);
        b.angle = 3.14159265f / 4.0f;
        shapes.push_back(b);
        const RayHit2D h = game::queryRay(math::vec2(0.0f, 0.0f), math::vec2(1.0f, 0.0f), shapes);
        CHECK(h.hit);
        CHECK_NEAR(h.t, 5.0f - std::sqrt(2.0f), 1e-3f);
        CHECK(h.normal.x < 0.0f); // a face whose outward normal has a -X component
    }

    // queryRay returns the NEAREST of several shapes.
    {
        std::vector<QueryShape2D> shapes;
        QueryShape2D farC;
        farC.pos = math::vec2(9.0f, 0.0f);
        farC.radius = 1.0f;
        farC.id = 1;
        QueryShape2D nearC;
        nearC.pos = math::vec2(4.0f, 0.0f);
        nearC.radius = 1.0f;
        nearC.id = 2;
        shapes.push_back(farC);
        shapes.push_back(nearC);
        const RayHit2D h = game::queryRay(math::vec2(0.0f, 0.0f), math::vec2(1.0f, 0.0f), shapes);
        CHECK(h.hit);
        CHECK(h.id == 2); // the closer one
        CHECK_NEAR(h.t, 3.0f, 1e-4f);
    }

    // A bounded segment stops short: a shape beyond the segment end is not hit.
    {
        std::vector<QueryShape2D> shapes;
        QueryShape2D c;
        c.pos = math::vec2(5.0f, 0.0f);
        c.radius = 1.0f;
        shapes.push_back(c);
        // Segment ends at x=3 (before the circle at x=4..6): no hit.
        const RayHit2D miss = game::querySegment(math::vec2(0.0f, 0.0f), math::vec2(3.0f, 0.0f), shapes);
        CHECK(!miss.hit);
        // Segment reaching x=4.5 (into the circle) hits.
        const RayHit2D hit = game::querySegment(math::vec2(0.0f, 0.0f), math::vec2(4.5f, 0.0f), shapes);
        CHECK(hit.hit);
    }

    // Layer/mask filtering: a shape whose layer misses the query mask is skipped, so a farther shape on
    // the matching layer is the reported hit.
    {
        std::vector<QueryShape2D> shapes;
        QueryShape2D wall;
        wall.pos = math::vec2(3.0f, 0.0f);
        wall.radius = 0.5f;
        wall.layer = 0x1; // "walls"
        wall.id = 100;
        QueryShape2D enemy;
        enemy.pos = math::vec2(6.0f, 0.0f);
        enemy.radius = 0.5f;
        enemy.layer = 0x2; // "enemies"
        enemy.id = 200;
        shapes.push_back(wall);
        shapes.push_back(enemy);
        // Query only the enemy layer: the nearer wall is ignored.
        const RayHit2D h = game::queryRay(math::vec2(0.0f, 0.0f), math::vec2(1.0f, 0.0f), shapes, 1e30f,
                                          0x2);
        CHECK(h.hit);
        CHECK(h.id == 200);
    }

    // pointInShape / queryPoint: mouse-pick semantics for a circle and a rotated box.
    {
        QueryShape2D c;
        c.pos = math::vec2(0.0f, 0.0f);
        c.radius = 2.0f;
        CHECK(game::pointInShape(math::vec2(1.0f, 1.0f), c));   // inside
        CHECK(!game::pointInShape(math::vec2(2.0f, 2.0f), c));  // outside (dist ~2.83 > 2)

        QueryShape2D b;
        b.kind = QueryShape2D::Box;
        b.pos = math::vec2(0.0f, 0.0f);
        b.half = math::vec2(2.0f, 1.0f);
        b.angle = 3.14159265f / 2.0f; // 90-degree rotation swaps the effective extents
        CHECK(game::pointInShape(math::vec2(0.0f, 1.8f), b));   // inside the rotated (now tall) box
        CHECK(!game::pointInShape(math::vec2(1.8f, 0.0f), b));  // outside along the now-narrow axis

        std::vector<QueryShape2D> shapes;
        shapes.push_back(c);
        shapes.push_back(b);
        const std::vector<int> inside = game::queryPoint(math::vec2(0.0f, 0.0f), shapes);
        CHECK(inside.size() == 2); // origin is inside both
    }
}

void testManifold2() {
    using game::Body2D;
    namespace d = game::detail;

    // Two axis-aligned boxes overlapping along y produce a TWO-point manifold on the shared face,
    // with the normal pointing a->b (+y) and both points at the contact plane.
    {
        Body2D a;
        a.shape = Body2D::Box;
        a.half = math::vec2(1.0f, 1.0f);
        a.pos = math::vec2(0.0f, 0.0f); // spans y[-1,1], top face y=1
        Body2D b;
        b.shape = Body2D::Box;
        b.half = math::vec2(1.0f, 1.0f);
        b.pos = math::vec2(0.0f, 1.5f); // spans y[0.5,2.5], bottom face y=0.5; overlap 0.5

        d::Contact2 m = d::obbObbManifold(a, b);
        CHECK(m.hit);
        CHECK(m.count == 2);
        CHECK_NEAR(m.n.x, 0.0f, 1e-4f);
        CHECK_NEAR(m.n.y, 1.0f, 1e-4f); // from a toward b (b is above in +y)
        // Both contact points lie on the incident (b bottom) face at y=0.5, spanning the shared width.
        for (int k = 0; k < 2; ++k) {
            CHECK_NEAR(m.point[k].y, 0.5f, 1e-3f);
            CHECK(m.point[k].x >= -1.0f - 1e-3f && m.point[k].x <= 1.0f + 1e-3f);
            CHECK_NEAR(m.pen[k], 0.5f, 1e-3f); // 0.5 deep behind the reference face
        }
        // The two points are at opposite ends of the face (distinct x).
        CHECK(std::fabs(m.point[0].x - m.point[1].x) > 1.5f);
    }

    // Horizontal overlap gives a vertical shared face with normal ~ (1,0) and two points.
    {
        Body2D a;
        a.shape = Body2D::Box;
        a.half = math::vec2(1.0f, 1.0f);
        a.pos = math::vec2(0.0f, 0.0f);
        Body2D b;
        b.shape = Body2D::Box;
        b.half = math::vec2(1.0f, 1.0f);
        b.pos = math::vec2(1.5f, 0.0f);
        d::Contact2 m = d::obbObbManifold(a, b);
        CHECK(m.hit);
        CHECK(m.count == 2);
        CHECK_NEAR(m.n.x, 1.0f, 1e-4f);
        CHECK_NEAR(m.n.y, 0.0f, 1e-4f);
    }

    // Clearly separated boxes: no contact.
    {
        Body2D a;
        a.shape = Body2D::Box;
        a.half = math::vec2(1.0f, 1.0f);
        a.pos = math::vec2(0.0f, 0.0f);
        Body2D b;
        b.shape = Body2D::Box;
        b.half = math::vec2(1.0f, 1.0f);
        b.pos = math::vec2(5.0f, 0.0f);
        d::Contact2 m = d::obbObbManifold(a, b);
        CHECK(!m.hit);
        CHECK(m.count == 0);
    }

    // End to end: a stack of oriented boxes on a static floor stays SQUARE with manifolds on, where the
    // single-point solver lets it rotate away. Build identical stacks, step both, compare max tilt.
    auto buildStack = [](bool manifolds) {
        game::PhysicsWorld2D w;
        w.gravity = math::vec2(0.0f, 600.0f); // +y down
        w.solveManifolds = manifolds;

        Body2D floor;
        floor.shape = Body2D::Box;
        floor.half = math::vec2(200.0f, 10.0f);
        floor.pos = math::vec2(0.0f, 200.0f);
        floor.invMass = 0.0f; // static
        floor.friction = 0.9f;
        w.add(floor);

        for (int i = 0; i < 4; ++i) {
            Body2D box;
            box.shape = Body2D::Box;
            box.half = math::vec2(30.0f, 18.0f);
            // Start each box a hair off-centre so a single-point solver has an asymmetry to amplify.
            const float dx = (i % 2 == 0) ? 3.0f : -3.0f;
            box.pos = math::vec2(dx, 154.0f - static_cast<float>(i) * 37.0f);
            box.invMass = 1.0f;
            box.friction = 0.9f;
            box.restitution = 0.0f;
            box.enableRotation();
            w.add(box);
        }
        for (int s = 0; s < 360; ++s) {
            w.step(1.0f / 60.0f, 10);
        }
        float maxTilt = 0.0f;
        for (std::size_t i = 1; i < w.bodies.size(); ++i) {
            maxTilt = std::max(maxTilt, std::fabs(w.bodies[i].angle));
        }
        return maxTilt;
    };

    const float tiltManifolds = buildStack(true);
    const float tiltSingle = buildStack(false);
    // With two-point manifolds the tower stays nearly upright...
    CHECK(tiltManifolds < 0.15f);
    // ...and it is meaningfully more stable than the single-point solver on the same scene.
    CHECK(tiltManifolds <= tiltSingle + 1e-4f);
}

void testNormalLight() {
    using game::PointLight2D;
    using math::vec2;
    using math::vec3;

    const vec3 white(1.0f, 1.0f, 1.0f);
    const vec3 flat(0.0f, 0.0f, 1.0f); // +z, facing the viewer

    // A light directly above a flat-facing texel lights it fully (N·L = 1 at the centre, atten ~1).
    {
        PointLight2D L;
        L.pos = vec2(0.0f, 0.0f);
        L.height = 50.0f;
        L.range = 400.0f;
        L.energy = 1.0f;
        L.color = white;
        const vec3 c = game::shadePointLight(vec2(0.0f, 0.0f), flat, white, L);
        // Straight down onto a +z normal -> N·L = 1, distance in-plane 0 -> atten 1.
        CHECK_NEAR(c.x, 1.0f, 1e-4f);
        CHECK_NEAR(c.y, 1.0f, 1e-4f);
        CHECK_NEAR(c.z, 1.0f, 1e-4f);
    }

    // A normal tilted TOWARD the light is brighter than one tilted AWAY.
    {
        PointLight2D L;
        L.pos = vec2(100.0f, 0.0f); // light to the right
        L.height = 40.0f;
        L.range = 400.0f;
        const vec3 nRight = glm::normalize(vec3(0.6f, 0.0f, 0.8f)); // tilts right, toward the light
        const vec3 nLeft = glm::normalize(vec3(-0.6f, 0.0f, 0.8f)); // tilts left, away
        const vec3 cR = game::shadePointLight(vec2(0.0f, 0.0f), nRight, white, L);
        const vec3 cL = game::shadePointLight(vec2(0.0f, 0.0f), nLeft, white, L);
        CHECK(cR.x > cL.x); // the face turned toward the light catches more
    }

    // A back-facing normal (pointing into the screen) receives nothing.
    {
        PointLight2D L;
        L.pos = vec2(0.0f, 0.0f);
        L.height = 40.0f;
        const vec3 back(0.0f, 0.0f, -1.0f);
        const vec3 c = game::shadePointLight(vec2(0.0f, 0.0f), back, white, L);
        CHECK_NEAR(c.x, 0.0f, 1e-6f);
        CHECK_NEAR(c.y, 0.0f, 1e-6f);
        CHECK_NEAR(c.z, 0.0f, 1e-6f);
    }

    // Distance attenuation: a nearer texel is brighter than a farther one, and past the range it is dark.
    {
        PointLight2D L;
        L.pos = vec2(0.0f, 0.0f);
        L.height = 30.0f;
        L.range = 200.0f;
        const vec3 near = game::shadePointLight(vec2(40.0f, 0.0f), flat, white, L);
        const vec3 far = game::shadePointLight(vec2(150.0f, 0.0f), flat, white, L);
        CHECK(near.x > far.x);
        const vec3 outside = game::shadePointLight(vec2(250.0f, 0.0f), flat, white, L);
        CHECK_NEAR(outside.x, 0.0f, 1e-6f); // beyond range
    }

    // shadeSurface: ambient keeps unlit areas from going fully black, and lights add on top clamped to 1.
    {
        std::vector<PointLight2D> lights;
        PointLight2D L;
        L.pos = vec2(0.0f, 0.0f);
        L.height = 40.0f;
        L.range = 300.0f;
        L.energy = 5.0f; // strong -> would exceed 1 before clamping
        lights.push_back(L);

        const vec3 albedo(0.8f, 0.4f, 0.2f);
        const vec3 ambient(0.2f, 0.2f, 0.2f);

        // Far, back-facing point: only ambient -> albedo*ambient.
        const vec3 dark = game::shadeSurface(vec2(0.0f, 0.0f), vec3(0.0f, 0.0f, -1.0f), albedo, lights,
                                             ambient);
        CHECK_NEAR(dark.x, albedo.x * ambient.x, 1e-4f);

        // Directly lit + strong energy clamps to 1.
        const vec3 lit = game::shadeSurface(vec2(0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f), albedo, lights,
                                            ambient);
        CHECK_NEAR(lit.x, 1.0f, 1e-4f);
        CHECK(lit.y <= 1.0f && lit.y >= 0.0f);
    }

    // decodeNormal: the flat "blue" texel (0.5,0.5,1) decodes to +z; a normalized result always.
    {
        const vec3 n = game::decodeNormal(vec3(0.5f, 0.5f, 1.0f));
        CHECK_NEAR(n.x, 0.0f, 1e-4f);
        CHECK_NEAR(n.y, 0.0f, 1e-4f);
        CHECK_NEAR(n.z, 1.0f, 1e-4f);
        const vec3 m = game::decodeNormal(vec3(1.0f, 0.5f, 0.5f)); // +x lean
        CHECK(m.x > 0.0f);
        CHECK_NEAR(std::sqrt(glm::dot(m, m)), 1.0f, 1e-4f); // unit length
    }
}

void testParallax() {
    using game::ParallaxLayer;
    using math::vec2;

    // layerOffset: a layer follows the camera by its motionScale (opposite sign — camera scrolls right,
    // layer slides left), plus a constant motionOffset.
    {
        ParallaxLayer l;
        l.motionScale = vec2(0.5f, 0.5f);
        const vec2 o = game::layerOffset(l, vec2(100.0f, 40.0f));
        CHECK_NEAR(o.x, -50.0f, 1e-4f);
        CHECK_NEAR(o.y, -20.0f, 1e-4f);
    }
    // A far backdrop (motionScale 0) is fixed on screen no matter how far the camera scrolls.
    {
        ParallaxLayer sky;
        sky.motionScale = vec2(0.0f, 0.0f);
        sky.motionOffset = vec2(7.0f, 3.0f);
        const vec2 a = game::layerOffset(sky, vec2(0.0f, 0.0f));
        const vec2 b = game::layerOffset(sky, vec2(9999.0f, -1234.0f));
        CHECK_NEAR(a.x, 7.0f, 1e-4f);
        CHECK_NEAR(b.x, 7.0f, 1e-4f); // unchanged
        CHECK_NEAR(b.y, 3.0f, 1e-4f);
    }
    // A near layer (scale > a far one) shifts MORE for the same camera move → the parallax effect.
    {
        ParallaxLayer far, near;
        far.motionScale = vec2(0.2f, 1.0f);
        near.motionScale = vec2(0.8f, 1.0f);
        const float df = game::layerOffset(far, vec2(100.0f, 0.0f)).x;
        const float dn = game::layerOffset(near, vec2(100.0f, 0.0f)).x;
        CHECK(std::fabs(dn) > std::fabs(df));
        CHECK_NEAR(df, -20.0f, 1e-4f);
        CHECK_NEAR(dn, -80.0f, 1e-4f);
    }
    // pmod is a positive modulo: result in [0, period) even for negative inputs.
    {
        CHECK_NEAR(game::pmod(5.0f, 3.0f), 2.0f, 1e-4f);
        CHECK_NEAR(game::pmod(6.0f, 3.0f), 0.0f, 1e-4f);
        CHECK_NEAR(game::pmod(-1.0f, 3.0f), 2.0f, 1e-4f);
        CHECK_NEAR(game::pmod(-50.0f, 40.0f), 30.0f, 1e-4f);
    }
    // firstTile lands in [-period, 0) so tiles drawn from it cover the left/top edge of the viewport.
    {
        const float ft = game::firstTile(-50.0f, 40.0f); // pmod(-50,40)=30 → 30-40 = -10
        CHECK_NEAR(ft, -10.0f, 1e-4f);
        CHECK(ft < 0.0f && ft >= -40.0f);
        // The tiles firstTile + k*period must bracket [0, extent].
        const int n = game::tileCount(200.0f, 40.0f); // ceil(5)+1 = 6
        CHECK(n == 6);
        CHECK(ft + static_cast<float>(n) * 40.0f >= 200.0f); // covers the right edge
    }
    // A non-tiled axis (period 0) returns the raw offset and a single tile.
    {
        CHECK_NEAR(game::firstTile(123.0f, 0.0f), 123.0f, 1e-4f);
        CHECK(game::tileCount(500.0f, 0.0f) == 1);
    }
}

void testAudioDsp() {
    const float sr = 44100.0f;

    // Helper: RMS of a tone of `freq` Hz after passing through a fresh copy of `filter`, skipping the
    // filter's start-up transient so we measure steady-state gain.
    auto toneRms = [&](audio::Biquad filter, float freq) {
        const int n = 4000;
        double acc = 0.0;
        int counted = 0;
        for (int i = 0; i < n; ++i) {
            const float x = std::sin(2.0f * 3.14159265f * freq * static_cast<float>(i) / sr);
            const float y = filter.process(x);
            if (i >= 1000) { // let the IIR settle
                acc += static_cast<double>(y) * static_cast<double>(y);
                ++counted;
            }
        }
        return std::sqrt(acc / static_cast<double>(counted));
    };

    // Lowpass passes DC (a constant input) at unity gain and near-fully passes a low tone.
    {
        audio::Biquad lp = audio::Biquad::lowpass(1000.0f, 0.707f, sr);
        float dc = 0.0f;
        for (int i = 0; i < 2000; ++i) {
            dc = lp.process(1.0f);
        }
        CHECK_NEAR(dc, 1.0f, 1e-3f);
        // A tone well below cutoff comes through much stronger than one well above it.
        const double lowPass = toneRms(audio::Biquad::lowpass(1000.0f, 0.707f, sr), 200.0f);
        const double highPass = toneRms(audio::Biquad::lowpass(1000.0f, 0.707f, sr), 8000.0f);
        CHECK(lowPass > 0.5);      // ~0.707 for a full-amplitude sine that passes
        CHECK(highPass < 0.05);    // heavily attenuated above cutoff
        CHECK(lowPass > highPass * 8.0);
    }

    // Highpass BLOCKS DC (a constant input decays to ~0) and blocks a low tone while passing a high one.
    {
        audio::Biquad hp = audio::Biquad::highpass(1000.0f, 0.707f, sr);
        float dc = 1.0f;
        for (int i = 0; i < 2000; ++i) {
            dc = hp.process(1.0f);
        }
        CHECK_NEAR(dc, 0.0f, 1e-3f);
        const double lowTone = toneRms(audio::Biquad::highpass(1000.0f, 0.707f, sr), 100.0f);
        const double highTone = toneRms(audio::Biquad::highpass(1000.0f, 0.707f, sr), 10000.0f);
        CHECK(highTone > 0.5);
        CHECK(lowTone < 0.05);
        CHECK(highTone > lowTone * 8.0);
    }

    // Delay: an impulse reappears after exactly `delaySamples`, scaled by `wet`, then a second, quieter
    // echo one delay-length later scaled by wet*feedback.
    {
        audio::Delay d;
        d.configure(10, 0.5f, 0.8f);
        std::vector<float> out;
        out.reserve(40);
        // Feed a single unit impulse followed by silence.
        out.push_back(d.process(1.0f));
        for (int i = 1; i < 40; ++i) {
            out.push_back(d.process(0.0f));
        }
        CHECK_NEAR(out[0], 1.0f, 1e-6f);        // dry impulse passes straight through
        CHECK_NEAR(out[10], 0.8f, 1e-6f);       // first echo: wet * 1.0
        CHECK_NEAR(out[20], 0.8f * 0.5f, 1e-6f);// second echo: wet * (echo fed back at feedback)
        CHECK_NEAR(out[5], 0.0f, 1e-6f);        // nothing between the taps
    }

    // Bus: an empty chain is a pass-through scaled by gain; a chain applies effects in order.
    {
        audio::Bus bus;
        bus.gain = 0.5f;
        CHECK_NEAR(bus.process(1.0f), 0.5f, 1e-6f); // empty chain, gain only

        audio::Bus chain;
        // Lowpass then delay: verify the delay tap still fires through the chain (order-preserving).
        chain.add(std::make_unique<audio::BiquadEffect>(audio::Biquad::lowpass(2000.0f, 0.707f, sr)));
        audio::Delay dd;
        dd.configure(8, 0.0f, 1.0f); // no feedback, full wet
        chain.add(std::make_unique<audio::DelayEffect>(dd));
        std::vector<float> sig(32, 0.0f);
        sig[0] = 1.0f;
        chain.processBuffer(sig);
        // Energy shows up both immediately (dry, lowpassed) and one delay-length later (the echo tap).
        double early = 0.0, echo = 0.0;
        for (int i = 0; i < 6; ++i) {
            early += std::fabs(static_cast<double>(sig[static_cast<std::size_t>(i)]));
        }
        for (int i = 8; i < 14; ++i) {
            echo += std::fabs(static_cast<double>(sig[static_cast<std::size_t>(i)]));
        }
        CHECK(early > 0.0);
        CHECK(echo > 0.0);
    }
}

void testAudioEffects() {
    // ---- Distortion (tanh waveshaper) ----
    {
        audio::Distortion d;
        d.drive = 3.0f;
        // Odd symmetry: f(-x) == -f(x).
        CHECK_NEAR(d.process(-0.4f), -d.process(0.4f), 1e-5f);
        // Full-scale preserved: f(1) == 1 (and f(0) == 0).
        CHECK_NEAR(d.process(1.0f), 1.0f, 1e-5f);
        CHECK_NEAR(d.process(0.0f), 0.0f, 1e-6f);
        // Monotonic increasing across the range.
        float prev = d.process(-1.0f);
        bool mono = true;
        for (float x = -0.9f; x <= 1.0f; x += 0.1f) {
            const float y = d.process(x);
            if (y < prev - 1e-6f) mono = false;
            prev = y;
        }
        CHECK(mono);
        // Soft compression of dynamics: a half-amplitude input keeps MORE than half the output level.
        CHECK(d.process(0.5f) / d.process(1.0f) > 0.5f);
    }

    // ---- Compressor ----
    {
        // A loud, sustained input (above threshold) is pulled DOWN toward the compressed level.
        audio::Compressor c;
        c.threshold = 0.5f;
        c.ratio = 4.0f;
        c.attack = 0.02f;
        c.release = 0.002f;
        float out = 0.0f;
        for (int i = 0; i < 4000; ++i) {
            out = c.process(1.0f); // DC at full scale so the envelope settles
        }
        // Steady-state gain = compressed/env with env->1: 0.5 + 0.5/4 = 0.625.
        CHECK(out < 0.9f);
        CHECK_NEAR(out, 0.625f, 0.03f);

        // A quiet input (below threshold) passes essentially unchanged.
        audio::Compressor c2;
        c2.threshold = 0.5f;
        float q = 0.0f;
        for (int i = 0; i < 4000; ++i) {
            q = c2.process(0.2f);
        }
        CHECK_NEAR(q, 0.2f, 1e-3f);
    }

    // ---- Reverb ----
    {
        const float sr = 44100.0f;
        // wet = 0 -> the reverb is a pass-through (dry only).
        audio::Reverb dryOnly;
        dryOnly.configure(sr, 0.84f, 0.2f, 0.0f);
        CHECK_NEAR(dryOnly.process(0.7f), 0.7f, 1e-5f);

        // wet > 0 -> an impulse produces a decaying TAIL: energy after the impulse sample is nonzero.
        audio::Reverb rev;
        rev.configure(sr, 0.84f, 0.2f, 0.5f);
        rev.process(1.0f); // the impulse
        double tail = 0.0;
        for (int i = 0; i < 8000; ++i) {
            tail += std::fabs(static_cast<double>(rev.process(0.0f)));
        }
        CHECK(tail > 0.1); // the room rings out well past the input

        // Comb feedback re-emits the impulse after its delay length.
        audio::Comb comb;
        comb.configure(20, 0.8f, 0.0f);
        std::vector<float> out;
        out.push_back(comb.process(1.0f));
        for (int i = 1; i < 60; ++i) {
            out.push_back(comb.process(0.0f));
        }
        CHECK_NEAR(out[20], 1.0f, 1e-4f);       // first repeat at the delay length
        CHECK_NEAR(out[40], 0.8f, 1e-3f);       // second repeat, decayed by feedback
        CHECK_NEAR(out[10], 0.0f, 1e-5f);       // silence between taps
    }
}

void testADSR() {
    using audio::ADSR;

    // A helper to run the envelope for `secs` seconds at 1 kHz, returning the final level.
    auto run = [](ADSR& e, float secs) {
        const float dt = 1.0f / 1000.0f;
        const int n = static_cast<int>(secs * 1000.0f + 0.5f);
        float lv = e.level;
        for (int i = 0; i < n; ++i) {
            lv = e.process(dt);
        }
        return lv;
    };

    // Full A/D/S/R contour with clear timings.
    {
        ADSR e;
        e.attack = 0.1f;
        e.decay = 0.2f;
        e.sustain = 0.5f;
        e.release = 0.3f;
        CHECK(!e.active());

        e.noteOn();
        CHECK(e.active());
        CHECK(e.stage == ADSR::Stage::Attack);

        // Halfway through the attack the linear ramp is ~0.5.
        run(e, 0.05f);
        CHECK_NEAR(e.level, 0.5f, 0.03f);

        // By the end of the attack it has hit 1.0 and moved into decay.
        run(e, 0.06f); // total ~0.11s > attack
        CHECK_NEAR(e.level, 1.0f, 0.05f);
        CHECK(e.stage == ADSR::Stage::Decay || e.stage == ADSR::Stage::Sustain);

        // After the decay completes it settles at the sustain level and HOLDS there.
        run(e, 0.3f);
        CHECK(e.stage == ADSR::Stage::Sustain);
        CHECK_NEAR(e.level, 0.5f, 1e-3f);
        run(e, 1.0f); // hold a long time
        CHECK_NEAR(e.level, 0.5f, 1e-3f);
        CHECK(e.active());

        // Release: from sustain 0.5 down to 0 over 0.3s. Halfway (~0.15s) it's ~0.25.
        e.noteOff();
        CHECK(e.stage == ADSR::Stage::Release);
        run(e, 0.15f);
        CHECK_NEAR(e.level, 0.25f, 0.03f);
        // After the full release it reaches 0 and goes Idle.
        run(e, 0.2f);
        CHECK_NEAR(e.level, 0.0f, 1e-3f);
        CHECK(!e.active());
        CHECK(e.stage == ADSR::Stage::Idle);
    }

    // Release can start mid-attack: the tail scales from the level reached, not from 1.
    {
        ADSR e;
        e.attack = 0.2f;
        e.decay = 0.1f;
        e.sustain = 0.8f;
        e.release = 0.2f;
        e.noteOn();
        run(e, 0.1f); // halfway up the attack -> ~0.5
        CHECK_NEAR(e.level, 0.5f, 0.03f);
        const float atRelease = e.level;
        e.noteOff();
        // Halfway through the release it's ~half of the release-start level.
        run(e, 0.1f);
        CHECK_NEAR(e.level, atRelease * 0.5f, 0.05f);
        run(e, 0.15f);
        CHECK_NEAR(e.level, 0.0f, 1e-3f);
    }

    // Zero attack snaps to 1 in a single step; zero decay/sustain 0 lands at 0 quickly (a stab).
    {
        ADSR e;
        e.attack = 0.0f;
        e.decay = 0.05f;
        e.sustain = 0.0f;
        e.release = 0.05f;
        e.noteOn();
        e.process(1.0f / 1000.0f);
        CHECK_NEAR(e.level, 1.0f, 1e-4f); // instant attack
        run(e, 0.06f);                    // decay to sustain 0
        CHECK_NEAR(e.level, 0.0f, 1e-3f);
        CHECK(e.stage == ADSR::Stage::Sustain);
    }

    // Sustain == 1: decay is a no-op, level stays at 1 while held.
    {
        ADSR e;
        e.attack = 0.05f;
        e.decay = 0.2f;
        e.sustain = 1.0f;
        e.release = 0.1f;
        e.noteOn();
        run(e, 0.1f);
        CHECK_NEAR(e.level, 1.0f, 1e-3f);
        run(e, 0.5f);
        CHECK_NEAR(e.level, 1.0f, 1e-3f);
    }
}

void testSpatial2D() {
    using math::vec2;
    audio::Listener2D lis;
    lis.pos = vec2(0.0f, 0.0f);
    lis.right = vec2(1.0f, 0.0f); // +x is the listener's right

    // Attenuation: full inside refDistance, zero past maxDistance, InverseDistance halves at 2x ref.
    CHECK_NEAR(audio::attenuation(5.0f, 10.0f, 100.0f, audio::Attenuation::InverseDistance), 1.0f, 1e-5f);
    CHECK_NEAR(audio::attenuation(20.0f, 10.0f, 100.0f, audio::Attenuation::InverseDistance), 0.5f, 1e-5f);
    CHECK(audio::attenuation(200.0f, 10.0f, 100.0f, audio::Attenuation::InverseDistance) == 0.0f);
    // Linear halfway between ref and max = 0.5.
    CHECK_NEAR(audio::attenuation(50.0f, 0.0f, 100.0f, audio::Attenuation::Linear), 0.5f, 1e-5f);

    // A source right at the listener: full attenuation, centred (equal L/R at constant power).
    {
        const auto g = audio::spatialize(lis, vec2(0, 0), 1.0f, 10.0f, 100.0f);
        CHECK_NEAR(g.left, 0.70710678f, 1e-4f);
        CHECK_NEAR(g.right, 0.70710678f, 1e-4f);
    }
    // Hard right: all energy in the right channel; hard left: all in the left.
    {
        const auto gr = audio::spatialize(lis, vec2(100, 0), 1.0f, 10.0f, 1000.0f);
        CHECK(gr.right > gr.left);
        CHECK_NEAR(gr.left, 0.0f, 1e-4f);
        const float att = 10.0f / 100.0f; // inverse-distance at d=100, ref=10
        CHECK_NEAR(gr.right, att, 1e-4f);

        const auto gl = audio::spatialize(lis, vec2(-100, 0), 1.0f, 10.0f, 1000.0f);
        CHECK(gl.left > gl.right);
        CHECK_NEAR(gl.right, 0.0f, 1e-4f);
    }
    // Past max distance -> silent.
    {
        const auto g = audio::spatialize(lis, vec2(0, 500), 1.0f, 10.0f, 100.0f);
        CHECK(g.left == 0.0f);
        CHECK(g.right == 0.0f);
    }
    // Constant power: L^2 + R^2 == (baseVolume*attenuation)^2 for any pan.
    {
        const auto g = audio::spatialize(lis, vec2(40, 30), 0.8f, 10.0f, 1000.0f); // d=50
        const float att = 10.0f / 50.0f;
        const float expected = 0.8f * att;
        CHECK_NEAR(std::sqrt(g.left * g.left + g.right * g.right), expected, 1e-4f);
    }
}

void testTwoBoneIK() {
    using math::vec2;
    const vec2 root(0.0f, 0.0f);
    const float l1 = 10.0f, l2 = 8.0f;
    auto len = [](vec2 a, vec2 b) {
        const vec2 d = b - a;
        return std::sqrt(d.x * d.x + d.y * d.y);
    };

    // Reachable target: the end effector lands exactly on it, and the bone lengths are preserved.
    {
        const vec2 target(9.0f, 6.0f); // |target| ~= 10.8, within [2, 18]
        auto s = anim::solveTwoBoneIK(root, l1, l2, target, +1.0f);
        CHECK(s.reachable);
        CHECK_NEAR(s.end.x, target.x, 1e-3f);
        CHECK_NEAR(s.end.y, target.y, 1e-3f);
        CHECK_NEAR(len(root, s.mid), l1, 1e-3f); // upper bone length
        CHECK_NEAR(len(s.mid, s.end), l2, 1e-3f); // lower bone length
    }

    // bendSign flips the elbow to the opposite side of the root->target line (cross product changes sign).
    {
        const vec2 target(12.0f, 2.0f);
        auto a = anim::solveTwoBoneIK(root, l1, l2, target, +1.0f);
        auto b = anim::solveTwoBoneIK(root, l1, l2, target, -1.0f);
        const auto cross = [&](const anim::IKResult& r) {
            return (r.mid.x - root.x) * (target.y - root.y) - (r.mid.y - root.y) * (target.x - root.x);
        };
        CHECK(cross(a) * cross(b) < 0.0f); // elbows on opposite sides
        // Both still reach the target with correct bone lengths.
        CHECK_NEAR(len(root, a.mid), l1, 1e-3f);
        CHECK_NEAR(len(a.mid, a.end), l2, 1e-3f);
        CHECK_NEAR(len(root, b.mid), l1, 1e-3f);
        CHECK_NEAR(len(b.mid, b.end), l2, 1e-3f);
    }

    // Out of reach: the chain points straight at the target, fully extended (root, mid, end collinear).
    {
        const vec2 target(30.0f, 0.0f); // |target| = 30 > 18
        auto s = anim::solveTwoBoneIK(root, l1, l2, target, +1.0f);
        CHECK(!s.reachable);
        CHECK_NEAR(s.mid.x, l1, 1e-3f);        // elbow at len1 along +x
        CHECK_NEAR(s.mid.y, 0.0f, 1e-3f);
        CHECK_NEAR(s.end.x, l1 + l2, 1e-3f);   // hand at len1+len2 along +x
        CHECK_NEAR(s.end.y, 0.0f, 1e-3f);
    }

    // Exactly at full stretch: reachable, straight arm ending on the target.
    {
        const vec2 target(l1 + l2, 0.0f);
        auto s = anim::solveTwoBoneIK(root, l1, l2, target, +1.0f);
        CHECK(s.reachable);
        CHECK_NEAR(s.end.x, l1 + l2, 1e-2f);
        CHECK_NEAR(s.mid.y, 0.0f, 1e-2f); // elbow on the line (no bend possible at full stretch)
    }
}

void testFabrik() {
    using math::vec2;

    // Helper: are all bone lengths preserved vs the originals?
    auto lengthsPreserved = [](const std::vector<vec2>& js, const std::vector<float>& orig) {
        for (std::size_t i = 0; i + 1 < js.size(); ++i) {
            const vec2 d = js[i + 1] - js[i];
            const float l = std::sqrt(glm::dot(d, d));
            if (std::fabs(l - orig[i]) > 1e-3f) return false;
        }
        return true;
    };

    // A 4-joint chain laid straight along +x (three unit bones, total reach 3).
    const std::vector<float> lens = {1.0f, 1.0f, 1.0f};
    auto fresh = [] {
        return std::vector<vec2>{{0, 0}, {1, 0}, {2, 0}, {3, 0}};
    };

    // Reachable target: the tip lands on it, bone lengths preserved, base fixed.
    {
        std::vector<vec2> js = fresh();
        anim::solveFabrik(js, vec2(2.0f, 1.5f), 20, 1e-4f);
        const vec2 end = js.back();
        CHECK_NEAR(end.x, 2.0f, 1e-2f);
        CHECK_NEAR(end.y, 1.5f, 1e-2f);
        CHECK(lengthsPreserved(js, lens));
        CHECK_NEAR(js.front().x, 0.0f, 1e-4f); // base pinned
        CHECK_NEAR(js.front().y, 0.0f, 1e-4f);
    }

    // A target straight up at distance 2 (< reach 3): reached exactly.
    {
        std::vector<vec2> js = fresh();
        anim::solveFabrik(js, vec2(0.0f, 2.0f), 20, 1e-4f);
        CHECK_NEAR(js.back().x, 0.0f, 1e-2f);
        CHECK_NEAR(js.back().y, 2.0f, 1e-2f);
        CHECK(lengthsPreserved(js, lens));
    }

    // Unreachable target (distance 5 > reach 3): the chain straightens toward it (collinear, full
    // stretch), tip at exactly `total` from the base along the target direction.
    {
        std::vector<vec2> js = fresh();
        anim::solveFabrik(js, vec2(5.0f, 0.0f), 20, 1e-4f);
        CHECK_NEAR(js.back().x, 3.0f, 1e-3f); // straightened along +x to the full reach
        CHECK_NEAR(js.back().y, 0.0f, 1e-3f);
        CHECK(lengthsPreserved(js, lens));
        // Diagonal unreachable target: tip lies on the ray to the target at distance `total`.
        std::vector<vec2> js2 = fresh();
        const vec2 t(6.0f, 8.0f); // distance 10 > 3
        anim::solveFabrik(js2, t, 20, 1e-4f);
        const vec2 dir = t / 10.0f;
        CHECK_NEAR(js2.back().x, dir.x * 3.0f, 1e-3f);
        CHECK_NEAR(js2.back().y, dir.y * 3.0f, 1e-3f);
    }

    // Single-bone chain (2 joints): rotates to point at a reachable target at the bone length.
    {
        std::vector<vec2> js{{0, 0}, {1, 0}};
        anim::solveFabrik(js, vec2(0.0f, 1.0f), 20, 1e-4f);
        CHECK_NEAR(js[1].x, 0.0f, 1e-3f);
        CHECK_NEAR(js[1].y, 1.0f, 1e-3f);
    }

    // Degenerate: fewer than 2 joints is a no-op.
    {
        std::vector<vec2> js{{5, 5}};
        anim::solveFabrik(js, vec2(0, 0));
        CHECK_NEAR(js[0].x, 5.0f, 1e-6f);
    }
}

void testGoap() {
    namespace goap = game::goap;

    // A "make fire" survival scenario. Facts:
    enum { HasAxe = 0, AtForest = 1, HasWood = 2, AtCamp = 3, HasFire = 4 };

    std::vector<goap::Action> lib;
    lib.push_back(goap::Action{"GetAxe"}.sets(HasAxe).withCost(2.0f));
    lib.push_back(goap::Action{"GoForest"}.sets(AtForest).clears(AtCamp).withCost(1.0f));
    lib.push_back(
        goap::Action{"ChopWood"}.needs(HasAxe, true).needs(AtForest, true).sets(HasWood).withCost(3.0f));
    lib.push_back(goap::Action{"GoCamp"}.sets(AtCamp).clears(AtForest).withCost(1.0f));
    lib.push_back(
        goap::Action{"BuildFire"}.needs(HasWood, true).needs(AtCamp, true).sets(HasFire).withCost(1.0f));
    // A decoy shortcut: scavenge wood with no axe, but expensive enough that the axe path wins.
    lib.push_back(goap::Action{"ScavengeWood"}.sets(HasWood).withCost(10.0f));

    const goap::State start = goap::bit(AtCamp); // begin at camp, nothing else
    const goap::Condition goal = goap::Condition{}.require(HasFire, true);

    // Optimal plan: GetAxe(2) + GoForest(1) + ChopWood(3) + GoCamp(1) + BuildFire(1) = 8, beating the
    // ScavengeWood(10)+BuildFire(1)=11 shortcut.
    {
        goap::Plan p = goap::plan(start, goal, lib);
        CHECK(p.found);
        CHECK(p.steps.size() == 5);
        CHECK_NEAR(p.cost, 8.0f, 1e-4f);

        // Replay the plan and confirm each action's precondition held and the goal is reached.
        goap::State s = start;
        bool scavenged = false;
        for (int idx : p.steps) {
            CHECK(goap::satisfied(s, lib[static_cast<std::size_t>(idx)].pre));
            if (lib[static_cast<std::size_t>(idx)].name == "ScavengeWood") scavenged = true;
            s = goap::apply(s, lib[static_cast<std::size_t>(idx)]);
        }
        CHECK(goap::satisfied(s, goal));
        CHECK(!scavenged); // the optimal plan avoids the expensive decoy
        // Last action must be the one that produces fire.
        CHECK(lib[static_cast<std::size_t>(p.steps.back())].name == "BuildFire");
    }

    // If the axe path is made costlier than the shortcut, the planner switches to ScavengeWood.
    {
        std::vector<goap::Action> lib2 = lib;
        lib2[0].cost = 20.0f; // GetAxe now absurdly expensive
        goap::Plan p = goap::plan(start, goal, lib2);
        CHECK(p.found);
        CHECK(p.steps.size() == 2); // ScavengeWood + BuildFire
        CHECK_NEAR(p.cost, 11.0f, 1e-4f);
        CHECK(lib2[static_cast<std::size_t>(p.steps.front())].name == "ScavengeWood");
    }

    // Goal already satisfied: empty plan, zero cost, found.
    {
        goap::Plan p = goap::plan(goap::bit(HasFire), goal, lib);
        CHECK(p.found);
        CHECK(p.steps.empty());
        CHECK_NEAR(p.cost, 0.0f, 1e-6f);
    }

    // Unreachable goal: need a fact no action can produce.
    {
        const goap::Condition impossible = goap::Condition{}.require(7, true); // fact 7 untouched by any action
        goap::Plan p = goap::plan(start, impossible, lib);
        CHECK(!p.found);
        CHECK(p.steps.empty());
    }

    // Condition partial-match semantics: cares only about its masked bits.
    {
        goap::State s = goap::bit(HasWood) | goap::bit(AtCamp);
        CHECK(goap::satisfied(s, goap::Condition{}.require(HasWood, true)));
        CHECK(goap::satisfied(s, goap::Condition{}.require(HasAxe, false)));
        CHECK(!goap::satisfied(s, goap::Condition{}.require(HasWood, false)));
        // A two-fact condition must match both.
        CHECK(goap::satisfied(s, goap::Condition{}.require(HasWood, true).require(AtCamp, true)));
        CHECK(!goap::satisfied(s, goap::Condition{}.require(HasWood, true).require(AtForest, true)));
    }

    // Empty library with an unmet goal is simply unreachable (heuristic must not divide by zero, etc.).
    {
        std::vector<goap::Action> none;
        goap::Plan p = goap::plan(start, goal, none);
        CHECK(!p.found);
    }
}

void testFlowField() {
    using math::vec2;

    // 6x1 open corridor, goal at the right end: cost rises leftward, every cell flows toward +x.
    {
        game::FlowField ff;
        std::vector<std::uint8_t> blocked(6, 0);
        ff.build(6, 1, blocked, 5, 0);
        CHECK_NEAR(ff.costAt(5, 0), 0.0f, 1e-4f); // goal
        CHECK_NEAR(ff.costAt(4, 0), 1.0f, 1e-4f);
        CHECK_NEAR(ff.costAt(0, 0), 5.0f, 1e-4f);
        for (int x = 0; x < 5; ++x) {
            CHECK_NEAR(ff.flowAt(x, 0).x, 1.0f, 1e-4f); // points toward the goal
            CHECK_NEAR(ff.flowAt(x, 0).y, 0.0f, 1e-4f);
        }
        CHECK(ff.reachable(0, 0));
    }

    // Open grid, goal at a corner: flow at each cell points (dot > 0) toward the goal, and a diagonal
    // approach costs less than going around two sides.
    {
        game::FlowField ff;
        std::vector<std::uint8_t> blocked(static_cast<std::size_t>(5 * 5), 0);
        ff.build(5, 5, blocked, 0, 0); // goal at (0,0)
        // The far corner (4,4) reaches the goal by a straight diagonal: cost = 4*sqrt2.
        CHECK_NEAR(ff.costAt(4, 4), 4.0f * 1.41421356f, 1e-3f);
        // Its flow heads back toward the goal (negative x and y).
        CHECK(ff.flowAt(4, 4).x < 0.0f);
        CHECK(ff.flowAt(4, 4).y < 0.0f);
        // A mid cell's flow points generally at the goal.
        const vec2 f = ff.flowAt(3, 1);
        const vec2 toGoal = glm::normalize(vec2(0.0f, 0.0f) - vec2(3.0f, 1.0f));
        CHECK(glm::dot(f, toGoal) > 0.5f);
    }

    // A wall forces a detour: cells behind it must route around, and a fully-walled-off cell is
    // unreachable with zero flow.
    {
        // 5x5, a vertical wall at x=2 for y=0..3, leaving a gap at y=4. Goal on the right at (4,2).
        const int w = 5, h = 5;
        std::vector<std::uint8_t> blocked(static_cast<std::size_t>(w * h), 0);
        for (int y = 0; y <= 3; ++y) {
            blocked[static_cast<std::size_t>(y * w + 2)] = 1;
        }
        game::FlowField ff;
        ff.build(w, h, blocked, 4, 2);

        // A cell on the left of the wall is reachable only via the gap at the bottom: its cost is much
        // more than the straight-line distance, and it must NOT flow straight into the wall (+x).
        CHECK(ff.reachable(0, 2));
        CHECK(ff.costAt(0, 2) > 4.0f); // detour is longer than the ~4 straight cells
        const vec2 f = ff.flowAt(1, 2); // just left of the wall
        // The neighbour at (2,2) is a wall, so flow cannot be purely +x with y≈0; it must steer toward
        // the gap (downward).
        CHECK(f.y > 0.2f);

        // The wall cells themselves have zero flow.
        CHECK_NEAR(ff.flowAt(2, 1).x, 0.0f, 1e-6f);
        CHECK_NEAR(ff.flowAt(2, 1).y, 0.0f, 1e-6f);
    }

    // An isolated region (goal boxed off) leaves outside cells unreachable.
    {
        const int w = 5, h = 5;
        std::vector<std::uint8_t> blocked(static_cast<std::size_t>(w * h), 0);
        // Box the goal cell (2,2) in on all four sides.
        blocked[static_cast<std::size_t>(1 * w + 2)] = 1;
        blocked[static_cast<std::size_t>(3 * w + 2)] = 1;
        blocked[static_cast<std::size_t>(2 * w + 1)] = 1;
        blocked[static_cast<std::size_t>(2 * w + 3)] = 1;
        game::FlowField ff;
        ff.build(w, h, blocked, 2, 2);
        CHECK(ff.reachable(2, 2));      // the goal itself
        CHECK(!ff.reachable(0, 0));     // walled out (diagonals blocked by corner rule)
        CHECK_NEAR(ff.costAt(0, 0), game::FlowField::kUnreachable, 1.0f);
    }

    // sampleFlow maps a world position through the cell size to the right cell.
    {
        game::FlowField ff;
        std::vector<std::uint8_t> blocked(6, 0);
        ff.build(6, 1, blocked, 5, 0);
        const vec2 f = ff.sampleFlow(vec2(32.0f + 5.0f, 5.0f), 32.0f); // world x 37 -> cell 1
        CHECK_NEAR(f.x, 1.0f, 1e-4f);
        const vec2 out = ff.sampleFlow(vec2(-100.0f, 0.0f), 32.0f); // outside -> zero
        CHECK_NEAR(out.x, 0.0f, 1e-6f);
    }
}

void testBlendSpace() {
    using math::vec2;

    // 1-D: three samples on a line; midpoints blend the neighbours, ends clamp.
    {
        anim::BlendSpace1D bs;
        bs.addPoint(0.0f, 10);
        bs.addPoint(2.0f, 20); // add out of order to exercise sorted insertion
        bs.addPoint(1.0f, 15);
        CHECK(bs.size() == 3);

        auto mid = bs.weights(0.5f); // halfway between id 10 (0.0) and id 15 (1.0)
        CHECK(mid.size() == 2);
        CHECK(mid[0].id == 10);
        CHECK(mid[1].id == 15);
        CHECK_NEAR(mid[0].weight, 0.5f, 1e-5f);
        CHECK_NEAR(mid[1].weight, 0.5f, 1e-5f);

        auto q = bs.weights(1.25f); // between 15 (1.0) and 20 (2.0), 25% toward 20
        CHECK(q.size() == 2);
        CHECK(q[0].id == 15);
        CHECK(q[1].id == 20);
        CHECK_NEAR(q[1].weight, 0.25f, 1e-5f);

        auto lo = bs.weights(-3.0f); // clamped to the low end
        CHECK(lo.size() == 1);
        CHECK(lo[0].id == 10);
        CHECK_NEAR(lo[0].weight, 1.0f, 1e-5f);

        auto hi = bs.weights(9.0f); // clamped to the high end
        CHECK(hi.size() == 1);
        CHECK(hi[0].id == 20);

        auto exact = bs.weights(1.0f); // exactly on a sample -> that sample only
        CHECK(exact.size() == 1);
        CHECK(exact[0].id == 15);
    }

    // 2-D: a unit right triangle; barycentric weights, a vertex query, and an outside clamp.
    {
        anim::BlendSpace2D bs;
        const int a = bs.addPoint(vec2{0, 0}, 100);
        const int b = bs.addPoint(vec2{1, 0}, 200);
        const int c = bs.addPoint(vec2{0, 1}, 300);
        bs.addTriangle(a, b, c);

        auto w = bs.weights(vec2{0.25f, 0.25f}); // u=0.5 (a), v=0.25 (b), w=0.25 (c)
        CHECK(w.size() == 3);
        CHECK(w[0].id == 100);
        CHECK_NEAR(w[0].weight, 0.5f, 1e-5f);
        CHECK_NEAR(w[1].weight, 0.25f, 1e-5f);
        CHECK_NEAR(w[2].weight, 0.25f, 1e-5f);
        float sum = w[0].weight + w[1].weight + w[2].weight;
        CHECK_NEAR(sum, 1.0f, 1e-5f);

        auto v = bs.weights(vec2{1, 0}); // exactly on vertex b -> weight concentrates there
        CHECK_NEAR(v[1].weight, 1.0f, 1e-4f);
        CHECK_NEAR(v[0].weight, 0.0f, 1e-4f);

        auto out = bs.weights(vec2{2.0f, 2.0f}); // far outside -> clamped, still sums to 1, no negatives
        float os = 0.0f;
        for (const auto& e : out) {
            CHECK(e.weight >= -1e-6f);
            os += e.weight;
        }
        CHECK_NEAR(os, 1.0f, 1e-5f);
    }

    // blendPosesWeighted: N-way weighted blend reproduces a single pose and midpoint blends translation.
    {
        using anim::JointPose;
        std::vector<JointPose> p0(1), p1(1), p2(1);
        p0[0].translation = math::vec3(0, 0, 0);
        p1[0].translation = math::vec3(10, 0, 0);
        p2[0].translation = math::vec3(0, 10, 0);

        std::vector<JointPose> out;
        anim::blendPosesWeighted({&p0, &p1, &p2}, {0.5f, 0.25f, 0.25f}, out);
        // Weighted average of translations: 0.5*(0,0)+0.25*(10,0)+0.25*(0,10) = (2.5, 2.5).
        CHECK_NEAR(out[0].translation.x, 2.5f, 1e-4f);
        CHECK_NEAR(out[0].translation.y, 2.5f, 1e-4f);

        anim::blendPosesWeighted({&p0, &p1, &p2}, {1.0f, 0.0f, 0.0f}, out); // all weight on p0
        CHECK_NEAR(out[0].translation.x, 0.0f, 1e-5f);
        CHECK_NEAR(out[0].translation.y, 0.0f, 1e-5f);
    }
}

void testBlendTree() {
    using anim::BlendTree;
    using anim::JointPose;
    using anim::Pose;

    // External pose table: three single-joint poses at distinct translations.
    Pose idle(1), walk(1), run(1);
    idle[0].translation = math::vec3(0, 0, 0);
    walk[0].translation = math::vec3(10, 0, 0);
    run[0].translation = math::vec3(30, 0, 0);
    std::vector<const Pose*> inputs{&idle, &walk, &run};

    // Blend2: cross-fade idle(0) -> walk(1) by "speed".
    {
        BlendTree t;
        const int i0 = t.addInput(0);
        const int i1 = t.addInput(1);
        const int b = t.addBlend2(i0, i1, "speed");
        t.setRoot(b);

        Pose out;
        t.setParam("speed", 0.0f);
        t.evaluate(inputs, out);
        CHECK(out.size() == 1);
        CHECK_NEAR(out[0].translation.x, 0.0f, 1e-4f); // all idle

        t.setParam("speed", 1.0f);
        t.evaluate(inputs, out);
        CHECK_NEAR(out[0].translation.x, 10.0f, 1e-4f); // all walk

        t.setParam("speed", 0.5f);
        t.evaluate(inputs, out);
        CHECK_NEAR(out[0].translation.x, 5.0f, 1e-4f); // halfway

        // A param that was never set defaults to 0 -> all idle.
        BlendTree t2;
        t2.setRoot(t2.addBlend2(t2.addInput(0), t2.addInput(1), "unset"));
        Pose o2;
        t2.evaluate(inputs, o2);
        CHECK_NEAR(o2[0].translation.x, 0.0f, 1e-4f);
    }

    // BlendSpace1 node over three child inputs, selected by "gait": a nested 1-D blend space.
    {
        BlendTree t;
        const int ci = t.addInput(0); // idle at 0
        const int cw = t.addInput(1); // walk at 10
        const int cr = t.addInput(2); // run at 30
        const int space = t.addBlendSpace1("gait");
        t.addBlendPoint(space, 0.0f, ci);
        t.addBlendPoint(space, 1.0f, cw);
        t.addBlendPoint(space, 2.0f, cr);
        t.setRoot(space);

        Pose out;
        t.setParam("gait", 0.0f);
        t.evaluate(inputs, out);
        CHECK_NEAR(out[0].translation.x, 0.0f, 1e-4f); // idle

        t.setParam("gait", 1.0f);
        t.evaluate(inputs, out);
        CHECK_NEAR(out[0].translation.x, 10.0f, 1e-4f); // walk

        t.setParam("gait", 1.5f); // halfway between walk(10) and run(30) -> 20
        t.evaluate(inputs, out);
        CHECK_NEAR(out[0].translation.x, 20.0f, 1e-4f);

        t.setParam("gait", 5.0f); // beyond the top -> clamps to run
        t.evaluate(inputs, out);
        CHECK_NEAR(out[0].translation.x, 30.0f, 1e-4f);
    }

    // Add2: an additive delta layered on a base, scaled by "amount".
    {
        Pose base(1), delta(1);
        base[0].translation = math::vec3(4, 0, 0);
        delta[0].translation = math::vec3(0, 6, 0); // additive offset in Y
        std::vector<const Pose*> in{&base, &delta};

        BlendTree t;
        const int nb = t.addInput(0);
        const int nd = t.addInput(1);
        const int add = t.addAdd2(nb, nd, "amount");
        t.setRoot(add);

        Pose out;
        t.setParam("amount", 0.0f);
        t.evaluate(in, out);
        CHECK_NEAR(out[0].translation.x, 4.0f, 1e-4f);
        CHECK_NEAR(out[0].translation.y, 0.0f, 1e-4f); // no additive contribution

        t.setParam("amount", 1.0f);
        t.evaluate(in, out);
        CHECK_NEAR(out[0].translation.x, 4.0f, 1e-4f); // base X untouched
        CHECK_NEAR(out[0].translation.y, 6.0f, 1e-4f); // full additive Y

        t.setParam("amount", 0.5f);
        t.evaluate(in, out);
        CHECK_NEAR(out[0].translation.y, 3.0f, 1e-4f); // half additive Y
    }

    // Nested tree: a walk/run blend space cross-faded into a jump pose by an "air" parameter — the
    // canonical AnimationTree shape (a sub-blend feeding a Blend2).
    {
        Pose jump(1);
        jump[0].translation = math::vec3(0, 100, 0);
        std::vector<const Pose*> in{&idle, &walk, &run, &jump};

        BlendTree t;
        const int space = t.addBlendSpace1("gait");
        t.addBlendPoint(space, 0.0f, t.addInput(1)); // walk
        t.addBlendPoint(space, 1.0f, t.addInput(2)); // run
        const int jin = t.addInput(3);
        const int mix = t.addBlend2(space, jin, "air");
        t.setRoot(mix);

        Pose out;
        t.setParam("gait", 0.5f); // halfway walk(10)/run(30) -> 20 on the ground
        t.setParam("air", 0.0f);
        t.evaluate(in, out);
        CHECK_NEAR(out[0].translation.x, 20.0f, 1e-4f);
        CHECK_NEAR(out[0].translation.y, 0.0f, 1e-4f);

        t.setParam("air", 1.0f); // fully airborne -> the jump pose
        t.evaluate(in, out);
        CHECK_NEAR(out[0].translation.y, 100.0f, 1e-4f);
    }

    // Degenerate cases: no root yields an empty pose; an out-of-range root too.
    {
        BlendTree t;
        Pose out;
        t.evaluate(inputs, out);
        CHECK(out.empty());
        t.setRoot(99);
        t.evaluate(inputs, out);
        CHECK(out.empty());
    }
}

void testAnimStateMachine() {
    using anim::AnimStateMachine;

    // Controllable inputs the transition conditions read.
    bool moving = false, jump = false;

    auto build = [&] {
        AnimStateMachine sm;
        sm.addState("idle", 0);
        sm.addState("move", 1);
        sm.addState("jump", 2);
        sm.addTransition("idle", "move", 0.2f, [&] { return moving; });
        sm.addTransition("move", "idle", 0.2f, [&] { return !moving; });
        sm.addTransition("idle", "jump", 0.1f, [&] { return jump; });
        sm.addTransition("move", "jump", 0.1f, [&] { return jump; });
        sm.setStart("idle");
        return sm;
    };

    // Starts in idle at full weight.
    {
        auto sm = build();
        const auto a = sm.active();
        CHECK(a.size() == 1);
        CHECK(a[0].id == 0);
        CHECK_NEAR(a[0].weight, 1.0f, 1e-6f);
        CHECK(!sm.transitioning());
        CHECK(sm.currentName() == "idle");
    }

    // Condition fires -> cross-fade idle->move; weights always sum to 1; midpoint ~0.5/0.5.
    {
        auto sm = build();
        moving = true;
        sm.update(1.0f / 60.0f); // begins the transition (fade 0.2s)
        CHECK(sm.transitioning());
        CHECK(sm.current() == 1); // heading toward move
        // Step to roughly half the fade.
        for (int i = 0; i < 5; ++i) {
            sm.update(1.0f / 60.0f);
        }
        auto a = sm.active();
        CHECK(a.size() == 2);
        CHECK_NEAR(a[0].weight + a[1].weight, 1.0f, 1e-5f); // partition of unity
        CHECK(a[0].id == 0 && a[1].id == 1);
        CHECK(a[1].weight > 0.2f && a[1].weight < 0.8f); // genuinely blending
        // Finish the fade -> pure move.
        for (int i = 0; i < 20; ++i) {
            sm.update(1.0f / 60.0f);
        }
        CHECK(!sm.transitioning());
        a = sm.active();
        CHECK(a.size() == 1);
        CHECK(a[0].id == 1);
        CHECK_NEAR(a[0].weight, 1.0f, 1e-6f);
        moving = false;
    }

    // travel() forces a transition using the defined fade; an instant (fade 0) switch has no blend.
    {
        AnimStateMachine sm;
        sm.addState("a", 10);
        sm.addState("b", 20);
        sm.addTransition("a", "b", 0.0f); // instant
        sm.setStart("a");
        sm.travel("b");
        CHECK(!sm.transitioning()); // fade 0 -> switched immediately
        CHECK(sm.active()[0].id == 20);
    }

    // Only OUTGOING transitions from the current state fire (idle's condition ignored while in move).
    {
        auto sm = build();
        moving = true;
        for (int i = 0; i < 30; ++i) {
            sm.update(1.0f / 60.0f); // settle into move
        }
        CHECK(sm.currentName() == "move");
        // Now request jump via condition; move->jump exists, fades over 0.1s.
        jump = true;
        sm.update(1.0f / 60.0f);
        CHECK(sm.current() == 2);
        jump = false;
        moving = false;
    }

    // Composition with a blend space: state weight x leaf weight still sums to 1.
    {
        auto sm = build();
        moving = true;
        sm.update(1.0f / 60.0f);
        for (int i = 0; i < 5; ++i) {
            sm.update(1.0f / 60.0f);
        }
        anim::BlendSpace1D moveBlend; // the "move" state is itself walk<->run
        moveBlend.addPoint(0.0f, 100); // walk
        moveBlend.addPoint(1.0f, 101); // run
        float total = 0.0f;
        for (const auto& act : sm.active()) {
            if (act.id == 1) { // the move state -> expand through its blend space at speed 0.5
                for (const auto& lw : moveBlend.weights(0.5f)) {
                    total += act.weight * lw.weight;
                }
            } else {
                total += act.weight;
            }
        }
        CHECK_NEAR(total, 1.0f, 1e-5f);
        moving = false;
    }
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

void testBehaviorTreeExtras() {
    using namespace maz::game::bt;
    auto ok = [] { return Status::Success; };
    auto no = [] { return Status::Failure; };
    auto run = [] { return Status::Running; };

    // ---- Blackboard: typed shared memory ----
    {
        Blackboard bb;
        CHECK(!bb.has("hp"));
        bb.set<int>("hp", 42);
        bb.set<float>("range", 3.5f);
        bb.set<bool>("alert", true);
        CHECK(bb.has("hp"));
        CHECK(bb.get<int>("hp") == 42);
        CHECK_NEAR(bb.get<float>("range"), 3.5f, 1e-6f);
        CHECK(bb.get<bool>("alert") == true);
        // getOr: missing key or wrong type -> fallback (never throws).
        CHECK(bb.getOr<int>("missing", -1) == -1);
        CHECK(bb.getOr<int>("range", 7) == 7); // "range" holds a float, not an int
        // overwrite + erase + clear.
        bb.set<int>("hp", 10);
        CHECK(bb.get<int>("hp") == 10);
        bb.erase("hp");
        CHECK(!bb.has("hp"));
        bb.clear();
        CHECK(!bb.has("alert"));
    }

    // ---- Parallel ----
    {
        // RequireAll: succeeds only when all succeed; fails as soon as one fails; else Running.
        CHECK(parallel(ParallelPolicy::RequireAll, action(ok), action(ok))->tick() == Status::Success);
        CHECK(parallel(ParallelPolicy::RequireAll, action(ok), action(no))->tick() == Status::Failure);
        CHECK(parallel(ParallelPolicy::RequireAll, action(ok), action(run))->tick() == Status::Running);
        // RequireOne: succeeds as soon as one succeeds; fails only when all fail.
        CHECK(parallel(ParallelPolicy::RequireOne, action(no), action(ok))->tick() == Status::Success);
        CHECK(parallel(ParallelPolicy::RequireOne, action(no), action(no))->tick() == Status::Failure);
        CHECK(parallel(ParallelPolicy::RequireOne, action(no), action(run))->tick() == Status::Running);

        // Parallel ticks EVERY child each tick (unlike a Sequence/Selector short-circuit).
        int a = 0, b = 0, c = 0;
        auto p = parallel(ParallelPolicy::RequireAll, action([&] { ++a; return Status::Success; }),
                          action([&] { ++b; return Status::Failure; }),
                          action([&] { ++c; return Status::Running; }));
        p->tick();
        CHECK(a == 1 && b == 1 && c == 1);
    }

    // ---- Repeater ----
    {
        int ticks = 0;
        auto rep = repeater(3, action([&] { ++ticks; return Status::Success; }));
        CHECK(rep->tick() == Status::Running); // 1st success
        CHECK(rep->tick() == Status::Running); // 2nd success
        CHECK(rep->tick() == Status::Success); // 3rd success -> done
        CHECK(ticks == 3);
        // A child failure aborts the repeater immediately.
        auto repFail = repeater(5, action(no));
        CHECK(repFail->tick() == Status::Failure);
    }

    // ---- AlwaysSucceed / AlwaysFail ----
    {
        CHECK(alwaysSucceed(action(no))->tick() == Status::Success);
        CHECK(alwaysSucceed(action(run))->tick() == Status::Running); // running passes through
        CHECK(alwaysFail(action(ok))->tick() == Status::Failure);
        CHECK(alwaysFail(action(run))->tick() == Status::Running);
    }

    // ---- Tap: records the child's status (0 Success / 1 Failure / 2 Running) ----
    {
        int probe = -1;
        auto t = tap(&probe, action(no));
        CHECK(t->tick() == Status::Failure);
        CHECK(probe == static_cast<int>(Status::Failure));
    }

    // ---- Integration: a blackboard flag drives a reactive selector with a parallel patrol branch ----
    {
        Blackboard bb;
        bb.set<bool>("visible", false);
        int engageSt = -1, patrolSt = -1;
        auto build = [&] {
            engageSt = -1;
            patrolSt = -1;
            return BehaviorTree(selector(
                tap(&engageSt, sequence(condition([&] { return bb.getOr<bool>("visible", false); }),
                                        action(run))),
                tap(&patrolSt, parallel(ParallelPolicy::RequireAll, repeater(0, action(run)),
                                        action(run)))));
        };
        {
            auto tree = build();
            tree.tick();
            CHECK(engageSt == static_cast<int>(Status::Failure)); // not visible -> engage fails
            CHECK(patrolSt == static_cast<int>(Status::Running)); // fell through to patrol
        }
        {
            bb.set<bool>("visible", true);
            auto tree = build();
            tree.tick();
            CHECK(engageSt == static_cast<int>(Status::Running)); // visible -> engage runs
            CHECK(patrolSt == -1);                                // selector short-circuits: patrol untouched
        }
    }
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
    testNavMesh();
    testAutoTile();
    testSpatial3D();
    testWav();
    testParticleEmitter();
    testTileSet();
    testCollisionLayers();
    testArea2D();
    testAvoidance();
    testVisibility2D();
    testSoftShadow2D();
    testPhysics2D();
    testPhysics2DRotation();
    testPhysics2DJoints();
    testPhysics2DGroove();
    testPhysicsQuery2D();
    testManifold2();
    testNormalLight();
    testParallax();
    testAudioDsp();
    testAudioEffects();
    testADSR();
    testSpatial2D();
    testTwoBoneIK();
    testFabrik();
    testGoap();
    testFlowField();
    testBlendSpace();
    testBlendTree();
    testAnimStateMachine();
    testBehaviorTree();
    testBehaviorTreeExtras();
    testSteering();
    testStateMachine();
    testSpriteAnim();
    testSkeleton();
    testAnimClip();
    testAdditiveBlend();
    testAnimator();
    testEventBus();
    testSignal();
    testStringId();
    testJobs();
    testResourceCache();
    testSceneStack();
    testTween();
    testTweenPlayer();
    testTimeline();
    testTriggerTrack();
    testLayout();
    testUiContainer();
    testStyleBox();
    testTheme();
    testTree();
    testTextLayout();
    testTextInput();
    testUI();
    testSerialize();
    testJson();
    testCVars();
    testProfiler();
    testNoise();
    testRandom();
    testScheduler();
    testSequence();
    testCameraController();
    testTransformGraph();
    testPrefab();
    testPrefabText();
    testLocalization();
    testGrid3D();
    testShapes3D();
    testPolyline();
    testActionMap();
    testSceneSerializer();
    testEcs();
    testShake();
    testParticleAttractor();
    std::printf("%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
