// tests/game/patrolroute.cpp — verifies the discrete waypoint patrol (game::PatrolRoute): walk toward the
// current waypoint at a set speed, dwell on arrival, then advance per mode (Once, Loop, PingPong). Ground
// truths are hand-computed: constant-speed motion reaches a waypoint in the expected number of frames; a
// dwell holds position for its duration; Loop wraps, Once stops at the end, PingPong bounces; the
// arrivedThisUpdate pulse fires exactly on the arrival frame. Deterministic CPU.
#include "maz/game/PatrolRoute.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::game::PatrolRoute;
using maz::game::PatrolMode;
using maz::math::vec2;

static bool nearv(const vec2& a, const vec2& b, float e = 1e-4f) {
    return std::fabs(a.x - b.x) <= e && std::fabs(a.y - b.y) <= e;
}

int main() {
    // --- 1. Starts on the first waypoint heading to the second. ---
    {
        PatrolRoute r(PatrolMode::Loop);
        r.addWaypoint(vec2(0, 0));
        r.addWaypoint(vec2(10, 0));
        CHECK(nearv(r.position(), vec2(0, 0)) && r.targetIndex() == 1, "starts at wp0 heading to wp1");
    }

    // --- 2. Constant-speed motion; arrival snaps and pulses arrivedThisUpdate. ---
    {
        PatrolRoute r(PatrolMode::Loop);
        r.addWaypoint(vec2(0, 0));
        r.addWaypoint(vec2(10, 0));
        r.update(5.0f, 1.0f); // move 5 -> (5,0)
        CHECK(nearv(r.position(), vec2(5, 0)) && !r.arrivedThisUpdate(), "moves at constant speed");
        r.update(5.0f, 1.0f); // dist 5 == travel 5 -> arrive (10,0); no wait -> loop target back to 0
        CHECK(nearv(r.position(), vec2(10, 0)) && r.arrivedThisUpdate(), "snaps onto the waypoint and pulses");
        CHECK(r.targetIndex() == 0, "Loop advances target 1 -> 0 (wrap)");
        r.update(5.0f, 1.0f);
        CHECK(!r.arrivedThisUpdate(), "arrival pulse is one frame only");
    }

    // --- 3. Dwell holds position for its duration before moving on. ---
    {
        PatrolRoute r(PatrolMode::Loop);
        r.addWaypoint(vec2(0, 0));
        r.addWaypoint(vec2(1, 0), 2.0f); // 2s dwell at wp1
        r.update(1.0f, 1.0f); // reach (1,0) exactly, begin dwell
        CHECK(nearv(r.position(), vec2(1, 0)) && r.isWaiting(), "arrives and starts dwelling");
        r.update(1.0f, 1.0f); // 1s of 2s elapsed, still waiting, position held
        CHECK(r.isWaiting() && nearv(r.position(), vec2(1, 0)) && std::fabs(r.waitRemaining() - 1.0f) < 1e-4f,
              "holds position mid-dwell");
        r.update(1.0f, 1.0f); // dwell elapses -> advance target (loop back to 0)
        CHECK(!r.isWaiting() && r.targetIndex() == 0, "resumes after dwell");
    }

    // --- 4. Once mode stops at the last waypoint. ---
    {
        PatrolRoute r(PatrolMode::Once);
        r.addWaypoint(vec2(0, 0));
        r.addWaypoint(vec2(1, 0));
        r.update(1.0f, 1.0f); // reach wp1 (the last) -> finished
        CHECK(r.arrivedThisUpdate() && r.isFinished(), "Once finishes at the last waypoint");
        const vec2 held = r.position();
        r.update(1.0f, 1.0f); // no-op once finished
        CHECK(nearv(r.position(), held) && !r.arrivedThisUpdate(), "finished route is a no-op");
    }

    // --- 5. PingPong bounces back at the ends: 0->1->2->1->0->1... ---
    {
        PatrolRoute r(PatrolMode::PingPong);
        r.addWaypoint(vec2(0, 0));
        r.addWaypoint(vec2(1, 0));
        r.addWaypoint(vec2(2, 0));
        int order[6];
        order[0] = r.targetIndex(); // heading to 1
        // Reach each successive waypoint (unit spacing, speed 1, dt 1 => one hop per update).
        for (int i = 1; i < 6; ++i) { r.update(1.0f, 1.0f); order[i] = r.targetIndex(); }
        // After reaching 1 -> target 2; reach 2 -> bounce to 1; reach 1 -> 0; reach 0 -> bounce to 1; reach 1 -> 2.
        CHECK(order[0] == 1 && order[1] == 2 && order[2] == 1 && order[3] == 0 && order[4] == 1 && order[5] == 2,
              "PingPong bounces at both ends");
    }

    // --- 6. reset returns to the first waypoint. ---
    {
        PatrolRoute r(PatrolMode::Loop);
        r.addWaypoint(vec2(0, 0));
        r.addWaypoint(vec2(10, 0));
        r.update(5.0f, 1.0f);
        r.reset();
        CHECK(nearv(r.position(), vec2(0, 0)) && r.targetIndex() == 1 && !r.isWaiting(), "reset returns to start");
    }

    // --- 7. Empty / single-point routes and non-positive inputs are safe no-ops. ---
    {
        PatrolRoute empty(PatrolMode::Loop);
        empty.update(5.0f, 1.0f);
        CHECK(empty.size() == 0 && !empty.arrivedThisUpdate(), "empty route is safe");

        PatrolRoute one(PatrolMode::Loop);
        one.addWaypoint(vec2(3, 4));
        one.update(5.0f, 1.0f);
        CHECK(nearv(one.position(), vec2(3, 4)), "single-point route sits still");

        PatrolRoute r(PatrolMode::Loop);
        r.addWaypoint(vec2(0, 0));
        r.addWaypoint(vec2(10, 0));
        r.update(0.0f, 1.0f); // zero speed
        r.update(5.0f, 0.0f); // zero dt
        CHECK(nearv(r.position(), vec2(0, 0)), "zero speed / zero dt do not move");
    }

    if (g_fail == 0) std::printf("patrol route: all tests passed\n");
    return g_fail == 0 ? 0 : 1;
}
