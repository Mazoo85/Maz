// tests/game/spread.cpp — verifies the weapon-spread helpers (game::spreadDirection2D / spreadFan2D /
// spreadDirection3D). Ground truths: the 2D fan is deterministic, evenly spaced across [-halfAngle,+halfAngle],
// symmetric, with the centre pellet on-axis for odd counts; random 2D/3D spread stays within the half-angle, is
// unit-length, varies across samples, is reproducible by seed; a 3D cone of angle 0 returns the axis exactly and
// the 3D distribution is uniform over the solid angle (mean angle < the half-angle). Pure CPU, deterministic.
#include "maz/game/Spread.hpp"
#include "maz/core/Pcg32.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz;

static float len2(const math::vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }
static float angle2(const math::vec2& a, const math::vec2& b) {
    float d = (a.x * b.x + a.y * b.y) / (len2(a) * len2(b));
    d = d < -1.0f ? -1.0f : (d > 1.0f ? 1.0f : d);
    return std::acos(d);
}
static float angle3(const math::vec3& a, const math::vec3& b) {
    float d = math::dot(math::normalize(a), math::normalize(b));
    d = d < -1.0f ? -1.0f : (d > 1.0f ? 1.0f : d);
    return std::acos(d);
}

int main() {
    const math::vec2 aim2(1.0f, 0.0f);
    const math::vec3 aim3(0.0f, 0.0f, 1.0f);
    const float ha = 0.3f;

    // --- 1. 2D fan: count, spacing, symmetry, centre on-axis. ---
    {
        const std::vector<math::vec2> fan = game::spreadFan2D(aim2, ha, 5);
        CHECK(fan.size() == 5, "fan has `count` directions");
        CHECK(std::fabs(angle2(fan[0], aim2) - ha) < 1e-4f && std::fabs(angle2(fan[4], aim2) - ha) < 1e-4f,
              "the outer pellets sit at ±halfAngle");
        CHECK(angle2(fan[2], aim2) < 1e-4f, "odd count puts the centre pellet on-axis");
        // Even spacing: consecutive angular gaps equal.
        const float g0 = angle2(fan[0], fan[1]), g1 = angle2(fan[1], fan[2]);
        CHECK(std::fabs(g0 - g1) < 1e-4f, "fan pellets are evenly spaced");
        for (const math::vec2& d : fan) CHECK(std::fabs(len2(d) - 1.0f) < 1e-4f, "fan directions are unit length");
        CHECK(game::spreadFan2D(aim2, ha, 1).size() == 1 && angle2(game::spreadFan2D(aim2, ha, 1)[0], aim2) < 1e-4f,
              "count 1 returns the aim direction");
        CHECK(game::spreadFan2D(aim2, ha, 0).empty(), "count < 1 -> empty");
    }

    // --- 2. Random 2D spread: within the half-angle, unit, varies, reproducible. ---
    {
        core::Pcg32 rng(42u, 1u);
        float maxA = 0.0f, minA = 1e9f;
        for (int i = 0; i < 500; ++i) {
            const math::vec2 d = game::spreadDirection2D(aim2, ha, rng);
            const float a = angle2(d, aim2);
            CHECK(a <= ha + 1e-4f, "2D spread stays within the half-angle");
            CHECK(std::fabs(len2(d) - 1.0f) < 1e-4f, "2D spread is unit length");
            maxA = std::fmax(maxA, a); minA = std::fmin(minA, a);
        }
        CHECK(maxA > 0.5f * ha && minA < 0.1f * ha, "2D spread actually varies across the cone");
        core::Pcg32 a(7u, 2u), b(7u, 2u);
        const math::vec2 da = game::spreadDirection2D(aim2, ha, a), db = game::spreadDirection2D(aim2, ha, b);
        CHECK(da.x == db.x && da.y == db.y, "same seed -> identical 2D spread");
    }

    // --- 3. Random 3D cone: within the half-angle, unit, uniform-ish, angle-0 returns axis. ---
    {
        const float cone = 0.4f;
        core::Pcg32 rng(99u, 3u);
        float sumA = 0.0f; int n = 800;
        for (int i = 0; i < n; ++i) {
            const math::vec3 d = game::spreadDirection3D(aim3, cone, rng);
            const float a = angle3(d, aim3);
            CHECK(a <= cone + 1e-4f, "3D spread stays within the cone half-angle");
            CHECK(std::fabs(math::dot(d, d) - 1.0f) < 1e-3f, "3D spread is unit length");
            sumA += a;
        }
        const float meanA = sumA / static_cast<float>(n);
        CHECK(meanA > 0.05f && meanA < cone, "3D cone samples spread out (mean angle between 0 and the half-angle)");
        // angle 0 -> exactly the axis.
        core::Pcg32 z(1u, 1u);
        const math::vec3 d0 = game::spreadDirection3D(aim3, 0.0f, z);
        CHECK(angle3(d0, aim3) < 1e-4f, "a zero-angle cone returns the axis");
    }

    if (g_fail == 0) {
        std::printf("spread: OK — 2D fan spacing/symmetry, random 2D/3D within cone, unit, varies, reproducible.\n");
        return 0;
    }
    std::printf("spread: %d failure(s).\n", g_fail);
    return 1;
}
