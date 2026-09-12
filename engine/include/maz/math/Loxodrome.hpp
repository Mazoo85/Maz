#pragma once

#include "maz/math/Math.hpp" // vec3

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math loxodrome (rhumb line) — the path across a sphere that holds a CONSTANT compass bearing, crossing
// every meridian at the same angle. It is the "steady heading" route a ship or plane follows when it just
// keeps the compass pinned (as opposed to the great-circle shortest path in GreatCircle.hpp, whose heading
// constantly changes). On a Mercator map a rhumb line is a straight line; on the globe it spirals toward the
// pole. Use it for navigation/strategy/globe games, constant-heading travel, or drawing rhumb spirals. Godot
// has no such helper. Latitudes/longitudes are in radians; bearing is measured clockwise from north
// (N=0, E=π/2). Header-only, std-only, deterministic.
namespace maz::math {

struct LatLon {
    float lat = 0.0f; // radians, +north
    float lon = 0.0f; // radians, +east
};

// A unit-sphere position (y-up) for a lat/lon: north pole is +Y, (lat0,lon0) maps by the usual spherical rule.
inline vec3 latLonToUnit(const LatLon& p) {
    const float cl = std::cos(p.lat);
    return vec3(cl * std::cos(p.lon), std::sin(p.lat), cl * std::sin(p.lon));
}

// The point reached by travelling `distance` (in units of the sphere radius `radius`) along a rhumb line of
// constant `bearing` from `start`. Handles the east/west degenerate case (bearing = ±90°) as a parallel.
inline LatLon loxodromePoint(const LatLon& start, float bearing, float distance, float radius = 1.0f) {
    const float pi = 3.14159265358979324f;
    const float cb = std::cos(bearing);
    const float sb = std::sin(bearing);
    LatLon out;
    if (std::fabs(cb) < 1e-6f) {
        // Due east/west: latitude is unchanged, longitude advances along the parallel.
        out.lat = start.lat;
        out.lon = start.lon + distance * sb / (radius * std::cos(start.lat));
        return out;
    }
    out.lat = start.lat + distance * cb / radius;
    // Clamp just shy of the poles to keep the Mercator term finite.
    const float lim = pi * 0.5f - 1e-4f;
    if (out.lat > lim) {
        out.lat = lim;
    }
    if (out.lat < -lim) {
        out.lat = -lim;
    }
    const float dPsi = std::log(std::tan(pi * 0.25f + out.lat * 0.5f) /
                                std::tan(pi * 0.25f + start.lat * 0.5f)); // Mercator ordinate difference
    out.lon = start.lon + std::tan(bearing) * dPsi;
    return out;
}

// A polyline of `samples + 1` lat/lon points sampling the rhumb line from `start` over `totalDistance`.
inline std::vector<LatLon> loxodromePolyline(const LatLon& start, float bearing, float totalDistance,
                                             int samples, float radius = 1.0f) {
    if (samples < 1) {
        samples = 1;
    }
    std::vector<LatLon> out;
    out.reserve(static_cast<std::size_t>(samples + 1));
    for (int i = 0; i <= samples; ++i) {
        const float d = totalDistance * static_cast<float>(i) / static_cast<float>(samples);
        out.push_back(loxodromePoint(start, bearing, d, radius));
    }
    return out;
}

} // namespace maz::math
