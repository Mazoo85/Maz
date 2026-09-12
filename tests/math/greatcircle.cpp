// tests/math/greatcircle.cpp — verifies great-circle / spherical geometry (math::haversineCentralAngle,
// latLonToUnit, angleBetweenUnit, slerpUnit, greatCircleDistance). Ground truths, exact known angles:
//   * pole to pole is pi; an equator quarter (0,0)->(0,90deg) is pi/2; a point to itself is 0;
//   * distance = radius * central angle;
//   * lat/lon -> unit vector matches the +Y-up convention, and haversine agrees with the angle between
//     the corresponding unit vectors;
//   * slerpUnit endpoints are exact, its midpoint is unit length and half the total angle from each end;
//   * a great-circle intermediate point stays on the unit sphere.
#include "maz/math/GreatCircle.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::angleBetweenUnit;
using maz::math::greatCircleDistance;
using maz::math::greatCirclePoint;
using maz::math::haversineCentralAngle;
using maz::math::latLonToUnit;
using maz::math::slerpUnit;
using maz::math::vec3;

static const float kPi = 3.14159265358979324f;
static bool near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }
static bool vnear(const vec3& a, const vec3& b, float e = 1e-4f) {
    return std::fabs(a.x - b.x) < e && std::fabs(a.y - b.y) < e && std::fabs(a.z - b.z) < e;
}
static float len(const vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

int main() {
    const float halfPi = kPi * 0.5f;

    // --- 1. Central-angle known values. ---
    {
        CHECK(near(haversineCentralAngle(halfPi, 0, -halfPi, 0), kPi), "pole to pole is pi");
        CHECK(near(haversineCentralAngle(0, 0, 0, halfPi), halfPi), "equator quarter is pi/2");
        CHECK(near(haversineCentralAngle(0.3f, 1.1f, 0.3f, 1.1f), 0.0f), "same point is 0");
    }

    // --- 2. Distance = radius * angle. ---
    {
        const float r = 6371.0f; // km-ish
        CHECK(near(greatCircleDistance(0, 0, 0, halfPi, r), r * halfPi, 1e-1f),
              "distance scales with radius");
    }

    // --- 3. lat/lon -> unit vector convention (+Y up). ---
    {
        CHECK(vnear(latLonToUnit(0, 0), vec3(1, 0, 0)), "(0,0) -> +X");
        CHECK(vnear(latLonToUnit(halfPi, 0), vec3(0, 1, 0)), "north pole -> +Y");
        CHECK(vnear(latLonToUnit(0, halfPi), vec3(0, 0, 1)), "(0,90deg) -> +Z");
        CHECK(near(len(latLonToUnit(0.7f, -1.2f)), 1.0f), "always unit length");
    }

    // --- 4. Haversine agrees with the angle between unit vectors. ---
    {
        const float lat1 = 0.4f, lon1 = 0.9f, lat2 = -0.6f, lon2 = 2.0f;
        const float hav = haversineCentralAngle(lat1, lon1, lat2, lon2);
        const float dot = angleBetweenUnit(latLonToUnit(lat1, lon1), latLonToUnit(lat2, lon2));
        CHECK(near(hav, dot, 1e-3f), "haversine == angle between the unit vectors");
    }

    // --- 5. SLERP endpoints, midpoint. ---
    {
        const vec3 a = latLonToUnit(0, 0);          // +X
        const vec3 b = latLonToUnit(0, halfPi);     // +Z, 90deg away
        CHECK(vnear(slerpUnit(a, b, 0.0f), a), "slerp(0) = a");
        CHECK(vnear(slerpUnit(a, b, 1.0f), b), "slerp(1) = b");
        const vec3 mid = slerpUnit(a, b, 0.5f);
        CHECK(near(len(mid), 1.0f), "slerp midpoint stays on the unit sphere");
        CHECK(near(angleBetweenUnit(a, mid), halfPi * 0.5f), "midpoint is half the arc from a");
        CHECK(near(angleBetweenUnit(mid, b), halfPi * 0.5f), "and half the arc from b");
        // The 45-degree point between +X and +Z is (cos45, 0, sin45).
        const float c45 = std::cos(halfPi * 0.5f);
        CHECK(vnear(mid, vec3(c45, 0, c45)), "midpoint is the 45-degree direction");
    }

    // --- 6. Great-circle intermediate point stays on the sphere. ---
    {
        for (int k = 0; k <= 10; ++k) {
            const float f = static_cast<float>(k) / 10.0f;
            const vec3 p = greatCirclePoint(0.2f, 0.1f, -0.5f, 2.5f, f);
            CHECK(near(len(p), 1.0f, 1e-3f), "great-circle point is unit length");
        }
    }

    if (g_fail == 0) {
        std::printf("greatcircle: OK — central angle, distance, lat/lon convention, haversine==angle, "
                    "slerp, on-sphere.\n");
        return 0;
    }
    std::printf("greatcircle: %d failure(s).\n", g_fail);
    return 1;
}
