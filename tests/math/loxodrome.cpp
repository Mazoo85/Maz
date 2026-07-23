// tests/math/loxodrome.cpp — verifies the loxodrome / rhumb line (math Loxodrome.hpp).
// Ground truths, deterministic (fixed parameters, no <random>, no clock):
//   * CONSTANT BEARING (airtight, the defining property): measured on the sphere via local north/east frames
//     and finite differences, the heading is the SAME at every point along the path and equals the input
//     bearing;
//   * DEGENERATE HEADINGS: bearing 0 stays on one meridian (longitude constant); bearing 90° stays on one
//     parallel (latitude constant);
//   * DISTANCE: the traversed path length (summed on the unit sphere) equals the requested travel distance;
//   * unit-sphere mapping; determinism.
#include "maz/math/Loxodrome.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

using maz::math::vec3;
using maz::math::LatLon;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len(const vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
static float dot3(const vec3& a, const vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// Local north/east unit vectors on the unit sphere at (lat,lon), y-up.
static vec3 northAt(const LatLon& p) {
    return vec3(-std::sin(p.lat) * std::cos(p.lon), std::cos(p.lat), -std::sin(p.lat) * std::sin(p.lon));
}
static vec3 eastAt(const LatLon& p) { return vec3(-std::sin(p.lon), 0.0f, std::cos(p.lon)); }

int main() {
    const float pi = 3.14159265358979324f;
    const float deg = pi / 180.0f;

    // --- 1. Constant bearing along the path (measured independently on the sphere). ---
    {
        for (float bDeg : {15.0f, 30.0f, 60.0f, -40.0f}) {
            const float b = bDeg * deg;
            const LatLon start{10.0f * deg, 20.0f * deg};
            float worst = 0.0f;
            const float ds = 1e-3f;
            // Keep the sampled distance short so the path never runs up to the pole clamp (which would
            // otherwise flatten the latitude and corrupt the measured heading).
            for (int i = 1; i < 200; ++i) {
                const float s = static_cast<float>(i) * 0.005f;
                const LatLon p0 = maz::math::loxodromePoint(start, b, s);
                const LatLon p1 = maz::math::loxodromePoint(start, b, s + ds);
                const vec3 d = maz::math::latLonToUnit(p1) - maz::math::latLonToUnit(p0);
                const float n = dot3(d, northAt(p0));
                const float e = dot3(d, eastAt(p0));
                const float measured = std::atan2(e, n);
                float diff = measured - b;
                while (diff > pi) diff -= 2.0f * pi;
                while (diff < -pi) diff += 2.0f * pi;
                worst = std::max(worst, std::fabs(diff));
            }
            CHECK(worst < 2e-3f, "the measured heading is constant and equals the input bearing");
        }
    }

    // --- 2. Degenerate headings. ---
    {
        const LatLon start{5.0f * deg, -30.0f * deg};
        // Due north: longitude stays constant, latitude increases.
        float worstLon = 0.0f;
        LatLon prevN = start;
        for (int i = 1; i <= 50; ++i) {
            const LatLon p = maz::math::loxodromePoint(start, 0.0f, static_cast<float>(i) * 0.01f);
            worstLon = std::max(worstLon, std::fabs(p.lon - start.lon));
            CHECK(p.lat > prevN.lat, "due-north bearing increases latitude");
            prevN = p;
        }
        CHECK(worstLon < 1e-5f, "due-north bearing keeps longitude constant (a meridian)");
        // Due east: latitude stays constant, longitude increases.
        float worstLat = 0.0f;
        LatLon prevE = start;
        for (int i = 1; i <= 50; ++i) {
            const LatLon p = maz::math::loxodromePoint(start, pi * 0.5f, static_cast<float>(i) * 0.01f);
            worstLat = std::max(worstLat, std::fabs(p.lat - start.lat));
            CHECK(p.lon > prevE.lon, "due-east bearing increases longitude");
            prevE = p;
        }
        CHECK(worstLat < 1e-5f, "due-east bearing keeps latitude constant (a parallel)");
    }

    // --- 3. Distance: the path length equals the requested distance. ---
    {
        const LatLon start{-20.0f * deg, 100.0f * deg};
        const float b = 35.0f * deg, D = 1.2f;
        // Closed form (airtight): the rhumb length from lat0 to lat is |lat-lat0|/cos(bearing).
        const LatLon endp = maz::math::loxodromePoint(start, b, D);
        const float closed = std::fabs(endp.lat - start.lat) / std::cos(b);
        CHECK(std::fabs(closed - D) < 1e-3f, "closed-form rhumb length |dlat|/cos(bearing) equals the distance");
        // Independent confirmation: a double-precision arc-length sum. The per-step angle is computed with the
        // spherical law of cosines entirely in double (no float unit-vector intermediates, which would drown
        // acos(~1) in rounding noise), and a moderate step count keeps acos well-conditioned.
        auto angle = [](const LatLon& p, const LatLon& q) {
            const double la = p.lat, lb = q.lat;
            double cd = std::sin(la) * std::sin(lb) +
                        std::cos(la) * std::cos(lb) * std::cos(static_cast<double>(p.lon) - q.lon);
            cd = cd > 1.0 ? 1.0 : (cd < -1.0 ? -1.0 : cd);
            return std::acos(cd);
        };
        const int N = 4000;
        double total = 0.0;
        LatLon prev = start;
        for (int i = 1; i <= N; ++i) {
            const LatLon cur = maz::math::loxodromePoint(start, b, D * static_cast<float>(i) / static_cast<float>(N));
            total += angle(prev, cur);
            prev = cur;
        }
        CHECK(std::fabs(total - static_cast<double>(D)) < 3e-3, "the rhumb-line path length equals the distance");
    }

    // --- 4. Unit-sphere mapping + determinism. ---
    {
        const LatLon p{40.0f * deg, 12.0f * deg};
        CHECK(std::fabs(len(maz::math::latLonToUnit(p)) - 1.0f) < 1e-6f, "latLonToUnit returns a unit vector");
        const LatLon eq{0.0f, 0.0f};
        const vec3 v = maz::math::latLonToUnit(eq);
        CHECK(len(v - vec3(1, 0, 0)) < 1e-6f, "lat=0,lon=0 maps to +X");
        const LatLon a = maz::math::loxodromePoint(p, 25.0f * deg, 0.7f);
        const LatLon b2 = maz::math::loxodromePoint(p, 25.0f * deg, 0.7f);
        CHECK(a.lat == b2.lat && a.lon == b2.lon, "identical inputs produce identical results");
    }

    if (g_fail == 0) {
        std::printf("loxodrome: OK — constant bearing, meridian/parallel limits, distance, determinism.\n");
        return 0;
    }
    std::printf("loxodrome: %d failure(s).\n", g_fail);
    return 1;
}
