// tests/math/superquadric.cpp — verifies the 3D superquadric / superellipsoid (math Superquadric.hpp).
// Ground truths, deterministic (fixed + seeded-LCG params, no <random>, no clock):
//   * PARAMETRIC ON THE IMPLICIT SURFACE (airtight): every parametric surface point satisfies the INDEPENDENT
//     inside-outside function F == 1, for ALL exponents — two unrelated formulas agreeing is a strong check;
//   * ELLIPSOID SPECIAL CASE: e1=e2=1 reduces to the ellipsoid point and (x/a)^2+(y/b)^2+(z/c)^2 == 1;
//   * SPHERE: a=b=c=r with e1=e2=1 puts every point at radius r;
//   * POLES: u=+/-pi/2 give (0,0,+/-c);
//   * SIGN: the centre is inside (F<1), a far point outside (F>1);
//   * NORMAL: the gradient normal is unit and perpendicular to both parametric surface tangents.
#include "maz/math/Superquadric.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::math::vec3;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len(const vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
static float dot3(const vec3& a, const vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 33); }
    float unit() { return static_cast<float>(next()) / 4294967296.0f; }
};

int main() {
    using namespace maz::math;
    const float PI = 3.14159265358979324f;

    // --- 1. Parametric points satisfy the independent implicit F == 1, across many exponents. ---
    {
        Lcg rng{0x59D1u};
        float worst = 0.0f;
        int tested = 0;
        for (int i = 0; i < 400; ++i) {
            const float a = 0.5f + rng.unit() * 2.0f, b = 0.5f + rng.unit() * 2.0f, c = 0.5f + rng.unit() * 2.0f;
            const float e1 = 0.4f + rng.unit() * 1.4f, e2 = 0.4f + rng.unit() * 1.4f; // avoid extreme near-box
            for (int k = 0; k < 12; ++k) {
                const float u = -PI * 0.5f + rng.unit() * PI;      // [-pi/2, pi/2]
                const float v = -PI + rng.unit() * 2.0f * PI;      // [-pi, pi]
                const vec3 p = superellipsoidPoint(a, b, c, e1, e2, u, v);
                const float F = superquadricInsideOutside(a, b, c, e1, e2, p);
                worst = std::max(worst, std::fabs(F - 1.0f));
                ++tested;
            }
        }
        CHECK(tested > 4000, "many parametric points tested across exponents");
        CHECK(worst < 5e-3f, "every parametric surface point satisfies the implicit F == 1");
    }

    // --- 2. Ellipsoid special case e1=e2=1. ---
    {
        const float a = 2.0f, b = 1.5f, c = 3.0f;
        Lcg rng{0xE111u};
        float worstP = 0.0f, worstF = 0.0f;
        for (int i = 0; i < 500; ++i) {
            const float u = -PI * 0.5f + rng.unit() * PI, v = -PI + rng.unit() * 2.0f * PI;
            const vec3 p = superellipsoidPoint(a, b, c, 1.0f, 1.0f, u, v);
            const vec3 want(a * std::cos(u) * std::cos(v), b * std::cos(u) * std::sin(v), c * std::sin(u));
            worstP = std::max(worstP, len(p - want));
            const float impl = (p.x / a) * (p.x / a) + (p.y / b) * (p.y / b) + (p.z / c) * (p.z / c);
            worstF = std::max(worstF, std::fabs(impl - 1.0f));
        }
        CHECK(worstP < 1e-4f, "e1=e2=1 reproduces the ellipsoid parameterization");
        CHECK(worstF < 1e-4f, "ellipsoid points satisfy (x/a)^2+(y/b)^2+(z/c)^2 = 1");
    }

    // --- 3. Sphere + poles. ---
    {
        const float r = 2.5f;
        Lcg rng{0x59E0u};
        float worst = 0.0f;
        for (int i = 0; i < 300; ++i) {
            const float u = -PI * 0.5f + rng.unit() * PI, v = -PI + rng.unit() * 2.0f * PI;
            worst = std::max(worst, std::fabs(len(superellipsoidPoint(r, r, r, 1.0f, 1.0f, u, v)) - r));
        }
        CHECK(worst < 1e-4f, "a=b=c, e1=e2=1 places every point at radius r");
        CHECK(len(superellipsoidPoint(1, 2, 3, 0.7f, 1.3f, PI * 0.5f, 0.4f) - vec3(0, 0, 3)) < 1e-4f,
              "u=+pi/2 is the north pole (0,0,c)");
        CHECK(len(superellipsoidPoint(1, 2, 3, 0.7f, 1.3f, -PI * 0.5f, 0.4f) - vec3(0, 0, -3)) < 1e-4f,
              "u=-pi/2 is the south pole (0,0,-c)");
    }

    // --- 4. Inside/outside sign. ---
    {
        const float a = 1.5f, b = 1.0f, c = 2.0f, e1 = 0.8f, e2 = 1.2f;
        CHECK(superquadricInsideOutside(a, b, c, e1, e2, vec3(0, 0, 0)) < 1.0f, "the centre is inside (F<1)");
        CHECK(superquadricInsideOutside(a, b, c, e1, e2, vec3(10, 10, 10)) > 1.0f, "a far point is outside (F>1)");
        CHECK(superquadricInsideOutside(a, b, c, e1, e2, vec3(a * 0.5f, 0, 0)) < 1.0f,
              "a point at half the x-axis is inside");
    }

    // --- 5. Normal is unit and perpendicular to both parametric tangents. ---
    {
        const float a = 1.3f, b = 1.7f, c = 1.1f, e1 = 0.9f, e2 = 1.1f;
        Lcg rng{0x40E1u};
        float worstLen = 0.0f, worstDot = 0.0f;
        const float du = 1e-3f;
        for (int i = 0; i < 300; ++i) {
            // Keep away from the poles/seams where the parameterization degenerates.
            const float u = -1.2f + rng.unit() * 2.4f;
            const float v = -2.8f + rng.unit() * 5.6f;
            const vec3 p = superellipsoidPoint(a, b, c, e1, e2, u, v);
            const vec3 tu = superellipsoidPoint(a, b, c, e1, e2, u + du, v) -
                            superellipsoidPoint(a, b, c, e1, e2, u - du, v);
            const vec3 tv = superellipsoidPoint(a, b, c, e1, e2, u, v + du) -
                            superellipsoidPoint(a, b, c, e1, e2, u, v - du);
            const vec3 n = superquadricNormal(a, b, c, e1, e2, p);
            worstLen = std::max(worstLen, std::fabs(len(n) - 1.0f));
            const float lu = len(tu), lv = len(tv);
            if (lu > 1e-4f && lv > 1e-4f) {
                worstDot = std::max(worstDot, std::fabs(dot3(n, tu) / lu));
                worstDot = std::max(worstDot, std::fabs(dot3(n, tv) / lv));
            }
        }
        CHECK(worstLen < 1e-4f, "the superquadric normal is unit length");
        CHECK(worstDot < 2e-2f, "the normal is perpendicular to both parametric surface tangents");
    }

    if (g_fail == 0) {
        std::printf("superquadric: OK — parametric-on-implicit, ellipsoid, sphere/poles, sign, normal.\n");
        return 0;
    }
    std::printf("superquadric: %d failure(s).\n", g_fail);
    return 1;
}
