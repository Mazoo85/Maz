#pragma once

#include "maz/math/Math.hpp" // vec3

#include <algorithm>
#include <cmath>

// maz::math great-circle / spherical geometry — distances and shortest paths ON a sphere, for planet
// and globe games, star/sky-dome positions, orbital/satellite tracks, and "fly the shortest route
// between two map points." A straight line through 3D space is not the shortest path along a spherical
// surface; the great-circle (the arc of the plane through both points and the sphere's center) is. This
// provides the haversine central-angle/distance between two lat/lon points (numerically stable for both
// tiny and antipodal separations), lat/lon <-> unit-vector conversion, the angle between unit vectors,
// and unit-vector SLERP so you can walk the great-circle arc at a uniform rate. Pure trig over floats —
// exactly unit-testable against known angles (pole-to-pole = pi, equator quarter = pi/2).
namespace maz::math {

// The central angle (in radians) between two lat/lon points via the haversine formula. Latitude and
// longitude are in radians. Multiply by the sphere radius for surface distance.
inline float haversineCentralAngle(float lat1, float lon1, float lat2, float lon2) {
    const float dLat = lat2 - lat1;
    const float dLon = lon2 - lon1;
    const float sinLat = std::sin(dLat * 0.5f);
    const float sinLon = std::sin(dLon * 0.5f);
    const float a = sinLat * sinLat + std::cos(lat1) * std::cos(lat2) * sinLon * sinLon;
    const float clamped = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a);
    return 2.0f * std::asin(std::sqrt(clamped));
}

// Great-circle surface distance between two lat/lon points on a sphere of the given radius.
inline float greatCircleDistance(float lat1, float lon1, float lat2, float lon2, float radius) {
    return radius * haversineCentralAngle(lat1, lon1, lat2, lon2);
}

// Convert latitude/longitude (radians) to a point on the unit sphere. Convention: +Y is the north pole,
// (lat=0, lon=0) maps to +X, longitude sweeps toward +Z.
inline vec3 latLonToUnit(float lat, float lon) {
    const float cl = std::cos(lat);
    return vec3(cl * std::cos(lon), std::sin(lat), cl * std::sin(lon));
}

// Angle (radians) between two (assumed unit) vectors — the central angle of their great circle.
inline float angleBetweenUnit(const vec3& a, const vec3& b) {
    const float d = a.x * b.x + a.y * b.y + a.z * b.z;
    const float clamped = d < -1.0f ? -1.0f : (d > 1.0f ? 1.0f : d);
    return std::acos(clamped);
}

// Spherical linear interpolation of two unit vectors — walks the great-circle arc from a to b as t goes
// 0 -> 1, keeping the result on the unit sphere and moving at a constant angular rate.
inline vec3 slerpUnit(const vec3& a, const vec3& b, float t) {
    const float d = a.x * b.x + a.y * b.y + a.z * b.z;
    const float clamped = d < -1.0f ? -1.0f : (d > 1.0f ? 1.0f : d);
    const float omega = std::acos(clamped);
    const float sinO = std::sin(omega);
    if (sinO < 1e-5f) {
        // Nearly parallel: fall back to normalized linear interpolation.
        const vec3 r(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
        const float len = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z);
        return len > 0.0f ? vec3(r.x / len, r.y / len, r.z / len) : a;
    }
    const float wa = std::sin((1.0f - t) * omega) / sinO;
    const float wb = std::sin(t * omega) / sinO;
    return vec3(a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb);
}

// Intermediate point at fraction f along the great circle between two lat/lon points (as a unit vector).
inline vec3 greatCirclePoint(float lat1, float lon1, float lat2, float lon2, float f) {
    return slerpUnit(latLonToUnit(lat1, lon1), latLonToUnit(lat2, lon2), f);
}

} // namespace maz::math
