#pragma once

#include "maz/math/Math.hpp" // vec3

#include <cmath>

// maz::math optics — refraction (Snell's law) and Fresnel reflectance, the light-bending math behind
// water, glass, gems, and physically-based rendering. The engine already has `reflect` (VectorOps), the
// mirror half; this adds the transmission half. `refract` bends an incident direction as it crosses a
// surface between media with a given index-of-refraction ratio, and returns the zero vector on TOTAL
// INTERNAL REFLECTION (past the critical angle, e.g. looking up through water from below). The
// Fresnel-Schlick term gives the fraction of light that REFLECTS versus transmits as a function of
// viewing angle — near-zero (its base reflectance f0) head-on, rising to 1 at grazing angles (the bright
// rim you see on water and glass edges), which every PBR shader multiplies its specular by. Pure vec3
// math — exactly unit-testable against Snell's law and known Fresnel limits.
namespace maz::math {

// Refract incident direction `i` (unit, pointing toward the surface) across normal `n` (unit) with
// index-of-refraction ratio `eta` = n_incident / n_transmitted. Returns the (unit) refracted direction,
// or the zero vector on total internal reflection. Matches GLSL's refract().
inline vec3 refract(const vec3& i, const vec3& n, float eta) {
    const float ni = i.x * n.x + i.y * n.y + i.z * n.z;
    const float k = 1.0f - eta * eta * (1.0f - ni * ni);
    if (k < 0.0f) {
        return vec3(0.0f, 0.0f, 0.0f); // total internal reflection
    }
    const float s = eta * ni + std::sqrt(k);
    return vec3(eta * i.x - s * n.x, eta * i.y - s * n.y, eta * i.z - s * n.z);
}

// True if the given configuration undergoes total internal reflection (no transmitted ray).
inline bool isTotalInternalReflection(const vec3& i, const vec3& n, float eta) {
    const float ni = i.x * n.x + i.y * n.y + i.z * n.z;
    return (1.0f - eta * eta * (1.0f - ni * ni)) < 0.0f;
}

// Base reflectance at normal incidence (f0) for a dielectric interface between two indices of
// refraction, e.g. air (1.0) -> glass (1.5) gives ~0.04.
inline float fresnelF0(float n1, float n2) {
    const float r = (n1 - n2) / (n1 + n2);
    return r * r;
}

// Fresnel-Schlick reflectance at a given cos(theta) between the view/light and the normal, from base
// reflectance f0. f0 at head-on (cos=1), rising to 1 at grazing (cos=0).
inline float fresnelSchlick(float cosTheta, float f0) {
    float m = 1.0f - cosTheta;
    if (m < 0.0f) {
        m = 0.0f;
    }
    if (m > 1.0f) {
        m = 1.0f;
    }
    const float m2 = m * m;
    const float m5 = m2 * m2 * m;
    return f0 + (1.0f - f0) * m5;
}

// Per-channel Fresnel-Schlick (colored f0, e.g. metals).
inline vec3 fresnelSchlick(float cosTheta, const vec3& f0) {
    return vec3(fresnelSchlick(cosTheta, f0.x), fresnelSchlick(cosTheta, f0.y),
                fresnelSchlick(cosTheta, f0.z));
}

} // namespace maz::math
