#pragma once

#include "maz/math/Math.hpp"      // vec3
#include "maz/math/VectorOps.hpp" // (vec3 ops)

#include <cmath>

// maz::math astronomical solar position — where the real Sun is in the sky for a given calendar date/time and
// place on Earth. The existing game::DayNightCycle is only an abstract 0..1 clock with a cosine "elevation";
// this computes the ACTUAL solar altitude and azimuth (and a world-space light direction) from a UTC date,
// latitude and longitude, so a day/night cycle can be geographically and seasonally correct — long low-angle
// winter sun, short high summer sun, sunrise swinging north of east in June, the works. Uses the standard
// low-precision solar model (accurate to ~0.01° for the Sun's declination/right-ascension over 1950–2050),
// which is far better than any game needs. Angles are radians unless a name ends in `Deg`. Godot ships no
// such helper. Header-only, std-only, deterministic.
namespace maz::math {

// --- Julian Day: the continuous day count astronomers reckon time by. ---

// The Julian Day Number for the given Gregorian calendar date at midnight, as an integer count.
inline long julianDayNumber(int year, int month, int day) {
    const long a = (14 - month) / 12;
    const long y = year + 4800 - a;
    const long m = month + 12 * a - 3;
    return day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
}

// The (fractional) Julian Date for a Gregorian date and UTC time-of-day in hours [0,24). At 2000-01-01
// 12:00 UTC this is exactly 2451545.0 (the J2000 epoch); at 1970-01-01 00:00 it is 2440587.5.
inline double julianDate(int year, int month, int day, double hourUtc) {
    return static_cast<double>(julianDayNumber(year, month, day)) + (hourUtc - 12.0) / 24.0;
}

// --- Sun position ---

struct SunAngles {
    float altitude = 0.0f; // radians above the horizon (negative = below / night)
    float azimuth = 0.0f;  // radians, measured clockwise from due north (N=0, E=π/2, S=π, W=3π/2)
};

// The Sun's declination (its latitude on the celestial sphere) in radians for the given Julian Date. Swings
// between about ±23.44° over the year: positive near the June solstice, negative near December, ~0 at the
// equinoxes.
inline double solarDeclination(double jd) {
    const double n = jd - 2451545.0;                            // days since J2000
    const double deg = 3.14159265358979324 / 180.0;
    const double L = (280.460 + 0.9856474 * n) * deg;           // mean longitude
    const double g = (357.528 + 0.9856003 * n) * deg;           // mean anomaly
    const double lambda = L + (1.915 * std::sin(g) + 0.020 * std::sin(2.0 * g)) * deg; // ecliptic longitude
    const double eps = (23.439 - 0.0000004 * n) * deg;          // obliquity of the ecliptic
    return std::asin(std::sin(eps) * std::sin(lambda));
}

// The Sun's altitude and azimuth for a Julian Date and an observer at (latitudeDeg north, longitudeDeg east).
inline SunAngles sunPosition(double jd, double latitudeDeg, double longitudeDeg) {
    const double deg = 3.14159265358979324 / 180.0;
    const double n = jd - 2451545.0;
    const double L = (280.460 + 0.9856474 * n) * deg;
    const double g = (357.528 + 0.9856003 * n) * deg;
    const double lambda = L + (1.915 * std::sin(g) + 0.020 * std::sin(2.0 * g)) * deg;
    const double eps = (23.439 - 0.0000004 * n) * deg;

    // Right ascension and declination.
    const double ra = std::atan2(std::cos(eps) * std::sin(lambda), std::cos(lambda));
    const double dec = std::asin(std::sin(eps) * std::sin(lambda));

    // Greenwich mean sidereal time (hours) -> local hour angle.
    double gmstHours = std::fmod(18.697374558 + 24.06570982441908 * n, 24.0);
    if (gmstHours < 0.0) {
        gmstHours += 24.0;
    }
    const double lstRad = gmstHours * 15.0 * deg + longitudeDeg * deg; // local sidereal time
    const double H = lstRad - ra;                                      // hour angle

    const double lat = latitudeDeg * deg;
    const double sinAlt = std::sin(lat) * std::sin(dec) + std::cos(lat) * std::cos(dec) * std::cos(H);
    const double alt = std::asin(sinAlt < -1.0 ? -1.0 : (sinAlt > 1.0 ? 1.0 : sinAlt));

    // Azimuth measured clockwise from north.
    const double y = -std::cos(dec) * std::sin(H);
    const double x = std::sin(dec) * std::cos(lat) - std::cos(dec) * std::sin(lat) * std::cos(H);
    double az = std::atan2(y, x);
    if (az < 0.0) {
        az += 2.0 * 3.14159265358979324;
    }

    SunAngles out;
    out.altitude = static_cast<float>(alt);
    out.azimuth = static_cast<float>(az);
    return out;
}

// A world-space unit vector pointing FROM the surface TOWARD the Sun, for lighting a scene where +Y is up,
// +X is east and +Z is south (so azimuth from north rotates from -Z toward +X). Negate it for the light's
// travel direction.
inline vec3 sunDirection(const SunAngles& a) {
    const float ca = std::cos(a.altitude);
    return vec3(ca * std::sin(a.azimuth),   // east component
                std::sin(a.altitude),       // up
                -ca * std::cos(a.azimuth));  // north is -Z, so +cos(az) points north => negate for +Z south
}

} // namespace maz::math
