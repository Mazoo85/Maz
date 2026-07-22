// tests/math/optics.cpp — verifies refraction + Fresnel (math::refract, fresnelSchlick, fresnelF0).
// Ground truths, exact optics, deterministic:
//   * at normal incidence light passes straight through (unit length, no bend), for any eta;
//   * at an angle, Snell's law holds: sin(theta_t) = eta * sin(theta_i), and the ray stays unit length;
//   * past the critical angle refract returns the zero vector (total internal reflection), flagged by
//     isTotalInternalReflection;
//   * fresnelF0(air, glass) ~ 0.04; Fresnel-Schlick is f0 head-on (cos=1) and 1 at grazing (cos=0),
//     monotonically increasing as the angle grazes;
//   * per-channel Fresnel applies independently.
#include "maz/math/Optics.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::fresnelF0;
using maz::math::fresnelSchlick;
using maz::math::isTotalInternalReflection;
using maz::math::refract;
using maz::math::vec3;

static bool near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }
static float len(const vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

int main() {
    const vec3 n(0, 0, 1); // surface normal (+Z)

    // --- 1. Normal incidence: straight through, unit length. ---
    {
        const vec3 i(0, 0, -1); // heading straight into the surface
        const vec3 r = refract(i, n, 0.75f);
        CHECK(near(len(r), 1.0f), "refracted ray is unit length");
        CHECK(near(r.x, 0) && near(r.y, 0) && r.z < 0, "no bend at normal incidence");
    }

    // --- 2. Snell's law at 45 degrees: sin(theta_t) = eta * sin(theta_i). ---
    {
        const float s = std::sin(3.14159265f * 0.25f); // sin 45
        const vec3 i(s, 0, -s); // 45deg incidence in the x-z plane, unit length
        const float eta = 0.5f;
        const vec3 r = refract(i, n, eta);
        CHECK(near(len(r), 1.0f), "refracted ray unit length");
        // Horizontal component magnitude is sin(theta_t); it should equal eta * sin(theta_i).
        const float sinT = std::fabs(r.x);
        CHECK(near(sinT, eta * s, 1e-4f), "Snell's law: sin(theta_t) = eta * sin(theta_i)");
        CHECK(r.z < 0.0f, "refracted ray continues into the surface");
    }

    // --- 3. Total internal reflection past the critical angle. ---
    {
        const float s = std::sin(3.14159265f * 0.25f);
        const vec3 i(s, 0, -s);
        const float eta = 2.0f; // dense -> sparse at 45deg exceeds the critical angle
        CHECK(isTotalInternalReflection(i, n, eta), "TIR flagged");
        const vec3 r = refract(i, n, eta);
        CHECK(r.x == 0.0f && r.y == 0.0f && r.z == 0.0f, "refract returns zero on TIR");
    }

    // --- 4. Base reflectance for air->glass. ---
    {
        CHECK(near(fresnelF0(1.0f, 1.5f), 0.04f, 1e-3f), "air->glass f0 ~ 0.04");
    }

    // --- 5. Fresnel-Schlick limits + monotonicity. ---
    {
        const float f0 = 0.04f;
        CHECK(near(fresnelSchlick(1.0f, f0), f0), "head-on Fresnel = f0");
        CHECK(near(fresnelSchlick(0.0f, f0), 1.0f), "grazing Fresnel = 1");
        float prev = -1.0f;
        bool mono = true;
        for (int k = 10; k >= 0; --k) { // cos from 1.0 down to 0.0
            const float cosT = static_cast<float>(k) / 10.0f;
            const float f = fresnelSchlick(cosT, f0);
            if (f < f0 - 1e-6f || f > 1.0f + 1e-6f) mono = false;
            (void)prev;
            prev = f;
        }
        CHECK(mono, "Fresnel stays within [f0, 1]");
        CHECK(fresnelSchlick(0.3f, f0) > fresnelSchlick(0.8f, f0), "more grazing -> more reflection");
    }

    // --- 6. Per-channel Fresnel. ---
    {
        const vec3 f0(0.02f, 0.04f, 0.10f);
        const vec3 f = fresnelSchlick(1.0f, f0);
        CHECK(near(f.x, 0.02f) && near(f.y, 0.04f) && near(f.z, 0.10f),
              "per-channel head-on Fresnel = channel f0");
    }

    if (g_fail == 0) {
        std::printf("optics: OK — normal incidence, Snell's law, TIR, air/glass f0, Fresnel limits, "
                    "per-channel.\n");
        return 0;
    }
    std::printf("optics: %d failure(s).\n", g_fail);
    return 1;
}
