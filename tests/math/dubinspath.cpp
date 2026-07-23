// tests/math/dubinspath.cpp — verifies the Dubins shortest-path planner (math DubinsPath.hpp).
// Ground truths, deterministic (fixed cases + seeded-LCG poses, no <random>, no clock):
//   * REACHES THE GOAL (airtight): following any VALID word to its end lands exactly on the goal pose
//     (position + heading). Because the sampler is an independent forward integrator of the turn/straight
//     controls, this validates every closed-form t/p/q formula — a wrong formula would miss the goal;
//   * SHORTEST (airtight vs the family): the returned path is the minimum-length word that reaches the goal;
//   * LOWER BOUND: the length is never below the straight-line distance;
//   * UNIT SPEED + CURVATURE (airtight): along the path the sampler moves at unit speed and the heading
//     turns at rate 0 (straight) or ±1/radius (turn) — never tighter than the turning radius;
//   * ANALYTIC: two poses aligned along +x give a pure straight line of length = the gap.
#include "maz/math/DubinsPath.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::math::vec2;
using maz::math::Pose2;
using maz::math::DubinsWord;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float angDiff(float a, float b) { return std::fabs(std::atan2(std::sin(a - b), std::cos(a - b))); }
static float dist2(const vec2& a, const vec2& b) {
    const float dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 33); }
    float unit() { return static_cast<float>(next()) / 4294967296.0f; } // [0,1)
    float sym() { return unit() * 2.0f - 1.0f; }
};

int main() {
    using namespace maz::math;

    const DubinsWord ALL[6] = {DubinsWord::LSL, DubinsWord::LSR, DubinsWord::RSL,
                               DubinsWord::RSR, DubinsWord::RLR, DubinsWord::LRL};

    // --- 1. Over random pose pairs: every valid word reaches the goal; the shortest is picked; lower bound. ---
    {
        Lcg rng{0xD0B1u};
        float worstReach = 0.0f, worstHead = 0.0f;
        int validWords = 0, checkedPairs = 0, shortestOk = 0;
        const float TWO_PI = 6.28318530717958647692f;
        for (int i = 0; i < 3000; ++i) {
            Pose2 a{vec2(rng.sym() * 8.0f, rng.sym() * 8.0f), rng.unit() * TWO_PI};
            Pose2 b{vec2(rng.sym() * 8.0f, rng.sym() * 8.0f), rng.unit() * TWO_PI};
            const float radius = 0.5f + rng.unit() * 3.0f;

            // Every valid word must reconstruct to the goal (this validates all six closed forms).
            float minValid = 1e30f;
            for (DubinsWord w : ALL) {
                const DubinsPath p = dubinsComputeWord(a, b, radius, w);
                if (!p.ok) {
                    continue;
                }
                ++validWords;
                const Pose2 end = dubinsSample(p, p.length);
                worstReach = std::max(worstReach, dist2(end.pos, b.pos));
                worstHead = std::max(worstHead, angDiff(end.heading, b.heading));
                minValid = std::min(minValid, p.length);
            }

            const DubinsPath best = dubinsShortestPath(a, b, radius);
            CHECK(best.ok, "a shortest path exists for a positive radius");
            if (best.ok) {
                ++checkedPairs;
                // Shortest equals the minimum valid word length.
                if (std::fabs(best.length - minValid) < 1e-3f * (1.0f + minValid)) {
                    ++shortestOk;
                }
                // Lower bound: never shorter than the straight-line distance.
                CHECK(best.length >= dist2(a.pos, b.pos) - 1e-3f, "length is at least the straight-line gap");
            }
        }
        CHECK(validWords > 6000, "the random pairs exercise many valid words across all six types");
        CHECK(checkedPairs > 2900, "nearly every pair yields a shortest path");
        CHECK(shortestOk == checkedPairs, "the returned path is the minimum-length word that reaches the goal");
        CHECK(worstReach < 4e-3f, "following any valid word lands on the goal position");
        CHECK(worstHead < 3e-3f, "following any valid word lands on the goal heading");
    }

    // --- 2. Unit speed + curvature: sampling densely, the sampler moves at unit speed and turns at rate
    //        0 or ±1/radius (never tighter than the turning radius). ---
    {
        Lcg rng{0x7EEDu};
        float worstSpeed = 0.0f, worstCurv = 0.0f;
        const float TWO_PI = 6.28318530717958647692f;
        for (int i = 0; i < 400; ++i) {
            Pose2 a{vec2(rng.sym() * 5.0f, rng.sym() * 5.0f), rng.unit() * TWO_PI};
            Pose2 b{vec2(rng.sym() * 5.0f, rng.sym() * 5.0f), rng.unit() * TWO_PI};
            const float radius = 1.0f + rng.unit() * 2.0f;
            const DubinsPath p = dubinsShortestPath(a, b, radius);
            if (!p.ok || p.length < 0.2f) {
                continue;
            }
            // Segment boundaries in world arc-length: sample windows straddling one mix two curvatures.
            const float b0 = p.segLen[0] * radius;
            const float b1 = (p.segLen[0] + p.segLen[1]) * radius;
            const float ds = 0.01f;
            for (float s = ds; s + ds < p.length; s += p.length / 40.0f) {
                if ((s - ds < b0 && s + ds > b0) || (s - ds < b1 && s + ds > b1)) {
                    continue; // window crosses a segment boundary — curvature legitimately transitions
                }
                const Pose2 q0 = dubinsSample(p, s - ds);
                const Pose2 q1 = dubinsSample(p, s + ds);
                const float moved = dist2(q0.pos, q1.pos);
                worstSpeed = std::max(worstSpeed, std::fabs(moved - 2.0f * ds)); // unit speed => 2*ds over 2*ds
                const float rate = angDiff(q1.heading, q0.heading) / (2.0f * ds); // |curvature|
                // Must be ~0 (straight) or ~1/radius (turn); take distance to the nearer.
                const float toStraight = rate;
                const float toTurn = std::fabs(rate - 1.0f / radius);
                worstCurv = std::max(worstCurv, std::min(toStraight, toTurn));
            }
        }
        CHECK(worstSpeed < 5e-4f, "the sampler advances at unit speed along the path");
        CHECK(worstCurv < 1e-2f, "heading turns at rate 0 or exactly 1/radius (never tighter)");
    }

    // --- 3. Analytic: aligned poses along +x give a pure straight line of length = the gap. ---
    {
        Pose2 a{vec2(0, 0), 0.0f};
        Pose2 b{vec2(5, 0), 0.0f};
        const DubinsPath p = dubinsShortestPath(a, b, 1.0f);
        CHECK(p.ok, "aligned poses have a path");
        CHECK(std::fabs(p.length - 5.0f) < 1e-3f, "aligned straight-shot length equals the gap");
        const Pose2 mid = dubinsSample(p, 2.5f);
        CHECK(dist2(mid.pos, vec2(2.5f, 0)) < 1e-3f, "the midpoint sits on the straight line");
        CHECK(angDiff(mid.heading, 0.0f) < 1e-4f, "heading stays constant on a straight shot");
    }

    if (g_fail == 0) {
        std::printf("dubinspath: OK — reaches goal, shortest, lower bound, unit speed + curvature, analytic.\n");
        return 0;
    }
    std::printf("dubinspath: %d failure(s).\n", g_fail);
    return 1;
}
