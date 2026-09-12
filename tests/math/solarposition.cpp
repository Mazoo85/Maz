// tests/math/solarposition.cpp — verifies the astronomical solar-position model (math SolarPosition.hpp).
// Ground truths, deterministic (fixed dates, no <random>, no clock):
//   * JULIAN DATE (airtight): 2000-01-01 12:00 UTC = 2451545.0 (J2000), 1970-01-01 00:00 = 2440587.5;
//   * DECLINATION: stays within ±23.45° all year, reaches ~+23.4° at the June solstice, ~-23.4° at the
//     December solstice, and ~0° at the March equinox — the textbook seasonal swing;
//   * OVERHEAD SUN: at the equator on the equinox the Sun passes (near) the zenith at local solar noon, and
//     at latitude φ the noon Sun tops out near (90°-φ) — checked by sampling a full day and taking the max;
//   * NOON IS SOUTH: for a northern mid-latitude observer the Sun is due south (azimuth ~180°) at its daily
//     peak; sunDirection is a unit vector pointing up when the Sun is high;
//   * determinism.
#include "maz/math/SolarPosition.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

using maz::math::vec3;
using maz::math::sunPosition;
using maz::math::SunAngles;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static const double kRad2Deg = 180.0 / 3.14159265358979324;

// Max solar altitude (degrees) over a full UTC day at (lat,lon), plus the azimuth (degrees) at that peak.
static void dayPeak(int y, int mo, int d, double lat, double lon, double& outAltDeg, double& outAzDeg) {
    outAltDeg = -1e30;
    outAzDeg = 0.0;
    for (int i = 0; i <= 1440; ++i) { // one-minute steps
        const double hour = static_cast<double>(i) / 60.0;
        const SunAngles a = sunPosition(maz::math::julianDate(y, mo, d, hour), lat, lon);
        const double altDeg = static_cast<double>(a.altitude) * kRad2Deg;
        if (altDeg > outAltDeg) {
            outAltDeg = altDeg;
            outAzDeg = static_cast<double>(a.azimuth) * kRad2Deg;
        }
    }
}

int main() {
    // --- 1. Julian Date anchors (airtight). ---
    {
        CHECK(std::fabs(maz::math::julianDate(2000, 1, 1, 12.0) - 2451545.0) < 1e-9, "J2000 epoch = 2451545.0");
        CHECK(std::fabs(maz::math::julianDate(1970, 1, 1, 0.0) - 2440587.5) < 1e-9, "unix epoch = 2440587.5");
        CHECK(maz::math::julianDayNumber(2000, 1, 1) == 2451545L, "JDN(2000-01-01) = 2451545");
    }

    // --- 2. Declination: yearly bound + solstice/equinox values. ---
    {
        // Never exceeds the obliquity (~23.44°) plus a hair, sampled across a whole year.
        double worstAbs = 0.0;
        for (int day = 0; day < 366; ++day) {
            const double jd = maz::math::julianDate(2024, 1, 1, 12.0) + static_cast<double>(day);
            worstAbs = std::max(worstAbs, std::fabs(maz::math::solarDeclination(jd) * kRad2Deg));
        }
        CHECK(worstAbs < 23.45 && worstAbs > 23.3, "declination peaks at the obliquity (~23.44 deg)");

        const double decJun = maz::math::solarDeclination(maz::math::julianDate(2024, 6, 20, 12.0)) * kRad2Deg;
        const double decDec = maz::math::solarDeclination(maz::math::julianDate(2024, 12, 21, 12.0)) * kRad2Deg;
        const double decEqx = maz::math::solarDeclination(maz::math::julianDate(2024, 3, 20, 3.0)) * kRad2Deg;
        CHECK(decJun > 23.0 && decJun < 23.45, "June solstice declination ~ +23.4 deg");
        CHECK(decDec < -23.0 && decDec > -23.45, "December solstice declination ~ -23.4 deg");
        CHECK(std::fabs(decEqx) < 0.6, "March equinox declination ~ 0 deg");
    }

    // --- 3. Overhead Sun at the equator on the equinox; noon altitude ~ 90-lat elsewhere. ---
    {
        double altEq, azEq;
        dayPeak(2024, 3, 20, 0.0, 0.0, altEq, azEq);
        CHECK(altEq > 89.0, "equinox noon Sun is ~overhead at the equator");

        double alt40, az40;
        dayPeak(2024, 3, 20, 40.0, 0.0, alt40, az40);
        CHECK(std::fabs(alt40 - 50.0) < 1.5, "equinox noon altitude at 40N is ~ 90-40 = 50 deg");
        // At its daily peak in the northern hemisphere the Sun is due south (~180 deg).
        CHECK(std::fabs(az40 - 180.0) < 2.0, "the noon Sun is due south for a northern observer");
    }

    // --- 4. sunDirection is a unit vector; points up when the Sun is high. ---
    {
        double alt, az;
        dayPeak(2024, 6, 21, 20.0, 0.0, alt, az); // high summer sun in the tropics
        (void)az;
        const SunAngles peak = sunPosition(maz::math::julianDate(2024, 6, 21, 12.0), 20.0, 0.0);
        const vec3 d = maz::math::sunDirection(peak);
        const float mag = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        CHECK(std::fabs(mag - 1.0f) < 1e-4f, "sunDirection is a unit vector");
        // Around local noon in June at 20N the Sun is high => strong +Y (up) component.
        CHECK(d.y > 0.6f, "the midday summer Sun direction points strongly upward");
    }

    // --- 5. Determinism. ---
    {
        const SunAngles a = sunPosition(2451545.0, 51.5, -0.12);
        const SunAngles b = sunPosition(2451545.0, 51.5, -0.12);
        CHECK(a.altitude == b.altitude && a.azimuth == b.azimuth, "identical inputs produce identical results");
    }

    if (g_fail == 0) {
        std::printf("solarposition: OK — Julian date, declination swing, overhead/noon geometry, direction.\n");
        return 0;
    }
    std::printf("solarposition: %d failure(s).\n", g_fail);
    return 1;
}
