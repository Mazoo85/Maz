#pragma once

#include "maz/math/Math.hpp" // vec3

#include <cmath>

// maz::math swept-sphere continuous collision — the time-of-impact of a MOVING sphere against a plane.
// Discrete collision (test the sphere where it lands each frame) tunnels through thin/fast geometry: a
// bullet or bouncing ball moving faster than its own radius per step can start one side of a wall and
// end the other with no overlap ever sampled. Continuous collision solves for the exact fraction t of
// the step at which the sphere first touches the plane, so you can advance to the contact and respond.
// This is the plane case (walls, floors, ground) — the primitive behind CCD, ball physics, and
// projectile-vs-surface. The engine has a 2D swept circle (ShapeCast2D) and a conservative 3D sphere
// cast; this adds the exact analytic 3D sphere-vs-plane sweep (Ericson, Real-Time Collision Detection).
// Pure vec3 math — exactly unit-testable against hand-computed impact times.
namespace maz::math {

struct SphereSweepHit {
    bool hit = false;
    float t = 0.0f;               // fraction of the motion [0,1] at first contact (0 if already touching)
    vec3 point{0.0f, 0.0f, 0.0f}; // contact point on the plane
};

// Sweep a sphere (center `c`, radius `r`) moving by displacement `vel` over one step against the plane
// dot(n, x) = planeD (n unit). Returns the first-contact fraction within [0,1], or hit=false if the
// sphere does not reach the plane this step (or is moving away).
inline SphereSweepHit sweepSpherePlane(const vec3& c, float r, const vec3& vel, const vec3& n,
                                       float planeD) {
    const float dist = (n.x * c.x + n.y * c.y + n.z * c.z) - planeD; // signed distance of the center
    SphereSweepHit out;
    if (std::fabs(dist) <= r) {
        // Already intersecting at the start of the step.
        out.hit = true;
        out.t = 0.0f;
        out.point = vec3(c.x - n.x * dist, c.y - n.y * dist, c.z - n.z * dist);
        return out;
    }
    const float denom = n.x * vel.x + n.y * vel.y + n.z * vel.z; // motion along the normal
    if (denom * dist >= 0.0f) {
        return out; // moving away from (or parallel to) the plane — no contact
    }
    const float rSigned = dist > 0.0f ? r : -r; // the sphere surface on its approach side
    const float t = (rSigned - dist) / denom;
    if (t < 0.0f || t > 1.0f) {
        return out; // contact happens outside this motion step
    }
    out.hit = true;
    out.t = t;
    // Center at contact, minus the radius along the approach-side normal = the touch point on the plane.
    const vec3 cc(c.x + vel.x * t, c.y + vel.y * t, c.z + vel.z * t);
    out.point = vec3(cc.x - n.x * rSigned, cc.y - n.y * rSigned, cc.z - n.z * rSigned);
    return out;
}

} // namespace maz::math
