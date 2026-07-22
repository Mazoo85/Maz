// tests/anim/springbone.cpp — verifies the jiggle-bone chain (anim::SpringBone).
// Ground truths: a chain left at rest stays at its rest pose (no drift, velocity dies); after the root is
// moved, the chain lags at first (a child is displaced from its rigid target) then converges to the new rigid
// pose over time; higher stiffness converges faster; the simulation is stable (no blow-up) over a long run
// with no gravity; reset() snaps rigidly to the pose relative to the root with zero velocity; and it is
// deterministic. Checked against the damped-spring-toward-rigid-pose model. Pure CPU, deterministic.
#include "maz/anim/SpringBone.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::anim::SpringBone;
using maz::math::vec3;

static float dist(const vec3& a, const vec3& b) {
    const vec3 d = a - b;
    return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

static std::vector<vec3> straightChain() {
    return {vec3(0, 0, 0), vec3(1, 0, 0), vec3(2, 0, 0)}; // root + two joints along +X
}

int main() {
    // --- 1. At rest the chain holds its pose and velocity decays. ---
    {
        SpringBone sb;
        sb.init(straightChain());
        sb.setGravity(vec3(0, 0, 0));
        for (int i = 0; i < 200; ++i) sb.update(1.0f / 60.0f);
        CHECK(dist(sb.position(1), vec3(1, 0, 0)) < 1e-3f, "joint 1 stays at rest");
        CHECK(dist(sb.position(2), vec3(2, 0, 0)) < 1e-3f, "joint 2 stays at rest");
        CHECK(dist(sb.velocity(2), vec3(0, 0, 0)) < 1e-3f, "velocity decays to ~0 at rest");
    }

    // --- 2. Move the root: the chain lags first, then converges to the new rigid pose. ---
    {
        SpringBone sb;
        sb.init(straightChain());
        sb.setGravity(vec3(0, 0, 0));
        sb.setStiffness(40.0f);
        sb.setDamping(8.0f);
        sb.setRoot(vec3(0, 1, 0)); // yank the root up by 1
        // Immediately after: the tip has not caught up yet (still near its old y=0).
        sb.update(1.0f / 240.0f);
        CHECK(sb.position(2).y < 0.5f, "tip lags right after the root jumps");
        // After enough time it converges to the rigid pose from the new root: (0,1,0),(1,1,0),(2,1,0).
        for (int i = 0; i < 400; ++i) sb.update(1.0f / 120.0f);
        CHECK(dist(sb.position(1), vec3(1, 1, 0)) < 1e-2f, "joint 1 converges to the new rigid pose");
        CHECK(dist(sb.position(2), vec3(2, 1, 0)) < 1e-2f, "joint 2 converges to the new rigid pose");
    }

    // --- 3. Higher stiffness converges faster. ---
    {
        auto tipErrorAfter = [](float stiffness) {
            SpringBone sb;
            sb.init(straightChain());
            sb.setGravity(vec3(0, 0, 0));
            sb.setStiffness(stiffness);
            sb.setDamping(2.0f);
            sb.setRoot(vec3(0, 1, 0));
            for (int i = 0; i < 12; ++i) sb.update(1.0f / 120.0f); // short window
            return dist(sb.position(2), vec3(2, 1, 0));
        };
        CHECK(tipErrorAfter(120.0f) < tipErrorAfter(15.0f), "stiffer chain is closer to target after a fixed time");
    }

    // --- 4. Stability: no blow-up over a long run while the root wiggles (no gravity). ---
    {
        SpringBone sb;
        sb.init(straightChain());
        sb.setGravity(vec3(0, 0, 0));
        bool finite = true;
        for (int i = 0; i < 2000; ++i) {
            const float t = static_cast<float>(i) * 0.01f;
            sb.setRoot(vec3(0, std::sin(t), 0));
            sb.update(1.0f / 120.0f);
            for (std::size_t k = 0; k < sb.size(); ++k) {
                const vec3 p = sb.position(k);
                if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) finite = false;
                if (std::fabs(p.y) > 10.0f) finite = false; // bounded near the ±1 drive
            }
        }
        CHECK(finite, "simulation stays finite and bounded under continuous motion");
    }

    // --- 5. reset snaps rigidly to the pose from the current root, zero velocity. ---
    {
        SpringBone sb;
        sb.init(straightChain());
        sb.setRoot(vec3(5, 5, 0));
        sb.reset();
        CHECK(dist(sb.position(1), vec3(6, 5, 0)) < 1e-5f, "reset places joint 1 rigidly from the root");
        CHECK(dist(sb.position(2), vec3(7, 5, 0)) < 1e-5f, "reset places joint 2 rigidly");
        CHECK(dist(sb.velocity(2), vec3(0, 0, 0)) < 1e-6f, "reset clears velocity");
    }

    // --- 6. Determinism. ---
    {
        SpringBone a, b;
        a.init(straightChain());
        b.init(straightChain());
        for (int i = 0; i < 100; ++i) {
            const float t = static_cast<float>(i) * 0.02f;
            a.setRoot(vec3(0, std::sin(t), 0));
            b.setRoot(vec3(0, std::sin(t), 0));
            a.update(1.0f / 90.0f);
            b.update(1.0f / 90.0f);
        }
        CHECK(dist(a.position(2), b.position(2)) < 1e-6f, "identical drive -> identical result");
    }

    if (g_fail == 0) {
        std::printf("springbone: OK — rest hold, lag+converge, stiffness ordering, stability, reset, determinism.\n");
        return 0;
    }
    std::printf("springbone: %d failure(s).\n", g_fail);
    return 1;
}
