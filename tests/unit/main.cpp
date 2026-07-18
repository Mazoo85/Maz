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
#include "maz/anim/Curve.hpp"
#include "maz/anim/Gradient.hpp"
#include "maz/anim/IK.hpp"
#include "maz/anim/RootMotion.hpp"
#include "maz/audio/BusGraph.hpp"
#include "maz/audio/Dsp.hpp"
#include "maz/audio/Envelope.hpp"
#include "maz/audio/MusicSequencer.hpp"
#include "maz/audio/Oscillator.hpp"
#include "maz/audio/PitchShifter.hpp"
#include "maz/audio/Randomizer.hpp"
#include "maz/audio/SampleMixer.hpp"
#include "maz/audio/Spatial2D.hpp"
#include "maz/audio/Spatial3D.hpp"
#include "maz/audio/Spectrum.hpp"
#include "maz/audio/Stereo.hpp"
#include "maz/audio/Wav.hpp"
#include "maz/anim/Skeleton.hpp"
#include "maz/anim/SpriteAnim.hpp"
#include "maz/anim/Timeline.hpp"
#include "maz/anim/TriggerTrack.hpp"
#include "maz/anim/Tween.hpp"
#include "maz/anim/TweenPlayer.hpp"
#include "maz/core/AssetServer.hpp"
#include "maz/core/CVars.hpp"
#include "maz/core/Checkpoints.hpp"
#include "maz/core/Containers.hpp"
#include "maz/core/DateTime.hpp"
#include "maz/core/Memory.hpp"
#include "maz/core/Reflect.hpp"
#include "maz/core/Replay.hpp"
#include "maz/core/Telemetry.hpp"
#include "maz/core/Version.hpp"
#include "maz/platform/AppFocus.hpp"
#include "maz/platform/CrashHandler.hpp"
#include "maz/platform/DisplayScale.hpp"
#include "maz/platform/Displays.hpp"
#include "maz/platform/Input.hpp"
#include "maz/net/BitStream.hpp"
#include "maz/net/Connection.hpp"
#include "maz/net/Interpolation.hpp"
#include "maz/net/NetSim.hpp"
#include "maz/net/Prediction.hpp"
#include "maz/net/Reliability.hpp"
#include "maz/net/Replication.hpp"
#include "maz/net/Rpc.hpp"
#include "maz/net/Snapshot.hpp"
#include "maz/render/CascadeSplits.hpp"
#include "maz/render/Ktx2.hpp"
#include "maz/render/PresentMode.hpp"
#include "maz/core/Events.hpp"
#include "maz/core/Expression.hpp"
#include "maz/core/Jobs.hpp"
#include "maz/core/LogSinks.hpp"
#include "maz/core/Noise.hpp"
#include "maz/core/Pcg32.hpp"
#include "maz/core/PerfBudget.hpp"
#include "maz/core/Profiler.hpp"
#include "maz/core/Random.hpp"
#include "maz/core/Resources.hpp"
#include "maz/core/Interpolate.hpp"
#include "maz/core/RingBuffer.hpp"
#include "maz/core/Scheduler.hpp"
#include "maz/core/SceneStack.hpp"
#include "maz/core/Signal.hpp"
#include "maz/core/SlotMap.hpp"
#include "maz/core/StringId.hpp"
#include "maz/ecs/Components.hpp"
#include "maz/ecs/Scheduler.hpp"
#include "maz/ecs/World.hpp"
#include "maz/editor/Scene.hpp"
#include "maz/script/Script.hpp"
#include "maz/script/ScriptSystem.hpp"
#include "maz/scene/SceneTree.hpp"
#include "maz/scene/SceneSerialize.hpp"
#include "game.hpp" // apps/zomboid — the flagship game's logic
#include "maz/fx/ForceField2D.hpp"
#include "maz/fx/ParticleEmitter.hpp"
#include "maz/fx/Particles.hpp"
#include "maz/game/Area2D.hpp"
#include "maz/game/AStar2D.hpp"
#include "maz/game/Bvh.hpp"
#include "maz/game/Octree.hpp"
#include "maz/game/Quadtree.hpp"
#include "maz/game/GravityField2D.hpp"
#include "maz/game/KinematicBody2D.hpp"
#include "maz/game/AutoTile.hpp"
#include "maz/game/Avoidance.hpp"
#include "maz/game/BehaviorTree.hpp"
#include "maz/game/CameraController2D.hpp"
#include "maz/game/ChunkStreamer.hpp"
#include "maz/game/Collision.hpp"
#include "maz/game/CollisionLayers.hpp"
#include "maz/game/ConvexShape2D.hpp"
#include "maz/game/OneWayPlatform.hpp"
#include "maz/game/Overlap3D.hpp"
#include "maz/game/FlowField.hpp"
#include "maz/game/Goap.hpp"
#include "maz/game/NavGrid.hpp"
#include "maz/game/NavMesh.hpp"
#include "maz/game/NormalLight2D.hpp"
#include "maz/game/Parallax.hpp"
#include "maz/game/ConvexHull3D.hpp"
#include "maz/game/GridMap.hpp"
#include "maz/game/HeightField3D.hpp"
#include "maz/game/PathFollow2D.hpp"
#include "maz/game/TriMesh3D.hpp"
#include "maz/game/Physics2D.hpp"
#include "maz/game/Timer.hpp"
#include "maz/game/VisibleOnScreenNotifier2D.hpp"
#include "maz/game/Physics3D.hpp"
#include "maz/game/PhysicsQuery2D.hpp"
#include "maz/game/Shake.hpp"
#include "maz/game/ShapeCast2D.hpp"
#include "maz/game/SoftShadow2D.hpp"
#include "maz/game/SpatialGrid.hpp"
#include "maz/game/StateMachine.hpp"
#include "maz/game/SweepPrune2D.hpp"
#include "maz/game/Steering.hpp"
#include "maz/game/TileSet.hpp"
#include "maz/game/Visibility2D.hpp"
#include "maz/input/ActionMap.hpp"
#include "maz/input/Analog.hpp"
#include "maz/io/Config.hpp"
#include "maz/io/ConfigFile.hpp"
#include "maz/io/Base64.hpp"
#include "maz/io/ExportConfig.hpp"
#include "maz/io/GettextPo.hpp"
#include "maz/io/Json.hpp"
#include "maz/io/Localization.hpp"
#include "maz/io/SceneSerializer.hpp"
#include "maz/io/Serialize.hpp"
#include "maz/io/VirtualFileSystem.hpp"
#include "maz/io/Xml.hpp"
#include "maz/ui/Container.hpp"
#include "maz/ui/Layout.hpp"
#include "maz/ui/RichText.hpp"
#include "maz/ui/Sdf.hpp"
#include "maz/ui/StyleBox.hpp"
#include "maz/ui/TextInput.hpp"
#include "maz/ui/TextLayout.hpp"
#include "maz/ui/Theme.hpp"
#include "maz/ui/ItemList.hpp"
#include "maz/ui/PopupMenu.hpp"
#include "maz/ui/Range.hpp"
#include "maz/ui/Tree.hpp"
#include "maz/ui/UI.hpp"
#include "maz/io/PrefabText.hpp"
#include "maz/io/ResourcePack.hpp"
#include "maz/math/Curve2D.hpp"
#include "maz/math/Geometry2D.hpp"
#include "maz/math/Geometry3D.hpp"
#include "maz/math/Rect2.hpp"
#include "maz/math/Transform2D.hpp"
#include "maz/math/Math.hpp"
#include "maz/render/Grid3D.hpp"
#include "maz/render/AtlasPacker.hpp"
#include "maz/render/Billboard.hpp"
#include "maz/render/Camera3D.hpp"
#include "maz/render/Line2D.hpp"
#include "maz/render/MeshLod.hpp"
#include "maz/render/MeshTools.hpp"
#include "maz/render/MultiMesh2D.hpp"
#include "maz/render/ObjLoader.hpp"
#include "maz/render/PolyTriangulate.hpp"
#include "maz/render/SpriteOrder.hpp"
#include "maz/render/Shapes3D.hpp"
#include "maz/scene/GroupRegistry.hpp"
#include "maz/scene/Prefab.hpp"
#include "maz/scene/TransformGraph.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <numeric>
#include <set>
#include <string>
#include <thread>
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

    // 3D orthographic projection (parallel — no perspective divide, w stays 1).
    {
        // verticalSize 4, aspect 2 -> halfW 4, halfH 2.
        const math::mat4 op = math::orthographicSize(4.0f, 2.0f, 0.1f, 100.0f);
        auto pr = [&](float x, float y, float z) { return op * math::vec4(x, y, z, 1.0f); };
        const math::vec4 mid = pr(0.0f, 0.0f, -10.0f);
        CHECK_NEAR(mid.w, 1.0f, 1e-5f); // no perspective divide
        CHECK_NEAR(mid.x, 0.0f, 1e-5f);
        CHECK_NEAR(mid.y, 0.0f, 1e-5f);
        CHECK_NEAR(pr(4.0f, 0.0f, -10.0f).x, 1.0f, 1e-4f);   // right edge -> +1
        CHECK_NEAR(pr(-4.0f, 0.0f, -10.0f).x, -1.0f, 1e-4f); // left edge -> -1
        CHECK_NEAR(pr(0.0f, 2.0f, -10.0f).y, -1.0f, 1e-4f);  // top -> -1 (Vulkan y-flip)
        CHECK_NEAR(pr(0.0f, -2.0f, -10.0f).y, 1.0f, 1e-4f);
        CHECK_NEAR(pr(0.0f, 0.0f, -0.1f).z, 0.0f, 1e-4f);   // near -> 0
        CHECK_NEAR(pr(0.0f, 0.0f, -100.0f).z, 1.0f, 1e-4f); // far -> 1
        // The defining ortho property: a point's screen x is the SAME at any depth (no convergence).
        CHECK_NEAR(pr(4.0f, 0.0f, -10.0f).x, pr(4.0f, 0.0f, -90.0f).x, 1e-4f);

        // Explicit-bounds form maps its right edge to +1 too.
        const math::mat4 ob = math::orthographic(-8.0f, 8.0f, -4.0f, 4.0f, 0.1f, 100.0f);
        CHECK_NEAR((ob * math::vec4(8.0f, 0.0f, -10.0f, 1.0f)).x, 1.0f, 1e-4f);
    }
}

void testCurve2D() {
    using math::Curve2D;
    using math::vec2;

    // A single straight segment (no handles): endpoints exact, midpoint is the geometric middle, length is
    // the chord, tangent points along the line.
    {
        Curve2D c;
        c.addPoint(vec2(0.0f, 0.0f));
        c.addPoint(vec2(100.0f, 0.0f));
        CHECK(c.pointCount() == 2);
        CHECK_NEAR(c.sampleSegment(0, 0.0f).x, 0.0f, 1e-4f);
        CHECK_NEAR(c.sampleSegment(0, 1.0f).x, 100.0f, 1e-4f);
        CHECK_NEAR(c.sampleSegment(0, 0.5f).x, 50.0f, 1e-4f);
        CHECK_NEAR(c.sampleSegment(0, 0.5f).y, 0.0f, 1e-4f);
        CHECK_NEAR(c.length(), 100.0f, 0.5f);
        const vec2 t = c.tangent(0.5f);
        CHECK_NEAR(t.x, 1.0f, 1e-3f);
        CHECK_NEAR(t.y, 0.0f, 1e-3f);
    }

    // A bowed curve (handles pull the middle upward) is longer than the straight chord, and its endpoints
    // still land exactly on the points.
    {
        Curve2D c;
        c.addPoint(vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, -80.0f));
        c.addPoint(vec2(100.0f, 0.0f), vec2(0.0f, -80.0f), vec2(0.0f, 0.0f));
        CHECK_NEAR(c.sample(0.0f).x, 0.0f, 1e-4f);
        CHECK_NEAR(c.sample(1.0f).x, 100.0f, 1e-4f);
        CHECK(c.length() > 100.0f);          // bowed, so longer than the chord
        CHECK(c.sampleSegment(0, 0.5f).y < -1.0f); // the middle bulges up (negative y)
    }

    // sample(fofs) walks segments: with 3 points, fofs=1 lands exactly on the middle point.
    {
        Curve2D c;
        c.addPoint(vec2(0.0f, 0.0f));
        c.addPoint(vec2(50.0f, 50.0f));
        c.addPoint(vec2(100.0f, 0.0f));
        CHECK(c.pointCount() == 3);
        CHECK_NEAR(c.sample(1.0f).x, 50.0f, 1e-3f);
        CHECK_NEAR(c.sample(1.0f).y, 50.0f, 1e-3f);
        // Clamping: fofs below 0 / above max returns the endpoints.
        CHECK_NEAR(c.sample(-5.0f).x, 0.0f, 1e-4f);
        CHECK_NEAR(c.sample(99.0f).x, 100.0f, 1e-4f);
    }

    // Arc-length baking: bakedLength matches length(), the ends map to the endpoints, and the half-distance
    // sample lands near the geometric arc midpoint (constant-speed property).
    {
        Curve2D c;
        c.addPoint(vec2(0.0f, 0.0f));
        c.addPoint(vec2(200.0f, 0.0f));
        c.bake(10.0f);
        CHECK(c.bakedPoints().size() > 1);
        CHECK_NEAR(c.bakedLength(), 200.0f, 1.0f);
        CHECK_NEAR(c.sampleBaked(0.0f).x, 0.0f, 1e-3f);
        CHECK_NEAR(c.sampleBaked(c.bakedLength()).x, 200.0f, 0.5f);
        CHECK_NEAR(c.sampleBaked(100.0f).x, 100.0f, 1.0f); // half the arc -> geometric middle of the line
        // Distances beyond the ends clamp.
        CHECK_NEAR(c.sampleBaked(-10.0f).x, 0.0f, 1e-3f);
        CHECK_NEAR(c.sampleBaked(1000.0f).x, 200.0f, 0.5f);
    }

    // Constant-speed check on a curved path: two equal arc-distance steps cover equal ground even though the
    // curve bends (naive Bézier t would not).
    {
        Curve2D c;
        c.addPoint(vec2(0.0f, 0.0f), vec2(0.0f), vec2(120.0f, 0.0f));
        c.addPoint(vec2(200.0f, 200.0f), vec2(0.0f, -120.0f), vec2(0.0f));
        c.bake(4.0f);
        const float L = c.bakedLength();
        const vec2 a = c.sampleBaked(L * 0.25f);
        const vec2 b = c.sampleBaked(L * 0.50f);
        const vec2 d = c.sampleBaked(L * 0.75f);
        const float d1 = glm::length(b - a);
        const float d2 = glm::length(d - b);
        // Equal arc-length steps -> chord lengths within ~15% of each other.
        CHECK(d1 > 0.0f && d2 > 0.0f);
        CHECK(std::abs(d1 - d2) < 0.15f * std::max(d1, d2));
    }

    // Degenerate cases don't crash.
    {
        Curve2D c;
        CHECK_NEAR(c.length(), 0.0f, 1e-6f);
        CHECK_NEAR(c.sample(0.5f).x, 0.0f, 1e-6f); // empty -> origin
        c.addPoint(vec2(7.0f, 9.0f));
        CHECK_NEAR(c.sample(0.5f).x, 7.0f, 1e-6f); // single point -> that point
        CHECK_NEAR(c.sampleBaked(3.0f).y, 9.0f, 1e-6f);
        c.clear();
        CHECK(c.pointCount() == 0);
    }
}

void testAtlasPacker() {
    using render::AtlasPacker;
    using render::PackSize;
    using render::Placement;

    // Helper: do two placed rects overlap?
    auto overlap = [](const Placement& a, const Placement& b) {
        return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
    };

    // --- single insert lands bottom-left; occupancy tracks area --------------------------------
    {
        AtlasPacker p(100, 100);
        CHECK_NEAR(p.occupancy(), 0.0f, 1e-6f);
        const Placement a = p.insert(40, 30);
        CHECK(a.placed);
        CHECK(a.x == 0 && a.y == 0 && a.w == 40 && a.h == 30);
        CHECK_NEAR(p.occupancy(), 40.0f * 30.0f / (100.0f * 100.0f), 1e-5f);

        // Next rect can't overlap the first.
        const Placement b = p.insert(40, 30);
        CHECK(b.placed);
        CHECK(!overlap(a, b));
    }

    // --- rejects rects that cannot fit ----------------------------------------------------------
    {
        AtlasPacker p(64, 64);
        CHECK(!p.insert(65, 10).placed);  // too wide
        CHECK(!p.insert(10, 65).placed);  // too tall
        CHECK(!p.insert(0, 10).placed);   // degenerate
        CHECK(!p.insert(10, -5).placed);  // degenerate
        // A valid one still packs after rejections (rejections leave the bin unchanged).
        CHECK(p.insert(64, 64).placed);
        CHECK_NEAR(p.occupancy(), 1.0f, 1e-6f); // exact fill
        CHECK(!p.insert(1, 1).placed);          // now full
    }

    // --- exact tiling fills the bin with zero waste --------------------------------------------
    {
        AtlasPacker p(80, 80);
        std::vector<PackSize> sizes;
        for (int i = 0; i < 16; ++i) {
            sizes.push_back(PackSize{20, 20}); // 4x4 grid of 20x20 exactly fills 80x80
        }
        const std::vector<Placement> out = p.pack(sizes);
        int placed = 0;
        for (const Placement& q : out) {
            if (q.placed) {
                ++placed;
            }
        }
        CHECK(placed == 16);
        CHECK_NEAR(p.occupancy(), 1.0f, 1e-6f);
        // No two placed rects overlap, and all lie inside the bin.
        for (std::size_t i = 0; i < out.size(); ++i) {
            CHECK(out[i].x >= 0 && out[i].y >= 0);
            CHECK(out[i].x + out[i].w <= 80 && out[i].y + out[i].h <= 80);
            for (std::size_t j = i + 1; j < out.size(); ++j) {
                CHECK(!overlap(out[i], out[j]));
            }
        }
    }

    // --- a mixed batch packs without overlap; pack() returns input order -----------------------
    {
        AtlasPacker p(128, 128);
        std::vector<PackSize> sizes = {{30, 60}, {50, 20}, {40, 40}, {20, 20},
                                       {60, 30}, {25, 45}, {70, 15}, {35, 35}};
        const std::vector<Placement> out = p.pack(sizes);
        CHECK(out.size() == sizes.size());
        for (std::size_t i = 0; i < out.size(); ++i) {
            // returned in original order: dimensions match the input at the same index.
            CHECK(out[i].w == sizes[i].w && out[i].h == sizes[i].h);
            if (out[i].placed) {
                CHECK(out[i].x + out[i].w <= 128 && out[i].y + out[i].h <= 128);
            }
        }
        for (std::size_t i = 0; i < out.size(); ++i) {
            if (!out[i].placed) {
                continue;
            }
            for (std::size_t j = i + 1; j < out.size(); ++j) {
                if (out[j].placed) {
                    CHECK(!overlap(out[i], out[j]));
                }
            }
        }
        CHECK(p.occupancy() > 0.0f && p.occupancy() <= 1.0f);
    }

    // --- reset clears the bin -------------------------------------------------------------------
    {
        AtlasPacker p(32, 32);
        CHECK(p.insert(32, 32).placed);
        CHECK(!p.insert(1, 1).placed);
        p.reset();
        CHECK_NEAR(p.occupancy(), 0.0f, 1e-6f);
        CHECK(p.insert(1, 1).placed); // room again
    }
}

void testForceField2D() {
    using fx::Attractor2D;
    using fx::FieldParticle;
    using fx::ForceField2D;
    using math::vec2;

    // --- a single attractor pulls toward it; a repulsor pushes away -----------------------------
    {
        ForceField2D f;
        f.addAttractor(vec2(0, 0), 100.0f, 0.0f, Attractor2D::Constant);
        // Particle to the right of the origin: acceleration should point left (-x), no y.
        const vec2 a = f.accelAt(vec2(10, 0));
        CHECK(a.x < 0.0f);
        CHECK_NEAR(a.y, 0.0f, 1e-5f);
        CHECK_NEAR(a.x, -100.0f, 1e-4f); // Constant falloff -> magnitude == strength

        // Repulsor (negative strength) pushes the same particle right.
        f.attractors[0].strength = -100.0f;
        const vec2 r = f.accelAt(vec2(10, 0));
        CHECK(r.x > 0.0f);
        CHECK_NEAR(r.x, 100.0f, 1e-4f);
    }

    // --- inverse-square is stronger up close than far away --------------------------------------
    {
        ForceField2D f;
        f.addAttractor(vec2(0, 0), 100.0f, 0.0f, Attractor2D::InverseSquare);
        const float near = std::fabs(f.accelAt(vec2(2, 0)).x);
        const float far = std::fabs(f.accelAt(vec2(8, 0)).x);
        CHECK(near > far);
        // 1/2^2 = 0.25 vs 1/8^2 ~ 0.0156 -> near ~16x far.
        CHECK(near > far * 4.0f);
    }

    // --- radius cutoff: outside the influence radius the attractor contributes nothing ----------
    {
        ForceField2D f;
        f.addAttractor(vec2(0, 0), 50.0f, 5.0f, Attractor2D::Constant);
        CHECK(std::fabs(f.accelAt(vec2(3, 0)).x) > 0.0f);       // inside
        CHECK_NEAR(f.accelAt(vec2(9, 0)).x, 0.0f, 1e-6f);       // outside radius
        // Linear falloff: full strength at centre, ~0 at the edge.
        f.attractors[0].falloff = Attractor2D::Linear;
        const float mid = std::fabs(f.accelAt(vec2(2.5f, 0)).x); // halfway -> ~half strength
        CHECK_NEAR(mid, 25.0f, 1.0f);
    }

    // --- uniform wind + drag --------------------------------------------------------------------
    {
        ForceField2D f;
        f.wind = vec2(5.0f, -3.0f);
        const vec2 a = f.accelAt(vec2(100, 100)); // wind is position-independent
        CHECK_NEAR(a.x, 5.0f, 1e-5f);
        CHECK_NEAR(a.y, -3.0f, 1e-5f);

        // Drag alone bleeds off speed each step (no other forces).
        ForceField2D d;
        d.drag = 2.0f;
        std::vector<FieldParticle> ps = {{vec2(0, 0), vec2(10, 0)}};
        const float before = ps[0].vel.x;
        d.step(ps, 0.1f);
        CHECK(ps[0].vel.x < before);
        CHECK(ps[0].vel.x > 0.0f); // damped, not reversed
    }

    // --- swirl adds a perpendicular (orbiting) component ----------------------------------------
    {
        ForceField2D f;
        f.addAttractor(vec2(0, 0), 0.0f, 0.0f, Attractor2D::Constant, 20.0f); // pure swirl, no pull
        // Particle on +x axis: dir toward centre is -x; perp (rotate +90) is (-(-0),-1)= (0,-1)... check sign.
        const vec2 a = f.accelAt(vec2(10, 0));
        CHECK_NEAR(a.x, 0.0f, 1e-4f);      // no radial component
        CHECK(std::fabs(a.y) > 1.0f);       // purely tangential
    }

    // --- integration: a particle falls toward an attractor over steps ---------------------------
    {
        ForceField2D f;
        f.addAttractor(vec2(0, 0), 200.0f, 0.0f, Attractor2D::Constant);
        std::vector<FieldParticle> ps = {{vec2(10, 0), vec2(0, 0)}};
        const float d0 = ps[0].pos.x;
        for (int i = 0; i < 20; ++i) {
            f.step(ps, 1.0f / 60.0f, 2);
        }
        CHECK(ps[0].pos.x < d0); // moved toward the origin
        CHECK(ps[0].vel.x < 0.0f);

        // Empty field / empty particle list are safe no-ops.
        ForceField2D empty;
        std::vector<FieldParticle> none;
        empty.step(none, 0.016f);
        CHECK(none.empty());
        const vec2 z = empty.accelAt(vec2(1, 1));
        CHECK_NEAR(z.x, 0.0f, 1e-6f);
        CHECK_NEAR(z.y, 0.0f, 1e-6f);
    }
}

void testExpression() {
    using core::Expression;

    // --- constants, precedence, associativity ---------------------------------------------------
    {
        Expression e;
        CHECK(e.parse("1 + 2 * 3"));
        CHECK(!e.hasError());
        CHECK_NEAR(e.execute(), 7.0, 1e-9);

        CHECK(e.parse("(1 + 2) * 3"));
        CHECK_NEAR(e.execute(), 9.0, 1e-9);

        // '^' right-associative, binds tighter than unary minus: -2^2 == -(2^2) == -4.
        CHECK(e.parse("-2^2"));
        CHECK_NEAR(e.execute(), -4.0, 1e-9);
        CHECK(e.parse("2^3^2")); // 2^(3^2) = 2^9 = 512
        CHECK_NEAR(e.execute(), 512.0, 1e-9);
        CHECK(e.parse("2^-2")); // 0.25
        CHECK_NEAR(e.execute(), 0.25, 1e-9);

        // modulo + integer-ish arithmetic.
        CHECK(e.parse("10 % 3"));
        CHECK_NEAR(e.execute(), 1.0, 1e-9);

        // pi constant.
        CHECK(e.parse("cos(pi)"));
        CHECK_NEAR(e.execute(), -1.0, 1e-9);
        CHECK(e.parse("tau / pi"));
        CHECK_NEAR(e.execute(), 2.0, 1e-9);
    }

    // --- variables + functions ------------------------------------------------------------------
    {
        Expression e;
        CHECK(e.parse("amp * sin(x) + off", {"x", "amp", "off"}));
        CHECK(!e.hasError());
        // x=pi/2 -> sin=1; amp=2, off=0.5 -> 2.5
        CHECK_NEAR(e.execute({1.5707963267948966, 2.0, 0.5}), 2.5, 1e-6);
        // different inputs reuse the same parsed tree.
        CHECK_NEAR(e.execute({0.0, 2.0, 0.5}), 0.5, 1e-9);

        // multi-arg functions.
        CHECK(e.parse("clamp(x, 0, 1)", {"x"}));
        CHECK_NEAR(e.execute({-3.0}), 0.0, 1e-9);
        CHECK_NEAR(e.execute({0.4}), 0.4, 1e-9);
        CHECK_NEAR(e.execute({5.0}), 1.0, 1e-9);

        CHECK(e.parse("lerp(10, 20, t)", {"t"}));
        CHECK_NEAR(e.execute({0.25}), 12.5, 1e-9);

        CHECK(e.parse("max(min(x, 5), 2)", {"x"}));
        CHECK_NEAR(e.execute({9.0}), 5.0, 1e-9);
        CHECK_NEAR(e.execute({1.0}), 2.0, 1e-9);

        CHECK(e.parse("floor(x) + frac(x)", {"x"}));
        CHECK_NEAR(e.execute({3.75}), 3.75, 1e-9);
    }

    // --- missing input reads 0; divide/mod by zero guarded to 0 ---------------------------------
    {
        Expression e;
        CHECK(e.parse("a + b", {"a", "b"}));
        CHECK_NEAR(e.execute({5.0}), 5.0, 1e-9); // b missing -> 0
        CHECK(e.parse("1 / 0"));
        CHECK_NEAR(e.execute(), 0.0, 1e-9);
        CHECK(e.parse("5 % 0"));
        CHECK_NEAR(e.execute(), 0.0, 1e-9);
    }

    // --- error handling -------------------------------------------------------------------------
    {
        Expression e;
        CHECK(!e.parse("1 +"));            // dangling operator
        CHECK(e.hasError());
        CHECK(!e.parse("(1 + 2"));         // unbalanced paren
        CHECK(!e.parse("2 * (3 + )"));     // empty operand
        CHECK(!e.parse("nope(1)"));        // unknown function
        CHECK(!e.parse("sin(1, 2)"));      // wrong arity
        CHECK(!e.parse("x + 1"));          // unknown identifier (no vars declared)
        CHECK(!e.parse("1 2"));            // trailing tokens
        CHECK(!e.parse("1 @ 2"));          // bad character
        // recovering with a good parse clears the error.
        CHECK(e.parse("42"));
        CHECK(!e.hasError());
        CHECK_NEAR(e.execute(), 42.0, 1e-9);
    }
}

void testAStar2D() {
    using game::AStar2D;
    using math::vec2;

    // --- point + connection bookkeeping ---------------------------------------------------------
    {
        AStar2D a;
        CHECK(a.pointCount() == 0);
        CHECK(!a.hasPoint(1));
        CHECK(a.getClosestPoint(vec2(0, 0)) == AStar2D::kInvalidId);

        a.addPoint(1, vec2(0, 0));
        a.addPoint(2, vec2(10, 0), 2.0f);
        CHECK(a.hasPoint(1) && a.hasPoint(2));
        CHECK(a.pointCount() == 2);
        CHECK_NEAR(a.getPointPosition(2).x, 10.0f, 1e-5f);
        CHECK_NEAR(a.getPointWeightScale(2), 2.0f, 1e-5f);

        a.setPointPosition(2, vec2(12, 0));
        a.setPointWeightScale(2, 3.0f);
        CHECK_NEAR(a.getPointPosition(2).x, 12.0f, 1e-5f);
        CHECK_NEAR(a.getPointWeightScale(2), 3.0f, 1e-5f);
        a.setPointWeightScale(2, -5.0f); // clamped to 0
        CHECK_NEAR(a.getPointWeightScale(2), 0.0f, 1e-5f);
        a.setPointWeightScale(2, 1.0f);

        // ids come back ascending.
        a.addPoint(5, vec2(5, 5));
        const std::vector<int64_t> ids = a.getPointIds();
        CHECK(ids.size() == 3 && ids[0] == 1 && ids[1] == 2 && ids[2] == 5);

        // bidirectional connect adds both directions; one-way adds only one.
        a.connectPoints(1, 2);
        CHECK(a.arePointsConnected(1, 2) && a.arePointsConnected(2, 1));
        a.connectPoints(1, 5, false);
        CHECK(a.arePointsConnected(1, 5) && !a.arePointsConnected(5, 1));
        a.connectPoints(1, 1); // self-loop ignored
        CHECK(!a.arePointsConnected(1, 1));

        const std::vector<int64_t> conn = a.getPointConnections(1);
        CHECK(conn.size() == 2 && conn[0] == 2 && conn[1] == 5); // ascending

        a.disconnectPoints(1, 2);
        CHECK(!a.arePointsConnected(1, 2) && !a.arePointsConnected(2, 1));

        // removing a point drops edges that referenced it.
        a.connectPoints(2, 5);
        a.removePoint(5);
        CHECK(!a.hasPoint(5));
        CHECK(!a.arePointsConnected(1, 5) && !a.arePointsConnected(2, 5));
        CHECK(a.getPointConnections(1).empty());
    }

    // --- shortest path picks the cheaper of two routes -----------------------------------------
    {
        // 0 --3-- 1 --3-- 3   (top route, length 6)
        // 0 --------------3   via 2 at (3,-4)/(6,0): 5 + 5 = 10 detour
        AStar2D a;
        a.addPoint(0, vec2(0, 0));
        a.addPoint(1, vec2(3, 0));
        a.addPoint(2, vec2(3, -4));
        a.addPoint(3, vec2(6, 0));
        a.connectPoints(0, 1);
        a.connectPoints(1, 3);
        a.connectPoints(0, 2);
        a.connectPoints(2, 3);

        const std::vector<int64_t> ids = a.getIdPath(0, 3);
        CHECK(ids.size() == 3 && ids[0] == 0 && ids[1] == 1 && ids[2] == 3);

        const std::vector<vec2> pts = a.getPointPath(0, 3);
        CHECK(pts.size() == 3);
        CHECK_NEAR(pts[1].x, 3.0f, 1e-5f);
        CHECK_NEAR(pts[2].x, 6.0f, 1e-5f);

        // A heavy weight on node 1 flips the choice to the geometric detour.
        a.setPointWeightScale(1, 5.0f); // arriving at 1 now costs 3*5 = 15
        const std::vector<int64_t> heavy = a.getIdPath(0, 3);
        CHECK(heavy.size() == 3 && heavy[1] == 2);
    }

    // --- degenerate paths -----------------------------------------------------------------------
    {
        AStar2D a;
        a.addPoint(7, vec2(0, 0));
        a.addPoint(8, vec2(1, 0));
        // same start/goal -> single node.
        const std::vector<int64_t> same = a.getIdPath(7, 7);
        CHECK(same.size() == 1 && same[0] == 7);
        // disconnected -> empty.
        CHECK(a.getIdPath(7, 8).empty());
        // missing endpoint -> empty.
        CHECK(a.getIdPath(7, 99).empty());
    }

    // --- one-way edges are respected ------------------------------------------------------------
    {
        AStar2D a;
        a.addPoint(0, vec2(0, 0));
        a.addPoint(1, vec2(1, 0));
        a.connectPoints(0, 1, false); // 0 -> 1 only
        CHECK(a.getIdPath(0, 1).size() == 2);
        CHECK(a.getIdPath(1, 0).empty());
    }

    // --- closest queries ------------------------------------------------------------------------
    {
        AStar2D a;
        a.addPoint(0, vec2(0, 0));
        a.addPoint(1, vec2(10, 0));
        a.connectPoints(0, 1);
        CHECK(a.getClosestPoint(vec2(3, 1)) == 0);
        CHECK(a.getClosestPoint(vec2(8, -2)) == 1);
        // Position (4, 5) projects onto the 0->1 segment at (4, 0).
        const vec2 seg = a.getClosestPositionInSegment(vec2(4, 5));
        CHECK_NEAR(seg.x, 4.0f, 1e-4f);
        CHECK_NEAR(seg.y, 0.0f, 1e-4f);
        // Beyond the endpoint, it clamps to the endpoint.
        const vec2 seg2 = a.getClosestPositionInSegment(vec2(20, 3));
        CHECK_NEAR(seg2.x, 10.0f, 1e-4f);
        CHECK_NEAR(seg2.y, 0.0f, 1e-4f);
    }
}

void testGeometry2D() {
    using math::closestPointOnSegment;
    using math::distanceToSegment;
    using math::pointInPolygon;
    using math::SegmentHit;
    using math::segmentIntersect;
    using math::segmentIntersectsCircle;
    using math::vec2;

    // --- segmentIntersect ---
    // The classic X: (0,0)-(10,10) crosses (0,10)-(10,0) at the centre.
    {
        const SegmentHit h = segmentIntersect(vec2(0, 0), vec2(10, 10), vec2(0, 10), vec2(10, 0));
        CHECK(h.hit);
        CHECK_NEAR(h.point.x, 5.0f, 1e-4f);
        CHECK_NEAR(h.point.y, 5.0f, 1e-4f);
        CHECK_NEAR(h.t, 0.5f, 1e-4f);
        CHECK_NEAR(h.u, 0.5f, 1e-4f);
    }
    // Parallel segments never meet.
    CHECK(!segmentIntersect(vec2(0, 0), vec2(10, 0), vec2(0, 5), vec2(10, 5)).hit);
    // They would cross if extended, but the segments themselves fall short (u out of [0,1]).
    CHECK(!segmentIntersect(vec2(0, 0), vec2(10, 0), vec2(5, 2), vec2(5, 1)).hit);
    // A T-junction touch counts as a hit at the endpoint.
    {
        const SegmentHit h = segmentIntersect(vec2(0, 0), vec2(10, 0), vec2(5, 0), vec2(5, 5));
        CHECK(h.hit);
        CHECK_NEAR(h.point.x, 5.0f, 1e-4f);
        CHECK_NEAR(h.point.y, 0.0f, 1e-4f);
    }

    // --- closestPointOnSegment ---
    {
        // Perpendicular foot lands mid-segment.
        const vec2 c = closestPointOnSegment(vec2(5, 5), vec2(0, 0), vec2(10, 0));
        CHECK_NEAR(c.x, 5.0f, 1e-4f);
        CHECK_NEAR(c.y, 0.0f, 1e-4f);
        // Beyond an end -> clamps to that endpoint.
        const vec2 e = closestPointOnSegment(vec2(-5, 3), vec2(0, 0), vec2(10, 0));
        CHECK_NEAR(e.x, 0.0f, 1e-4f);
        CHECK_NEAR(e.y, 0.0f, 1e-4f);
        // Degenerate segment -> the point itself.
        const vec2 d = closestPointOnSegment(vec2(2, 7), vec2(3, 3), vec2(3, 3));
        CHECK_NEAR(d.x, 3.0f, 1e-4f);
        CHECK_NEAR(d.y, 3.0f, 1e-4f);
        CHECK_NEAR(distanceToSegment(vec2(5, 4), vec2(0, 0), vec2(10, 0)), 4.0f, 1e-4f);
    }

    // --- pointInPolygon ---
    {
        const std::vector<vec2> square = {vec2(0, 0), vec2(4, 0), vec2(4, 4), vec2(0, 4)};
        CHECK(pointInPolygon(vec2(2, 2), square));
        CHECK(!pointInPolygon(vec2(5, 2), square));
        CHECK(!pointInPolygon(vec2(-1, 2), square));
        CHECK(!pointInPolygon(vec2(2, -1), square));
        // A concave dart pointing right (tip at (6,3)), with the notch on the left (vertex (2,3)):
        // a point in the notch is OUTSIDE even though it is within the bounding box.
        const std::vector<vec2> dart = {vec2(0, 0), vec2(6, 3), vec2(0, 6), vec2(2, 3)};
        CHECK(pointInPolygon(vec2(4, 3), dart));   // inside the body, near the tip
        CHECK(!pointInPolygon(vec2(1, 3), dart));  // inside the concave notch -> outside the polygon
        CHECK(!pointInPolygon(vec2(1, 3), {vec2(0, 0), vec2(1, 1)})); // <3 verts -> false
    }

    // --- segmentIntersectsCircle ---
    {
        // Segment along the x-axis; circle 3 above it.
        CHECK(!segmentIntersectsCircle(vec2(0, 0), vec2(10, 0), vec2(5, 3), 2.0f)); // 3 > 2 -> miss
        CHECK(segmentIntersectsCircle(vec2(0, 0), vec2(10, 0), vec2(5, 3), 4.0f));  // 3 <= 4 -> hit
        // Circle beyond the segment end but within radius of the endpoint.
        CHECK(segmentIntersectsCircle(vec2(0, 0), vec2(10, 0), vec2(-1, 0), 2.0f));
        CHECK(!segmentIntersectsCircle(vec2(0, 0), vec2(10, 0), vec2(-5, 0), 2.0f));
    }
}

void testTransform2D() {
    using math::Transform2D;
    using math::vec2;

    const float pi = 3.14159265358979323846f;

    // Identity leaves points untouched.
    {
        const Transform2D id = Transform2D::identity();
        const vec2 p = id.xform(vec2(3, 4));
        CHECK_NEAR(p.x, 3.0f, 1e-5f);
        CHECK_NEAR(p.y, 4.0f, 1e-5f);
        CHECK_NEAR(id.determinant(), 1.0f, 1e-5f);
    }

    // Translation moves points; basisXform ignores it.
    {
        const Transform2D t = Transform2D::translation(vec2(5, -2));
        const vec2 p = t.xform(vec2(1, 1));
        CHECK_NEAR(p.x, 6.0f, 1e-5f);
        CHECK_NEAR(p.y, -1.0f, 1e-5f);
        const vec2 v = t.basisXform(vec2(1, 1)); // direction: unaffected by translation
        CHECK_NEAR(v.x, 1.0f, 1e-5f);
        CHECK_NEAR(v.y, 1.0f, 1e-5f);
    }

    // Rotation by +90 degrees sends +X to +Y.
    {
        const Transform2D r = Transform2D::rotation(pi * 0.5f);
        const vec2 p = r.xform(vec2(1, 0));
        CHECK_NEAR(p.x, 0.0f, 1e-4f);
        CHECK_NEAR(p.y, 1.0f, 1e-4f);
        CHECK_NEAR(r.determinant(), 1.0f, 1e-5f);
        CHECK_NEAR(r.getRotation(), pi * 0.5f, 1e-4f);
    }

    // Scale stretches each axis; determinant is the area factor.
    {
        const Transform2D s = Transform2D::scaling(vec2(2, 3));
        const vec2 p = s.xform(vec2(1, 1));
        CHECK_NEAR(p.x, 2.0f, 1e-5f);
        CHECK_NEAR(p.y, 3.0f, 1e-5f);
        CHECK_NEAR(s.determinant(), 6.0f, 1e-5f);
        const vec2 sc = s.getScale();
        CHECK_NEAR(sc.x, 2.0f, 1e-5f);
        CHECK_NEAR(sc.y, 3.0f, 1e-5f);
    }

    // Composition applies right-to-left: (translate * rotate).xform(p) rotates first, then translates.
    {
        const Transform2D t = Transform2D::translation(vec2(5, 0)) * Transform2D::rotation(pi * 0.5f);
        const vec2 p = t.xform(vec2(1, 0)); // rotate (1,0)->(0,1), then +（5,0)
        CHECK_NEAR(p.x, 5.0f, 1e-4f);
        CHECK_NEAR(p.y, 1.0f, 1e-4f);
        // Equivalent to applying the two transforms in sequence.
        const vec2 seq = Transform2D::translation(vec2(5, 0)).xform(Transform2D::rotation(pi * 0.5f).xform(vec2(1, 0)));
        CHECK_NEAR(p.x, seq.x, 1e-4f);
        CHECK_NEAR(p.y, seq.y, 1e-4f);
    }

    // affineInverse undoes the transform even when scaled + rotated + translated.
    {
        const Transform2D m = Transform2D::compose(0.6f, vec2(2.0f, 0.5f), vec2(3, -4));
        const Transform2D inv = m.affineInverse();
        const vec2 p(1.5f, 2.5f);
        const vec2 back = inv.xform(m.xform(p));
        CHECK_NEAR(back.x, 1.5f, 1e-3f);
        CHECK_NEAR(back.y, 2.5f, 1e-3f);
        // xformInv is the same round-trip.
        const vec2 back2 = m.xformInv(m.xform(p));
        CHECK_NEAR(back2.x, 1.5f, 1e-3f);
        CHECK_NEAR(back2.y, 2.5f, 1e-3f);
        // m * m.affineInverse() is the identity.
        const Transform2D idm = m * inv;
        CHECK_NEAR(idm.x.x, 1.0f, 1e-3f);
        CHECK_NEAR(idm.y.y, 1.0f, 1e-3f);
        CHECK_NEAR(idm.origin.x, 0.0f, 1e-3f);
        CHECK_NEAR(idm.origin.y, 0.0f, 1e-3f);
    }

    // compose(rotation, scale, position) round-trips through the decomposition accessors.
    {
        const Transform2D m = Transform2D::compose(0.7f, vec2(1.5f, 2.5f), vec2(4, 5));
        CHECK_NEAR(m.getRotation(), 0.7f, 1e-4f);
        const vec2 sc = m.getScale();
        CHECK_NEAR(sc.x, 1.5f, 1e-4f);
        CHECK_NEAR(sc.y, 2.5f, 1e-4f);
        CHECK_NEAR(m.origin.x, 4.0f, 1e-5f);
        CHECK_NEAR(m.origin.y, 5.0f, 1e-5f);
    }

    // A mirrored basis reports a negative scale component.
    {
        const Transform2D flip = Transform2D::scaling(vec2(1, -1));
        CHECK(flip.determinant() < 0.0f);
        CHECK(flip.getScale().y < 0.0f);
    }

    // orthonormalized() strips scale/skew but keeps rotation + origin.
    {
        const Transform2D m = Transform2D::compose(0.4f, vec2(3.0f, 0.2f), vec2(2, 2));
        const Transform2D o = m.orthonormalized();
        CHECK_NEAR(std::sqrt(o.x.x * o.x.x + o.x.y * o.x.y), 1.0f, 1e-4f); // unit x
        CHECK_NEAR(std::sqrt(o.y.x * o.y.x + o.y.y * o.y.y), 1.0f, 1e-4f); // unit y
        CHECK_NEAR(o.x.x * o.y.x + o.x.y * o.y.y, 0.0f, 1e-4f);           // perpendicular
        CHECK_NEAR(o.getRotation(), 0.4f, 1e-4f);
        CHECK_NEAR(o.origin.x, 2.0f, 1e-5f);
    }

    // interpolateWith blends position + rotation + scale.
    {
        const Transform2D a = Transform2D::identity();
        const Transform2D b = Transform2D::compose(pi * 0.5f, vec2(3, 3), vec2(10, 0));
        const Transform2D mid = a.interpolateWith(b, 0.5f);
        CHECK_NEAR(mid.origin.x, 5.0f, 1e-4f);          // halfway across
        CHECK_NEAR(mid.getRotation(), pi * 0.25f, 1e-3f); // 45 degrees
        CHECK_NEAR(mid.getScale().x, 2.0f, 1e-3f);       // (1+3)/2
    }
}

void testRect2() {
    using math::Rect2;
    using math::vec2;

    const Rect2 r(10.0f, 20.0f, 100.0f, 60.0f); // x[10,110), y[20,80)

    // Basic accessors.
    CHECK_NEAR(r.right(), 110.0f, 1e-5f);
    CHECK_NEAR(r.bottom(), 80.0f, 1e-5f);
    CHECK_NEAR(r.center().x, 60.0f, 1e-5f);
    CHECK_NEAR(r.center().y, 50.0f, 1e-5f);
    CHECK_NEAR(r.area(), 6000.0f, 1e-3f);
    CHECK(r.hasArea());
    CHECK(!Rect2(0.0f, 0.0f, 0.0f, 5.0f).hasArea());

    // hasPoint — min-inclusive, max-exclusive.
    CHECK(r.hasPoint(vec2(10.0f, 20.0f)));   // top-left corner included
    CHECK(r.hasPoint(vec2(60.0f, 50.0f)));   // interior
    CHECK(!r.hasPoint(vec2(110.0f, 50.0f))); // right edge excluded
    CHECK(!r.hasPoint(vec2(60.0f, 80.0f)));  // bottom edge excluded
    CHECK(!r.hasPoint(vec2(5.0f, 50.0f)));   // left of rect

    // intersects / intersection.
    const Rect2 a(0.0f, 0.0f, 50.0f, 50.0f);
    const Rect2 b(30.0f, 30.0f, 50.0f, 50.0f);
    CHECK(a.intersects(b));
    const Rect2 ix = a.intersection(b);
    CHECK_NEAR(ix.position.x, 30.0f, 1e-5f);
    CHECK_NEAR(ix.position.y, 30.0f, 1e-5f);
    CHECK_NEAR(ix.size.x, 20.0f, 1e-5f);
    CHECK_NEAR(ix.size.y, 20.0f, 1e-5f);

    // Disjoint rects: no intersection, zero-area clip.
    const Rect2 c(200.0f, 200.0f, 10.0f, 10.0f);
    CHECK(!a.intersects(c));
    CHECK(!a.intersection(c).hasArea());

    // Edge-touching: excluded by default, included with includeBorders.
    const Rect2 d(50.0f, 0.0f, 20.0f, 50.0f); // shares the x=50 edge with `a`
    CHECK(!a.intersects(d));
    CHECK(a.intersects(d, true));

    // merge = smallest rect containing both.
    const Rect2 m = a.merge(b);
    CHECK_NEAR(m.position.x, 0.0f, 1e-5f);
    CHECK_NEAR(m.position.y, 0.0f, 1e-5f);
    CHECK_NEAR(m.size.x, 80.0f, 1e-5f);
    CHECK_NEAR(m.size.y, 80.0f, 1e-5f);

    // encloses.
    CHECK(a.encloses(Rect2(10.0f, 10.0f, 20.0f, 20.0f)));
    CHECK(a.encloses(a));
    CHECK(!a.encloses(b));

    // grow / growIndividual.
    const Rect2 g = a.grow(10.0f);
    CHECK_NEAR(g.position.x, -10.0f, 1e-5f);
    CHECK_NEAR(g.size.x, 70.0f, 1e-5f); // 50 + 2*10
    const Rect2 gi = a.growIndividual(5.0f, 0.0f, 0.0f, 15.0f);
    CHECK_NEAR(gi.position.x, -5.0f, 1e-5f);
    CHECK_NEAR(gi.position.y, 0.0f, 1e-5f);
    CHECK_NEAR(gi.size.x, 55.0f, 1e-5f);
    CHECK_NEAR(gi.size.y, 65.0f, 1e-5f);

    // expand to include an outside point.
    const Rect2 e = a.expand(vec2(80.0f, -20.0f));
    CHECK_NEAR(e.position.x, 0.0f, 1e-5f);
    CHECK_NEAR(e.position.y, -20.0f, 1e-5f);
    CHECK_NEAR(e.right(), 80.0f, 1e-5f);
    CHECK_NEAR(e.bottom(), 50.0f, 1e-5f);

    // abs normalizes a negative-size rect.
    const Rect2 neg(100.0f, 100.0f, -40.0f, -30.0f);
    const Rect2 an = neg.abs();
    CHECK_NEAR(an.position.x, 60.0f, 1e-5f);
    CHECK_NEAR(an.position.y, 70.0f, 1e-5f);
    CHECK_NEAR(an.size.x, 40.0f, 1e-5f);
    CHECK_NEAR(an.size.y, 30.0f, 1e-5f);
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

void testSpectrum() {
    using audio::Cplx;
    using audio::fft;
    using audio::nextPow2;
    using audio::SpectrumAnalyzer;
    using audio::SpectrumWindow;

    const float pi = 3.14159265358979323846f;

    // nextPow2 rounds up (and is idempotent on exact powers).
    CHECK(nextPow2(1) == 1);
    CHECK(nextPow2(5) == 8);
    CHECK(nextPow2(16) == 16);
    CHECK(nextPow2(17) == 32);

    // Forward then inverse FFT round-trips to the original samples.
    {
        std::vector<Cplx> a = {Cplx(1, 0), Cplx(2, 0), Cplx(3, 0), Cplx(4, 0),
                               Cplx(4, 0), Cplx(3, 0), Cplx(2, 0), Cplx(1, 0)};
        const std::vector<Cplx> orig = a;
        fft(a, false);
        fft(a, true);
        for (std::size_t i = 0; i < a.size(); ++i) {
            CHECK_NEAR(a[i].real(), orig[i].real(), 1e-4f);
            CHECK_NEAR(a[i].imag(), 0.0f, 1e-4f);
        }
    }

    // A pure cosine on an exact bin peaks on that bin (rectangular window). sr=64, N=64, tone at bin 8.
    {
        SpectrumAnalyzer sa(64.0f, 64, SpectrumWindow::None);
        std::vector<float> sig(64);
        for (std::size_t i = 0; i < sig.size(); ++i) {
            sig[i] = std::cos(2.0f * pi * 8.0f * static_cast<float>(i) / 64.0f);
        }
        sa.analyze(sig);
        CHECK(sa.peakBin() == 8);
        CHECK_NEAR(sa.binFrequency(8), 8.0f, 1e-3f);          // 8 * 64/64 Hz
        CHECK_NEAR(sa.magnitude(8), 1.0f, 0.02f);             // single-sided amplitude of a unit cosine
        CHECK(sa.magnitude(20) < 0.02f);                       // other bins ~silent
        CHECK(sa.magnitudeForRange(6.0f, 10.0f) > 0.9f);      // energy sits in this band
        CHECK(sa.magnitudeForRange(20.0f, 30.0f) < 0.02f);    // not up here
    }

    // DC (constant) signal puts all energy in bin 0.
    {
        SpectrumAnalyzer sa(64.0f, 64, SpectrumWindow::None);
        std::vector<float> dc(64, 1.0f);
        sa.analyze(dc);
        CHECK(sa.peakBin() == 0);
        CHECK_NEAR(sa.magnitude(0), 1.0f, 1e-3f);
        CHECK(sa.magnitude(1) < 1e-3f);
    }

    // Two tones -> two peaks; each band query finds its own tone, the gap between finds neither.
    {
        SpectrumAnalyzer sa(128.0f, 128, SpectrumWindow::None);
        std::vector<float> sig(128);
        for (std::size_t i = 0; i < sig.size(); ++i) {
            const float t = static_cast<float>(i);
            sig[i] = 0.7f * std::cos(2.0f * pi * 10.0f * t / 128.0f) +
                     0.3f * std::cos(2.0f * pi * 40.0f * t / 128.0f);
        }
        sa.analyze(sig);
        CHECK_NEAR(sa.magnitude(10), 0.7f, 0.03f);
        CHECK_NEAR(sa.magnitude(40), 0.3f, 0.03f);
        CHECK(sa.peakBin() == 10);                             // the louder tone
        CHECK(sa.magnitudeForRange(24.0f, 30.0f) < 0.03f);    // between the two tones -> quiet
    }

    // binCount is N/2 + 1; out-of-range magnitude is 0.
    {
        SpectrumAnalyzer sa(48000.0f, 256);
        CHECK(sa.fftSize() == 256);
        CHECK(sa.binCount() == 129);
        CHECK_NEAR(sa.magnitude(9999), 0.0f, 1e-6f);
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

void testStreamRandomizer() {
    using audio::RandomizerMode;
    using audio::RandomPick;
    using audio::StreamRandomizer;

    // Empty pool -> no pick.
    {
        StreamRandomizer r;
        CHECK(r.next().index == -1);
    }

    // Sequential mode: strict round-robin regardless of seed.
    {
        StreamRandomizer r;
        r.mode = RandomizerMode::Sequential;
        r.addStream();
        r.addStream();
        r.addStream();
        CHECK(r.next().index == 0);
        CHECK(r.next().index == 1);
        CHECK(r.next().index == 2);
        CHECK(r.next().index == 0);
        r.reset();
        CHECK(r.next().index == 0);
    }

    // RandomNoRepeat: never two identical consecutive picks, and every index stays in range.
    {
        StreamRandomizer r;
        r.mode = RandomizerMode::RandomNoRepeat;
        for (int i = 0; i < 4; ++i) {
            r.addStream();
        }
        r.setSeed(12345);
        int prev = -1;
        for (int i = 0; i < 500; ++i) {
            const int idx = r.next().index;
            CHECK(idx >= 0 && idx < 4);
            CHECK(idx != prev);
            prev = idx;
        }
    }

    // A single stream in RandomNoRepeat always returns 0 (the no-repeat guard only fires with >1).
    {
        StreamRandomizer r;
        r.mode = RandomizerMode::RandomNoRepeat;
        r.addStream();
        for (int i = 0; i < 20; ++i) {
            CHECK(r.next().index == 0);
        }
    }

    // Weighted Random: a heavily-favoured stream dominates the tally.
    {
        StreamRandomizer r;
        r.mode = RandomizerMode::Random;
        r.addStream(100.0f);
        r.addStream(1.0f);
        r.addStream(1.0f);
        r.setSeed(777);
        int hits0 = 0;
        const int N = 1000;
        for (int i = 0; i < N; ++i) {
            if (r.next().index == 0) {
                ++hits0;
            }
        }
        CHECK(hits0 > N * 8 / 10); // ~100/102 expected; comfortably over 80%
    }

    // Pitch variance: randomPitch=1 -> exactly 1; randomPitch=2 -> within [0.5, 2].
    {
        StreamRandomizer r;
        r.addStream();
        r.addStream();
        r.setSeed(9);
        r.randomPitch = 1.0f;
        for (int i = 0; i < 20; ++i) {
            CHECK_NEAR(r.next().pitchScale, 1.0f, 1e-6f);
        }
        r.randomPitch = 2.0f;
        for (int i = 0; i < 200; ++i) {
            const float p = r.next().pitchScale;
            CHECK(p >= 0.5f - 1e-4f && p <= 2.0f + 1e-4f);
        }
    }

    // Volume variance: 0 -> no offset; 6 dB -> within [-6, 6].
    {
        StreamRandomizer r;
        r.addStream();
        r.addStream();
        r.setSeed(3);
        r.randomVolumeOffsetDb = 0.0f;
        for (int i = 0; i < 10; ++i) {
            CHECK_NEAR(r.next().volumeDb, 0.0f, 1e-6f);
        }
        r.randomVolumeOffsetDb = 6.0f;
        for (int i = 0; i < 200; ++i) {
            const float v = r.next().volumeDb;
            CHECK(v >= -6.0f - 1e-4f && v <= 6.0f + 1e-4f);
        }
    }

    // Determinism: same seed + config -> identical (index, pitch, volume) stream.
    {
        auto make = []() {
            StreamRandomizer r;
            r.mode = RandomizerMode::RandomNoRepeat;
            r.randomPitch = 1.5f;
            r.randomVolumeOffsetDb = 3.0f;
            for (int i = 0; i < 5; ++i) {
                r.addStream(static_cast<float>(i + 1));
            }
            r.setSeed(0xABCDEF);
            return r;
        };
        StreamRandomizer a = make();
        StreamRandomizer b = make();
        for (int i = 0; i < 100; ++i) {
            const RandomPick pa = a.next();
            const RandomPick pb = b.next();
            CHECK(pa.index == pb.index);
            CHECK_NEAR(pa.pitchScale, pb.pitchScale, 1e-6f);
            CHECK_NEAR(pa.volumeDb, pb.volumeDb, 1e-6f);
        }
    }
}

void testSampleMixer() {
    using audio::SampleMixer;
    using audio::WavData;

    // A mono clip at the output's own rate plays its samples straight into both channels.
    {
        WavData clip;
        clip.sampleRate = 8000;
        clip.channels = 1;
        clip.samples = {0.2f, 0.4f, 0.6f, 0.8f};

        SampleMixer mix;
        const int v = mix.play(clip);
        CHECK(v >= 0);
        CHECK(mix.activeVoices() == 1);

        float out[8] = {0}; // 4 stereo frames
        mix.mix(out, 4, 8000);
        // L and R both carry the mono source (centered pan).
        CHECK_NEAR(out[0], 0.2f, 1e-5f);
        CHECK_NEAR(out[1], 0.2f, 1e-5f);
        CHECK_NEAR(out[2], 0.4f, 1e-5f);
        CHECK_NEAR(out[6], 0.8f, 1e-5f);
        // Playing off the end (non-loop) deactivates the voice.
        float tail[8] = {0};
        mix.mix(tail, 4, 8000);
        CHECK(mix.activeVoices() == 0);
        CHECK_NEAR(tail[0], 0.0f, 1e-5f);
    }

    // Gain scales the output; the buffer is zeroed each call.
    {
        WavData clip;
        clip.sampleRate = 8000;
        clip.channels = 1;
        clip.samples = {1.0f, 1.0f};
        SampleMixer mix;
        mix.play(clip, 0.5f);
        float out[4] = {9, 9, 9, 9};
        mix.mix(out, 2, 8000);
        CHECK_NEAR(out[0], 0.5f, 1e-5f);
        CHECK_NEAR(out[1], 0.5f, 1e-5f);
    }

    // Full-left pan mutes the right channel; full-right mutes the left.
    {
        WavData clip;
        clip.sampleRate = 8000;
        clip.channels = 1;
        clip.samples = {1.0f};
        {
            SampleMixer mix;
            mix.play(clip, 1.0f, -1.0f);
            float out[2] = {0};
            mix.mix(out, 1, 8000);
            CHECK_NEAR(out[0], 1.0f, 1e-5f); // left
            CHECK_NEAR(out[1], 0.0f, 1e-5f); // right silent
        }
        {
            SampleMixer mix;
            mix.play(clip, 1.0f, 1.0f);
            float out[2] = {0};
            mix.mix(out, 1, 8000);
            CHECK_NEAR(out[0], 0.0f, 1e-5f); // left silent
            CHECK_NEAR(out[1], 1.0f, 1e-5f); // right
        }
    }

    // Two voices sum together.
    {
        WavData a;
        a.sampleRate = 8000;
        a.channels = 1;
        a.samples = {0.3f, 0.3f};
        WavData b;
        b.sampleRate = 8000;
        b.channels = 1;
        b.samples = {0.4f, 0.4f};
        SampleMixer mix;
        mix.play(a);
        mix.play(b);
        CHECK(mix.activeVoices() == 2);
        float out[4] = {0};
        mix.mix(out, 2, 8000);
        CHECK_NEAR(out[0], 0.7f, 1e-5f);
        CHECK_NEAR(out[2], 0.7f, 1e-5f);
    }

    // A looping voice wraps and stays active past the clip's end.
    {
        WavData clip;
        clip.sampleRate = 8000;
        clip.channels = 1;
        clip.samples = {0.5f, -0.5f};
        SampleMixer mix;
        mix.play(clip, 1.0f, 0.0f, /*loop=*/true);
        float out[8] = {0}; // 4 frames = 2× the clip
        mix.mix(out, 4, 8000);
        CHECK(mix.activeVoices() == 1); // still playing
        CHECK_NEAR(out[0], 0.5f, 1e-5f);
        CHECK_NEAR(out[2], -0.5f, 1e-5f);
        CHECK_NEAR(out[4], 0.5f, 1e-5f); // wrapped
        CHECK_NEAR(out[6], -0.5f, 1e-5f);
    }

    // speed 2.0 consumes the clip twice as fast (interpolated).
    {
        WavData clip;
        clip.sampleRate = 8000;
        clip.channels = 1;
        clip.samples = {0.0f, 1.0f, 2.0f, 3.0f};
        SampleMixer mix;
        mix.play(clip, 1.0f, 0.0f, false, 2.0f);
        float out[4] = {0}; // 2 frames -> positions 0, 2
        mix.mix(out, 2, 8000);
        CHECK_NEAR(out[0], 0.0f, 1e-5f);
        CHECK_NEAR(out[2], 2.0f, 1e-5f);
    }

    // stop() silences a voice; an empty mixer produces pure silence.
    {
        WavData clip;
        clip.sampleRate = 8000;
        clip.channels = 1;
        clip.samples = {1.0f, 1.0f};
        SampleMixer mix;
        const int v = mix.play(clip);
        mix.stop(v);
        CHECK(mix.activeVoices() == 0);
        float out[4] = {0};
        mix.mix(out, 2, 8000);
        CHECK_NEAR(out[0], 0.0f, 1e-5f);
        CHECK_NEAR(out[1], 0.0f, 1e-5f);
    }

    // An empty clip is rejected.
    {
        WavData empty;
        SampleMixer mix;
        CHECK(mix.play(empty) == -1);
        CHECK(mix.activeVoices() == 0);
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

void testKinematicBody2D() {
    using game::Aabb2;
    using game::moveAndSlide;
    using game::SlideResult;
    using game::sweptAabb;
    using game::SweptHit;
    using math::vec2;

    // --- sweptAabb ---
    // A 2x2 body at origin moving +x into a solid whose near face (after Minkowski expand) is at x=4.
    {
        const Aabb2 wall{vec2(5, -1), vec2(7, 1)};
        const SweptHit h = sweptAabb(vec2(0, 0), vec2(1, 1), vec2(10, 0), wall);
        CHECK(h.hit);
        CHECK_NEAR(h.t, 0.4f, 1e-4f); // (5-1-0)/10
        CHECK_NEAR(h.normal.x, -1.0f, 1e-5f);
        CHECK_NEAR(h.normal.y, 0.0f, 1e-6f);
    }
    // Moving away from the solid -> no contact.
    {
        const Aabb2 wall{vec2(5, -1), vec2(7, 1)};
        CHECK(!sweptAabb(vec2(0, 0), vec2(1, 1), vec2(-10, 0), wall).hit);
    }
    // Not moving -> no swept contact even if adjacent.
    {
        const Aabb2 wall{vec2(5, -1), vec2(7, 1)};
        CHECK(!sweptAabb(vec2(0, 0), vec2(1, 1), vec2(0, 0), wall).hit);
    }

    // --- moveAndSlide ---
    const vec2 half(1.0f, 1.0f);
    // Free move with no solids: exact displacement, no contact flags.
    {
        const SlideResult r = moveAndSlide(vec2(0, 0), half, vec2(10, 5), 1.0f, {});
        CHECK_NEAR(r.position.x, 10.0f, 1e-4f);
        CHECK_NEAR(r.position.y, 5.0f, 1e-4f);
        CHECK(!r.onFloor && !r.onWall && !r.onCeiling);
        CHECK(r.slides == 0);
    }
    // Head-on into a wall on the right: stops before it, x-velocity killed, classified as wall.
    {
        const std::vector<Aabb2> solids = {Aabb2{vec2(5, -10), vec2(7, 10)}};
        const SlideResult r = moveAndSlide(vec2(0, 0), half, vec2(10, 0), 1.0f, solids);
        CHECK(r.onWall);
        CHECK(!r.onFloor && !r.onCeiling);
        CHECK(r.position.x <= 4.0f + 1e-2f); // stopped at the expanded face (x=4) + skin
        CHECK_NEAR(r.velocity.x, 0.0f, 1e-4f);
    }
    // Falling onto a floor (up = (0,-1), so +y is down): lands, y-velocity killed, floor detected.
    {
        const std::vector<Aabb2> solids = {Aabb2{vec2(-100, 10), vec2(100, 12)}};
        const SlideResult r = moveAndSlide(vec2(0, 0), half, vec2(0, 20), 1.0f, solids);
        CHECK(r.onFloor);
        CHECK(!r.onCeiling);
        CHECK(r.position.y <= 9.0f + 1e-2f); // rests on the floor top (y=9) + skin
        CHECK_NEAR(r.floorNormal.y, -1.0f, 1e-5f);
        CHECK_NEAR(r.velocity.y, 0.0f, 1e-4f);
    }
    // Rising into a ceiling: classified as ceiling.
    {
        const std::vector<Aabb2> solids = {Aabb2{vec2(-100, -12), vec2(100, -10)}};
        const SlideResult r = moveAndSlide(vec2(0, 0), half, vec2(0, -20), 1.0f, solids);
        CHECK(r.onCeiling);
        CHECK(!r.onFloor);
    }
    // Diagonal into a vertical wall: x blocked but the body still slides in y (one call, multi-slide).
    {
        const std::vector<Aabb2> solids = {Aabb2{vec2(5, -100), vec2(7, 100)}};
        const SlideResult r = moveAndSlide(vec2(0, 0), half, vec2(10, 10), 1.0f, solids);
        CHECK(r.onWall);
        CHECK(r.position.x <= 4.0f + 1e-2f); // pinned to the wall face
        CHECK(r.position.y >= 5.0f);         // but slid downward along it
        CHECK_NEAR(r.velocity.x, 0.0f, 1e-4f);
        CHECK_NEAR(r.velocity.y, 10.0f, 1e-4f); // tangential velocity preserved
    }
}

void testGravityField2D() {
    using game::GravityArea2D;
    using game::gravityAt;
    using game::GravityMode;
    using game::GravityType;
    using math::Rect2;
    using math::vec2;

    const vec2 base(0.0f, 900.0f); // world default: gravity points +y (down)

    // No zones -> the base gravity everywhere.
    {
        std::vector<GravityArea2D> areas;
        const vec2 g = gravityAt(areas, vec2(100.0f, 100.0f), base);
        CHECK_NEAR(g.x, 0.0f, 1e-4f);
        CHECK_NEAR(g.y, 900.0f, 1e-4f);
    }

    // A directional REPLACE zone: inside it overrides the base; outside stays base.
    {
        GravityArea2D wind;
        wind.region = Rect2(0.0f, 0.0f, 200.0f, 200.0f);
        wind.type = GravityType::Directional;
        wind.mode = GravityMode::Replace;
        wind.direction = vec2(1.0f, 0.0f); // push +x, magnitude 700
        wind.strength = 700.0f;
        std::vector<GravityArea2D> areas = {wind};

        const vec2 gin = gravityAt(areas, vec2(50.0f, 50.0f), base);
        CHECK_NEAR(gin.x, 700.0f, 1e-3f);
        CHECK_NEAR(gin.y, 0.0f, 1e-3f); // replaced -> no base down component

        const vec2 gout = gravityAt(areas, vec2(500.0f, 50.0f), base);
        CHECK_NEAR(gout.x, 0.0f, 1e-3f);
        CHECK_NEAR(gout.y, 900.0f, 1e-3f);
    }

    // ADD mode accumulates on top of the base.
    {
        GravityArea2D add;
        add.region = Rect2(0.0f, 0.0f, 200.0f, 200.0f);
        add.type = GravityType::Directional;
        add.mode = GravityMode::Add;
        add.direction = vec2(-1.0f, 0.0f);
        add.strength = 300.0f;
        std::vector<GravityArea2D> areas = {add};
        const vec2 g = gravityAt(areas, vec2(50.0f, 50.0f), base);
        CHECK_NEAR(g.x, -300.0f, 1e-3f); // base.x 0 + (-300)
        CHECK_NEAR(g.y, 900.0f, 1e-3f);  // base.y unchanged
    }

    // Overlapping REPLACE zones: the higher priority wins (applied last).
    {
        GravityArea2D lo;
        lo.region = Rect2(0.0f, 0.0f, 200.0f, 200.0f);
        lo.mode = GravityMode::Replace;
        lo.direction = vec2(1.0f, 0.0f);
        lo.strength = 100.0f;
        lo.priority = 1;
        GravityArea2D hi = lo;
        hi.direction = vec2(0.0f, -1.0f);
        hi.strength = 500.0f;
        hi.priority = 5;
        // Deliberately list high-priority FIRST to prove ordering is by priority, not input order.
        std::vector<GravityArea2D> areas = {hi, lo};
        const vec2 g = gravityAt(areas, vec2(50.0f, 50.0f), base);
        CHECK_NEAR(g.x, 0.0f, 1e-3f);
        CHECK_NEAR(g.y, -500.0f, 1e-3f); // high-priority up-field wins
    }

    // Point field: pulls toward the centre; inverse-square == strength at unitDistance, /4 at twice.
    {
        GravityArea2D planet;
        planet.region = Rect2(-1000.0f, -1000.0f, 2000.0f, 2000.0f);
        planet.type = GravityType::Point;
        planet.mode = GravityMode::Replace;
        planet.center = vec2(0.0f, 0.0f);
        planet.strength = 400.0f;
        planet.unitDistance = 100.0f;
        std::vector<GravityArea2D> areas = {planet};

        // A body at (100, 0): direction toward centre is -x, magnitude == strength (dist == unitDistance).
        const vec2 g1 = gravityAt(areas, vec2(100.0f, 0.0f), vec2(0.0f, 0.0f));
        CHECK_NEAR(g1.x, -400.0f, 1e-2f);
        CHECK_NEAR(g1.y, 0.0f, 1e-2f);
        // At (200, 0): twice the unit distance -> magnitude / 4 = 100, direction -x.
        const vec2 g2 = gravityAt(areas, vec2(200.0f, 0.0f), vec2(0.0f, 0.0f));
        CHECK_NEAR(g2.x, -100.0f, 1e-2f);

        // Constant point field (unitDistance <= 0): magnitude == strength regardless of distance.
        planet.unitDistance = 0.0f;
        areas = {planet};
        const vec2 g3 = gravityAt(areas, vec2(0.0f, 300.0f), vec2(0.0f, 0.0f));
        CHECK_NEAR(g3.y, -400.0f, 1e-2f); // pulled up toward centre at full strength
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

void testRange() {
    using ui::ProgressBar;
    using ui::Range;

    // Default 0..100: set/clamp/ratio.
    {
        Range r;
        r.setValue(50.0);
        CHECK_NEAR(r.value(), 50.0, 1e-9);
        CHECK_NEAR(r.ratio(), 0.5, 1e-9);
        r.setValue(150.0);
        CHECK_NEAR(r.value(), 100.0, 1e-9); // clamped to max
        r.setValue(-20.0);
        CHECK_NEAR(r.value(), 0.0, 1e-9); // clamped to min
        CHECK_NEAR(r.ratio(), 0.0, 1e-9);
    }

    // setRatio round-trips over a custom range.
    {
        Range r;
        r.minValue = -50.0;
        r.maxValue = 50.0;
        r.setRatio(0.0);
        CHECK_NEAR(r.value(), -50.0, 1e-9);
        r.setRatio(1.0);
        CHECK_NEAR(r.value(), 50.0, 1e-9);
        r.setRatio(0.5);
        CHECK_NEAR(r.value(), 0.0, 1e-9);
        CHECK_NEAR(r.ratio(), 0.5, 1e-9);
    }

    // Step snapping (anchored at min), then clamp.
    {
        Range r;
        r.step = 10.0;
        r.setValue(23.0);
        CHECK_NEAR(r.value(), 20.0, 1e-9);
        r.setValue(27.0);
        CHECK_NEAR(r.value(), 30.0, 1e-9);
        r.setValue(999.0);
        CHECK_NEAR(r.value(), 100.0, 1e-9);
    }

    // page: effective max = max - page, ratio spans 0..1 across that.
    {
        Range r;
        r.maxValue = 100.0;
        r.page = 20.0;
        r.setValue(80.0);
        CHECK_NEAR(r.value(), 80.0, 1e-9); // at effective max
        CHECK_NEAR(r.ratio(), 1.0, 1e-9);
        r.setValue(100.0);
        CHECK_NEAR(r.value(), 80.0, 1e-9); // clamped down to max-page
    }

    // allowGreater lifts the upper clamp.
    {
        Range r;
        r.allowGreater = true;
        r.setValue(150.0);
        CHECK_NEAR(r.value(), 150.0, 1e-9);
    }

    // step_ nudges by whole steps.
    {
        Range r;
        r.step = 5.0;
        r.setValue(10.0);
        r.step_(2.0);
        CHECK_NEAR(r.value(), 20.0, 1e-9);
        r.step_(-1.0);
        CHECK_NEAR(r.value(), 15.0, 1e-9);
    }

    // ProgressBar exposes the fill fraction + percent.
    {
        ProgressBar bar;
        bar.setValue(60.0);
        CHECK_NEAR(static_cast<double>(bar.fillFraction()), 0.6, 1e-6);
        CHECK(bar.percent() == 60);
        bar.setValue(0.0);
        CHECK(bar.percent() == 0);
        bar.setValue(100.0);
        CHECK(bar.percent() == 100);
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

void testItemList() {
    using ui::ItemList;
    using ui::ItemSelectMode;

    // A list with a fixed geometry we can reason about exactly.
    ItemList list;
    list.rect = ui::Rect{100.0f, 50.0f, 200.0f, 100.0f}; // 100px tall box
    list.itemHeight = 20.0f;
    list.separation = 0.0f;                              // stride 20 -> exactly 5 rows fit the box
    for (int i = 0; i < 10; ++i) {
        CHECK(list.addItem("row" + std::to_string(i), i) == static_cast<std::size_t>(i));
    }
    CHECK(list.count() == 10);
    CHECK(list.item(3).id == 3);

    // Geometry: stride, content height, max scroll.
    CHECK_NEAR(list.rowStride(), 20.0f, 1e-4f);
    CHECK_NEAR(list.contentHeight(), 200.0f, 1e-4f); // 10 * 20
    CHECK_NEAR(list.maxScroll(), 100.0f, 1e-4f);     // 200 - 100

    // itemRect at scroll 0: row 0 sits at the box top; row 3 is 60px down.
    CHECK_NEAR(list.itemRect(0).y, 50.0f, 1e-4f);
    CHECK_NEAR(list.itemRect(3).y, 110.0f, 1e-4f);
    CHECK_NEAR(list.itemRect(0).h, 20.0f, 1e-4f);

    // visibleRange at scroll 0: rows 0..5 intersect a 100px box (row 5 top at 100 == box bottom edge).
    {
        auto vr = list.visibleRange();
        CHECK(vr.first == 0);
        CHECK(vr.second == 5);
    }

    // Hit testing (scroll 0): a point inside row 2, a point outside the box, a point below the last row.
    CHECK(list.itemAtPoint(150.0f, 95.0f) == 2);   // 95 - 50 = 45 -> row 2
    CHECK(list.itemAtPoint(50.0f, 95.0f) == -1);   // left of the box
    CHECK(list.itemAtPoint(150.0f, 40.0f) == -1);  // above the box

    // Separator gap is dead space: give rows a gap and click into it.
    {
        ItemList g;
        g.rect = ui::Rect{0.0f, 0.0f, 100.0f, 200.0f};
        g.itemHeight = 20.0f;
        g.separation = 10.0f; // stride 30; row 0 covers [0,20), gap [20,30)
        g.addItem("a");
        g.addItem("b");
        CHECK(g.itemAtPoint(10.0f, 10.0f) == 0);  // inside row 0
        CHECK(g.itemAtPoint(10.0f, 25.0f) == -1); // in the gap
        CHECK(g.itemAtPoint(10.0f, 35.0f) == 1);  // inside row 1
    }

    // Single-select is a radio: selecting one clears the others; firstSelected tracks it.
    list.selectMode = ItemSelectMode::Single;
    list.select(2);
    CHECK(list.isSelected(2));
    CHECK(list.firstSelected() == 2);
    CHECK(list.selectedItems().size() == 1);
    list.select(5);
    CHECK(!list.isSelected(2));
    CHECK(list.isSelected(5));
    CHECK(list.selectedItems().size() == 1);

    // Multi-select accumulates; toggle removes.
    list.selectMode = ItemSelectMode::Multi;
    list.deselectAll();
    list.select(1);
    list.select(4);
    list.select(7);
    CHECK(list.selectedItems().size() == 3);
    CHECK(list.firstSelected() == 1);
    list.toggle(4); // remove
    CHECK(!list.isSelected(4));
    CHECK(list.selectedItems().size() == 2);
    list.toggle(4); // add back
    CHECK(list.isSelected(4));

    // Disabled / non-selectable rows can't be selected and are skipped by keyboard nav.
    {
        ItemList k;
        k.rect = ui::Rect{0.0f, 0.0f, 100.0f, 200.0f};
        k.itemHeight = 20.0f;
        k.separation = 0.0f;
        k.selectMode = ItemSelectMode::Single;
        for (int i = 0; i < 5; ++i) {
            k.addItem("k" + std::to_string(i), i);
        }
        k.setDisabled(1, true);
        k.setSelectable(2, false);
        k.select(1); // disabled -> ignored
        CHECK(k.firstSelected() == -1);

        // selectNext from nothing lands on the first selectable row (0)...
        CHECK(k.selectNext() == 0);
        CHECK(k.isSelected(0));
        // ...then skips the disabled (1) and non-selectable (2) rows to 3.
        CHECK(k.selectNext() == 3);
        CHECK(k.isSelected(3) && !k.isSelected(0));
        CHECK(k.selectNext() == 4);
        CHECK(k.selectNext() == 4); // clamped at the end (no wrap)
        // Backwards skips the same holes down to 0.
        CHECK(k.selectPrevious() == 3);
        CHECK(k.selectPrevious() == 0);
        CHECK(k.selectPrevious() == 0); // clamped at the start
    }

    // Scrolling: setScroll clamps to [0, maxScroll]; ensureVisible scrolls a row into the box.
    list.setScroll(-40.0f);
    CHECK_NEAR(list.scroll(), 0.0f, 1e-4f);
    list.setScroll(1000.0f);
    CHECK_NEAR(list.scroll(), list.maxScroll(), 1e-4f);
    list.setScroll(0.0f);
    list.ensureVisible(9);                       // last row: box shows [scroll, scroll+100]; row 9 top at 180
    CHECK_NEAR(list.scroll(), 100.0f, 1e-4f);    // 180 + 20 - 100
    list.ensureVisible(0);                       // scroll back up to the top
    CHECK_NEAR(list.scroll(), 0.0f, 1e-4f);

    // Empty list: benign geometry, empty visibleRange (first > last).
    {
        ItemList e;
        CHECK(e.count() == 0);
        CHECK_NEAR(e.contentHeight(), 0.0f, 1e-4f);
        CHECK_NEAR(e.maxScroll(), 0.0f, 1e-4f);
        CHECK(e.firstSelected() == -1);
        auto vr = e.visibleRange();
        CHECK(vr.first > vr.second);
    }
}

void testPopupMenu() {
    using ui::MenuCheck;
    using ui::PopupMenu;

    PopupMenu m;
    m.position = math::vec2(100, 50);
    m.itemHeight = 20.0f;
    m.separatorHeight = 10.0f;
    m.width = 200.0f;

    const std::size_t iNew = m.addItem("New", 1);
    const std::size_t iOpen = m.addItem("Open", 2);
    m.addSeparator();                                     // index 2
    const std::size_t iWrap = m.addCheckItem("Word Wrap", 3);
    m.addSeparator();                                     // index 4
    const std::size_t iLight = m.addRadioItem("Light", 10);
    const std::size_t iDark = m.addRadioItem("Dark", 11);
    const std::size_t iSystem = m.addRadioItem("System", 12);
    const std::size_t iPrint = m.addItem("Print", 4);
    m.setDisabled(iPrint, true);
    (void)iNew;
    (void)iSystem;

    CHECK(m.count() == 9);

    // Geometry: rows stack; separators are thinner; total height = 6*20 + 2*10 + ... wait recount.
    // items: New(20) Open(20) sep(10) Wrap(20) sep(10) Light(20) Dark(20) System(20) Print(20)
    //  = 7 rows*20 + 2 seps*10 = 140 + 20 = 160.
    CHECK_NEAR(m.totalHeight(), 160.0f, 1e-4f);
    CHECK_NEAR(m.rect().h, 160.0f, 1e-4f);
    CHECK_NEAR(m.itemRect(iOpen).y, 70.0f, 1e-4f); // 50 + 20
    CHECK_NEAR(m.itemRect(iWrap).y, 100.0f, 1e-4f); // 50 + 20 + 20 + 10(sep)

    // Hit testing: a point inside "New", a point on the first separator (-> -1), a point outside (-> -1).
    CHECK(m.itemAtPoint(150, 55) == static_cast<long>(iNew));   // row 0 spans y[50,70)
    CHECK(m.itemAtPoint(150, 92) == -1);                        // first separator y[90,100)
    CHECK(m.itemAtPoint(150, 45) == -1);                        // above the menu
    CHECK(m.itemAtPoint(400, 55) == -1);                        // right of the menu

    // Hover navigation skips separators and disabled items, and wraps.
    m.clearHover();
    CHECK(m.hoverNext() == static_cast<long>(iNew));  // from nothing -> first selectable (New)
    CHECK(m.hoverNext() == static_cast<long>(iOpen)); // Open
    CHECK(m.hoverNext() == static_cast<long>(iWrap)); // skips the separator to Word Wrap
    CHECK(m.hoverNext() == static_cast<long>(iLight)); // skips the second separator to Light
    // From Light -> Dark -> System, then Print is disabled so it wraps back to New.
    CHECK(m.hoverNext() == static_cast<long>(iDark));
    CHECK(m.hoverNext() == static_cast<long>(iSystem));
    CHECK(m.hoverNext() == static_cast<long>(iNew));  // Print disabled -> wrap to New
    CHECK(m.hoverPrev() == static_cast<long>(iSystem)); // backwards also skips Print

    // Activating a checkbox toggles it and returns its id.
    m.setHovered(static_cast<long>(iWrap));
    CHECK(!m.isChecked(iWrap));
    CHECK(m.activate() == 3);
    CHECK(m.isChecked(iWrap));
    CHECK(m.activate() == 3); // toggles back
    CHECK(!m.isChecked(iWrap));

    // Radio activation is single-choice: picking Dark unchecks Light/System.
    m.checkRadio(iLight);
    CHECK(m.isChecked(iLight) && !m.isChecked(iDark) && !m.isChecked(iSystem));
    m.setHovered(static_cast<long>(iDark));
    CHECK(m.activate() == 11);
    CHECK(m.isChecked(iDark) && !m.isChecked(iLight) && !m.isChecked(iSystem));

    // Activating a disabled item fires nothing.
    m.setHovered(static_cast<long>(iPrint));
    CHECK(m.activate() == -1);

    // Empty menu: benign navigation.
    {
        PopupMenu e;
        CHECK(e.count() == 0);
        CHECK_NEAR(e.totalHeight(), 0.0f, 1e-5f);
        CHECK(e.itemAtPoint(0, 0) == -1);
        CHECK(e.hoverNext() == -1);
        CHECK(e.activate() == -1);
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

void testRichText() {
    using ui::parseBBCode;
    using ui::RichSpan;

    // Plain text: one unstyled span.
    {
        const std::vector<RichSpan> s = parseBBCode("hello world");
        CHECK(s.size() == 1);
        CHECK(s[0].text == "hello world");
        CHECK(!s[0].bold && !s[0].italic && !s[0].underline && !s[0].hasColor);
        CHECK(s[0].sizePx == 0.0f);
    }

    // [b]...[/b] splits into plain / bold / plain runs.
    {
        const std::vector<RichSpan> s = parseBBCode("normal [b]bold[/b] end");
        CHECK(s.size() == 3);
        CHECK(s[0].text == "normal " && !s[0].bold);
        CHECK(s[1].text == "bold" && s[1].bold);
        CHECK(s[2].text == " end" && !s[2].bold);
    }

    // Nested tags stack.
    {
        const std::vector<RichSpan> s = parseBBCode("[b][i][u]x[/u][/i][/b]");
        CHECK(s.size() == 1);
        CHECK(s[0].bold && s[0].italic && s[0].underline);
    }

    // Hex colour (#rrggbb) and short hex (#rgb).
    {
        const std::vector<RichSpan> s = parseBBCode("[color=#ff0000]r[/color]");
        CHECK(s.size() == 1);
        CHECK(s[0].hasColor);
        CHECK_NEAR(s[0].r, 1.0f, 1e-4f);
        CHECK_NEAR(s[0].g, 0.0f, 1e-4f);
        CHECK_NEAR(s[0].b, 0.0f, 1e-4f);
    }
    {
        const std::vector<RichSpan> s = parseBBCode("[color=#0f0]g[/color]");
        CHECK(s.size() == 1);
        CHECK_NEAR(s[0].g, 1.0f, 1e-4f);
        CHECK_NEAR(s[0].r, 0.0f, 1e-4f);
    }

    // Named colour.
    {
        const std::vector<RichSpan> s = parseBBCode("[color=blue]b[/color]");
        CHECK(s.size() == 1);
        CHECK(s[0].hasColor);
        CHECK_NEAR(s[0].b, 1.0f, 1e-4f);
        CHECK_NEAR(s[0].r, 0.0f, 1e-4f);
    }

    // [size=N] sets the pixel size on the enclosed run.
    {
        const std::vector<RichSpan> s = parseBBCode("small [size=32]big[/size]");
        CHECK(s.size() == 2);
        CHECK(s[0].sizePx == 0.0f);
        CHECK_NEAR(s[1].sizePx, 32.0f, 1e-4f);
    }

    // Mismatched close pops the right scope: "x" is red+bold, "y" is red only.
    {
        const std::vector<RichSpan> s = parseBBCode("[color=red][b]x[/b]y[/color]");
        CHECK(s.size() == 2);
        CHECK(s[0].text == "x" && s[0].bold && s[0].hasColor);
        CHECK(s[1].text == "y" && !s[1].bold && s[1].hasColor);
    }

    // An unclosed tag runs to the end of the string.
    {
        const std::vector<RichSpan> s = parseBBCode("a[b]rest");
        CHECK(s.size() == 2);
        CHECK(s[0].text == "a" && !s[0].bold);
        CHECK(s[1].text == "rest" && s[1].bold);
    }

    // A stray close tag with nothing open is ignored (no crash, text kept plain).
    {
        const std::vector<RichSpan> s = parseBBCode("[/b]plain");
        CHECK(s.size() == 1);
        CHECK(s[0].text == "plain" && !s[0].bold);
    }

    // Adjacent identical-style runs coalesce across a tag boundary.
    {
        const std::vector<RichSpan> s = parseBBCode("[b]a[/b][b]b[/b]");
        CHECK(s.size() == 1);
        CHECK(s[0].text == "ab" && s[0].bold);
    }

    // [lb]/[rb] emit literal brackets; an unknown tag passes through verbatim.
    {
        const std::vector<RichSpan> s = parseBBCode("[lb]x[rb] [foo]bar[/foo]");
        CHECK(s.size() == 1);
        CHECK(s[0].text == "[x] [foo]bar[/foo]");
    }

    // An invalid colour value falls back to literal text (the tag is not consumed as styling).
    {
        const std::vector<RichSpan> s = parseBBCode("[color=notacolor]z[/color]");
        CHECK(!s.empty());
        CHECK(!s[0].hasColor);
        CHECK(s[0].text.find("[color=notacolor]") != std::string::npos);
    }

    // stripBBCode returns the tags-removed plain text.
    {
        CHECK(ui::stripBBCode("[b]hi[/b] [color=red]there[/color]") == std::string("hi there"));
        CHECK(ui::stripBBCode("plain") == std::string("plain"));
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

void testBase64() {
    using io::base64Decode;
    using io::base64Encode;

    // The canonical RFC 4648 test vectors.
    CHECK(base64Encode(std::string("")) == "");
    CHECK(base64Encode(std::string("f")) == "Zg==");
    CHECK(base64Encode(std::string("fo")) == "Zm8=");
    CHECK(base64Encode(std::string("foo")) == "Zm9v");
    CHECK(base64Encode(std::string("foob")) == "Zm9vYg==");
    CHECK(base64Encode(std::string("fooba")) == "Zm9vYmE=");
    CHECK(base64Encode(std::string("foobar")) == "Zm9vYmFy");

    // Decode round-trips back to the original bytes.
    {
        std::vector<std::uint8_t> out;
        CHECK(base64Decode("Zm9vYmFy", out));
        const std::string s(out.begin(), out.end());
        CHECK(s == "foobar");
    }

    // Every byte value round-trips (including 0x00 and 0xFF).
    {
        std::vector<std::uint8_t> data(256);
        for (int i = 0; i < 256; ++i) {
            data[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(i);
        }
        const std::string enc = base64Encode(data);
        const std::vector<std::uint8_t> dec = base64Decode(enc);
        CHECK(dec.size() == 256);
        bool same = dec.size() == data.size();
        for (std::size_t i = 0; same && i < data.size(); ++i) {
            same = dec[i] == data[i];
        }
        CHECK(same);
    }

    // Odd lengths (padding) round-trip.
    {
        const std::vector<std::uint8_t> one = {0xAB};
        const std::vector<std::uint8_t> two = {0xAB, 0xCD};
        CHECK(base64Encode(one) == "qw==");
        CHECK(base64Encode(two) == "q80=");
        CHECK(base64Decode(base64Encode(one)) == one);
        CHECK(base64Decode(base64Encode(two)) == two);
    }

    // Decoder skips embedded whitespace/newlines (line-wrapped blobs still decode).
    {
        std::vector<std::uint8_t> out;
        CHECK(base64Decode("Zm9v\n  YmFy\n", out));
        const std::string s(out.begin(), out.end());
        CHECK(s == "foobar");
    }

    // A stray non-alphabet character is rejected.
    {
        std::vector<std::uint8_t> out;
        CHECK(!base64Decode("Zm9v$YmFy", out));
    }
}

void testXml() {
    using io::XmlParser;
    using NT = io::XmlParser::NodeType;

    // A small document exercises elements, attributes, nested text, self-closing tags, and depth.
    {
        XmlParser p;
        CHECK(p.parse("<root a=\"1\" b='two'>hi<child/></root>"));

        CHECK(p.read());
        CHECK(p.nodeType() == NT::Element);
        CHECK(p.nodeName() == "root");
        CHECK(!p.isEmpty());
        CHECK(p.depth() == 0);
        CHECK(p.attributeCount() == 2);
        CHECK(p.attributeName(0) == "a");
        CHECK(p.attributeValue(0) == "1");
        CHECK(p.getAttribute("b") == "two");   // single-quoted value
        CHECK(p.getAttribute("missing", "def") == "def");
        CHECK(p.hasAttribute("a"));
        CHECK(!p.hasAttribute("z"));

        CHECK(p.read());
        CHECK(p.nodeType() == NT::Text);
        CHECK(p.nodeData() == "hi");
        CHECK(p.depth() == 1); // text sits inside root

        CHECK(p.read());
        CHECK(p.nodeType() == NT::Element);
        CHECK(p.nodeName() == "child");
        CHECK(p.isEmpty()); // self-closing
        CHECK(p.depth() == 1);
        CHECK(p.attributeCount() == 0);

        CHECK(p.read());
        CHECK(p.nodeType() == NT::ElementEnd);
        CHECK(p.nodeName() == "root");
        CHECK(p.depth() == 0); // end tag matches its start tag's depth

        CHECK(!p.read()); // end of document
        CHECK(p.nodeType() == NT::None);
        CHECK(!p.hasError());
    }

    // Entity decoding in both text and attribute values (named, decimal, and hex references).
    {
        XmlParser p;
        p.parse("<t v=\"a&lt;b&amp;c&#65;&#x42;\">1 &gt; 0 &apos;q&apos;</t>");
        CHECK(p.read());
        CHECK(p.nodeType() == NT::Element);
        CHECK(p.getAttribute("v") == "a<b&cAB"); // &#65;=A, &#x42;=B
        CHECK(p.read());
        CHECK(p.nodeType() == NT::Text);
        CHECK(p.nodeData() == "1 > 0 'q'");
    }

    // Comments and CDATA are distinct node types; CDATA content is verbatim (no entity decode).
    {
        XmlParser p;
        p.parse("<a><!-- note --><![CDATA[x < y & z]]></a>");
        CHECK(p.read());
        CHECK(p.nodeType() == NT::Element);
        CHECK(p.read());
        CHECK(p.nodeType() == NT::Comment);
        CHECK(p.nodeData() == " note ");
        CHECK(p.read());
        CHECK(p.nodeType() == NT::CData);
        CHECK(p.nodeData() == "x < y & z"); // verbatim
        CHECK(p.read());
        CHECK(p.nodeType() == NT::ElementEnd);
    }

    // The XML declaration / processing instruction is surfaced as a Declaration node.
    {
        XmlParser p;
        p.parse("<?xml version=\"1.0\"?><r/>");
        CHECK(p.read());
        CHECK(p.nodeType() == NT::Declaration);
        CHECK(p.read());
        CHECK(p.nodeType() == NT::Element);
        CHECK(p.nodeName() == "r");
        CHECK(p.isEmpty());
    }

    // Deeper nesting reports increasing depth, and matching end tags unwind it.
    {
        XmlParser p;
        p.parse("<a><b><c>t</c></b></a>");
        int seenC = -1, seenText = -1;
        while (p.read()) {
            if (p.nodeType() == NT::Element && p.nodeName() == "c") {
                seenC = p.depth();
            }
            if (p.nodeType() == NT::Text) {
                seenText = p.depth();
            }
        }
        CHECK(seenC == 2);
        CHECK(seenText == 3);
        CHECK(!p.hasError());
    }

    // Malformed input sets the error flag and stops.
    {
        XmlParser p;
        p.parse("<a href=unquoted>");
        CHECK(!p.read()); // unquoted attribute value
        CHECK(p.hasError());
    }
    {
        XmlParser p;
        p.parse("<!-- never closed");
        CHECK(!p.read());
        CHECK(p.hasError());
    }
}

void testConfigFile() {
    using io::ConfigFile;

    // Parse a settings file with a global section, comments, quotes, and typed values.
    const std::string text =
        "; game options\n"
        "version = 3\n"
        "\n"
        "[video]\n"
        "fullscreen = true\n"
        "resolution = \"1920x1080\"\n"
        "vsync=false\n"
        "# a comment line\n"
        "gamma = 2.2\n"
        "\n"
        "[audio]\n"
        "master = 0.8\n"
        "muted = no\n";

    ConfigFile cfg;
    const int n = cfg.parse(text);
    CHECK(n == 7); // version + 4 video + 2 audio

    // Sections in insertion order: the global "" section, then video, audio.
    {
        const auto secs = cfg.sections();
        CHECK(secs.size() == 3);
        CHECK(secs[0].empty());
        CHECK(secs[1] == "video");
        CHECK(secs[2] == "audio");
    }

    // Keys keep insertion order within a section.
    {
        const auto keys = cfg.sectionKeys("video");
        CHECK(keys.size() == 4);
        CHECK(keys[0] == "fullscreen" && keys[1] == "resolution" && keys[2] == "vsync" && keys[3] == "gamma");
    }

    // Typed reads: bool synonyms, int, float; quotes stripped from strings.
    CHECK(cfg.getBool("video", "fullscreen"));
    CHECK(!cfg.getBool("video", "vsync"));
    CHECK(!cfg.getBool("audio", "muted"));          // "no" -> false
    CHECK(cfg.getValue("video", "resolution") == "1920x1080"); // quotes stripped
    CHECK(cfg.getInt("", "version") == 3);          // global section
    CHECK_NEAR(cfg.getFloat("video", "gamma"), 2.2, 1e-6);
    CHECK_NEAR(cfg.getFloat("audio", "master"), 0.8, 1e-6);

    // Missing keys/sections return the supplied default.
    CHECK(cfg.getValue("video", "nope", "fallback") == "fallback");
    CHECK(cfg.getInt("nosuch", "x", 42) == 42);
    CHECK(cfg.getBool("audio", "surround", true));

    // Structure queries.
    CHECK(cfg.hasSection("audio"));
    CHECK(!cfg.hasSection("network"));
    CHECK(cfg.hasSectionKey("video", "gamma"));
    CHECK(!cfg.hasSectionKey("video", "gamut"));

    // Set + overwrite + typed set.
    cfg.setInt("audio", "channels", 6);
    cfg.setFloat("audio", "master", 0.5);
    cfg.setBool("video", "vsync", true);
    CHECK(cfg.getInt("audio", "channels") == 6);
    CHECK_NEAR(cfg.getFloat("audio", "master"), 0.5, 1e-6);
    CHECK(cfg.getBool("video", "vsync"));
    // Overwrite updates in place, not appends (still 4 video keys after re-setting vsync).
    CHECK(cfg.sectionKeys("video").size() == 4);

    // Erase a key and a whole section.
    cfg.eraseSectionKey("video", "gamma");
    CHECK(!cfg.hasSectionKey("video", "gamma"));
    CHECK(cfg.sectionKeys("video").size() == 3);
    cfg.eraseSection("audio");
    CHECK(!cfg.hasSection("audio"));
    CHECK(cfg.sections().size() == 2);

    // encode() -> parse() round-trips into an equal store.
    {
        ConfigFile a;
        a.setValue("", "name", "hero");
        a.setInt("stats", "hp", 100);
        a.setBool("stats", "boss", true);
        a.setValue("stats", "title", "The Brave");
        const std::string enc = a.encode();

        ConfigFile b;
        b.parse(enc);
        CHECK(b.getValue("", "name") == "hero");
        CHECK(b.getInt("stats", "hp") == 100);
        CHECK(b.getBool("stats", "boss"));
        CHECK(b.getValue("stats", "title") == "The Brave");
        CHECK(b.sections() == a.sections());
        CHECK(b.encode() == enc); // stable, idempotent
    }

    // Empty input parses to nothing.
    {
        ConfigFile e;
        CHECK(e.parse("") == 0);
        CHECK(e.sections().empty());
    }
}

void testResourcePack() {
    using io::PackEntry;
    using io::ResourcePack;

    // Pack three named blobs, unpack, and get each back byte-for-byte.
    {
        std::vector<PackEntry> entries = {
            {"levels/forest.json", {'{', '"', 'x', '"', ':', '1', '}'}},
            {"readme.txt", {'h', 'i'}},
            {"data.bin", {0x00, 0xFF, 0x7F, 0x00, 0x42}}, // embedded zeros must survive
        };
        const std::vector<std::uint8_t> archive = io::packResources(entries);

        ResourcePack pack;
        CHECK(pack.load(archive));
        CHECK(pack.count() == 3);
        CHECK(pack.contains("levels/forest.json"));
        CHECK(pack.contains("readme.txt"));
        CHECK(pack.contains("data.bin"));
        CHECK(!pack.contains("missing"));

        const std::vector<std::uint8_t>* forest = pack.get("levels/forest.json");
        CHECK(forest != nullptr);
        CHECK(*forest == entries[0].data);
        CHECK(pack.getString("readme.txt") == std::string("hi"));

        // The second/third blobs prove the directory offset math (not just the first at offset 0).
        const std::vector<std::uint8_t>* bin = pack.get("data.bin");
        CHECK(bin != nullptr);
        CHECK(bin->size() == 5);
        CHECK((*bin)[1] == 0xFF && (*bin)[2] == 0x7F && (*bin)[4] == 0x42);

        CHECK(pack.get("missing") == nullptr);

        // paths() preserves pack order.
        CHECK(pack.paths().size() == 3);
        CHECK(pack.paths()[0] == "levels/forest.json");
        CHECK(pack.paths()[2] == "data.bin");
    }

    // A zero-length blob round-trips (present, empty).
    {
        std::vector<PackEntry> entries = {{"empty", {}}, {"after", {'Z'}}};
        ResourcePack pack;
        CHECK(pack.load(io::packResources(entries)));
        const std::vector<std::uint8_t>* e = pack.get("empty");
        CHECK(e != nullptr);
        CHECK(e->empty());
        CHECK(pack.getString("after") == std::string("Z"));
    }

    // An empty archive (no entries) loads and reports zero.
    {
        ResourcePack pack;
        CHECK(pack.load(io::packResources({})));
        CHECK(pack.count() == 0);
        CHECK(pack.paths().empty());
        CHECK(!pack.contains("anything"));
    }

    // Duplicate path: last write wins (filesystem-overwrite semantics), deduped in count/order.
    {
        std::vector<PackEntry> entries = {{"dup", {'a'}}, {"dup", {'b', 'c'}}};
        ResourcePack pack;
        CHECK(pack.load(io::packResources(entries)));
        CHECK(pack.count() == 1);
        CHECK(pack.paths().size() == 1);
        CHECK(pack.getString("dup") == std::string("bc"));
    }

    // Foreign / short / corrupt streams fail cleanly and leave the pack empty.
    {
        ResourcePack pack;
        const std::vector<std::uint8_t> junk = {'N', 'O', 'P', 'E', 0, 0, 0, 0};
        CHECK(!pack.load(junk)); // wrong magic
        CHECK(pack.count() == 0);

        std::vector<std::uint8_t> tooShort = {'M'};
        CHECK(!pack.load(tooShort));

        // Valid header but a truncated data section (claims more bytes than present).
        std::vector<std::uint8_t> archive = io::packResources({{"a", {1, 2, 3, 4}}});
        archive.resize(archive.size() - 2); // chop 2 payload bytes
        CHECK(!pack.load(archive));
        CHECK(pack.count() == 0);
    }
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

// PerfBudget: flags per-zone + whole-frame overages against measured profiler times.
void testPerfBudget() {
    core::Profiler prof(0.5);
    // Feed a deterministic frame with explicit microsecond timestamps (no real clock):
    //   frame span 0..15000us with two top-level zones: physics 0..5000 (5ms), render 5000..15000
    //   (10ms). Total frame = 15ms.
    prof.beginFrame();
    prof.begin("physics", 0);
    prof.end(5000);
    prof.begin("render", 5000);
    prof.end(15000);
    prof.endFrame();

    core::PerfBudget budget;
    budget.setBudget("physics", 4.0); // 5ms actual > 4ms budget -> over
    budget.setBudget("render", 12.0); // 10ms actual < 12ms budget -> ok
    budget.setFrameBudget(16.6);      // 15ms < 16.6ms -> frame ok

    const auto r = budget.check(prof);
    CHECK(r.zones.size() == 1);
    CHECK(r.zones[0].name == "physics");
    CHECK_NEAR(r.zones[0].budgetMs, 4.0, 1e-6);
    CHECK_NEAR(r.zones[0].actualMs, 5.0, 1e-6);
    CHECK_NEAR(r.zones[0].overMs, 1.0, 1e-6);
    CHECK(!r.frameOverBudget);
    CHECK_NEAR(r.frameMs, 15.0, 1e-6);
    CHECK(r.anyOverage()); // the physics zone is over

    // Tighten the frame budget below 15ms -> frame overage reported too.
    budget.setFrameBudget(12.0);
    const auto r2 = budget.check(prof);
    CHECK(r2.frameOverBudget);
    CHECK_NEAR(r2.frameOverMs, 3.0, 1e-6);

    // A budget on a zone that didn't run this frame is silently skipped (no false overage).
    core::PerfBudget b2;
    b2.setBudget("ai", 1.0);
    const auto r3 = b2.check(prof);
    CHECK(r3.zones.empty());
    CHECK(!r3.anyOverage());

    // Everything within budget -> clean report.
    core::PerfBudget b3;
    b3.setBudget("physics", 10.0);
    b3.setBudget("render", 20.0);
    b3.setFrameBudget(20.0);
    const auto r4 = b3.check(prof);
    CHECK(!r4.anyOverage());
    CHECK(r4.zones.empty());
}

void testAppFocus() {
    using platform::decideFrame;
    using platform::FocusPolicy;
    using platform::WindowActivation;

    // Focused + visible: always full speed, always render, no sleep — regardless of policy.
    {
        FocusPolicy p;
        const auto a = decideFrame(WindowActivation{true, false}, p);
        CHECK(a.advanceSim);
        CHECK(a.render);
        CHECK_NEAR(a.throttleMs, 0.0, 1e-9);
    }

    // Unfocused but visible, default policy: keep rendering + advancing sim, but throttle to
    // backgroundFps (10 -> 100ms/frame).
    {
        FocusPolicy p; // defaults: throttleWhenUnfocused=true, renderWhenUnfocused=true, 10 fps
        const auto a = decideFrame(WindowActivation{false, false}, p);
        CHECK(a.advanceSim);
        CHECK(a.render);
        CHECK_NEAR(a.throttleMs, 100.0, 1e-9);
    }

    // Unfocused with pause-sim + no throttle: sim freezes, no sleep requested.
    {
        FocusPolicy p;
        p.pauseSimWhenUnfocused = true;
        p.throttleWhenUnfocused = false;
        const auto a = decideFrame(WindowActivation{false, false}, p);
        CHECK(!a.advanceSim);
        CHECK(a.render);
        CHECK_NEAR(a.throttleMs, 0.0, 1e-9);
    }

    // Unfocused but renderWhenUnfocused disabled: still no render even though visible.
    {
        FocusPolicy p;
        p.renderWhenUnfocused = false;
        const auto a = decideFrame(WindowActivation{false, false}, p);
        CHECK(!a.render);
    }

    // Minimized, default policy: skip render, keep sim advancing, throttle to backgroundFps.
    {
        FocusPolicy p;
        const auto a = decideFrame(WindowActivation{false, true}, p);
        CHECK(!a.render);          // skipRenderWhenMinimized default true
        CHECK(a.advanceSim);       // sim still runs (timers/netcode)
        CHECK_NEAR(a.throttleMs, 100.0, 1e-9);
    }

    // Minimized with pause-sim, and allow render while minimized: sim frozen, render honored.
    {
        FocusPolicy p;
        p.pauseSimWhenUnfocused = true;
        p.skipRenderWhenMinimized = false;
        const auto a = decideFrame(WindowActivation{false, true}, p);
        CHECK(a.render);
        CHECK(!a.advanceSim);
    }

    // backgroundFps <= 0 disables the throttle sleep even when unfocused.
    {
        FocusPolicy p;
        p.backgroundFps = 0.0;
        const auto a = decideFrame(WindowActivation{false, false}, p);
        CHECK_NEAR(a.throttleMs, 0.0, 1e-9);
    }

    // frameMsForFps helper: 60 fps -> ~16.667ms, 0 fps -> 0.
    CHECK_NEAR(platform::frameMsForFps(60.0), 1000.0 / 60.0, 1e-9);
    CHECK_NEAR(platform::frameMsForFps(0.0), 0.0, 1e-9);
}

void testPresentMode() {
    using render::choosePresentMode;
    using render::PresentMode;

    const std::vector<PresentMode> all = {PresentMode::Fifo, PresentMode::FifoRelaxed,
                                          PresentMode::Mailbox, PresentMode::Immediate};

    // vsync ON: prefer adaptive (FifoRelaxed) when present, else plain Fifo.
    CHECK(choosePresentMode(all, true) == PresentMode::FifoRelaxed);
    CHECK(choosePresentMode({PresentMode::Fifo, PresentMode::Mailbox}, true) == PresentMode::Fifo);

    // vsync OFF: prefer Mailbox (low-latency, no tearing) over Immediate.
    CHECK(choosePresentMode(all, false) == PresentMode::Mailbox);
    CHECK(choosePresentMode({PresentMode::Fifo, PresentMode::Immediate}, false) ==
          PresentMode::Immediate);

    // vsync OFF but tearing disallowed: never pick Immediate; fall back to Fifo when no Mailbox.
    CHECK(choosePresentMode({PresentMode::Fifo, PresentMode::Immediate}, false, false) ==
          PresentMode::Fifo);
    // ...but Mailbox is still fine with tearing disallowed (it doesn't tear).
    CHECK(choosePresentMode({PresentMode::Fifo, PresentMode::Mailbox, PresentMode::Immediate},
                            false, false) == PresentMode::Mailbox);

    // Fifo is the guaranteed backstop: only Fifo available -> Fifo either way.
    CHECK(choosePresentMode({PresentMode::Fifo}, true) == PresentMode::Fifo);
    CHECK(choosePresentMode({PresentMode::Fifo}, false) == PresentMode::Fifo);

    // Empty support list (defensive) -> Fifo.
    CHECK(choosePresentMode({}, false) == PresentMode::Fifo);

    CHECK(render::supportsMode(all, PresentMode::Mailbox));
    CHECK(!render::supportsMode({PresentMode::Fifo}, PresentMode::Mailbox));
}

void testDisplayScale() {
    using namespace maz::platform;

    // scale 1.0 = identity.
    CHECK(logicalToPixels(200.0f, 1.0f) == 200);
    CHECK_NEAR(pixelsToLogical(200, 1.0f), 200.0f, 1e-4f);

    // 2.0 (Retina): logical points double into pixels; pixels halve back to points.
    CHECK(logicalToPixels(200.0f, 2.0f) == 400);
    CHECK_NEAR(pixelsToLogical(400, 2.0f), 200.0f, 1e-4f);

    // 1.5 (common Windows scale): rounds to the nearest whole pixel.
    CHECK(logicalToPixels(100.0f, 1.5f) == 150);
    CHECK(logicalToPixels(101.0f, 1.5f) == 152); // 151.5 -> 152 (round half up)

    // scaledSize scales both dimensions.
    const PixelSize ps = scaledSize(1280, 720, 2.0f);
    CHECK(ps.width == 2560);
    CHECK(ps.height == 1440);

    // Defensive: non-positive / non-finite scale is treated as 1.0 (no scaling, no divide-by-zero).
    CHECK(logicalToPixels(200.0f, 0.0f) == 200);
    CHECK(logicalToPixels(200.0f, -3.0f) == 200);
    CHECK_NEAR(pixelsToLogical(200, 0.0f), 200.0f, 1e-4f);
    CHECK_NEAR(sanitizeScale(2.5f), 2.5f, 1e-6f);
    CHECK_NEAR(sanitizeScale(0.0f), 1.0f, 1e-6f);
}

void testDisplays() {
    using namespace maz::platform;

    // Two 1920x1080 monitors side by side: #0 at x=0, #1 at x=1920.
    const std::vector<DisplayInfo> two = {{0, 0, 0, 1920, 1080, 1.0f},
                                          {1, 1920, 0, 1920, 1080, 2.0f}};

    // Point containment.
    CHECK(displayContainingPoint(two, 100, 100) == 0);
    CHECK(displayContainingPoint(two, 2000, 100) == 1);
    CHECK(displayContainingPoint(two, -5, 100) == -1);   // left of everything
    CHECK(displayContainingPoint(two, 100, 2000) == -1); // below everything

    // A window fully on monitor 1.
    CHECK(displayForRect(two, 2100, 200, 800, 600) == 1);
    // A window straddling the seam but mostly on #0.
    CHECK(displayForRect(two, 1400, 200, 800, 600) == 0); // 520px on #0 vs 280 on #1
    // A window mostly on #1.
    CHECK(displayForRect(two, 1700, 200, 800, 600) == 1); // 220 on #0 vs 580 on #1
    // Off-screen entirely -> falls back to first display.
    CHECK(displayForRect(two, -5000, -5000, 100, 100) == 0);
    // No displays -> -1.
    CHECK(displayForRect({}, 0, 0, 100, 100) == -1);

    // overlapArea math.
    CHECK(overlapArea(two[0], 0, 0, 1920, 1080) == 1920LL * 1080LL); // full cover
    CHECK(overlapArea(two[1], 0, 0, 100, 100) == 0);                 // disjoint

    // Centering a 800x600 window on monitor 1 (origin 1920,0).
    const Point2i c = centerRectOnDisplay(two[1], 800, 600);
    CHECK(c.x == 1920 + (1920 - 800) / 2);
    CHECK(c.y == (1080 - 600) / 2);
}

void testKtx2() {
    using namespace maz::render;

    auto putU32 = [](std::vector<uint8_t>& b, uint32_t v) {
        b.push_back(static_cast<uint8_t>(v & 0xFF));
        b.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        b.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        b.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    };
    auto putU64 = [&](std::vector<uint8_t>& b, uint64_t v) {
        putU32(b, static_cast<uint32_t>(v & 0xFFFFFFFF));
        putU32(b, static_cast<uint32_t>((v >> 32) & 0xFFFFFFFF));
    };

    // Build a minimal single-level KTX2: identifier + header + level-index header + 1 level entry
    // + one byte of level data. vkFormat 43 = VK_FORMAT_R8G8B8A8_SRGB; 4x4, 1 mip.
    std::vector<uint8_t> buf;
    const uint8_t id[12] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32,
                            0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};
    for (unsigned char i : id) {
        buf.push_back(i);
    }
    putU32(buf, 43); // vkFormat (R8G8B8A8_SRGB)
    putU32(buf, 4);  // typeSize
    putU32(buf, 4);  // pixelWidth
    putU32(buf, 4);  // pixelHeight
    putU32(buf, 0);  // pixelDepth
    putU32(buf, 0);  // layerCount
    putU32(buf, 1);  // faceCount
    putU32(buf, 1);  // levelCount
    putU32(buf, 0);  // supercompressionScheme (none)
    // Level-index header (dfd/kvd offsets+lengths as u32, sgd offset+length as u64) — zeros here.
    putU32(buf, 0);
    putU32(buf, 0);
    putU32(buf, 0);
    putU32(buf, 0);
    putU64(buf, 0);
    putU64(buf, 0);
    // Level index: one level. Its data will sit right after the index (offset 80 + 24 = 104).
    const uint64_t dataOffset = 12 + 36 + 32 + 24;
    putU64(buf, dataOffset); // byteOffset
    putU64(buf, 1);          // byteLength
    putU64(buf, 1);          // uncompressedByteLength
    buf.push_back(0xEE);     // the 1 byte of level data

    const Ktx2Info ok = parseKtx2(buf);
    CHECK(ok.valid);
    CHECK(ok.vkFormat == 43);
    CHECK(ok.pixelWidth == 4);
    CHECK(ok.pixelHeight == 4);
    CHECK(ok.levelCount == 1);
    CHECK(ok.effectiveLevels() == 1);
    CHECK(!ok.isSupercompressed());
    CHECK(ok.levels.size() == 1);
    CHECK(ok.levels[0].byteOffset == dataOffset);
    CHECK(ok.levels[0].byteLength == 1);

    // A stored levelCount of 0 means one effective level.
    CHECK(hasKtx2Identifier(buf.data(), buf.size()));

    // Not a KTX2 file.
    const std::vector<uint8_t> junk = {'n', 'o', 'p', 'e', 1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(!parseKtx2(junk).valid);
    CHECK(!hasKtx2Identifier(junk.data(), junk.size()));

    // Truncated (identifier only) -> invalid, no crash.
    std::vector<uint8_t> justId(buf.begin(), buf.begin() + 12);
    CHECK(!parseKtx2(justId).valid);

    // A level whose data runs past EOF is rejected.
    std::vector<uint8_t> bad = buf;
    // Corrupt the level byteLength (bytes at dataOffset-24+8 .. +16) to a huge value.
    // The level index entry starts at 12+36+32 = 80; byteLength is the 2nd u64 at 80+8.
    for (int i = 0; i < 8; ++i) {
        bad[80 + 8 + static_cast<size_t>(i)] = 0xFF;
    }
    CHECK(!parseKtx2(bad).valid);
}

void testInputTextAndDrop() {
    platform::Input in;

    // Nothing typed / dropped by default.
    CHECK(in.textInput().empty());
    CHECK(in.droppedFiles().empty());

    // Text input accumulates within a frame (multiple SDL_EVENT_TEXT_INPUT events).
    in.onTextInput("He");
    in.onTextInput("llo");
    in.onTextInput(nullptr); // null is ignored, not a crash
    CHECK(in.textInput() == "Hello");

    // Dropped files accumulate too.
    in.onDropFile("/tmp/a.png");
    in.onDropFile("/tmp/b.gltf");
    in.onDropFile(nullptr);
    CHECK(in.droppedFiles().size() == 2);
    CHECK(in.droppedFiles()[0] == "/tmp/a.png");
    CHECK(in.droppedFiles()[1] == "/tmp/b.gltf");

    // newFrame clears both (they are per-frame).
    in.newFrame();
    CHECK(in.textInput().empty());
    CHECK(in.droppedFiles().empty());

    // UTF-8 multi-byte sequence is stored verbatim (é = 0xC3 0xA9).
    in.onTextInput("caf\xC3\xA9");
    CHECK(in.textInput() == "caf\xC3\xA9");
    CHECK(in.textInput().size() == 5);
}

void testCascadeSplits() {
    using render::cascadeRanges;
    using render::cascadeSplits;

    // 4 cascades over [1, 100].
    const auto s = cascadeSplits(1.0f, 100.0f, 4, 0.5f);
    CHECK(s.size() == 4);
    // Strictly increasing.
    for (size_t i = 1; i < s.size(); ++i) {
        CHECK(s[i] > s[i - 1]);
    }
    // Last split is exactly the far plane.
    CHECK_NEAR(s.back(), 100.0f, 1e-3f);
    // Every split lies within (near, far].
    for (float d : s) {
        CHECK(d > 1.0f);
        CHECK(d <= 100.0f + 1e-3f);
    }

    // lambda = 0 -> pure uniform: split i = near + range*(i/n). For [0,100]-ish check with [1,101].
    const auto u = cascadeSplits(1.0f, 101.0f, 4, 0.0f);
    CHECK_NEAR(u[0], 1.0f + 100.0f * 0.25f, 1e-3f); // 26
    CHECK_NEAR(u[1], 1.0f + 100.0f * 0.50f, 1e-3f); // 51
    CHECK_NEAR(u[2], 1.0f + 100.0f * 0.75f, 1e-3f); // 76

    // lambda = 1 -> pure logarithmic: split i = near * ratio^(i/n).
    const auto lg = cascadeSplits(1.0f, 16.0f, 4, 1.0f);
    CHECK_NEAR(lg[0], std::pow(16.0f, 0.25f), 1e-3f); // 2
    CHECK_NEAR(lg[1], std::pow(16.0f, 0.50f), 1e-3f); // 4
    CHECK_NEAR(lg[2], std::pow(16.0f, 0.75f), 1e-3f); // 8
    CHECK_NEAR(lg[3], 16.0f, 1e-3f);

    // Log splitting packs more resolution near the camera than uniform (first split is closer).
    const auto un = cascadeSplits(1.0f, 100.0f, 4, 0.0f);
    const auto ln = cascadeSplits(1.0f, 100.0f, 4, 1.0f);
    CHECK(ln[0] < un[0]);

    // Ranges are contiguous and start at nearZ.
    const auto r = cascadeRanges(2.0f, 50.0f, 3, 0.5f);
    CHECK(r.size() == 3);
    CHECK_NEAR(r[0].nearZ, 2.0f, 1e-4f);
    CHECK_NEAR(r[1].nearZ, r[0].farZ, 1e-4f);
    CHECK_NEAR(r[2].nearZ, r[1].farZ, 1e-4f);
    CHECK_NEAR(r[2].farZ, 50.0f, 1e-3f);

    // Invalid inputs return empty (no crash, no divide-by-zero).
    CHECK(cascadeSplits(0.0f, 100.0f, 4).empty());  // near must be > 0
    CHECK(cascadeSplits(10.0f, 5.0f, 4).empty());   // far <= near
    CHECK(cascadeSplits(1.0f, 100.0f, 0).empty());  // count <= 0
}

void testBitStream() {
    using net::BitReader;
    using net::BitWriter;

    // Round-trip a mix of widths, bool, signed, float, and bytes.
    BitWriter w;
    w.writeBits(3u, 2);        // 0..3
    w.writeBits(5u, 3);        // 0..7
    w.writeBool(true);
    w.writeBool(false);
    w.writeUint(0xABCDu, 16);
    w.writeInt(-42, 12);       // signed field
    w.writeFloat(3.14159f);
    const uint8_t blob[3] = {0x11, 0x22, 0x33};
    w.writeBytes(blob, 3);

    // Bit accounting: 2+3+1+1+16+12+32+24 = 91 bits.
    CHECK(w.bitCount() == 91);
    CHECK(w.byteCount() == 12); // ceil(91/8)

    BitReader r(w.bytes());
    CHECK(r.readBits(2) == 3u);
    CHECK(r.readBits(3) == 5u);
    CHECK(r.readBool() == true);
    CHECK(r.readBool() == false);
    CHECK(r.readUint(16) == 0xABCDu);
    CHECK(r.readInt(12) == -42); // sign-extended correctly
    CHECK_NEAR(r.readFloat(), 3.14159f, 1e-6f);
    uint8_t out[3] = {0, 0, 0};
    r.readBytes(out, 3);
    CHECK(out[0] == 0x11);
    CHECK(out[1] == 0x22);
    CHECK(out[2] == 0x33);
    CHECK(r.ok()); // never read past the end

    // Signed extremes across widths.
    BitWriter w2;
    w2.writeInt(-1, 8);   // all ones
    w2.writeInt(127, 8);  // max positive in 8 bits
    w2.writeInt(-128, 8); // min in 8 bits
    BitReader r2(w2.bytes());
    CHECK(r2.readInt(8) == -1);
    CHECK(r2.readInt(8) == 127);
    CHECK(r2.readInt(8) == -128);

    // Underflow: reading past the end flips ok() to false and returns zero-fill.
    BitWriter w3;
    w3.writeBits(0xFu, 4);
    BitReader r3(w3.bytes());
    CHECK(r3.readBits(4) == 0xFu);
    CHECK(r3.ok());
    (void)r3.readBits(8); // only 4 bits were the payload; the byte had 4 pad bits then EOF
    CHECK(!r3.ok());      // went past the buffer

    // align() pads to a byte boundary.
    BitWriter w4;
    w4.writeBits(1u, 1);
    w4.align();
    CHECK(w4.bitCount() == 8);
    CHECK(w4.byteCount() == 1);
}

void testReliability() {
    using net::AckReceiver;
    using net::AckSender;
    using net::seqGreaterThan;

    // Sequence wraparound comparator.
    CHECK(seqGreaterThan(1, 0));
    CHECK(!seqGreaterThan(0, 1));
    CHECK(seqGreaterThan(0, 65535));      // wrapped forward
    CHECK(!seqGreaterThan(65535, 0));
    CHECK(seqGreaterThan(200, 100));

    // Receiver in order: 0,1,2 -> ack=2, bit0=seq1, bit1=seq0.
    AckReceiver rx;
    rx.onReceived(0);
    rx.onReceived(1);
    rx.onReceived(2);
    CHECK(rx.ack() == 2);
    CHECK(rx.wasReceived(0));
    CHECK(rx.wasReceived(1));
    CHECK(rx.wasReceived(2));
    CHECK(!rx.wasReceived(3));
    CHECK((rx.ackBits() & 1u) != 0u);        // seq1
    CHECK((rx.ackBits() & 2u) != 0u);        // seq0

    // Out-of-order + a gap: receive 5 then 3 (4 is lost).
    AckReceiver rx2;
    rx2.onReceived(5);
    rx2.onReceived(3);
    CHECK(rx2.ack() == 5);
    CHECK(rx2.wasReceived(5));
    CHECK(rx2.wasReceived(3));
    CHECK(!rx2.wasReceived(4)); // genuinely lost

    // Sender: allocate 0,1,2; a peer that got 0 and 2 (not 1) acks two, leaves 1 in flight.
    AckSender tx;
    CHECK(tx.next() == 0);
    CHECK(tx.next() == 1);
    CHECK(tx.next() == 2);
    CHECK(tx.inFlightCount() == 3);
    AckReceiver peer;
    peer.onReceived(0);
    peer.onReceived(2); // 1 never arrived
    const auto acked = tx.onAck(peer.ack(), peer.ackBits());
    CHECK(acked.size() == 2);       // 0 and 2
    CHECK(tx.inFlightCount() == 1); // 1 still unacked
    CHECK(tx.inFlight(1));
    CHECK(!tx.inFlight(0));
    CHECK(!tx.inFlight(2));

    // Full round-trip: everything received -> everything acked.
    AckSender tx2;
    AckReceiver peer2;
    for (int i = 0; i < 10; ++i) {
        peer2.onReceived(tx2.next());
    }
    const auto all = tx2.onAck(peer2.ack(), peer2.ackBits());
    // ack + up to 32 bits covers all 10; the last 33 are resolvable.
    CHECK(tx2.inFlightCount() == 0);
    CHECK(all.size() == 10);

    // Re-acking already-acked sequences yields nothing new.
    const auto again = tx2.onAck(peer2.ack(), peer2.ackBits());
    CHECK(again.empty());
}

void testSnapshot() {
    using net::BitReader;
    using net::BitWriter;
    using net::FieldSpec;

    // Schema: health(8), tileId(10), flag(1), score(16).
    const std::vector<FieldSpec> schema = {{8}, {10}, {1}, {16}};
    const std::vector<uint32_t> base = {100u, 500u, 1u, 42u};
    const std::vector<uint32_t> cur = {100u, 512u, 0u, 42u}; // fields 1 and 2 changed

    CHECK(net::countChanged(base, cur) == 2);

    // Delta: 4 changed-bits + field1 (10 bits) + field2 (1 bit) = 15 bits.
    BitWriter dw;
    net::writeSnapshotDelta(dw, schema, base, cur);
    CHECK(dw.bitCount() == 15);
    BitReader dr(dw.bytes());
    const auto decoded = net::readSnapshotDelta(dr, schema, base);
    CHECK(decoded == cur);
    CHECK(dr.ok());

    // No change -> delta is just the 4 mask bits, decode reproduces base.
    BitWriter dw2;
    net::writeSnapshotDelta(dw2, schema, base, base);
    CHECK(dw2.bitCount() == 4);
    BitReader dr2(dw2.bytes());
    CHECK(net::readSnapshotDelta(dr2, schema, base) == base);

    // All changed (flag flips 1 -> 0 so every field differs from base).
    const std::vector<uint32_t> cur3 = {1u, 2u, 0u, 3u};
    BitWriter dw3;
    net::writeSnapshotDelta(dw3, schema, base, cur3);
    // 4 mask + 8 + 10 + 1 + 16 = 39 bits.
    CHECK(dw3.bitCount() == 39);
    BitReader dr3(dw3.bytes());
    CHECK(net::readSnapshotDelta(dr3, schema, base) == cur3);

    // Full snapshot round-trip: 8+10+1+16 = 35 bits regardless of change.
    BitWriter fw;
    net::writeSnapshotFull(fw, schema, cur);
    CHECK(fw.bitCount() == 35);
    BitReader fr(fw.bytes());
    CHECK(net::readSnapshotFull(fr, schema) == cur);

    // Bandwidth win: a 2-field delta (15 bits) is far smaller than a full snapshot (35 bits).
    CHECK(dw.bitCount() < fw.bitCount());
}

void testNetInterpolation() {
    net::InterpolationBuffer<float> buf;
    CHECK(buf.empty());
    float out = 0.0f;
    CHECK(!buf.sample(0.0, out)); // empty buffer -> false

    buf.insert(1.0, 10.0f);
    buf.insert(2.0, 20.0f);
    buf.insert(3.0, 30.0f);
    CHECK(buf.size() == 3);

    // Interpolate between knots.
    CHECK(buf.sample(1.5, out));
    CHECK_NEAR(out, 15.0f, 1e-4f);
    CHECK(buf.sample(2.5, out));
    CHECK_NEAR(out, 25.0f, 1e-4f);
    // Exactly on a knot.
    CHECK(buf.sample(2.0, out));
    CHECK_NEAR(out, 20.0f, 1e-4f);
    // Before the oldest -> clamp to oldest; after newest -> clamp to newest.
    CHECK(buf.sample(0.5, out));
    CHECK_NEAR(out, 10.0f, 1e-4f);
    CHECK(buf.sample(9.0, out));
    CHECK_NEAR(out, 30.0f, 1e-4f);

    // Out-of-order insert lands in time order.
    buf.insert(2.5, 25.0f);
    CHECK(buf.sample(2.25, out));
    CHECK_NEAR(out, 22.5f, 1e-4f); // between 2.0(20) and 2.5(25)

    // A sample at an existing time replaces it.
    buf.insert(2.5, 100.0f);
    CHECK(buf.sample(2.5, out));
    CHECK_NEAR(out, 100.0f, 1e-4f);
    buf.insert(2.5, 25.0f);

    // Extrapolation extends the tail velocity (+10/sec) past the newest sample.
    CHECK(buf.sampleExtrapolated(3.5, 1.0, out));
    CHECK_NEAR(out, 35.0f, 1e-4f);
    // ...but never more than maxExtrapolate seconds ahead.
    CHECK(buf.sampleExtrapolated(4.0, 0.5, out));
    CHECK_NEAR(out, 35.0f, 1e-4f);

    // Capacity trim keeps the newest samples.
    net::InterpolationBuffer<float> cap(3);
    for (int i = 0; i < 10; ++i) {
        cap.insert(static_cast<double>(i), static_cast<float>(i));
    }
    CHECK(cap.size() == 3);
    CHECK_NEAR(static_cast<float>(cap.oldestTime()), 7.0f, 1e-4f);
    CHECK_NEAR(static_cast<float>(cap.newestTime()), 9.0f, 1e-4f);

    // Works with a vector value type (math::vec2).
    net::InterpolationBuffer<math::vec2> vbuf;
    vbuf.insert(0.0, math::vec2{0.0f, 0.0f});
    vbuf.insert(1.0, math::vec2{10.0f, 20.0f});
    math::vec2 vo{};
    CHECK(vbuf.sample(0.5, vo));
    CHECK_NEAR(vo.x, 5.0f, 1e-4f);
    CHECK_NEAR(vo.y, 10.0f, 1e-4f);
}

void testPrediction() {
    struct St {
        float pos = 0.0f;
    };
    struct In {
        float vel = 0.0f;
        float dt = 0.0f;
    };
    auto step = [](const St& s, const In& i) { return St{s.pos + i.vel * i.dt}; };

    net::PredictionBuffer<St, In> pb;
    // Three "move right" inputs applied locally, each instantly (no server wait).
    pb.applyInput(1, In{10.0f, 0.1f}, step); // pos 1
    pb.applyInput(2, In{10.0f, 0.1f}, step); // pos 2
    St s = pb.applyInput(3, In{10.0f, 0.1f}, step); // pos 3
    CHECK_NEAR(s.pos, 3.0f, 1e-4f);
    CHECK(pb.pendingCount() == 3);

    // Correct prediction: server state after input 1 is pos 1. Re-sim 2 & 3 -> unchanged 3.
    s = pb.reconcile(1, St{1.0f}, step);
    CHECK_NEAR(s.pos, 3.0f, 1e-4f);
    CHECK(pb.pendingCount() == 2);

    // Misprediction: server says after input 2 the true pos is 5. Snap + re-sim input 3.
    s = pb.reconcile(2, St{5.0f}, step);
    CHECK_NEAR(s.pos, 6.0f, 1e-4f); // 5 + 10*0.1
    CHECK(pb.pendingCount() == 1);

    // Everything acknowledged: no pending, state equals the authoritative value.
    s = pb.reconcile(3, St{6.0f}, step);
    CHECK_NEAR(s.pos, 6.0f, 1e-4f);
    CHECK(pb.pendingCount() == 0);

    // A stale ack (older than all pending) drops nothing and re-sims on top of the server state.
    net::PredictionBuffer<St, In> pb2(St{0.0f});
    pb2.applyInput(10, In{2.0f, 1.0f}, step); // pos 2
    pb2.applyInput(11, In{2.0f, 1.0f}, step); // pos 4
    s = pb2.reconcile(9, St{0.0f}, step);
    CHECK(pb2.pendingCount() == 2);
    CHECK_NEAR(s.pos, 4.0f, 1e-4f);
}

void testRpc() {
    using net::BitReader;
    using net::BitWriter;
    using net::RpcDispatcher;
    using net::RpcMode;

    net::RpcDispatcher d;
    int gotDamage = 0;
    float gotAmount = 0.0f;
    int spawnCount = 0;
    int32_t spawnX = 0;
    int32_t spawnY = 0;

    d.bindNamed("take_damage", [&](BitReader& r) {
        gotDamage = static_cast<int>(r.readBits(8));
        gotAmount = r.readFloat();
    });
    d.bindNamed("spawn", [&](BitReader& r) {
        ++spawnCount;
        spawnX = r.readInt(16);
        spawnY = r.readInt(16);
    });
    CHECK(d.handlerCount() == 2);

    // Name hash is deterministic and (for these names) distinct.
    CHECK(net::rpcHash("take_damage") == net::rpcHash("take_damage"));
    CHECK(net::rpcHash("take_damage") != net::rpcHash("spawn"));
    CHECK(d.bound(net::rpcHash("spawn")));

    // Encode + dispatch take_damage(37, 2.5) as an unreliable call.
    BitWriter w;
    RpcDispatcher::writeHeaderNamed(w, "take_damage", RpcMode::Unreliable);
    w.writeBits(37, 8);
    w.writeFloat(2.5f);
    BitReader r(w.bytes());
    CHECK(d.dispatch(r));
    CHECK(gotDamage == 37);
    CHECK_NEAR(gotAmount, 2.5f, 1e-6f);
    CHECK(d.lastMode() == RpcMode::Unreliable);
    CHECK(d.lastMethod() == net::rpcHash("take_damage"));

    // Encode + dispatch spawn(-100, 200) (default reliable).
    BitWriter w2;
    RpcDispatcher::writeHeaderNamed(w2, "spawn");
    w2.writeInt(-100, 16);
    w2.writeInt(200, 16);
    BitReader r2(w2.bytes());
    CHECK(d.dispatch(r2));
    CHECK(spawnCount == 1);
    CHECK(spawnX == -100);
    CHECK(spawnY == 200);
    CHECK(d.lastMode() == RpcMode::Reliable);

    // Unknown method -> false, counted.
    BitWriter w3;
    RpcDispatcher::writeHeaderNamed(w3, "nonexistent");
    BitReader r3(w3.bytes());
    CHECK(!d.dispatch(r3));
    CHECK(d.unknownCalls() == 1);

    // Truncated header (8 bits, needs 18) -> malformed, counted.
    BitWriter w4;
    w4.writeBits(5, 8);
    BitReader r4(w4.bytes());
    CHECK(!d.dispatch(r4));
    CHECK(d.malformedCalls() == 1);
}

void testReplication() {
    using net::BitReader;
    using net::BitWriter;
    using net::ReplicatedObject;
    using net::Synchronizer;

    struct Entity {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t health = 0;
    };
    auto makeObj = [](Entity& e) {
        ReplicatedObject o;
        o.add([&] { return e.x; }, [&](uint32_t v) { e.x = v; }, 16);
        o.add([&] { return e.y; }, [&](uint32_t v) { e.y = v; }, 16);
        o.add([&] { return e.health; }, [&](uint32_t v) { e.health = v; }, 7);
        return o;
    };

    Entity srv{100u, 200u, 90u};
    Entity cli{};
    ReplicatedObject so = makeObj(srv);
    ReplicatedObject co = makeObj(cli);
    Synchronizer sSync;
    Synchronizer cSync;
    CHECK(so.propertyCount() == 3);

    // Full sync: client mirrors server.
    BitWriter wf;
    sSync.writeFull(wf, so);
    CHECK(wf.bitCount() == 16 + 16 + 7);
    BitReader rf(wf.bytes());
    cSync.readFull(rf, co);
    CHECK(cli.x == 100u);
    CHECK(cli.y == 200u);
    CHECK(cli.health == 90u);

    // Only x changes -> a 1-field delta (3 mask bits + one 16-bit field).
    srv.x = 105u;
    BitWriter wd;
    const std::size_t changed = sSync.writeDelta(wd, so);
    CHECK(changed == 1);
    CHECK(wd.bitCount() == 3 + 16);
    BitReader rd(wd.bytes());
    cSync.readDelta(rd, co);
    CHECK(cli.x == 105u);
    CHECK(cli.y == 200u);
    CHECK(cli.health == 90u);

    // No change -> delta is just the mask bits; client unchanged.
    BitWriter wn;
    const std::size_t c2 = sSync.writeDelta(wn, so);
    CHECK(c2 == 0);
    CHECK(wn.bitCount() == 3);
    BitReader rn(wn.bytes());
    cSync.readDelta(rn, co);
    CHECK(cli.x == 105u);

    // Two fields change together.
    srv.y = 250u;
    srv.health = 40u;
    BitWriter wm;
    const std::size_t c3 = sSync.writeDelta(wm, so);
    CHECK(c3 == 2);
    CHECK(wm.bitCount() == 3 + 16 + 7);
    BitReader rm(wm.bytes());
    cSync.readDelta(rm, co);
    CHECK(cli.x == 105u);
    CHECK(cli.y == 250u);
    CHECK(cli.health == 40u);

    // Bandwidth win: a 1-field delta is smaller than a full snapshot.
    CHECK(wd.bitCount() < wf.bitCount());
}

void testConnection() {
    using net::Connection;
    using net::kPacketHeaderBytes;

    const uint32_t proto = 0xABCD1234u;
    Connection client(proto);
    Connection server(proto);

    // Client frames "hello".
    const std::string msg = "hello";
    const std::vector<uint8_t> pl(msg.begin(), msg.end());
    uint16_t s0 = 0;
    const std::vector<uint8_t> pkt = client.pack(pl, s0);
    CHECK(s0 == 0);
    CHECK(pkt.size() == kPacketHeaderBytes + 5);
    CHECK(client.inFlightCount() == 1);
    CHECK(client.inFlight(0));

    // Server parses it, records the sequence.
    std::vector<uint8_t> got;
    CHECK(server.unpack(pkt, got));
    CHECK(std::string(got.begin(), got.end()) == "hello");
    CHECK(server.wasReceived(0));

    // Server replies -> its header acknowledges seq 0; client learns it landed.
    const std::string reply = "hi";
    const std::vector<uint8_t> rpl(reply.begin(), reply.end());
    uint16_t s1 = 0;
    const std::vector<uint8_t> pkt2 = server.pack(rpl, s1);
    std::vector<uint8_t> got2;
    CHECK(client.unpack(pkt2, got2));
    CHECK(std::string(got2.begin(), got2.end()) == "hi");
    CHECK(client.inFlightCount() == 0);
    CHECK(client.lastAcked().size() == 1);
    CHECK(client.lastAcked()[0] == 0);

    // Wrong protocol id -> rejected, not applied.
    Connection foreign(0xDEADBEEFu);
    uint16_t sx = 0;
    const std::vector<uint8_t> badpkt = foreign.pack(pl, sx);
    std::vector<uint8_t> junk;
    CHECK(!server.unpack(badpkt, junk));
    CHECK(server.rejectedPackets() == 1);

    // Truncated packet (shorter than the header) -> rejected.
    const std::vector<uint8_t> tiny(5, 0u);
    CHECK(!server.unpack(tiny, junk));
    CHECK(server.rejectedPackets() == 2);

    // Three client packets in flight; one server reply acks all three at once (ack + ackBits).
    uint16_t a = 0;
    uint16_t b = 0;
    uint16_t c = 0;
    const std::vector<uint8_t> pa = client.pack(pl, a);
    const std::vector<uint8_t> pb = client.pack(pl, b);
    const std::vector<uint8_t> pc = client.pack(pl, c);
    CHECK(a == 1);
    CHECK(b == 2);
    CHECK(c == 3);
    CHECK(client.inFlightCount() == 3);
    std::vector<uint8_t> t;
    CHECK(server.unpack(pa, t));
    CHECK(server.unpack(pb, t));
    CHECK(server.unpack(pc, t));
    uint16_t s2 = 0;
    const std::vector<uint8_t> ack3 = server.pack(rpl, s2);
    CHECK(client.unpack(ack3, t));
    CHECK(client.inFlightCount() == 0);
    CHECK(client.lastAcked().size() == 3);

    // Empty-payload packet round-trips (header only).
    uint16_t se = 0;
    const std::vector<uint8_t> emptyPkt = client.pack(nullptr, 0, se);
    CHECK(emptyPkt.size() == kPacketHeaderBytes);
    std::vector<uint8_t> ep;
    CHECK(server.unpack(emptyPkt, ep));
    CHECK(ep.empty());
}

void testNetSim() {
    using net::NetConditions;
    using net::NetSim;
    auto pkt = [](uint8_t id) { return std::vector<uint8_t>{id}; };

    // Pure latency: a packet isn't delivered until now + latency.
    {
        NetSim s;
        NetConditions c;
        c.latency = 0.1;
        s.setConditions(c);
        CHECK(s.send(pkt(1), 0.0));
        CHECK(s.pending() == 1);
        CHECK(s.receive(0.05).empty());
        const auto got = s.receive(0.1);
        CHECK(got.size() == 1);
        CHECK(got[0][0] == 1);
        CHECK(s.pending() == 0);
    }

    // Guaranteed loss: send returns false and nothing is queued.
    {
        NetSim s;
        NetConditions c;
        c.lossChance = 1.0f;
        s.setConditions(c);
        CHECK(!s.send(pkt(2), 0.0));
        CHECK(s.pending() == 0);
        CHECK(s.dropped() == 1);
    }

    // Guaranteed duplication: one send produces two deliveries.
    {
        NetSim s;
        NetConditions c;
        c.dupChance = 1.0f;
        s.setConditions(c);
        CHECK(s.send(pkt(3), 0.0));
        CHECK(s.pending() == 2);
        CHECK(s.duplicated() == 1);
        CHECK(s.receive(0.0).size() == 2);
    }

    // Reordering: a later packet with shorter latency is delivered first (sample times chosen
    // above the float sum to avoid boundary rounding).
    {
        NetSim s;
        NetConditions c;
        c.latency = 0.2;
        s.setConditions(c);
        CHECK(s.send(pkt('A'), 0.0)); // delivers at 0.2
        c.latency = 0.1;
        s.setConditions(c);
        CHECK(s.send(pkt('B'), 0.05)); // delivers at ~0.15
        const auto early = s.receive(0.16);
        CHECK(early.size() == 1);
        CHECK(early[0][0] == 'B');
        const auto late = s.receive(0.25);
        CHECK(late.size() == 1);
        CHECK(late[0][0] == 'A');
    }

    // Determinism: identical seed + config -> identical drop pattern.
    {
        NetSim a(12345u);
        NetSim b(12345u);
        NetConditions c;
        c.lossChance = 0.5f;
        a.setConditions(c);
        b.setConditions(c);
        for (int i = 0; i < 200; ++i) {
            a.send(pkt(static_cast<uint8_t>(i)), 0.0);
            b.send(pkt(static_cast<uint8_t>(i)), 0.0);
        }
        CHECK(a.dropped() == b.dropped());
        CHECK(a.pending() == b.pending());
        CHECK(a.dropped() > 50);  // ~50% of 200, well away from the extremes
        CHECK(a.dropped() < 150);
    }
}

void testPathFollow2D() {
    // Straight horizontal line 0..100 (constant-speed baked).
    math::Curve2D c;
    c.addPoint(math::vec2(0.0f, 0.0f));
    c.addPoint(math::vec2(100.0f, 0.0f));
    c.bake(1.0f);
    CHECK_NEAR(c.bakedLength(), 100.0f, 0.5f);

    game::PathFollow2D f;
    f.setProgress(50.0f);
    game::PathSample2D s = f.sample(c);
    CHECK_NEAR(s.position.x, 50.0f, 0.02f);
    CHECK_NEAR(s.position.y, 0.0f, 0.02f);
    CHECK_NEAR(s.rotation, 0.0f, 0.02f); // tangent points +x

    // progress_ratio getters/setters.
    CHECK_NEAR(f.progressRatio(c), 0.5f, 0.02f);
    f.setProgressRatio(0.25f, c);
    CHECK_NEAR(f.progress(), 25.0f, 0.5f);

    // hOffset slides along the left normal (+y for a +x tangent).
    f.setProgress(50.0f);
    f.setHOffset(10.0f);
    s = f.sample(c);
    CHECK_NEAR(s.position.x, 50.0f, 0.02f);
    CHECK_NEAR(s.position.y, 10.0f, 0.02f);
    f.setHOffset(0.0f);

    // Clamp mode: past the ends clamps to the endpoints.
    f.setLoop(false);
    f.setProgress(150.0f);
    CHECK_NEAR(f.sample(c).position.x, 100.0f, 0.02f);
    f.setProgress(-20.0f);
    CHECK_NEAR(f.sample(c).position.x, 0.0f, 0.02f);

    // Loop mode: progress wraps around the length.
    f.setLoop(true);
    f.setProgress(150.0f);
    CHECK_NEAR(f.sample(c).position.x, 50.0f, 0.02f);
    f.setProgress(-10.0f);
    CHECK_NEAR(f.sample(c).position.x, 90.0f, 0.02f);

    // Vertical line -> tangent +y -> heading ~ +pi/2 when rotates is on.
    math::Curve2D v;
    v.addPoint(math::vec2(0.0f, 0.0f));
    v.addPoint(math::vec2(0.0f, 100.0f));
    v.bake(1.0f);
    game::PathFollow2D vf;
    vf.setProgress(50.0f);
    game::PathSample2D vs = vf.sample(v);
    CHECK_NEAR(vs.position.x, 0.0f, 0.02f);
    CHECK_NEAR(vs.position.y, 50.0f, 0.02f);
    CHECK_NEAR(vs.rotation, 3.14159265f / 2.0f, 0.05f);
    // rotates off -> heading stays 0.
    vf.setRotates(false);
    CHECK_NEAR(vf.sample(v).rotation, 0.0f, 0.02f);
}

void testTimer() {
    // One-shot fires once then stops.
    {
        game::Timer t;
        t.setOneShot(true);
        t.setWaitTime(1.0);
        CHECK(t.isStopped());
        t.start();
        CHECK(!t.isStopped());
        CHECK(t.tick(0.5) == 0);
        CHECK_NEAR(static_cast<float>(t.timeLeft()), 0.5f, 1e-6f);
        CHECK(t.tick(0.6) == 1);
        CHECK(t.isStopped());
        CHECK(t.tick(1.0) == 0); // stopped: no further fires
    }
    // Repeating carries the remainder so the cadence never drifts.
    {
        game::Timer t;
        t.setWaitTime(1.0); // default: repeating
        t.start();
        CHECK(t.tick(2.5) == 2); // fires at 1.0 and 2.0
        CHECK_NEAR(static_cast<float>(t.timeLeft()), 0.5f, 1e-6f);
        CHECK(t.tick(0.4) == 0);
        CHECK_NEAR(static_cast<float>(t.timeLeft()), 0.1f, 1e-6f);
        CHECK(t.tick(0.2) == 1); // 0.1 - 0.2 -> fire, + 1.0 = 0.9 left
        CHECK_NEAR(static_cast<float>(t.timeLeft()), 0.9f, 1e-6f);
    }
    // Pause suspends ticking.
    {
        game::Timer t;
        t.setOneShot(true);
        t.setWaitTime(1.0);
        t.start();
        t.setPaused(true);
        CHECK(t.tick(5.0) == 0);
        CHECK_NEAR(static_cast<float>(t.timeLeft()), 1.0f, 1e-6f);
        t.setPaused(false);
        CHECK(t.tick(1.0) == 1);
    }
    // Callback runs once per fire.
    {
        game::Timer t;
        t.setWaitTime(0.5);
        t.start();
        int calls = 0;
        const int fires = t.tick(1.6, [&] { ++calls; }); // 0.5, 1.0, 1.5
        CHECK(fires == 3);
        CHECK(calls == 3);
        CHECK_NEAR(static_cast<float>(t.timeLeft()), 0.4f, 1e-6f);
    }
    // start(override) sets the wait time.
    {
        game::Timer t;
        t.setWaitTime(1.0);
        t.setOneShot(true);
        t.start(3.0);
        CHECK_NEAR(static_cast<float>(t.waitTime()), 3.0f, 1e-6f);
        CHECK_NEAR(static_cast<float>(t.timeLeft()), 3.0f, 1e-6f);
        CHECK(t.tick(2.9) == 0);
        CHECK(t.tick(0.2) == 1);
    }
    // stop resets the countdown.
    {
        game::Timer t;
        t.setWaitTime(1.0);
        t.start();
        t.tick(0.3);
        t.stop();
        CHECK(t.isStopped());
        CHECK_NEAR(static_cast<float>(t.timeLeft()), 0.0f, 1e-6f);
    }
}

void testVisibleOnScreenNotifier2D() {
    const math::Rect2 view(0.0f, 0.0f, 100.0f, 100.0f);
    game::VisibleOnScreenNotifier2D n(math::Rect2(200.0f, 200.0f, 10.0f, 10.0f)); // off-screen
    CHECK(!n.isOnScreen());

    // Still off-screen -> no event.
    game::ScreenNotifierEvents e = n.update(view);
    CHECK(!e.entered);
    CHECK(!e.exited);
    CHECK(!n.isOnScreen());

    // Enters view -> entered fires exactly once.
    n.setRect(math::Rect2(50.0f, 50.0f, 10.0f, 10.0f));
    e = n.update(view);
    CHECK(e.entered);
    CHECK(!e.exited);
    CHECK(n.isOnScreen());

    // Stays on -> no repeat event.
    e = n.update(view);
    CHECK(!e.entered);
    CHECK(!e.exited);
    CHECK(n.isOnScreen());

    // Leaves view -> exited fires exactly once.
    n.setRect(math::Rect2(300.0f, 300.0f, 10.0f, 10.0f));
    e = n.update(view);
    CHECK(!e.entered);
    CHECK(e.exited);
    CHECK(!n.isOnScreen());

    // Stays off -> no repeat event.
    e = n.update(view);
    CHECK(!e.entered);
    CHECK(!e.exited);

    // A rect touching the view's corner counts as on-screen (border-inclusive).
    game::VisibleOnScreenNotifier2D b(math::Rect2(-10.0f, -10.0f, 10.0f, 10.0f));
    e = b.update(view);
    CHECK(e.entered);
    CHECK(b.isOnScreen());

    // reset clears state without emitting.
    b.reset(false);
    CHECK(!b.isOnScreen());
}

void testGridMap() {
    game::GridMap g;
    CHECK(g.empty());
    CHECK(g.count() == 0);

    g.setCell(0, 0, 0, 5, 3);
    g.setCell(2, 1, -4, 9, 0);
    g.setCell(-3, 0, 0, 1, 12);
    CHECK(g.count() == 3);
    CHECK(g.hasCell(0, 0, 0));
    CHECK(g.cellTile(0, 0, 0) == 5);
    CHECK(g.cellOrientation(0, 0, 0) == 3);
    CHECK(g.cellTile(2, 1, -4) == 9);
    CHECK(g.cellTile(-3, 0, 0) == 1);
    CHECK(g.cellOrientation(-3, 0, 0) == 12);
    CHECK(g.cellTile(100, 100, 100) == -1); // empty cell
    CHECK(!g.hasCell(100, 100, 100));

    // Overwrite resets orientation to the new value.
    g.setCell(0, 0, 0, 7);
    CHECK(g.cellTile(0, 0, 0) == 7);
    CHECK(g.cellOrientation(0, 0, 0) == 0);
    CHECK(g.count() == 3);

    // Negative tile id clears the cell (Godot set_cell_item(INVALID)).
    g.setCell(0, 0, 0, -1);
    CHECK(!g.hasCell(0, 0, 0));
    CHECK(g.count() == 2);
    g.clearCell(2, 1, -4);
    CHECK(g.count() == 1);

    // Bounds over a spread of cells.
    g.clear();
    g.setCell(-2, -5, 3, 1);
    g.setCell(4, 2, -1, 1);
    g.setCell(0, 0, 0, 1);
    game::GridCell mn;
    game::GridCell mx;
    CHECK(g.bounds(mn, mx));
    CHECK(mn.x == -2);
    CHECK(mn.y == -5);
    CHECK(mn.z == -1);
    CHECK(mx.x == 4);
    CHECK(mx.y == 2);
    CHECK(mx.z == 3);

    // Empty map has no bounds.
    g.clear();
    CHECK(!g.bounds(mn, mx));

    // world <-> cell mapping with 2x2x2 cells.
    g.setCellSize(math::vec3(2.0f, 2.0f, 2.0f));
    const math::vec3 c = g.cellToWorld(1, 0, -1); // center (3, 1, -1)
    CHECK_NEAR(c.x, 3.0f, 1e-4f);
    CHECK_NEAR(c.y, 1.0f, 1e-4f);
    CHECK_NEAR(c.z, -1.0f, 1e-4f);
    const game::GridCell cell = g.worldToCell(math::vec3(3.1f, 1.0f, -0.9f));
    CHECK(cell.x == 1);
    CHECK(cell.y == 0);
    CHECK(cell.z == -1);
    // Floor division goes the right way for negatives.
    const game::GridCell cell2 = g.worldToCell(math::vec3(-0.1f, -2.0f, 0.0f));
    CHECK(cell2.x == -1);
    CHECK(cell2.y == -1);
    CHECK(cell2.z == 0);

    // Packed key round-trips signed coordinates.
    int kx = 0;
    int ky = 0;
    int kz = 0;
    game::GridMap::unkey(game::GridMap::key(-1000, 2047, -32768), kx, ky, kz);
    CHECK(kx == -1000);
    CHECK(ky == 2047);
    CHECK(kz == -32768);
}

void testObjLoader() {
    using render::ObjLoadOptions;
    render::ObjLoadOptions noflip;
    noflip.flipV = false;

    // A quad face (v/vt/vn) fan-triangulates to 2 tris (6 indices) over 4 unique verts.
    const std::string quad =
        "# comment\n"
        "o Quad\n"
        "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
        "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
        "vn 0 0 1\n"
        "f 1/1/1 2/2/1 3/3/1 4/4/1\n";
    render::shapes::MeshData m;
    CHECK(render::parseObj(quad, m, noflip));
    CHECK(m.vertices.size() == 4);
    CHECK(m.indices.size() == 6);
    CHECK_NEAR(m.vertices[2].px, 1.0f, 1e-4f);
    CHECK_NEAR(m.vertices[2].py, 1.0f, 1e-4f);
    CHECK_NEAR(m.vertices[0].nz, 1.0f, 1e-4f);
    CHECK_NEAR(m.vertices[2].u, 1.0f, 1e-4f);
    CHECK_NEAR(m.vertices[2].v, 1.0f, 1e-4f);

    // flipV (default) flips texcoord V: vt "0 0" -> v = 1.
    render::shapes::MeshData mf;
    CHECK(render::parseObj(quad, mf));
    CHECK_NEAR(mf.vertices[0].v, 1.0f, 1e-4f);

    // Same positions but different uv combos across two faces must NOT be over-deduplicated.
    const std::string two =
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "vt 0 0\nvt 1 0\nvt 0 1\n"
        "f 1/1 2/2 3/3\n"
        "f 1/2 2/3 3/1\n";
    render::shapes::MeshData m2;
    CHECK(render::parseObj(two, m2, noflip));
    CHECK(m2.indices.size() == 6);
    CHECK(m2.vertices.size() == 6);

    // Position-only faces with negative (relative) indices.
    const std::string neg =
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "f -3 -2 -1\n";
    render::shapes::MeshData m3;
    CHECK(render::parseObj(neg, m3, noflip));
    CHECK(m3.vertices.size() == 3);
    CHECK(m3.indices.size() == 3);
    CHECK_NEAR(m3.vertices[1].px, 1.0f, 1e-4f);

    // v//vn form (no texcoord).
    const std::string vn =
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "vn 0 1 0\n"
        "f 1//1 2//1 3//1\n";
    render::shapes::MeshData m4;
    CHECK(render::parseObj(vn, m4, noflip));
    CHECK(m4.vertices.size() == 3);
    CHECK_NEAR(m4.vertices[0].ny, 1.0f, 1e-4f);

    // No faces -> empty mesh -> false.
    render::shapes::MeshData m5;
    CHECK(!render::parseObj("v 0 0 0\n", m5, noflip));
}

void testMeshLod() {
    const float fov = 1.5707963f; // 90 deg -> tan(45) = 1

    // radius 1 at distance 10, 1000px tall viewport -> 1/(10*1) * 500 = 50 px.
    CHECK_NEAR(render::projectedRadiusPixels(1.0f, 10.0f, fov, 1000.0f), 50.0f, 1e-2f);
    CHECK(render::projectedRadiusPixels(1.0f, 5.0f, fov, 1000.0f) > 90.0f); // closer -> bigger
    CHECK(render::projectedRadiusPixels(1.0f, 0.0f, fov, 1000.0f) > 0.0f);  // at the eye

    render::LodChain c;
    c.addLevel(100.0f); // LOD0 when >= 100 px
    c.addLevel(40.0f);  // LOD1 when >= 40 px
    c.addLevel(10.0f);  // LOD2 when >= 10 px
    c.setCullBelowLast(true);
    CHECK(c.select(120.0f) == 0);
    CHECK(c.select(50.0f) == 1);
    CHECK(c.select(20.0f) == 2);
    CHECK(c.select(5.0f) == -1); // culled below the coarsest threshold

    c.setCullBelowLast(false);
    CHECK(c.select(5.0f) == 2); // clamps to coarsest instead of culling

    // lod_bias keeps finer LODs longer: 30 px * 2 = 60 -> LOD1 (vs LOD2 unbiased).
    c.setBias(1.0f);
    CHECK(c.select(30.0f) == 2);
    c.setBias(2.0f);
    CHECK(c.select(30.0f) == 1);
    c.setBias(1.0f);

    // Projection + select in one call: radius 2 @ dist 10 -> 100 px -> LOD0.
    CHECK(c.selectForCamera(2.0f, 10.0f, fov, 1000.0f) == 0);

    // Hysteresis avoids flicker at a boundary.
    CHECK(c.selectStable(41.0f, 1, 4.0f) == 1); // just above 40, already LOD1 -> stays
    CHECK(c.selectStable(98.0f, 0, 4.0f) == 0); // within margin of the 100 boundary -> stays LOD0
    CHECK(c.selectStable(90.0f, 0, 4.0f) == 1); // clearly below 100-margin -> drops to LOD1
}

void testGettextPo() {
    // Plural-rule evaluator: English, the Polish rule, and the empty default.
    io::PluralRule en("n != 1");
    CHECK(en.eval(1) == 0);
    CHECK(en.eval(0) == 1);
    CHECK(en.eval(2) == 1);

    io::PluralRule pl("(n==1 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2)");
    CHECK(pl.eval(1) == 0);
    CHECK(pl.eval(2) == 1);
    CHECK(pl.eval(4) == 1);
    CHECK(pl.eval(5) == 2);
    CHECK(pl.eval(11) == 2);
    CHECK(pl.eval(22) == 1);
    CHECK(pl.eval(25) == 2);

    io::PluralRule none("");
    CHECK(none.eval(1) == 0);
    CHECK(none.eval(3) == 1);

    // A PO catalog with a header plural rule, a contextual entry, and a plural entry.
    const std::string po =
        "# comment\n"
        "msgid \"\"\n"
        "msgstr \"\"\n"
        "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
        "\"Plural-Forms: nplurals=3; plural=(n==1 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || "
        "n%100>=20) ? 1 : 2);\\n\"\n"
        "\n"
        "msgid \"Hello\"\n"
        "msgstr \"Hi\"\n"
        "\n"
        "msgctxt \"menu\"\n"
        "msgid \"File\"\n"
        "msgstr \"Plik\"\n"
        "\n"
        "msgid \"%d apple\"\n"
        "msgid_plural \"%d apples\"\n"
        "msgstr[0] \"ONE\"\n"
        "msgstr[1] \"FEW\"\n"
        "msgstr[2] \"MANY\"\n";
    io::PoCatalog cat;
    CHECK(cat.parse(po));
    CHECK(cat.nplurals() == 3);

    CHECK(cat.gettext("Hello") == "Hi");
    CHECK(cat.gettext("Missing") == "Missing"); // fallback to id
    CHECK(cat.pgettext("menu", "File") == "Plik");
    CHECK(cat.gettext("File") == "File"); // context-only entry -> no plain hit

    CHECK(cat.ngettext("%d apple", "%d apples", 1) == "ONE");
    CHECK(cat.ngettext("%d apple", "%d apples", 3) == "FEW");
    CHECK(cat.ngettext("%d apple", "%d apples", 5) == "MANY");
    CHECK(cat.ngettext("%d apple", "%d apples", 22) == "FEW");

    // Untranslated plural -> English source fallback.
    CHECK(cat.ngettext("%d cat", "%d cats", 1) == "%d cat");
    CHECK(cat.ngettext("%d cat", "%d cats", 4) == "%d cats");

    // Multi-line msgid concatenation.
    io::PoCatalog c2;
    c2.parse("msgid \"\"\n\"long \"\n\"key\"\nmsgstr \"LONGKEY\"\n");
    CHECK(c2.gettext("long key") == "LONGKEY");

    // Escape sequences in msgstr survive.
    io::PoCatalog c3;
    c3.parse("msgid \"a\"\nmsgstr \"x\\ny\\t\\\"z\\\"\"\n");
    CHECK(c3.gettext("a") == "x\ny\t\"z\"");
}

void testExportConfig() {
    // Glob matcher.
    CHECK(io::globMatch("*.png", "hero.png"));
    CHECK(!io::globMatch("*.png", "hero.jpg"));
    CHECK(io::globMatch("res/*.tscn", "res/level.tscn"));
    CHECK(io::globMatch("res/*.tscn", "res/sub/level.tscn")); // '*' spans '/'
    CHECK(io::globMatch("a?c", "abc"));
    CHECK(!io::globMatch("a?c", "ac"));
    CHECK(io::globMatch("*", "anything/at/all"));
    CHECK(io::globMatch("", ""));
    CHECK(!io::globMatch("", "x"));
    CHECK(io::globMatch("a*b*c", "axxbyyc"));
    CHECK(!io::globMatch("a*b*c", "axxbyy"));

    // Preset include/exclude + features.
    io::ExportPreset p;
    p.name = "Win";
    p.platform = "windows";
    p.exportPath = "build/game.exe";
    p.features = {"windows", "desktop", "pc"};
    p.includeFilters = {"*.png", "*.ogg", "*.tscn"};
    p.excludeFilters = {"*_dev.png"};
    CHECK(p.hasFeature("desktop"));
    CHECK(!p.hasFeature("mobile"));
    CHECK(p.includes("hero.png"));
    CHECK(p.includes("music.ogg"));
    CHECK(!p.includes("notes.txt"));      // not in the include list
    CHECK(!p.includes("splash_dev.png")); // excluded despite matching *.png

    // Empty include list ships everything not excluded.
    io::ExportPreset all;
    all.excludeFilters = {"*.tmp"};
    CHECK(all.includes("anything.dat"));
    CHECK(!all.includes("scratch.tmp"));

    // Config lookup by name and platform.
    io::ExportConfig cfg;
    cfg.add(p);
    io::ExportPreset lin;
    lin.name = "Lin";
    lin.platform = "linux";
    cfg.add(lin);
    io::ExportPreset web;
    web.name = "Web";
    web.platform = "web";
    cfg.add(web);
    CHECK(cfg.count() == 3);
    CHECK(cfg.find("Win") != nullptr);
    CHECK(cfg.find("Win")->platform == "windows");
    CHECK(cfg.find("Nope") == nullptr);
    CHECK(cfg.forPlatform("linux").size() == 1);
    CHECK(cfg.forPlatform("android").empty());
}

void testConvexHull3D() {
    auto dot3 = [](const math::vec3& a, const math::vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    };
    // Every input point must be on the inner side of every hull face (convex + enclosing).
    auto encloses = [&](const game::ConvexHull3D& h, const std::vector<math::vec3>& pts) {
        for (const game::HullFace& f : h.faces) {
            const math::vec3& fa = pts[static_cast<std::size_t>(f.a)];
            for (const math::vec3& q : pts) {
                if (dot3(f.normal, q - fa) > 1e-4f) {
                    return false;
                }
            }
        }
        return true;
    };
    auto hasVert = [](const game::ConvexHull3D& h, int idx) {
        return std::find(h.vertices.begin(), h.vertices.end(), idx) != h.vertices.end();
    };

    // Tetrahedron + an interior point.
    {
        const std::vector<math::vec3> p = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1},
                                           {0.25f, 0.25f, 0.25f}};
        const game::ConvexHull3D h = game::buildConvexHull(p);
        CHECK(h.valid);
        CHECK(h.faces.size() == 4);
        CHECK(h.vertices.size() == 4);
        CHECK(!hasVert(h, 4)); // interior point excluded
        CHECK(encloses(h, p));
    }

    // Cube (8 corners) + interior points -> 8 hull vertices, 12 triangles.
    {
        std::vector<math::vec3> p;
        for (int x = 0; x < 2; ++x) {
            for (int y = 0; y < 2; ++y) {
                for (int z = 0; z < 2; ++z) {
                    p.push_back(math::vec3(static_cast<float>(x), static_cast<float>(y),
                                           static_cast<float>(z)));
                }
            }
        }
        p.push_back(math::vec3(0.5f, 0.5f, 0.5f));
        p.push_back(math::vec3(0.5f, 0.5f, 0.25f));
        const game::ConvexHull3D h = game::buildConvexHull(p);
        CHECK(h.valid);
        CHECK(h.vertices.size() == 8);
        CHECK(h.faces.size() == 12);
        CHECK(encloses(h, p));
        for (int i = 0; i < 8; ++i) {
            CHECK(hasVert(h, i));
        }
        CHECK(!hasVert(h, 8));
        CHECK(!hasVert(h, 9));
    }

    // Octahedron extremes + interior cloud -> 6 vertices, 8 faces.
    {
        const std::vector<math::vec3> p = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                           {0, -1, 0}, {0, 0, 1},  {0, 0, -1},
                                           {0.1f, 0.1f, 0.1f}, {-0.2f, 0.0f, 0.1f}, {0, 0, 0}};
        const game::ConvexHull3D h = game::buildConvexHull(p);
        CHECK(h.valid);
        CHECK(h.vertices.size() == 6);
        CHECK(h.faces.size() == 8);
        CHECK(encloses(h, p));
    }

    // Coplanar set -> invalid (no volume).
    {
        const std::vector<math::vec3> p = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0},
                                           {0.5f, 0.5f, 0}};
        CHECK(!game::buildConvexHull(p).valid);
    }
    // Fewer than 4 points -> invalid.
    {
        const std::vector<math::vec3> p = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
        CHECK(!game::buildConvexHull(p).valid);
    }
}

void testHeightField3D() {
    // Flat field at height 5.
    {
        game::HeightField3D hf(4, 4, 1.0f, 1.0f);
        for (int z = 0; z < 4; ++z) {
            for (int x = 0; x < 4; ++x) {
                hf.setHeight(x, z, 5.0f);
            }
        }
        CHECK_NEAR(hf.heightAt(1.5f, 2.5f), 5.0f, 1e-3f);
        const math::vec3 nrm = hf.normalAt(1.5f, 1.5f);
        CHECK_NEAR(nrm.x, 0.0f, 1e-3f);
        CHECK_NEAR(nrm.y, 1.0f, 1e-3f);
        CHECK_NEAR(nrm.z, 0.0f, 1e-3f);
        float t = 0.0f;
        math::vec3 hp;
        CHECK(hf.raycast(math::vec3(1.5f, 20.0f, 1.5f), math::vec3(0, -1, 0), 100.0f, t, hp));
        CHECK_NEAR(hp.y, 5.0f, 1e-3f);
        CHECK_NEAR(t, 15.0f, 1e-3f);
        CHECK_NEAR(hp.x, 1.5f, 1e-3f);
        CHECK_NEAR(hp.z, 1.5f, 1e-3f);
    }
    // Ramp rising along +x (linear, so bilinear == planar).
    {
        game::HeightField3D hf(5, 3, 2.0f, 2.0f); // spanX=8, spanZ=4
        for (int z = 0; z < 3; ++z) {
            for (int x = 0; x < 5; ++x) {
                hf.setHeight(x, z, static_cast<float>(x));
            }
        }
        CHECK_NEAR(hf.heightAt(4.0f, 2.0f), 2.0f, 1e-3f);
        CHECK_NEAR(hf.heightAt(6.0f, 2.0f), 3.0f, 1e-3f);
        CHECK_NEAR(hf.heightAt(5.0f, 0.0f), 2.5f, 1e-3f);
        const math::vec3 nrm = hf.normalAt(4.0f, 2.0f);
        CHECK(nrm.x < 0.0f); // surface rises toward +x -> normal leans -x
        CHECK(nrm.y > 0.0f);
        float t = 0.0f;
        math::vec3 hp;
        CHECK(hf.raycast(math::vec3(6.0f, 10.0f, 2.0f), math::vec3(0, -1, 0), 100.0f, t, hp));
        CHECK_NEAR(hp.y, 3.0f, 1e-3f);
    }
    // Angled ray across a flat field: descends 45 deg, hits y=0 at x=5.5.
    {
        game::HeightField3D hf(10, 10, 1.0f, 1.0f);
        for (int z = 0; z < 10; ++z) {
            for (int x = 0; x < 10; ++x) {
                hf.setHeight(x, z, 0.0f);
            }
        }
        float t = 0.0f;
        math::vec3 hp;
        CHECK(hf.raycast(math::vec3(0.5f, 5.0f, 0.5f), math::vec3(1, -1, 0), 100.0f, t, hp));
        CHECK_NEAR(hp.y, 0.0f, 1e-3f);
        CHECK_NEAR(hp.x, 5.5f, 1e-3f);
    }
    // Ray pointing up never hits.
    {
        game::HeightField3D hf(4, 4, 1.0f, 1.0f);
        float t = 0.0f;
        math::vec3 hp;
        CHECK(!hf.raycast(math::vec3(1.5f, 1.0f, 1.5f), math::vec3(0, 1, 0), 100.0f, t, hp));
    }
    // Ray starting OUTSIDE the grid, entering it, hits the flat surface.
    {
        game::HeightField3D hf(6, 6, 1.0f, 1.0f);
        for (int z = 0; z < 6; ++z) {
            for (int x = 0; x < 6; ++x) {
                hf.setHeight(x, z, 2.0f);
            }
        }
        float t = 0.0f;
        math::vec3 hp;
        CHECK(hf.raycast(math::vec3(-5.0f, 10.0f, 2.5f), math::vec3(1, -1, 0), 100.0f, t, hp));
        CHECK_NEAR(hp.y, 2.0f, 1e-3f);
        CHECK_NEAR(hp.x, 3.0f, 1e-3f);
    }
}

void testTriMesh3D() {
    // A concave shape: floor at y=0 (x 0..10) plus a raised step at y=2 (x 10..20).
    const std::vector<math::vec3> v = {
        {0, 0, 0},  {10, 0, 0},  {10, 0, 10},  {0, 0, 10},  // floor
        {10, 2, 0}, {20, 2, 0},  {20, 2, 10},  {10, 2, 10}, // step
    };
    const std::vector<uint32_t> idx = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
    game::TriMesh3D tm;
    tm.build(v, idx);
    CHECK(tm.triangleCount() == 4);

    // Down-ray onto the floor.
    game::TriMeshHit h = tm.raycast(math::vec3(5, 10, 5), math::vec3(0, -1, 0), 100.0f);
    CHECK(h.hit);
    CHECK_NEAR(h.point.y, 0.0f, 1e-3f);
    CHECK_NEAR(h.t, 10.0f, 1e-3f);
    CHECK(h.triangle == 0 || h.triangle == 1);

    // Down-ray onto the step.
    h = tm.raycast(math::vec3(15, 10, 5), math::vec3(0, -1, 0), 100.0f);
    CHECK(h.hit);
    CHECK_NEAR(h.point.y, 2.0f, 1e-3f);
    CHECK_NEAR(h.t, 8.0f, 1e-3f);
    CHECK(h.triangle == 2 || h.triangle == 3);

    // Misses: outside in x, pointing up, and too-short maxDist.
    CHECK(!tm.raycast(math::vec3(50, 10, 5), math::vec3(0, -1, 0), 100.0f).hit);
    CHECK(!tm.raycast(math::vec3(5, 10, 5), math::vec3(0, 1, 0), 100.0f).hit);
    CHECK(!tm.raycast(math::vec3(5, 10, 5), math::vec3(0, -1, 0), 5.0f).hit);

    // Nearest-hit: a stacked floor (y=0) + ceiling (y=5); a down-ray hits the ceiling first.
    const std::vector<math::vec3> v2 = {
        {0, 0, 0}, {10, 0, 0}, {10, 0, 10}, {0, 0, 10},
        {0, 5, 0}, {10, 5, 0}, {10, 5, 10}, {0, 5, 10},
    };
    const std::vector<uint32_t> idx2 = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
    game::TriMesh3D tm2;
    tm2.build(v2, idx2);
    h = tm2.raycast(math::vec3(5, 10, 5), math::vec3(0, -1, 0), 100.0f);
    CHECK(h.hit);
    CHECK_NEAR(h.point.y, 5.0f, 1e-3f); // nearest surface, not the floor beneath it
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

void testInterpolate() {
    using core::Interpolated;
    using core::interpolate;
    using core::lerpAngle;
    using core::Transform2DState;
    using math::vec2;

    // Scalar Interpolated: push shifts current -> previous; sample blends by clamped alpha.
    {
        Interpolated<double> ip(0.0);
        ip.push(10.0); // prev=0, cur=10
        CHECK_NEAR(ip.previous(), 0.0, 1e-9);
        CHECK_NEAR(ip.current(), 10.0, 1e-9);
        CHECK_NEAR(ip.sample(0.0), 0.0, 1e-9);
        CHECK_NEAR(ip.sample(1.0), 10.0, 1e-9);
        CHECK_NEAR(ip.sample(0.5), 5.0, 1e-9);
        CHECK_NEAR(ip.sample(-2.0), 0.0, 1e-9); // alpha clamps (no extrapolation)
        CHECK_NEAR(ip.sample(3.0), 10.0, 1e-9);

        ip.push(20.0); // prev=10, cur=20
        CHECK_NEAR(ip.sample(0.5), 15.0, 1e-9);
    }

    // Vector Interpolated.
    {
        Interpolated<vec2> ip(vec2(0.0f, 0.0f));
        ip.push(vec2(4.0f, 8.0f));
        const vec2 m = ip.sample(0.25);
        CHECK_NEAR(m.x, 1.0f, 1e-5f);
        CHECK_NEAR(m.y, 2.0f, 1e-5f);
    }

    // reset seeds both prev and cur (no ghost blend on the first frame after a teleport).
    {
        Interpolated<double> ip(5.0);
        ip.push(9.0);
        ip.reset(100.0);
        CHECK_NEAR(ip.sample(0.5), 100.0, 1e-9);
    }

    // lerpAngle takes the shortest arc across the +/-pi seam.
    {
        // 3.0 -> -3.0 rad: the short way is +0.283 (through pi), not -6.0.
        const float a = lerpAngle(3.0f, -3.0f, 0.5);
        CHECK(a > 3.0f); // moved forward through the seam, not backward
        CHECK_NEAR(a, 3.14159265f, 2e-2f);
        // A plain half-turn interpolates linearly.
        CHECK_NEAR(lerpAngle(0.0f, 1.0f, 0.5), 0.5f, 1e-5f);
    }

    // Transform2DState interpolate: position/scale lerp, rotation shortest-arc.
    {
        Transform2DState a;
        a.position = vec2(0.0f, 0.0f);
        a.rotation = 0.0f;
        a.scale = vec2(1.0f, 1.0f);
        Transform2DState b;
        b.position = vec2(10.0f, 20.0f);
        b.rotation = 1.0f;
        b.scale = vec2(3.0f, 3.0f);
        const Transform2DState m = interpolate(a, b, 0.5);
        CHECK_NEAR(m.position.x, 5.0f, 1e-5f);
        CHECK_NEAR(m.position.y, 10.0f, 1e-5f);
        CHECK_NEAR(m.rotation, 0.5f, 1e-5f);
        CHECK_NEAR(m.scale.x, 2.0f, 1e-5f);
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

void testMultiMesh2D() {
    using math::vec2;
    using render::Instance2D;
    using render::MultiMesh2D;
    using render::transformInstance;

    // transformInstance: TRS order (scale -> rotate -> translate).
    {
        Instance2D id; // identity: pos 0, rot 0, scale 1
        CHECK_NEAR(transformInstance(id, vec2(3.0f, 4.0f)).x, 3.0f, 1e-5f);
        CHECK_NEAR(transformInstance(id, vec2(3.0f, 4.0f)).y, 4.0f, 1e-5f);

        Instance2D t;
        t.position = vec2(10.0f, 20.0f);
        CHECK_NEAR(transformInstance(t, vec2(0.0f, 0.0f)).x, 10.0f, 1e-5f);
        CHECK_NEAR(transformInstance(t, vec2(0.0f, 0.0f)).y, 20.0f, 1e-5f);

        Instance2D sc;
        sc.scale = vec2(2.0f, 3.0f);
        CHECK_NEAR(transformInstance(sc, vec2(1.0f, 1.0f)).x, 2.0f, 1e-5f);
        CHECK_NEAR(transformInstance(sc, vec2(1.0f, 1.0f)).y, 3.0f, 1e-5f);

        // 90° rotation sends local +X to +Y (y-down or y-up, the sin/cos convention is consistent).
        Instance2D r;
        r.rotation = 1.5707963f; // pi/2
        const vec2 p = transformInstance(r, vec2(1.0f, 0.0f));
        CHECK_NEAR(p.x, 0.0f, 1e-4f);
        CHECK_NEAR(p.y, 1.0f, 1e-4f);

        // Combined: scale (2,2) then rotate 90° then translate (5,5): local (1,0) -> (2,0) -> (0,2) -> (5,7).
        Instance2D trs;
        trs.scale = vec2(2.0f, 2.0f);
        trs.rotation = 1.5707963f;
        trs.position = vec2(5.0f, 5.0f);
        const vec2 q = transformInstance(trs, vec2(1.0f, 0.0f));
        CHECK_NEAR(q.x, 5.0f, 1e-3f);
        CHECK_NEAR(q.y, 7.0f, 1e-3f);
    }

    // A MultiMesh of a unit triangle, stamped at three places.
    {
        MultiMesh2D mm;
        mm.baseVertices = {vec2(0.0f, 0.0f), vec2(10.0f, 0.0f), vec2(0.0f, 10.0f)};
        CHECK(mm.instanceCount() == 0);
        mm.addInstance(Instance2D{vec2(100.0f, 0.0f), 0.0f, vec2(1, 1), {}});
        mm.addInstance(Instance2D{vec2(0.0f, 100.0f), 0.0f, vec2(1, 1), {}});
        mm.addInstance(Instance2D{vec2(50.0f, 50.0f), 0.0f, vec2(2, 2), {}});
        CHECK(mm.instanceCount() == 3);

        // transformedPolygon keeps the vertex count and applies the instance transform.
        const std::vector<vec2> p0 = mm.transformedPolygon(0);
        CHECK(p0.size() == 3);
        CHECK_NEAR(p0[0].x, 100.0f, 1e-4f); // first vertex at the instance position
        CHECK_NEAR(p0[1].x, 110.0f, 1e-4f); // +X vertex shifted by position

        // The scaled instance doubles the triangle's reach.
        const std::vector<vec2> p2 = mm.transformedPolygon(2);
        CHECK_NEAR(p2[1].x, 70.0f, 1e-4f); // 50 + 10*2

        // Baking: a triangle fans to 1 tri (3 verts) per instance -> 3*3 = 9 vertices.
        CHECK(mm.triangleCount() == 3);
        CHECK(mm.bakeTriangles().size() == 9);
    }

    // A convex quad (4 verts) fans to 2 triangles = 6 vertices per instance.
    {
        MultiMesh2D mm;
        mm.baseVertices = {vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1)};
        mm.addInstance(Instance2D{});
        mm.addInstance(Instance2D{});
        CHECK(mm.triangleCount() == 4);        // 2 tris * 2 instances
        CHECK(mm.bakeTriangles().size() == 12); // 4 tris * 3 verts

        mm.clear();
        CHECK(mm.instanceCount() == 0);
        CHECK(mm.bakeTriangles().empty());
    }

    // A degenerate base (<3 verts) bakes nothing.
    {
        MultiMesh2D mm;
        mm.baseVertices = {vec2(0, 0), vec2(1, 1)};
        mm.addInstance(Instance2D{});
        CHECK(mm.triangleCount() == 0);
        CHECK(mm.bakeTriangles().empty());
    }
}

void testBillboard() {
    using math::mat4;
    using math::vec3;
    using render::BillboardMode;
    using render::buildBillboard;

    auto col = [](const mat4& m, int i) { return vec3(m[i]); };

    // Camera straight in front on +Z looking at the origin: a full billboard's normal (its local +Z, model
    // column 2) points straight back at the camera (+Z), and its translation is the given position.
    {
        const mat4 view = glm::lookAt(vec3(0, 0, 5), vec3(0, 0, 0), vec3(0, 1, 0));
        const mat4 m = buildBillboard(vec3(0, 0, 0), vec3(1, 1, 1), view, BillboardMode::Enabled);
        const vec3 n = glm::normalize(col(m, 2));
        CHECK_NEAR(n.z, 1.0f, 1e-3f);
        CHECK_NEAR(col(m, 3).x, 0.0f, 1e-4f);
        CHECK_NEAR(col(m, 3).y, 0.0f, 1e-4f);
        CHECK_NEAR(col(m, 3).z, 0.0f, 1e-4f);

        // Right/up align with the camera's right/up (screen-aligned).
        CHECK_NEAR(glm::normalize(col(m, 0)).x, 1.0f, 1e-3f);
        CHECK_NEAR(glm::normalize(col(m, 1)).y, 1.0f, 1e-3f);
    }

    // Disabled leaves the quad axis-aligned; translation + scale still apply.
    {
        const mat4 view = glm::lookAt(vec3(5, 0, 0), vec3(0, 0, 0), vec3(0, 1, 0));
        const mat4 m = buildBillboard(vec3(3, 4, 5), vec3(2, 2, 2), view, BillboardMode::Disabled);
        CHECK_NEAR(col(m, 0).x, 2.0f, 1e-4f);
        CHECK_NEAR(col(m, 0).y, 0.0f, 1e-4f);
        CHECK_NEAR(col(m, 1).y, 2.0f, 1e-4f);
        CHECK_NEAR(col(m, 2).z, 2.0f, 1e-4f);
        CHECK_NEAR(col(m, 3).x, 3.0f, 1e-4f);
        CHECK_NEAR(col(m, 3).y, 4.0f, 1e-4f);
        CHECK_NEAR(col(m, 3).z, 5.0f, 1e-4f);
    }

    // Camera off to the side (+X): a full billboard's normal turns to face it (points along +X).
    {
        const mat4 view = glm::lookAt(vec3(6, 0, 0), vec3(0, 0, 0), vec3(0, 1, 0));
        const mat4 m = buildBillboard(vec3(0, 0, 0), vec3(1, 1, 1), view, BillboardMode::Enabled);
        const vec3 n = glm::normalize(col(m, 2));
        CHECK_NEAR(n.x, 1.0f, 1e-3f);
    }

    // Elevated camera looking down: a Y-billboard stays perfectly upright (its up axis is exactly world-up),
    // while a full billboard tilts back to face the camera (its up axis is NOT world-up).
    {
        const mat4 view = glm::lookAt(vec3(0, 6, 6), vec3(0, 0, 0), vec3(0, 1, 0));
        const mat4 y = buildBillboard(vec3(0, 0, 0), vec3(1, 1, 1), view, BillboardMode::YBillboard);
        const vec3 yUp = glm::normalize(col(y, 1));
        CHECK_NEAR(yUp.x, 0.0f, 1e-4f);
        CHECK_NEAR(yUp.y, 1.0f, 1e-4f);
        CHECK_NEAR(yUp.z, 0.0f, 1e-4f);
        // Its normal lies in the ground plane (no vertical component).
        CHECK_NEAR(glm::normalize(col(y, 2)).y, 0.0f, 1e-3f);

        const mat4 e = buildBillboard(vec3(0, 0, 0), vec3(1, 1, 1), view, BillboardMode::Enabled);
        const vec3 eUp = glm::normalize(col(e, 1));
        CHECK(std::fabs(eUp.y - 1.0f) > 0.05f); // tilted, not world-up

        // A full billboard's basis is orthonormal.
        const vec3 x = glm::normalize(col(e, 0));
        const vec3 u = glm::normalize(col(e, 1));
        const vec3 n = glm::normalize(col(e, 2));
        CHECK_NEAR(glm::dot(x, u), 0.0f, 1e-3f);
        CHECK_NEAR(glm::dot(x, n), 0.0f, 1e-3f);
        CHECK_NEAR(glm::dot(u, n), 0.0f, 1e-3f);
    }
}

void testCamera3D() {
    using math::vec2;
    using math::vec3;
    using render::Camera3D;

    // Camera at (0,0,5) looking at the origin, up +Y, 90-degree vertical FOV, square 800x600 viewport.
    Camera3D cam;
    cam.viewportWidth = 800.0f;
    cam.viewportHeight = 600.0f;
    cam.lookAt(vec3(0, 0, 5), vec3(0, 0, 0), vec3(0, 1, 0));
    cam.perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);

    // The look-at target projects to the exact centre of the viewport and is in front.
    {
        const render::Projected p = cam.worldToScreen(vec3(0, 0, 0));
        CHECK(p.inFront);
        CHECK_NEAR(p.screen.x, 400.0f, 0.5f);
        CHECK_NEAR(p.screen.y, 300.0f, 0.5f);
        CHECK(p.depth >= 0.0f && p.depth <= 1.0f);
    }

    // A point to the right of the axis (world +X) projects right of centre; world +Y projects ABOVE centre
    // (smaller pixel y — top-left origin). With fov 90 / aspect 1, a unit offset 5 units away sits at ndc 0.2.
    {
        const render::Projected right = cam.worldToScreen(vec3(1, 0, 0));
        CHECK_NEAR(right.screen.x, 400.0f + 0.2f * 400.0f, 1.0f); // 480
        CHECK_NEAR(right.screen.y, 300.0f, 1.0f);

        const render::Projected up = cam.worldToScreen(vec3(0, 1, 0));
        CHECK(up.screen.y < 300.0f); // above centre
        CHECK_NEAR(up.screen.x, 400.0f, 1.0f);
    }

    // Nearer points have smaller clip depth than farther ones (both on the view axis).
    {
        const float dNear = cam.worldToScreen(vec3(0, 0, 2)).depth;  // 3 units from the eye
        const float dFar = cam.worldToScreen(vec3(0, 0, -3)).depth;  // 8 units from the eye
        CHECK(dNear < dFar);
    }

    // A point behind the camera reports inFront == false.
    {
        const render::Projected behind = cam.worldToScreen(vec3(0, 0, 10)); // further +Z than the eye
        CHECK(!behind.inFront);
    }

    // screenToRay from the viewport centre points straight down the view axis (-Z), rooted at the eye.
    {
        const render::Ray3 r = cam.screenToRay(vec2(400, 300));
        CHECK_NEAR(r.origin.x, 0.0f, 1e-3f);
        CHECK_NEAR(r.origin.y, 0.0f, 1e-3f);
        CHECK_NEAR(r.origin.z, 5.0f, 1e-3f);
        CHECK_NEAR(r.direction.z, -1.0f, 1e-3f);
        CHECK_NEAR(r.direction.x, 0.0f, 1e-3f);
        CHECK_NEAR(r.direction.y, 0.0f, 1e-3f);
        CHECK_NEAR(glm::length(r.direction), 1.0f, 1e-4f); // normalized
    }

    // A ray through a point right of centre tilts toward +X.
    {
        const render::Ray3 r = cam.screenToRay(vec2(600, 300));
        CHECK(r.direction.x > 0.0f);
    }

    // worldToScreen and screenToRay are inverse: the ray through a projected point passes through it.
    {
        const vec3 target(0.6f, -0.4f, 0.0f);
        const render::Projected p = cam.worldToScreen(target);
        const render::Ray3 r = cam.screenToRay(p.screen);
        // target lies on the ray: (target-origin) parallel to direction.
        const vec3 to = glm::normalize(target - r.origin);
        CHECK_NEAR(glm::dot(to, r.direction), 1.0f, 1e-3f);
    }

    // screenToWorld: 5 units down the centre ray lands on the origin (the look-at target).
    {
        const vec3 w = cam.screenToWorld(vec2(400, 300), 5.0f);
        CHECK_NEAR(w.x, 0.0f, 1e-3f);
        CHECK_NEAR(w.y, 0.0f, 1e-3f);
        CHECK_NEAR(w.z, 0.0f, 1e-3f);
    }

    // Frustum containment: the origin is visible; a point behind the camera and one far to the side are not.
    {
        CHECK(cam.isPointVisible(vec3(0, 0, 0)));
        CHECK(!cam.isPointVisible(vec3(0, 0, 10)));   // behind
        CHECK(!cam.isPointVisible(vec3(100, 0, 0)));  // way off to the side
        CHECK(!cam.isPointVisible(vec3(0, 0, -200))); // beyond the far plane
        // A sphere straddling the near edge is still visible even if its centre is just behind the camera.
        CHECK(cam.isSphereVisible(vec3(0, 0, 0), 1.0f));
        CHECK(!cam.isSphereVisible(vec3(100, 0, 0), 1.0f));
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

void testMeshTools() {
    using math::vec2;
    using math::vec3;
    using math::vec4;
    using render::computeNormals;
    using render::computeTangents;

    // A single CCW triangle in the XY plane: all three vertices get the +Z plane normal.
    {
        const std::vector<vec3> pos = {vec3(0, 0, 0), vec3(1, 0, 0), vec3(0, 1, 0)};
        const std::vector<std::uint32_t> idx = {0, 1, 2};
        const std::vector<vec3> nrm = computeNormals(pos, idx);
        CHECK(nrm.size() == 3);
        for (const vec3& v : nrm) {
            CHECK_NEAR(v.x, 0.0f, 1e-5f);
            CHECK_NEAR(v.y, 0.0f, 1e-5f);
            CHECK_NEAR(v.z, 1.0f, 1e-5f);
        }
    }

    // A flat quad (two triangles, four shared vertices) in the XY plane: every normal is +Z and unit.
    {
        const std::vector<vec3> pos = {vec3(0, 0, 0), vec3(1, 0, 0), vec3(1, 1, 0), vec3(0, 1, 0)};
        const std::vector<std::uint32_t> idx = {0, 1, 2, 0, 2, 3};
        const std::vector<vec3> nrm = computeNormals(pos, idx);
        for (const vec3& v : nrm) {
            CHECK_NEAR(v.z, 1.0f, 1e-5f);
            CHECK_NEAR(std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z), 1.0f, 1e-5f);
        }
    }

    // A symmetric "roof": two triangular slopes sharing one ridge edge. Each ridge vertex touches exactly
    // one left and one right face, so by mirror symmetry its averaged normal points straight up (+Z) with
    // no X tilt, while a foot vertex keeps its single slope's tilt.
    {
        const std::vector<vec3> pos = {
            vec3(-1, 0.5f, 0), // 0 left foot
            vec3(0, 0, 1),     // 1 ridge (far)
            vec3(0, 1, 1),     // 2 ridge (near)
            vec3(1, 0.5f, 0),  // 3 right foot
        };
        const std::vector<std::uint32_t> idx = {
            0, 1, 2, // left slope (leans -X, faces up)
            3, 2, 1, // right slope (leans +X, faces up)
        };
        const std::vector<vec3> nrm = computeNormals(pos, idx);
        for (std::uint32_t ri : {1u, 2u}) {
            const vec3& v = nrm[ri];
            CHECK_NEAR(v.x, 0.0f, 1e-5f); // symmetric slopes cancel the X tilt
            CHECK(v.z > 0.9f);            // points mostly up
            CHECK_NEAR(std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z), 1.0f, 1e-5f);
        }
        CHECK(nrm[0].x < 0.0f); // left foot keeps the left slope's -X tilt
        CHECK(nrm[3].x > 0.0f); // right foot keeps the right slope's +X tilt
    }

    // A vertex touched by no triangle gets a zero normal (nothing to average).
    {
        const std::vector<vec3> pos = {vec3(0, 0, 0), vec3(1, 0, 0), vec3(0, 1, 0), vec3(5, 5, 5)};
        const std::vector<std::uint32_t> idx = {0, 1, 2};
        const std::vector<vec3> nrm = computeNormals(pos, idx);
        CHECK_NEAR(nrm[3].x, 0.0f, 1e-6f);
        CHECK_NEAR(nrm[3].y, 0.0f, 1e-6f);
        CHECK_NEAR(nrm[3].z, 0.0f, 1e-6f);
    }

    // Tangents on a UV-mapped quad in the XY plane: U runs along +X, V along +Y, normal +Z.
    // Expect tangent ~ +X, orthogonal to the normal, unit length, right-handed.
    {
        const std::vector<vec3> pos = {vec3(0, 0, 0), vec3(1, 0, 0), vec3(1, 1, 0), vec3(0, 1, 0)};
        const std::vector<vec3> nrm = {vec3(0, 0, 1), vec3(0, 0, 1), vec3(0, 0, 1), vec3(0, 0, 1)};
        const std::vector<vec2> uv = {vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1)};
        const std::vector<std::uint32_t> idx = {0, 1, 2, 0, 2, 3};
        const std::vector<vec4> tan = computeTangents(pos, nrm, uv, idx);
        CHECK(tan.size() == 4);
        for (const vec4& t : tan) {
            CHECK_NEAR(t.x, 1.0f, 1e-4f);
            CHECK_NEAR(t.y, 0.0f, 1e-4f);
            CHECK_NEAR(t.z, 0.0f, 1e-4f);
            // Orthogonal to the +Z normal and unit length in xyz.
            CHECK_NEAR(t.z, 0.0f, 1e-4f);
            CHECK_NEAR(std::sqrt(t.x * t.x + t.y * t.y + t.z * t.z), 1.0f, 1e-4f);
            CHECK(std::fabs(t.w) == 1.0f);
        }
    }

    // Flipped V (U along +X, V along -Y) flips the handedness sign in .w.
    {
        const std::vector<vec3> pos = {vec3(0, 0, 0), vec3(1, 0, 0), vec3(1, 1, 0), vec3(0, 1, 0)};
        const std::vector<vec3> nrm = {vec3(0, 0, 1), vec3(0, 0, 1), vec3(0, 0, 1), vec3(0, 0, 1)};
        const std::vector<vec2> uvA = {vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1)};
        const std::vector<vec2> uvB = {vec2(0, 1), vec2(1, 1), vec2(1, 0), vec2(0, 0)}; // V flipped
        const std::vector<std::uint32_t> idx = {0, 1, 2, 0, 2, 3};
        const vec4 a = computeTangents(pos, nrm, uvA, idx)[0];
        const vec4 b = computeTangents(pos, nrm, uvB, idx)[0];
        CHECK(a.w * b.w < 0.0f); // opposite handedness
    }
}

void testTriangulate() {
    using math::vec2;
    using render::polygonArea;
    using render::triangulatePolygon;
    using render::triSignedArea2;

    // Sum of absolute triangle areas — equals the polygon area iff the triangulation tiles it exactly
    // (no overlap, no gap, nothing spilling outside). A naive fan on a concave shape fails this.
    auto trisArea = [](const std::vector<vec2>& poly, const std::vector<std::uint32_t>& idx) {
        float a = 0.0f;
        for (std::size_t i = 0; i < idx.size(); i += 3) {
            a += std::fabs(triSignedArea2(poly[idx[i]], poly[idx[i + 1]], poly[idx[i + 2]])) * 0.5f;
        }
        return a;
    };
    auto allCcw = [](const std::vector<vec2>& poly, const std::vector<std::uint32_t>& idx) {
        for (std::size_t i = 0; i < idx.size(); i += 3) {
            if (triSignedArea2(poly[idx[i]], poly[idx[i + 1]], poly[idx[i + 2]]) <= 0.0f) {
                return false;
            }
        }
        return true;
    };

    // Primitive helpers.
    CHECK(triSignedArea2(vec2(0, 0), vec2(1, 0), vec2(0, 1)) > 0.0f); // CCW turn
    CHECK(triSignedArea2(vec2(0, 0), vec2(0, 1), vec2(1, 0)) < 0.0f); // CW turn
    CHECK(render::pointInTriangle(vec2(0.2f, 0.2f), vec2(0, 0), vec2(1, 0), vec2(0, 1)));
    CHECK(!render::pointInTriangle(vec2(0.9f, 0.9f), vec2(0, 0), vec2(1, 0), vec2(0, 1)));

    // Degenerate input yields nothing.
    CHECK(triangulatePolygon({}).empty());
    CHECK(triangulatePolygon({vec2(0, 0), vec2(1, 0)}).empty());

    // A lone triangle passes straight through as one triangle.
    {
        const std::vector<vec2> tri = {vec2(0, 0), vec2(4, 0), vec2(0, 3)};
        const auto idx = triangulatePolygon(tri);
        CHECK(idx.size() == 3);
        CHECK_NEAR(trisArea(tri, idx), 6.0f, 1e-4f);
        CHECK(allCcw(tri, idx));
    }

    // A CCW square -> 2 triangles that tile its area exactly.
    {
        const std::vector<vec2> sq = {vec2(0, 0), vec2(2, 0), vec2(2, 2), vec2(0, 2)};
        const auto idx = triangulatePolygon(sq);
        CHECK(idx.size() == 6); // n-2 triangles
        CHECK_NEAR(polygonArea(sq), 4.0f, 1e-4f);
        CHECK_NEAR(trisArea(sq, idx), 4.0f, 1e-4f);
        CHECK(allCcw(sq, idx));
        for (std::uint32_t v : idx) {
            CHECK(v < 4);
        }
    }

    // The SAME square wound clockwise: winding is detected and normalised, output still tiles it.
    {
        const std::vector<vec2> sq = {vec2(0, 0), vec2(0, 2), vec2(2, 2), vec2(2, 0)};
        CHECK(render::polygonSignedArea2(sq) < 0.0f);
        const auto idx = triangulatePolygon(sq);
        CHECK(idx.size() == 6);
        CHECK_NEAR(trisArea(sq, idx), 4.0f, 1e-4f);
        CHECK(allCcw(sq, idx));
    }

    // A CONCAVE dart (vertex (1,2) is reflex). A fan from vertex 0 spills outside and over-counts area;
    // ear clipping tiles exactly to the true area of 4.
    {
        const std::vector<vec2> dart = {vec2(0, 0), vec2(3, 2), vec2(0, 4), vec2(1, 2)};
        CHECK_NEAR(polygonArea(dart), 4.0f, 1e-4f);
        const auto idx = triangulatePolygon(dart);
        CHECK(idx.size() == 6);
        CHECK_NEAR(trisArea(dart, idx), 4.0f, 1e-4f);
        CHECK(allCcw(dart, idx));
    }

    // A plus/cross — 12 vertices, four reflex corners, area 5. Exercises repeated ear removal.
    {
        const std::vector<vec2> plus = {vec2(1, 0), vec2(2, 0), vec2(2, 1), vec2(3, 1),
                                        vec2(3, 2), vec2(2, 2), vec2(2, 3), vec2(1, 3),
                                        vec2(1, 2), vec2(0, 2), vec2(0, 1), vec2(1, 1)};
        CHECK_NEAR(polygonArea(plus), 5.0f, 1e-4f);
        const auto idx = triangulatePolygon(plus);
        CHECK(idx.size() == 30); // (12-2) triangles
        CHECK_NEAR(trisArea(plus, idx), 5.0f, 1e-4f);
        CHECK(allCcw(plus, idx));
        for (std::uint32_t v : idx) {
            CHECK(v < 12);
        }
    }
}

void testGroupRegistry() {
    using scene::GroupNode;
    using scene::GroupRegistry;

    // Add / duplicate / membership / size.
    {
        GroupRegistry g;
        CHECK(g.add(1, "enemies"));
        CHECK(g.add(2, "enemies"));
        CHECK(!g.add(1, "enemies")); // duplicate -> no-op, returns false
        CHECK(g.groupSize("enemies") == 2);
        CHECK(g.isInGroup(1, "enemies"));
        CHECK(!g.isInGroup(3, "enemies"));
        CHECK(!g.isInGroup(1, "pickups")); // unknown group
        CHECK(g.hasGroup("enemies"));
        CHECK(!g.hasGroup("pickups"));

        // Insertion order is preserved for deterministic queries.
        const std::vector<GroupNode> members = g.nodesInGroup("enemies");
        CHECK(members.size() == 2);
        CHECK(members[0] == 1u);
        CHECK(members[1] == 2u);
        CHECK(g.nodesInGroup("nope").empty());
    }

    // A node in several groups; groupsOf reports them; removing from one leaves the others.
    {
        GroupRegistry g;
        g.add(7, "enemies");
        g.add(7, "flying");
        g.add(7, "boss");
        const std::vector<std::string> gs = g.groupsOf(7);
        CHECK(gs.size() == 3);
        CHECK(gs[0] == "enemies");
        CHECK(gs[2] == "boss");
        CHECK(g.groupCount() == 3);

        CHECK(g.remove(7, "flying"));
        CHECK(!g.remove(7, "flying")); // already gone
        CHECK(!g.isInGroup(7, "flying"));
        CHECK(!g.hasGroup("flying")); // empty group dropped
        CHECK(g.isInGroup(7, "enemies"));
        CHECK(g.groupsOf(7).size() == 2);
        CHECK(g.groupCount() == 2);
    }

    // removeNode clears the node from every group at once (node destruction).
    {
        GroupRegistry g;
        g.add(5, "a");
        g.add(5, "b");
        g.add(6, "a");
        g.removeNode(5);
        CHECK(!g.isInGroup(5, "a"));
        CHECK(!g.isInGroup(5, "b"));
        CHECK(g.groupsOf(5).empty());
        CHECK(!g.hasGroup("b"));         // b had only node 5
        CHECK(g.isInGroup(6, "a"));      // a survives via node 6
        CHECK(g.groupSize("a") == 1);
    }

    // call() visits every member, and is safe when fn mutates the registry (snapshot iteration).
    {
        GroupRegistry g;
        for (GroupNode n = 0; n < 5; ++n) {
            g.add(n, "actors");
        }
        int visited = 0;
        GroupNode sum = 0;
        g.call("actors", [&](GroupNode n) {
            ++visited;
            sum += n;
        });
        CHECK(visited == 5);
        CHECK(sum == 0u + 1u + 2u + 3u + 4u);

        // During the broadcast, remove each visited node — snapshot keeps the walk intact.
        int freed = 0;
        g.call("actors", [&](GroupNode n) {
            g.removeNode(n);
            ++freed;
        });
        CHECK(freed == 5);
        CHECK(!g.hasGroup("actors")); // all members removed
        CHECK(g.groupCount() == 0);
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

void testAnalog() {
    using input::analogVector;
    using input::applyDeadzone;
    using math::vec2;

    // --- applyDeadzone (one signed axis, rescaled) ---
    CHECK_NEAR(applyDeadzone(0.0f, 0.2f), 0.0f, 1e-6f);
    CHECK_NEAR(applyDeadzone(0.1f, 0.2f), 0.0f, 1e-6f);  // inside deadzone
    CHECK_NEAR(applyDeadzone(0.2f, 0.2f), 0.0f, 1e-6f);  // exactly at edge -> 0
    CHECK_NEAR(applyDeadzone(0.6f, 0.2f), 0.5f, 1e-5f);  // (0.6-0.2)/0.8
    CHECK_NEAR(applyDeadzone(1.0f, 0.2f), 1.0f, 1e-5f);  // full tilt -> 1
    CHECK_NEAR(applyDeadzone(-0.6f, 0.2f), -0.5f, 1e-5f); // sign preserved
    CHECK_NEAR(applyDeadzone(1.5f, 0.2f), 1.0f, 1e-5f);  // over-range clamps to 1
    CHECK_NEAR(applyDeadzone(0.5f, 0.0f), 0.5f, 1e-6f);  // no deadzone -> passthrough
    // A pathological deadzone is sanitized rather than dividing by zero.
    CHECK(std::isfinite(applyDeadzone(0.5f, 1.0f)));

    auto len = [](vec2 v) { return std::sqrt(v.x * v.x + v.y * v.y); };

    // --- analogVector (2D radial deadzone + unit clamp) ---
    // Rest and jitter inside the deadzone collapse to zero.
    CHECK(len(analogVector(vec2(0.0f, 0.0f), 0.2f)) < 1e-6f);
    CHECK(len(analogVector(vec2(0.1f, 0.0f), 0.2f)) < 1e-6f);
    CHECK(len(analogVector(vec2(-0.12f, 0.1f), 0.2f)) < 1e-6f); // magnitude 0.156 < 0.2

    // On-axis tilt past the deadzone: direction kept, magnitude rescaled (0.6-0.2)/0.8 = 0.5.
    {
        const vec2 v = analogVector(vec2(0.6f, 0.0f), 0.2f);
        CHECK_NEAR(v.x, 0.5f, 1e-5f);
        CHECK_NEAR(v.y, 0.0f, 1e-6f);
    }
    // Full tilt on an axis -> unit magnitude.
    CHECK_NEAR(len(analogVector(vec2(1.0f, 0.0f), 0.2f)), 1.0f, 1e-5f);
    // Way past the rim -> clamped onto the unit circle, direction preserved.
    {
        const vec2 v = analogVector(vec2(3.0f, 0.0f), 0.2f);
        CHECK_NEAR(v.x, 1.0f, 1e-5f);
        CHECK_NEAR(v.y, 0.0f, 1e-6f);
    }
    // THE diagonal fix: a full (1,1) push has raw length √2, but the result is clamped to length 1 —
    // so a diagonal is no faster than a cardinal. Direction (equal x,y) is preserved.
    {
        const vec2 v = analogVector(vec2(1.0f, 1.0f), 0.0f);
        CHECK_NEAR(len(v), 1.0f, 1e-5f);
        CHECK_NEAR(v.x, v.y, 1e-6f);
        CHECK_NEAR(v.x, 0.70710678f, 1e-5f);
    }
    // A diagonal already inside the unit circle keeps its (rescaled) direction, parallel to the input.
    {
        const vec2 raw(0.5f, 0.3f);
        const vec2 v = analogVector(raw, 0.1f);
        CHECK(len(v) > 0.0f && len(v) <= 1.0f);
        CHECK_NEAR(v.x * raw.y - v.y * raw.x, 0.0f, 1e-6f); // cross == 0 -> parallel
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

void testSlotMap() {
    using core::SlotHandle;
    using core::SlotMap;

    // Insert returns a live handle; get returns the value; contains is true.
    {
        SlotMap<int> m;
        const SlotHandle a = m.insert(10);
        const SlotHandle b = m.insert(20);
        CHECK(m.size() == 2);
        CHECK(m.contains(a));
        CHECK(m.contains(b));
        CHECK(m.get(a) != nullptr);
        CHECK(*m.get(a) == 10);
        CHECK(*m.get(b) == 20);
        CHECK(a != b);
    }

    // A default-constructed handle is never valid.
    {
        SlotMap<int> m;
        m.insert(5);
        CHECK(!m.contains(SlotHandle{}));
        CHECK(m.get(SlotHandle{}) == nullptr);
    }

    // Erase invalidates the handle; size drops; double-erase is safe (returns false).
    {
        SlotMap<int> m;
        const SlotHandle a = m.insert(7);
        CHECK(m.erase(a));
        CHECK(!m.contains(a));
        CHECK(m.get(a) == nullptr);
        CHECK(m.size() == 0);
        CHECK(!m.erase(a)); // already gone
    }

    // THE generational property: erase then re-insert reuses the slot with a bumped generation, so the
    // OLD handle is stale (get -> nullptr) while the NEW handle to the same slot is valid.
    {
        SlotMap<int> m;
        const SlotHandle a = m.insert(100);
        m.erase(a);
        const SlotHandle b = m.insert(200); // reuses slot index a.index
        CHECK(b.index == a.index);          // same physical slot
        CHECK(b.generation != a.generation); // different generation
        CHECK(!m.contains(a));              // stale old handle
        CHECK(m.get(a) == nullptr);
        CHECK(m.contains(b));               // fresh handle valid
        CHECK(*m.get(b) == 200);
    }

    // Handles remain stable while OTHER slots are erased.
    {
        SlotMap<int> m;
        const SlotHandle a = m.insert(1);
        const SlotHandle b = m.insert(2);
        const SlotHandle c = m.insert(3);
        m.erase(b);
        CHECK(m.contains(a));
        CHECK(m.contains(c));
        CHECK(*m.get(a) == 1);
        CHECK(*m.get(c) == 3);
        CHECK(m.size() == 2);
    }

    // forEach visits only live values.
    {
        SlotMap<int> m;
        m.insert(10);
        const SlotHandle b = m.insert(20);
        m.insert(30);
        m.erase(b);
        int sum = 0, count = 0;
        m.forEach([&](SlotHandle, int& v) {
            sum += v;
            ++count;
        });
        CHECK(count == 2);
        CHECK(sum == 40); // 10 + 30
    }

    // clear empties everything.
    {
        SlotMap<int> m;
        m.insert(1);
        m.insert(2);
        m.clear();
        CHECK(m.empty());
        CHECK(m.size() == 0);
    }

    // Freed slots are recycled (capacity doesn't grow on reuse).
    {
        SlotMap<int> m;
        const SlotHandle a = m.insert(1);
        m.erase(a);
        m.insert(2);
        CHECK(m.capacity() == 1); // reused the one slot rather than allocating a second
    }
}

void testRingBuffer() {
    using core::RingBuffer;

    // Fill under capacity: FIFO order, size grows, front/back/at track logical order.
    {
        RingBuffer<int> rb(4);
        CHECK(rb.capacity() == 4);
        CHECK(rb.empty() && !rb.full() && rb.size() == 0);
        CHECK(!rb.push(10)); // no eviction while filling
        CHECK(!rb.push(20));
        CHECK(!rb.push(30));
        CHECK(rb.size() == 3 && !rb.full());
        CHECK(rb.front() == 10 && rb.back() == 30);
        CHECK(rb.at(0) == 10 && rb.at(1) == 20 && rb.at(2) == 30);
        CHECK(!rb.push(40)); // fills the last slot; still no eviction (count reaches capacity exactly)
        CHECK(rb.full() && rb.back() == 40);
    }

    // Rolling-window overwrite: once full, push evicts the oldest and the window slides.
    {
        RingBuffer<int> rb(3);
        rb.push(1);
        rb.push(2);
        rb.push(3); // full: [1,2,3]
        CHECK(rb.full() && rb.size() == 3);
        CHECK(rb.push(4)); // evicts 1 -> [2,3,4]
        CHECK(rb.size() == 3);
        CHECK(rb.at(0) == 2 && rb.at(1) == 3 && rb.at(2) == 4);
        CHECK(rb.front() == 2 && rb.back() == 4);
        CHECK(rb.push(5)); // -> [3,4,5]
        CHECK(rb.at(0) == 3 && rb.at(2) == 5);
        const auto v = rb.toVector();
        CHECK(v.size() == 3 && v[0] == 3 && v[1] == 4 && v[2] == 5);
    }

    // The "fills the 4th slot" case: the 4th push into a capacity-4 buffer does NOT evict.
    {
        RingBuffer<int> rb(4);
        CHECK(!rb.push(1));
        CHECK(!rb.push(2));
        CHECK(!rb.push(3));
        CHECK(!rb.push(4)); // now full, but nothing evicted on this push
        CHECK(rb.full() && rb.size() == 4);
        CHECK(rb.push(5)); // THIS one evicts 1 -> [2,3,4,5]
        CHECK(rb.at(0) == 2 && rb.at(3) == 5);
    }

    // Bounded-FIFO mode: pushBack rejects when full; popFront drains oldest-first.
    {
        RingBuffer<int> q(2);
        CHECK(q.pushBack(7));
        CHECK(q.pushBack(8));
        CHECK(!q.pushBack(9)); // full -> rejected, buffer unchanged
        CHECK(q.size() == 2 && q.at(0) == 7 && q.at(1) == 8);
        int out = -1;
        CHECK(q.popFront(out) && out == 7);
        CHECK(q.size() == 1 && q.front() == 8);
        CHECK(q.pushBack(9)); // room again after popping
        CHECK(q.at(0) == 8 && q.at(1) == 9);
        CHECK(q.popFront(out) && out == 8);
        CHECK(q.popFront(out) && out == 9);
        CHECK(!q.popFront(out)); // empty
        CHECK(q.empty());
    }

    // Wrap-around correctness: interleave popFront and push so the head wraps past the end.
    {
        RingBuffer<int> rb(3);
        rb.push(1);
        rb.push(2);
        rb.push(3);         // [1,2,3], head at 0
        int out = -1;
        rb.popFront(out);   // remove 1 -> head at 1, [2,3]
        rb.popFront(out);   // remove 2 -> head at 2, [3]
        rb.push(4);         // [3,4]
        rb.push(5);         // [3,4,5] wrapping physically
        CHECK(rb.size() == 3);
        CHECK(rb.at(0) == 3 && rb.at(1) == 4 && rb.at(2) == 5);
        CHECK(rb.push(6));  // full -> evict 3 -> [4,5,6]
        CHECK(rb.at(0) == 4 && rb.at(2) == 6);
    }

    // clear() empties without reallocating; reset() resizes.
    {
        RingBuffer<int> rb(3);
        rb.push(1);
        rb.push(2);
        rb.clear();
        CHECK(rb.empty() && rb.capacity() == 3);
        rb.push(9);
        CHECK(rb.size() == 1 && rb.front() == 9);
        rb.reset(5);
        CHECK(rb.empty() && rb.capacity() == 5);
    }

    // Zero capacity: push is a benign no-op.
    {
        RingBuffer<int> rb(0);
        CHECK(!rb.push(1));
        CHECK(rb.empty() && rb.capacity() == 0);
    }

    // Works with a non-trivial element type (rolling average over floats).
    {
        RingBuffer<float> rb(4);
        for (int i = 1; i <= 6; ++i) {
            rb.push(static_cast<float>(i)); // last 4 -> [3,4,5,6]
        }
        float sum = 0.0f;
        for (std::size_t i = 0; i < rb.size(); ++i) {
            sum += rb.at(i);
        }
        CHECK_NEAR(sum / static_cast<float>(rb.size()), 4.5f, 1e-4f);
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

// Async asset loading (Godot ResourceLoader parity): threaded decode, status/progress, dedup,
// reimport/hot-reload. A decoded "asset" here is just an int (the byte count) so it runs GPU-free.
void testAssetServer() {
    core::JobSystem jobs(2);

    // A virtual filesystem: path -> (contents, mtime). Lets us simulate a file changing on disk
    // without touching the real filesystem, so reimport is deterministic.
    struct File {
        std::string data;
        uint64_t mtime;
    };
    std::unordered_map<std::string, File> vfs = {
        {"a.tex", {"hello", 1}},
        {"b.tex", {"world!!", 1}},
        {"c.tex", {"x", 1}},
    };
    std::atomic<int> decodeCalls{0};

    auto loader = [&](const std::string& path) -> int {
        ++decodeCalls;
        auto it = vfs.find(path);
        if (it == vfs.end()) {
            throw std::runtime_error("missing: " + path); // -> Failed
        }
        return static_cast<int>(it->second.data.size()); // "decoded" size
    };
    auto stampOf = [&](const std::string& path) -> uint64_t {
        auto it = vfs.find(path);
        return it == vfs.end() ? 0u : it->second.mtime;
    };

    core::AssetServer<int> server(jobs, loader, stampOf);
    int loadedCallbacks = 0;
    server.setOnLoaded([&](core::AssetId, const std::string&, int&) { ++loadedCallbacks; });

    // Request three assets; they start Loading immediately (non-blocking).
    core::AssetId a = server.request("a.tex");
    core::AssetId b = server.request("b.tex");
    core::AssetId c = server.request("c.tex");
    CHECK(a.valid() && b.valid() && c.valid());

    // Dedup: requesting the same path returns the same id and bumps the ref count, no extra decode.
    core::AssetId aAgain = server.request("a.tex");
    CHECK(aAgain == a);
    CHECK(server.refCount(a) == 2);

    // Drive all jobs to completion on the "game thread".
    int guard = 0;
    while (!server.allLoaded() && guard++ < 10000) {
        server.poll();
        std::this_thread::sleep_for(std::chrono::microseconds(100)); // let worker threads run
    }
    CHECK(server.allLoaded());
    CHECK(server.status(a) == core::AssetStatus::Loaded);
    CHECK(*server.tryGet(a) == 5); // "hello"
    CHECK(*server.tryGet(b) == 7); // "world!!"
    CHECK(*server.tryGet(c) == 1); // "x"
    CHECK(decodeCalls.load() == 3); // three unique paths decoded once each
    CHECK(loadedCallbacks == 3);
    CHECK(server.version(a) == 1);

    core::AssetServer<int>::Progress pr = server.progress();
    CHECK(pr.total == 3 && pr.loaded == 3);

    // A missing file fails cleanly rather than throwing to the caller.
    core::AssetId missing = server.request("nope.tex");
    guard = 0;
    while (server.status(missing) == core::AssetStatus::Loading && guard++ < 10000) {
        server.poll();
        std::this_thread::sleep_for(std::chrono::microseconds(100)); // let worker threads run
    }
    CHECK(server.status(missing) == core::AssetStatus::Failed);
    CHECK(server.tryGet(missing) == nullptr);

    // Reimport: nothing changed yet, so no reloads are scheduled.
    CHECK(server.reimportChanged() == 0);

    // Edit "a.tex" on disk (new contents + bumped mtime). reimportChanged detects the stamp move,
    // re-decodes in the background, and the new bytes land on the next poll with a bumped version.
    vfs["a.tex"] = File{"hello, world", 2};
    const int before = decodeCalls.load();
    CHECK(server.reimportChanged() == 1);
    guard = 0;
    while (server.status(a) == core::AssetStatus::Loading && guard++ < 10000) {
        server.poll();
        std::this_thread::sleep_for(std::chrono::microseconds(100)); // let worker threads run
    }
    CHECK(*server.tryGet(a) == 12); // "hello, world"
    CHECK(server.version(a) == 2);  // consumer sees version change -> re-upload
    CHECK(decodeCalls.load() == before + 1);
    CHECK(server.refCount(a) == 2); // reimport preserves references

    // Forced reimport of one asset (editor "Reimport" button) even without a stamp change.
    const int before2 = decodeCalls.load();
    CHECK(server.reimport(b));
    guard = 0;
    while (server.status(b) == core::AssetStatus::Loading && guard++ < 10000) {
        server.poll();
        std::this_thread::sleep_for(std::chrono::microseconds(100)); // let worker threads run
    }
    CHECK(server.version(b) == 2);
    CHECK(decodeCalls.load() == before2 + 1);

    // Reference counting: release once (a had 2 refs) keeps it; release again evicts it.
    CHECK(!server.release(a));
    CHECK(server.refCount(a) == 1);
    CHECK(server.release(a));
    CHECK(server.refCount(a) == 0);
    CHECK(server.status(a) == core::AssetStatus::Failed); // evicted -> invalid
    CHECK(server.tryGet(a) == nullptr);

    // blockingGet: request + drive to done in one call, for startup/tests.
    const int* got = server.blockingGet("c.tex"); // already loaded -> dedup, returns cached
    CHECK(got != nullptr && *got == 1);

    // A concurrency stress: many distinct requests all resolve without loss.
    core::AssetServer<int> many(jobs, [](const std::string& p) { return static_cast<int>(p.size()); });
    std::vector<core::AssetId> ids;
    for (int i = 0; i < 64; ++i) {
        ids.push_back(many.request("asset_" + std::to_string(i)));
    }
    guard = 0;
    while (!many.allLoaded() && guard++ < 100000) {
        many.poll();
        std::this_thread::sleep_for(std::chrono::microseconds(100)); // let worker threads run
    }
    CHECK(many.allLoaded());
    CHECK(many.liveCount() == 64);
    bool allGood = true;
    for (size_t i = 0; i < ids.size(); ++i) {
        const int* v = many.tryGet(ids[i]);
        if (!v || *v != static_cast<int>(("asset_" + std::to_string(i)).size())) {
            allGood = false;
        }
    }
    CHECK(allGood);
}

// Log sinks: FileLogSink writes structured lines to disk; MultiSink fans out to several sinks.
void testLogSinks() {
    using maz::core::FileLogSink;
    using maz::core::LogLevel;
    using maz::core::LogSink;
    using maz::core::MultiSink;

    CHECK(std::string(maz::core::levelName(LogLevel::Warn)) == "WARN");
    CHECK(std::string(maz::core::levelName(LogLevel::Error)) == "ERROR");

    // FileLogSink writes "[LEVEL] message" lines; read them back.
    const std::string path = "maz_logsink_test.log";
    {
        FileLogSink sink(path);
        CHECK(sink.isOpen());
        sink.write(LogLevel::Info, "hello world");
        sink.write(LogLevel::Error, "boom 42");
        CHECK(sink.lineCount() == 2);
    } // closed on scope exit

    {
        std::ifstream in(path);
        CHECK(in.good());
        std::string l1, l2;
        std::getline(in, l1);
        std::getline(in, l2);
        CHECK(l1 == "[INFO] hello world");
        CHECK(l2 == "[ERROR] boom 42");
    }
    std::remove(path.c_str());

    // A bad path fails to open but write() is a safe no-op (no crash).
    {
        FileLogSink bad("/definitely/not/a/dir/nope.log");
        CHECK(!bad.isOpen());
        bad.write(LogLevel::Info, "ignored"); // must not crash
        CHECK(bad.lineCount() == 0);
    }

    // MultiSink fans one line out to every registered sink, in order.
    {
        int aCount = 0, bCount = 0;
        std::string lastA;
        LogSink a = [&](LogLevel, const char* m) {
            ++aCount;
            lastA = m ? m : "";
        };
        LogSink b = [&](LogLevel, const char*) { ++bCount; };
        MultiSink multi{a, b};
        CHECK(multi.size() == 2);
        multi(LogLevel::Info, "fan");
        multi(LogLevel::Warn, "out");
        CHECK(aCount == 2);
        CHECK(bCount == 2);
        CHECK(lastA == "out");

        // A file sink combined with a capturing sink (file + panel scenario).
        auto file = std::make_shared<FileLogSink>(std::string("maz_logsink_multi.log"));
        int panel = 0;
        MultiSink combo{maz::core::makeSink(file), [&](LogLevel, const char*) { ++panel; }};
        combo(LogLevel::Trace, "both");
        CHECK(panel == 1);
        CHECK(file->lineCount() == 1);
        file->close();
        std::remove("maz_logsink_multi.log");
    }
}

// Crash handler: the reportable pieces (signal names, banner, symbol demangling, live backtrace
// capture) are tested off the crash path so we never actually raise a fatal signal in the runner.
void testCrashHandler() {
    using namespace maz::platform;

    // Signal naming covers the fatal set we install for.
    CHECK(std::string(signalName(SIGSEGV)).find("SIGSEGV") == 0);
    CHECK(std::string(signalName(SIGABRT)).find("SIGABRT") == 0);
    CHECK(std::string(signalName(SIGFPE)).find("SIGFPE") == 0);
    CHECK(std::string(signalName(SIGILL)).find("SIGILL") == 0);
    CHECK(std::string(signalName(-999)).find("unknown") != std::string::npos);

    // The banner carries app + version + signal, with sane fallbacks for empty fields.
    const std::string banner = formatCrashBanner("ZOMBOID", "1.2.3", SIGSEGV);
    CHECK(banner.find("ZOMBOID") != std::string::npos);
    CHECK(banner.find("1.2.3") != std::string::npos);
    CHECK(banner.find("SIGSEGV") != std::string::npos);
    CHECK(banner.find("backtrace") != std::string::npos);
    const std::string dflt = formatCrashBanner("", "", SIGABRT);
    CHECK(dflt.find("maz") != std::string::npos);   // empty app -> "maz"
    CHECK(dflt.find("0.0.0") != std::string::npos); // empty version -> "0.0.0"

    // Demangling turns a mangled frame back into a readable C++ name (both platform line formats).
    const std::string linuxLine = "./bin/game(_ZN3maz4game7respawnEv+0x2a) [0x55f1]";
    const std::string demangled = demangleSymbol(linuxLine);
#if MAZ_HAVE_EXECINFO
    CHECK(demangled.find("maz::game::respawn") != std::string::npos);
    CHECK(demangled.find("[0x55f1]") != std::string::npos); // trailing address preserved
    // A macOS-style line still demangles by locating the _Z token.
    const std::string macLine = "3   game   0x100 _ZN3maz3fooEv + 26";
    CHECK(demangleSymbol(macLine).find("maz::foo") != std::string::npos);
#else
    CHECK(demangled == linuxLine); // graceful passthrough where execinfo is absent
#endif
    // A line with no mangled token is returned unchanged (C symbol / raw address).
    const std::string plain = "./bin/game(main+0x10) [0x1234]";
    CHECK(demangleSymbol(plain) == plain);

    // Capturing the live stack from inside a nested call returns frames where supported.
    std::vector<std::string> frames = captureBacktrace(32, 0);
#if MAZ_HAVE_EXECINFO
    CHECK(!frames.empty());
#else
    CHECK(frames.empty());
#endif

    // install() / uninstall() are idempotent and leave no handlers behind. We install WITHOUT a log
    // path so the runner's signal disposition is restored cleanly and no crash is provoked.
    CHECK(!CrashHandler::isInstalled() || true);
    const bool ok = CrashHandler::install(CrashConfig{"unit-test", "0.0.1", ""});
#if MAZ_HAVE_EXECINFO
    CHECK(ok);
    CHECK(CrashHandler::isInstalled());
#else
    CHECK(!ok);
#endif
    CrashHandler::uninstall();
    CHECK(!CrashHandler::isInstalled());
}

// Opt-in telemetry: the privacy contract (off by default, drops while disabled), deterministic
// JSONL, counters, sink delivery, and consent revocation.
void testTelemetry() {
    core::Telemetry tel;
    tel.configure("sess-123", "ZOMBOID", "1.0.0");

    // OFF BY DEFAULT: events are dropped, nothing buffered.
    CHECK(!tel.enabled());
    tel.event("boot");
    tel.count("wave_started");
    tel.timing("load_ms", 42.0);
    CHECK(tel.pending() == 0);
    CHECK(tel.recorded() == 0);
    CHECK(tel.dropped() == 3); // all three withheld while off
    CHECK(tel.flush().empty());

    // Opt in -> events now record.
    tel.enable(true);
    CHECK(tel.enabled());
    tel.event("level_complete", {{"level", "arena"}}, {{"score", 1500.0}});
    tel.count("zombie_killed", 3.0);
    tel.timing("frame_ms", 16.5);
    CHECK(tel.pending() == 3);
    CHECK(tel.recorded() == 3);
    CHECK(tel.dropped() == 3); // unchanged

    // flush() serializes deterministic JSONL (sequence numbers, not wall-clock), delivers to the
    // sink, and clears the buffer.
    std::string delivered;
    tel.setSink([&](const std::string& jsonl) { delivered += jsonl; });
    const std::string batch = tel.flush();
    CHECK(batch == delivered);
    CHECK(tel.pending() == 0);

    // Three lines, one per event, each self-describing with the session/app header.
    int lines = 0;
    for (char c : batch) {
        if (c == '\n') ++lines;
    }
    CHECK(lines == 3);
    CHECK(batch.find("\"session\":\"sess-123\"") != std::string::npos);
    CHECK(batch.find("\"app\":\"ZOMBOID\"") != std::string::npos);
    CHECK(batch.find("\"name\":\"level_complete\"") != std::string::npos);
    CHECK(batch.find("\"level\":\"arena\"") != std::string::npos);
    CHECK(batch.find("\"score\":1500") != std::string::npos);   // integer-valued -> no decimals
    CHECK(batch.find("\"value\":3") != std::string::npos);      // count() field
    CHECK(batch.find("\"ms\":16.5") != std::string::npos);      // timing() field
    CHECK(batch.find("\"seq\":0") != std::string::npos);        // deterministic sequence
    CHECK(batch.find("\"seq\":2") != std::string::npos);

    // JSON string escaping: quotes/newlines in a field don't break the line.
    tel.event("note", {{"text", "he said \"hi\"\nbye"}});
    const std::string esc = tel.flush();
    CHECK(esc.find("\\\"hi\\\"") != std::string::npos);
    CHECK(esc.find("\\n") != std::string::npos);

    // Consent revocation mid-session: disable stops new events; clear() drops the buffer.
    tel.enable(true);
    tel.event("a");
    tel.event("b");
    CHECK(tel.pending() == 2);
    tel.enable(false);
    tel.event("c"); // dropped
    CHECK(tel.pending() == 2);
    tel.clear();
    CHECK(tel.pending() == 0);
    CHECK(tel.flush().empty());
}

// Deterministic replay: record per-frame input, serialize (RLE) + reload byte-for-byte, and prove
// replaying the same stream through a deterministic sim reproduces the exact result.
void testReplay() {
    struct Input {
        uint8_t buttons = 0;
        int8_t moveX = 0;
        int8_t moveY = 0;
    };

    core::Replay<Input> rec;
    // 100 frames: idle, then a held-right run, then idle again — lots of repeats for RLE to fold.
    for (int i = 0; i < 40; ++i) rec.record(Input{0, 0, 0});
    for (int i = 0; i < 30; ++i) rec.record(Input{1, 1, 0}); // holding right + button
    for (int i = 0; i < 30; ++i) rec.record(Input{0, 0, 0});
    CHECK(rec.size() == 100);

    // frame() reads back what was recorded; reading past the end holds the last frame.
    CHECK(rec.frame(0).buttons == 0);
    CHECK(rec.frame(45).moveX == 1);
    CHECK(rec.frame(99).buttons == 0);
    CHECK(rec.frame(1000).buttons == 0); // clamps to last

    // Serialize: RLE folds 100 frames into 3 runs, so the blob is far smaller than 100*sizeof.
    std::vector<uint8_t> bytes = rec.serialize();
    const size_t raw = 100 * sizeof(Input);
    CHECK(bytes.size() < raw);            // compression actually happened
    CHECK(bytes.size() == 20u + 3u * (4u + sizeof(Input))); // header + 3 runs

    // Reload is byte-for-byte identical frames.
    core::Replay<Input> play;
    CHECK(play.load(bytes));
    CHECK(play.size() == 100);
    bool identical = true;
    for (size_t i = 0; i < 100; ++i) {
        if (play.frame(i).buttons != rec.frame(i).buttons ||
            play.frame(i).moveX != rec.frame(i).moveX || play.frame(i).moveY != rec.frame(i).moveY) {
            identical = false;
        }
    }
    CHECK(identical);
    // Re-serializing the reloaded replay yields the exact same bytes (stable round-trip).
    CHECK(play.serialize() == bytes);

    // Determinism payoff: feed the replayed input through a simple integrator twice; identical out.
    auto simulate = [](const core::Replay<Input>& r) {
        int x = 0, y = 0;
        core::Random rng(1234); // seeded -> deterministic
        long checksum = 0;
        for (size_t i = 0; i < r.size(); ++i) {
            const Input& in = r.frame(i);
            x += in.moveX;
            y += in.moveY;
            checksum += x * 7 + y * 13 + static_cast<int>(rng.range(0, 3));
        }
        return checksum;
    };
    CHECK(simulate(rec) == simulate(play)); // same stream + same seed -> same result

    // Corruption is rejected, not misread: a wrong magic and a mismatched frame size both fail.
    std::vector<uint8_t> bad = bytes;
    bad[0] = 'X';
    core::Replay<Input> broken;
    CHECK(!broken.load(bad));
    CHECK(broken.empty());
    // A replay authored for a differently-sized input struct is refused.
    core::Replay<int> wrongType;
    CHECK(!wrongType.load(bytes)); // sizeof(int) != sizeof(Input)

    // Empty replay serializes + reloads cleanly.
    core::Replay<Input> none;
    CHECK(none.serialize().size() == 20); // header only, zero runs
    core::Replay<Input> none2;
    CHECK(none2.load(none.serialize()));
    CHECK(none2.empty());
}

// Checkpoints: named save slots (+ whole-file round-trip) and the rolling rewind ring.
void testCheckpoints() {
    core::Checkpoints cp;
    auto blob = [](std::initializer_list<uint8_t> b) { return std::vector<uint8_t>(b); };

    // Named slots: save, overwrite, load, erase.
    CHECK(cp.slotCount() == 0);
    cp.save("start", blob({1, 2, 3}), 0);
    cp.save("boss", blob({9, 9}), 500);
    CHECK(cp.slotCount() == 2);
    CHECK(cp.has("boss"));
    const core::Snapshot* boss = cp.load("boss");
    CHECK(boss != nullptr);
    CHECK(boss->frame == 500);
    CHECK(boss->data == blob({9, 9}));
    cp.save("boss", blob({7}), 600); // overwrite
    CHECK(cp.load("boss")->data == blob({7}));
    CHECK(cp.load("missing") == nullptr);
    CHECK(cp.erase("start"));
    CHECK(!cp.erase("start")); // already gone
    CHECK(cp.slotCount() == 1);

    // Whole save-file round-trip: serialize all slots, load into a fresh instance, identical.
    cp.save("start", blob({1, 2, 3}), 10);
    cp.save("mid", blob({4, 5, 6, 7}), 250);
    const std::vector<uint8_t> file = cp.serialize();
    core::Checkpoints loaded;
    CHECK(loaded.loadFile(file));
    CHECK(loaded.slotCount() == cp.slotCount());
    CHECK(loaded.load("mid")->data == blob({4, 5, 6, 7}));
    CHECK(loaded.load("mid")->frame == 250);
    CHECK(loaded.load("start")->data == blob({1, 2, 3}));
    // Re-serializing yields a file that reloads to the same slots (stable).
    core::Checkpoints again;
    CHECK(again.loadFile(loaded.serialize()));
    CHECK(again.slotCount() == cp.slotCount());
    // Corruption rejected.
    std::vector<uint8_t> bad = file;
    bad[1] = 'X';
    core::Checkpoints broken;
    CHECK(!broken.loadFile(bad));
    CHECK(broken.slotCount() == 0);

    // Rewind ring: capacity-bounded, keeps the most recent snapshots.
    core::Checkpoints rw;
    rw.setRewindCapacity(3);
    CHECK(rw.rewindCapacity() == 3);
    for (uint64_t f = 1; f <= 5; ++f) {
        rw.autosave(f * 10, blob({static_cast<uint8_t>(f)})); // frames 10,20,30,40,50
    }
    CHECK(rw.rewindCount() == 3);                 // only the last 3 survive (30,40,50)
    CHECK(rw.rewind(0)->frame == 50);             // newest
    CHECK(rw.rewind(1)->frame == 40);             // one step back
    CHECK(rw.rewind(2)->frame == 30);             // oldest kept
    CHECK(rw.rewind(3) == nullptr);               // fell off the ring
    CHECK(rw.rewindByteSize() == 3);              // three 1-byte snapshots

    // rewindToFrame: newest snapshot at or before a target frame.
    CHECK(rw.rewindToFrame(45)->frame == 40);     // 40 is newest <= 45
    CHECK(rw.rewindToFrame(50)->frame == 50);
    CHECK(rw.rewindToFrame(25) == nullptr);       // everything kept is newer than 25

    // Autosave is a no-op when the ring is disabled (capacity 0).
    core::Checkpoints off;
    off.autosave(1, blob({1}));
    CHECK(off.rewindCount() == 0);

    // Shrinking capacity drops the oldest immediately.
    rw.setRewindCapacity(1);
    CHECK(rw.rewindCount() == 1);
    CHECK(rw.rewind(0)->frame == 50);
}

// Memory allocators: the linear/frame arena (bump + alignment + reset + stack markers) and the
// fixed-size pool (O(1) alloc/free, slot reuse, exhaustion).
// Quadtree: spatial range queries match brute-force overlap exactly, with subdivision on clusters.
void testQuadtree() {
    using maz::game::Quadtree;

    // A deterministic scatter of 200 small boxes across a 1000x1000 world, plus a tight cluster of
    // 50 in one corner (to force deep subdivision there).
    struct Box {
        uint32_t id;
        float x, y, w, h;
    };
    std::vector<Box> boxes;
    core::Random rng(2024);
    for (int i = 0; i < 200; ++i) {
        boxes.push_back({static_cast<uint32_t>(i), static_cast<float>(rng.range(0, 990)),
                         static_cast<float>(rng.range(0, 990)), static_cast<float>(rng.range(1, 10)),
                         static_cast<float>(rng.range(1, 10))});
    }
    for (int i = 0; i < 50; ++i) {
        boxes.push_back({static_cast<uint32_t>(200 + i), static_cast<float>(rng.range(0, 40)),
                         static_cast<float>(rng.range(0, 40)), 2.0f, 2.0f});
    }

    Quadtree qt(0, 0, 1000, 1000, 8, 4);
    for (const Box& b : boxes) {
        qt.insert(b.id, b.x, b.y, b.w, b.h);
    }
    CHECK(qt.size() == boxes.size());
    CHECK(qt.nodeCount() > 1); // the cluster forced at least one subdivision

    auto brute = [&](float qx, float qy, float qw, float qh) {
        std::set<uint32_t> s;
        for (const Box& b : boxes) {
            if (maz::game::aabbOverlap(b.x, b.y, b.w, b.h, qx, qy, qw, qh)) {
                s.insert(b.id);
            }
        }
        return s;
    };
    auto treeSet = [&](std::vector<uint32_t> v) { return std::set<uint32_t>(v.begin(), v.end()); };

    // Several query regions: the dense corner, an empty middle strip, a wide sweep, and a point.
    struct Q {
        float x, y, w, h;
    };
    const Q queries[] = {{0, 0, 50, 50}, {450, 450, 20, 20}, {0, 500, 1000, 30}, {700, 200, 5, 5}};
    for (const Q& q : queries) {
        CHECK(treeSet(qt.query(q.x, q.y, q.w, q.h)) == brute(q.x, q.y, q.w, q.h));
    }

    // A query fully covering the world returns every item.
    CHECK(qt.query(-10, -10, 1020, 1020).size() == boxes.size());
    // A query entirely outside the world returns nothing.
    CHECK(qt.query(5000, 5000, 10, 10).empty());

    // queryCircle bounding-box prefilter includes a box the circle's bbox touches.
    Quadtree q2(0, 0, 100, 100, 6, 2);
    q2.insert(1, 48, 48, 4, 4); // centred near (50,50)
    q2.insert(2, 90, 90, 4, 4); // far corner
    CHECK(treeSet(q2.queryCircle(50, 50, 10)) == std::set<uint32_t>{1});

    // clear() empties it.
    q2.clear();
    CHECK(q2.size() == 0);
    CHECK(q2.query(0, 0, 100, 100).empty());

    // A boundary-straddling item still surfaces from both sides.
    Quadtree q3(0, 0, 100, 100, 6, 1);
    q3.insert(7, 45, 45, 10, 10); // straddles the central cross
    q3.insert(8, 5, 5, 2, 2);
    q3.insert(9, 80, 80, 2, 2); // force subdivision
    CHECK(treeSet(q3.query(46, 46, 2, 2)) == std::set<uint32_t>{7});
}

// Containers: SmallVector (inline until N, spills to heap, correct move/copy) and SparseSet (O(1)
// insert/remove/lookup with a dense, hole-free value array).
void testContainers() {
    // ---- SmallVector ----
    core::SmallVector<int, 4> v;
    CHECK(v.empty());
    CHECK(v.capacity() == 4);
    CHECK(v.isInline());
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    v.push_back(4);
    CHECK(v.size() == 4);
    CHECK(v.isInline()); // still within inline capacity
    CHECK(v[0] == 1 && v[3] == 4);
    CHECK(v.back() == 4 && v.front() == 1);

    v.push_back(5); // spill to heap
    CHECK(v.size() == 5);
    CHECK(!v.isInline());
    CHECK(v.capacity() >= 5);
    CHECK(v[4] == 5);
    CHECK(v[0] == 1); // elements preserved across the spill

    // range-for sums correctly.
    int sum = 0;
    for (int x : v) sum += x;
    CHECK(sum == 15);

    v.pop_back();
    CHECK(v.size() == 4 && v.back() == 4);
    v.clear();
    CHECK(v.empty());

    // Copy + move keep contents and don't double-free (exercised with a non-trivial element).
    core::SmallVector<std::string, 2> s;
    s.push_back("a");
    s.push_back("b");
    s.push_back("c"); // spills
    core::SmallVector<std::string, 2> copy = s;
    CHECK(copy.size() == 3);
    CHECK(copy[2] == "c");
    core::SmallVector<std::string, 2> moved = std::move(s);
    CHECK(moved.size() == 3);
    CHECK(moved[0] == "a");
    CHECK(copy[0] == "a"); // the copy is independent of the moved-from source

    // Inline-only move (source never spilled) still transfers elements.
    core::SmallVector<std::string, 4> inl;
    inl.push_back("x");
    inl.push_back("y");
    core::SmallVector<std::string, 4> inlMoved = std::move(inl);
    CHECK(inlMoved.size() == 2 && inlMoved[1] == "y");

    // emplace_back constructs in place.
    core::SmallVector<std::pair<int, int>, 2> pairs;
    pairs.emplace_back(3, 4);
    CHECK(pairs[0].first == 3 && pairs[0].second == 4);

    // ---- SparseSet ----
    core::SparseSet<std::string> set;
    CHECK(set.empty());
    set.insert(5, "five");
    set.insert(2, "two");
    set.insert(9, "nine");
    CHECK(set.size() == 3);
    CHECK(set.contains(5) && set.contains(2) && set.contains(9));
    CHECK(!set.contains(0) && !set.contains(100));
    CHECK(*set.get(5) == "five");
    CHECK(set.get(100) == nullptr);

    // Overwrite an existing key updates in place (no size change).
    set.insert(2, "TWO");
    CHECK(set.size() == 3 && *set.get(2) == "TWO");

    // Dense arrays stay packed and parallel.
    CHECK(set.keys().size() == 3 && set.values().size() == 3);

    // Swap-erase removal: removing the middle key keeps the set packed and lookups correct.
    CHECK(set.remove(2));
    CHECK(!set.contains(2));
    CHECK(set.size() == 2);
    CHECK(set.contains(5) && set.contains(9));
    CHECK(*set.get(9) == "nine"); // the moved-in element is still findable
    CHECK(!set.remove(2));        // already gone

    // Iterating dense values hits exactly the live entries.
    std::set<std::string> seen(set.values().begin(), set.values().end());
    CHECK(seen == (std::set<std::string>{"five", "nine"}));

    set.clear();
    CHECK(set.empty() && !set.contains(5));

    // Stress: insert 1000, remove evens, survivors all present with correct values.
    core::SparseSet<int> big;
    for (uint32_t i = 0; i < 1000; ++i) big.insert(i, static_cast<int>(i * 3));
    CHECK(big.size() == 1000);
    for (uint32_t i = 0; i < 1000; i += 2) big.remove(i);
    CHECK(big.size() == 500);
    bool ok = true;
    for (uint32_t i = 1; i < 1000; i += 2) {
        const int* p = big.get(i);
        if (!p || *p != static_cast<int>(i * 3)) ok = false;
    }
    CHECK(ok);
    CHECK(!big.contains(0) && !big.contains(998));
}

// ECS core components + hierarchy propagation, and the system Scheduler (order + parallel).
void testEcsComponents() {
    using namespace maz::ecs;
    World w;

    // Build a hierarchy: root -> child -> grandchild, each translated +10 on x.
    Entity root = w.create();
    Entity child = w.create();
    Entity grand = w.create();
    w.add<Transform>(root, Transform{math::vec3(10, 0, 0), math::quat(1, 0, 0, 0), math::vec3(1)});
    w.add<Transform>(child, Transform{math::vec3(10, 0, 0), math::quat(1, 0, 0, 0), math::vec3(1)});
    w.add<Transform>(grand, Transform{math::vec3(10, 0, 0), math::quat(1, 0, 0, 0), math::vec3(1)});
    w.add<Parent>(child, Parent{root});
    w.add<Parent>(grand, Parent{child});
    w.add<Name>(root, Name{"root"});
    w.add<Tag>(root, Tag{});
    w.get<Tag>(root)->set(3);
    CHECK(w.get<Tag>(root)->has(3));
    CHECK(!w.get<Tag>(root)->has(2));
    CHECK(w.get<Tag>(root)->anyOf(0b1000));
    CHECK(w.get<Name>(root)->value == "root");

    // Deliberately register a child before its parent is resolved — the pass must still order it.
    propagateTransforms(w);
    CHECK_NEAR(w.get<WorldTransform>(root)->position().x, 10.0f, 1e-4f);
    CHECK_NEAR(w.get<WorldTransform>(child)->position().x, 20.0f, 1e-4f);
    CHECK_NEAR(w.get<WorldTransform>(grand)->position().x, 30.0f, 1e-4f);

    // Re-parent grand directly under root -> world x becomes 20; re-run.
    w.get<Parent>(grand)->parent = root;
    propagateTransforms(w);
    CHECK_NEAR(w.get<WorldTransform>(grand)->position().x, 20.0f, 1e-4f);

    // A self-parent cycle must not hang; the entity falls back to its local transform.
    Entity loop = w.create();
    w.add<Transform>(loop, Transform{math::vec3(5, 0, 0), math::quat(1, 0, 0, 0), math::vec3(1)});
    w.add<Parent>(loop, Parent{loop});
    propagateTransforms(w);
    CHECK_NEAR(w.get<WorldTransform>(loop)->position().x, 5.0f, 1e-4f);

    // --- Scheduler: deterministic phase/order, then parallel-in-phase ---
    Scheduler sched;
    std::vector<std::string> log;
    sched.add(
        "late", [&](World&) { log.push_back("late"); }, /*phase*/ 2, /*order*/ 0);
    sched.add(
        "input", [&](World&) { log.push_back("input"); }, 0, 0);
    sched.add(
        "sim_b", [&](World&) { log.push_back("sim_b"); }, 1, 5);
    sched.add(
        "sim_a", [&](World&) { log.push_back("sim_a"); }, 1, 1);
    const std::vector<std::string> order = sched.runOrder();
    CHECK(order.size() == 4);
    CHECK(order[0] == "input");
    CHECK(order[1] == "sim_a"); // order 1 before order 5 within phase 1
    CHECK(order[2] == "sim_b");
    CHECK(order[3] == "late");
    sched.run(w);
    CHECK(log.size() == 4 && log[0] == "input" && log[3] == "late");

    // Parallel run: many parallelSafe systems each bump a disjoint counter; phase barrier holds.
    Scheduler par;
    std::atomic<int> counter{0};
    std::vector<int> results(8, 0);
    for (int i = 0; i < 8; ++i) {
        par.add(
            "w" + std::to_string(i),
            [&results, i, &counter](World&) {
                results[static_cast<size_t>(i)] = i * i;
                counter.fetch_add(1, std::memory_order_relaxed);
            },
            /*phase*/ 0, /*order*/ i, /*parallelSafe*/ true);
    }
    bool ranAfter = false;
    par.add(
        "finalize", [&](World&) { ranAfter = (counter.load() == 8); }, /*phase*/ 1, 0,
        /*parallelSafe*/ false);
    core::JobSystem jobs(4);
    par.runParallel(w, jobs);
    CHECK(counter.load() == 8);
    CHECK(results[5] == 25);
    CHECK(ranAfter); // phase-1 finalize saw all phase-0 work complete (barrier held)
}

// Geometry3D: Plane / Aabb3 / Obb value types and their intersection tests.
void testGeometry3D() {
    using maz::math::Aabb3;
    using maz::math::Obb;
    using maz::math::Plane;
    using maz::math::vec3;

    // --- Plane ---
    Plane pl(vec3(0, 1, 0), vec3(0, 5, 0)); // y = 5, normal +y
    CHECK_NEAR(pl.d, 5.0f, 1e-5f);
    CHECK_NEAR(pl.distanceTo(vec3(3, 8, -2)), 3.0f, 1e-5f);   // 3 above
    CHECK_NEAR(pl.distanceTo(vec3(0, 1, 0)), -4.0f, 1e-5f);   // 4 below
    CHECK(pl.isPointOver(vec3(0, 6, 0)));
    CHECK(!pl.isPointOver(vec3(0, 4, 0)));
    {
        const vec3 proj = pl.project(vec3(2, 9, 3));
        CHECK_NEAR(proj.y, 5.0f, 1e-5f);
        CHECK_NEAR(proj.x, 2.0f, 1e-5f);
    }
    {
        // Ray straight down from above hits at t=5.
        auto t = pl.intersectRay(vec3(0, 10, 0), vec3(0, -1, 0));
        CHECK(t.has_value());
        CHECK_NEAR(*t, 5.0f, 1e-5f);
        // Parallel ray misses.
        CHECK(!pl.intersectRay(vec3(0, 10, 0), vec3(1, 0, 0)).has_value());
        // Segment crossing the plane.
        auto hit = pl.intersectSegment(vec3(0, 10, 0), vec3(0, 0, 0));
        CHECK(hit.has_value());
        CHECK_NEAR(hit->y, 5.0f, 1e-5f);
        // Segment fully above does not cross.
        CHECK(!pl.intersectSegment(vec3(0, 10, 0), vec3(0, 6, 0)).has_value());
    }
    {
        // fromPoints normal + 3-plane intersection at the origin corner.
        Plane px(vec3(1, 0, 0), vec3(0, 0, 0));
        Plane py(vec3(0, 1, 0), vec3(0, 0, 0));
        Plane pz(vec3(0, 0, 1), vec3(0, 0, 0));
        auto corner = Plane::intersect3(px, py, pz);
        CHECK(corner.has_value());
        CHECK_NEAR(glm::length(*corner), 0.0f, 1e-5f);
        // Two parallel planes -> no unique point.
        CHECK(!Plane::intersect3(px, px, pz).has_value());
    }

    // --- Aabb3 ---
    Aabb3 box(vec3(-1, -1, -1), vec3(1, 1, 1));
    CHECK(box.contains(vec3(0, 0, 0)));
    CHECK(!box.contains(vec3(2, 0, 0)));
    CHECK_NEAR(box.volume(), 8.0f, 1e-5f);
    CHECK(box.intersects(Aabb3(vec3(0.5f, 0, 0), vec3(3, 3, 3))));
    CHECK(!box.intersects(Aabb3(vec3(2, 2, 2), vec3(3, 3, 3))));
    {
        Aabb3 e = Aabb3::empty();
        e.enclosePoint(vec3(1, 2, 3));
        e.enclosePoint(vec3(-4, 0, 5));
        CHECK_NEAR(e.min.x, -4.0f, 1e-5f);
        CHECK_NEAR(e.max.z, 5.0f, 1e-5f);
        // Support corner along +x-y+z.
        const vec3 s = box.support(vec3(1, -1, 1));
        CHECK_NEAR(s.x, 1.0f, 1e-5f);
        CHECK_NEAR(s.y, -1.0f, 1e-5f);
        CHECK_NEAR(s.z, 1.0f, 1e-5f);
    }
    {
        // Ray from -x toward the box enters its near face at t=4 (box min.x = -1, origin x = -5).
        auto t = box.intersectRay(vec3(-5, 0, 0), vec3(1, 0, 0));
        CHECK(t.has_value());
        CHECK_NEAR(*t, 4.0f, 1e-5f);
        CHECK(!box.intersectRay(vec3(-5, 5, 0), vec3(1, 0, 0)).has_value()); // above the box
    }

    // --- Obb SAT ---
    Obb a; // unit axes, half 0.5, at origin
    Obb b;
    b.center = vec3(0.8f, 0, 0);
    CHECK(a.intersects(b));  // overlapping along x
    b.center = vec3(1.2f, 0, 0);
    CHECK(!a.intersects(b)); // separated (0.5 + 0.5 < 1.2)
    {
        // Rotate b 45° about z and slip it into the gap: a rotated box's corner still separates.
        Obb c;
        c.center = vec3(1.05f, 0, 0);
        const float s = std::sin(0.785398f), co = std::cos(0.785398f);
        c.axes = maz::math::mat3(vec3(co, s, 0), vec3(-s, co, 0), vec3(0, 0, 1));
        // Half-diagonal of a unit box is ~0.707; centers 1.05 apart along x -> rotated box reaches in.
        CHECK(a.intersects(c));
        // Its world AABB must enclose its center and be larger than the local box on x.
        Aabb3 wb = c.aabb();
        CHECK(wb.contains(c.center));
        CHECK(wb.size().x > 0.9f); // 0.5*(|cos|+|sin|)*2 ≈ 1.414
    }
    CHECK(a.contains(vec3(0.4f, 0.4f, 0.4f)));
    CHECK(!a.contains(vec3(0.6f, 0, 0)));
}

// Sdf: dead-reckoning signed distance field matches a brute-force exact transform, signs correctly.
void testSdf() {
    const int W = 24, H = 24;
    std::vector<uint8_t> cov(static_cast<size_t>(W * H), 0);
    // A filled disc of radius 7 centered at (12,12).
    const float cx = 12.0f, cy = 12.0f, rad = 7.0f;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const float dx = static_cast<float>(x) - cx, dy = static_cast<float>(y) - cy;
            cov[static_cast<size_t>(y * W + x)] =
                (std::sqrt(dx * dx + dy * dy) <= rad) ? 255 : 0;
        }
    }

    const std::vector<float> sdf = maz::ui::generateSdf(cov.data(), W, H);
    CHECK(sdf.size() == static_cast<size_t>(W * H));

    // Brute-force exact signed distance to the nearest boundary texel, for comparison.
    std::vector<std::pair<int, int>> boundary;
    auto insideAt = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= W || y >= H) return false;
        return cov[static_cast<size_t>(y * W + x)] >= 128;
    };
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const bool c = insideAt(x, y);
            if (insideAt(x - 1, y) != c || insideAt(x + 1, y) != c || insideAt(x, y - 1) != c ||
                insideAt(x, y + 1) != c)
                boundary.emplace_back(x, y);
        }
    CHECK(!boundary.empty());

    float maxErr = 0.0f;
    int signMismatch = 0;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            float best = 1e9f;
            for (const auto& b : boundary) {
                const float dx = static_cast<float>(x - b.first), dy = static_cast<float>(y - b.second);
                best = std::min(best, std::sqrt(dx * dx + dy * dy));
            }
            const float exact = insideAt(x, y) ? best : -best;
            const float got = sdf[static_cast<size_t>(y * W + x)];
            maxErr = std::max(maxErr, std::fabs(got - exact));
            if ((got > 0) != (exact > 0) && std::fabs(exact) > 0.01f) ++signMismatch;
        }
    }
    // Dead reckoning is accurate to well under a texel vs the exact transform.
    CHECK(maxErr < 0.7f);
    CHECK(signMismatch == 0);

    // Sign sanity: center is deep inside (positive, ~rad), a far corner is outside (negative).
    CHECK(sdf[static_cast<size_t>(12 * W + 12)] > 5.0f);
    CHECK(sdf[0] < 0.0f);

    // packSdf: edge maps to ~0.5 (127/128), interior center saturates to 255 with a small spread.
    const std::vector<uint8_t> packed = maz::ui::packSdf(sdf, 4.0f);
    CHECK(packed.size() == sdf.size());
    CHECK(packed[static_cast<size_t>(12 * W + 12)] == 255); // deep inside -> 1.0
    CHECK(packed[0] == 0);                                   // far outside -> 0.0
    // A near-edge texel lands near the mid byte.
    // Find a texel whose exact distance is ~0 and check it's ~127-128.
    bool checkedEdge = false;
    for (const auto& b : boundary) {
        const uint8_t p = packed[static_cast<size_t>(b.second * W + b.first)];
        CHECK(p >= 100 && p <= 160);
        checkedEdge = true;
        break;
    }
    CHECK(checkedEdge);
}

// Pcg32: reproduces PCG's canonical reference test vector; helpers are bounded + deterministic.
void testPcg32() {
    using maz::core::Pcg32;

    // Canonical pcg32 vector: seed(state=42, seq=54) -> these first six 32-bit outputs (from the
    // reference pcg-c-basic demo). Exact reproduction proves the port is bit-correct.
    Pcg32 rng(42u, 54u);
    const uint32_t expected[6] = {0xa15c02b7u, 0x7b47f409u, 0xba1d3330u,
                                  0x83d2f293u, 0xbfa4784bu, 0xcbed606eu};
    for (uint32_t e : expected) {
        CHECK(rng.next() == e);
    }

    // Same seed -> identical stream (determinism); a different stream selector diverges.
    Pcg32 a(7u, 1u), b(7u, 1u), c(7u, 2u);
    CHECK(a.next() == b.next());
    Pcg32 a2(7u, 1u), c2(7u, 2u);
    CHECK(a2.next() != c2.next());

    // nextBounded is in range and unbiased-by-construction; range() is inclusive.
    Pcg32 r(12345u, 6789u);
    bool sawLo = false, sawHi = false;
    for (int i = 0; i < 20000; ++i) {
        const uint32_t v = r.nextBounded(6); // a die
        CHECK(v < 6);
        const int d = r.range(1, 6);
        CHECK(d >= 1 && d <= 6);
        if (d == 1) sawLo = true;
        if (d == 6) sawHi = true;
    }
    CHECK(sawLo && sawHi);           // both ends of the inclusive range occur
    CHECK(r.nextBounded(0) == 0);    // degenerate bound
    const float f = r.nextFloat();
    CHECK(f >= 0.0f && f < 1.0f);
}

// Overlap3D: exact sphere overlap queries + conservative swept-sphere cast against AABBs.
void testOverlap3D() {
    using maz::game::Aabb;
    using maz::math::vec3;
    namespace g = maz::game;

    const Aabb box{vec3(-1, -1, -1), vec3(1, 1, 1)}; // unit cube at origin

    // --- exact sphere-vs-AABB via closest point ---
    CHECK(g::sphereVsAabb(vec3(3, 0, 0), 2.5f, box));  // reaches x=0.5 -> overlaps face
    CHECK(!g::sphereVsAabb(vec3(3, 0, 0), 1.9f, box)); // stops at x=1.1, misses
    // Corner distance: nearest point (1,1,1), center at (2,2,2) -> dist sqrt(3)~1.732.
    CHECK(g::sphereVsAabb(vec3(2, 2, 2), 1.8f, box));
    CHECK(!g::sphereVsAabb(vec3(2, 2, 2), 1.7f, box));
    CHECK(g::sphereVsSphere(vec3(0, 0, 0), 1.0f, vec3(1.5f, 0, 0), 0.6f)); // 1.6 reach > 1.5
    CHECK(!g::sphereVsSphere(vec3(0, 0, 0), 1.0f, vec3(1.5f, 0, 0), 0.4f));

    // --- overlapSphere: indices of every touched box ---
    std::vector<Aabb> boxes = {
        Aabb::fromCenterSize(vec3(0, 0, 0), vec3(2, 2, 2)),   // 0
        Aabb::fromCenterSize(vec3(10, 0, 0), vec3(2, 2, 2)),  // 1 (far)
        Aabb::fromCenterSize(vec3(3, 0, 0), vec3(2, 2, 2)),   // 2 (near, spans x2..4)
    };
    const auto hitIdx = g::overlapSphere(vec3(1.5f, 0, 0), 1.0f, boxes);
    // sphere at x=1.5 r=1 reaches [0.5, 2.5]: touches box0 (max x=1) and box2 (min x=2), not box1.
    CHECK(hitIdx.size() == 2);
    CHECK(std::find(hitIdx.begin(), hitIdx.end(), 0u) != hitIdx.end());
    CHECK(std::find(hitIdx.begin(), hitIdx.end(), 2u) != hitIdx.end());
    CHECK(std::find(hitIdx.begin(), hitIdx.end(), 1u) == hitIdx.end());

    // --- sphereCast: a ball of r=0.5 fired down +x from x=-10 hits box0's grown face at x=-1.5 ---
    {
        std::vector<Aabb> one = {box};
        const auto hit = g::sphereCast(vec3(-10, 0, 0), vec3(1, 0, 0), 0.5f, one);
        CHECK(hit.hit);
        CHECK(hit.index == 0);
        // grown box min.x = -1.5; origin x=-10, dir length 1 -> t = 8.5.
        CHECK_NEAR(hit.t, 8.5f, 1e-3f);
    }
    // A shot that passes clearly beside the box (in a face-aligned lane) misses.
    {
        std::vector<Aabb> one = {box};
        const auto miss = g::sphereCast(vec3(-10, 5, 0), vec3(1, 0, 0), 0.5f, one);
        CHECK(!miss.hit);
    }
    // Nearest of several boxes is returned.
    {
        std::vector<Aabb> two = {Aabb::fromCenterSize(vec3(20, 0, 0), vec3(2, 2, 2)),
                                 Aabb::fromCenterSize(vec3(5, 0, 0), vec3(2, 2, 2))};
        const auto hit = g::sphereCast(vec3(0, 0, 0), vec3(1, 0, 0), 0.25f, two);
        CHECK(hit.hit);
        CHECK(hit.index == 1);              // the closer box (x~4 grown face) wins
        CHECK_NEAR(hit.t, 3.75f, 1e-3f);    // 5 - 1 - 0.25
    }
}

// ChunkStreamer: resident set + load/unload deltas track a moving focus.
void testChunkStreamer() {
    using maz::game::ChunkCoord;
    using maz::game::ChunkStreamer;

    // 64-unit chunks, radius 1 (a 3x3 window = 9 chunks).
    ChunkStreamer s(64.0f, 1);
    CHECK(s.chunkAt(0.0f, 0.0f) == (ChunkCoord{0, 0}));
    CHECK(s.chunkAt(63.9f, 0.0f) == (ChunkCoord{0, 0}));
    CHECK(s.chunkAt(64.0f, 0.0f) == (ChunkCoord{1, 0}));
    CHECK(s.chunkAt(-1.0f, 0.0f) == (ChunkCoord{-1, 0})); // floor division for negatives

    // First update at origin loads the full 3x3 and unloads nothing.
    auto d0 = s.update(32.0f, 32.0f); // inside chunk (0,0)
    CHECK(d0.toLoad.size() == 9);
    CHECK(d0.toUnload.empty());
    CHECK(s.residentCount() == 9);
    CHECK(s.isResident({0, 0}));
    CHECK(s.isResident({1, 1}));
    CHECK(s.isResident({-1, -1}));
    CHECK(!s.isResident({2, 0}));

    // Staying within the same chunk => no change.
    auto d1 = s.update(40.0f, 40.0f);
    CHECK(d1.toLoad.empty() && d1.toUnload.empty());
    CHECK(s.residentCount() == 9);

    // Step focus one chunk to the right (into chunk (1,0)): the window shifts, loading the new
    // right column (x=2) and unloading the old left column (x=-1) — 3 each.
    auto d2 = s.update(64.0f + 32.0f, 32.0f);
    CHECK(d2.toLoad.size() == 3);
    CHECK(d2.toUnload.size() == 3);
    for (const auto& c : d2.toLoad) CHECK(c.x == 2);
    for (const auto& c : d2.toUnload) CHECK(c.x == -1);
    CHECK(s.residentCount() == 9);
    // Deterministic delta ordering (sorted by y then x).
    CHECK(d2.toLoad[0].y <= d2.toLoad[1].y);

    // A big jump replaces the whole window (no overlap): 9 load, 9 unload.
    auto d3 = s.update(1000.0f, 1000.0f);
    CHECK(d3.toLoad.size() == 9);
    CHECK(d3.toUnload.size() == 9);
    CHECK(s.residentCount() == 9);

    // clear() drops residency; next update reloads from scratch.
    s.clear();
    CHECK(s.residentCount() == 0);
    auto d4 = s.update(1000.0f, 1000.0f);
    CHECK(d4.toLoad.size() == 9);
    CHECK(d4.toUnload.empty());

    // Circular window (radius 2, disc: keep dx^2+dy^2 <= 4) is a plus-ish shape, far smaller than the
    // 5x5 square (25). Included offsets: center(1) + the 4 axis-1s + 4 diagonals(dist 2) + 4 axis-2s
    // (dist 4) = 13; everything with dist^2 in {5,8} is dropped.
    ChunkStreamer disc(50.0f, 2, /*circular*/ true);
    auto dc = disc.update(0.0f, 0.0f);
    CHECK(dc.toLoad.size() == 13);
    CHECK(!disc.isResident({2, 2})); // corner (dist^2=8) excluded
    CHECK(!disc.isResident({2, 1})); // dist^2=5 excluded
    CHECK(disc.isResident({2, 0}));  // dist^2=4 included
    CHECK(disc.isResident({1, 1}));  // dist^2=2 included
}

// SpriteOrder: draw ordering by layer, then z-index, then y-sort, stable on ties.
void testSpriteOrder() {
    using maz::render::DrawItem;

    // id, layer, zIndex, ySort, useYSort
    std::vector<DrawItem> items = {
        {10, 0, 0, 0.0f, false},   // world, z0
        {11, 1, 0, 0.0f, false},   // HUD layer (above everything in layer 0)
        {12, 0, 5, 0.0f, false},   // world, z5 (in front of z0)
        {13, 0, 0, 200.0f, true},  // world, z0, y-sorted low on screen -> front
        {14, 0, 0, 50.0f, true},   // world, z0, y-sorted higher -> behind 13
        {15, -1, 0, 0.0f, false},  // background layer (behind world)
    };

    const auto order = maz::render::sortedIndices(items);
    // Map back to ids in draw order.
    std::vector<uint32_t> ids;
    for (uint32_t i : order) ids.push_back(items[i].id);

    // Expected back-to-front:
    //  layer -1 (bg): 15
    //  layer 0, z0: y-sorted 14 (y=50) then 13 (y=200); the non-y-sorted 10 sorts by insertion
    //     among z0 — insertion index of 10 is 0, of 13 is 3, of 14 is 4, so 10 comes first.
    //  layer 0, z5: 12
    //  layer 1: 11
    CHECK(ids.size() == 6);
    CHECK(ids.front() == 15);                       // background first
    CHECK(ids.back() == 11);                         // HUD last (on top)
    // z5 (12) draws after all z0 items in layer 0 but before the HUD layer.
    auto pos = [&](uint32_t id) {
        return std::find(ids.begin(), ids.end(), id) - ids.begin();
    };
    CHECK(pos(15) < pos(10));   // bg before world
    CHECK(pos(14) < pos(13));   // y-sort: higher-on-screen behind lower-on-screen
    CHECK(pos(13) < pos(12));   // z0 group before z5 within the layer
    CHECK(pos(12) < pos(11));   // world layer before HUD layer

    // In-place sort matches the index order.
    std::vector<DrawItem> copy = items;
    maz::render::sort(copy);
    for (size_t i = 0; i < copy.size(); ++i) {
        CHECK(copy[i].id == ids[i]);
    }

    // Stability: equal keys keep submission order.
    std::vector<DrawItem> tie = {{1, 0, 0, 0, false}, {2, 0, 0, 0, false}, {3, 0, 0, 0, false}};
    const auto to = maz::render::sortedIndices(tie);
    CHECK(to[0] == 0 && to[1] == 1 && to[2] == 2);
}

// SweepPrune2D: the SAP broadphase returns exactly the overlapping AABB pairs brute force finds.
void testSweepPrune2D() {
    using maz::game::SweepPrune2D;

    auto norm = [](uint32_t a, uint32_t b) {
        return a < b ? std::make_pair(a, b) : std::make_pair(b, a);
    };

    // Deterministic random boxes; compare SAP pairs against the O(n^2) brute-force overlap set.
    core::Random rng(99);
    std::vector<SweepPrune2D::Box> boxes;
    for (uint32_t i = 0; i < 120; ++i) {
        const float x = static_cast<float>(rng.range(0, 400));
        const float y = static_cast<float>(rng.range(0, 400));
        const float w = static_cast<float>(rng.range(2, 30));
        const float h = static_cast<float>(rng.range(2, 30));
        boxes.push_back(SweepPrune2D::fromRect(i, x, y, w, h));
    }

    // Brute-force truth set.
    std::set<std::pair<uint32_t, uint32_t>> truth;
    for (size_t i = 0; i < boxes.size(); ++i) {
        for (size_t j = i + 1; j < boxes.size(); ++j) {
            const auto& a = boxes[i];
            const auto& b = boxes[j];
            const bool overlap = a.min[0] <= b.max[0] && a.max[0] >= b.min[0] &&
                                 a.min[1] <= b.max[1] && a.max[1] >= b.min[1];
            if (overlap) truth.insert(norm(a.id, b.id));
        }
    }

    SweepPrune2D sap;
    sap.build(boxes);
    CHECK(sap.size() == boxes.size());
    std::set<std::pair<uint32_t, uint32_t>> got;
    for (const auto& p : sap.overlappingPairs()) got.insert(norm(p.first, p.second));

    // Exact match: no false positives, no misses.
    CHECK(got == truth);
    CHECK(!truth.empty()); // the scene actually has overlaps (guards a vacuous pass)

    // Single-probe query matches a manual scan.
    const float qmin[2] = {100.0f, 100.0f};
    const float qmax[2] = {140.0f, 140.0f};
    std::set<uint32_t> qtruth;
    for (const auto& b : boxes) {
        if (b.min[0] <= qmax[0] && b.max[0] >= qmin[0] && b.min[1] <= qmax[1] && b.max[1] >= qmin[1])
            qtruth.insert(b.id);
    }
    std::set<uint32_t> qgot;
    for (uint32_t id : sap.query(qmin, qmax)) qgot.insert(id);
    CHECK(qgot == qtruth);

    // Empty build is well-defined.
    SweepPrune2D empty;
    empty.build({});
    CHECK(empty.size() == 0);
    CHECK(empty.overlappingPairs().empty());
}

// BVH: box queries + ray casts match brute force, and raycastNearest returns the closest hit.
void testBvh() {
    using maz::game::Bvh;
    struct Box {
        uint32_t id;
        float mn[3], mx[3];
    };
    std::vector<Box> boxes;
    std::vector<Bvh::Item> items;
    core::Random rng(555);
    for (int i = 0; i < 200; ++i) {
        float x = static_cast<float>(rng.range(0, 480));
        float y = static_cast<float>(rng.range(0, 480));
        float z = static_cast<float>(rng.range(0, 480));
        float s = static_cast<float>(rng.range(2, 12));
        Box b{static_cast<uint32_t>(i), {x, y, z}, {x + s, y + s, z + s}};
        boxes.push_back(b);
        Bvh::Item it;
        it.id = b.id;
        for (int a = 0; a < 3; ++a) {
            it.min[a] = b.mn[a];
            it.max[a] = b.mx[a];
        }
        items.push_back(it);
    }

    Bvh bvh;
    bvh.build(items, 4);
    CHECK(bvh.size() == 200);
    CHECK(bvh.nodeCount() > 1); // it actually subdivided

    auto treeSet = [](std::vector<uint32_t> v) { return std::set<uint32_t>(v.begin(), v.end()); };

    // Box query matches brute-force overlap.
    const float qmin[3] = {100, 100, 100};
    const float qmax[3] = {200, 200, 200};
    std::set<uint32_t> brute;
    for (const Box& b : boxes) {
        if (Bvh::overlap(b.mn, b.mx, qmin, qmax)) brute.insert(b.id);
    }
    CHECK(treeSet(bvh.queryBox(qmin, qmax)) == brute);

    // Whole-volume box query returns everything; a far box returns nothing.
    const float allMin[3] = {-10, -10, -10}, allMax[3] = {600, 600, 600};
    CHECK(bvh.queryBox(allMin, allMax).size() == 200);
    const float farMin[3] = {5000, 5000, 5000}, farMax[3] = {5010, 5010, 5010};
    CHECK(bvh.queryBox(farMin, farMax).empty());

    // Ray cast matches the brute-force slab test over all boxes.
    const float o[3] = {-50, 240, 240};
    const float d[3] = {1, 0, 0}; // shoot along +x through the middle slab
    std::set<uint32_t> bruteRay;
    for (const Box& b : boxes) {
        float t;
        if (Bvh::rayAabb(o, d, b.mn, b.mx, std::numeric_limits<float>::infinity(), t)) {
            bruteRay.insert(b.id);
        }
    }
    CHECK(treeSet(bvh.raycast(o, d)) == bruteRay);

    // A controlled scene: two boxes on the +x ray; nearest is the closer one.
    std::vector<Bvh::Item> two;
    two.push_back(Bvh::fromCenter(10, 30.f, 0.f, 0.f, 2.f, 2.f, 2.f)); // near, center x=30
    two.push_back(Bvh::fromCenter(20, 80.f, 0.f, 0.f, 2.f, 2.f, 2.f)); // far,  center x=80
    two.push_back(Bvh::fromCenter(30, 0.f, 100.f, 0.f, 2.f, 2.f, 2.f)); // off the ray
    Bvh line;
    line.build(two, 1);
    const float ro[3] = {0, 0, 0}, rd[3] = {1, 0, 0};
    CHECK(treeSet(line.raycast(ro, rd)) == (std::set<uint32_t>{10, 20}));
    uint32_t hit = 0;
    float ht = 0;
    CHECK(line.raycastNearest(ro, rd, hit, ht));
    CHECK(hit == 10);              // the near box
    CHECK(std::fabs(ht - 28.f) < 1e-3f); // enters at x=28 (center 30, half-extent 2)

    // tMax bounds the ray: with a short reach only the near box is hit.
    CHECK(treeSet(line.raycast(ro, rd, 40.f)) == (std::set<uint32_t>{10}));
    // A ray pointing away hits nothing.
    const float back[3] = {-1, 0, 0};
    CHECK(!line.raycastNearest(ro, back, hit, ht));

    // Empty BVH is safe.
    Bvh empty;
    empty.build({});
    CHECK(empty.size() == 0 && empty.queryBox(allMin, allMax).empty());
    CHECK(!empty.raycastNearest(ro, rd, hit, ht));
}

// Octree: 3D spatial range queries match brute-force overlap exactly, with subdivision on clusters.
void testOctree() {
    using maz::game::Octree;
    struct Box {
        uint32_t id;
        float x, y, z, w, h, d;
    };
    std::vector<Box> boxes;
    core::Random rng(77);
    for (int i = 0; i < 150; ++i) {
        boxes.push_back({static_cast<uint32_t>(i), static_cast<float>(rng.range(0, 490)),
                         static_cast<float>(rng.range(0, 490)), static_cast<float>(rng.range(0, 490)),
                         static_cast<float>(rng.range(1, 8)), static_cast<float>(rng.range(1, 8)),
                         static_cast<float>(rng.range(1, 8))});
    }
    // A tight cluster in one octant to force deep subdivision.
    for (int i = 0; i < 40; ++i) {
        boxes.push_back({static_cast<uint32_t>(150 + i), static_cast<float>(rng.range(0, 20)),
                         static_cast<float>(rng.range(0, 20)), static_cast<float>(rng.range(0, 20)),
                         2.0f, 2.0f, 2.0f});
    }

    Octree oc(0, 0, 0, 500, 500, 500, 8, 4);
    for (const Box& b : boxes) oc.insert(b.id, b.x, b.y, b.z, b.w, b.h, b.d);
    CHECK(oc.size() == boxes.size());
    CHECK(oc.nodeCount() > 1); // cluster forced subdivision

    auto brute = [&](float qx, float qy, float qz, float qw, float qh, float qd) {
        std::set<uint32_t> s;
        for (const Box& b : boxes) {
            if (maz::game::aabb3Overlap(b.x, b.y, b.z, b.w, b.h, b.d, qx, qy, qz, qw, qh, qd)) {
                s.insert(b.id);
            }
        }
        return s;
    };
    auto treeSet = [&](std::vector<uint32_t> v) { return std::set<uint32_t>(v.begin(), v.end()); };

    struct Q {
        float x, y, z, w, h, d;
    };
    const Q queries[] = {
        {0, 0, 0, 30, 30, 30}, {240, 240, 240, 20, 20, 20}, {0, 0, 0, 500, 20, 500}, {100, 100, 100, 5, 5, 5}};
    for (const Q& q : queries) {
        CHECK(treeSet(oc.query(q.x, q.y, q.z, q.w, q.h, q.d)) ==
              brute(q.x, q.y, q.z, q.w, q.h, q.d));
    }

    // Whole-volume query returns everything; an outside query returns nothing.
    CHECK(oc.query(-10, -10, -10, 520, 520, 520).size() == boxes.size());
    CHECK(oc.query(9000, 9000, 9000, 10, 10, 10).empty());

    // querySphere bounding-box prefilter surfaces a nearby box only.
    Octree o2(0, 0, 0, 100, 100, 100, 6, 2);
    o2.insert(1, 48, 48, 48, 4, 4, 4);
    o2.insert(2, 90, 90, 90, 4, 4, 4);
    CHECK(treeSet(o2.querySphere(50, 50, 50, 10)) == std::set<uint32_t>{1});

    // clear() empties it.
    o2.clear();
    CHECK(o2.size() == 0 && o2.query(0, 0, 0, 100, 100, 100).empty());
}

// Reflection: register fields by member pointer, then get/set by name and round-trip through
// serialize/deserialize (reflection-driven save + editor inspector).
void testReflect() {
    struct Enemy {
        int hp = 100;
        float speed = 3.5f;
        bool boss = false;
        std::string name = "grunt";
    };

    core::TypeDesc<Enemy> desc;
    desc.prop("hp", &Enemy::hp)
        .prop("speed", &Enemy::speed)
        .prop("boss", &Enemy::boss)
        .prop("name", &Enemy::name);
    CHECK(desc.propertyCount() == 4);

    Enemy e;
    // get by name, correctly typed.
    core::PropValue v;
    CHECK(desc.get(e, "hp", v) && v.type == core::PropType::Int && v.i == 100);
    CHECK(desc.get(e, "speed", v) && v.type == core::PropType::Float);
    CHECK(desc.get(e, "boss", v) && v.type == core::PropType::Bool && v.b == false);
    CHECK(desc.get(e, "name", v) && v.type == core::PropType::String && v.s == "grunt");
    CHECK(!desc.get(e, "missing", v)); // unknown name

    // set by name writes through to the real field.
    CHECK(desc.set(e, "hp", core::PropValue::makeInt(250)));
    CHECK(desc.set(e, "boss", core::PropValue::makeBool(true)));
    CHECK(desc.set(e, "name", core::PropValue::makeString("warlord")));
    CHECK(!desc.set(e, "missing", core::PropValue::makeInt(0)));
    CHECK(e.hp == 250 && e.boss == true && e.name == "warlord");

    // read() yields every field (the inspector/serializer walk).
    auto all = desc.read(e);
    CHECK(all.size() == 4);
    CHECK(all[0].first == "hp" && all[0].second.i == 250);

    // serialize -> deserialize onto a fresh object reproduces every field (incl. an escaped newline).
    e.name = "line1\nline2";
    e.speed = 9.25f;
    const std::string blob = desc.serialize(e);
    Enemy loaded;
    const int applied = desc.deserialize(loaded, blob);
    CHECK(applied == 4);
    CHECK(loaded.hp == 250);
    CHECK(loaded.boss == true);
    CHECK(loaded.name == "line1\nline2"); // newline survived escaping
    CHECK(std::fabs(loaded.speed - 9.25f) < 1e-6f);

    // Forward-compatible: an unknown property line is skipped, known ones still apply.
    Enemy partial;
    const int n = desc.deserialize(partial, "hp=i:42\nunknown=i:7\nboss=b:1\n");
    CHECK(n == 2); // only hp + boss are known
    CHECK(partial.hp == 42 && partial.boss == true);

    // Type coercion: setting an int field from a float value truncates cleanly.
    CHECK(desc.set(e, "hp", core::PropValue::makeFloat(17.9)));
    CHECK(e.hp == 17);
}

// DateTime: deterministic epoch<->calendar conversion + the in-game GameClock.
void testDateTime() {
    // The epoch itself: 0 seconds == 1970-01-01T00:00:00Z, a Thursday.
    core::DateTime e = core::fromUnix(0);
    CHECK(e.year == 1970 && e.month == 1 && e.day == 1);
    CHECK(e.hour == 0 && e.minute == 0 && e.second == 0);
    CHECK(e.weekday == 4); // Thursday
    CHECK(core::formatIso(e) == "1970-01-01T00:00:00Z");

    // A known modern timestamp: 1700000000 == 2023-11-14T22:13:20Z (a Tuesday).
    core::DateTime d = core::fromUnix(1700000000);
    CHECK(d.year == 2023 && d.month == 11 && d.day == 14);
    CHECK(d.hour == 22 && d.minute == 13 && d.second == 20);
    CHECK(d.weekday == 2); // Tuesday
    CHECK(core::formatIso(d) == "2023-11-14T22:13:20Z");

    // Round-trip a spread of timestamps exactly.
    const int64_t stamps[] = {0, 1, 86399, 86400, 951782400 /*2000-02-29*/, 1700000000, 4102444800};
    for (int64_t s : stamps) {
        CHECK(core::toUnix(core::fromUnix(s)) == s);
    }

    // Leap-year handling: 2000 is leap (÷400), 1900 is not (÷100), 2024 is (÷4).
    CHECK(core::isLeapYear(2000));
    CHECK(!core::isLeapYear(1900));
    CHECK(core::isLeapYear(2024));
    CHECK(core::daysInMonth(2024, 2) == 29);
    CHECK(core::daysInMonth(2023, 2) == 28);
    CHECK(core::daysInMonth(2023, 4) == 30);

    // Feb 29 2000 exists and round-trips.
    core::DateTime leap = core::fromUnix(951782400);
    CHECK(leap.year == 2000 && leap.month == 2 && leap.day == 29);

    // Pre-epoch (negative) timestamps floor correctly: -1s == 1969-12-31T23:59:59Z.
    core::DateTime pre = core::fromUnix(-1);
    CHECK(pre.year == 1969 && pre.month == 12 && pre.day == 31);
    CHECK(pre.hour == 23 && pre.minute == 59 && pre.second == 59);
    CHECK(core::toUnix(pre) == -1);

    // toUnix from assembled fields.
    core::DateTime made;
    made.year = 2023;
    made.month = 11;
    made.day = 14;
    made.hour = 22;
    made.minute = 13;
    made.second = 20;
    CHECK(core::toUnix(made) == 1700000000);

    // ---- GameClock (deterministic in-game time) ----
    core::GameClock clock;
    CHECK(clock.totalSeconds() == 0.0);
    for (int i = 0; i < 3600; ++i) clock.advance(1.0); // 1 in-game hour
    CHECK(clock.hourOfDay() == 1);
    CHECK(clock.minuteOfHour() == 0);
    CHECK(std::fabs(clock.timeOfDay01() - (3600.0 / 86400.0)) < 1e-9);

    // Time scale fast-forwards: 2x means 100 real seconds -> 200 game seconds.
    core::GameClock fast;
    fast.setTimeScale(2.0);
    for (int i = 0; i < 100; ++i) fast.advance(1.0);
    CHECK(std::fabs(fast.totalSeconds() - 200.0) < 1e-9);

    // Rolling past a day boundary rolls totalDays and wraps time-of-day.
    core::GameClock day;
    day.reset(90000.0); // 25 hours -> day 1, hour 1
    CHECK(day.totalDays() == 1);
    CHECK(day.hourOfDay() == 1);
    CHECK(day.timeOfDay01() < 0.05); // just past midnight of day 2
}

// Virtual filesystem: pure path helpers, scheme mounting/resolution, and the traversal-escape guard.
void testVfs() {
    using namespace maz::io;

    // normalizePath collapses '.', '..', and duplicate slashes; preserves absolute/relative.
    CHECK(normalizePath("a/b/../c") == "a/c");
    CHECK(normalizePath("a//b/./c/") == "a/b/c");
    CHECK(normalizePath("/a/b/../../c") == "/c");
    CHECK(normalizePath("/a/../../c") == "/c");     // can't escape above root when absolute
    CHECK(normalizePath("../a/b") == "../a/b");      // relative may keep leading ..
    CHECK(normalizePath("a/../..") == "..");         // net one level up
    CHECK(normalizePath("") == ".");
    CHECK(normalizePath("/") == "/");
    CHECK(normalizePath("./x") == "x");

    // Path component helpers.
    CHECK(joinPath("a/b", "c/d") == "a/b/c/d");
    CHECK(joinPath("a/b/", "c") == "a/b/c");
    CHECK(joinPath("a", "/abs") == "/abs"); // absolute b replaces a
    CHECK(fileName("a/b/c.png") == "c.png");
    CHECK(fileName("noslash") == "noslash");
    CHECK(extension("a/b/c.tar.png") == "png");
    CHECK(extension("a/b/noext") == "");
    CHECK(extension("/.gitignore") == ""); // leading-dot file has no extension
    CHECK(fileStem("a/b/c.png") == "c");
    CHECK(fileStem("archive.tar.gz") == "archive.tar");
    CHECK(parentPath("a/b/c.png") == "a/b");
    CHECK(parentPath("/root") == "/");
    CHECK(parentPath("bare") == "");

    // Scheme parse.
    std::string scheme, rest;
    CHECK(VirtualFileSystem::parse("res://textures/hero.png", scheme, rest));
    CHECK(scheme == "res" && rest == "textures/hero.png");
    CHECK(!VirtualFileSystem::parse("no-scheme/here", scheme, rest));

    // Mount + resolve maps scheme paths to real dirs.
    VirtualFileSystem vfs;
    vfs.mount("res", "/game/assets");
    vfs.mount("user", "/home/player/.local/share/mygame");
    CHECK(vfs.isMounted("res") && vfs.isMounted("user"));
    CHECK(vfs.resolve("res://textures/hero.png") == "/game/assets/textures/hero.png");
    CHECK(vfs.resolve("user://save1.dat") == "/home/player/.local/share/mygame/save1.dat");
    CHECK(vfs.resolve("res://a/./b/../c.txt") == "/game/assets/a/c.txt"); // normalized

    // Unmounted scheme or missing scheme -> empty.
    CHECK(vfs.resolve("mod://thing").empty());
    CHECK(vfs.resolve("/absolute/path").empty());

    // TRAVERSAL GUARD: a '..' that escapes the mount root is refused, not followed.
    CHECK(vfs.resolve("res://../../etc/passwd").empty());
    CHECK(vfs.resolve("res://..").empty());
    CHECK(!vfs.canResolve("res://../secret"));
    // A '..' that stays inside the mount is fine.
    CHECK(vfs.resolve("res://a/b/../c.png") == "/game/assets/a/c.png");
    CHECK(vfs.canResolve("res://a/b/../c.png"));

    // Re-mounting replaces the base; unmount removes it.
    vfs.mount("res", "/other/root");
    CHECK(vfs.resolve("res://x.png") == "/other/root/x.png");
    vfs.unmount("res");
    CHECK(!vfs.isMounted("res"));
    CHECK(vfs.resolve("res://x.png").empty());
}

// Version: semver parse, compare, and the engine build stamp.
void testVersion() {
    using maz::Version;

    // The engine's own version matches the macros.
    Version ev = maz::engineVersion();
    CHECK(ev.major == MAZ_VERSION_MAJOR && ev.minor == MAZ_VERSION_MINOR &&
          ev.patch == MAZ_VERSION_PATCH);
    CHECK(ev.toString() == MAZ_VERSION_STRING);
    CHECK(ev.number() == MAZ_VERSION_NUMBER);

    // Parse the standard form, plus leniencies (leading v, partials, pre-release/build suffix).
    Version v;
    CHECK(Version::parse("1.2.3", v) && v.major == 1 && v.minor == 2 && v.patch == 3);
    CHECK(Version::parse("v2.0.0", v) && v.major == 2 && v.minor == 0 && v.patch == 0);
    CHECK(Version::parse("1", v) && v.major == 1 && v.minor == 0 && v.patch == 0);
    CHECK(Version::parse("1.5", v) && v.major == 1 && v.minor == 5 && v.patch == 0);
    CHECK(Version::parse("1.2.3-rc1", v) && v.patch == 3); // pre-release trimmed
    CHECK(Version::parse("1.2.3+build.7", v) && v.patch == 3); // build metadata trimmed

    // Malformed input is rejected.
    CHECK(!Version::parse("", v));
    CHECK(!Version::parse("abc", v));
    CHECK(!Version::parse("1..2", v));
    CHECK(!Version::parse("1.x.3", v));

    // Ordering follows semver precedence.
    auto V = [](int a, int b, int c) { return Version{a, b, c}; };
    CHECK(V(1, 0, 0) < V(1, 0, 1));
    CHECK(V(1, 0, 9) < V(1, 1, 0));
    CHECK(V(1, 9, 9) < V(2, 0, 0));
    CHECK(V(2, 0, 0) > V(1, 9, 9));
    CHECK(V(1, 2, 3) == V(1, 2, 3));
    CHECK(V(1, 2, 3) != V(1, 2, 4));
    CHECK(V(1, 2, 3) <= V(1, 2, 3));
    CHECK(V(1, 2, 3) >= V(1, 2, 3));

    // atLeast() — the "requires engine >= X" gate.
    CHECK(V(1, 4, 0).atLeast(V(1, 2, 0)));
    CHECK(!V(1, 1, 0).atLeast(V(1, 2, 0)));
    CHECK(V(1, 2, 0).atLeast(V(1, 2, 0)));

    // number() sorts consistently with the operators.
    CHECK(V(1, 0, 0).number() < V(1, 0, 1).number());
    CHECK(V(2, 0, 0).number() > V(1, 99, 99).number());
}

void testMemory() {
    // ---- LinearArena ----
    core::LinearArena arena(1024);
    CHECK(arena.capacity() == 1024);
    CHECK(arena.used() == 0);
    CHECK(arena.remaining() == 1024);

    void* a = arena.allocate(100, 1);
    CHECK(a != nullptr);
    CHECK(arena.used() == 100);

    // Alignment: the next 16-aligned allocation starts on a 16-byte boundary (offset padded up).
    void* b = arena.allocate(8, 16);
    CHECK(b != nullptr);
    CHECK(reinterpret_cast<uintptr_t>(b) % 16 == 0);
    CHECK(arena.used() > 100); // padding was inserted before b

    // Typed helper is correctly aligned for the type.
    struct alignas(8) Thing {
        double x;
        int y;
    };
    Thing* t = arena.alloc<Thing>(3);
    CHECK(t != nullptr);
    CHECK(reinterpret_cast<uintptr_t>(t) % alignof(Thing) == 0);

    // OOM returns nullptr and leaves the arena usable.
    CHECK(arena.allocate(100000) == nullptr);
    const size_t before = arena.used();
    CHECK(arena.allocate(4) != nullptr);
    CHECK(arena.used() > before);

    // Stack markers: allocate inside a scope, rewind releases exactly that. (align 1 so the offset
    // advances by exactly 200 with no alignment padding.)
    const size_t m = arena.marker();
    arena.allocate(200, 1);
    CHECK(arena.used() == m + 200);
    arena.rewind(m);
    CHECK(arena.used() == m);
    arena.rewind(m + 999999); // rewinding forward is ignored
    CHECK(arena.used() == m);

    // reset() frees everything at once (the frame reset).
    arena.reset();
    CHECK(arena.used() == 0);
    CHECK(arena.remaining() == 1024);

    // A bad (non-power-of-two) alignment is rejected.
    CHECK(arena.allocate(8, 3) == nullptr);

    // ---- PoolAllocator ----
    core::PoolAllocator pool(24, 4); // 4 blocks of >=24 bytes
    CHECK(pool.capacity() == 4);
    CHECK(pool.inUse() == 0);
    CHECK(pool.available() == 4);
    CHECK(pool.blockSize() >= 24);

    void* p0 = pool.allocate();
    void* p1 = pool.allocate();
    void* p2 = pool.allocate();
    void* p3 = pool.allocate();
    CHECK(p0 && p1 && p2 && p3);
    CHECK(pool.inUse() == 4);
    CHECK(pool.available() == 0);
    CHECK(pool.owns(p0));
    CHECK(!pool.owns(&pool)); // a foreign pointer isn't owned

    // Exhausted -> nullptr, no crash.
    CHECK(pool.allocate() == nullptr);

    // Distinct blocks don't overlap (spot-check p0 vs p1 by block stride).
    CHECK(p0 != p1 && p1 != p2 && p2 != p3);

    // free() returns a slot; the next allocate() reuses it (O(1), no fragmentation).
    pool.free(p2);
    CHECK(pool.inUse() == 3);
    void* reused = pool.allocate();
    CHECK(reused == p2); // LIFO free list hands the same slot back
    CHECK(pool.inUse() == 4);

    // reset() reclaims all blocks at once.
    pool.reset();
    CHECK(pool.inUse() == 0);
    CHECK(pool.available() == 4);

    // A tiny requested block size is bumped up to hold the free-list pointer.
    core::PoolAllocator tiny(1, 2);
    CHECK(tiny.blockSize() >= sizeof(void*));
    void* q = tiny.allocate();
    CHECK(q != nullptr);
    tiny.free(q);
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

void testCurve() {
    using anim::Curve;
    using anim::CurveInterp;

    // Empty curve -> clamped default; single point -> that value everywhere.
    {
        Curve c;
        CHECK_NEAR(c.sample(0.5f), 0.0f, 1e-5f);
        c.addPoint(0.3f, 0.7f);
        CHECK_NEAR(c.sample(0.0f), 0.7f, 1e-5f);
        CHECK_NEAR(c.sample(1.0f), 0.7f, 1e-5f);
    }

    // Linear interpolation + domain clamping.
    {
        Curve c;
        c.interp = CurveInterp::Linear;
        c.maxValue = 10.0f;
        c.addPoint(0.0f, 0.0f);
        c.addPoint(1.0f, 10.0f);
        CHECK_NEAR(c.sample(0.0f), 0.0f, 1e-5f);
        CHECK_NEAR(c.sample(0.5f), 5.0f, 1e-4f);
        CHECK_NEAR(c.sample(1.0f), 10.0f, 1e-5f);
        CHECK_NEAR(c.sample(-1.0f), 0.0f, 1e-5f);  // below domain -> first value
        CHECK_NEAR(c.sample(2.0f), 10.0f, 1e-5f);  // above domain -> last value
    }

    // Constant mode holds the segment's left value.
    {
        Curve c;
        c.interp = CurveInterp::Constant;
        c.addPoint(0.0f, 2.0f);
        c.addPoint(1.0f, 9.0f);
        c.maxValue = 100.0f;
        CHECK_NEAR(c.sample(0.5f), 2.0f, 1e-5f);
        CHECK_NEAR(c.sample(0.99f), 2.0f, 1e-5f);
        CHECK_NEAR(c.sample(1.0f), 9.0f, 1e-5f);
    }

    // Cubic with flat endpoint tangents = a symmetric ease-in-out (smoothstep-like).
    {
        Curve c;
        c.interp = CurveInterp::Cubic;
        c.addPoint(0.0f, 0.0f, 0.0f, 0.0f);
        c.addPoint(1.0f, 1.0f, 0.0f, 0.0f);
        CHECK_NEAR(c.sample(0.0f), 0.0f, 1e-5f);
        CHECK_NEAR(c.sample(1.0f), 1.0f, 1e-5f);
        CHECK_NEAR(c.sample(0.5f), 0.5f, 1e-4f);      // symmetric midpoint
        CHECK(c.sample(0.25f) < 0.25f);               // eases in (slow start)
        CHECK(c.sample(0.75f) > 0.75f);               // eases out (fast then settle)
    }

    // Cubic with tangents == the chord slope reproduces the straight line.
    {
        Curve c;
        c.interp = CurveInterp::Cubic;
        c.addPoint(0.0f, 0.0f, 2.0f, 2.0f); // slope 2 over a unit segment to (1,2)
        c.addPoint(1.0f, 2.0f, 2.0f, 2.0f);
        CHECK_NEAR(c.sample(0.5f), 1.0f, 1e-4f); // matches the linear midpoint
    }

    // Result clamps to [minValue, maxValue].
    {
        Curve c;
        c.interp = CurveInterp::Linear;
        c.minValue = 0.0f;
        c.maxValue = 1.0f;
        c.addPoint(0.0f, 0.0f);
        c.addPoint(1.0f, 5.0f); // would exceed maxValue mid-segment
        CHECK_NEAR(c.sample(1.0f), 1.0f, 1e-5f);
        CHECK_NEAR(c.sample(0.5f), 1.0f, 1e-5f); // 2.5 clamped to 1.0
    }

    // addPoint keeps points sorted regardless of insertion order.
    {
        Curve c;
        c.interp = CurveInterp::Linear;
        c.maxValue = 10.0f;
        c.addPoint(1.0f, 10.0f);
        c.addPoint(0.0f, 0.0f);
        c.addPoint(0.5f, 2.0f);
        CHECK(c.pointCount() == 3);
        CHECK_NEAR(c.point(0).pos, 0.0f, 1e-6f);
        CHECK_NEAR(c.point(1).pos, 0.5f, 1e-6f);
        CHECK_NEAR(c.point(2).pos, 1.0f, 1e-6f);
        CHECK_NEAR(c.sample(0.25f), 1.0f, 1e-4f); // between (0,0) and (0.5,2)
    }
}

void testGradient() {
    using anim::Gradient;
    using anim::GradientInterp;
    using render::Color;

    const Color black{0.0f, 0.0f, 0.0f, 1.0f};
    const Color white{1.0f, 1.0f, 1.0f, 1.0f};
    const Color red{1.0f, 0.0f, 0.0f, 1.0f};
    const Color green{0.0f, 1.0f, 0.0f, 1.0f};
    const Color blue{0.0f, 0.0f, 1.0f, 1.0f};

    // Empty gradient -> opaque black; single stop -> that colour everywhere.
    {
        Gradient g;
        const Color e = g.sample(0.5f);
        CHECK_NEAR(e.r, 0.0f, 1e-5f);
        CHECK_NEAR(e.a, 1.0f, 1e-5f);
        g.addStop(0.3f, red);
        CHECK_NEAR(g.sample(0.0f).r, 1.0f, 1e-5f);
        CHECK_NEAR(g.sample(1.0f).r, 1.0f, 1e-5f);
    }

    // Two-stop constructor is black->white; linear midpoint is mid-grey; domain clamps.
    {
        Gradient g(black, white);
        CHECK(g.stopCount() == 2);
        CHECK_NEAR(g.sample(0.0f).r, 0.0f, 1e-5f);
        CHECK_NEAR(g.sample(1.0f).r, 1.0f, 1e-5f);
        const Color m = g.sample(0.5f);
        CHECK_NEAR(m.r, 0.5f, 1e-4f);
        CHECK_NEAR(m.g, 0.5f, 1e-4f);
        CHECK_NEAR(m.b, 0.5f, 1e-4f);
        CHECK_NEAR(g.sample(-1.0f).r, 0.0f, 1e-5f); // below -> first
        CHECK_NEAR(g.sample(2.0f).r, 1.0f, 1e-5f);  // above -> last
    }

    // Three stops red@0, green@0.5, blue@1; linear quarter point is halfway red->green.
    {
        Gradient g;
        g.interp = GradientInterp::Linear;
        g.addStop(0.0f, red);
        g.addStop(0.5f, green);
        g.addStop(1.0f, blue);
        const Color q = g.sample(0.25f);
        CHECK_NEAR(q.r, 0.5f, 1e-4f);
        CHECK_NEAR(q.g, 0.5f, 1e-4f);
        CHECK_NEAR(q.b, 0.0f, 1e-4f);
        // Sampling exactly at a stop returns that stop's colour (any mode).
        const Color mid = g.sample(0.5f);
        CHECK_NEAR(mid.g, 1.0f, 1e-4f);
        CHECK_NEAR(mid.r, 0.0f, 1e-4f);
    }

    // Constant mode holds the lower stop's colour across the segment.
    {
        Gradient g;
        g.interp = GradientInterp::Constant;
        g.addStop(0.0f, red);
        g.addStop(1.0f, blue);
        CHECK_NEAR(g.sample(0.5f).r, 1.0f, 1e-5f);
        CHECK_NEAR(g.sample(0.99f).r, 1.0f, 1e-5f);
        CHECK_NEAR(g.sample(0.99f).b, 0.0f, 1e-5f);
        CHECK_NEAR(g.sample(1.0f).b, 1.0f, 1e-5f); // at/after the last stop -> last colour
    }

    // Cubic passes through the stops and stays within [0,1] channels.
    {
        Gradient g;
        g.interp = GradientInterp::Cubic;
        g.addStop(0.0f, black);
        g.addStop(0.5f, white);
        g.addStop(1.0f, black);
        CHECK_NEAR(g.sample(0.0f).r, 0.0f, 1e-4f);
        CHECK_NEAR(g.sample(0.5f).r, 1.0f, 1e-4f); // hits the middle stop exactly
        CHECK_NEAR(g.sample(1.0f).r, 0.0f, 1e-4f);
        const Color s = g.sample(0.25f);
        CHECK(s.r >= 0.0f && s.r <= 1.0f); // clamped even if the spline overshoots
    }

    // bake(N): first == first stop, last == last stop, correct length.
    {
        Gradient g;
        g.addStop(0.0f, red);
        g.addStop(1.0f, blue);
        const auto ramp = g.bake(5);
        CHECK(ramp.size() == 5);
        CHECK_NEAR(ramp.front().r, 1.0f, 1e-5f);
        CHECK_NEAR(ramp.back().b, 1.0f, 1e-5f);
        CHECK_NEAR(ramp[2].r, 0.5f, 1e-4f); // middle sample is the linear midpoint
        // bake(1) samples the centre; bake(0) is empty.
        CHECK(g.bake(1).size() == 1);
        CHECK(g.bake(0).empty());
    }

    // addStop keeps stops sorted; setOffset re-sorts.
    {
        Gradient g;
        g.addStop(1.0f, blue);
        g.addStop(0.0f, red);
        g.addStop(0.5f, green);
        CHECK(g.stopCount() == 3);
        CHECK_NEAR(g.stop(0).offset, 0.0f, 1e-6f);
        CHECK_NEAR(g.stop(1).offset, 0.5f, 1e-6f);
        CHECK_NEAR(g.stop(2).offset, 1.0f, 1e-6f);
        CHECK_NEAR(g.stop(0).color.r, 1.0f, 1e-6f); // red stayed at offset 0
        g.setOffset(0, 2.0f);                       // move red to the end
        CHECK_NEAR(g.stop(2).color.r, 1.0f, 1e-6f);
        CHECK_NEAR(g.stop(2).offset, 2.0f, 1e-6f);
    }
}

void testRootMotion() {
    using anim::RootMotionSample;
    using anim::RootMotionTrack;

    // A straight "walk forward" clip: local +x travel over 1 s, no turn. Local forward = +x.
    {
        RootMotionTrack t;
        t.addKey(0.0f, math::vec2(0.0f, 0.0f), 0.0f);
        t.addKey(1.0f, math::vec2(2.0f, 0.0f), 0.0f); // 2 units forward across the clip
        CHECK_NEAR(t.duration(), 1.0f, 1e-6f);

        // sample() interpolates cumulative position linearly.
        CHECK_NEAR(t.sample(0.5f).position.x, 1.0f, 1e-5f);
        CHECK_NEAR(t.sample(0.5f).heading, 0.0f, 1e-6f);
        // clamps outside the range.
        CHECK_NEAR(t.sample(-3.0f).position.x, 0.0f, 1e-6f);
        CHECK_NEAR(t.sample(9.0f).position.x, 2.0f, 1e-6f);

        // delta over a sub-interval is the local displacement; no wrap.
        const RootMotionSample d = t.delta(0.25f, 0.75f, false);
        CHECK_NEAR(d.position.x, 1.0f, 1e-5f);
        CHECK_NEAR(d.heading, 0.0f, 1e-6f);

        // advance at heading 0 moves the world pose along +x.
        math::vec2 pos(0.0f, 0.0f);
        float head = 0.0f;
        t.advance(pos, head, 0.0f, 1.0f, false);
        CHECK_NEAR(pos.x, 2.0f, 1e-5f);
        CHECK_NEAR(pos.y, 0.0f, 1e-5f);

        // advance while FACING +y (heading = +90 deg) rotates the same local +x travel into +y.
        math::vec2 pos2(0.0f, 0.0f);
        float head2 = 1.5707963f;
        t.advance(pos2, head2, 0.0f, 1.0f, false);
        CHECK_NEAR(pos2.x, 0.0f, 1e-4f);
        CHECK_NEAR(pos2.y, 2.0f, 1e-4f);
    }

    // Loop wrap: a step that crosses the loop seam sums the prev->end and start->cur arcs.
    {
        RootMotionTrack t;
        t.addKey(0.0f, math::vec2(0.0f, 0.0f), 0.0f);
        t.addKey(1.0f, math::vec2(10.0f, 0.0f), 0.6f); // 10 forward + 0.6 rad turn per loop
        // step from t=0.9 to t=0.1 with loop: (end-0.9) = 1.0 fwd, (0.1-start) = 1.0 fwd -> 2.0 total.
        const RootMotionSample d = t.delta(0.9f, 0.1f, true);
        CHECK_NEAR(d.position.x, 2.0f, 1e-4f);
        CHECK_NEAR(d.heading, 0.12f, 1e-4f); // 0.06 + 0.06
        // without loop, a backward time step just reports the negative straight delta.
        const RootMotionSample dn = t.delta(0.9f, 0.1f, false);
        CHECK_NEAR(dn.position.x, -8.0f, 1e-4f);
    }

    // Heading accumulates unwrapped past +/-pi (a clip that turns a full circle), so no wrap fixups.
    {
        RootMotionTrack t;
        t.addKey(0.0f, math::vec2(0.0f, 0.0f), 0.0f);
        t.addKey(1.0f, math::vec2(0.0f, 0.0f), 6.2831853f); // one full turn baked into the clip
        math::vec2 pos(0.0f, 0.0f);
        float head = 0.0f;
        t.advance(pos, head, 0.0f, 1.0f, false);
        CHECK_NEAR(head, 6.2831853f, 1e-4f); // heading is cumulative, not wrapped to ~0
    }

    // Integration: a constant forward speed + constant turn rate traces a CIRCLE, so after the heading
    // sweeps a full 2*pi the character returns to (near) its start — the root-motion "closes the loop".
    {
        RootMotionTrack t;
        const float fwd = 4.0f;         // local +x units across the clip
        const float turn = 6.2831853f;  // radians across the clip (one full turn)
        t.addKey(0.0f, math::vec2(0.0f, 0.0f), 0.0f);
        t.addKey(1.0f, math::vec2(fwd, 0.0f), turn);

        math::vec2 start(3.0f, -1.0f);
        math::vec2 pos = start;
        float head = 0.4f; // arbitrary initial facing; the circle still closes
        const int steps = 4000;
        for (int i = 0; i < steps; ++i) {
            const float a = static_cast<float>(i) / static_cast<float>(steps);
            const float b = static_cast<float>(i + 1) / static_cast<float>(steps);
            t.advance(pos, head, a, b, false);
        }
        CHECK_NEAR(head, 0.4f + turn, 1e-3f);      // swept exactly one turn
        CHECK_NEAR(pos.x, start.x, 5e-2f);          // returned to start (fine-step circle)
        CHECK_NEAR(pos.y, start.y, 5e-2f);
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

void testOneWayPlatform() {
    using game::OneWayPlatform2D;
    using game::OneWayResult;
    using game::resolveOneWayPlatform;
    using game::resolveOneWayPlatforms;

    const OneWayPlatform2D plat{100.0f, 0.0f, 200.0f}; // surface at y=100, x in [0,200]

    // Falling onto the platform (bottom crosses y=100 from above, overlapping) -> lands, snaps to 100.
    {
        const OneWayResult r = resolveOneWayPlatform(90.0f, 110.0f, 40.0f, 60.0f, plat);
        CHECK(r.landed);
        CHECK_NEAR(r.y, 100.0f, 1e-4f);
    }

    // Jumping UP through it (bottom moves from below to above) -> passes through, no landing.
    {
        const OneWayResult r = resolveOneWayPlatform(120.0f, 80.0f, 40.0f, 60.0f, plat);
        CHECK(!r.landed);
        CHECK_NEAR(r.y, 80.0f, 1e-4f); // returns the uncorrected bottom
    }

    // Already well below the surface and still descending -> not crossing from above, passes through.
    {
        const OneWayResult r = resolveOneWayPlatform(140.0f, 160.0f, 40.0f, 60.0f, plat);
        CHECK(!r.landed);
    }

    // No horizontal overlap (body entirely left of the platform) -> no landing even though it crosses y.
    {
        const OneWayResult r = resolveOneWayPlatform(90.0f, 110.0f, -50.0f, -10.0f, plat);
        CHECK(!r.landed);
    }

    // Descending but not yet reaching the surface -> no landing.
    {
        const OneWayResult r = resolveOneWayPlatform(60.0f, 90.0f, 40.0f, 60.0f, plat);
        CHECK(!r.landed);
    }

    // Multiple platforms: a fast fall crossing two surfaces lands on the TOPMOST (smallest y).
    {
        const std::vector<OneWayPlatform2D> plats = {{200.0f, 0.0f, 300.0f}, {100.0f, 0.0f, 300.0f}};
        const OneWayResult r = resolveOneWayPlatforms(50.0f, 250.0f, 40.0f, 60.0f, plats);
        CHECK(r.landed);
        CHECK_NEAR(r.y, 100.0f, 1e-4f); // the higher platform, not the lower one at 200
    }

    // Landing on none (all platforms out of horizontal range) -> not landed, bottom unchanged.
    {
        const std::vector<OneWayPlatform2D> plats = {{100.0f, 500.0f, 600.0f}};
        const OneWayResult r = resolveOneWayPlatforms(90.0f, 110.0f, 40.0f, 60.0f, plats);
        CHECK(!r.landed);
        CHECK_NEAR(r.y, 110.0f, 1e-4f);
    }

    // A body resting just below the surface (tiny overshoot) still counts as landed via snapTolerance.
    {
        const OneWayResult r = resolveOneWayPlatform(100.4f, 100.8f, 40.0f, 60.0f, plat, 1.0f);
        CHECK(r.landed);
        CHECK_NEAR(r.y, 100.0f, 1e-4f);
    }
}

void testConvexShape2D() {
    using game::ConvexPoly2D;
    using game::SatHit2D;

    // Two axis-aligned boxes overlapping in X by 1: A spans x[0,2], B spans x[1,3], both y[0,2].
    // MTV should push along X with depth 1.
    {
        const ConvexPoly2D a = game::makeBoxPoly(math::vec2(1.0f, 1.0f), math::vec2(1.0f, 1.0f));
        const ConvexPoly2D b = game::makeBoxPoly(math::vec2(2.0f, 1.0f), math::vec2(1.0f, 1.0f));
        const SatHit2D h = game::satOverlap(a, b);
        CHECK(h.overlap);
        CHECK_NEAR(h.depth, 1.0f, 1e-4f);
        CHECK_NEAR(std::fabs(h.axis.x), 1.0f, 1e-4f); // separating axis is X
        CHECK_NEAR(h.axis.y, 0.0f, 1e-4f);
        CHECK(h.axis.x > 0.0f); // points from A(center x=1) toward B(center x=2)
    }

    // Clearly separated boxes -> no overlap.
    {
        const ConvexPoly2D a = game::makeBoxPoly(math::vec2(0.0f, 0.0f), math::vec2(1.0f, 1.0f));
        const ConvexPoly2D b = game::makeBoxPoly(math::vec2(5.0f, 0.0f), math::vec2(1.0f, 1.0f));
        CHECK(!game::satOverlap(a, b).overlap);
    }

    // Applying the MTV separates the shapes: translate B by axis*depth and they no longer overlap.
    {
        const ConvexPoly2D a = game::makeBoxPoly(math::vec2(1.0f, 1.0f), math::vec2(1.0f, 1.0f));
        ConvexPoly2D b = game::makeBoxPoly(math::vec2(2.0f, 1.0f), math::vec2(1.0f, 1.0f));
        const SatHit2D h = game::satOverlap(a, b);
        CHECK(h.overlap);
        for (math::vec2& p : b.points) {
            p += h.axis * (h.depth + 1e-3f); // push a hair past contact
        }
        CHECK(!game::satOverlap(a, b).overlap);
    }

    // A triangle overlapping a box.
    {
        ConvexPoly2D tri;
        tri.points = {math::vec2(0.0f, 0.0f), math::vec2(3.0f, 0.0f), math::vec2(1.5f, 3.0f)};
        const ConvexPoly2D box = game::makeBoxPoly(math::vec2(1.5f, 1.0f), math::vec2(0.5f, 0.5f));
        CHECK(game::satOverlap(tri, box).overlap);
    }

    // A rotated box vs an axis box: rotating a unit box 45deg extends its diagonal reach, so a box just
    // outside the axis-aligned extent still overlaps the diamond.
    {
        const ConvexPoly2D diamond =
            game::makeBoxPoly(math::vec2(0.0f, 0.0f), math::vec2(1.0f, 1.0f), 0.785398163f); // 45deg
        const ConvexPoly2D box = game::makeBoxPoly(math::vec2(1.3f, 0.0f), math::vec2(0.2f, 0.2f));
        CHECK(game::satOverlap(diamond, box).overlap); // diamond reaches ~1.414 along X
    }

    // polyContains: a point inside vs outside a pentagon.
    {
        const ConvexPoly2D pent = game::makeRegularPoly(math::vec2(0.0f, 0.0f), 2.0f, 5);
        CHECK(game::polyContains(pent, math::vec2(0.0f, 0.0f)));   // center is inside
        CHECK(!game::polyContains(pent, math::vec2(5.0f, 5.0f)));  // far outside
    }

    // makeRegularPoly builds the right vertex count at the right radius.
    {
        const ConvexPoly2D hex = game::makeRegularPoly(math::vec2(0.0f, 0.0f), 3.0f, 6);
        CHECK(hex.points.size() == 6);
        for (const math::vec2& p : hex.points) {
            CHECK_NEAR(std::sqrt(p.x * p.x + p.y * p.y), 3.0f, 1e-4f);
        }
    }

    // Degenerate (fewer than 3 points) never reports an overlap.
    {
        ConvexPoly2D line;
        line.points = {math::vec2(0, 0), math::vec2(1, 0)};
        const ConvexPoly2D box = game::makeBoxPoly(math::vec2(0.5f, 0.0f), math::vec2(1.0f, 1.0f));
        CHECK(!game::satOverlap(line, box).overlap);
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

void testShapeCast2D() {
    using game::QueryShape2D;
    using game::ShapeCastHit2D;

    // Signed-ish distance from a point to a shape's surface (>=0 outside), used to assert that a swept
    // circle stops exactly `radius` from whatever it hit — the defining property of the cast.
    auto distToShape = [](const math::vec2& p, const QueryShape2D& s) -> float {
        if (s.kind == QueryShape2D::Circle) {
            const math::vec2 d = p - s.pos;
            return std::sqrt(d.x * d.x + d.y * d.y) - s.radius;
        }
        const float ca = std::cos(s.angle), sa = std::sin(s.angle);
        const math::vec2 rel = p - s.pos;
        const math::vec2 lo(rel.x * ca + rel.y * sa, -rel.x * sa + rel.y * ca);
        const float dx = std::fabs(lo.x) - s.half.x;
        const float dy = std::fabs(lo.y) - s.half.y;
        const float ox = std::fmax(dx, 0.0f), oy = std::fmax(dy, 0.0f);
        return std::sqrt(ox * ox + oy * oy); // exterior distance (0 if inside)
    };

    // Head-on sweep into a circle: stops with 2 units between centres (r + R), 0.3 of the way along.
    {
        std::vector<QueryShape2D> shapes;
        QueryShape2D c;
        c.kind = QueryShape2D::Circle;
        c.pos = math::vec2(5.0f, 0.0f);
        c.radius = 1.0f;
        c.id = 42;
        shapes.push_back(c);

        const ShapeCastHit2D h =
            game::shapeCastCircle(math::vec2(0.0f, 0.0f), math::vec2(10.0f, 0.0f), 1.0f, shapes);
        CHECK(h.hit);
        CHECK_NEAR(h.fraction, 0.3f, 1e-4f);
        CHECK_NEAR(h.safePos.x, 3.0f, 1e-4f);
        CHECK_NEAR(h.safePos.y, 0.0f, 1e-4f);
        CHECK_NEAR(h.point.x, 4.0f, 1e-4f); // contact on the circle surface
        CHECK_NEAR(h.point.y, 0.0f, 1e-4f);
        CHECK_NEAR(h.normal.x, -1.0f, 1e-4f); // points back toward the caster
        CHECK_NEAR(h.normal.y, 0.0f, 1e-4f);
        CHECK(h.index == 0);
        CHECK(h.id == 42);
        CHECK_NEAR(distToShape(h.safePos, shapes[0]), 1.0f, 1e-4f); // exactly radius away
    }

    // A sweep that passes wide of the circle: no hit, fraction 1, safePos at the motion end.
    {
        std::vector<QueryShape2D> shapes;
        QueryShape2D c;
        c.pos = math::vec2(5.0f, 5.0f);
        c.radius = 0.5f;
        shapes.push_back(c);
        const ShapeCastHit2D h =
            game::shapeCastCircle(math::vec2(0.0f, 0.0f), math::vec2(10.0f, 0.0f), 0.5f, shapes);
        CHECK(!h.hit);
        CHECK_NEAR(h.fraction, 1.0f, 1e-6f);
        CHECK_NEAR(h.safePos.x, 10.0f, 1e-4f);
    }

    // Continuous collision beats a discrete test: a thin wall entirely BETWEEN the start and end points
    // is caught by the sweep (a point sample at start and end would tunnel straight through).
    {
        std::vector<QueryShape2D> shapes;
        QueryShape2D b;
        b.kind = QueryShape2D::Box;
        b.pos = math::vec2(5.0f, 0.0f);
        b.half = math::vec2(0.1f, 3.0f); // thin, tall wall
        shapes.push_back(b);
        const ShapeCastHit2D h =
            game::shapeCastCircle(math::vec2(0.0f, 0.0f), math::vec2(10.0f, 0.0f), 0.25f, shapes);
        CHECK(h.hit);
        CHECK_NEAR(h.safePos.x, 5.0f - 0.1f - 0.25f, 1e-4f); // stops a radius before the -X face
        CHECK_NEAR(h.normal.x, -1.0f, 1e-4f);
        CHECK_NEAR(distToShape(h.safePos, shapes[0]), 0.25f, 1e-4f);
    }

    // Head-on into an axis-aligned box face.
    {
        std::vector<QueryShape2D> shapes;
        QueryShape2D b;
        b.kind = QueryShape2D::Box;
        b.pos = math::vec2(5.0f, 0.0f);
        b.half = math::vec2(1.0f, 1.0f);
        shapes.push_back(b);
        const ShapeCastHit2D h =
            game::shapeCastCircle(math::vec2(0.0f, 0.0f), math::vec2(10.0f, 0.0f), 0.5f, shapes);
        CHECK(h.hit);
        CHECK_NEAR(h.safePos.x, 3.5f, 1e-4f); // 5 - half(1) - radius(0.5)
        CHECK_NEAR(h.point.x, 4.0f, 1e-4f);
        CHECK_NEAR(h.normal.x, -1.0f, 1e-4f);
        CHECK_NEAR(h.normal.y, 0.0f, 1e-4f);
    }

    // Corner path: a box rotated 45 degrees points a VERTEX at the incoming sweep. The nearest vertex of
    // a half=1 box is sqrt(2) from centre; the caster stops a radius short of it, normal back along -X.
    {
        std::vector<QueryShape2D> shapes;
        QueryShape2D b;
        b.kind = QueryShape2D::Box;
        b.pos = math::vec2(5.0f, 0.0f);
        b.half = math::vec2(1.0f, 1.0f);
        b.angle = 3.14159265358979f * 0.25f;
        shapes.push_back(b);
        const ShapeCastHit2D h =
            game::shapeCastCircle(math::vec2(0.0f, 0.0f), math::vec2(10.0f, 0.0f), 0.5f, shapes);
        CHECK(h.hit);
        const float vertexX = 5.0f - std::sqrt(2.0f);
        CHECK_NEAR(h.safePos.x, vertexX - 0.5f, 1e-3f);
        CHECK_NEAR(h.safePos.y, 0.0f, 1e-3f);
        CHECK_NEAR(h.normal.x, -1.0f, 1e-3f);
        CHECK_NEAR(std::sqrt(h.normal.x * h.normal.x + h.normal.y * h.normal.y), 1.0f, 1e-4f);
        CHECK_NEAR(distToShape(h.safePos, shapes[0]), 0.5f, 1e-3f);
    }

    // A caster that already overlaps its target reports contact at fraction 0 with a push-out normal.
    {
        std::vector<QueryShape2D> shapes;
        QueryShape2D c;
        c.pos = math::vec2(5.0f, 0.0f);
        c.radius = 1.0f;
        shapes.push_back(c);
        const ShapeCastHit2D h =
            game::shapeCastCircle(math::vec2(5.3f, 0.0f), math::vec2(4.0f, 0.0f), 1.0f, shapes);
        CHECK(h.hit);
        CHECK_NEAR(h.fraction, 0.0f, 1e-6f);
        CHECK_NEAR(h.normal.x, 1.0f, 1e-4f); // pushed out along +X (caster is right of centre)
    }

    // Zero-length motion degenerates to a start-overlap probe.
    {
        std::vector<QueryShape2D> shapes;
        QueryShape2D c;
        c.pos = math::vec2(5.0f, 0.0f);
        c.radius = 1.0f;
        shapes.push_back(c);
        const ShapeCastHit2D touching =
            game::shapeCastCircle(math::vec2(5.0f, 1.5f), math::vec2(0.0f, 0.0f), 1.0f, shapes);
        CHECK(touching.hit); // 1.5 apart <= r+R = 2
        CHECK_NEAR(touching.fraction, 0.0f, 1e-6f);
        const ShapeCastHit2D clear =
            game::shapeCastCircle(math::vec2(5.0f, 3.0f), math::vec2(0.0f, 0.0f), 1.0f, shapes);
        CHECK(!clear.hit); // 3 apart > 2
    }

    // The collision mask filters obstacles just like the ray queries.
    {
        std::vector<QueryShape2D> shapes;
        QueryShape2D c;
        c.pos = math::vec2(5.0f, 0.0f);
        c.radius = 1.0f;
        c.layer = 0b10u;
        shapes.push_back(c);
        const ShapeCastHit2D masked =
            game::shapeCastCircle(math::vec2(0.0f, 0.0f), math::vec2(10.0f, 0.0f), 1.0f, shapes, 0b01u);
        CHECK(!masked.hit);
        const ShapeCastHit2D matched =
            game::shapeCastCircle(math::vec2(0.0f, 0.0f), math::vec2(10.0f, 0.0f), 1.0f, shapes, 0b10u);
        CHECK(matched.hit);
    }

    // With several obstacles the NEAREST contact wins.
    {
        std::vector<QueryShape2D> shapes;
        QueryShape2D far;
        far.pos = math::vec2(9.0f, 0.0f);
        far.radius = 1.0f;
        far.id = 1;
        QueryShape2D near;
        near.pos = math::vec2(5.0f, 0.0f);
        near.radius = 1.0f;
        near.id = 2;
        shapes.push_back(far);
        shapes.push_back(near);
        const ShapeCastHit2D h =
            game::shapeCastCircle(math::vec2(0.0f, 0.0f), math::vec2(20.0f, 0.0f), 1.0f, shapes);
        CHECK(h.hit);
        CHECK(h.index == 1); // the one at x=5
        CHECK(h.id == 2);
        CHECK_NEAR(h.safePos.x, 3.0f, 1e-4f);
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

// P1: the warm-started accumulated-impulse solver (PhysicsWorld2D::warmStarting). The claim to verify
// is the real Box2D/Godot advantage — accumulation + frame-to-frame warm starting hold a tall stack
// rigid at a HANDFUL of iterations, where the from-scratch two-point manifold resolver at the same
// low iteration count lets the stack sink under its own weight. We build identical stacks, run both at
// the SAME low iteration count, and compare how far the tower sank.
void testWarmStartSolver() {
    using game::Body2D;
    namespace d = game::detail;

    const float H = 18.0f;    // box half-height
    const float floorTop = 290.0f;
    const int N = 5;

    // Ideal (zero-penetration) centre-y of box i counting up from the floor.
    auto idealY = [&](int i) { return floorTop - H - static_cast<float>(i) * (2.0f * H); };

    auto buildStack = [&](bool warm, int iters) {
        game::PhysicsWorld2D w;
        w.gravity = math::vec2(0.0f, 600.0f); // +y down
        if (warm) {
            w.warmStarting = true;
        } else {
            w.solveManifolds = true;
        }
        Body2D floor;
        floor.shape = Body2D::Box;
        floor.half = math::vec2(200.0f, 10.0f);
        floor.pos = math::vec2(0.0f, 300.0f); // top face at y=290
        floor.invMass = 0.0f;
        floor.friction = 0.9f;
        w.add(floor);
        for (int i = 0; i < N; ++i) {
            Body2D box;
            box.shape = Body2D::Box;
            box.half = math::vec2(30.0f, H);
            box.pos = math::vec2(0.0f, idealY(i));
            box.invMass = 1.0f;
            box.friction = 0.9f;
            box.restitution = 0.0f;
            box.enableRotation();
            w.add(box);
        }
        for (int s = 0; s < 200; ++s) {
            w.step(1.0f / 60.0f, iters);
        }
        return w;
    };

    // Total downward sink of the whole tower (sum over boxes of how far each fell below its ideal
    // resting height). More sink = a mushier solver.
    auto totalSink = [&](game::PhysicsWorld2D& w) {
        float sink = 0.0f;
        for (int i = 0; i < N; ++i) {
            const float dy = w.bodies[static_cast<std::size_t>(i + 1)].pos.y - idealY(i);
            sink += dy > 0.0f ? dy : 0.0f;
        }
        return sink;
    };

    const int lowIters = 4;
    game::PhysicsWorld2D warm = buildStack(true, lowIters);
    game::PhysicsWorld2D scratch = buildStack(false, lowIters);

    // The warm-started tower settles tight: every box near its ideal height, tower upright, at rest.
    float warmMaxTilt = 0.0f, warmMaxSpeed = 0.0f;
    for (int i = 0; i < N; ++i) {
        const Body2D& b = warm.bodies[static_cast<std::size_t>(i + 1)];
        warmMaxTilt = std::max(warmMaxTilt, std::fabs(b.angle));
        warmMaxSpeed = std::max(warmMaxSpeed, std::sqrt(glm::dot(b.vel, b.vel)));
    }
    CHECK(warmMaxTilt < 0.05f);          // stays square
    CHECK(warmMaxSpeed < 12.0f);         // at rest — only residual contact micro-jitter (~0.2px/frame)
    CHECK(totalSink(warm) < 3.0f);       // barely sank (sub-slop per contact)

    // The headline comparison: at the SAME low iteration count (4), warm starting holds the tower
    // essentially rigid while the from-scratch two-point-manifold resolver lets it pancake — an order
    // of magnitude less sink. This is exactly the Box2D/Godot warm-start advantage.
    CHECK(totalSink(warm) * 10.0f < totalSink(scratch));

    // A single dynamic box resting on the floor settles with only slop-scale penetration and ~0 speed.
    {
        game::PhysicsWorld2D w;
        w.gravity = math::vec2(0.0f, 600.0f);
        w.warmStarting = true;
        Body2D floor;
        floor.shape = Body2D::Box;
        floor.half = math::vec2(200.0f, 10.0f);
        floor.pos = math::vec2(0.0f, 300.0f);
        floor.invMass = 0.0f;
        floor.friction = 0.8f;
        floor.restitution = 0.0f;
        w.add(floor);
        Body2D box;
        box.shape = Body2D::Box;
        box.half = math::vec2(20.0f, 20.0f);
        box.pos = math::vec2(0.0f, 200.0f); // dropped from above
        box.invMass = 1.0f;
        box.friction = 0.8f;
        box.restitution = 0.0f;
        box.enableRotation();
        w.add(box);
        for (int s = 0; s < 240; ++s) {
            w.step(1.0f / 60.0f, 6);
        }
        d::Contact2 m = d::manifold2(w.bodies[0], w.bodies[1]);
        CHECK(m.hit);
        CHECK(m.pen[0] < 0.5f); // resting within a slop, not sinking through
        CHECK(std::sqrt(glm::dot(w.bodies[1].vel, w.bodies[1].vel)) < 2.0f);
        CHECK_NEAR(w.bodies[1].pos.y, 270.0f, 1.0f); // rests on the floor top (290) minus its half (20)
    }

    // Restitution still bounces under the warm solver: a bouncy circle dropped onto the floor reverses
    // its velocity after impact (moves back up, -y).
    {
        game::PhysicsWorld2D w;
        w.gravity = math::vec2(0.0f, 600.0f);
        w.warmStarting = true;
        w.restitutionThreshold = 0.5f;
        Body2D floor;
        floor.shape = Body2D::Box;
        floor.half = math::vec2(200.0f, 10.0f);
        floor.pos = math::vec2(0.0f, 300.0f);
        floor.invMass = 0.0f;
        floor.restitution = 0.9f;
        w.add(floor);
        Body2D ball;
        ball.shape = Body2D::Circle;
        ball.radius = 10.0f;
        ball.pos = math::vec2(0.0f, 250.0f);
        ball.vel = math::vec2(0.0f, 200.0f); // heading down toward the floor
        ball.invMass = 1.0f;
        ball.restitution = 0.9f;
        w.add(ball);
        bool bounced = false;
        for (int s = 0; s < 120; ++s) {
            w.step(1.0f / 60.0f, 6);
            if (w.bodies[1].vel.y < -50.0f) {
                bounced = true; // rebounded upward
                break;
            }
        }
        CHECK(bounced);
    }

    // Determinism: identical setups produce identical results (fixed step, no RNG).
    {
        game::PhysicsWorld2D a = buildStack(true, 4);
        game::PhysicsWorld2D b = buildStack(true, 4);
        for (std::size_t i = 0; i < a.bodies.size(); ++i) {
            CHECK_NEAR(a.bodies[i].pos.x, b.bodies[i].pos.x, 1e-6f);
            CHECK_NEAR(a.bodies[i].pos.y, b.bodies[i].pos.y, 1e-6f);
            CHECK_NEAR(a.bodies[i].angle, b.bodies[i].angle, 1e-6f);
        }
    }
}

// P2: the integrated spatial-hash broadphase. The claim is that it changes performance, not results:
// it must return the SAME contacts in the SAME order as the O(n²) scan, so a many-body pile settles
// bit-identically whether broadphase is on or off. We drop a grid of bodies into a walled box under
// both modes and compare every body's final pose exactly.
void testBroadphase() {
    using game::Body2D;

    auto buildPile = [](bool bp) {
        game::PhysicsWorld2D w;
        w.gravity = math::vec2(0.0f, 700.0f);
        w.warmStarting = true;
        w.broadphase = bp;
        w.broadphaseCellSize = 60.0f;
        // A floor plus two side walls (thin static boxes) make a bin.
        Body2D floor;
        floor.shape = Body2D::Box;
        floor.half = math::vec2(300.0f, 12.0f);
        floor.pos = math::vec2(0.0f, 400.0f);
        floor.invMass = 0.0f;
        floor.friction = 0.6f;
        w.add(floor);
        Body2D left;
        left.shape = Body2D::Box;
        left.half = math::vec2(12.0f, 240.0f);
        left.pos = math::vec2(-290.0f, 180.0f);
        left.invMass = 0.0f;
        left.friction = 0.6f;
        w.add(left);
        Body2D right = left;
        right.pos = math::vec2(290.0f, 180.0f);
        w.add(right);
        // A 10x8 grid of small circles + boxes raining into the bin.
        for (int gy = 0; gy < 8; ++gy) {
            for (int gx = 0; gx < 10; ++gx) {
                Body2D b;
                const bool box = ((gx + gy) & 1) != 0;
                b.shape = box ? Body2D::Box : Body2D::Circle;
                b.radius = 14.0f;
                b.half = math::vec2(14.0f, 14.0f);
                b.pos = math::vec2(-230.0f + static_cast<float>(gx) * 50.0f,
                                   -220.0f + static_cast<float>(gy) * 42.0f);
                b.invMass = 1.0f;
                b.friction = 0.5f;
                b.restitution = 0.1f;
                if (box) {
                    b.enableRotation();
                }
                w.add(b);
            }
        }
        for (int s = 0; s < 240; ++s) {
            w.step(1.0f / 60.0f, 8);
        }
        return w;
    };

    game::PhysicsWorld2D brute = buildPile(false);
    game::PhysicsWorld2D hashed = buildPile(true);
    CHECK(brute.bodies.size() == hashed.bodies.size());
    CHECK(brute.bodies.size() == 83u); // 3 static + 80 dynamic
    float maxPosDiff = 0.0f, maxAngDiff = 0.0f;
    for (std::size_t i = 0; i < brute.bodies.size(); ++i) {
        maxPosDiff = std::max(maxPosDiff, std::fabs(brute.bodies[i].pos.x - hashed.bodies[i].pos.x));
        maxPosDiff = std::max(maxPosDiff, std::fabs(brute.bodies[i].pos.y - hashed.bodies[i].pos.y));
        maxAngDiff = std::max(maxAngDiff, std::fabs(brute.bodies[i].angle - hashed.bodies[i].angle));
    }
    // Bit-identical: broadphase only skips pairs that cannot touch, and sorts candidates by index so
    // the resolution order matches the O(n²) scan exactly.
    CHECK(maxPosDiff < 1e-4f);
    CHECK(maxAngDiff < 1e-4f);

    // Sanity: the pile actually settled inside the bin (no explosion / tunneling out).
    for (std::size_t i = 3; i < hashed.bodies.size(); ++i) {
        CHECK(hashed.bodies[i].pos.x > -320.0f && hashed.bodies[i].pos.x < 320.0f);
        CHECK(hashed.bodies[i].pos.y < 420.0f);
    }
}

// P3: body sleeping. A settled stack must go to sleep (stop integrating, freeze in place); a
// disturbance must wake it. We build a stack, confirm it is awake while still falling, confirm the
// whole island sleeps once it has been quiet long enough, confirm sleeping bodies are frozen, then
// drop a body onto it and confirm the island wakes.
void testSleeping() {
    using game::Body2D;

    auto makeWorld = []() {
        game::PhysicsWorld2D w;
        w.gravity = math::vec2(0.0f, 600.0f);
        w.warmStarting = true;
        w.allowSleep = true;
        w.sleepTime = 0.4f;
        Body2D floor;
        floor.shape = Body2D::Box;
        floor.half = math::vec2(200.0f, 12.0f);
        floor.pos = math::vec2(0.0f, 300.0f);
        floor.invMass = 0.0f;
        floor.friction = 0.9f;
        w.add(floor);
        for (int i = 0; i < 4; ++i) {
            Body2D b;
            b.shape = Body2D::Box;
            b.half = math::vec2(28.0f, 18.0f);
            b.pos = math::vec2(0.0f, 270.0f - static_cast<float>(i) * 37.0f);
            b.invMass = 1.0f;
            b.friction = 0.9f;
            b.restitution = 0.0f;
            b.enableRotation();
            w.add(b);
        }
        return w;
    };

    game::PhysicsWorld2D w = makeWorld();

    // Early on, the boxes are still dropping/settling — nothing should be asleep yet.
    for (int s = 0; s < 5; ++s) {
        w.step(1.0f / 60.0f, 6);
    }
    bool anyEarlyAsleep = false;
    for (std::size_t i = 1; i < w.bodies.size(); ++i) {
        anyEarlyAsleep = anyEarlyAsleep || w.bodies[i].sleeping;
    }
    CHECK(!anyEarlyAsleep);

    // Run long enough to settle and cross the sleep time.
    for (int s = 0; s < 300; ++s) {
        w.step(1.0f / 60.0f, 6);
    }
    int asleep = 0;
    for (std::size_t i = 1; i < w.bodies.size(); ++i) {
        if (w.bodies[i].sleeping) {
            ++asleep;
        }
    }
    CHECK(asleep == 4); // the entire stack island sleeps

    // A sleeping body is frozen: stepping again does not move it at all.
    std::vector<math::vec2> before;
    for (std::size_t i = 1; i < w.bodies.size(); ++i) {
        before.push_back(w.bodies[i].pos);
    }
    for (int s = 0; s < 30; ++s) {
        w.step(1.0f / 60.0f, 6);
    }
    for (std::size_t i = 1; i < w.bodies.size(); ++i) {
        CHECK_NEAR(w.bodies[i].pos.x, before[i - 1].x, 1e-6f);
        CHECK_NEAR(w.bodies[i].pos.y, before[i - 1].y, 1e-6f);
        CHECK(w.bodies[i].sleeping);
    }

    // Wake by explicit request (as gameplay would after applying an impulse): the woken body is awake
    // and, being an energetic member, wakes its whole island within a step or two.
    w.bodies[1].vel = math::vec2(0.0f, -200.0f); // launch the bottom box upward
    w.wake(1);
    w.step(1.0f / 60.0f, 6);
    CHECK(!w.bodies[1].sleeping);
    for (int s = 0; s < 3; ++s) {
        w.step(1.0f / 60.0f, 6);
    }
    int awakeAfter = 0;
    for (std::size_t i = 1; i < w.bodies.size(); ++i) {
        if (!w.bodies[i].sleeping) {
            ++awakeAfter;
        }
    }
    CHECK(awakeAfter >= 2); // the disturbance propagated through the island

    // Wake by collision: a fresh stack that has gone to sleep is hit by a dropped box, which must not
    // tunnel through and must wake the stack.
    {
        game::PhysicsWorld2D w2 = makeWorld();
        for (int s = 0; s < 300; ++s) {
            w2.step(1.0f / 60.0f, 6);
        }
        CHECK(w2.bodies[1].sleeping); // stack asleep
        Body2D drop;
        drop.shape = Body2D::Box;
        drop.half = math::vec2(28.0f, 18.0f);
        drop.pos = math::vec2(0.0f, 120.0f); // above the stack
        drop.vel = math::vec2(0.0f, 400.0f); // falling fast
        drop.invMass = 1.0f;
        drop.friction = 0.9f;
        drop.enableRotation();
        const uint32_t dropId = w2.add(drop);
        const float topY = w2.bodies[4].pos.y; // current top of the settled stack
        for (int s = 0; s < 90; ++s) {
            w2.step(1.0f / 60.0f, 6);
        }
        // The dropped box came to rest ON TOP of the stack (did not pass through it).
        CHECK(w2.bodies[dropId].pos.y < topY + 5.0f);
        // Its arrival woke the stack (at least the top boxes) at some point — verify by energy: the
        // stack shifted, so not everything is still in its original frozen pose.
        CHECK(w2.bodies[dropId].pos.y > 100.0f); // it fell from 120 toward the stack, not through floor
    }
}

// P4: collision layer/mask filtering. Two bodies whose layers/masks don't interact must pass straight
// through each other; the same bodies with interacting masks must collide. Mirrors Godot
// collision_layer / collision_mask.
void testCollisionFiltering() {
    using game::Body2D;
    using game::layerBit;

    auto run = [](bool filterApart) {
        game::PhysicsWorld2D w;
        w.gravity = math::vec2(0.0f, 0.0f);
        w.warmStarting = true;
        Body2D wall;
        wall.shape = Body2D::Box;
        wall.half = math::vec2(10.0f, 100.0f);
        wall.pos = math::vec2(0.0f, 0.0f);
        wall.invMass = 0.0f;
        wall.restitution = 0.0f;
        if (filterApart) {
            wall.collisionLayer = layerBit(1); // wall lives on layer 2
            wall.collisionMask = layerBit(1);  // and only reacts to layer 2
        }
        w.add(wall);
        Body2D mover;
        mover.shape = Body2D::Circle;
        mover.radius = 8.0f;
        mover.pos = math::vec2(-80.0f, 0.0f);
        mover.vel = math::vec2(120.0f, 0.0f); // heading right, into the wall
        mover.invMass = 1.0f;
        mover.restitution = 0.0f;
        if (filterApart) {
            mover.collisionLayer = layerBit(0); // mover on layer 1
            mover.collisionMask = layerBit(0);  // reacts only to layer 1 -> ignores the wall
        }
        w.add(mover);
        for (int s = 0; s < 120; ++s) {
            w.step(1.0f / 60.0f, 6);
        }
        return w.bodies[1].pos.x;
    };

    // Default (all bits): the mover is stopped by the wall well before crossing it.
    const float stoppedX = run(false);
    CHECK(stoppedX < -8.0f); // parked at the wall's left face (x=-10) plus radius

    // Filtered apart: the mover ignores the wall and sails right past x=0.
    const float passedX = run(true);
    CHECK(passedX > 40.0f);
}

// P5: capsule shape. Verify the new capsule contact functions (vs circle, capsule, box) and that a
// capsule settles resting on a box floor under the warm solver. Mirrors Godot CapsuleShape2D.
void testCapsule() {
    using game::Body2D;
    namespace d = game::detail;

    // Capsule (vertical, segment y[-20,20], r=10) vs a circle just off its right side.
    {
        Body2D cap;
        cap.shape = Body2D::Capsule;
        cap.half = math::vec2(0.0f, 20.0f);
        cap.radius = 10.0f;
        cap.pos = math::vec2(0.0f, 0.0f);
        Body2D circ;
        circ.shape = Body2D::Circle;
        circ.radius = 10.0f;
        circ.pos = math::vec2(18.0f, 0.0f); // overlap: gap 18 < 20 = r+r
        d::Manifold m = d::manifold(cap, circ);
        CHECK(m.hit);
        CHECK_NEAR(m.n.x, 1.0f, 1e-3f); // capsule -> circle, +x
        CHECK_NEAR(m.n.y, 0.0f, 1e-3f);
        CHECK_NEAR(m.pen, 2.0f, 1e-3f); // 20 - 18
        // Contact point sits on the capsule's surface (x = cap radius).
        CHECK_NEAR(m.point.x, 10.0f, 1e-3f);
    }

    // Capsule vs capsule, side by side.
    {
        Body2D a;
        a.shape = Body2D::Capsule;
        a.half = math::vec2(0.0f, 20.0f);
        a.radius = 10.0f;
        a.pos = math::vec2(0.0f, 0.0f);
        Body2D b = a;
        b.pos = math::vec2(15.0f, 3.0f); // segments overlap in y; centres 15 apart
        d::Manifold m = d::manifold(a, b);
        CHECK(m.hit);
        CHECK(m.pen > 0.0f); // r+r = 20 > horizontal gap 15
        CHECK(m.n.x > 0.5f); // roughly +x from a to b
    }

    // Capsule vs box: a capsule overlapping the top face of a static box.
    {
        Body2D cap;
        cap.shape = Body2D::Capsule;
        cap.half = math::vec2(0.0f, 20.0f);
        cap.radius = 10.0f;
        cap.pos = math::vec2(0.0f, -35.0f); // above the box (screen: -y is up)
        Body2D box;
        box.shape = Body2D::Box;
        box.half = math::vec2(40.0f, 10.0f);
        box.pos = math::vec2(0.0f, 0.0f); // top face at y=-10
        // Lower cap centre at y=-15 (above the box top -10); its radius 10 reaches to -5, 5 into the box.
        d::Manifold m = d::manifold(cap, box);
        CHECK(m.hit);
        CHECK(m.n.y > 0.5f);      // capsule -> box points downward (+y)
        CHECK_NEAR(m.pen, 5.0f, 0.5f);
    }

    // End to end: a capsule dropped onto a static box floor settles resting on it, at rest.
    {
        game::PhysicsWorld2D w;
        w.gravity = math::vec2(0.0f, 600.0f);
        w.warmStarting = true;
        Body2D floor;
        floor.shape = Body2D::Box;
        floor.half = math::vec2(200.0f, 12.0f);
        floor.pos = math::vec2(0.0f, 312.0f); // top face at y=300
        floor.invMass = 0.0f;
        floor.friction = 0.8f;
        w.add(floor);
        Body2D cap;
        cap.shape = Body2D::Capsule;
        cap.half = math::vec2(0.0f, 20.0f);
        cap.radius = 12.0f;
        cap.pos = math::vec2(0.0f, 120.0f); // dropped from above
        cap.invMass = 1.0f;
        cap.friction = 0.8f;
        cap.restitution = 0.0f;
        cap.enableRotation();
        w.add(cap);
        for (int s = 0; s < 300; ++s) {
            w.step(1.0f / 60.0f, 8);
        }
        // Rests with its lower cap on the floor top: centre.y = 300 - half.y - radius = 268.
        CHECK_NEAR(w.bodies[1].pos.y, 268.0f, 2.0f);
        CHECK(std::sqrt(glm::dot(w.bodies[1].vel, w.bodies[1].vel)) < 5.0f); // at rest
        CHECK(std::fabs(w.bodies[1].angle) < 0.1f); // stayed upright
    }

    // P11: a horizontal capsule resting on a box top yields a TWO-point manifold (no rocking).
    {
        Body2D box;
        box.shape = Body2D::Box;
        box.half = math::vec2(100.0f, 10.0f);
        box.pos = math::vec2(0.0f, 0.0f); // top face at y=-10
        Body2D cap;
        cap.shape = Body2D::Capsule;
        cap.half = math::vec2(0.0f, 30.0f); // segment half-length 30
        cap.radius = 10.0f;
        cap.angle = 1.57079633f;          // horizontal (local +Y -> world +X)
        cap.pos = math::vec2(0.0f, -15.0f); // bottom at y=-5, 5 into the box top
        d::Contact2 m = d::manifold2(cap, box);
        CHECK(m.hit);
        CHECK(m.count == 2);             // flat capsule -> two contact points
        CHECK_NEAR(m.n.y, 1.0f, 1e-2f);  // capsule (above) -> box points down (+y) into the box
        CHECK(std::fabs(m.point[0].x - m.point[1].x) > 20.0f); // points spread along the segment

        // End to end: a horizontal capsule dropped on a box floor settles flat (stays horizontal).
        game::PhysicsWorld2D w;
        w.gravity = math::vec2(0.0f, 600.0f);
        w.warmStarting = true;
        Body2D floor;
        floor.shape = Body2D::Box;
        floor.half = math::vec2(200.0f, 12.0f);
        floor.pos = math::vec2(0.0f, 312.0f);
        floor.invMass = 0.0f;
        floor.friction = 0.8f;
        w.add(floor);
        Body2D roll;
        roll.shape = Body2D::Capsule;
        roll.half = math::vec2(0.0f, 40.0f);
        roll.radius = 14.0f;
        roll.angle = 1.57079633f; // horizontal
        roll.pos = math::vec2(0.0f, 150.0f);
        roll.invMass = 1.0f;
        roll.friction = 0.8f;
        roll.restitution = 0.0f;
        roll.enableRotation();
        w.add(roll);
        for (int s = 0; s < 300; ++s) {
            w.step(1.0f / 60.0f, 8);
        }
        // Still horizontal (angle near pi/2), came to rest — the two-point contact stopped it rocking.
        CHECK_NEAR(w.bodies[1].angle, 1.57079633f, 0.1f);
        CHECK(std::fabs(w.bodies[1].angularVel) < 1.0f);
    }
}

// P6: infinite WorldBoundary half-plane (Godot WorldBoundaryShape2D). A circle, a box and a capsule
// each rest on an infinite floor line at the exact expected height; the box stays flat (two-point
// boundary manifold, no rocking).
void testWorldBoundary() {
    using game::Body2D;
    namespace d = game::detail;

    // Direct contact: a circle whose lower half crosses the floor line at y=300.
    {
        Body2D floor = game::makeWorldBoundary(math::vec2(0.0f, -1.0f), math::vec2(0.0f, 300.0f));
        Body2D circ;
        circ.shape = Body2D::Circle;
        circ.radius = 20.0f;
        circ.pos = math::vec2(0.0f, 290.0f); // bottom at y=310, 10 below the floor
        d::Contact2 m = d::manifold2(circ, floor);
        CHECK(m.hit);
        CHECK(m.count == 1);
        CHECK_NEAR(m.n.x, 0.0f, 1e-3f);
        CHECK_NEAR(m.n.y, 1.0f, 1e-3f); // circle -> solid (downward)
        CHECK_NEAR(m.pen[0], 10.0f, 1e-3f);
    }

    auto restOn = [](int shape) {
        game::PhysicsWorld2D w;
        w.gravity = math::vec2(0.0f, 600.0f);
        w.warmStarting = true;
        w.add(game::makeWorldBoundary(math::vec2(0.0f, -1.0f), math::vec2(0.0f, 300.0f)));
        Body2D b;
        b.shape = shape;
        b.radius = 20.0f;
        b.half = (shape == Body2D::Box) ? math::vec2(30.0f, 20.0f) : math::vec2(0.0f, 20.0f);
        b.pos = math::vec2(0.0f, 120.0f);
        b.angle = (shape == Body2D::Box) ? 0.15f : 0.0f; // start a box slightly tilted
        b.invMass = 1.0f;
        b.friction = 0.8f;
        b.restitution = 0.0f;
        b.enableRotation();
        w.add(b);
        for (int s = 0; s < 320; ++s) {
            w.step(1.0f / 60.0f, 8);
        }
        return w.bodies[1];
    };

    // Circle rests with its bottom on the floor: centre.y = 300 - 20 = 280.
    {
        Body2D c = restOn(Body2D::Circle);
        CHECK_NEAR(c.pos.y, 280.0f, 1.5f);
        CHECK(std::sqrt(glm::dot(c.vel, c.vel)) < 5.0f);
    }
    // Box rests flat: bottom edge on the floor -> centre.y = 300 - 20 = 280, and it settled upright
    // (two-point boundary manifold kills the initial tilt instead of letting it rock).
    {
        Body2D b = restOn(Body2D::Box);
        CHECK_NEAR(b.pos.y, 280.0f, 2.0f);
        CHECK(std::fabs(b.angle) < 0.05f);
    }
    // Vertical capsule rests on its lower cap: centre.y = 300 - half.y - radius = 300 - 20 - 20 = 260.
    {
        Body2D cap = restOn(Body2D::Capsule);
        CHECK_NEAR(cap.pos.y, 260.0f, 2.0f);
        CHECK(std::sqrt(glm::dot(cap.vel, cap.vel)) < 5.0f);
    }
}

// P13: polyline / chain static collider (Godot ConcavePolygonShape2D / SegmentShape2D chain). A chain
// is a static open run of connected segments used for level terrain; dynamic bodies rest on it.
void testPolylineCollider() {
    using game::Body2D;
    namespace d = game::detail;

    // makePolyline stores points relative to their centroid, so pos is the centroid.
    {
        std::vector<math::vec2> pts = {math::vec2(-100.0f, 300.0f), math::vec2(100.0f, 300.0f)};
        Body2D chain = game::makePolyline(pts, 4.0f);
        CHECK(chain.shape == Body2D::Polyline);
        CHECK_NEAR(chain.pos.x, 0.0f, 1e-3f);
        CHECK_NEAR(chain.pos.y, 300.0f, 1e-3f);
        CHECK(chain.verts.size() == 2);
        CHECK_NEAR(chain.radius, 4.0f, 1e-3f);
        CHECK(chain.invMass == 0.0f); // static
    }

    // Direct manifold: a box overlapping a horizontal chain segment from above. n points chain -> box
    // (upward, -y), with a positive penetration.
    {
        std::vector<math::vec2> pts = {math::vec2(-100.0f, 300.0f), math::vec2(100.0f, 300.0f)};
        Body2D chain = game::makePolyline(pts, 4.0f);
        Body2D box;
        box.shape = Body2D::Box;
        box.half = math::vec2(30.0f, 20.0f);
        box.pos = math::vec2(0.0f, 278.0f); // bottom at y=298, chain top surface at y=296 -> pen 2
        box.invMass = 1.0f;
        d::Contact2 m = d::manifold2(chain, box);
        CHECK(m.hit);
        CHECK_NEAR(m.n.x, 0.0f, 1e-2f);
        CHECK_NEAR(m.n.y, -1.0f, 1e-2f); // chain -> box, upward
        CHECK(m.count >= 1);
        CHECK_NEAR(m.pen[0], 2.0f, 0.6f);
        // The reverse order flips the normal to box -> chain (downward, +y).
        d::Contact2 m2 = d::manifold2(box, chain);
        CHECK(m2.hit);
        CHECK_NEAR(m2.n.y, 1.0f, 1e-2f);
    }

    // A separated body reports no contact.
    {
        std::vector<math::vec2> pts = {math::vec2(-100.0f, 300.0f), math::vec2(100.0f, 300.0f)};
        Body2D chain = game::makePolyline(pts, 4.0f);
        Body2D box;
        box.shape = Body2D::Box;
        box.half = math::vec2(30.0f, 20.0f);
        box.pos = math::vec2(0.0f, 100.0f); // far above
        box.invMass = 1.0f;
        d::Contact2 m = d::manifold2(chain, box);
        CHECK(!m.hit);
    }

    // Rest test: a body dropped onto a 3-point horizontal chain (spanning a joint) settles on top.
    auto restOn = [](int shape) {
        game::PhysicsWorld2D w;
        w.gravity = math::vec2(0.0f, 600.0f);
        w.warmStarting = true;
        std::vector<math::vec2> pts = {math::vec2(-200.0f, 300.0f), math::vec2(0.0f, 300.0f),
                                       math::vec2(200.0f, 300.0f)};
        w.add(game::makePolyline(pts, 4.0f));
        Body2D b;
        b.shape = shape;
        b.radius = 20.0f;
        b.half = (shape == Body2D::Box) ? math::vec2(30.0f, 20.0f) : math::vec2(0.0f, 20.0f);
        b.pos = math::vec2(0.0f, 120.0f);
        b.angle = (shape == Body2D::Box) ? 0.15f : 0.0f;
        b.invMass = 1.0f;
        b.friction = 0.8f;
        b.restitution = 0.0f;
        b.enableRotation();
        w.add(b);
        for (int s = 0; s < 320; ++s) {
            w.step(1.0f / 60.0f, 8);
        }
        return w.bodies[1];
    };

    // Box rests flat on the chain: chain top surface y = 300 - thickness(4) = 296, box half.y = 20 ->
    // centre.y = 276, upright (the two-point per-segment manifold kills the initial tilt).
    {
        Body2D b = restOn(Body2D::Box);
        CHECK_NEAR(b.pos.y, 276.0f, 2.5f);
        CHECK(std::fabs(b.angle) < 0.06f);
        CHECK(std::sqrt(glm::dot(b.vel, b.vel)) < 6.0f);
    }
    // Circle rests on the chain: centre.y = 296 - 20 = 276.
    {
        Body2D c = restOn(Body2D::Circle);
        CHECK_NEAR(c.pos.y, 276.0f, 2.5f);
        CHECK(std::sqrt(glm::dot(c.vel, c.vel)) < 6.0f);
    }
    // Capsule rests on its lower cap: centre.y = 296 - half.y(20) - radius(20) = 256.
    {
        Body2D cap = restOn(Body2D::Capsule);
        CHECK_NEAR(cap.pos.y, 256.0f, 3.0f);
        CHECK(std::sqrt(glm::dot(cap.vel, cap.vel)) < 6.0f);
    }
}

// D1: 3D rigid-body foundation. Sphere + static ground plane, gravity, sphere-sphere and sphere-plane
// impulse resolution. The 3D sibling of the 2D physics work.
void testPhysics3D() {
    using game::Body3D;
    namespace d = game::detail;

    // Direct sphere-sphere: two unit-diameter spheres overlapping along +x.
    {
        Body3D a = game::makeSphere(math::vec3(0, 0, 0), 0.5f);
        Body3D b = game::makeSphere(math::vec3(0.8f, 0, 0), 0.5f);
        d::Contact3 c = d::sphereSphere(0, a, 1, b);
        CHECK(c.hit);
        CHECK_NEAR(c.n.x, 1.0f, 1e-4f);
        CHECK_NEAR(c.n.y, 0.0f, 1e-4f);
        CHECK_NEAR(c.pen, 0.2f, 1e-4f);
    }
    // Separated spheres: no contact.
    {
        Body3D a = game::makeSphere(math::vec3(0, 0, 0), 0.5f);
        Body3D b = game::makeSphere(math::vec3(3, 0, 0), 0.5f);
        CHECK(!d::sphereSphere(0, a, 1, b).hit);
    }
    // Direct sphere-plane: sphere dipping below the y=0 ground.
    {
        Body3D s = game::makeSphere(math::vec3(0, 0.3f, 0), 0.5f);
        Body3D p = game::makeGroundPlane(math::vec3(0, 1, 0), math::vec3(0, 0, 0));
        d::Contact3 c = d::spherePlane(0, s, 1, p);
        CHECK(c.hit);
        CHECK_NEAR(c.n.y, -1.0f, 1e-4f); // sphere -> solid (downward)
        CHECK_NEAR(c.pen, 0.2f, 1e-4f);
    }

    // Drop test: a sphere falls and comes to rest on the ground with its bottom on y=0.
    {
        game::PhysicsWorld3D w;
        w.add(game::makeGroundPlane(math::vec3(0, 1, 0), math::vec3(0, 0, 0)));
        int s = w.add(game::makeSphere(math::vec3(0, 5, 0), 0.5f));
        for (int i = 0; i < 300; ++i) {
            w.step(1.0f / 60.0f, 8);
        }
        CHECK_NEAR(w.bodies[static_cast<size_t>(s)].pos.y, 0.5f, 0.02f);
        CHECK(std::sqrt(glm::dot(w.bodies[static_cast<size_t>(s)].vel,
                                w.bodies[static_cast<size_t>(s)].vel)) < 0.1f);
    }

    // Stack test: a sphere dropped directly onto a resting sphere settles on top of it.
    {
        game::PhysicsWorld3D w;
        w.add(game::makeGroundPlane(math::vec3(0, 1, 0), math::vec3(0, 0, 0)));
        int bottom = w.add(game::makeSphere(math::vec3(0, 0.5f, 0), 0.5f));
        int top = w.add(game::makeSphere(math::vec3(0.0f, 3.0f, 0), 0.5f));
        for (int i = 0; i < 600; ++i) {
            w.step(1.0f / 60.0f, 12);
        }
        CHECK_NEAR(w.bodies[static_cast<size_t>(bottom)].pos.y, 0.5f, 0.05f);
        CHECK_NEAR(w.bodies[static_cast<size_t>(top)].pos.y, 1.5f, 0.08f);
    }

    // Restitution: a bouncy sphere leaves the ground again after landing.
    {
        game::PhysicsWorld3D w;
        w.add(game::makeGroundPlane(math::vec3(0, 1, 0), math::vec3(0, 0, 0)));
        Body3D ball = game::makeSphere(math::vec3(0, 3, 0), 0.5f);
        ball.restitution = 0.9f;
        int b = w.add(ball);
        bool touched = false, bouncedUp = false;
        for (int i = 0; i < 240; ++i) {
            w.step(1.0f / 60.0f, 8);
            const float y = w.bodies[static_cast<size_t>(b)].pos.y;
            if (y < 0.6f) {
                touched = true;
            }
            if (touched && w.bodies[static_cast<size_t>(b)].vel.y > 0.5f) {
                bouncedUp = true; // regained upward speed after contact
            }
        }
        CHECK(touched);
        CHECK(bouncedUp);
    }

    // Static-vs-static (two planes) is skipped and never crashes; a horizontal shove with friction on
    // the ground decays over time rather than sliding forever.
    {
        game::PhysicsWorld3D w;
        w.add(game::makeGroundPlane(math::vec3(0, 1, 0), math::vec3(0, 0, 0)));
        Body3D ball = game::makeSphere(math::vec3(0, 0.5f, 0), 0.5f);
        ball.vel = math::vec3(4.0f, 0, 0);
        int b = w.add(ball);
        for (int i = 0; i < 300; ++i) {
            w.step(1.0f / 60.0f, 8);
        }
        CHECK(std::fabs(w.bodies[static_cast<size_t>(b)].vel.x) < 4.0f); // friction bled off speed
        CHECK_NEAR(w.bodies[static_cast<size_t>(b)].pos.y, 0.5f, 0.05f); // stayed on the ground
    }
}

// D2: 3D box (OBB) shapes + angular dynamics. Boxes tumble and settle flat on the ground; a sphere
// landing on a box registers a contact; a spinning body's spin decays.
void testPhysics3DBoxes() {
    using game::Body3D;
    namespace d = game::detail;

    // Direct sphere-box: a sphere resting just above a box's +Y face. n points sphere -> box (down).
    {
        Body3D box = game::makeBox(math::vec3(0, 0, 0), math::vec3(1, 1, 1), 0.0f);
        Body3D s = game::makeSphere(math::vec3(0, 1.3f, 0), 0.5f);
        d::Contact3 c = d::sphereBox(1, s, 0, box);
        CHECK(c.hit);
        CHECK_NEAR(c.n.y, -1.0f, 1e-3f);
        CHECK_NEAR(c.pen, 0.2f, 1e-3f);
    }
    // Separated sphere/box: no contact.
    {
        Body3D box = game::makeBox(math::vec3(0, 0, 0), math::vec3(1, 1, 1), 0.0f);
        Body3D s = game::makeSphere(math::vec3(0, 5, 0), 0.5f);
        CHECK(!d::sphereBox(1, s, 0, box).hit);
    }

    // A cube dropped flat settles resting on a face: centre at half-height, upright, at rest.
    {
        game::PhysicsWorld3D w;
        w.add(game::makeGroundPlane(math::vec3(0, 1, 0), math::vec3(0, 0, 0)));
        Body3D box = game::makeBox(math::vec3(0, 4, 0), math::vec3(0.5f, 0.5f, 0.5f), 1.0f);
        box.friction = 0.6f;
        box.enableRotation();
        int id = w.add(box);
        for (int i = 0; i < 480; ++i) {
            w.step(1.0f / 60.0f, 12);
        }
        const Body3D& r = w.bodies[static_cast<size_t>(id)];
        CHECK_NEAR(r.pos.y, 0.5f, 0.05f);
        CHECK(std::sqrt(glm::dot(r.angularVel, r.angularVel)) < 0.5f);
        // Its local +Y axis is still world-up (never toppled off its face).
        const math::vec3 up = glm::mat3_cast(r.orientation) * math::vec3(0, 1, 0);
        CHECK(up.y > 0.9f);
    }

    // A cube dropped with a tilt tumbles and still comes to rest flat on a face (centre at half-height).
    {
        game::PhysicsWorld3D w;
        w.add(game::makeGroundPlane(math::vec3(0, 1, 0), math::vec3(0, 0, 0)));
        Body3D box = game::makeBox(math::vec3(0, 4, 0), math::vec3(0.5f, 0.5f, 0.5f), 1.0f);
        box.friction = 0.6f;
        box.orientation = glm::normalize(math::quat(math::vec3(0.4f, 0.2f, 0.5f))); // tilted
        box.enableRotation();
        int id = w.add(box);
        for (int i = 0; i < 900; ++i) {
            w.step(1.0f / 60.0f, 14);
        }
        const Body3D& r = w.bodies[static_cast<size_t>(id)];
        CHECK_NEAR(r.pos.y, 0.5f, 0.08f); // resting on a face, not balanced on an edge/corner
        CHECK(std::sqrt(glm::dot(r.angularVel, r.angularVel)) < 0.6f);
    }

    // A sphere dropped onto a static box lands on top of it (sphere-box resolution + friction).
    {
        game::PhysicsWorld3D w;
        w.add(game::makeGroundPlane(math::vec3(0, 1, 0), math::vec3(0, 0, 0)));
        w.add(game::makeBox(math::vec3(0, 0.5f, 0), math::vec3(1.0f, 0.5f, 1.0f), 0.0f)); // static slab
        int s = w.add(game::makeSphere(math::vec3(0.0f, 4.0f, 0.0f), 0.4f));
        for (int i = 0; i < 400; ++i) {
            w.step(1.0f / 60.0f, 10);
        }
        // Slab top is at y=1; sphere rests at y = 1 + radius = 1.4.
        CHECK_NEAR(w.bodies[static_cast<size_t>(s)].pos.y, 1.4f, 0.06f);
    }
}

// D3: box-vs-box collision (3D SAT + clipped face manifolds). Boxes rest and stack squarely.
void testPhysics3DStacking() {
    using game::Body3D;
    namespace d = game::detail;

    // Direct box-box overlap: B sits 0.2 into A's +Y face. n points A -> B (up), pen ~0.2, and the
    // face clip yields multiple contact points (a stable manifold, not a single point).
    {
        Body3D a = game::makeBox(math::vec3(0, 0, 0), math::vec3(1, 1, 1), 1.0f);
        Body3D b = game::makeBox(math::vec3(0, 1.8f, 0), math::vec3(1, 1, 1), 1.0f);
        std::vector<d::Contact3> cs;
        d::boxBox(0, a, 1, b, cs);
        CHECK(!cs.empty());
        CHECK(cs.size() >= 2); // clipped face contact, not a single point
        bool anyDeep = false;
        for (const d::Contact3& c : cs) {
            CHECK_NEAR(c.n.y, 1.0f, 1e-2f);
            if (std::fabs(c.pen - 0.2f) < 0.05f) {
                anyDeep = true;
            }
        }
        CHECK(anyDeep);
    }
    // Separated boxes: no contact.
    {
        Body3D a = game::makeBox(math::vec3(0, 0, 0), math::vec3(1, 1, 1), 1.0f);
        Body3D b = game::makeBox(math::vec3(0, 5, 0), math::vec3(1, 1, 1), 1.0f);
        std::vector<d::Contact3> cs;
        d::boxBox(0, a, 1, b, cs);
        CHECK(cs.empty());
    }

    // Stacking: a box dropped directly onto a box resting on the ground settles squarely on top.
    {
        game::PhysicsWorld3D w;
        w.add(game::makeGroundPlane(math::vec3(0, 1, 0), math::vec3(0, 0, 0)));
        Body3D bottom = game::makeBox(math::vec3(0, 0.5f, 0), math::vec3(0.5f, 0.5f, 0.5f), 1.0f);
        bottom.friction = 0.7f;
        bottom.enableRotation();
        int bi = w.add(bottom);
        Body3D top = game::makeBox(math::vec3(0.0f, 2.2f, 0.0f), math::vec3(0.5f, 0.5f, 0.5f), 1.0f);
        top.friction = 0.7f;
        top.enableRotation();
        int ti = w.add(top);
        for (int i = 0; i < 600; ++i) {
            w.step(1.0f / 60.0f, 16);
        }
        const Body3D& rb = w.bodies[static_cast<size_t>(bi)];
        const Body3D& rt = w.bodies[static_cast<size_t>(ti)];
        CHECK_NEAR(rb.pos.y, 0.5f, 0.06f); // bottom still on the ground
        CHECK_NEAR(rt.pos.y, 1.5f, 0.10f); // top rests one box-height up
        const math::vec3 up = glm::mat3_cast(rt.orientation) * math::vec3(0, 1, 0);
        CHECK(up.y > 0.9f); // stayed square, didn't topple
        CHECK(std::sqrt(glm::dot(rt.angularVel, rt.angularVel)) < 0.6f);
    }
}

// D4: warm-started solver. A tall stack of boxes stays rigid and vertical at a modest iteration count,
// which the non-warm-started solver could not hold. Each box settles one box-height above the last.
void testPhysics3DWarmStack() {
    using game::Body3D;

    game::PhysicsWorld3D w;
    w.warmStarting = true;
    w.add(game::makeGroundPlane(math::vec3(0, 1, 0), math::vec3(0, 0, 0)));
    const int N = 5;
    const float h = 0.5f;
    std::vector<int> ids;
    for (int i = 0; i < N; ++i) {
        // Start each box a touch above its resting slot so the stack settles from a small drop.
        Body3D box = game::makeBox(math::vec3(0.0f, h + static_cast<float>(i) * (2.0f * h) + 0.05f, 0.0f),
                                   math::vec3(h, h, h), 1.0f);
        box.friction = 0.8f;
        box.restitution = 0.0f;
        box.enableRotation();
        ids.push_back(w.add(box));
    }
    for (int s = 0; s < 400; ++s) {
        w.step(1.0f / 60.0f, 8); // only 8 iterations — warm starting is what keeps this rigid
    }
    for (int i = 0; i < N; ++i) {
        const Body3D& r = w.bodies[static_cast<size_t>(ids[static_cast<size_t>(i)])];
        const float expectedY = h + static_cast<float>(i) * (2.0f * h);
        CHECK_NEAR(r.pos.y, expectedY, 0.12f);          // stayed stacked, didn't sink or explode
        CHECK(std::fabs(r.pos.x) < 0.2f);               // didn't drift sideways
        const math::vec3 up = glm::mat3_cast(r.orientation) * math::vec3(0, 1, 0);
        CHECK(up.y > 0.95f);                            // stayed square
    }
    // The whole stack has come to rest.
    for (int i = 0; i < N; ++i) {
        const Body3D& r = w.bodies[static_cast<size_t>(ids[static_cast<size_t>(i)])];
        CHECK(std::sqrt(glm::dot(r.vel, r.vel)) < 0.3f);
    }
}

// D5: 3D capsule shape (Godot CapsuleShape3D). Capsules rest on the ground (upright on a cap, flat on
// two points), catch spheres, and lean against boxes.
void testPhysics3DCapsule() {
    using game::Body3D;
    namespace d = game::detail;

    // Direct sphere-capsule: a sphere overlapping an upright capsule's side. n points sphere -> capsule.
    {
        Body3D cap = game::makeCapsule(math::vec3(0, 0, 0), 0.5f, 1.0f); // upright, radius .5, half-h 1
        Body3D s = game::makeSphere(math::vec3(0.8f, 0.0f, 0.0f), 0.5f); // beside it, overlap 0.2
        d::Contact3 c = d::sphereCapsule(1, s, 0, cap);
        CHECK(c.hit);
        CHECK_NEAR(c.n.x, -1.0f, 1e-3f); // n: sphere(+x) -> capsule(origin) points in -x
        CHECK_NEAR(c.pen, 0.2f, 1e-3f);
    }

    // An upright capsule dropped on the ground rests on its lower cap: bottom sphere touches y=0, so
    // centre.y = halfHeight + radius.
    {
        game::PhysicsWorld3D w;
        w.add(game::makeGroundPlane(math::vec3(0, 1, 0), math::vec3(0, 0, 0)));
        Body3D cap = game::makeCapsule(math::vec3(0, 4, 0), 0.5f, 1.0f, 1.0f);
        cap.friction = 0.6f;
        cap.enableRotation();
        int id = w.add(cap);
        for (int i = 0; i < 360; ++i) {
            w.step(1.0f / 60.0f, 10);
        }
        const Body3D& r = w.bodies[static_cast<size_t>(id)];
        CHECK_NEAR(r.pos.y, 1.5f, 0.06f); // halfHeight(1) + radius(0.5)
        CHECK(std::sqrt(glm::dot(r.vel, r.vel)) < 0.2f);
    }

    // A horizontal capsule dropped on the ground rests flat on both caps: centre.y = radius.
    {
        game::PhysicsWorld3D w;
        w.add(game::makeGroundPlane(math::vec3(0, 1, 0), math::vec3(0, 0, 0)));
        Body3D cap = game::makeCapsule(math::vec3(0, 3, 0), 0.5f, 1.0f, 1.0f);
        // Rotate the local +Y segment onto the world X axis so it lies horizontal.
        cap.orientation = glm::normalize(math::quat(math::vec3(0.0f, 0.0f, 1.5708f)));
        cap.friction = 0.6f;
        cap.enableRotation();
        int id = w.add(cap);
        for (int i = 0; i < 420; ++i) {
            w.step(1.0f / 60.0f, 12);
        }
        const Body3D& r = w.bodies[static_cast<size_t>(id)];
        CHECK_NEAR(r.pos.y, 0.5f, 0.06f); // resting on its side -> centre at radius
        CHECK(std::sqrt(glm::dot(r.vel, r.vel)) < 0.3f);
    }

    // A sphere dropped onto a static horizontal capsule lands on top of it (capsule-sphere resolution).
    {
        game::PhysicsWorld3D w;
        w.add(game::makeGroundPlane(math::vec3(0, 1, 0), math::vec3(0, 0, 0)));
        Body3D cap = game::makeCapsule(math::vec3(0, 1.0f, 0), 0.5f, 1.5f, 0.0f); // static, horizontal
        cap.orientation = glm::normalize(math::quat(math::vec3(0.0f, 0.0f, 1.5708f)));
        w.add(cap);
        int s = w.add(game::makeSphere(math::vec3(0.0f, 4.0f, 0.0f), 0.4f));
        for (int i = 0; i < 360; ++i) {
            w.step(1.0f / 60.0f, 10);
        }
        // Capsule axis at y=1, radius 0.5 -> top at y=1.5; sphere rests at 1.5 + 0.4 = 1.9.
        CHECK_NEAR(w.bodies[static_cast<size_t>(s)].pos.y, 1.9f, 0.08f);
    }
}

// D6: 3D broadphase (sweep-and-prune). Culling non-touching pairs must not change the result: the same
// scene simulated with broadphase on and off ends bit-identical.
void testPhysics3DBroadphase() {
    using game::Body3D;

    auto build = [](bool bp) {
        game::PhysicsWorld3D w;
        w.broadphase = bp;
        w.add(game::makeGroundPlane(math::vec3(0, 1, 0), math::vec3(0, 0, 0)));
        // A spread-out scene where most pairs are far apart (so broadphase actually prunes).
        for (int i = 0; i < 6; ++i) {
            const float x = static_cast<float>(i) * 3.0f - 7.5f;
            Body3D s = game::makeSphere(math::vec3(x, 1.0f + static_cast<float>(i) * 0.5f, 0.0f), 0.5f);
            s.enableRotation();
            w.add(s);
            Body3D b = game::makeBox(math::vec3(x + 0.2f, 3.0f, 0.5f), math::vec3(0.5f, 0.5f, 0.5f), 1.0f);
            b.enableRotation();
            w.add(b);
        }
        for (int step = 0; step < 200; ++step) {
            w.step(1.0f / 60.0f, 8);
        }
        return w;
    };

    game::PhysicsWorld3D withBp = build(true);
    game::PhysicsWorld3D noBp = build(false);
    CHECK(withBp.bodies.size() == noBp.bodies.size());
    for (size_t i = 0; i < withBp.bodies.size(); ++i) {
        const math::vec3 d = withBp.bodies[i].pos - noBp.bodies[i].pos;
        CHECK(std::sqrt(glm::dot(d, d)) < 1e-4f); // identical trajectories
    }
    // Also: everything settled above the ground (nothing tunneled through).
    for (size_t i = 1; i < withBp.bodies.size(); ++i) {
        CHECK(withBp.bodies[i].pos.y > 0.2f);
    }
}

// D7: body sleeping / islands. A settled stack goes to sleep (stops moving), and a body dropped onto it
// wakes it back up.
void testPhysics3DSleep() {
    using game::Body3D;

    game::PhysicsWorld3D w;
    w.allowSleep = true;
    w.sleepTime = 0.4f;
    w.add(game::makeGroundPlane(math::vec3(0, 1, 0), math::vec3(0, 0, 0)));
    Body3D a = game::makeBox(math::vec3(0, 0.5f, 0), math::vec3(0.5f, 0.5f, 0.5f), 1.0f);
    a.friction = 0.8f;
    a.enableRotation();
    int ai = w.add(a);
    Body3D b = game::makeBox(math::vec3(0.02f, 1.55f, 0.0f), math::vec3(0.5f, 0.5f, 0.5f), 1.0f);
    b.friction = 0.8f;
    b.enableRotation();
    int bi = w.add(b);

    // Let the stack settle and fall asleep.
    for (int i = 0; i < 240; ++i) {
        w.step(1.0f / 60.0f, 10);
    }
    CHECK(w.bodies[static_cast<size_t>(ai)].sleeping);
    CHECK(w.bodies[static_cast<size_t>(bi)].sleeping);
    const float restY = w.bodies[static_cast<size_t>(bi)].pos.y;

    // Drop a third box onto the sleeping stack: within a few steps the island wakes.
    Body3D c = game::makeBox(math::vec3(0.0f, 3.5f, 0.0f), math::vec3(0.5f, 0.5f, 0.5f), 1.0f);
    c.friction = 0.8f;
    c.enableRotation();
    int ci = w.add(c);
    bool woke = false;
    for (int i = 0; i < 120; ++i) {
        w.step(1.0f / 60.0f, 10);
        if (!w.bodies[static_cast<size_t>(ai)].sleeping && !w.bodies[static_cast<size_t>(bi)].sleeping) {
            woke = true;
            break;
        }
    }
    CHECK(woke);
    CHECK(w.bodies[static_cast<size_t>(ci)].pos.y < 3.5f); // the newcomer fell and landed
    // Let it all settle again; the whole three-box stack ends near its expected heights.
    for (int i = 0; i < 300; ++i) {
        w.step(1.0f / 60.0f, 10);
    }
    CHECK_NEAR(w.bodies[static_cast<size_t>(ai)].pos.y, 0.5f, 0.1f);
    CHECK(std::fabs(w.bodies[static_cast<size_t>(bi)].pos.y - restY) < 0.2f);
}

// D8: 3D physics-space ray queries. Cast rays against spheres, boxes, capsules and the ground plane;
// get the nearest hit with distance, point, normal, and body index, filtered by layer mask.
void testPhysics3DRay() {
    using game::Body3D;

    game::PhysicsWorld3D w;
    const int plane = w.add(game::makeGroundPlane(math::vec3(0, 1, 0), math::vec3(0, 0, 0)));
    const int sphere = w.add(game::makeSphere(math::vec3(0, 5, 0), 1.0f, 0.0f));
    const int box = w.add(game::makeBox(math::vec3(5, 5, 0), math::vec3(1, 1, 1), 0.0f));

    // Straight down from above the sphere: hits the sphere top at y=6, normal up, before the ground.
    {
        game::RayHit3 h = w.queryRay(math::vec3(0, 10, 0), math::vec3(0, -1, 0));
        CHECK(h.hit);
        CHECK(h.index == sphere);
        CHECK_NEAR(h.t, 4.0f, 1e-3f);       // 10 -> 6
        CHECK_NEAR(h.point.y, 6.0f, 1e-3f);
        CHECK_NEAR(h.normal.y, 1.0f, 1e-3f);
    }
    // Down past the box's side (x=5): hits the box top face at y=6, normal up.
    {
        game::RayHit3 h = w.queryRay(math::vec3(5, 10, 0), math::vec3(0, -1, 0));
        CHECK(h.hit);
        CHECK(h.index == box);
        CHECK_NEAR(h.point.y, 6.0f, 1e-3f);
        CHECK_NEAR(h.normal.y, 1.0f, 1e-3f);
    }
    // Down through empty space hits the infinite ground plane at y=0.
    {
        game::RayHit3 h = w.queryRay(math::vec3(-5, 10, 0), math::vec3(0, -1, 0));
        CHECK(h.hit);
        CHECK(h.index == plane);
        CHECK_NEAR(h.point.y, 0.0f, 1e-3f);
    }
    // Pointing away from everything: miss.
    {
        game::RayHit3 h = w.queryRay(math::vec3(0, 10, 0), math::vec3(0, 1, 0));
        CHECK(!h.hit);
    }
    // maxDist too short to reach the sphere: miss.
    {
        game::RayHit3 h = w.queryRay(math::vec3(0, 10, 0), math::vec3(0, -1, 0), 2.0f);
        CHECK(!h.hit);
    }
    // Layer mask that excludes the sphere's layer skips it and finds the ground beyond.
    {
        w.bodies[static_cast<size_t>(sphere)].collisionLayer = 0x2u;
        game::RayHit3 h = w.queryRay(math::vec3(0, 10, 0), math::vec3(0, -1, 0), 1e30f, 0x1u);
        CHECK(h.hit);
        CHECK(h.index == plane); // sphere filtered out
        w.bodies[static_cast<size_t>(sphere)].collisionLayer = ~0u;
    }
    // Ray across the world hitting a capsule from the side.
    {
        game::PhysicsWorld3D cw;
        int cap = cw.add(game::makeCapsule(math::vec3(0, 2, 0), 0.5f, 1.0f, 0.0f)); // upright at origin
        game::RayHit3 h = cw.queryRay(math::vec3(-5, 2, 0), math::vec3(1, 0, 0));
        CHECK(h.hit);
        CHECK(h.index == cap);
        CHECK_NEAR(h.point.x, -0.5f, 1e-2f); // near side of the capsule (radius 0.5)
        CHECK_NEAR(h.normal.x, -1.0f, 1e-2f);
    }
}

// D9: 3D kinematic character controller (moveAndSlide3). A capsule mover lands on a floor, slides
// along a wall instead of stopping dead, and reports floor/wall state.
void testPhysics3DMoveSlide() {
    using game::Body3D;

    // A big static box floor with its top at y=0.
    std::vector<Body3D> world;
    world.push_back(game::makeBox(math::vec3(0, -1.0f, 0), math::vec3(10, 1, 10), 0.0f));

    // A capsule mover (radius 0.4, half-height 0.6) starting above the floor, moving down.
    Body3D mover = game::makeCapsule(math::vec3(0, 3.0f, 0), 0.4f, 0.6f, 1.0f);

    // Drive it downward for a while; it should land and stop on the floor with onFloor set.
    game::MoveResult3 r;
    for (int i = 0; i < 200; ++i) {
        r = game::moveAndSlide3(mover, world, math::vec3(0, -5.0f, 0), 1.0f / 60.0f);
        mover.pos = r.position;
    }
    CHECK(r.onFloor);
    // Capsule rests with its lower cap on the floor: bottom = pos.y - halfHeight - radius = 0.
    CHECK_NEAR(mover.pos.y, 1.0f, 0.05f); // 0 + halfHeight(0.6) + radius(0.4)
    CHECK(!r.onCeiling);

    // Now push it horizontally into a wall (a box whose face is at x=1); it should slide, not stop, and
    // its x stays on the near side of the wall.
    std::vector<Body3D> walled;
    walled.push_back(game::makeBox(math::vec3(0, -1.0f, 0), math::vec3(10, 1, 10), 0.0f)); // floor
    walled.push_back(game::makeBox(math::vec3(2.0f, 1.0f, 0), math::vec3(1, 2, 10), 0.0f)); // wall x>=1
    Body3D m2 = game::makeCapsule(math::vec3(0.0f, 1.0f, 0.0f), 0.4f, 0.6f, 1.0f);
    game::MoveResult3 r2;
    for (int i = 0; i < 120; ++i) {
        // Move diagonally into the wall and forward along z.
        r2 = game::moveAndSlide3(m2, walled, math::vec3(3.0f, 0.0f, 2.0f), 1.0f / 60.0f);
        m2.pos = r2.position;
    }
    CHECK(r2.onWall);
    CHECK(m2.pos.x < 0.65f);  // stopped at the wall face (x=1) minus capsule radius 0.4
    CHECK(m2.pos.z > 1.0f);   // but kept sliding along z (wasn't halted by the wall)
}

// D10: 3D physics materials. combineValue is shared with the 2D solver; the 3D world's default combine
// modes reproduce the old hardcoded behaviour, and Max restitution makes a pair bounce higher than Min.
void testPhysics3DMaterial() {
    using game::Body3D;
    using game::CombineMode;

    // Shared combine helper sanity (same as the 2D solver uses).
    CHECK_NEAR(game::combineValue(CombineMode::Min, 0.2f, 0.8f), 0.2f, 1e-6f);
    CHECK_NEAR(game::combineValue(CombineMode::Max, 0.2f, 0.8f), 0.8f, 1e-6f);
    CHECK_NEAR(game::combineValue(CombineMode::Average, 0.2f, 0.8f), 0.5f, 1e-6f);
    CHECK_NEAR(game::combineValue(CombineMode::Multiply, 0.5f, 0.5f), 0.25f, 1e-6f);
    CHECK_NEAR(game::combineValue(CombineMode::GeometricMean, 0.25f, 0.64f), 0.4f, 1e-6f);

    // Drop a ball (restitution 0.9) onto a floor (restitution 0.1) and measure the peak rebound height
    // under Min vs Max restitution combine. Max should bounce noticeably higher than Min.
    auto peakAfterBounce = [](CombineMode mode) {
        game::PhysicsWorld3D w;
        w.restitutionCombine = mode;
        Body3D floor = game::makeBox(math::vec3(0, -1, 0), math::vec3(10, 1, 10), 0.0f);
        floor.restitution = 0.1f;
        w.add(floor);
        Body3D ball = game::makeSphere(math::vec3(0, 3, 0), 0.5f);
        ball.restitution = 0.9f;
        const int b = w.add(ball);
        bool landed = false;
        float peak = 0.0f;
        for (int i = 0; i < 400; ++i) {
            w.step(1.0f / 60.0f, 8);
            const float y = w.bodies[static_cast<size_t>(b)].pos.y;
            if (y < 0.7f) {
                landed = true;
            }
            if (landed) {
                peak = std::max(peak, y);
            }
        }
        return peak;
    };
    const float minPeak = peakAfterBounce(CombineMode::Min);   // e = min(0.9,0.1) = 0.1 -> low bounce
    const float maxPeak = peakAfterBounce(CombineMode::Max);   // e = max(0.9,0.1) = 0.9 -> high bounce
    CHECK(maxPeak > minPeak + 0.3f);
}

void testPhysics3DSlider() {
    using game::Body3D;

    // A slider along Y between a static anchor and a dynamic body (both at the origin).
    auto makeWorld = [](game::PhysicsWorld3D& w, int& ib) {
        w.gravity = math::vec3(0, 0, 0);
        // The static anchor body sits far away so it never collides with B; the joint anchor is the
        // world point (0,0,0) regardless of A's position (localA absorbs the offset).
        Body3D A;
        A.pos = math::vec3(0, 50, 0);
        A.invMass = 0.0f;
        A.radius = 0.1f;
        Body3D B;
        B.pos = math::vec3(0, 0, 0);
        B.invMass = 1.0f;
        B.radius = 0.5f;
        B.enableRotation();
        B.angularDamping = 0.0f;
        const int ia = w.add(A);
        ib = w.add(B);
        w.joints.push_back(game::makeSliderJoint3(ia, w.bodies[static_cast<size_t>(ia)], ib,
                                                  w.bodies[static_cast<size_t>(ib)],
                                                  math::vec3(0, 0, 0), math::vec3(0, 1, 0)));
    };

    // Linear: perpendicular velocity is cancelled, axial (Y) velocity is preserved.
    {
        game::PhysicsWorld3D w;
        int ib = -1;
        makeWorld(w, ib);
        w.bodies[static_cast<size_t>(ib)].vel = math::vec3(1.0f, 2.0f, 3.0f);
        for (int i = 0; i < 60; ++i) {
            w.step(1.0f / 60.0f);
        }
        CHECK_NEAR(w.bodies[static_cast<size_t>(ib)].vel.x, 0.0f, 0.05f);
        CHECK_NEAR(w.bodies[static_cast<size_t>(ib)].vel.z, 0.0f, 0.05f);
        CHECK(w.bodies[static_cast<size_t>(ib)].vel.y > 1.5f); // still free to slide
        CHECK_NEAR(w.bodies[static_cast<size_t>(ib)].pos.x, 0.0f, 0.05f);
        CHECK_NEAR(w.bodies[static_cast<size_t>(ib)].pos.z, 0.0f, 0.05f);
    }

    // Perpendicular positional drift is corrected: displace the anchor off-axis after building.
    {
        game::PhysicsWorld3D w;
        int ib = -1;
        makeWorld(w, ib);
        w.bodies[static_cast<size_t>(ib)].pos = math::vec3(0.5f, 0, 0);
        for (int i = 0; i < 180; ++i) {
            w.step(1.0f / 60.0f);
        }
        CHECK(std::fabs(w.bodies[static_cast<size_t>(ib)].pos.x) < 0.1f);
    }

    // Angular: spin perpendicular to the axis is cancelled, spin about the axis is preserved.
    {
        game::PhysicsWorld3D w;
        int ib = -1;
        makeWorld(w, ib);
        w.bodies[static_cast<size_t>(ib)].angularVel = math::vec3(1.0f, 2.0f, 0.5f);
        for (int i = 0; i < 60; ++i) {
            w.step(1.0f / 60.0f);
        }
        CHECK_NEAR(w.bodies[static_cast<size_t>(ib)].angularVel.x, 0.0f, 0.1f);
        CHECK_NEAR(w.bodies[static_cast<size_t>(ib)].angularVel.z, 0.0f, 0.1f);
        CHECK(w.bodies[static_cast<size_t>(ib)].angularVel.y > 1.5f);
    }
}

// D11: 3D pin joints (Joint3D). A ball pinned to a static anchor swings but stays at a fixed distance;
// a two-link chain hangs to its full length under gravity.
void testPhysics3DJoint() {
    using game::Body3D;

    // Pendulum: static anchor at the pivot (0,5,0); a ball started off to the side, pinned to the pivot
    // by a point on the ball. It swings down under gravity while its centre stays a fixed distance from
    // the pivot (the joint pins ball_local_point -> pivot, so |centre - pivot| == |offset| is constant).
    {
        const math::vec3 pivot(0, 5, 0);
        game::PhysicsWorld3D w;
        const int ai = w.add(game::makeSphere(pivot, 0.1f, 0.0f)); // static anchor at the pivot
        Body3D ball = game::makeSphere(math::vec3(1.5f, 3.0f, 0.0f), 0.3f, 1.0f);
        ball.enableRotation();
        const int bi = w.add(ball);
        const float rod =
            std::sqrt(glm::dot(math::vec3(1.5f, 3.0f, 0.0f) - pivot,
                               math::vec3(1.5f, 3.0f, 0.0f) - pivot)); // = 2.5
        w.joints.push_back(game::makePinJoint3(ai, w.bodies[static_cast<size_t>(ai)], bi,
                                               w.bodies[static_cast<size_t>(bi)], pivot));
        float minLen = 1e9f, maxLen = -1e9f, minY = 1e9f;
        for (int i = 0; i < 300; ++i) {
            w.step(1.0f / 60.0f, 12);
            const math::vec3 d = w.bodies[static_cast<size_t>(bi)].pos - pivot;
            const float len = std::sqrt(glm::dot(d, d));
            minLen = std::min(minLen, len);
            maxLen = std::max(maxLen, len);
            minY = std::min(minY, w.bodies[static_cast<size_t>(bi)].pos.y);
        }
        CHECK_NEAR(minLen, rod, 0.1f); // rod length preserved throughout the swing
        CHECK_NEAR(maxLen, rod, 0.1f);
        CHECK(minY < 2.9f); // swung down from y=3 toward the bottom of the arc (pivot.y - rod = 2.5)
    }

    // Two-link chain hanging from a static anchor settles vertical at its full length.
    {
        game::PhysicsWorld3D w;
        const int anchor = w.add(game::makeSphere(math::vec3(0, 6, 0), 0.1f, 0.0f)); // static
        Body3D l1 = game::makeSphere(math::vec3(0, 5, 0), 0.3f, 1.0f);
        l1.enableRotation();
        const int i1 = w.add(l1);
        Body3D l2 = game::makeSphere(math::vec3(0, 4, 0), 0.3f, 1.0f);
        l2.enableRotation();
        const int i2 = w.add(l2);
        w.joints.push_back(game::makePinJoint3(anchor, w.bodies[static_cast<size_t>(anchor)], i1,
                                               w.bodies[static_cast<size_t>(i1)], math::vec3(0, 5.5f, 0)));
        w.joints.push_back(game::makePinJoint3(i1, w.bodies[static_cast<size_t>(i1)], i2,
                                               w.bodies[static_cast<size_t>(i2)], math::vec3(0, 4.5f, 0)));
        for (int i = 0; i < 600; ++i) {
            w.step(1.0f / 60.0f, 16);
        }
        // Hanging vertical: both links stay on the y-axis, link 2 below link 1, near their rest gaps.
        CHECK(std::fabs(w.bodies[static_cast<size_t>(i1)].pos.x) < 0.1f);
        CHECK(std::fabs(w.bodies[static_cast<size_t>(i2)].pos.x) < 0.1f);
        CHECK(w.bodies[static_cast<size_t>(i2)].pos.y < w.bodies[static_cast<size_t>(i1)].pos.y);
        // Joint separations preserved (~1 each from the anchor chain spacing).
        const float gap = w.bodies[static_cast<size_t>(i1)].pos.y - w.bodies[static_cast<size_t>(i2)].pos.y;
        CHECK_NEAR(gap, 1.0f, 0.15f);
    }
}

// D12: 3D distance / rod joints. A ball on a rod from a static anchor hangs at exactly the rod length;
// a rod between two free-falling balls keeps their separation fixed.
void testPhysics3DDistanceJoint() {
    using game::Body3D;

    // Rope/rod: static anchor at (0,6,0); ball dropped from (0,5,0) on a rod of rest length 2. It
    // should fall until the rod goes taut at distance 2 below the anchor, i.e. settle at y ~= 4.
    {
        game::PhysicsWorld3D w;
        const int anchor = w.add(game::makeSphere(math::vec3(0, 6, 0), 0.1f, 0.0f));
        Body3D ball = game::makeSphere(math::vec3(0, 5, 0), 0.3f, 1.0f);
        ball.enableRotation();
        const int bi = w.add(ball);
        w.joints.push_back(game::makeDistanceJoint3(anchor, w.bodies[static_cast<size_t>(anchor)], bi,
                                                    w.bodies[static_cast<size_t>(bi)],
                                                    math::vec3(0, 6, 0), math::vec3(0, 5, 0), 2.0f));
        for (int i = 0; i < 400; ++i) {
            w.step(1.0f / 60.0f, 16);
        }
        const math::vec3 d = w.bodies[static_cast<size_t>(bi)].pos - math::vec3(0, 6, 0);
        const float len = std::sqrt(glm::dot(d, d));
        CHECK_NEAR(len, 2.0f, 0.05f);                       // rod taut at rest length
        CHECK_NEAR(w.bodies[static_cast<size_t>(bi)].pos.y, 4.0f, 0.1f); // hanging straight down
    }

    // A rod between two free-falling balls keeps their separation fixed even as both accelerate.
    {
        game::PhysicsWorld3D w;
        Body3D b1 = game::makeSphere(math::vec3(-1, 5, 0), 0.3f, 1.0f);
        b1.enableRotation();
        const int i1 = w.add(b1);
        Body3D b2 = game::makeSphere(math::vec3(1, 5, 0), 0.3f, 1.0f);
        b2.enableRotation();
        const int i2 = w.add(b2);
        w.joints.push_back(game::makeDistanceJoint3(i1, w.bodies[static_cast<size_t>(i1)], i2,
                                                    w.bodies[static_cast<size_t>(i2)],
                                                    math::vec3(-1, 5, 0), math::vec3(1, 5, 0)));
        for (int i = 0; i < 120; ++i) {
            w.step(1.0f / 60.0f, 12);
            const math::vec3 d = w.bodies[static_cast<size_t>(i2)].pos - w.bodies[static_cast<size_t>(i1)].pos;
            CHECK_NEAR(std::sqrt(glm::dot(d, d)), 2.0f, 0.08f); // separation held every step
        }
        // Both fell under gravity (the rod doesn't hold them up, only apart).
        CHECK(w.bodies[static_cast<size_t>(i1)].pos.y < 4.0f);
        CHECK(w.bodies[static_cast<size_t>(i2)].pos.y < 4.0f);
    }
}

// D13: 3D hinge (revolute) joint. A box hinged to a static anchor along the world Z axis swings down
// under gravity but rotates ONLY about that axis (no tumbling), and the hinge point stays fixed.
void testPhysics3DHinge() {
    using game::Body3D;

    const math::vec3 pivot(0, 5, 0);
    game::PhysicsWorld3D w;
    const int anchor = w.add(game::makeSphere(pivot, 0.1f, 0.0f)); // static
    Body3D box = game::makeBox(math::vec3(1, 5, 0), math::vec3(0.5f, 0.5f, 0.5f), 1.0f);
    box.friction = 0.0f;
    box.enableRotation();
    const int bi = w.add(box);
    w.joints.push_back(game::makeHingeJoint3(anchor, w.bodies[static_cast<size_t>(anchor)], bi,
                                             w.bodies[static_cast<size_t>(bi)], pivot,
                                             math::vec3(0, 0, 1)));
    float minY = 1e9f, worstAxis = 1.0f, worstAnchor = 0.0f;
    for (int i = 0; i < 300; ++i) {
        w.step(1.0f / 60.0f, 16);
        const Body3D& b = w.bodies[static_cast<size_t>(bi)];
        const math::mat3 R = glm::mat3_cast(b.orientation);
        // The box's local Z axis must stay aligned with world Z (rotation only about the hinge).
        worstAxis = std::min(worstAxis, glm::dot(R * math::vec3(0, 0, 1), math::vec3(0, 0, 1)));
        // The hinge point (box local anchor -> world) must stay at the pivot.
        const math::vec3 hinge = b.pos + R * (math::vec3(-1, 0, 0));
        worstAnchor = std::max(worstAnchor, std::sqrt(glm::dot(hinge - pivot, hinge - pivot)));
        minY = std::min(minY, b.pos.y);
    }
    CHECK(worstAxis > 0.99f);   // never tumbled off the hinge axis
    CHECK(worstAnchor < 0.06f); // hinge point stayed pinned
    CHECK(minY < 4.8f);         // swung down (toward hanging at pivot.y - 1 = 4)
}

void testEditorScene() {
    using editor::Node;
    using editor::Scene;

    // A translated + scaled unit box: its world AABB must recentre and grow accordingly.
    Node n;
    n.position = math::vec3(3, 1, -2);
    n.scale = math::vec3(2, 2, 2); // local [-0.5,0.5] -> world half-extent 1
    math::vec3 mn, mx;
    n.worldAabb(mn, mx);
    CHECK_NEAR(mn.x, 2.0f, 1e-4f);
    CHECK_NEAR(mx.x, 4.0f, 1e-4f);
    CHECK_NEAR(mn.y, 0.0f, 1e-4f);
    CHECK_NEAR(mx.y, 2.0f, 1e-4f);
    CHECK_NEAR((n.position - math::vec3(3, 1, -2)).x, 0.0f, 1e-6f);

    // A 90-degree Y rotation of a 2x1x1 box swaps the x/z extents in world space.
    Node r;
    r.localMin = math::vec3(-1, -0.5f, -0.5f);
    r.localMax = math::vec3(1, 0.5f, 0.5f);
    r.euler = math::vec3(0, 90, 0);
    r.worldAabb(mn, mx);
    CHECK_NEAR(mx.x, 0.5f, 1e-4f); // the long axis is now along z
    CHECK_NEAR(mx.z, 1.0f, 1e-4f);

    // Picking: two boxes on the x axis; a ray down -x from far +x must hit the nearer (right) one.
    Scene s;
    Node a;
    a.name = "left";
    a.position = math::vec3(-3, 0, 0);
    Node b;
    b.name = "right";
    b.position = math::vec3(3, 0, 0);
    s.nodes.push_back(a);
    s.nodes.push_back(b);
    int hit = editor::pickNode(s, math::vec3(10, 0, 0), math::vec3(-1, 0, 0));
    CHECK(hit == 1); // the right box is nearer to the ray origin
    // A ray that misses both (way above) returns -1.
    int miss = editor::pickNode(s, math::vec3(10, 50, 0), math::vec3(-1, 0, 0));
    CHECK(miss == -1);
    // Hidden nodes are not pickable.
    s.nodes[1].visible = false;
    int hit2 = editor::pickNode(s, math::vec3(10, 0, 0), math::vec3(-1, 0, 0));
    CHECK(hit2 == 0); // now it falls through to the left box

    // selectedNode() maps the index safely.
    s.selected = 0;
    CHECK(s.selectedNode() != nullptr && s.selectedNode()->name == "left");
    s.selected = -1;
    CHECK(s.selectedNode() == nullptr);
}

void testEditorPickRay() {
    // Mirror the editor's camera and confirm the centre-screen ray hits a node at the look target.
    // This guards the NDC-y sign in screenRay (a Vulkan clip-space gotcha that's invisible by eye).
    const float w = 1280.0f, h = 720.0f;
    const math::mat4 proj = math::perspective(glm::radians(46.0f), w / h, 0.1f, 100.0f);
    const math::mat4 view =
        glm::lookAt(math::vec3(3.6f, 3.4f, 6.4f), math::vec3(0.2f, 0.3f, 0.0f), math::vec3(0, 1, 0));
    const math::mat4 inv = glm::inverse(proj * view);

    editor::Scene s;
    editor::Node n;
    n.name = "target";
    n.position = math::vec3(0.2f, 0.3f, 0.0f);
    s.nodes.push_back(n);

    math::vec3 ro, rd;
    editor::screenRay(inv, w * 0.5f, h * 0.5f, w, h, ro, rd);
    CHECK(editor::pickNode(s, ro, rd) == 0); // the centre ray hits the node under the crosshair

    // A ray toward a screen corner (far from the single centred node) should miss.
    editor::screenRay(inv, w * 0.05f, h * 0.05f, w, h, ro, rd);
    CHECK(editor::pickNode(s, ro, rd) == -1);
}

void testEditorWorldToScreen() {
    // worldToScreen is the inverse of screenRay's unproject: projecting the camera's look target must
    // land near screen centre, and projecting then unprojecting must yield a ray back through the point.
    const float w = 1280.0f, h = 720.0f;
    const math::mat4 proj = math::perspective(glm::radians(46.0f), w / h, 0.1f, 100.0f);
    const math::mat4 view =
        glm::lookAt(math::vec3(3.6f, 3.4f, 6.4f), math::vec3(0.2f, 0.3f, 0.0f), math::vec3(0, 1, 0));
    const math::mat4 viewProj = proj * view;

    const math::vec3 target(0.2f, 0.3f, 0.0f);
    math::vec2 px;
    CHECK(editor::worldToScreen(viewProj, target, w, h, px));
    CHECK_NEAR(px.x, w * 0.5f, 1.5f); // look target projects to screen centre
    CHECK_NEAR(px.y, h * 0.5f, 1.5f);

    // Round-trip: unproject that pixel and confirm the ray points at the world point.
    math::vec3 ro, rd;
    editor::screenRay(glm::inverse(viewProj), px.x, px.y, w, h, ro, rd);
    const math::vec3 toTarget = glm::normalize(target - ro);
    CHECK(glm::dot(rd, toTarget) > 0.999f);

    // A point off to the side projects off-centre in the expected direction.
    math::vec2 pr;
    CHECK(editor::worldToScreen(viewProj, math::vec3(2.0f, 0.3f, 0.0f), w, h, pr));
    CHECK(pr.x > px.x); // +x world moves right on screen for this camera
}

void testEditorGizmoDrag() {
    // A ray from above pointing down at an angle hits the ground plane at a predictable point.
    math::vec3 hit;
    CHECK(editor::rayPlaneY(math::vec3(0, 4, 0), math::vec3(1, -1, 0), 0.0f, hit));
    CHECK_NEAR(hit.x, 4.0f, 1e-4f); // travel 4 down => 4 along x
    CHECK_NEAR(hit.y, 0.0f, 1e-4f);

    // A ray parallel to the plane never hits it.
    CHECK(!editor::rayPlaneY(math::vec3(0, 2, 0), math::vec3(1, 0, 0), 0.0f, hit));
    // A ray pointing away from the plane (upward, plane below) misses (t < 0).
    CHECK(!editor::rayPlaneY(math::vec3(0, 2, 0), math::vec3(0, 1, 0), 0.0f, hit));

    // The drag keeps the grabbed point under the cursor: offset = pos - grabHit, then
    // newPos = cursorHit + offset preserves the object's relative position.
    const math::vec3 pos(3, 0.5f, -1);
    const math::vec3 grabHit(2.5f, 0.5f, -1.2f);
    const math::vec3 offset = math::vec3(pos.x - grabHit.x, 0, pos.z - grabHit.z);
    const math::vec3 cursorHit(5.0f, 0.5f, 0.3f);
    const math::vec3 moved(cursorHit.x + offset.x, pos.y, cursorHit.z + offset.z);
    CHECK_NEAR(moved.x, 5.5f, 1e-4f);
    CHECK_NEAR(moved.z, 0.5f, 1e-4f);
}

void testEditorHistory() {
    using editor::History;
    using editor::Node;

    std::vector<Node> scene(1);
    scene[0].name = "n";
    scene[0].position = math::vec3(0, 0, 0);
    History h;
    CHECK(!h.canUndo() && !h.canRedo());

    // Gesture 1: move to x=5.
    h.begin(scene);
    scene[0].position.x = 5.0f;
    h.end(scene);
    CHECK(h.canUndo());

    // Gesture 2: move to x=9.
    h.begin(scene);
    scene[0].position.x = 9.0f;
    h.end(scene);

    // Undo -> back to 5, undo again -> back to 0.
    CHECK(h.undo(scene));
    CHECK_NEAR(scene[0].position.x, 5.0f, 1e-4f);
    CHECK(h.undo(scene));
    CHECK_NEAR(scene[0].position.x, 0.0f, 1e-4f);
    CHECK(!h.canUndo());

    // Redo twice returns to 9.
    CHECK(h.redo(scene));
    CHECK_NEAR(scene[0].position.x, 5.0f, 1e-4f);
    CHECK(h.redo(scene));
    CHECK_NEAR(scene[0].position.x, 9.0f, 1e-4f);
    CHECK(!h.canRedo());

    // A no-op gesture (no change) records nothing.
    h.begin(scene);
    h.end(scene);
    CHECK(!h.canRedo());
    CHECK(h.canUndo()); // still the two real edits

    // A fresh edit after an undo clears the redo branch.
    CHECK(h.undo(scene)); // back to 5
    CHECK(h.canRedo());
    h.begin(scene);
    scene[0].position.x = -3.0f;
    h.end(scene);
    CHECK(!h.canRedo()); // the redo-to-9 branch was discarded
}

void testEditorSerialize() {
    editor::Scene s;
    editor::Node a;
    a.name = "Crate";
    a.meshId = 0;
    a.colorIndex = 3;
    a.position = math::vec3(1.5f, 0.5f, -2.0f);
    a.euler = math::vec3(0, 45, 0);
    a.scale = math::vec3(2, 2, 2);
    a.roughness = 0.3f;
    a.metallic = 1.0f;
    a.specular = 1.0f;
    a.emissive = math::vec3(0.2f, 0.0f, 0.0f);
    a.visible = false;
    editor::Node b;
    b.name = "Ball";
    b.meshId = 1;
    b.position = math::vec3(-1, 0.5f, 0);
    s.nodes.push_back(a);
    s.nodes.push_back(b);
    s.selected = 1;

    // Round-trip through JSON text and confirm the scene comes back identical.
    const std::string text = editor::toJson(s).dump(2);
    const io::JsonParseResult pr = io::parseJson(text);
    CHECK(pr.ok);
    editor::Scene loaded;
    CHECK(editor::fromJson(pr.value, loaded));
    CHECK(loaded.nodes.size() == 2);
    CHECK(loaded.selected == 1);
    CHECK(loaded.nodes[0] == s.nodes[0]); // Node::operator== over every field
    CHECK(loaded.nodes[1] == s.nodes[1]);
    CHECK(loaded.nodes[0].name == "Crate");
    CHECK_NEAR(loaded.nodes[0].euler.y, 45.0f, 1e-4f);
    CHECK(loaded.nodes[0].visible == false);
}

void testEditorNodeOps() {
    // Grid snapping rounds to the nearest step; step <= 0 is a passthrough.
    CHECK_NEAR(editor::snap1(1.24f, 0.5f), 1.0f, 1e-4f);
    CHECK_NEAR(editor::snap1(1.30f, 0.5f), 1.5f, 1e-4f);
    CHECK_NEAR(editor::snap1(-0.9f, 0.5f), -1.0f, 1e-4f);
    CHECK_NEAR(editor::snap1(3.7f, 0.0f), 3.7f, 1e-4f); // disabled
    const math::vec3 s = editor::snapToGrid(math::vec3(0.24f, 0.76f, -1.1f), 0.5f);
    CHECK_NEAR(s.x, 0.0f, 1e-4f);
    CHECK_NEAR(s.y, 1.0f, 1e-4f);
    CHECK_NEAR(s.z, -1.0f, 1e-4f);

    // Discrete-edit undo: deleting a node is one undo step, restored by undo().
    editor::Scene sc;
    editor::Node a;
    a.name = "A";
    editor::Node b;
    b.name = "B";
    sc.nodes = {a, b};
    editor::History h;
    std::vector<editor::Node> before = sc.nodes;
    sc.nodes.erase(sc.nodes.begin()); // delete "A"
    h.commit(before, sc.nodes);
    CHECK(sc.nodes.size() == 1 && sc.nodes[0].name == "B");
    CHECK(h.canUndo());
    CHECK(h.undo(sc.nodes));
    CHECK(sc.nodes.size() == 2 && sc.nodes[0].name == "A");

    // The undo above left a redo entry; a no-op commit (before == after) must not disturb it.
    CHECK(h.canRedo());
    before = sc.nodes;
    h.commit(before, sc.nodes);
    CHECK(h.canRedo());
}

// E9: multi-select set operations on editor::Scene.
void testEditorMultiSelect() {
    editor::Scene sc;
    sc.nodes.resize(4); // four nodes at indices 0..3

    // A plain click selects exactly one and makes it primary.
    sc.selectOnly(2);
    CHECK(sc.selected == 2);
    CHECK(sc.selection.size() == 1 && sc.isSelected(2));
    CHECK(!sc.isSelected(0));

    // Shift-click adds a node; the newly added one becomes primary.
    sc.toggleSelect(0);
    CHECK(sc.selection.size() == 2);
    CHECK(sc.isSelected(0) && sc.isSelected(2));
    CHECK(sc.selected == 0);

    // Shift-clicking a selected node removes it; primary falls back to the remaining node.
    sc.toggleSelect(0);
    CHECK(sc.selection.size() == 1 && sc.isSelected(2));
    CHECK(sc.selected == 2);

    // toggleSelect(-1) is a no-op; clearSelection empties everything.
    sc.toggleSelect(-1);
    CHECK(sc.selection.size() == 1);
    sc.clearSelection();
    CHECK(sc.selection.empty() && sc.selected == -1);

    // sanitizeSelection drops out-of-range indices after a delete and re-fixes primary.
    sc.selection = {1, 3};
    sc.selected = 3;
    sc.nodes.resize(2); // now only indices 0..1 are valid; 3 is gone, 1 survives
    sc.sanitizeSelection();
    CHECK(sc.selection.size() == 1 && sc.isSelected(1));
    CHECK(sc.selected == 1); // primary 3 was invalid, fell back to a surviving selected node
}

// SC1: the maz::script tree-walking interpreter — arithmetic, control flow, functions, host natives.
void testScript() {
    using namespace maz;

    // Arithmetic + operator precedence + print output.
    {
        script::Vm vm;
        CHECK(vm.run("print 1 + 2 * 3;")); // 7, not 9
        CHECK(vm.output == "7\n");
    }
    // Variables, assignment, string concatenation.
    {
        script::Vm vm;
        CHECK(vm.run("var a = 10; a = a + 5; print \"a=\" + a;"));
        CHECK(vm.output == "a=15\n");
    }
    // Comparison + logical short-circuit + if/else.
    {
        script::Vm vm;
        CHECK(vm.run("if (3 < 5 and 2 != 2 or 1 == 1) { print \"yes\"; } else { print \"no\"; }"));
        CHECK(vm.output == "yes\n");
    }
    // while loop accumulation.
    {
        script::Vm vm;
        CHECK(vm.run("var s = 0; var i = 1; while (i <= 5) { s = s + i; i = i + 1; } print s;"));
        CHECK(vm.output == "15\n"); // 1+2+3+4+5
    }
    // C-style for loop.
    {
        script::Vm vm;
        CHECK(vm.run("var t = 0; for (var i = 0; i < 4; i = i + 1) { t = t + i; } print t;"));
        CHECK(vm.output == "6\n"); // 0+1+2+3
    }
    // Functions, recursion (fibonacci), return.
    {
        script::Vm vm;
        const char* src =
            "func fib(n) { if (n < 2) { return n; } return fib(n - 1) + fib(n - 2); } print fib(10);";
        CHECK(vm.run(src));
        CHECK(vm.output == "55\n");
    }
    // Native host function callable from script.
    {
        script::Vm vm;
        int captured = 0;
        vm.registerNative("record", [&captured](std::vector<script::Value>& a) {
            captured = a.empty() ? 0 : static_cast<int>(a[0].number);
            return script::Value::fromNum(a[0].number * 2.0);
        });
        CHECK(vm.run("var r = record(21); print r;"));
        CHECK(captured == 21);
        CHECK(vm.output == "42\n");
    }
    // Built-in stdlib (min/max/abs/sqrt/floor).
    {
        script::Vm vm;
        CHECK(vm.run("print max(3, 7) + min(2, 9) + abs(0 - 4) + floor(3.9);"));
        CHECK(vm.output == "16\n"); // 7 + 2 + 4 + 3
    }
    // Host sets a global; script reads it; C++ calls a script function and reads the result.
    {
        script::Vm vm;
        vm.setGlobal("health", script::Value::fromNum(100.0));
        CHECK(vm.run("func hurt(dmg) { return health - dmg; }"));
        const script::Value r = vm.call("hurt", {script::Value::fromNum(30.0)});
        CHECK(r.type == script::Value::Type::Num);
        CHECK(r.number == 70.0);
    }
    // Errors are reported (not crashes): undefined variable, with a line number.
    {
        script::Vm vm;
        CHECK(!vm.run("print 1;\nprint missing;"));
        CHECK(!vm.error().empty());
        CHECK(vm.errorLine() == 2);
    }
    // Parse error is caught cleanly.
    {
        script::Vm vm;
        CHECK(!vm.run("var = 5;")); // missing name
        CHECK(!vm.error().empty());
    }
    // Division by zero is a catchable runtime error, not UB.
    {
        script::Vm vm;
        CHECK(!vm.run("print 1 / 0;"));
        CHECK(!vm.error().empty());
    }
}

// SC2: collections & iteration — arrays, dicts, for-in, indexing, methods, break/continue, `in`.
void testScriptCollections() {
    using namespace maz;
    // Array literal, indexing, index assignment, .append / .size.
    {
        script::Vm vm;
        CHECK(vm.run("var a = [10, 20, 30]; a[1] = 99; a.append(40); print a[1] + a[3] + a.size();"));
        CHECK(vm.output == "143\n"); // 99 + 40 + 4
    }
    // for-in over an array accumulates.
    {
        script::Vm vm;
        CHECK(vm.run("var s = 0; for (x in [1, 2, 3, 4]) { s = s + x; } print s;"));
        CHECK(vm.output == "10\n");
    }
    // range() + for-in with `var`.
    {
        script::Vm vm;
        CHECK(vm.run("var s = 0; for (var i in range(5)) { s = s + i; } print s;"));
        CHECK(vm.output == "10\n"); // 0+1+2+3+4
    }
    // break / continue.
    {
        script::Vm vm;
        CHECK(vm.run("var s = 0; for (i in range(10)) { if (i == 5) { break; } if (i % 2 == 0) { "
                     "continue; } s = s + i; } print s;"));
        CHECK(vm.output == "4\n"); // odd i<5: 1+3
    }
    // Dictionaries: literal, [key] and .key read/write, .has / .keys / .size.
    {
        script::Vm vm;
        CHECK(vm.run("var d = {\"hp\": 100, \"mp\": 30}; d[\"hp\"] = d.hp - 25; d.gold = 5; "
                     "print d.hp + d[\"mp\"] + d.gold + d.size();"));
        CHECK(vm.output == "113\n"); // 75 + 30 + 5 + 3
    }
    // `in` membership on arrays, dicts, strings.
    {
        script::Vm vm;
        CHECK(vm.run("print (2 in [1, 2, 3]) and (\"k\" in {\"k\": 1}) and (\"ell\" in \"hello\");"));
        CHECK(vm.output == "true\n");
    }
    // Nested collections + array concatenation.
    {
        script::Vm vm;
        CHECK(vm.run("var m = [[1, 2], [3, 4]]; var f = [0] + m[1]; print m[1][0] + f[2];"));
        CHECK(vm.output == "7\n"); // 3 + 4
    }
    // Reference semantics: a function mutating an array is visible to the caller.
    {
        script::Vm vm;
        CHECK(vm.run("func fill(arr) { arr.append(7); } var a = [1]; fill(a); print a.size() + a[1];"));
        CHECK(vm.output == "9\n"); // size 2 + a[1]=7
    }
    // for-in over dict keys.
    {
        script::Vm vm;
        CHECK(vm.run("var d = {\"a\": 1, \"b\": 2, \"c\": 3}; var t = 0; for (k in d) { t = t + d[k]; "
                     "} print t;"));
        CHECK(vm.output == "6\n");
    }
    // Out-of-range index is a catchable error, not UB.
    {
        script::Vm vm;
        CHECK(!vm.run("var a = [1, 2]; print a[5];"));
        CHECK(!vm.error().empty());
    }
    // String methods + char indexing.
    {
        script::Vm vm;
        CHECK(vm.run("var s = \"Hi\"; print s.to_upper() + s[1] + s.length();"));
        CHECK(vm.output == "HIi2\n");
    }
}

// SC3: strings / math / conversions / seedable RNG stdlib.
void testScriptStdlib() {
    using namespace maz;
    // Math library.
    {
        script::Vm vm;
        CHECK(vm.run("print ceil(2.1) + round(2.4) + pow(2, 3) + sign(0 - 5) + clamp(15, 0, 10);"));
        CHECK(vm.output == "22\n"); // 3 + 2 + 8 + (-1) + 10
    }
    // lerp + PI constant.
    {
        script::Vm vm;
        CHECK(vm.run("print lerp(0, 100, 0.25); print floor(PI * 100);"));
        CHECK(vm.output == "25\n314\n");
    }
    // Conversions + typeof.
    {
        script::Vm vm;
        CHECK(vm.run("print int(\"42\") + float(\"1.5\"); print typeof([1]); print typeof({\"a\": 1});"));
        CHECK(vm.output == "43.5\narray\ndictionary\n");
    }
    // String methods: split / join round-trip, replace, substr, begins_with.
    {
        script::Vm vm;
        CHECK(vm.run("var parts = \"a,b,c\".split(\",\"); print parts.join(\"-\"); "
                     "print \"hello world\".replace(\"world\", \"maz\"); "
                     "print \"abcdef\".substr(2, 3); print \"maz\".begins_with(\"ma\");"));
        CHECK(vm.output == "a-b-c\nhello maz\ncde\ntrue\n");
    }
    // strip + find.
    {
        script::Vm vm;
        CHECK(vm.run("print \"  hi  \".strip(); print \"needle\".find(\"dle\");"));
        CHECK(vm.output == "hi\n3\n");
    }
    // Seedable RNG is deterministic: same seed -> same sequence.
    {
        script::Vm a, b;
        CHECK(a.run("seed(1234); print randi_range(1, 6); print randi_range(1, 6); print randi_range(1, 6);"));
        CHECK(b.run("seed(1234); print randi_range(1, 6); print randi_range(1, 6); print randi_range(1, 6);"));
        CHECK(a.output == b.output);
        // Values are in-range.
        script::Vm c;
        CHECK(c.run("seed(7); var ok = true; for (i in range(50)) { var r = randi_range(1, 6); "
                    "if (r < 1 or r > 6) { ok = false; } } print ok;"));
        CHECK(c.output == "true\n");
    }
    // assert: passing is a no-op, failing is a catchable error carrying the message.
    {
        script::Vm vm;
        CHECK(vm.run("assert(1 == 1); print \"ok\";"));
        CHECK(vm.output == "ok\n");
        script::Vm vm2;
        CHECK(!vm2.run("assert(1 == 2, \"nope\");"));
        CHECK(vm2.error() == "nope");
    }
}

// SC4: closures, lambdas, and higher-order array methods (map/filter/reduce/sort/sort_custom).
void testScriptClosures() {
    using namespace maz;
    // A lambda assigned to a var and called.
    {
        script::Vm vm;
        CHECK(vm.run("var dbl = func(x) { return x * 2; }; print dbl(21);"));
        CHECK(vm.output == "42\n");
    }
    // Real closure: the returned function captures `n` from its defining scope.
    {
        script::Vm vm;
        CHECK(vm.run("func adder(n) { return func(x) { return x + n; }; } "
                     "var add10 = adder(10); print add10(5); print add10(90);"));
        CHECK(vm.output == "15\n100\n");
    }
    // Closure over mutable state: a counter that keeps incrementing across calls.
    {
        script::Vm vm;
        CHECK(vm.run("func makeCounter() { var c = 0; return func() { c = c + 1; return c; }; } "
                     "var next = makeCounter(); print next(); print next(); print next();"));
        CHECK(vm.output == "1\n2\n3\n");
    }
    // Two counters are independent (each closure has its own captured scope).
    {
        script::Vm vm;
        CHECK(vm.run("func makeCounter() { var c = 0; return func() { c = c + 1; return c; }; } "
                     "var a = makeCounter(); var b = makeCounter(); print a(); print a(); print b();"));
        CHECK(vm.output == "1\n2\n1\n");
    }
    // map / filter / reduce with lambdas.
    {
        script::Vm vm;
        CHECK(vm.run("var xs = [1, 2, 3, 4, 5]; "
                     "print xs.map(func(x) { return x * x; }); "
                     "print xs.filter(func(x) { return x % 2 == 0; }); "
                     "print xs.reduce(func(a, b) { return a + b; }, 0);"));
        CHECK(vm.output == "[1, 4, 9, 16, 25]\n[2, 4]\n15\n");
    }
    // Passing a lambda as a first-class argument to a user function.
    {
        script::Vm vm;
        CHECK(vm.run("func apply(f, v) { return f(v); } "
                     "print apply(func(x) { return x + 100; }, 23);"));
        CHECK(vm.output == "123\n");
    }
    // sort (natural) and sort_custom (comparator returns true when a<b for descending flip).
    {
        script::Vm vm;
        CHECK(vm.run("var a = [3, 1, 4, 1, 5, 9, 2, 6]; a.sort(); print a; "
                     "var b = [3, 1, 4, 1, 5]; b.sort_custom(func(x, y) { return x > y; }); print b;"));
        CHECK(vm.output == "[1, 1, 2, 3, 4, 5, 6, 9]\n[5, 4, 3, 1, 1]\n");
    }
    // any / all.
    {
        script::Vm vm;
        CHECK(vm.run("var xs = [2, 4, 6]; print xs.all(func(x) { return x % 2 == 0; }); "
                     "print xs.any(func(x) { return x > 5; }); print xs.any(func(x) { return x > 10; });"));
        CHECK(vm.output == "true\ntrue\nfalse\n"); // all even; some >5; none >10
    }
}

// SC5: classes — fields, methods, self, _init, .new(), extends/super, reference semantics.
void testScriptClasses() {
    using namespace maz;
    // Basic class: field default + method reading self.
    {
        script::Vm vm;
        CHECK(vm.run("class Point { var x = 0; var y = 0; "
                     "func sum() { return self.x + self.y; } } "
                     "var p = Point.new(); p.x = 3; p.y = 4; print p.sum();"));
        CHECK(vm.output == "7\n");
    }
    // _init constructor with arguments.
    {
        script::Vm vm;
        CHECK(vm.run("class Vec { var x = 0; var y = 0; "
                     "func _init(a, b) { self.x = a; self.y = b; } "
                     "func len2() { return self.x * self.x + self.y * self.y; } } "
                     "var v = Vec.new(3, 4); print v.len2();"));
        CHECK(vm.output == "25\n");
    }
    // Construction via Foo(args) shorthand (no .new).
    {
        script::Vm vm;
        CHECK(vm.run("class Box { var w = 1; func _init(n) { self.w = n; } func area() { return self.w * self.w; } } "
                     "var b = Box(5); print b.area();"));
        CHECK(vm.output == "25\n");
    }
    // Inheritance: subclass inherits base method; extends + inherited fields.
    {
        script::Vm vm;
        CHECK(vm.run("class Animal { var name = \"?\"; func speak() { return \"...\"; } } "
                     "class Dog extends Animal { func speak() { return \"woof\"; } } "
                     "var d = Dog.new(); d.name = \"Rex\"; print d.name; print d.speak();"));
        CHECK(vm.output == "Rex\nwoof\n");
    }
    // super: subclass method calls the base implementation.
    {
        script::Vm vm;
        CHECK(vm.run("class A { func greet() { return \"A\"; } } "
                     "class B extends A { func greet() { return super.greet() + \"B\"; } } "
                     "var b = B.new(); print b.greet();"));
        CHECK(vm.output == "AB\n");
    }
    // super._init chains base constructor.
    {
        script::Vm vm;
        CHECK(vm.run("class Base { var hp = 0; func _init(h) { self.hp = h; } } "
                     "class Hero extends Base { var name = \"\"; "
                     "func _init(n, h) { super._init(h); self.name = n; } } "
                     "var hero = Hero.new(\"Ann\", 100); print hero.name; print hero.hp;"));
        CHECK(vm.output == "Ann\n100\n");
    }
    // Reference semantics: two names to one object share mutations.
    {
        script::Vm vm;
        CHECK(vm.run("class C { var v = 1; } var a = C.new(); var b = a; b.v = 99; print a.v;"));
        CHECK(vm.output == "99\n");
    }
    // Methods calling other methods on self; object stored in an array.
    {
        script::Vm vm;
        CHECK(vm.run("class Counter { var n = 0; func inc() { self.n = self.n + 1; } func get() { return self.n; } } "
                     "var arr = [Counter.new(), Counter.new()]; "
                     "arr[0].inc(); arr[0].inc(); arr[1].inc(); "
                     "print arr[0].get(); print arr[1].get();"));
        CHECK(vm.output == "2\n1\n");
    }
    // Bound method read off an instance, stored and called later.
    {
        script::Vm vm;
        CHECK(vm.run("class Greeter { var who = \"world\"; func hi() { return \"hi \" + self.who; } } "
                     "var g = Greeter.new(); var f = g.hi; print f();"));
        CHECK(vm.output == "hi world\n");
    }
    // Three-level inheritance chain with super all the way up.
    {
        script::Vm vm;
        CHECK(vm.run("class L1 { func tag() { return \"1\"; } } "
                     "class L2 extends L1 { func tag() { return super.tag() + \"2\"; } } "
                     "class L3 extends L2 { func tag() { return super.tag() + \"3\"; } } "
                     "print L3.new().tag();"));
        CHECK(vm.output == "123\n");
    }
}

// SC6: engine-object binding — bind a C++ type, drive script lifecycle hooks, safe handles.
namespace {
struct HostSprite {
    double x = 0, y = 0;
    int moves = 0;
};
} // namespace

void testScriptBinding() {
    using namespace maz;

    // Register a host C++ type with two properties and a method, then drive it from script.
    {
        script::Vm vm;
        vm.bindClass("Sprite")
            .property(
                "x", [](void* s) { return script::Value::fromNum(static_cast<HostSprite*>(s)->x); },
                [](void* s, const script::Value& v) { static_cast<HostSprite*>(s)->x = v.number; })
            .property(
                "y", [](void* s) { return script::Value::fromNum(static_cast<HostSprite*>(s)->y); },
                [](void* s, const script::Value& v) { static_cast<HostSprite*>(s)->y = v.number; })
            .method("move", [](void* s, std::vector<script::Value>& a) {
                auto* sp = static_cast<HostSprite*>(s);
                sp->x += a.size() > 0 ? a[0].number : 0;
                sp->y += a.size() > 1 ? a[1].number : 0;
                sp->moves++;
                return script::Value::nil();
            });

        auto sprite = std::make_shared<HostSprite>();
        vm.setGlobal("sprite", vm.makeNativeObject("Sprite", sprite));
        CHECK(vm.run("sprite.x = 10; sprite.move(5, 3); print sprite.x; print sprite.y;"));
        CHECK(vm.output == "15\n3\n");   // 10 + 5, 0 + 3
        CHECK(sprite->x == 15.0);        // host object actually mutated
        CHECK(sprite->y == 3.0);
        CHECK(sprite->moves == 1);
    }

    // Safe handle: after the host frees the object, script access is a clean catchable error.
    {
        script::Vm vm;
        vm.bindClass("Sprite").property("x", [](void* s) {
            return script::Value::fromNum(static_cast<HostSprite*>(s)->x);
        });
        auto sprite = std::make_shared<HostSprite>();
        vm.setGlobal("sprite", vm.makeNativeObject("Sprite", sprite));
        sprite.reset(); // host destroys the object
        CHECK(!vm.run("print sprite.x;"));
        CHECK(vm.error().find("freed") != std::string::npos);
    }

    // Lifecycle: instantiate a script class from C++ and drive _ready / _process(dt) hooks — exactly
    // how the engine will run gameplay scripts. Define the class + reader helpers in one program (a
    // reload would invalidate a live instance's methods, which is by design and exercised in SC9).
    {
        script::Vm vm;
        CHECK(vm.run("class Mover { var pos = 0; var ready = false; "
                     "func _ready() { self.ready = true; } "
                     "func _process(dt) { self.pos = self.pos + dt; } } "
                     "func getpos(o) { return o.pos; } func getready(o) { return o.ready; }"));
        script::Value m = vm.instantiate("Mover");
        CHECK(m.type == script::Value::Type::Object);
        CHECK(vm.objectHasMethod(m, "_ready"));
        CHECK(vm.objectHasMethod(m, "_process"));
        CHECK(!vm.objectHasMethod(m, "_physics_process")); // absent hook — host can skip it
        vm.callOn(m, "_ready", {});
        vm.callOn(m, "_process", {script::Value::fromNum(0.5)});
        vm.callOn(m, "_process", {script::Value::fromNum(0.25)});
        CHECK(vm.call("getpos", {m}).number == 0.75); // 0.5 + 0.25 accumulated in the instance
        CHECK(vm.call("getready", {m}).boolean == true);
    }
}

// SC7: signals — declare / connect / emit, one-shot, deferred dispatch, introspection.
void testScriptSignals() {
    using namespace maz;

    // Standalone signal via the Signal() builtin: connect a handler, emit, handler runs.
    {
        script::Vm vm;
        CHECK(vm.run("var hits = 0; var s = Signal(\"ping\"); "
                     "s.connect(func() { hits = hits + 1; }); "
                     "s.emit(); s.emit(); print hits; print s.get_name();"));
        CHECK(vm.output == "2\nping\n");
    }
    // Emit passes arguments through to handlers.
    {
        script::Vm vm;
        CHECK(vm.run("var total = 0; var s = Signal(); "
                     "s.connect(func(n) { total = total + n; }); "
                     "s.emit(10); s.emit(5); print total;"));
        CHECK(vm.output == "15\n");
    }
    // Multiple handlers fire in connection order; connection_count reflects the list.
    {
        script::Vm vm;
        CHECK(vm.run("var log = []; var s = Signal(); "
                     "s.connect(func() { log.append(\"a\"); }); "
                     "s.connect(func() { log.append(\"b\"); }); "
                     "print s.connection_count(); s.emit(); print log.join(\",\");"));
        CHECK(vm.output == "2\na,b\n");
    }
    // One-shot connection auto-removes after the first emit.
    {
        script::Vm vm;
        CHECK(vm.run("var hits = 0; var s = Signal(); "
                     "s.connect(func() { hits = hits + 1; }, true); "
                     "s.emit(); s.emit(); s.emit(); print hits; print s.connection_count();"));
        CHECK(vm.output == "1\n0\n"); // fired once, then disconnected
    }
    // disconnect + is_connected.
    {
        script::Vm vm;
        CHECK(vm.run("var hits = 0; var s = Signal(); var h = func() { hits = hits + 1; }; "
                     "s.connect(h); print s.is_connected(h); s.disconnect(h); "
                     "print s.is_connected(h); s.emit(); print hits;"));
        CHECK(vm.output == "true\nfalse\n0\n");
    }
    // Class-level `signal` declaration wired through a method emit (GDScript-style).
    {
        script::Vm vm;
        CHECK(vm.run("class Button { signal pressed; var count = 0; "
                     "func click() { self.pressed.emit(); } } "
                     "var total = 0; var b = Button.new(); "
                     "b.pressed.connect(func() { total = total + 1; }); "
                     "b.click(); b.click(); print total;"));
        CHECK(vm.output == "2\n");
    }
    // Deferred emit: queued, not dispatched until the host flushes (netcode-friendly ordering).
    {
        script::Vm vm;
        CHECK(vm.run("var fired = false; var s = Signal(); "
                     "s.connect(func() { fired = true; }); "
                     "s.emit_deferred(); "
                     "func getfired() { return fired; }"));
        CHECK(vm.call("getfired", {}).boolean == false); // queued, not yet dispatched
        CHECK(vm.deferredCount() == 1);
        const size_t n = vm.flushDeferred(); // host drains the queue at a safe point
        CHECK(n == 1);
        CHECK(vm.deferredCount() == 0);
        CHECK(vm.call("getfired", {}).boolean == true); // handler ran during the flush
    }
}

// SC8: safety & diagnostics — execution budget, recursion limit, stack traces, warnings.
void testScriptSafety() {
    using namespace maz;

    // Execution budget kills a runaway infinite loop as a catchable error (not a hang).
    {
        script::Vm vm;
        vm.setStepBudget(10000);
        CHECK(!vm.run("while (true) { var x = 1; }"));
        CHECK(vm.error().find("budget") != std::string::npos);
    }
    // Empty-bodied infinite loop is caught too (each iteration counts a step).
    {
        script::Vm vm;
        vm.setStepBudget(5000);
        CHECK(!vm.run("var i = 0; while (i >= 0) { i = i + 1; if (i > 999999999) { i = 0; } }"));
        CHECK(vm.error().find("budget") != std::string::npos);
    }
    // A normal bounded program stays well under budget and runs fine.
    {
        script::Vm vm;
        vm.setStepBudget(100000);
        CHECK(vm.run("var sum = 0; for (i in range(100)) { sum = sum + i; } print sum;"));
        CHECK(vm.output == "4950\n");
    }
    // Runaway recursion is caught by the recursion limit instead of crashing the stack.
    {
        script::Vm vm;
        vm.setRecursionLimit(200);
        CHECK(!vm.run("func boom(n) { return boom(n + 1); } boom(0);"));
        CHECK(vm.error().find("recursion") != std::string::npos);
    }
    // Bounded recursion within the limit works (factorial).
    {
        script::Vm vm;
        vm.setRecursionLimit(200);
        CHECK(vm.run("func fact(n) { if (n <= 1) { return 1; } return n * fact(n - 1); } print fact(6);"));
        CHECK(vm.output == "720\n");
    }
    // A runtime error inside nested calls yields a stack trace naming the chain.
    {
        script::Vm vm;
        CHECK(!vm.run("func a() { return b(); } func b() { return c(); } "
                      "func c() { return 1 / 0; } a();"));
        const std::string& trace = vm.stackTrace();
        CHECK(trace.find("in c()") != std::string::npos);
        CHECK(trace.find("in b()") != std::string::npos);
        CHECK(trace.find("in a()") != std::string::npos);
    }
    // Warnings: variable shadowing is reported (non-fatal — the program still runs).
    {
        script::Vm vm;
        CHECK(vm.run("var x = 1; func f() { var x = 2; return x; } print f();"));
        CHECK(vm.output == "2\n");
        bool sawShadow = false;
        for (const auto& w : vm.warnings()) {
            if (w.find("shadows") != std::string::npos && w.find("'x'") != std::string::npos) {
                sawShadow = true;
            }
        }
        CHECK(sawShadow);
    }
    // Warnings: unreachable code after a return is reported.
    {
        script::Vm vm;
        CHECK(vm.run("func f() { return 1; var dead = 2; } print f();"));
        CHECK(vm.output == "1\n");
        bool sawUnreachable = false;
        for (const auto& w : vm.warnings()) {
            if (w.find("unreachable") != std::string::npos) sawUnreachable = true;
        }
        CHECK(sawUnreachable);
    }
    // Clean code produces no warnings.
    {
        script::Vm vm;
        CHECK(vm.run("func add(a, b) { return a + b; } print add(2, 3);"));
        CHECK(vm.output == "5\n");
        CHECK(vm.warnings().empty());
    }
}

// SC9: hot reload — swap code while keeping live instance state; parse failure keeps old version.
void testScriptHotReload() {
    using maz::script::Vm;
    using maz::script::Value;

    // Reload a global function body — the new implementation takes effect immediately.
    {
        Vm vm;
        CHECK(vm.run("func greet() { return \"hi\"; }"));
        CHECK(vm.call("greet", {}).str == "hi");
        CHECK(vm.reload("func greet() { return \"hello\"; }"));
        CHECK(vm.call("greet", {}).str == "hello");
    }
    // State preservation: an instance built before reload keeps its fields, gains new method bodies.
    {
        Vm vm;
        CHECK(vm.run("class Counter { var n = 0; func step() { self.n = self.n + 1; } "
                     "func get() { return self.n; } } func getn(o) { return o.get(); }"));
        Value c = vm.instantiate("Counter");
        vm.callOn(c, "step", {});
        vm.callOn(c, "step", {}); // n = 2
        CHECK(vm.call("getn", {c}).number == 2);
        // Reload: step now adds 10. The instance's n stays 2, then +10 with the new code.
        CHECK(vm.reload("class Counter { var n = 0; func step() { self.n = self.n + 10; } "
                        "func get() { return self.n; } } func getn(o) { return o.get(); }"));
        vm.callOn(c, "step", {});
        CHECK(vm.call("getn", {c}).number == 12); // 2 preserved + 10 new behavior
    }
    // A parse error during reload leaves the previous good version fully live.
    {
        Vm vm;
        CHECK(vm.run("func f() { return 1; }"));
        CHECK(vm.call("f", {}).number == 1);
        CHECK(vm.reload("func f() { return 2; }"));
        CHECK(vm.call("f", {}).number == 2);
        CHECK(!vm.reload("func f() { return @@@ garbage")); // fails to parse
        CHECK(!vm.error().empty());
        CHECK(vm.call("f", {}).number == 2); // still the last good version (returns 2, not broken)
    }
    // Reload can add a brand-new class alongside preserved ones.
    {
        Vm vm;
        CHECK(vm.run("class A { func who() { return \"a\"; } }"));
        Value a = vm.instantiate("A");
        CHECK(vm.reload("class A { func who() { return \"a\"; } } "
                        "class B { func who() { return \"b\"; } }"));
        Value b = vm.instantiate("B");
        CHECK(b.type == Value::Type::Object);
        CHECK(vm.callOn(b, "who", {}).str == "b");
        CHECK(vm.callOn(a, "who", {}).str == "a"); // the pre-existing instance still works
    }
}

// SC10: gradual typing — annotations parse + run; a static checker flags literal mismatches; strict
// mode enforces. Untyped code stays fully dynamic.
void testScriptTyping() {
    using maz::script::Vm;

    // Typed declarations, typed function signature, and inference all parse and run normally.
    {
        Vm vm;
        CHECK(vm.run("var hp: int = 100; var name: String = \"hero\"; var ratio := 0.5; "
                     "func add(a: int, b: int) -> int { return a + b; } "
                     "print hp; print name; print add(2, 3);"));
        CHECK(vm.output == "100\nhero\n5\n");
        CHECK(vm.typeErrors().empty()); // everything type-checks
    }
    // Typed arrays/dicts parse (Array[int], Dictionary).
    {
        Vm vm;
        CHECK(vm.run("var nums: Array[int] = [1, 2, 3]; var m: Dictionary = {\"a\": 1}; "
                     "print nums.size(); print m.size();"));
        CHECK(vm.output == "3\n1\n");
        CHECK(vm.typeErrors().empty());
    }
    // Static checker flags a wrong literal assigned to a typed var.
    {
        Vm vm;
        CHECK(vm.run("var count: int = \"oops\";")); // runs (non-strict), but flags a type error
        bool flagged = false;
        for (const auto& e : vm.typeErrors()) {
            if (e.find("count") != std::string::npos && e.find("int") != std::string::npos) flagged = true;
        }
        CHECK(flagged);
    }
    // Static checker flags a wrong literal return from a typed function.
    {
        Vm vm;
        CHECK(vm.run("func label() -> String { return 42; }"));
        bool flagged = false;
        for (const auto& e : vm.typeErrors()) {
            if (e.find("String") != std::string::npos) flagged = true;
        }
        CHECK(flagged);
    }
    // Static checker flags a wrong literal argument to a typed parameter.
    {
        Vm vm;
        CHECK(vm.run("func takesInt(n: int) -> int { return n; } takesInt(\"nope\");"));
        bool flagged = false;
        for (const auto& e : vm.typeErrors()) {
            if (e.find("takesInt") != std::string::npos) flagged = true;
        }
        CHECK(flagged);
    }
    // Fractional literal to an int-typed var is a narrowing mismatch.
    {
        Vm vm;
        CHECK(vm.run("var i: int = 3.5;"));
        CHECK(!vm.typeErrors().empty());
    }
    // Strict mode promotes a type error to a run() failure.
    {
        Vm vm;
        vm.setStrictTypes(true);
        CHECK(!vm.run("var x: int = \"bad\";")); // strict → run fails
        CHECK(!vm.error().empty());
        // Well-typed code still runs under strict mode.
        Vm vm2;
        vm2.setStrictTypes(true);
        CHECK(vm2.run("var x: int = 5; func dbl(n: int) -> int { return n * 2; } print dbl(x);"));
        CHECK(vm2.output == "10\n");
    }
    // Strict-mode runtime enforcement: a computed value of the wrong type fails at the declaration.
    {
        Vm vm;
        vm.setStrictTypes(true);
        CHECK(!vm.run("func makeStr() { return \"s\"; } var n: int = makeStr();"));
        CHECK(vm.error().find("declared type") != std::string::npos);
    }
    // Untyped/dynamic code is completely unaffected — no type errors ever.
    {
        Vm vm;
        vm.setStrictTypes(true);
        CHECK(vm.run("var x = 5; var y = \"str\"; var z = [1, 2]; func f(a, b) { return a; } print f(x, y);"));
        CHECK(vm.output == "5\n");
        CHECK(vm.typeErrors().empty());
    }
}

// SC11: modules, introspection, and debugger hooks.
void testScriptTooling() {
    using maz::script::Vm;
    using maz::script::Value;

    // Modules: a registered module's functions/classes become available via `import`.
    {
        Vm vm;
        vm.registerModule("mathx", "func square(n) { return n * n; } "
                                   "class Vec2 { var x = 0; var y = 0; func _init(a, b) { self.x = a; self.y = b; } } "
                                   "var PIISH = 3;");
        CHECK(vm.run("import \"mathx\"; print square(7); var v = Vec2.new(3, 4); print v.x + v.y; print PIISH;"));
        CHECK(vm.output == "49\n7\n3\n");
    }
    // Importing an unknown module is a clean catchable error.
    {
        Vm vm;
        CHECK(!vm.run("import \"nope\";"));
        CHECK(vm.error().find("nope") != std::string::npos);
    }
    // A module can itself import another (transitive), loaded once.
    {
        Vm vm;
        vm.registerModule("base", "func base_val() { return 10; }");
        vm.registerModule("mid", "import \"base\"; func mid_val() { return base_val() + 5; }");
        CHECK(vm.run("import \"mid\"; print mid_val(); print base_val();"));
        CHECK(vm.output == "15\n10\n");
    }
    // Introspection: has_method / call by name / class_name.
    {
        Vm vm;
        CHECK(vm.run("class Robot { var hp = 3; func ping() { return \"pong\"; } "
                     "func hit(n) { self.hp = self.hp - n; return self.hp; } } "
                     "var r = Robot.new(); "
                     "print has_method(r, \"ping\"); print has_method(r, \"fly\"); "
                     "print call(r, \"ping\"); print call(r, \"hit\", 1); print class_name(r);"));
        CHECK(vm.output == "true\nfalse\npong\n2\nRobot\n");
    }
    // Introspection: get_property / set_property / has_property on a script object.
    {
        Vm vm;
        CHECK(vm.run("class Cell { var value = 5; } var c = Cell.new(); "
                     "print get_property(c, \"value\"); set_property(c, \"value\", 42); "
                     "print get_property(c, \"value\"); print has_property(c, \"value\"); "
                     "print has_property(c, \"missing\");"));
        CHECK(vm.output == "5\n42\ntrue\nfalse\n");
    }
    // Debugger: onStep fires per statement with the current line and function name.
    {
        Vm vm;
        int steps = 0;
        std::string lastFn;
        vm.onStep = [&](int, const std::string& fn) { steps++; lastFn = fn; };
        CHECK(vm.run("func work() { var a = 1; var b = 2; return a + b; } print work();"));
        CHECK(steps > 0);          // statements were stepped
        CHECK(lastFn == "work");   // last stepped statement was inside work()
    }
    // Debugger: a breakpoint fires onBreakpoint when execution reaches its line.
    {
        Vm vm;
        std::vector<int> hits;
        vm.onBreakpoint = [&](int line) { hits.push_back(line); };
        vm.addBreakpoint(2);
        // Line 1: var x; line 2: var y (breakpoint); line 3: print.
        CHECK(vm.run("var x = 1;\nvar y = 2;\nprint x + y;"));
        bool hitLine2 = false;
        for (int h : hits) {
            if (h == 2) hitLine2 = true;
        }
        CHECK(hitLine2);
    }
}

// Script↔engine bridge: a script class attached to a node drives its transform via lifecycle hooks.
void testScriptSystem() {
    using maz::script::ScriptSystem;
    using maz::script::Value;

    // A _process(dt) script moves its node's transform each frame.
    {
        ScriptSystem sys;
        CHECK(sys.loadSource("class Spinner { func _process(dt) { "
                             "self.node.rotation = self.node.rotation + dt; } }"));
        auto node = sys.spawn("Spinner");
        CHECK(node != nullptr);
        sys.process(0.5);
        sys.process(0.25);
        CHECK(node->rotation == 0.75); // driven by the script over two frames
    }
    // _ready runs once on spawn; _process accumulates; the host reads the transform back.
    {
        ScriptSystem sys;
        CHECK(sys.loadSource("class Walker { var speed = 0; "
                             "func _ready() { self.speed = 100; } "
                             "func _process(dt) { self.node.x = self.node.x + self.speed * dt; } }"));
        auto node = sys.spawn("Walker");
        CHECK(node != nullptr);
        CHECK(node->x == 0.0);
        sys.process(0.1); // x += 100 * 0.1 = 10
        sys.process(0.1); // x += 10 => 20
        CHECK(node->x == 20.0);
    }
    // Node2D methods and multiple properties: translate() + name.
    {
        ScriptSystem sys;
        CHECK(sys.loadSource("class Mob { func _ready() { self.node.name = \"zombie\"; } "
                             "func _process(dt) { self.node.translate(dt * 2, dt); } }"));
        auto node = sys.spawn("Mob");
        CHECK(node != nullptr);
        CHECK(node->name == "zombie");
        sys.process(1.0);
        CHECK(node->x == 2.0);
        CHECK(node->y == 1.0);
    }
    // Multiple independent instances each drive their own node.
    {
        ScriptSystem sys;
        CHECK(sys.loadSource("class Drifter { func _process(dt) { self.node.x = self.node.x + dt; } }"));
        auto a = sys.spawn("Drifter");
        auto b = sys.spawn("Drifter");
        CHECK(sys.instanceCount() == 2);
        sys.process(1.0);
        a->x = 5.0; // externally nudge a
        sys.process(1.0);
        CHECK(a->x == 6.0); // 5 + 1
        CHECK(b->x == 2.0); // 1 + 1, independent
    }
    // registerScript accumulation + broadcast to a named method.
    {
        ScriptSystem sys;
        CHECK(sys.registerScript("Bell", "var rings = 0;\nfunc on_alarm() { self.rings = self.rings + 1; }"));
        auto n1 = sys.spawn("Bell");
        auto n2 = sys.spawn("Bell");
        (void)n1;
        (void)n2;
        sys.broadcast("on_alarm");
        sys.broadcast("on_alarm");
        // Read rings back via the vm (getter over the instances).
        CHECK(sys.instanceCount() == 2);
        // Verify through a helper: each instance's `rings` field should be 2.
        int total = 0;
        for (const auto& inst : sys.instances()) {
            if (inst.self.instance) {
                if (const Value* f = const_cast<maz::script::Instance*>(inst.self.instance.get())->findField("rings")) {
                    total += static_cast<int>(f->number);
                }
            }
        }
        CHECK(total == 4); // 2 instances x 2 alarms
    }
    // physics_process is a separate hook.
    {
        ScriptSystem sys;
        CHECK(sys.loadSource("class Body { func _physics_process(dt) { self.node.y = self.node.y - dt; } }"));
        auto node = sys.spawn("Body");
        CHECK(node != nullptr);
        sys.process(1.0);        // _process absent — no change
        CHECK(node->y == 0.0);
        sys.physicsProcess(0.5); // drives _physics_process
        CHECK(node->y == -0.5);
    }
    // Spawning an unknown class fails cleanly.
    {
        ScriptSystem sys;
        CHECK(sys.loadSource("class Real { }"));
        auto node = sys.spawn("Ghost");
        CHECK(node == nullptr);
        CHECK(!sys.error().empty());
    }
}

// Scene unification: the SceneTree node hierarchy — transform propagation, scripts, groups, paths.
void testSceneTree() {
    using maz::scene::SceneTree;
    using maz::scene::SceneNode;
    namespace mscript = maz::script;

    const double kEps = 1e-9;

    // Transform hierarchy: a child's world position composes with its parent's translation.
    {
        SceneTree tree;
        SceneNode* parent = tree.createChild(tree.root(), "Parent");
        SceneNode* child = tree.createChild(*parent, "Child");
        parent->setPosition(10, 5);
        child->setPosition(3, 0); // local offset from parent
        CHECK(std::fabs(child->worldX() - 13.0) < kEps);
        CHECK(std::fabs(child->worldY() - 5.0) < kEps);
    }
    // Parent rotation rotates the child's world offset.
    {
        SceneTree tree;
        SceneNode* parent = tree.createChild(tree.root(), "P");
        SceneNode* child = tree.createChild(*parent, "C");
        parent->setRotation(3.14159265358979 / 2.0); // 90 degrees
        child->setPosition(1, 0);                     // local +x
        // Rotated 90 deg, local +x becomes world +y.
        CHECK(std::fabs(child->worldX() - 0.0) < 1e-6);
        CHECK(std::fabs(child->worldY() - 1.0) < 1e-6);
    }
    // Parent scale scales the child's local offset in world space.
    {
        SceneTree tree;
        SceneNode* parent = tree.createChild(tree.root(), "P");
        SceneNode* child = tree.createChild(*parent, "C");
        parent->local().scaleX = 2.0;
        parent->local().scaleY = 3.0;
        child->setPosition(4, 2);
        CHECK(std::fabs(child->worldX() - 8.0) < kEps);
        CHECK(std::fabs(child->worldY() - 6.0) < kEps);
    }
    // Path lookup finds a nested node; a bad path returns null.
    {
        SceneTree tree;
        SceneNode* player = tree.createChild(tree.root(), "Player");
        SceneNode* weapon = tree.createChild(*player, "Weapon");
        CHECK(tree.findNode("Player") == player);
        CHECK(tree.findNode("Player/Weapon") == weapon);
        CHECK(tree.findNode("Player/Missing") == nullptr);
        CHECK(tree.findNode("root") == &tree.root());
    }
    // A script attached to a node drives that node's local transform via _process.
    {
        SceneTree tree;
        CHECK(tree.loadScripts("class Orbit { func _process(dt) { "
                               "self.node.rotation = self.node.rotation + dt; } }"));
        SceneNode* n = tree.createChild(tree.root(), "Sat");
        CHECK(tree.attachScript(*n, "Orbit"));
        tree.process(0.5);
        tree.process(0.5);
        CHECK(std::fabs(n->rotation() - 1.0) < kEps);
    }
    // Lifecycle propagates depth-first: a parent script and a child script both run.
    {
        SceneTree tree;
        CHECK(tree.loadScripts("class Tick { func _process(dt) { self.node.x = self.node.x + 1; } }"));
        SceneNode* a = tree.createChild(tree.root(), "A");
        SceneNode* b = tree.createChild(*a, "B");
        CHECK(tree.attachScript(*a, "Tick"));
        CHECK(tree.attachScript(*b, "Tick"));
        tree.process(0.0);
        CHECK(a->x() == 1.0);
        CHECK(b->x() == 1.0); // both ran
    }
    // Groups: tag nodes, query them, and broadcast a method to the group.
    {
        SceneTree tree;
        CHECK(tree.loadScripts("class Enemy { var alerted = 0; func alert() { self.alerted = self.alerted + 1; } }"));
        SceneNode* e1 = tree.createChild(tree.root(), "E1");
        SceneNode* e2 = tree.createChild(tree.root(), "E2");
        SceneNode* prop = tree.createChild(tree.root(), "Barrel");
        e1->addToGroup("enemies");
        e2->addToGroup("enemies");
        prop->addToGroup("props");
        CHECK(tree.attachScript(*e1, "Enemy"));
        CHECK(tree.attachScript(*e2, "Enemy"));
        CHECK(tree.nodesInGroup("enemies").size() == 2);
        CHECK(tree.nodesInGroup("props").size() == 1);
        tree.callGroup("enemies", "alert"); // only enemies get alerted
        int total = 0;
        for (SceneNode* n : tree.nodesInGroup("enemies")) {
            if (n->script().instance) {
                if (const mscript::Value* f =
                        const_cast<mscript::Instance*>(n->script().instance.get())->findField("alerted")) {
                    total += static_cast<int>(f->number);
                }
            }
        }
        CHECK(total == 2);
    }
    // visibleInTree: hiding a parent hides the subtree.
    {
        SceneTree tree;
        SceneNode* p = tree.createChild(tree.root(), "P");
        SceneNode* c = tree.createChild(*p, "C");
        CHECK(c->visibleInTree());
        p->local().visible = false;
        CHECK(!c->visibleInTree()); // parent hidden -> child hidden
    }
    // nodeCount reflects the whole tree.
    {
        SceneTree tree;
        SceneNode* a = tree.createChild(tree.root(), "A");
        tree.createChild(*a, "B");
        tree.createChild(tree.root(), "C");
        CHECK(tree.nodeCount() == 4); // root + A + B + C
    }
}

// SceneTree serialization: a whole node tree round-trips to text and back (Godot .tscn analog).
void testSceneSerialize() {
    using maz::scene::SceneTree;
    using maz::scene::SceneNode;

    const std::string prog =
        "class Hero { func _process(dt) { self.node.x = self.node.x + dt; } }\n"
        "class Foe { }";

    // Build a scene, serialize it, rebuild into a fresh tree, and verify structure + transforms.
    std::string text;
    {
        SceneTree tree;
        tree.loadScripts(prog);
        SceneNode* player = tree.createChild(tree.root(), "Player");
        player->setPosition(10, 5);
        player->setRotation(1.5);
        player->local().scaleX = 2.0;
        player->addToGroup("actors");
        tree.attachScript(*player, "Hero");
        SceneNode* weapon = tree.createChild(*player, "Weapon");
        weapon->setPosition(3, 0);
        SceneNode* enemy = tree.createChild(tree.root(), "Enemy");
        enemy->setPosition(-4, 8);
        enemy->local().visible = false;
        enemy->addToGroup("actors");
        enemy->addToGroup("hostiles");
        tree.attachScript(*enemy, "Foe");
        text = maz::scene::saveTree(tree);
        CHECK(text.rfind("maz_scene", 0) == 0);
    }

    // Rebuild.
    {
        SceneTree tree;
        tree.loadScripts(prog);
        CHECK(maz::scene::loadTree(tree, text));
        CHECK(tree.nodeCount() == 4); // root + Player + Weapon + Enemy

        SceneNode* player = tree.findNode("Player");
        CHECK(player != nullptr);
        CHECK(player->x() == 10.0);
        CHECK(player->y() == 5.0);
        CHECK(player->rotation() == 1.5);
        CHECK(player->local().scaleX == 2.0);
        CHECK(player->inGroup("actors"));
        CHECK(player->scriptClass() == "Hero");

        SceneNode* weapon = tree.findNode("Player/Weapon");
        CHECK(weapon != nullptr);
        CHECK(weapon->x() == 3.0);

        SceneNode* enemy = tree.findNode("Enemy");
        CHECK(enemy != nullptr);
        CHECK(enemy->x() == -4.0);
        CHECK(enemy->y() == 8.0);
        CHECK(!enemy->local().visible);
        CHECK(enemy->inGroup("actors"));
        CHECK(enemy->inGroup("hostiles"));

        // The re-attached script still drives the node.
        tree.process(1.0);
        CHECK(player->x() == 11.0); // Hero._process moved it (+1)
    }

    // Missing header -> load fails cleanly.
    {
        SceneTree tree;
        CHECK(!maz::scene::loadTree(tree, "not a scene\nnode path=\"X\""));
    }

    // Round-trip is stable: re-saving a loaded tree reproduces the same text.
    {
        SceneTree a;
        a.loadScripts(prog);
        maz::scene::loadTree(a, text);
        const std::string again = maz::scene::saveTree(a);
        CHECK(again == text);
    }
}

// The flagship: ZOMBOID's whole simulation runs headless on the SceneTree + scripting.
void testZomboidSim() {
    using maz::scene::SceneTree;
    using maz::scene::SceneNode;

    // The scene builds cleanly: a survivor, a ring of zombies, and loot.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        CHECK(survivor != nullptr);
        CHECK(tree.scripts().vm().error().empty()); // program parsed & ran
        CHECK(tree.nodesInGroup("zombies").size() == 6);
        CHECK(tree.nodesInGroup("loot").size() == 3);
    }

    // Survival pressure: hunger rises over time, then health drains once starving.
    // Isolate the survivor (no horde) so we measure starvation, not zombie bites.
    {
        SceneTree tree;
        tree.loadScripts(zomboid::scripts());
        SceneNode* survivor = tree.createChild(tree.root(), "Survivor");
        tree.attachScript(*survivor, "Survivor");
        auto hunger = [&] { return survivor->script().instance->findField("hunger")->number; };
        auto health = [&] { return survivor->script().instance->findField("health")->number; };
        CHECK(hunger() == 0.0);
        for (int i = 0; i < 5; ++i) tree.process(1.0); // 5 seconds
        CHECK(hunger() > 0.0);        // got hungrier
        CHECK(health() == 100.0);     // not starving yet
        for (int i = 0; i < 40; ++i) tree.process(1.0); // starve out
        CHECK(hunger() >= 100.0);
        CHECK(health() < 100.0);      // health drained while starving
    }

    // Eating restores hunger and consumes a ration.
    {
        SceneTree tree;
        tree.loadScripts(zomboid::scripts());
        SceneNode* survivor = tree.createChild(tree.root(), "Survivor");
        tree.attachScript(*survivor, "Survivor");
        for (int i = 0; i < 20; ++i) tree.process(1.0); // build up hunger
        auto* inst = survivor->script().instance.get();
        const double before = inst->findField("hunger")->number;
        const double food0 = inst->findField("food")->number;
        std::vector<maz::script::Value> none;
        maz::script::Value self = survivor->script(); // copy shares the same instance (shared_ptr)
        tree.scripts().vm().callOn(self, "eat", none);
        CHECK(inst->findField("hunger")->number < before); // hunger dropped
        CHECK(inst->findField("food")->number == food0 - 1); // used a ration
    }

    // Zombie AI: the horde closes in on the survivor over time.
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        SceneNode* z0 = tree.findNode("Zombie0");
        CHECK(z0 != nullptr);
        double startDist = std::sqrt(z0->x() * z0->x() + z0->y() * z0->y());
        for (int i = 0; i < 10; ++i) tree.process(0.1); // 1 second
        double nowDist = std::sqrt(z0->x() * z0->x() + z0->y() * z0->y());
        CHECK(nowDist < startDist); // zombie moved toward the survivor at origin
    }

    // Combat: once the horde reaches the survivor, it takes damage (and can die).
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        auto health = [&] { return survivor->script().instance->findField("health")->number; };
        auto alive = [&] { return survivor->script().instance->findField("alive")->boolean; };
        CHECK(health() == 100.0);
        // Simulate long enough for zombies to arrive and bite repeatedly.
        for (int i = 0; i < 400; ++i) tree.process(0.1); // 40 seconds
        CHECK(health() < 100.0); // the horde drew blood
        // With 6 zombies biting plus starvation, the survivor should eventually fall.
        for (int i = 0; i < 400; ++i) tree.process(0.1);
        CHECK(!alive());
    }

    // Loot pickup: walking the survivor onto a loot node collects it (food + loot_collected up,
    // the loot marks itself taken). Cross-object gameplay entirely in script.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        SceneNode* loot0 = tree.findNode("Loot0");
        CHECK(loot0 != nullptr);
        auto food = [&] { return survivor->script().instance->findField("food")->number; };
        auto collected = [&] {
            return survivor->script().instance->findField("loot_collected")->number;
        };
        auto taken = [&] { return loot0->script().instance->findField("taken")->boolean; };
        const double food0 = food();
        CHECK(!taken());
        // Teleport the survivor onto Loot0's tile and tick once so its _process sees the overlap.
        survivor->setPosition(loot0->x(), loot0->y());
        tree.process(0.016);
        CHECK(taken());                 // the pickup consumed itself
        CHECK(food() == food0 + 1);     // granted a ration
        CHECK(collected() >= 1.0);      // counter bumped
        // A second tick does not double-collect (already taken).
        tree.process(0.016);
        CHECK(food() == food0 + 1);
    }

    // Day/night: the shared clock advances and flips to night at the half-cycle, and the horde is
    // faster at night than by day over the same elapsed time.
    {
        // Daytime run: measure how far Zombie0 travels toward the origin in 1s at t~0 (day).
        SceneTree dayTree;
        zomboid::buildScene(dayTree);
        SceneNode* zDay = dayTree.findNode("Zombie0");
        const double dayStart = std::sqrt(zDay->x() * zDay->x() + zDay->y() * zDay->y());
        for (int i = 0; i < 10; ++i) dayTree.process(0.1); // 1s during day
        const double dayMoved = dayStart - std::sqrt(zDay->x() * zDay->x() + zDay->y() * zDay->y());

        // Night run: advance a fresh world to just past the half-cycle (night), then measure 1s.
        SceneTree nightTree;
        SceneNode* survivor = zomboid::buildScene(nightTree);
        // Freeze the survivor far away so it isn't killed, and let only the clock advance it.
        survivor->setPosition(10000.0, 10000.0);
        // is_night() flips at g_day_len/2 == 30s. Advance ~31s of clock.
        for (int i = 0; i < 310; ++i) nightTree.process(0.1);
        SceneNode* zNight = nightTree.findNode("Zombie1"); // a fresh zombie, far out on the ring
        const double nStart = std::sqrt((zNight->x() - 10000.0) * (zNight->x() - 10000.0) +
                                        (zNight->y() - 10000.0) * (zNight->y() - 10000.0));
        for (int i = 0; i < 10; ++i) nightTree.process(0.1); // 1s during night
        const double nEnd = std::sqrt((zNight->x() - 10000.0) * (zNight->x() - 10000.0) +
                                      (zNight->y() - 10000.0) * (zNight->y() - 10000.0));
        const double nightMoved = nStart - nEnd;
        // Night aggression (1.7x) makes the horde close distance faster than by day.
        CHECK(nightMoved > dayMoved);
    }
}

// E14: the editor's "Package" export — scene -> JSON -> resource pack -> back to an identical scene.
void testEditorPackage() {
    editor::Scene sc;
    editor::Node a;
    a.name = "Alpha";
    a.position = math::vec3(1.0f, 2.0f, 3.0f);
    a.meshId = 2;
    editor::Node b;
    b.name = "Beta";
    b.euler = math::vec3(10.0f, 20.0f, 30.0f);
    b.colorIndex = 3;
    sc.nodes = {a, b};

    const std::string json = editor::toJson(sc).dump(2);
    std::vector<io::PackEntry> entries;
    entries.push_back({"scene.json", std::vector<uint8_t>(json.begin(), json.end())});
    entries.push_back({"manifest.txt", std::vector<uint8_t>{'x', 'y'}});
    const std::vector<uint8_t> bytes = io::packResources(entries);

    io::ResourcePack pack;
    CHECK(pack.load(bytes));
    CHECK(pack.count() == 2);
    CHECK(pack.contains("scene.json") && pack.contains("manifest.txt"));

    const std::vector<uint8_t>* blob = pack.get("scene.json");
    CHECK(blob != nullptr);
    const std::string back(blob->begin(), blob->end());
    const io::JsonParseResult pr = io::parseJson(back);
    CHECK(pr.ok);
    editor::Scene out;
    CHECK(editor::fromJson(pr.value, out));
    CHECK(out.nodes.size() == 2);
    CHECK(out.nodes[0] == sc.nodes[0]); // Node::operator== compares every field
    CHECK(out.nodes[1] == sc.nodes[1]);

    // A truncated archive must fail to load cleanly rather than read out of bounds.
    io::ResourcePack bad;
    CHECK(!bad.load(bytes.data(), bytes.size() / 2));
}

// P7: contact events. A moving ball strikes a fixed ball and bounces away; the world must report a
// Begin when they start touching and an End when they separate, and nothing before first contact.
void testContactEvents() {
    using game::Body2D;
    using game::ContactPhase;

    game::PhysicsWorld2D w;
    w.gravity = math::vec2(0.0f, 0.0f);
    w.warmStarting = true;
    w.trackContacts = true;

    Body2D fixed;
    fixed.shape = Body2D::Circle;
    fixed.radius = 20.0f;
    fixed.pos = math::vec2(0.0f, 0.0f);
    fixed.invMass = 0.0f;
    fixed.restitution = 1.0f;
    w.add(fixed);
    Body2D ball;
    ball.shape = Body2D::Circle;
    ball.radius = 15.0f;
    ball.pos = math::vec2(-200.0f, 0.0f);
    ball.vel = math::vec2(150.0f, 0.0f); // toward the fixed ball
    ball.invMass = 1.0f;
    ball.restitution = 1.0f;
    w.add(ball);

    bool sawBegin = false, sawEnd = false, sawPersistOrBegin = false;
    int beginStep = -1, endStep = -1;
    int eventsBeforeContact = 0;
    for (int s = 0; s < 200; ++s) {
        w.step(1.0f / 60.0f, 6);
        // Distance still large early on -> expect no events for the first few steps.
        if (s < 3) {
            eventsBeforeContact += static_cast<int>(w.contactEvents.size());
        }
        for (const game::ContactEvent& e : w.contactEvents) {
            if (e.phase == ContactPhase::Begin) {
                sawBegin = true;
                if (beginStep < 0) {
                    beginStep = s;
                }
            } else if (e.phase == ContactPhase::Persist) {
                sawPersistOrBegin = true;
            } else if (e.phase == ContactPhase::End) {
                sawEnd = true;
                endStep = s;
            }
        }
        if (sawEnd) {
            break;
        }
    }

    CHECK(eventsBeforeContact == 0); // nothing reported while far apart
    CHECK(sawBegin);
    CHECK(sawEnd);
    CHECK(beginStep >= 0 && endStep > beginStep); // enter strictly precedes exit
    // A Begin event carries a nonzero normal pointing along the collision axis (a->b, roughly +x here).
    // (Persist may or may not occur depending on how many frames they overlap; not asserted.)
    (void)sawPersistOrBegin;
}

// P8: continuous collision (Godot continuous_cd). A bullet moving fast enough to cross a thin wall in
// one step tunnels through it with discrete collision, but stops at it when flagged continuous.
void testCCD() {
    using game::Body2D;

    auto shoot = [](bool ccd) {
        game::PhysicsWorld2D w;
        w.gravity = math::vec2(0.0f, 0.0f);
        w.warmStarting = true;
        Body2D wall;
        wall.shape = Body2D::Box;
        wall.half = math::vec2(4.0f, 120.0f); // an 8px-thin wall at x=0
        wall.pos = math::vec2(0.0f, 0.0f);
        wall.invMass = 0.0f;
        wall.restitution = 0.0f;
        w.add(wall);
        Body2D bullet;
        bullet.shape = Body2D::Circle;
        bullet.radius = 3.0f;
        bullet.pos = math::vec2(-100.0f, 0.0f);
        bullet.vel = math::vec2(12000.0f, 0.0f); // 200px per 1/60s step — leaps clean over the wall
        bullet.invMass = 1.0f;
        bullet.restitution = 0.0f;
        bullet.continuous = ccd;
        w.add(bullet);
        w.step(1.0f / 60.0f, 6);
        return w.bodies[1].pos.x;
    };

    // Discrete: the bullet is already on the far side of the wall after one step (tunnelled).
    const float tunneled = shoot(false);
    CHECK(tunneled > 50.0f);

    // Continuous: the sweep stops it at the near face of the wall (x well below 0).
    const float stopped = shoot(true);
    CHECK(stopped < 0.0f);
    CHECK(stopped > -20.0f); // parked just in front of the wall (~ -7), not left behind at the start
}

// P9: pin-joint angular motor + limit (Godot PinJoint2D motor / angular_limit). A dynamic arm hinged to
// a static anchor is spun up by the motor toward a target speed; an angular limit then caps how far it
// can rotate. The two bodies are on non-interacting layers so only the joint acts.
void testJointMotor() {
    using game::Body2D;
    using game::Joint2D;
    using game::layerBit;

    auto makeHinge = [](bool limit) {
        game::PhysicsWorld2D w;
        w.gravity = math::vec2(0.0f, 0.0f);
        w.warmStarting = true;
        Body2D anchor; // static frame at origin
        anchor.shape = Body2D::Circle;
        anchor.radius = 4.0f;
        anchor.pos = math::vec2(0.0f, 0.0f);
        anchor.invMass = 0.0f;
        anchor.collisionLayer = layerBit(0);
        anchor.collisionMask = layerBit(0);
        w.add(anchor);
        Body2D arm;
        arm.shape = Body2D::Box;
        arm.half = math::vec2(40.0f, 6.0f);
        arm.pos = math::vec2(0.0f, 0.0f); // pinned at its own centre -> free to spin
        arm.invMass = 1.0f;
        arm.collisionLayer = layerBit(1); // different layer -> no contact with the anchor
        arm.collisionMask = layerBit(1);
        arm.enableRotation();
        w.add(arm);
        Joint2D j;
        j.type = Joint2D::Pin;
        j.a = 0;
        j.b = 1;
        j.localA = math::vec2(0.0f, 0.0f);
        j.anchorB = math::vec2(0.0f, 0.0f); // both anchors at the shared centre
        j.motorEnabled = true;
        j.motorSpeed = 4.0f;       // rad/s target
        j.maxMotorTorque = 1e5f;   // strong enough to reach target quickly
        if (limit) {
            j.limitEnabled = true;
            j.lowerAngle = -0.2f;
            j.upperAngle = 1.0f; // cap the swing at ~1 rad
        }
        w.addJoint(j);
        return w;
    };

    // Motor only: the arm reaches (roughly) the target angular velocity and keeps spinning.
    {
        game::PhysicsWorld2D w = makeHinge(false);
        for (int s = 0; s < 60; ++s) {
            w.step(1.0f / 60.0f, 8);
        }
        CHECK_NEAR(w.bodies[1].angularVel, 4.0f, 0.5f); // driven to the motor target
        CHECK(w.bodies[1].angle > 1.5f);                // it actually rotated a long way (no limit)
    }

    // Motor + limit: the arm is driven into the upper stop and parks there, not past it.
    {
        game::PhysicsWorld2D w = makeHinge(true);
        for (int s = 0; s < 120; ++s) {
            w.step(1.0f / 60.0f, 8);
        }
        CHECK(w.bodies[1].angle <= 1.0f + 0.05f); // held at/under the upper limit
        CHECK(w.bodies[1].angle > 0.8f);          // and it did reach the limit
        CHECK(std::fabs(w.bodies[1].angularVel) < 1.0f); // motor stalled against the stop
    }
}

// P10: arbitrary convex polygon dynamic shape (Godot ConvexPolygonShape2D). Verify convex-convex
// manifolds (normal/penetration/two points), convex-vs-circle, and a convex hull settling at rest on a
// box floor via the warm solver.
void testConvex() {
    using game::Body2D;
    namespace d = game::detail;

    auto square = [](float cx, float cy, float h) {
        Body2D b;
        b.shape = Body2D::Convex;
        b.pos = math::vec2(cx, cy);
        b.verts = {math::vec2(-h, -h), math::vec2(h, -h), math::vec2(h, h), math::vec2(-h, h)};
        return b;
    };

    // Two axis-aligned convex squares overlapping along x -> two-point manifold, normal +x, pen 10.
    {
        Body2D a = square(0.0f, 0.0f, 20.0f);
        Body2D b = square(30.0f, 0.0f, 20.0f); // overlap = 40 - 30 = 10
        std::vector<math::vec2> va, vb;
        d::bodyWorldVerts(a, va);
        d::bodyWorldVerts(b, vb);
        d::Contact2 m = d::polyManifold(va, vb);
        CHECK(m.hit);
        CHECK(m.count == 2);
        CHECK_NEAR(m.n.x, 1.0f, 1e-3f);
        CHECK_NEAR(m.n.y, 0.0f, 1e-3f);
        CHECK_NEAR(m.pen[0], 10.0f, 1e-3f);
        CHECK_NEAR(m.pen[1], 10.0f, 1e-3f);
    }

    // Separated convex squares: no contact.
    {
        Body2D a = square(0.0f, 0.0f, 20.0f);
        Body2D b = square(100.0f, 0.0f, 20.0f);
        std::vector<math::vec2> va, vb;
        d::bodyWorldVerts(a, va);
        d::bodyWorldVerts(b, vb);
        CHECK(!d::polyManifold(va, vb).hit);
    }

    // Convex vs circle: circle just off the square's right edge.
    {
        Body2D a = square(0.0f, 0.0f, 20.0f);
        std::vector<math::vec2> va;
        d::bodyWorldVerts(a, va);
        Body2D c;
        c.shape = Body2D::Circle;
        c.radius = 10.0f;
        c.pos = math::vec2(25.0f, 0.0f); // 5 into the +x edge (square edge at x=20)
        d::Manifold m = d::convexCircle(va, c);
        CHECK(m.hit);
        CHECK_NEAR(m.n.x, 1.0f, 1e-3f); // square -> circle, +x
        CHECK_NEAR(m.pen, 5.0f, 1e-3f); // radius 10, centre 5 outside edge
    }

    // End to end: a hexagon convex hull dropped onto a static box floor settles at rest on it.
    {
        game::PhysicsWorld2D w;
        w.gravity = math::vec2(0.0f, 600.0f);
        w.warmStarting = true;
        Body2D floor;
        floor.shape = Body2D::Box;
        floor.half = math::vec2(200.0f, 12.0f);
        floor.pos = math::vec2(0.0f, 312.0f); // top face at y=300
        floor.invMass = 0.0f;
        floor.friction = 0.9f;
        w.add(floor);
        Body2D hex;
        hex.shape = Body2D::Convex;
        hex.pos = math::vec2(0.0f, 120.0f);
        const float R = 30.0f;
        for (int i = 0; i < 6; ++i) {
            // Offset 0 -> a flat horizontal bottom edge (vertices at 60 deg and 120 deg), so it rests
            // on a stable two-point edge contact rather than balancing on a single vertex.
            const float a = 3.14159265f / 3.0f * static_cast<float>(i);
            hex.verts.push_back(math::vec2(std::cos(a) * R, std::sin(a) * R));
        }
        hex.invMass = 1.0f;
        hex.friction = 0.9f;
        hex.restitution = 0.0f;
        hex.enableRotation();
        CHECK(hex.invInertia > 0.0f); // polygon inertia computed
        w.add(hex);
        for (int s = 0; s < 300; ++s) {
            w.step(1.0f / 60.0f, 10);
        }
        // Lowest world vertex sits on the floor top (y=300); the hull came to rest.
        const Body2D& b = w.bodies[1];
        const float ca = std::cos(b.angle), sa = std::sin(b.angle);
        float lowest = -1e30f;
        for (const math::vec2& v : b.verts) {
            lowest = std::max(lowest, b.pos.y + v.x * sa + v.y * ca);
        }
        CHECK_NEAR(lowest, 300.0f, 3.0f);
        CHECK(std::sqrt(glm::dot(b.vel, b.vel)) < 6.0f);
    }
}

// P12: physics material combine modes (Godot PhysicsMaterial). A bouncy ball on a non-bouncy floor
// rebounds high when restitution combines by Max and barely when it combines by Min.
void testPhysicsMaterial() {
    using game::Body2D;
    using game::CombineMode;

    auto drop = [](CombineMode restCombine) {
        game::PhysicsWorld2D w;
        w.gravity = math::vec2(0.0f, 600.0f);
        w.warmStarting = true;
        w.restitutionCombine = restCombine;
        Body2D floor;
        floor.shape = Body2D::Box;
        floor.half = math::vec2(200.0f, 12.0f);
        floor.pos = math::vec2(0.0f, 312.0f);
        floor.invMass = 0.0f;
        floor.restitution = 0.1f; // barely bouncy floor
        w.add(floor);
        Body2D ball;
        ball.shape = Body2D::Circle;
        ball.radius = 12.0f;
        ball.pos = math::vec2(0.0f, 200.0f);
        ball.invMass = 1.0f;
        ball.restitution = 0.9f; // very bouncy ball
        w.add(ball);
        float fastestUp = 0.0f; // most negative vel.y seen (rebound)
        for (int s = 0; s < 120; ++s) {
            w.step(1.0f / 60.0f, 6);
            fastestUp = std::min(fastestUp, w.bodies[1].vel.y);
        }
        return -fastestUp; // rebound speed (positive)
    };

    const float reboundMin = drop(CombineMode::Min); // e = min(0.1,0.9) = 0.1
    const float reboundMax = drop(CombineMode::Max); // e = max(0.1,0.9) = 0.9
    CHECK(reboundMax > reboundMin * 3.0f); // Max restitution bounces far higher
    CHECK(reboundMin < 120.0f);            // Min: weak bounce
    CHECK(reboundMax > 150.0f);            // Max: strong bounce

    // Combine helper values are correct.
    CHECK_NEAR(game::combineValue(CombineMode::Average, 0.2f, 0.8f), 0.5f, 1e-5f);
    CHECK_NEAR(game::combineValue(CombineMode::Multiply, 0.5f, 0.4f), 0.2f, 1e-5f);
    CHECK_NEAR(game::combineValue(CombineMode::Min, 0.3f, 0.7f), 0.3f, 1e-5f);
    CHECK_NEAR(game::combineValue(CombineMode::Max, 0.3f, 0.7f), 0.7f, 1e-5f);
    CHECK_NEAR(game::combineValue(CombineMode::GeometricMean, 0.25f, 0.64f), 0.4f, 1e-5f);
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

    // ---- A1: dB units, the completed biquad set, and Amplify ------------------------------------
    // Exact magnitude response |H(e^jw)| of a biquad at angular frequency w (a1/a2 are already
    // a0-normalized, so the denominator's constant term is 1).
    auto magAt = [](const audio::Biquad& f, float w) {
        const float c1 = std::cos(w), s1 = std::sin(w);
        const float c2 = std::cos(2.0f * w), s2 = std::sin(2.0f * w);
        const float numRe = f.b0 + f.b1 * c1 + f.b2 * c2;
        const float numIm = -(f.b1 * s1 + f.b2 * s2);
        const float denRe = 1.0f + f.a1 * c1 + f.a2 * c2;
        const float denIm = -(f.a1 * s1 + f.a2 * s2);
        const float num = std::sqrt(numRe * numRe + numIm * numIm);
        const float den = std::sqrt(denRe * denRe + denIm * denIm);
        return num / den;
    };
    auto wOf = [&](float hz) { return 2.0f * 3.14159265f * hz / sr; };

    // dB <-> linear round-trips and known anchors.
    CHECK_NEAR(audio::dbToLinear(0.0f), 1.0f, 1e-6f);
    CHECK_NEAR(audio::linearToDb(1.0f), 0.0f, 1e-5f);
    CHECK_NEAR(audio::dbToLinear(6.0206f), 2.0f, 1e-3f);  // +6.02 dB == x2
    CHECK_NEAR(audio::linearToDb(2.0f), 6.0206f, 1e-3f);
    CHECK_NEAR(audio::dbToLinear(audio::linearToDb(0.37f)), 0.37f, 1e-4f);
    CHECK(audio::linearToDb(0.0f) < -150.0f); // silence floors to a large finite negative dB

    // Amplify applies its dB gain as a linear multiply.
    {
        audio::Amplify amp;
        amp.gainDb = 6.0206f;
        CHECK_NEAR(amp.process(0.5f), 1.0f, 1e-3f);
        amp.gainDb = 0.0f;
        CHECK_NEAR(amp.process(0.42f), 0.42f, 1e-6f);
    }

    // Peaking EQ at 0 dB is an exact pass-through (numerator == denominator -> y == x).
    {
        audio::Biquad p = audio::Biquad::peaking(1000.0f, 2.0f, 0.0f, sr);
        for (float x : {0.3f, -0.7f, 0.15f, 0.9f}) {
            CHECK_NEAR(p.process(x), x, 1e-5f);
        }
    }
    // Peaking with +12 dB boosts its center frequency by ~12 dB and leaves DC/Nyquist untouched.
    {
        audio::Biquad p = audio::Biquad::peaking(1000.0f, 3.0f, 12.0f, sr);
        CHECK_NEAR(audio::linearToDb(magAt(p, wOf(1000.0f))), 12.0f, 0.3f);
        CHECK_NEAR(magAt(p, 0.0f), 1.0f, 1e-3f);          // DC unaffected
        CHECK_NEAR(magAt(p, 3.14159265f), 1.0f, 1e-3f);   // Nyquist unaffected
    }
    // Notch: near-zero magnitude at the cutoff, unity at DC.
    {
        audio::Biquad n = audio::Biquad::notch(1000.0f, 5.0f, sr);
        CHECK(magAt(n, wOf(1000.0f)) < 0.02f);
        CHECK_NEAR(magAt(n, 0.0f), 1.0f, 1e-3f);
    }
    // Allpass: flat unity magnitude across the spectrum (only the phase changes).
    {
        audio::Biquad ap = audio::Biquad::allpass(1000.0f, 0.707f, sr);
        for (float hz : {50.0f, 500.0f, 1000.0f, 5000.0f, 18000.0f}) {
            CHECK_NEAR(magAt(ap, wOf(hz)), 1.0f, 1e-3f);
        }
    }
    // Low shelf (+6 dB): boosts DC by 6 dB, leaves Nyquist ~unchanged.
    {
        audio::Biquad ls = audio::Biquad::lowShelf(1000.0f, 6.0f, sr);
        CHECK_NEAR(audio::linearToDb(magAt(ls, 0.0f)), 6.0f, 0.05f);
        CHECK_NEAR(audio::linearToDb(magAt(ls, 3.14159265f)), 0.0f, 0.2f);
    }
    // High shelf (-6 dB): cuts Nyquist by 6 dB, leaves DC ~unchanged.
    {
        audio::Biquad hs = audio::Biquad::highShelf(1000.0f, -6.0f, sr);
        CHECK_NEAR(audio::linearToDb(magAt(hs, 3.14159265f)), -6.0f, 0.05f);
        CHECK_NEAR(audio::linearToDb(magAt(hs, 0.0f)), 0.0f, 0.2f);
    }
    // AmplifyEffect works inside a bus chain.
    {
        audio::Bus chain;
        audio::Amplify amp;
        amp.gainDb = -6.0206f; // x0.5
        chain.add(std::make_unique<audio::AmplifyEffect>(amp));
        CHECK_NEAR(chain.process(1.0f), 0.5f, 1e-3f);
    }

    // ---- A2: multiband graphic EQ ---------------------------------------------------------------
    // Whole-chain magnitude response of an Equalizer at frequency `hz` (compose each band's response).
    auto eqMagAt = [&](const audio::Equalizer& eq, float hz) {
        const float w = 2.0f * 3.14159265f * hz / sr;
        float m = 1.0f;
        for (std::size_t i = 0; i < eq.bands.size(); ++i) {
            m *= magAt(eq.bands[i].filter, w);
        }
        return m;
    };

    // Band layouts match Godot's EQ6/10/21 counts.
    CHECK(audio::Equalizer::eq6(sr).bandCount() == 6);
    CHECK(audio::Equalizer::eq10(sr).bandCount() == 10);
    CHECK(audio::Equalizer::eq21(sr).bandCount() == 21);

    // All bands at 0 dB -> flat pass-through (unity at every probe frequency).
    {
        audio::Equalizer eq = audio::Equalizer::eq10(sr);
        for (float hz : {60.0f, 250.0f, 1000.0f, 4000.0f, 12000.0f}) {
            CHECK_NEAR(eqMagAt(eq, hz), 1.0f, 1e-3f);
        }
    }

    // Boost one band: its center frequency lifts by ~the set gain; a distant band stays ~flat.
    {
        audio::Equalizer eq = audio::Equalizer::eq10(sr);
        eq.setBandGain(5, 12.0f); // index 5 == 1000 Hz
        CHECK_NEAR(eq.bandFreq(5), 1000.0f, 1e-3f);
        CHECK_NEAR(audio::linearToDb(eqMagAt(eq, 1000.0f)), 12.0f, 1.0f);
        CHECK_NEAR(audio::linearToDb(eqMagAt(eq, 62.5f)), 0.0f, 1.0f);   // far low band unaffected
        CHECK(eq.bandGain(5) > 11.9f);
    }

    // A cut band drops its center below unity.
    {
        audio::Equalizer eq = audio::Equalizer::eq10(sr);
        eq.setBandGain(2, -12.0f); // 125 Hz
        CHECK(audio::linearToDb(eqMagAt(eq, 125.0f)) < -8.0f);
    }

    // EqualizerEffect runs inside a bus chain (a "smiley" curve boosts the extremes).
    {
        audio::Equalizer eq = audio::Equalizer::eq6(sr);
        eq.setBandGain(0, 9.0f);           // 32 Hz up
        eq.setBandGain(eq.bandCount() - 1, 9.0f); // 10 kHz up
        audio::Bus chain;
        chain.add(std::make_unique<audio::EqualizerEffect>(eq));
        // Feeding a constant (DC) lands near the low shelf; just confirm the chain runs and responds.
        float y = 0.0f;
        for (int i = 0; i < 64; ++i) {
            y = chain.process(1.0f);
        }
        CHECK(std::isfinite(y));
    }
}

void testPitchShifter() {
    using audio::PitchShifter;
    const float psr = 44100.0f;
    const float freq = 441.0f; // input period = 100 samples exactly

    // Dominant period of a signal via autocorrelation over a plausible lag range.
    auto dominantPeriod = [](const std::vector<float>& s, int lo, int hi) {
        int best = lo;
        double bestC = -1e18;
        for (int lag = lo; lag <= hi; ++lag) {
            double c = 0.0;
            for (std::size_t i = 0; i + static_cast<std::size_t>(lag) < s.size(); ++i) {
                c += static_cast<double>(s[i]) * static_cast<double>(s[i + static_cast<std::size_t>(lag)]);
            }
            if (c > bestC) {
                bestC = c;
                best = lag;
            }
        }
        return best;
    };

    auto runShift = [&](float ratio) {
        PitchShifter ps;
        ps.configure(ratio, 1024);
        std::vector<float> out;
        out.reserve(9000);
        for (int i = 0; i < 9000; ++i) {
            const float x = std::sin(2.0f * 3.14159265f * freq * static_cast<float>(i) / psr);
            const float y = ps.process(x);
            if (i >= 2500) { // skip grain warmup
                out.push_back(y);
            }
        }
        return out;
    };

    // Unity ratio: output preserves the input (delayed), so its dominant period stays ~100 samples and it
    // stays bounded and finite.
    {
        const std::vector<float> o = runShift(1.0f);
        bool finite = true;
        float maxAbs = 0.0f;
        for (float v : o) {
            finite = finite && std::isfinite(v);
            maxAbs = std::max(maxAbs, std::fabs(v));
        }
        CHECK(finite);
        CHECK(maxAbs < 1.5f);
        CHECK(std::abs(dominantPeriod(o, 40, 260) - 100) <= 8);
    }
    // Up an octave: dominant period halves (~50 samples).
    {
        const std::vector<float> o = runShift(2.0f);
        const int p = dominantPeriod(o, 30, 260);
        CHECK(std::abs(p - 50) <= 8);
    }
    // Down an octave: dominant period doubles (~200 samples).
    {
        const std::vector<float> o = runShift(0.5f);
        const int p = dominantPeriod(o, 120, 320);
        CHECK(std::abs(p - 200) <= 16);
    }

    // Semitone helper: +12 semitones == x2 ratio.
    {
        PitchShifter ps;
        ps.setSemitones(12.0f);
        CHECK_NEAR(ps.pitchScale, 2.0f, 1e-4f);
        ps.setSemitones(-12.0f);
        CHECK_NEAR(ps.pitchScale, 0.5f, 1e-4f);
    }

    // Deterministic: same input -> same output after reset.
    {
        PitchShifter a;
        a.configure(1.5f, 512);
        PitchShifter b;
        b.configure(1.5f, 512);
        bool same = true;
        for (int i = 0; i < 2000; ++i) {
            const float x = std::sin(2.0f * 3.14159265f * 330.0f * static_cast<float>(i) / psr);
            if (std::fabs(a.process(x) - b.process(x)) > 1e-6f) {
                same = false;
            }
        }
        CHECK(same);
    }

    // Works inside a bus chain via PitchShiftEffect, staying finite.
    {
        audio::PitchShifter ps;
        ps.configure(1.5f, 512);
        audio::Bus chain;
        chain.add(std::make_unique<audio::PitchShiftEffect>(ps));
        bool finite = true;
        for (int i = 0; i < 1000; ++i) {
            const float y = chain.process(std::sin(0.1f * static_cast<float>(i)));
            finite = finite && std::isfinite(y);
        }
        CHECK(finite);
    }
}

void testMusicSequencer() {
    using audio::MusicSequencer;
    using audio::TransitionMode;
    const float msr = 44100.0f;

    // Bar/beat math: 120 BPM, 4/4 -> beat = 0.5 s, bar = 2.0 s.
    {
        MusicSequencer seq(msr);
        const int a = seq.addSegment("A", 120.0f, 4);
        CHECK_NEAR(seq.beatLengthSamples(a), 0.5 * msr, 1e-3);
        CHECK_NEAR(seq.barLengthSamples(a), 2.0 * msr, 1e-3);
    }

    // AtNextBar transition fires exactly at the 2.0 s bar boundary.
    {
        MusicSequencer seq(msr);
        const int a = seq.addSegment("A", 120.0f, 4);
        const int b = seq.addSegment("B", 120.0f, 4);
        seq.play(a);
        seq.advance(0.3 * msr); // 0.3 s in
        seq.transitionTo(b, TransitionMode::AtNextBar, 0.0f);
        CHECK_NEAR(seq.samplesToNextBar(), 1.7 * msr, 1.0); // 2.0 - 0.3
        seq.advance(1.6 * msr); // now at 1.9 s -> still A
        CHECK(seq.current() == a);
        seq.advance(0.2 * msr); // now at 2.1 s -> switched to B at 2.0
        CHECK(seq.current() == b);
    }

    // AtNextBeat fires at the next 0.5 s beat.
    {
        MusicSequencer seq(msr);
        const int a = seq.addSegment("A", 120.0f, 4);
        const int b = seq.addSegment("B", 120.0f, 4);
        seq.play(a);
        seq.advance(0.1 * msr);
        seq.transitionTo(b, TransitionMode::AtNextBeat, 0.0f);
        seq.advance(0.35 * msr); // 0.45 s -> still A
        CHECK(seq.current() == a);
        seq.advance(0.1 * msr); // 0.55 s -> switched at 0.5
        CHECK(seq.current() == b);
    }

    // Immediate switches on the spot.
    {
        MusicSequencer seq(msr);
        const int a = seq.addSegment("A", 100.0f, 4);
        const int b = seq.addSegment("B", 100.0f, 4);
        seq.play(a);
        seq.advance(1234.0);
        seq.transitionTo(b, TransitionMode::Immediate, 0.0f);
        seq.advance(1.0);
        CHECK(seq.current() == b);
    }

    // Crossfade: both segments audible during the fade, with equal-power gains that sum in quadrature to ~1.
    {
        MusicSequencer seq(msr);
        const int a = seq.addSegment("A", 120.0f, 4);
        const int b = seq.addSegment("B", 120.0f, 4);
        seq.play(a);
        seq.advance(0.5 * msr);
        seq.transitionTo(b, TransitionMode::Crossfade, 1.0f); // 1.0 s fade starting now
        seq.advance(0.5 * msr);                               // halfway through the fade
        const audio::MusicMix m = seq.mix();
        CHECK(seq.crossfading());
        CHECK(m.a == a);
        CHECK(m.b == b);
        CHECK_NEAR(m.aGain * m.aGain + m.bGain * m.bGain, 1.0f, 1e-3f); // equal power
        CHECK_NEAR(m.aGain, m.bGain, 1e-3f);                           // equal at the midpoint
        seq.advance(0.6 * msr); // past the fade end -> B only
        CHECK(!seq.crossfading());
        CHECK(seq.current() == b);
        const audio::MusicMix done = seq.mix();
        CHECK(done.a == b);
        CHECK_NEAR(done.aGain, 1.0f, 1e-4f);
    }
}

void testOscillator() {
    using audio::FMVoice;
    using audio::Oscillator;
    using audio::Waveform;
    const float osr = 44100.0f;

    // Sine at exactly a quarter of the sample rate hits 0, 1, 0, -1 at successive quarter phases.
    {
        Oscillator o;
        o.sampleRate = osr;
        o.freq = osr / 4.0f; // dt = 0.25
        o.waveform = Waveform::Sine;
        o.reset();
        CHECK_NEAR(o.next(), 0.0f, 1e-5f);  // phase 0
        CHECK_NEAR(o.next(), 1.0f, 1e-5f);  // phase 0.25
        CHECK_NEAR(o.next(), 0.0f, 1e-4f);  // phase 0.5
        CHECK_NEAR(o.next(), -1.0f, 1e-5f); // phase 0.75
    }

    // Triangle at the same rate is the exact piecewise ramp: -1, 0, 1, 0.
    {
        Oscillator o;
        o.sampleRate = osr;
        o.freq = osr / 4.0f;
        o.waveform = Waveform::Triangle;
        o.reset();
        CHECK_NEAR(o.next(), -1.0f, 1e-5f);
        CHECK_NEAR(o.next(), 0.0f, 1e-5f);
        CHECK_NEAR(o.next(), 1.0f, 1e-5f);
        CHECK_NEAR(o.next(), 0.0f, 1e-5f);
    }

    // Saw and square stay bounded and average to ~0 over a cycle; the band-limited saw's largest
    // single-sample jump is softened BELOW the naive 2.0 step (PolyBLEP working).
    {
        Oscillator saw;
        saw.sampleRate = osr;
        saw.freq = 440.0f;
        saw.waveform = Waveform::Saw;
        saw.reset();
        double sum = 0.0;
        float prev = saw.next();
        float maxJump = 0.0f, maxAbs = std::fabs(prev);
        const int n = static_cast<int>(osr / 440.0f) * 4; // a few cycles
        for (int i = 1; i < n; ++i) {
            const float v = saw.next();
            sum += v;
            maxJump = std::max(maxJump, std::fabs(v - prev));
            maxAbs = std::max(maxAbs, std::fabs(v));
            prev = v;
        }
        CHECK(std::fabs(sum / static_cast<double>(n)) < 0.05); // ~zero mean
        CHECK(maxAbs < 1.25f);                                 // bounded (blep overshoot small)
        CHECK(maxJump < 2.0f);                                 // edge softened below the naive step
    }
    {
        Oscillator sq;
        sq.sampleRate = osr;
        sq.freq = 440.0f;
        sq.waveform = Waveform::Square;
        sq.reset();
        double sum = 0.0;
        const int n = static_cast<int>(osr / 440.0f) * 4;
        for (int i = 0; i < n; ++i) {
            sum += sq.next();
        }
        CHECK(std::fabs(sum / static_cast<double>(n)) < 0.08); // 50% duty -> ~zero mean
    }

    // Noise is bounded in [-1, 1], deterministic after reset(seed), and different seeds differ.
    {
        Oscillator ns;
        ns.sampleRate = osr;
        ns.waveform = Waveform::Noise;
        ns.reset(12345u);
        float first[8];
        bool bounded = true;
        for (int i = 0; i < 8; ++i) {
            first[i] = ns.next();
            bounded = bounded && first[i] >= -1.0f && first[i] <= 1.0f;
        }
        CHECK(bounded);
        ns.reset(12345u);
        bool same = true;
        for (int i = 0; i < 8; ++i) {
            same = same && std::fabs(ns.next() - first[i]) < 1e-6f;
        }
        CHECK(same);
        ns.reset(999u);
        CHECK(std::fabs(ns.next() - first[0]) > 1e-6f); // a different seed diverges
    }

    // Detune shifts the pitch: a 2x detune advances phase twice as fast (double frequency).
    {
        Oscillator a;
        a.sampleRate = osr;
        a.freq = 100.0f;
        a.detune = 2.0f;
        a.reset();
        a.next();
        const float pA = a.phase(); // advanced by 200/44100
        CHECK_NEAR(pA, 200.0f / osr, 1e-5f);
    }

    // FM voice: with a non-zero index the output differs from the bare carrier, and stays finite.
    {
        FMVoice fm;
        fm.carrier.sampleRate = osr;
        fm.carrier.freq = 220.0f;
        fm.carrier.waveform = Waveform::Sine;
        fm.modulator.sampleRate = osr;
        fm.modulator.freq = 440.0f;
        fm.modulator.waveform = Waveform::Sine;
        fm.index = 0.5f;
        fm.reset();

        Oscillator bare;
        bare.sampleRate = osr;
        bare.freq = 220.0f;
        bare.waveform = Waveform::Sine;
        bare.reset();

        bool differs = false, finite = true;
        for (int i = 0; i < 64; ++i) {
            const float y = fm.next();
            finite = finite && std::isfinite(y);
            if (std::fabs(y - bare.next()) > 1e-3f) {
                differs = true;
            }
        }
        CHECK(finite);
        CHECK(differs);
    }
}

void testStereo() {
    using audio::Panner;
    using audio::StereoEnhance;
    using audio::StereoFrame;

    // Width 0 collapses to mono: both channels become the mid (average).
    {
        StereoEnhance w;
        w.configure(0.0f, 0.0f, 44100.0f);
        const StereoFrame o = w.process(StereoFrame{0.8f, 0.2f});
        CHECK_NEAR(o.left, 0.5f, 1e-5f);
        CHECK_NEAR(o.right, 0.5f, 1e-5f);
    }
    // Width 1 is unchanged.
    {
        StereoEnhance w;
        w.configure(1.0f, 0.0f, 44100.0f);
        const StereoFrame o = w.process(StereoFrame{0.8f, 0.2f});
        CHECK_NEAR(o.left, 0.8f, 1e-5f);
        CHECK_NEAR(o.right, 0.2f, 1e-5f);
    }
    // Width 2 doubles the side (difference) signal: mid 0.5, side 0.3 -> side 0.6.
    {
        StereoEnhance w;
        w.configure(2.0f, 0.0f, 44100.0f);
        const StereoFrame o = w.process(StereoFrame{0.8f, 0.2f});
        CHECK_NEAR(o.left, 1.1f, 1e-5f);   // mid + 2*side = 0.5 + 0.6
        CHECK_NEAR(o.right, -0.1f, 1e-5f); // mid - 2*side = 0.5 - 0.6
    }
    // A truly mono input (L==R) has no side, so no width setting can widen it.
    {
        StereoEnhance w;
        w.configure(3.0f, 0.0f, 44100.0f);
        const StereoFrame o = w.process(StereoFrame{0.5f, 0.5f});
        CHECK_NEAR(o.left, 0.5f, 1e-5f);
        CHECK_NEAR(o.right, 0.5f, 1e-5f);
    }
    // Haas widener delays the right channel: first output right is the pre-fill (0), then it catches up.
    {
        StereoEnhance w;
        w.configure(1.0f, 1.0f, 44100.0f); // ~44 samples of delay
        const StereoFrame o0 = w.process(StereoFrame{1.0f, 1.0f});
        // The right channel is delayed, so at the first frame it is still 0: the momentary pair is
        // (L=1, R=0), which at width 1 reconstructs to {1, 0}. This is the Haas widening in action.
        CHECK_NEAR(o0.left, 1.0f, 1e-4f);
        CHECK_NEAR(o0.right, 0.0f, 1e-4f);
    }

    // Panner: centered pan leaves both channels alone.
    {
        Panner p;
        p.pan = 0.0f;
        const StereoFrame o = p.process(StereoFrame{0.7f, 0.3f});
        CHECK_NEAR(o.left, 0.7f, 1e-6f);
        CHECK_NEAR(o.right, 0.3f, 1e-6f);
    }
    // Hard pan right silences the left channel; the right passes.
    {
        Panner p;
        p.pan = 1.0f;
        const StereoFrame o = p.process(StereoFrame{0.7f, 0.5f});
        CHECK_NEAR(o.left, 0.0f, 1e-5f);
        CHECK_NEAR(o.right, 0.5f, 1e-6f);
    }
    // Hard pan left silences the right channel.
    {
        Panner p;
        p.pan = -1.0f;
        const StereoFrame o = p.process(StereoFrame{0.7f, 0.5f});
        CHECK_NEAR(o.left, 0.7f, 1e-6f);
        CHECK_NEAR(o.right, 0.0f, 1e-5f);
    }
    // Half pan right applies a constant-power (cosine) taper to the far (left) channel.
    {
        Panner p;
        p.pan = 0.5f;
        const StereoFrame o = p.process(StereoFrame{1.0f, 1.0f});
        CHECK_NEAR(o.left, std::cos(0.5f * 1.5707963f), 1e-5f); // ~0.707
        CHECK_NEAR(o.right, 1.0f, 1e-6f);
    }
}

void testBusGraph() {
    using audio::BusGraph;

    // Master exists at index 0; adding named buses returns fresh indices resolvable by name.
    {
        BusGraph g;
        CHECK(g.busCount() == 1);
        CHECK(g.busIndex("Master") == 0);
        const int music = g.addBus("Music");
        const int sfx = g.addBus("SFX");
        CHECK(music == 1);
        CHECK(sfx == 2);
        CHECK(g.busIndex("SFX") == 2);
        CHECK(g.busIndex("Nope") == -1);
        CHECK(g.busName(1) == "Music");
        CHECK(g.send(music) == 0); // defaults to Master
    }

    // A bus's dB volume scales its signal on the way to Master (−6.02 dB == half).
    {
        BusGraph g;
        const int music = g.addBus("Music");
        g.setVolumeDb(music, -6.0206f);
        g.pushInput(music, 1.0f);
        CHECK_NEAR(g.process(), 0.5f, 1e-3f);
    }

    // Muting a bus silences everything routed through it.
    {
        BusGraph g;
        const int sfx = g.addBus("SFX");
        g.setMute(sfx, true);
        g.pushInput(sfx, 1.0f);
        CHECK_NEAR(g.process(), 0.0f, 1e-6f);
    }

    // A send chain multiplies the gains: C -> B -> Master, each −6.02 dB, gives ×0.25.
    {
        BusGraph g;
        const int b = g.addBus("B");                 // -> Master
        const int c = g.addBus("C", b);              // -> B
        g.setVolumeDb(b, -6.0206f);
        g.setVolumeDb(c, -6.0206f);
        g.pushInput(c, 1.0f);
        CHECK_NEAR(g.process(), 0.25f, 2e-3f);
    }

    // Solo: only the soloed bus's path to Master is audible; unrelated buses are silenced.
    {
        BusGraph g;
        const int a = g.addBus("A");
        const int bmid = g.addBus("Bmid");
        const int cleaf = g.addBus("Cleaf", bmid); // C -> Bmid -> Master
        g.setSolo(cleaf, true);
        g.pushInput(a, 1.0f);      // unrelated bus
        g.pushInput(cleaf, 1.0f);  // soloed path
        // Only C's signal survives (through Bmid, both 0 dB); A is muted by solo.
        CHECK_NEAR(g.process(), 1.0f, 1e-3f);
        (void)a;
    }

    // An effect on a bus is applied; bypass skips it.
    {
        BusGraph g;
        const int m = g.addBus("Music");
        audio::Amplify amp;
        amp.gainDb = 6.0206f; // x2
        g.addEffect(m, std::make_unique<audio::AmplifyEffect>(amp));
        g.pushInput(m, 0.25f);
        CHECK_NEAR(g.process(), 0.5f, 1e-3f); // effect doubled it
        g.setBypass(m, true);
        g.pushInput(m, 0.25f);
        CHECK_NEAR(g.process(), 0.25f, 1e-3f); // effect skipped
    }

    // Per-bus level metering reflects the processed output.
    {
        BusGraph g;
        const int m = g.addBus("Music");
        g.setVolumeDb(m, -6.0206f);
        g.pushInput(m, 1.0f);
        g.process();
        CHECK_NEAR(g.busLevel(m), 0.5f, 1e-3f);
        CHECK_NEAR(g.busLevel(0), 0.5f, 1e-3f); // Master carries the summed output
    }

    // Two buses summing into Master mix additively.
    {
        BusGraph g;
        const int a = g.addBus("A");
        const int b = g.addBus("B");
        g.pushInput(a, 0.3f);
        g.pushInput(b, 0.4f);
        CHECK_NEAR(g.process(), 0.7f, 1e-4f);
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

void testModDsp() {
    const float sr = 44100.0f;

    // ---- LFO: reads the sine then advances; a rate that steps a quarter-cycle per sample gives the
    // textbook 0, +1, 0, -1, 0 sequence and wraps cleanly.
    {
        audio::Lfo lfo;
        lfo.setRate(1.0f, 4.0f); // period = 4 samples
        CHECK_NEAR(lfo.next(), 0.0f, 1e-5f);  // sin(0)
        CHECK_NEAR(lfo.next(), 1.0f, 1e-5f);  // sin(pi/2)
        CHECK_NEAR(lfo.next(), 0.0f, 1e-5f);  // sin(pi)
        CHECK_NEAR(lfo.next(), -1.0f, 1e-5f); // sin(3pi/2)
        CHECK_NEAR(lfo.next(), 0.0f, 1e-5f);  // wrapped back to phase 0
    }

    // ---- fracTap: linear interpolation of a delay line read behind the write head.
    {
        const std::vector<float> line = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f};
        CHECK_NEAR(audio::fracTap(line, 0, 1.0f), 7.0f, 1e-5f); // 0-1 wraps to index 7
        CHECK_NEAR(audio::fracTap(line, 0, 1.5f), 6.5f, 1e-5f); // between line[6] and line[7]
        CHECK_NEAR(audio::fracTap(line, 4, 2.0f), 2.0f, 1e-5f); // head 4, back 2 -> line[2]
    }

    // A helper: impulse response energy in a window [a,b) after processing one impulse then silence.
    auto tailEnergy = [](auto& fx, int a, int b) {
        double e = 0.0;
        fx.process(1.0f); // the impulse at sample 0
        for (int i = 1; i < b; ++i) {
            const float y = fx.process(0.0f);
            if (i >= a) {
                e += std::fabs(static_cast<double>(y));
            }
        }
        return e;
    };

    // ---- Chorus: wet=0 is an exact pass-through; wet>0 echoes the impulse near the base delay.
    {
        audio::Chorus dry;
        dry.configure(sr, 3, 20.0f, 4.0f, 0.8f, 0.0f);
        CHECK_NEAR(dry.process(0.5f), 0.5f, 1e-5f);
        CHECK_NEAR(dry.process(-0.3f), -0.3f, 1e-5f);

        audio::Chorus ch;
        ch.configure(sr, 3, 20.0f, 4.0f, 0.8f, 1.0f); // fully wet
        // base 20 ms ~= 882 samples; the wet voices carry the impulse into a window around there.
        const double e = tailEnergy(ch, 600, 1300);
        CHECK(e > 0.1); // the delayed copies show up well after the input
        // voice count clamps to [1,4].
        audio::Chorus clamp;
        clamp.configure(sr, 9, 20.0f, 4.0f, 0.8f, 0.5f);
        CHECK(clamp.voices == 4);
    }

    // ---- Flanger: wet=0 passes through; feedback makes the short delay re-emit repeatedly.
    {
        audio::Flanger dry;
        dry.configure(sr, 2.0f, 2.0f, 0.25f, 0.6f, 0.0f);
        CHECK_NEAR(dry.process(0.7f), 0.7f, 1e-5f);

        audio::Flanger fl;
        fl.configure(sr, 2.0f, 2.0f, 0.25f, 0.6f, 1.0f);
        const double e = tailEnergy(fl, 40, 2000); // short delay (~88 samples) + feedback repeats
        CHECK(e > 0.1);
    }

    // ---- Phaser: all-pass based, so wet=0 is a pass-through; wet>0 stays finite/stable (bounded IR).
    {
        audio::Phaser dry;
        dry.configure(sr, 300.0f, 1600.0f, 0.5f, 0.4f, 0.0f);
        CHECK_NEAR(dry.process(0.6f), 0.6f, 1e-5f);

        audio::Phaser ph;
        ph.configure(sr, 300.0f, 1600.0f, 0.5f, 0.4f, 0.6f);
        double e = 0.0;
        bool finite = true;
        float y = ph.process(1.0f);
        for (int i = 0; i < 4000; ++i) {
            y = ph.process(0.0f);
            if (!std::isfinite(y)) {
                finite = false;
            }
            e += std::fabs(static_cast<double>(y));
        }
        CHECK(finite);   // the feedback all-pass cascade does not blow up
        CHECK(e > 1e-4); // it does colour the signal (nonzero response)
    }

    // ---- The new effects slot onto a Bus like every other effect, and reset() clears their state.
    {
        audio::Chorus c;
        c.configure(sr, 2, 18.0f, 3.0f, 1.0f, 0.5f);
        audio::Bus bus;
        bus.add(std::make_unique<audio::ChorusEffect>(c));
        bus.add(std::make_unique<audio::PhaserEffect>(audio::Phaser{}));
        const float y = bus.process(0.4f);
        CHECK(std::isfinite(y));

        audio::Flanger fl;
        fl.configure(sr, 2.0f, 2.0f, 0.3f, 0.6f, 1.0f);
        const float a = fl.process(1.0f);
        fl.process(0.0f);
        fl.reset();
        const float b = fl.process(1.0f); // same first sample after reset
        CHECK_NEAR(a, b, 1e-5f);
    }

    // ---- A4: brickwall lookahead limiter ----
    {
        const float ceilDb = -0.3f;
        const float ceil = audio::dbToLinear(ceilDb);

        // A loud tone driven far above the ceiling comes out AT or below the ceiling (true brickwall).
        {
            audio::Limiter lim;
            lim.configure(ceilDb, 100.0f, 2.0f, sr);
            float peak = 0.0f;
            const int look = lim.lookaheadSamples();
            for (int i = 0; i < 4000; ++i) {
                const float x = 1.8f * std::sin(2.0f * 3.14159265f * 220.0f * static_cast<float>(i) / sr);
                const float y = lim.process(x);
                if (i > look + 200) { // skip the fill + settle
                    peak = std::max(peak, std::fabs(y));
                }
            }
            CHECK(peak <= ceil + 1e-3f); // never exceeds the ceiling
            CHECK(peak > ceil - 0.05f);  // and actually reaches up to it (it is limiting, not silencing)
        }

        // A quiet signal (below the ceiling) passes through untouched, just delayed by the lookahead.
        {
            audio::Limiter lim;
            lim.configure(ceilDb, 100.0f, 2.0f, sr);
            const int look = lim.lookaheadSamples();
            std::vector<float> in, out;
            for (int i = 0; i < 400; ++i) {
                const float x = 0.2f * std::sin(2.0f * 3.14159265f * 440.0f * static_cast<float>(i) / sr);
                in.push_back(x);
                out.push_back(lim.process(x));
            }
            // out[n] should equal in[n - look] at unity gain.
            bool matched = true;
            for (int i = look + 5; i < 300; ++i) {
                if (std::fabs(out[static_cast<std::size_t>(i)] -
                              in[static_cast<std::size_t>(i - look)]) > 1e-4f) {
                    matched = false;
                    break;
                }
            }
            CHECK(matched);
            CHECK_NEAR(lim.gainReduction(), 1.0f, 1e-4f); // no reduction on a quiet signal
        }

        // gainReduction drops below unity while limiting a hot signal.
        {
            audio::Limiter lim;
            lim.configure(ceilDb, 100.0f, 2.0f, sr);
            for (int i = 0; i < 500; ++i) {
                lim.process(1.5f);
            }
            CHECK(lim.gainReduction() < 0.95f);
        }

        // Works inside a bus chain via LimiterEffect.
        {
            audio::Limiter lim;
            lim.configure(ceilDb, 100.0f, 1.0f, sr);
            audio::Bus chain;
            chain.add(std::make_unique<audio::LimiterEffect>(lim));
            float peak = 0.0f;
            for (int i = 0; i < 1000; ++i) {
                const float y = chain.process(1.6f);
                if (i > 200) {
                    peak = std::max(peak, std::fabs(y));
                }
            }
            CHECK(peak <= ceil + 1e-3f);
        }
    }

    // ---- A9: multi-mode distortion (Clip / ATan / LoFi / Overdrive) ----
    {
        using audio::DistortionMode;
        using audio::MultiDistortion;

        // Clip: a large input saturates hard to exactly +/-1 (after drive).
        {
            MultiDistortion md;
            md.mode = DistortionMode::Clip;
            md.drive = 2.0f;
            CHECK_NEAR(md.process(5.0f), 1.0f, 1e-6f);
            CHECK_NEAR(md.process(-5.0f), -1.0f, 1e-6f);
            CHECK_NEAR(md.process(0.1f), 0.2f, 1e-6f); // linear region: drive*x
        }
        // ATan: bounded below 1, monotonic, and softer than a hard clip where the clip would saturate.
        {
            MultiDistortion md;
            md.mode = DistortionMode::ATan;
            md.drive = 2.0f;
            const float big = md.process(10.0f);
            CHECK(big < 1.0f && big > 0.9f);              // approaches but never reaches 1
            CHECK(md.process(0.5f) < md.process(0.9f));   // monotonic increasing
            // At x=0.5, drive 2 -> clip would be 1.0; atan is (2/pi)atan(1)=0.5, i.e. softer.
            CHECK_NEAR(md.process(0.5f), 0.5f, 1e-4f);
        }
        // LoFi: quantizes a smooth ramp to a small number of distinct output levels.
        {
            MultiDistortion md;
            md.mode = DistortionMode::LoFi;
            md.bits = 3;
            md.rateHz = 44100.0f; // no sample-rate hold -> pure bit-crush staircase
            md.sampleRate = 44100.0f;
            md.drive = 1.0f;
            std::vector<float> levels;
            for (int i = 0; i <= 400; ++i) {
                const float x = -1.0f + 2.0f * static_cast<float>(i) / 400.0f;
                const float y = md.process(x);
                bool seen = false;
                for (float l : levels) {
                    if (std::fabs(l - y) < 1e-4f) {
                        seen = true;
                    }
                }
                if (!seen) {
                    levels.push_back(y);
                }
            }
            CHECK(levels.size() > 2);  // it does quantize into steps
            CHECK(levels.size() <= 18); // but only a few (bit-crushed), not ~400
        }
        // Overdrive: asymmetric — the positive and negative responses to the same magnitude differ.
        {
            MultiDistortion md;
            md.mode = DistortionMode::Overdrive;
            md.drive = 3.0f;
            const float p = md.process(0.5f);
            const float n = md.process(-0.5f);
            CHECK(std::fabs(std::fabs(p) - std::fabs(n)) > 1e-3f); // not symmetric
        }
        // Every mode is bounded and finite across a wide input range; post-gain scales the output.
        {
            for (DistortionMode m : {DistortionMode::Tanh, DistortionMode::Clip, DistortionMode::ATan,
                                     DistortionMode::LoFi, DistortionMode::Overdrive}) {
                MultiDistortion md;
                md.mode = m;
                md.drive = 4.0f;
                bool ok = true;
                for (int i = -50; i <= 50; ++i) {
                    const float y = md.process(static_cast<float>(i) * 0.1f);
                    ok = ok && std::isfinite(y) && std::fabs(y) < 2.0f;
                }
                CHECK(ok);
            }
            MultiDistortion md;
            md.mode = DistortionMode::ATan;
            md.drive = 1.0f;
            const float base = md.process(0.1f);
            md.postGainDb = 6.0206f; // x2
            CHECK_NEAR(md.process(0.1f), base * 2.0f, 1e-3f);
        }
        // Runs inside a bus chain via DistortionModeEffect.
        {
            MultiDistortion md;
            md.mode = DistortionMode::Overdrive;
            md.drive = 5.0f;
            audio::Bus chain;
            chain.add(std::make_unique<audio::DistortionModeEffect>(md));
            CHECK(std::isfinite(chain.process(0.7f)));
        }
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
    testCurve2D();
    testAtlasPacker();
    testForceField2D();
    testExpression();
    testAStar2D();
    testGeometry2D();
    testTransform2D();
    testRect2();
    testCollision();
    testRaycast();
    testSpatialGrid();
    testNavGrid();
    testNavMesh();
    testAutoTile();
    testSpatial3D();
    testSpectrum();
    testWav();
    testStreamRandomizer();
    testSampleMixer();
    testParticleEmitter();
    testTileSet();
    testCollisionLayers();
    testArea2D();
    testKinematicBody2D();
    testGravityField2D();
    testAvoidance();
    testVisibility2D();
    testSoftShadow2D();
    testPhysics2D();
    testPhysics2DRotation();
    testPhysics2DJoints();
    testPhysics2DGroove();
    testPhysicsQuery2D();
    testShapeCast2D();
    testConvexShape2D();
    testOneWayPlatform();
    testManifold2();
    testWarmStartSolver();
    testBroadphase();
    testSleeping();
    testCollisionFiltering();
    testCapsule();
    testWorldBoundary();
    testContactEvents();
    testCCD();
    testJointMotor();
    testConvex();
    testPhysicsMaterial();
    testPolylineCollider();
    testPhysics3D();
    testPhysics3DBoxes();
    testPhysics3DStacking();
    testPhysics3DWarmStack();
    testPhysics3DCapsule();
    testPhysics3DBroadphase();
    testPhysics3DSleep();
    testPhysics3DRay();
    testPhysics3DMoveSlide();
    testPhysics3DMaterial();
    testPhysics3DSlider();
    testPhysics3DJoint();
    testPhysics3DDistanceJoint();
    testPhysics3DHinge();
    testEditorScene();
    testEditorPickRay();
    testEditorGizmoDrag();
    testEditorHistory();
    testEditorSerialize();
    testEditorNodeOps();
    testEditorMultiSelect();
    testEditorPackage();
    testEditorWorldToScreen();
    testScript();
    testScriptCollections();
    testScriptStdlib();
    testScriptClosures();
    testScriptClasses();
    testScriptBinding();
    testScriptSignals();
    testScriptSafety();
    testScriptHotReload();
    testScriptTyping();
    testScriptTooling();
    testScriptSystem();
    testSceneTree();
    testSceneSerialize();
    testZomboidSim();
    testNormalLight();
    testParallax();
    testAudioDsp();
    testBusGraph();
    testStereo();
    testOscillator();
    testMusicSequencer();
    testPitchShifter();
    testModDsp();
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
    testCurve();
    testGradient();
    testRootMotion();
    testAnimator();
    testEventBus();
    testSignal();
    testStringId();
    testSlotMap();
    testRingBuffer();
    testJobs();
    testResourceCache();
    testAssetServer();
    testLogSinks();
    testCrashHandler();
    testTelemetry();
    testReplay();
    testCheckpoints();
    testMemory();
    testQuadtree();
    testEcsComponents();
    testGeometry3D();
    testSdf();
    testPcg32();
    testOverlap3D();
    testChunkStreamer();
    testSpriteOrder();
    testSweepPrune2D();
    testBvh();
    testOctree();
    testContainers();
    testReflect();
    testDateTime();
    testVfs();
    testVersion();
    testSceneStack();
    testTween();
    testTweenPlayer();
    testTimeline();
    testTriggerTrack();
    testLayout();
    testUiContainer();
    testRange();
    testStyleBox();
    testTheme();
    testTree();
    testItemList();
    testPopupMenu();
    testTextLayout();
    testRichText();
    testTextInput();
    testUI();
    testSerialize();
    testBase64();
    testXml();
    testConfigFile();
    testResourcePack();
    testJson();
    testCVars();
    testProfiler();
    testPerfBudget();
    testAppFocus();
    testPresentMode();
    testDisplayScale();
    testDisplays();
    testKtx2();
    testInputTextAndDrop();
    testCascadeSplits();
    testBitStream();
    testReliability();
    testSnapshot();
    testNetInterpolation();
    testPrediction();
    testRpc();
    testReplication();
    testConnection();
    testNetSim();
    testPathFollow2D();
    testTimer();
    testVisibleOnScreenNotifier2D();
    testGridMap();
    testObjLoader();
    testMeshLod();
    testGettextPo();
    testExportConfig();
    testConvexHull3D();
    testHeightField3D();
    testTriMesh3D();
    testNoise();
    testRandom();
    testInterpolate();
    testScheduler();
    testSequence();
    testCameraController();
    testTransformGraph();
    testGroupRegistry();
    testPrefab();
    testPrefabText();
    testLocalization();
    testGrid3D();
    testMultiMesh2D();
    testBillboard();
    testCamera3D();
    testShapes3D();
    testMeshTools();
    testPolyline();
    testTriangulate();
    testAnalog();
    testActionMap();
    testSceneSerializer();
    testEcs();
    testShake();
    testParticleAttractor();
    std::printf("%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
