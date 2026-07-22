// tests/math/sphericalcoords.cpp — verifies spherical<->cartesian conversion (math::SphericalCoords).
// Ground truths: the three cardinal directions map to the documented (azimuth, elevation); a full
// round-trip cartesian->spherical->cartesian is the identity across a grid of directions; radius scales
// the result linearly; the origin and the poles are handled without NaN; orbitPosition places the eye at
// the right offset from a target; and equirect direction<->UV round-trips with the poles at v=0/v=1.
// Values checked against the closed-form convention. Pure CPU, deterministic.
#include "maz/math/SphericalCoords.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::math;

static bool near(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) <= eps; }
static bool vnear(const vec3& a, const vec3& b, float eps = 1e-5f) {
    return near(a.x, b.x, eps) && near(a.y, b.y, eps) && near(a.z, b.z, eps);
}

int main() {
    const float pi = 3.14159265358979323846f;

    // --- 1. Cardinal directions map as documented. ---
    {
        CHECK(vnear(sphericalToCartesian(1, 0, 0), vec3(0, 0, 1)), "azimuth 0, elev 0 -> +Z");
        CHECK(vnear(sphericalToCartesian(1, pi / 2, 0), vec3(1, 0, 0)), "azimuth pi/2 -> +X");
        CHECK(vnear(sphericalToCartesian(1, 0, pi / 2), vec3(0, 1, 0)), "elev pi/2 -> +Y (up)");
        CHECK(vnear(sphericalToCartesian(1, pi, 0), vec3(0, 0, -1)), "azimuth pi -> -Z");
        CHECK(vnear(sphericalToCartesian(1, 0, -pi / 2), vec3(0, -1, 0)), "elev -pi/2 -> -Y (down)");
    }

    // --- 2. cartesianToSpherical is the exact inverse across a grid of directions. ---
    {
        for (int ai = 0; ai < 8; ++ai) {
            for (int ei = -3; ei <= 3; ++ei) {
                const float az = static_cast<float>(ai) * (pi / 4.0f) - pi + 0.1f; // in (-pi, pi)
                const float el = static_cast<float>(ei) * (pi / 8.0f);             // in [-3pi/8, 3pi/8]
                const float r = 2.5f;
                const vec3 p = sphericalToCartesian(r, az, el);
                const Spherical s = cartesianToSpherical(p);
                CHECK(near(s.radius, r, 1e-4f), "round-trip radius preserved");
                // Compare by re-projecting (avoids azimuth wrap ambiguity at the same point).
                CHECK(vnear(sphericalToCartesian(s), p, 1e-4f), "round-trip direction preserved");
            }
        }
    }

    // --- 3. Radius scales linearly. ---
    {
        const vec3 a = sphericalToCartesian(1, 1.0f, 0.5f);
        const vec3 b = sphericalToCartesian(3, 1.0f, 0.5f);
        CHECK(vnear(b, vec3(a.x * 3, a.y * 3, a.z * 3), 1e-5f), "3x radius == 3x vector");
    }

    // --- 4. Origin and poles are NaN-free and sane. ---
    {
        const Spherical o = cartesianToSpherical(vec3(0, 0, 0));
        CHECK(o.radius == 0.0f && o.azimuth == 0.0f && o.elevation == 0.0f, "origin -> all zero");
        const Spherical up = cartesianToSpherical(vec3(0, 5, 0));
        CHECK(near(up.radius, 5.0f) && near(up.elevation, pi / 2), "north pole: r=5, elev=pi/2");
        CHECK(std::isfinite(up.azimuth), "pole azimuth is finite (not NaN)");
        // A tiny numerical overshoot past the unit sphere must not produce NaN from asin.
        const Spherical clamped = cartesianToSpherical(vec3(0, 1.0000001f, 0));
        CHECK(std::isfinite(clamped.elevation), "asin argument clamped -> finite elevation");
    }

    // --- 5. orbitPosition offsets from the target. ---
    {
        const vec3 target(10, 2, -4);
        const vec3 eye = orbitPosition(target, 5.0f, 0.0f, 0.0f); // +Z offset of 5
        CHECK(vnear(eye, vec3(10, 2, 1), 1e-5f), "orbit eye = target + (0,0,5)");
        // Distance from target is always the radius.
        const vec3 e2 = orbitPosition(target, 5.0f, 1.2f, 0.6f);
        const vec3 d = e2 - target;
        CHECK(near(std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z), 5.0f, 1e-4f), "orbit distance == radius");
    }

    // --- 6. Equirect direction <-> UV round-trips; poles and seam land where documented. ---
    {
        // Straight up is the top row (v=0); straight down is the bottom (v=1).
        CHECK(near(directionToEquirectUV(vec3(0, 1, 0)).y, 0.0f, 1e-5f), "up -> v=0 (top)");
        CHECK(near(directionToEquirectUV(vec3(0, -1, 0)).y, 1.0f, 1e-5f), "down -> v=1 (bottom)");
        // +Z (azimuth 0) sits at the horizontal centre u=0.5.
        CHECK(near(directionToEquirectUV(vec3(0, 0, 1)).x, 0.5f, 1e-5f), "+Z -> u=0.5");
        // Round-trip a spread of directions.
        for (int i = 0; i < 6; ++i) {
            for (int j = 1; j < 6; ++j) {
                const float az = static_cast<float>(i) * (pi / 3.0f) - pi + 0.2f;
                const float el = static_cast<float>(j) * (pi / 6.0f) - pi / 2.0f + 0.05f;
                const vec3 dir = sphericalToCartesian(1, az, el);
                const vec2 uv = directionToEquirectUV(dir);
                CHECK(uv.x >= 0.0f && uv.x <= 1.0f && uv.y >= 0.0f && uv.y <= 1.0f, "uv within [0,1]");
                CHECK(vnear(equirectUVToDirection(uv.x, uv.y), dir, 1e-4f), "uv -> dir round-trips");
            }
        }
    }

    if (g_fail == 0) {
        std::printf("sphericalcoords: OK — cardinals, round-trip, scaling, poles, orbit, equirect UV.\n");
        return 0;
    }
    std::printf("sphericalcoords: %d failure(s).\n", g_fail);
    return 1;
}
