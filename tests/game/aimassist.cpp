// tests/game/aimassist.cpp — verifies target magnetism (game::aimAssist): rotate the aim toward the
// nearest-by-angle target inside a cone, closing a strength fraction of the gap, capped at maxCorrection,
// never overshooting. Ground truths are hand-computed against a +X aim and targets at known angles: a
// target 30deg up with full strength and a big cap aligns the aim onto it; a small cap limits the swing; a
// half strength closes half the gap; a target outside the cone is ignored; the nearest of several wins; the
// sign follows whether the target is above or below. Deterministic CPU.
#include "maz/game/AimAssist.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::game::aimAssist;
using maz::math::vec2;

static float deg(float d) { return d * 3.14159265358979f / 180.0f; }
static float aimAngle(const vec2& v) { return std::atan2(v.y, v.x); } // aim measured from +X

int main() {
    const vec2 origin(0, 0);
    const vec2 aim(1, 0); // pointing along +X

    // --- 1. No targets: aim unchanged, nothing locked. ---
    {
        auto r = aimAssist(origin, aim, {}, deg(45), deg(90));
        CHECK(r.target == -1 && r.applied == 0.0f && std::fabs(aimAngle(r.aim)) < 1e-5f, "no targets: unchanged");
    }

    // --- 2. Target 30deg up, full strength, generous cap: aim snaps onto it. ---
    {
        const vec2 t(std::cos(deg(30)), std::sin(deg(30)));
        auto r = aimAssist(origin, aim, {t}, deg(45), deg(90), 1.0f);
        CHECK(r.target == 0 && std::fabs(aimAngle(r.aim) - deg(30)) < 1e-4f, "aligns onto the in-cone target");
        CHECK(std::fabs(r.applied - deg(30)) < 1e-4f, "applied equals the full gap");
    }

    // --- 3. Small cap limits the swing to maxCorrection. ---
    {
        const vec2 t(std::cos(deg(30)), std::sin(deg(30)));
        auto r = aimAssist(origin, aim, {t}, deg(45), deg(10), 1.0f); // cap 10deg of a 30deg gap
        CHECK(r.target == 0 && std::fabs(aimAngle(r.aim) - deg(10)) < 1e-4f, "cap limits the correction");
    }

    // --- 4. Half strength closes half the gap. ---
    {
        const vec2 t(std::cos(deg(40)), std::sin(deg(40)));
        auto r = aimAssist(origin, aim, {t}, deg(60), deg(90), 0.5f); // 0.5 * 40 = 20deg
        CHECK(std::fabs(aimAngle(r.aim) - deg(20)) < 1e-4f, "strength closes a fraction of the gap");
    }

    // --- 5. Target outside the cone is ignored. ---
    {
        const vec2 t(std::cos(deg(60)), std::sin(deg(60)));
        auto r = aimAssist(origin, aim, {t}, deg(45), deg(90), 1.0f); // 60 > 45 cone
        CHECK(r.target == -1 && std::fabs(aimAngle(r.aim)) < 1e-5f, "out-of-cone target ignored");
    }

    // --- 6. Nearest-by-angle target wins among several. ---
    {
        const vec2 far(std::cos(deg(30)), std::sin(deg(30)));
        const vec2 near_(std::cos(deg(-10)), std::sin(deg(-10)));
        auto r = aimAssist(origin, aim, {far, near_}, deg(45), deg(90), 1.0f);
        CHECK(r.target == 1 && std::fabs(aimAngle(r.aim) - deg(-10)) < 1e-4f, "closest angle wins");
    }

    // --- 7. Sign: a target below rotates the aim downward. ---
    {
        const vec2 t(std::cos(deg(-25)), std::sin(deg(-25)));
        auto r = aimAssist(origin, aim, {t}, deg(45), deg(90), 1.0f);
        CHECK(r.applied < 0.0f && std::fabs(aimAngle(r.aim) - deg(-25)) < 1e-4f, "downward target -> negative correction");
    }

    // --- 8. A target dead ahead needs no correction but still locks. ---
    {
        const vec2 t(5, 0);
        auto r = aimAssist(origin, aim, {t}, deg(45), deg(90), 1.0f);
        CHECK(r.target == 0 && std::fabs(r.applied) < 1e-5f, "on-axis target: locked, no rotation");
    }

    // --- 9. Guards: degenerate aim, zero cone/cap/strength, coincident target. ---
    {
        auto r0 = aimAssist(origin, vec2(0, 0), {vec2(1, 0)}, deg(45), deg(90));
        CHECK(r0.target == -1, "degenerate aim returns no lock");
        const vec2 t(std::cos(deg(20)), std::sin(deg(20)));
        CHECK(aimAssist(origin, aim, {t}, 0.0f, deg(90)).target == -1, "zero cone: no lock");
        CHECK(aimAssist(origin, aim, {t}, deg(45), 0.0f).target == -1, "zero cap: no assist");
        CHECK(aimAssist(origin, aim, {t}, deg(45), deg(90), 0.0f).target == -1, "zero strength: no assist");
        auto rc = aimAssist(origin, aim, {origin}, deg(45), deg(90)); // target on the shooter
        CHECK(rc.target == -1, "coincident target ignored");
    }

    if (g_fail == 0) std::printf("aim assist: all tests passed\n");
    return g_fail == 0 ? 0 : 1;
}
