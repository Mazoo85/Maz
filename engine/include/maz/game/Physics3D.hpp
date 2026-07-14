#pragma once

#include "maz/game/CollisionLayers.hpp"
#include "maz/math/Math.hpp"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace maz::game {

// Impulse-based 3D rigid-body dynamics — the 3D sibling of Physics2D. This module resolves collisions
// (it doesn't just report them). Bodies carry a position, linear velocity, an inverse mass (0 =
// immovable/infinite mass), restitution (bounciness) and friction; the world integrates gravity with
// semi-implicit Euler, detects contacts, and resolves them with a rotation-aware normal impulse +
// Coulomb friction impulse + a positional correction (so resting bodies don't sink). Deterministic
// under a fixed timestep and free of GPU/RNG, so it unit-tests headlessly.
//
// D1 shipped dynamic Spheres + a static ground Plane with linear impulses. D2 (this milestone) adds
// Box (OBB) shapes and full angular dynamics: an inverse-inertia tensor per body, quaternion
// orientation integration, and contact impulses applied at the contact point (lever arms), so a
// tilted box dropped on the ground tumbles and settles flat, and a sphere landing off-centre on a box
// imparts spin. Rotation is opt-in via enableRotation() — a body left with a zero inverse-inertia
// tensor behaves exactly like the D1 translation-only body, so every D1 result is unchanged.
// Box-vs-box contact (3D SAT) and warm-started stacking arrive in later milestones.

struct Body3D {
    // A Sphere is a point + `radius`. A Box is an oriented box (Godot BoxShape3D) with per-axis
    // half-extents `half`, rotated by `orientation`. A Capsule is a segment along the body's LOCAL Y
    // axis (half-length `half.y`) swept by `radius` (Godot CapsuleShape3D), rotated by `orientation` —
    // the standard character/prop shape. A Plane is an infinite static half-space (Godot
    // WorldBoundaryShape3D): `normal` is the unit outward normal (toward the free space where bodies
    // live) and `planeD` the plane offset, so the solid region is { p : dot(p, normal) < planeD };
    // build one with makeGroundPlane().
    enum Shape { Sphere, Box, Plane, Capsule };

    math::vec3 pos{0.0f, 0.0f, 0.0f};
    math::vec3 vel{0.0f, 0.0f, 0.0f};
    int shape = Sphere;
    float radius = 0.5f;                 // used when shape == Sphere
    math::vec3 half{0.5f, 0.5f, 0.5f};   // half-extents when shape == Box
    math::vec3 normal{0.0f, 1.0f, 0.0f}; // unit outward normal when shape == Plane
    float planeD = 0.0f;                 // plane offset when shape == Plane

    float invMass = 1.0f;     // 0 => static (infinite mass, never moves)
    float restitution = 0.3f; // 0 = inelastic, 1 = perfectly bouncy
    float friction = 0.5f;    // Coulomb coefficient (0 = frictionless)
    float linearDamping = 0.0f;  // per-second velocity decay (0 => none), like Godot linear_damp
    float angularDamping = 0.05f; // per-second spin decay, so tumbling bodies eventually rest

    // --- Rotation (opt-in via enableRotation) ------------------------------------------------------
    // A body carries an orientation and spin, but rotation is locked by default: invInertiaLocal is the
    // zero matrix (infinite moment of inertia), so contact impulses produce no torque and the body
    // behaves exactly like a D1 translation-only body. enableRotation() fills the local inverse-inertia
    // tensor from the shape + mass; the solver rotates it into world space each step.
    math::quat orientation{1.0f, 0.0f, 0.0f, 0.0f}; // w,x,y,z
    math::vec3 angularVel{0.0f, 0.0f, 0.0f};
    math::mat3 invInertiaLocal{0.0f}; // zero => rotation locked

    // --- Sleeping (opt-in via PhysicsWorld3D::allowSleep) ------------------------------------------
    // A body quiet (below the world's linear + angular thresholds) for long enough goes to sleep: it
    // stops integrating and solving and acts as a static obstacle until an island-mate wakes it (Godot
    // can_sleep / sleeping). sleepTimer accumulates quiet time; sleeping is the current state.
    float sleepTimer = 0.0f;
    bool sleeping = false;

    // Give the body a finite moment of inertia derived from its shape + mass, letting it spin.
    void enableRotation() {
        invInertiaLocal = math::mat3(0.0f);
        if (invMass <= 0.0f) {
            return; // static: stays locked
        }
        const float m = 1.0f / invMass;
        if (shape == Sphere) {
            const float I = 0.4f * m * radius * radius; // solid sphere 2/5 m r^2
            const float inv = I > 0.0f ? 1.0f / I : 0.0f;
            invInertiaLocal[0][0] = inv;
            invInertiaLocal[1][1] = inv;
            invInertiaLocal[2][2] = inv;
        } else if (shape == Box) {
            // Solid box: I_xx = (1/3) m (hy^2 + hz^2) for half-extents h (= (1/12) m (sy^2+sz^2)).
            const float ix = (1.0f / 3.0f) * m * (half.y * half.y + half.z * half.z);
            const float iy = (1.0f / 3.0f) * m * (half.x * half.x + half.z * half.z);
            const float iz = (1.0f / 3.0f) * m * (half.x * half.x + half.y * half.y);
            invInertiaLocal[0][0] = ix > 0.0f ? 1.0f / ix : 0.0f;
            invInertiaLocal[1][1] = iy > 0.0f ? 1.0f / iy : 0.0f;
            invInertiaLocal[2][2] = iz > 0.0f ? 1.0f / iz : 0.0f;
        } else if (shape == Capsule) {
            // Approximated as a cylinder about the local Y axis (radius r, length L = 2*half.y): exact
            // enough for gameplay; the caps add a little inertia we fold into the cylinder terms.
            const float r = radius;
            const float L = 2.0f * half.y;
            const float iaxis = 0.5f * m * r * r;                     // about Y (the long axis)
            const float iperp = (1.0f / 12.0f) * m * (3.0f * r * r + L * L); // about X and Z
            invInertiaLocal[0][0] = iperp > 0.0f ? 1.0f / iperp : 0.0f;
            invInertiaLocal[1][1] = iaxis > 0.0f ? 1.0f / iaxis : 0.0f;
            invInertiaLocal[2][2] = iperp > 0.0f ? 1.0f / iperp : 0.0f;
        }
    }

    // --- Collision filtering (Godot collision_layer / collision_mask) ------------------------------
    LayerMask collisionLayer = ~0u;
    LayerMask collisionMask = ~0u;
};

// Build a dynamic sphere body at `pos` with the given radius and mass (mass <= 0 => static).
inline Body3D makeSphere(math::vec3 pos, float radius, float mass = 1.0f) {
    Body3D b;
    b.shape = Body3D::Sphere;
    b.pos = pos;
    b.radius = radius;
    b.invMass = mass > 0.0f ? 1.0f / mass : 0.0f;
    return b;
}

// Build a dynamic box body at `pos` with the given half-extents and mass (mass <= 0 => static).
inline Body3D makeBox(math::vec3 pos, math::vec3 half, float mass = 1.0f) {
    Body3D b;
    b.shape = Body3D::Box;
    b.pos = pos;
    b.half = half;
    b.invMass = mass > 0.0f ? 1.0f / mass : 0.0f;
    return b;
}

// Build a dynamic capsule at `pos`: a segment along local Y of half-length `halfHeight` swept by
// `radius` (Godot CapsuleShape3D), with the given mass (mass <= 0 => static).
inline Body3D makeCapsule(math::vec3 pos, float radius, float halfHeight, float mass = 1.0f) {
    Body3D b;
    b.shape = Body3D::Capsule;
    b.pos = pos;
    b.radius = radius;
    b.half = math::vec3(radius, halfHeight, radius);
    b.invMass = mass > 0.0f ? 1.0f / mass : 0.0f;
    return b;
}

// Build a static infinite ground plane (half-space) from an outward unit normal and any point on it —
// Godot WorldBoundaryShape3D. Bodies live on the +normal side; the solid fills the -normal side.
inline Body3D makeGroundPlane(math::vec3 normal, math::vec3 pointOnPlane) {
    Body3D b;
    b.shape = Body3D::Plane;
    const float len = std::sqrt(glm::dot(normal, normal));
    b.normal = len > 1e-9f ? normal / len : math::vec3(0.0f, 1.0f, 0.0f);
    b.planeD = glm::dot(b.normal, pointOnPlane);
    b.friction = 0.6f; // grippy ground so bodies come to rest instead of sliding forever
    b.invMass = 0.0f;
    return b;
}

namespace detail {

// A single contact: `n` points from body a toward body b, `pen` is the overlap depth (>0), `point` is
// the world contact point (used as the impulse application point for torque).
struct Contact3 {
    int a = -1, b = -1;
    math::vec3 n{0.0f, 1.0f, 0.0f};
    math::vec3 point{0.0f, 0.0f, 0.0f};
    float pen = 0.0f;
    bool hit = false;
};

// Sphere a vs sphere b. n from a -> b.
inline Contact3 sphereSphere(int ia, const Body3D& a, int ib, const Body3D& b) {
    Contact3 c;
    const math::vec3 d = b.pos - a.pos;
    const float dist2 = glm::dot(d, d);
    const float r = a.radius + b.radius;
    if (dist2 >= r * r) {
        return c;
    }
    const float dist = std::sqrt(dist2);
    const math::vec3 n = dist > 1e-6f ? d / dist : math::vec3(0.0f, 1.0f, 0.0f);
    c.a = ia;
    c.b = ib;
    c.n = n;
    c.pen = r - dist;
    c.point = a.pos + n * a.radius;
    c.hit = true;
    return c;
}

// Sphere `s` vs static plane `p`. n from s -> plane (into the solid, i.e. -plane.normal).
inline Contact3 spherePlane(int is, const Body3D& s, int ip, const Body3D& p) {
    Contact3 c;
    const float sd = glm::dot(s.pos, p.normal) - p.planeD; // signed distance, +normal side positive
    const float pen = s.radius - sd;
    if (pen <= 0.0f) {
        return c;
    }
    c.a = is;
    c.b = ip;
    c.n = -p.normal; // sphere -> solid
    c.pen = pen;
    c.point = s.pos - p.normal * s.radius; // deepest point on the sphere
    c.hit = true;
    return c;
}

// Sphere `s` vs oriented box `box`. Closest point on the OBB to the sphere centre (in box-local space);
// n from s -> box.
inline Contact3 sphereBox(int is, const Body3D& s, int ib, const Body3D& box) {
    Contact3 c;
    const math::mat3 R = glm::mat3_cast(box.orientation);
    const math::mat3 Rt = glm::transpose(R);
    const math::vec3 local = Rt * (s.pos - box.pos); // sphere centre in box space
    const math::vec3 q(std::clamp(local.x, -box.half.x, box.half.x),
                       std::clamp(local.y, -box.half.y, box.half.y),
                       std::clamp(local.z, -box.half.z, box.half.z));
    const math::vec3 delta = local - q; // box surface -> sphere centre (local)
    const float dist2 = glm::dot(delta, delta);
    if (dist2 > s.radius * s.radius) {
        return c;
    }
    math::vec3 nLocal;
    float pen;
    if (dist2 > 1e-10f) {
        const float dist = std::sqrt(dist2);
        nLocal = delta / dist; // box -> sphere
        pen = s.radius - dist;
    } else {
        // Centre inside the box: push out along the axis of least penetration.
        const float dx = box.half.x - std::fabs(local.x);
        const float dy = box.half.y - std::fabs(local.y);
        const float dz = box.half.z - std::fabs(local.z);
        if (dx <= dy && dx <= dz) {
            nLocal = math::vec3(local.x < 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f);
            pen = s.radius + dx;
        } else if (dy <= dz) {
            nLocal = math::vec3(0.0f, local.y < 0.0f ? -1.0f : 1.0f, 0.0f);
            pen = s.radius + dy;
        } else {
            nLocal = math::vec3(0.0f, 0.0f, local.z < 0.0f ? -1.0f : 1.0f);
            pen = s.radius + dz;
        }
    }
    c.a = is;
    c.b = ib;
    c.n = -(R * nLocal); // sphere -> box (flip box->sphere)
    c.pen = pen;
    c.point = box.pos + R * q; // point on the box surface
    c.hit = true;
    return c;
}

// Oriented box `box` vs static plane `p`. Every penetrating corner becomes a contact (n = -plane
// normal), so a box rests flat on the ground on up to four points without rocking. Appends to `out`.
inline void boxPlane(int ibox, const Body3D& box, int ip, const Body3D& p,
                     std::vector<Contact3>& out) {
    const math::mat3 R = glm::mat3_cast(box.orientation);
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sy = -1; sy <= 1; sy += 2) {
            for (int sz = -1; sz <= 1; sz += 2) {
                const math::vec3 corner =
                    box.pos + R * math::vec3(static_cast<float>(sx) * box.half.x,
                                             static_cast<float>(sy) * box.half.y,
                                             static_cast<float>(sz) * box.half.z);
                const float sd = glm::dot(corner, p.normal) - p.planeD;
                if (sd < 0.0f) {
                    Contact3 c;
                    c.a = ibox;
                    c.b = ip;
                    c.n = -p.normal; // box -> solid
                    c.pen = -sd;
                    c.point = corner;
                    c.hit = true;
                    out.push_back(c);
                }
            }
        }
    }
}

// Closest points c1,c2 between 3D segments (p1,q1) and (p2,q2) — Ericson, Real-Time Collision Detection.
inline void closestSegSeg3(const math::vec3& p1, const math::vec3& q1, const math::vec3& p2,
                           const math::vec3& q2, math::vec3& c1, math::vec3& c2) {
    const math::vec3 d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2;
    const float a = glm::dot(d1, d1), e = glm::dot(d2, d2), f = glm::dot(d2, r);
    float s, t;
    if (a < 1e-9f && e < 1e-9f) {
        c1 = p1;
        c2 = p2;
        return;
    }
    if (a < 1e-9f) {
        s = 0.0f;
        t = glm::clamp(f / e, 0.0f, 1.0f);
    } else {
        const float c = glm::dot(d1, r);
        if (e < 1e-9f) {
            t = 0.0f;
            s = glm::clamp(-c / a, 0.0f, 1.0f);
        } else {
            const float b = glm::dot(d1, d2);
            const float denom = a * e - b * b;
            s = denom > 1e-9f ? glm::clamp((b * f - c * e) / denom, 0.0f, 1.0f) : 0.0f;
            t = (b * s + f) / e;
            if (t < 0.0f) {
                t = 0.0f;
                s = glm::clamp(-c / a, 0.0f, 1.0f);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = glm::clamp((b - c) / a, 0.0f, 1.0f);
            }
        }
    }
    c1 = p1 + d1 * s;
    c2 = p2 + d2 * t;
}

// Clip a convex polygon against the half-space { p : dot(planeN, p) <= offset } (Sutherland-Hodgman).
inline std::vector<math::vec3> clipPoly3(const std::vector<math::vec3>& poly, const math::vec3& planeN,
                                         float offset) {
    std::vector<math::vec3> out;
    const int n = static_cast<int>(poly.size());
    for (int i = 0; i < n; ++i) {
        const math::vec3& cur = poly[static_cast<size_t>(i)];
        const math::vec3& nxt = poly[static_cast<size_t>((i + 1) % n)];
        const float dc = glm::dot(planeN, cur) - offset;
        const float dn = glm::dot(planeN, nxt) - offset;
        if (dc <= 0.0f) {
            out.push_back(cur);
        }
        if ((dc < 0.0f) != (dn < 0.0f)) {
            const float tt = dc / (dc - dn);
            out.push_back(cur + (nxt - cur) * tt);
        }
    }
    return out;
}

// --- Capsule support ------------------------------------------------------------------------------
// World endpoints of a capsule's inner segment (local +Y axis, half-length half.y), swept by radius.
inline void capsuleSegment3(const Body3D& c, math::vec3& p0, math::vec3& p1) {
    const math::vec3 axis = glm::mat3_cast(c.orientation) * math::vec3(0.0f, 1.0f, 0.0f);
    p0 = c.pos - axis * c.half.y;
    p1 = c.pos + axis * c.half.y;
}

inline math::vec3 closestOnSeg3(const math::vec3& p, const math::vec3& a, const math::vec3& b) {
    const math::vec3 ab = b - a;
    const float len2 = glm::dot(ab, ab);
    if (len2 < 1e-12f) {
        return a;
    }
    return a + ab * glm::clamp(glm::dot(p - a, ab) / len2, 0.0f, 1.0f);
}

// Capsule `cap` vs static plane `p`: both caps are spheres tested against the plane, so a horizontal
// capsule rests flat on two points. n from cap -> plane (-plane.normal). Appends to `out`.
inline void capsulePlane(int icap, const Body3D& cap, int ip, const Body3D& p,
                         std::vector<Contact3>& out) {
    math::vec3 e0, e1;
    capsuleSegment3(cap, e0, e1);
    const math::vec3 ends[2] = {e0, e1};
    for (const math::vec3& e : ends) {
        const float sd = glm::dot(e, p.normal) - p.planeD;
        const float pen = cap.radius - sd;
        if (pen > 0.0f) {
            Contact3 c;
            c.a = icap;
            c.b = ip;
            c.n = -p.normal;
            c.pen = pen;
            c.point = e - p.normal * cap.radius;
            c.hit = true;
            out.push_back(c);
        }
    }
}

// Sphere `s` vs capsule `cap`: closest point on the capsule segment to the sphere centre, then a
// sphere-sphere test. n from s -> cap.
inline Contact3 sphereCapsule(int is, const Body3D& s, int ic, const Body3D& cap) {
    Contact3 c;
    math::vec3 p0, p1;
    capsuleSegment3(cap, p0, p1);
    const math::vec3 cp = closestOnSeg3(s.pos, p0, p1);
    const math::vec3 d = cp - s.pos;
    const float dist2 = glm::dot(d, d);
    const float r = s.radius + cap.radius;
    if (dist2 >= r * r) {
        return c;
    }
    const float dist = std::sqrt(dist2);
    const math::vec3 n = dist > 1e-6f ? d / dist : math::vec3(0.0f, 1.0f, 0.0f);
    c.a = is;
    c.b = ic;
    c.n = n;
    c.pen = r - dist;
    c.point = s.pos + n * s.radius;
    c.hit = true;
    return c;
}

// Capsule a vs capsule b: closest points between the two segments, then sphere-sphere. n from a -> b.
inline Contact3 capsuleCapsule(int ia, const Body3D& a, int ib, const Body3D& b) {
    Contact3 c;
    math::vec3 a0, a1, b0, b1;
    capsuleSegment3(a, a0, a1);
    capsuleSegment3(b, b0, b1);
    math::vec3 ca, cb;
    closestSegSeg3(a0, a1, b0, b1, ca, cb);
    const math::vec3 d = cb - ca;
    const float dist2 = glm::dot(d, d);
    const float r = a.radius + b.radius;
    if (dist2 >= r * r) {
        return c;
    }
    const float dist = std::sqrt(dist2);
    const math::vec3 n = dist > 1e-6f ? d / dist : math::vec3(0.0f, 1.0f, 0.0f);
    c.a = ia;
    c.b = ib;
    c.n = n;
    c.pen = r - dist;
    c.point = ca + n * a.radius;
    c.hit = true;
    return c;
}

// Capsule `cap` vs oriented box `box`: ternary-search the segment point closest to the box, then treat
// it as a sphere-vs-box contact. n from cap -> box. Single point (good enough for resting/leaning).
inline Contact3 capsuleBox(int icap, const Body3D& cap, int ib, const Body3D& box) {
    math::vec3 p0, p1;
    capsuleSegment3(cap, p0, p1);
    const math::mat3 R = glm::mat3_cast(box.orientation);
    const math::mat3 Rt = glm::transpose(R);
    auto distToBox = [&](float t) {
        const math::vec3 w = p0 + (p1 - p0) * t;
        const math::vec3 l = Rt * (w - box.pos);
        const math::vec3 q(glm::clamp(l.x, -box.half.x, box.half.x),
                           glm::clamp(l.y, -box.half.y, box.half.y),
                           glm::clamp(l.z, -box.half.z, box.half.z));
        return glm::dot(l - q, l - q);
    };
    float lo = 0.0f, hi = 1.0f;
    for (int i = 0; i < 40; ++i) {
        const float m1 = lo + (hi - lo) / 3.0f, m2 = hi - (hi - lo) / 3.0f;
        if (distToBox(m1) < distToBox(m2)) {
            hi = m2;
        } else {
            lo = m1;
        }
    }
    const float t = (lo + hi) * 0.5f;
    Body3D probe;
    probe.shape = Body3D::Sphere;
    probe.pos = p0 + (p1 - p0) * t;
    probe.radius = cap.radius;
    return sphereBox(icap, probe, ib, box); // n: probe(cap) -> box
}

// Oriented box A vs oriented box B via the Separating-Axis Theorem over 15 axes (3 face normals each +
// 9 edge-edge cross products). A face-contact produces up to four contact points by clipping the
// incident face against the reference face's side planes (Godot BoxShape3D stacking); an edge-contact
// produces a single point at the closest approach of the two edges. n is oriented A -> B. Appends to
// `out`. Face axes are preferred over edge axes by a small tolerance to avoid normal flip-flop.
inline void boxBox(int ia, const Body3D& A, int ib, const Body3D& B, std::vector<Contact3>& out) {
    const math::mat3 RA = glm::mat3_cast(A.orientation);
    const math::mat3 RB = glm::mat3_cast(B.orientation);
    const math::vec3 Aax[3] = {RA[0], RA[1], RA[2]};
    const math::vec3 Bax[3] = {RB[0], RB[1], RB[2]};
    const math::vec3 d = B.pos - A.pos;

    auto projRadius = [](const math::vec3& L, const math::vec3 ax[3], const math::vec3& half) {
        return std::fabs(glm::dot(L, ax[0])) * half.x + std::fabs(glm::dot(L, ax[1])) * half.y +
               std::fabs(glm::dot(L, ax[2])) * half.z;
    };
    // Face query: separation along each of `ax`'s 3 axes; returns max separation + its face index.
    auto queryFaces = [&](const math::vec3 ax[3], const math::vec3& half, const math::vec3 oax[3],
                          const math::vec3& ohalf, int& faceIdx) {
        float best = -1e30f;
        faceIdx = 0;
        for (int i = 0; i < 3; ++i) {
            const float rSelf = half[i];
            const float rOther = projRadius(ax[i], oax, ohalf);
            const float sep = std::fabs(glm::dot(ax[i], d)) - (rSelf + rOther);
            if (sep > best) {
                best = sep;
                faceIdx = i;
            }
        }
        return best;
    };
    int fa = 0, fb = 0;
    const float sepA = queryFaces(Aax, A.half, Bax, B.half, fa);
    if (sepA > 0.0f) {
        return;
    }
    const float sepB = queryFaces(Bax, B.half, Aax, A.half, fb);
    if (sepB > 0.0f) {
        return;
    }
    // Edge query: 9 cross-product axes.
    float sepE = -1e30f;
    int ea = 0, eb = 0;
    math::vec3 edgeAxis(0.0f);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            math::vec3 L = glm::cross(Aax[i], Bax[j]);
            const float l2 = glm::dot(L, L);
            if (l2 < 1e-8f) {
                continue; // parallel edges: degenerate axis
            }
            L /= std::sqrt(l2);
            if (glm::dot(L, d) < 0.0f) {
                L = -L; // orient A -> B
            }
            const float sep = glm::dot(L, d) - (projRadius(L, Aax, A.half) + projRadius(L, Bax, B.half));
            if (sep > sepE) {
                sepE = sep;
                ea = i;
                eb = j;
                edgeAxis = L;
            }
        }
    }
    if (sepE > 0.0f) {
        return;
    }

    // Choose the contact type. Faces are preferred unless an edge axis penetrates clearly less.
    const float absTol = 0.002f;
    const float faceSep = std::max(sepA, sepB);
    if (sepE > faceSep + absTol) {
        // Edge-edge: single contact at the closest approach of the two supporting edges.
        auto supportEdge = [](const Body3D& box, const math::vec3 ax[3], int edgeDir,
                              const math::vec3& toward, math::vec3& p0, math::vec3& p1) {
            math::vec3 c = box.pos;
            for (int k = 0; k < 3; ++k) {
                if (k == edgeDir) {
                    continue;
                }
                const float s = glm::dot(toward, ax[k]) >= 0.0f ? 1.0f : -1.0f;
                c += ax[k] * (s * box.half[k]);
            }
            p0 = c - ax[edgeDir] * box.half[edgeDir];
            p1 = c + ax[edgeDir] * box.half[edgeDir];
        };
        math::vec3 a0, a1, b0, b1;
        supportEdge(A, Aax, ea, edgeAxis, a0, a1);   // A edge toward +axis
        supportEdge(B, Bax, eb, -edgeAxis, b0, b1);  // B edge toward -axis
        math::vec3 c1, c2;
        closestSegSeg3(a0, a1, b0, b1, c1, c2);
        Contact3 c;
        c.a = ia;
        c.b = ib;
        c.n = edgeAxis;
        c.pen = -sepE;
        c.point = (c1 + c2) * 0.5f;
        c.hit = true;
        out.push_back(c);
        return;
    }

    // Face contact. Reference = the box with the larger (less negative) face separation.
    const bool refIsA = sepA >= sepB - absTol;
    const math::vec3* refAx = refIsA ? Aax : Bax;
    const math::vec3* incAx = refIsA ? Bax : Aax;
    const Body3D& refBody = refIsA ? A : B;
    const Body3D& incBody = refIsA ? B : A;
    const int rf = refIsA ? fa : fb;

    math::vec3 n = refAx[rf];
    const math::vec3 refToInc = incBody.pos - refBody.pos;
    if (glm::dot(n, refToInc) < 0.0f) {
        n = -n; // outward from the reference box toward the incident box
    }
    const math::vec3 refCenter = refBody.pos + n * refBody.half[rf];
    const int rt1 = (rf + 1) % 3, rt2 = (rf + 2) % 3;

    // Incident face: the incident box face whose normal is most anti-parallel to n.
    int ida = 0;
    float bestDot = 1e30f;
    float isign = 1.0f;
    for (int k = 0; k < 3; ++k) {
        const float dp = glm::dot(incAx[k], n);
        if (dp < bestDot) { // most negative => most anti-parallel with +axis
            bestDot = dp;
            ida = k;
            isign = 1.0f;
        }
        if (-dp < bestDot) {
            bestDot = -dp;
            ida = k;
            isign = -1.0f;
        }
    }
    const math::vec3 incN = incAx[ida] * isign;
    const math::vec3 incCenter = incBody.pos + incN * incBody.half[ida];
    const int it1 = (ida + 1) % 3, it2 = (ida + 2) % 3;
    const math::vec3 u = incAx[it1] * incBody.half[it1];
    const math::vec3 v = incAx[it2] * incBody.half[it2];
    std::vector<math::vec3> poly = {incCenter - u - v, incCenter + u - v, incCenter + u + v,
                                    incCenter - u + v};

    // Clip against the reference face's four side planes.
    const math::vec3 t1 = refAx[rt1];
    const math::vec3 t2 = refAx[rt2];
    poly = clipPoly3(poly, t1, glm::dot(t1, refCenter) + refBody.half[rt1]);
    poly = clipPoly3(poly, -t1, -glm::dot(t1, refCenter) + refBody.half[rt1]);
    poly = clipPoly3(poly, t2, glm::dot(t2, refCenter) + refBody.half[rt2]);
    poly = clipPoly3(poly, -t2, -glm::dot(t2, refCenter) + refBody.half[rt2]);

    // Keep points below the reference face plane; each is a contact. n reported A -> B.
    const math::vec3 nAB = refIsA ? n : -n;
    for (const math::vec3& p : poly) {
        const float sd = glm::dot(p - refCenter, n); // <0 => penetrating into the reference box
        if (sd < 0.0f) {
            Contact3 c;
            c.a = ia;
            c.b = ib;
            c.n = nAB;
            c.pen = -sd;
            c.point = p - n * (sd * 0.5f);
            c.hit = true;
            out.push_back(c);
        }
    }
}

// A solver constraint built from a contact: precomputed lever arms, a contact frame (normal + two
// tangents), effective masses, a restitution target, and the accumulated impulses that are warm-started
// from the previous frame's matching contact. Accumulation + warm starting are what make a tall stack
// stay rigid at a handful of iterations (the same technique as the 2D warm solver).
struct Constraint3 {
    int a = -1, b = -1;
    math::vec3 rA{0.0f}, rB{0.0f};
    math::vec3 n{0.0f}, t1{0.0f}, t2{0.0f};
    float massN = 0.0f, massT1 = 0.0f, massT2 = 0.0f;
    float restitutionBias = 0.0f;
    float pen = 0.0f;
    math::vec3 point{0.0f};
    float mu = 0.0f;
    float an = 0.0f, jt1 = 0.0f, jt2 = 0.0f; // accumulated normal + friction impulses
    uint64_t key = 0;
};

// Build an orthonormal tangent basis (t1,t2) spanning the plane perpendicular to unit `n`. Chosen
// deterministically from `n` so a stable contact normal yields a stable basis frame to frame (which is
// what lets the accumulated friction impulses warm-start correctly).
inline void makeBasis3(const math::vec3& n, math::vec3& t1, math::vec3& t2) {
    if (std::fabs(n.x) >= 0.577f) {
        t1 = glm::normalize(math::vec3(n.y, -n.x, 0.0f));
    } else {
        t1 = glm::normalize(math::vec3(0.0f, n.z, -n.y));
    }
    t2 = glm::cross(n, t1);
}

// --- Ray casts against individual shapes (o = origin, d = unit direction) --------------------------
// Each returns whether the ray enters the shape within [0, maxDist], writing the entry distance and
// the outward surface normal at the hit.
inline bool raySphere(const math::vec3& o, const math::vec3& d, const math::vec3& c, float r,
                      float maxDist, float& tOut, math::vec3& nOut) {
    const math::vec3 m = o - c;
    const float b = glm::dot(m, d);
    const float cc = glm::dot(m, m) - r * r;
    if (cc > 0.0f && b > 0.0f) {
        return false; // origin outside and pointing away
    }
    const float disc = b * b - cc;
    if (disc < 0.0f) {
        return false;
    }
    float t = -b - std::sqrt(disc);
    if (t < 0.0f) {
        t = 0.0f; // origin inside
    }
    if (t > maxDist) {
        return false;
    }
    tOut = t;
    const math::vec3 p = o + d * t;
    nOut = r > 1e-6f ? (p - c) / r : math::vec3(0.0f, 1.0f, 0.0f);
    return true;
}

inline bool rayPlane(const math::vec3& o, const math::vec3& d, const math::vec3& N, float D,
                     float maxDist, float& tOut, math::vec3& nOut) {
    const float denom = glm::dot(d, N);
    if (std::fabs(denom) < 1e-8f) {
        return false; // parallel to the plane
    }
    const float t = (D - glm::dot(o, N)) / denom;
    if (t < 0.0f || t > maxDist) {
        return false;
    }
    tOut = t;
    nOut = N; // outward normal of the half-space
    return true;
}

inline bool rayBox(const math::vec3& o, const math::vec3& d, const Body3D& box, float maxDist,
                   float& tOut, math::vec3& nOut) {
    const math::mat3 R = glm::mat3_cast(box.orientation);
    const math::mat3 Rt = glm::transpose(R);
    const math::vec3 lo = Rt * (o - box.pos);
    const math::vec3 ld = Rt * d;
    float tmin = 0.0f, tmax = maxDist;
    math::vec3 nLocal(0.0f);
    for (int a = 0; a < 3; ++a) {
        const float h = box.half[a];
        if (std::fabs(ld[a]) < 1e-8f) {
            if (lo[a] < -h || lo[a] > h) {
                return false; // parallel to this slab and outside it
            }
            continue;
        }
        const float inv = 1.0f / ld[a];
        float tNear = (-h - lo[a]) * inv, tFar = (h - lo[a]) * inv;
        float s = -1.0f;
        if (tNear > tFar) {
            std::swap(tNear, tFar);
            s = 1.0f;
        }
        if (tNear > tmin) {
            tmin = tNear;
            nLocal = math::vec3(0.0f);
            nLocal[a] = s;
        }
        if (tFar < tmax) {
            tmax = tFar;
        }
        if (tmin > tmax) {
            return false;
        }
    }
    tOut = tmin;
    nOut = R * nLocal;
    return true;
}

inline bool rayCapsule(const math::vec3& o, const math::vec3& d, const Body3D& cap, float maxDist,
                       float& tOut, math::vec3& nOut) {
    math::vec3 p0, p1;
    capsuleSegment3(cap, p0, p1);
    const float r = cap.radius;
    float best = maxDist;
    bool found = false;
    float t;
    math::vec3 nrm;
    // The two hemispherical caps.
    if (raySphere(o, d, p0, r, best, t, nrm)) {
        best = t;
        nOut = nrm;
        found = true;
    }
    if (raySphere(o, d, p1, r, best, t, nrm)) {
        best = t;
        nOut = nrm;
        found = true;
    }
    // The cylindrical side.
    const math::vec3 axis = p1 - p0;
    const float len = std::sqrt(glm::dot(axis, axis));
    if (len > 1e-6f) {
        const math::vec3 va = axis / len;
        const math::vec3 dp = o - p0;
        const math::vec3 A = d - va * glm::dot(d, va);
        const math::vec3 B = dp - va * glm::dot(dp, va);
        const float ca = glm::dot(A, A);
        if (ca > 1e-8f) {
            const float cb = 2.0f * glm::dot(A, B);
            const float cc = glm::dot(B, B) - r * r;
            const float disc = cb * cb - 4.0f * ca * cc;
            if (disc >= 0.0f) {
                const float sq = std::sqrt(disc);
                const float roots[2] = {(-cb - sq) / (2.0f * ca), (-cb + sq) / (2.0f * ca)};
                for (float tc : roots) {
                    if (tc < 0.0f || tc >= best) {
                        continue;
                    }
                    const math::vec3 p = o + d * tc;
                    const float s = glm::dot(p - p0, va);
                    if (s >= 0.0f && s <= len) {
                        best = tc;
                        nOut = glm::normalize(p - (p0 + va * s));
                        found = true;
                        break;
                    }
                }
            }
        }
    }
    if (found) {
        tOut = best;
    }
    return found;
}

} // namespace detail

// Result of a ray query against the physics world (Godot PhysicsDirectSpaceState3D.intersect_ray).
struct RayHit3 {
    bool hit = false;
    float t = 0.0f;         // distance along the ray to the entry point
    math::vec3 point{0.0f}; // world hit point (origin + dir * t)
    math::vec3 normal{0.0f};
    int index = -1; // struck body index
};

// A world of 3D rigid bodies resolved with sequential impulses under a fixed timestep.
struct PhysicsWorld3D {
    std::vector<Body3D> bodies;
    math::vec3 gravity{0.0f, -9.81f, 0.0f};

    // Positional correction (Baumgarte): push overlapping bodies apart by `correctionPercent` of the
    // penetration beyond `slop` each step, so resting stacks don't sink but don't jitter either.
    float slop = 0.005f;
    float correctionPercent = 0.2f;
    // Relative normal speed below which restitution is suppressed, so resting bodies don't buzz.
    float restitutionThreshold = 1.0f;
    // Warm starting: carry each contact's accumulated impulses to the next frame (matched by body pair
    // + contact-point proximity). This is what makes tall stacks stay rigid at a few iterations.
    bool warmStarting = true;
    // Broadphase: sweep-and-prune over world AABBs to cull pairs that cannot touch. It yields the same
    // candidate set (in the same (i,j) order) as the brute-force O(n^2) test, so results are identical;
    // it only skips pairs whose AABBs are disjoint. Infinite planes are tested against every body.
    bool broadphase = true;
    // Sleeping (Godot can_sleep). Bodies connected by contacts form an island; an island sleeps only
    // once every dynamic member has stayed below both thresholds for `sleepTime`. Off by default so
    // existing scenes are unchanged.
    bool allowSleep = false;
    float sleepLinearThreshold = 0.05f;
    float sleepAngularThreshold = 0.05f;
    float sleepTime = 0.5f;

    int add(const Body3D& b) {
        bodies.push_back(b);
        return static_cast<int>(bodies.size()) - 1;
    }

    // Cast a ray and return the nearest body it enters within `maxDist` (Godot intersect_ray). Only
    // bodies whose collisionLayer intersects `mask` are considered. Pure geometry — no simulation step.
    RayHit3 queryRay(const math::vec3& origin, const math::vec3& dir, float maxDist = 1e30f,
                     LayerMask mask = ~0u) const {
        const float dl = std::sqrt(glm::dot(dir, dir));
        RayHit3 best;
        if (dl < 1e-9f) {
            return best;
        }
        const math::vec3 d = dir / dl;
        best.t = maxDist;
        for (int i = 0; i < static_cast<int>(bodies.size()); ++i) {
            const Body3D& b = bodies[static_cast<size_t>(i)];
            if ((b.collisionLayer & mask) == 0u) {
                continue;
            }
            float t = 0.0f;
            math::vec3 nrm(0.0f);
            bool hit = false;
            if (b.shape == Body3D::Sphere) {
                hit = detail::raySphere(origin, d, b.pos, b.radius, best.t, t, nrm);
            } else if (b.shape == Body3D::Box) {
                hit = detail::rayBox(origin, d, b, best.t, t, nrm);
            } else if (b.shape == Body3D::Capsule) {
                hit = detail::rayCapsule(origin, d, b, best.t, t, nrm);
            } else { // Plane
                hit = detail::rayPlane(origin, d, b.normal, b.planeD, best.t, t, nrm);
            }
            if (hit && t <= best.t) {
                best.hit = true;
                best.t = t;
                best.point = origin + d * t;
                best.normal = nrm;
                best.index = i;
            }
        }
        return best;
    }

    void step(float dt, int iterations = 8) {
        const int n = static_cast<int>(bodies.size());

        // Sleeping: decide (from last frame's islands) which bodies are asleep, then make each sleeper
        // temporarily immovable so it is skipped by integration and acts as static in the solve. Its
        // real mass/inertia are restored after the step, so the whole solver is reused unchanged.
        std::vector<size_t> sleptIdx;
        std::vector<float> sleptInvMass;
        std::vector<math::mat3> sleptInvInertia;
        if (allowSleep) {
            updateSleepStates();
            for (size_t i = 0; i < bodies.size(); ++i) {
                if (bodies[i].sleeping) {
                    sleptIdx.push_back(i);
                    sleptInvMass.push_back(bodies[i].invMass);
                    sleptInvInertia.push_back(bodies[i].invInertiaLocal);
                    bodies[i].invMass = 0.0f;
                    bodies[i].invInertiaLocal = math::mat3(0.0f);
                    bodies[i].vel = math::vec3(0.0f);
                    bodies[i].angularVel = math::vec3(0.0f);
                }
            }
        }

        // Integrate velocity (gravity + damping) for dynamic bodies.
        for (Body3D& b : bodies) {
            if (b.invMass > 0.0f) {
                b.vel += gravity * dt;
                b.vel *= 1.0f / (1.0f + b.linearDamping * dt);
                b.angularVel *= 1.0f / (1.0f + b.angularDamping * dt);
            }
        }

        // World-space inverse inertia tensor per body (rotate the local tensor by the orientation).
        m_invIw.assign(static_cast<size_t>(n), math::mat3(0.0f));
        for (int i = 0; i < n; ++i) {
            const Body3D& b = bodies[static_cast<size_t>(i)];
            const math::mat3 R = glm::mat3_cast(b.orientation);
            m_invIw[static_cast<size_t>(i)] = R * b.invInertiaLocal * glm::transpose(R);
        }

        // Narrow-phase over candidate pairs (broadphase-culled or the full O(n^2) set).
        std::vector<detail::Contact3> contacts;
        for (const std::pair<int, int>& pr : collectPairs3()) {
            const int i = pr.first, j = pr.second;
            if (bodies[static_cast<size_t>(i)].invMass == 0.0f &&
                bodies[static_cast<size_t>(j)].invMass == 0.0f) {
                continue; // two static bodies never interact
            }
            if (!interact(bodies[static_cast<size_t>(i)].collisionLayer,
                          bodies[static_cast<size_t>(i)].collisionMask,
                          bodies[static_cast<size_t>(j)].collisionLayer,
                          bodies[static_cast<size_t>(j)].collisionMask)) {
                continue;
            }
            collide(i, j, contacts);
        }

        // Build solver constraints from the contacts and warm-start them from last frame.
        std::vector<detail::Constraint3> cons;
        cons.reserve(contacts.size());
        for (const detail::Contact3& c : contacts) {
            if (c.hit) {
                cons.push_back(buildConstraint(c));
            }
        }
        if (warmStarting) {
            warmStart(cons);
        }
        // Velocity iterations: normal then friction impulses, accumulated + clamped.
        for (int it = 0; it < iterations; ++it) {
            for (detail::Constraint3& c : cons) {
                solveConstraint(c);
            }
        }
        // Positional correction (split from velocity so it never injects energy).
        for (const detail::Constraint3& c : cons) {
            correctPosition(c);
        }
        m_prev = std::move(cons); // keep for next frame's warm start

        // Integrate position + orientation.
        for (Body3D& b : bodies) {
            if (b.invMass > 0.0f) {
                b.pos += b.vel * dt;
                const math::quat wq(0.0f, b.angularVel.x, b.angularVel.y, b.angularVel.z);
                b.orientation = glm::normalize(b.orientation + 0.5f * dt * (wq * b.orientation));
            }
        }

        // Restore the sleepers' real mass/inertia, then update quiet timers from this step's result.
        for (size_t k = 0; k < sleptIdx.size(); ++k) {
            bodies[sleptIdx[k]].invMass = sleptInvMass[k];
            bodies[sleptIdx[k]].invInertiaLocal = sleptInvInertia[k];
        }
        if (allowSleep) {
            updateSleepTimers(dt);
        }
    }

private:
    std::vector<math::mat3> m_invIw; // world-space inverse inertia per body, refreshed each step

    // Decide sleep state per island. Bodies connected by last frame's contacts form islands (union-
    // find); an island sleeps only once every dynamic member has been quiet for `sleepTime`. A body
    // touched this frame by a still-awake mover wakes on the following step (islands lag one frame),
    // and it still collides as a static obstacle in between, so nothing tunnels.
    void updateSleepStates() {
        const int n = static_cast<int>(bodies.size());
        std::vector<int> parent(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            parent[static_cast<size_t>(i)] = i;
        }
        std::function<int(int)> find = [&](int x) {
            while (parent[static_cast<size_t>(x)] != x) {
                parent[static_cast<size_t>(x)] = parent[static_cast<size_t>(parent[static_cast<size_t>(x)])];
                x = parent[static_cast<size_t>(x)];
            }
            return x;
        };
        auto unite = [&](int a, int b) { parent[static_cast<size_t>(find(a))] = find(b); };
        for (const detail::Constraint3& c : m_prev) {
            unite(c.a, c.b);
        }
        // An island is "quiet" iff every dynamic member has stayed below threshold long enough.
        std::unordered_map<int, bool> quiet;
        for (int i = 0; i < n; ++i) {
            if (bodies[static_cast<size_t>(i)].invMass > 0.0f) {
                const int r = find(i);
                const bool q = bodies[static_cast<size_t>(i)].sleepTimer >= sleepTime;
                auto it = quiet.find(r);
                quiet[r] = (it == quiet.end()) ? q : (it->second && q);
            }
        }
        for (int i = 0; i < n; ++i) {
            if (bodies[static_cast<size_t>(i)].invMass > 0.0f) {
                bodies[static_cast<size_t>(i)].sleeping = quiet[find(i)];
            }
        }
    }

    // Accumulate quiet time for bodies below both thresholds; reset the moment one moves.
    void updateSleepTimers(float dt) {
        for (Body3D& b : bodies) {
            if (b.invMass <= 0.0f) {
                continue;
            }
            const bool slow = glm::dot(b.vel, b.vel) < sleepLinearThreshold * sleepLinearThreshold &&
                              glm::dot(b.angularVel, b.angularVel) <
                                  sleepAngularThreshold * sleepAngularThreshold;
            b.sleepTimer = slow ? b.sleepTimer + dt : 0.0f;
        }
    }

    // World-space AABB of a body. `infinite` is set for planes (no finite bounds).
    void bodyAabb(const Body3D& b, math::vec3& mn, math::vec3& mx, bool& infinite) const {
        infinite = false;
        if (b.shape == Body3D::Plane) {
            infinite = true;
            return;
        }
        if (b.shape == Body3D::Sphere) {
            const math::vec3 r(b.radius);
            mn = b.pos - r;
            mx = b.pos + r;
        } else if (b.shape == Body3D::Box) {
            const math::mat3 R = glm::mat3_cast(b.orientation);
            math::vec3 ext;
            for (int a = 0; a < 3; ++a) {
                ext[a] = std::fabs(R[0][a]) * b.half.x + std::fabs(R[1][a]) * b.half.y +
                         std::fabs(R[2][a]) * b.half.z;
            }
            mn = b.pos - ext;
            mx = b.pos + ext;
        } else { // Capsule
            math::vec3 e0, e1;
            detail::capsuleSegment3(b, e0, e1);
            const math::vec3 r(b.radius);
            mn = glm::min(e0, e1) - r;
            mx = glm::max(e0, e1) + r;
        }
    }

    // Candidate collision pairs. With broadphase off this is the full O(n^2) set; with it on, a
    // sweep-and-prune over world AABBs (planes tested against all). Either way the returned list is
    // sorted ascending by (i,j) so the narrow-phase + solve order — and the result — is identical.
    std::vector<std::pair<int, int>> collectPairs3() const {
        const int n = static_cast<int>(bodies.size());
        std::vector<std::pair<int, int>> pairs;
        if (!broadphase) {
            for (int i = 0; i < n; ++i) {
                for (int j = i + 1; j < n; ++j) {
                    pairs.emplace_back(i, j);
                }
            }
            return pairs;
        }
        struct Entry {
            math::vec3 mn, mx;
            int idx;
        };
        std::vector<Entry> fin;
        std::vector<int> planes;
        fin.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            math::vec3 mn, mx;
            bool inf = false;
            bodyAabb(bodies[static_cast<size_t>(i)], mn, mx, inf);
            if (inf) {
                planes.push_back(i);
            } else {
                fin.push_back(Entry{mn, mx, i});
            }
        }
        std::sort(fin.begin(), fin.end(),
                  [](const Entry& a, const Entry& b) { return a.mn.x < b.mn.x; });
        auto add = [&](int a, int b) {
            if (a > b) {
                std::swap(a, b);
            }
            pairs.emplace_back(a, b);
        };
        // Sweep along x; for each body test following bodies until their min.x passes this max.x.
        for (size_t a = 0; a < fin.size(); ++a) {
            for (size_t b = a + 1; b < fin.size(); ++b) {
                if (fin[b].mn.x > fin[a].mx.x) {
                    break;
                }
                const bool overlap = fin[a].mn.y <= fin[b].mx.y && fin[a].mx.y >= fin[b].mn.y &&
                                     fin[a].mn.z <= fin[b].mx.z && fin[a].mx.z >= fin[b].mn.z;
                if (overlap) {
                    add(fin[a].idx, fin[b].idx);
                }
            }
        }
        // Infinite planes are tested against every other body.
        for (int p : planes) {
            for (int k = 0; k < n; ++k) {
                if (k != p) {
                    add(p, k);
                }
            }
        }
        std::sort(pairs.begin(), pairs.end());
        pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
        return pairs;
    }

    void collide(int i, int j, std::vector<detail::Contact3>& out) const {
        const Body3D& a = bodies[static_cast<size_t>(i)];
        const Body3D& b = bodies[static_cast<size_t>(j)];
        auto push = [&](detail::Contact3 c) {
            if (c.hit) {
                out.push_back(c);
            }
        };
        if (a.shape == Body3D::Sphere && b.shape == Body3D::Sphere) {
            push(detail::sphereSphere(i, a, j, b));
        } else if (a.shape == Body3D::Sphere && b.shape == Body3D::Plane) {
            push(detail::spherePlane(i, a, j, b));
        } else if (a.shape == Body3D::Plane && b.shape == Body3D::Sphere) {
            detail::Contact3 c = detail::spherePlane(j, b, i, a);
            std::swap(c.a, c.b);
            c.n = -c.n;
            push(c);
        } else if (a.shape == Body3D::Sphere && b.shape == Body3D::Box) {
            push(detail::sphereBox(i, a, j, b));
        } else if (a.shape == Body3D::Box && b.shape == Body3D::Sphere) {
            detail::Contact3 c = detail::sphereBox(j, b, i, a);
            std::swap(c.a, c.b);
            c.n = -c.n;
            push(c);
        } else if (a.shape == Body3D::Box && b.shape == Body3D::Plane) {
            detail::boxPlane(i, a, j, b, out);
        } else if (a.shape == Body3D::Plane && b.shape == Body3D::Box) {
            std::vector<detail::Contact3> tmp;
            detail::boxPlane(j, b, i, a, tmp);
            for (detail::Contact3 c : tmp) {
                std::swap(c.a, c.b);
                c.n = -c.n;
                out.push_back(c);
            }
        } else if (a.shape == Body3D::Box && b.shape == Body3D::Box) {
            detail::boxBox(i, a, j, b, out);
        } else if (a.shape == Body3D::Capsule && b.shape == Body3D::Plane) {
            detail::capsulePlane(i, a, j, b, out);
        } else if (a.shape == Body3D::Plane && b.shape == Body3D::Capsule) {
            std::vector<detail::Contact3> tmp;
            detail::capsulePlane(j, b, i, a, tmp);
            for (detail::Contact3 c : tmp) {
                std::swap(c.a, c.b);
                c.n = -c.n;
                out.push_back(c);
            }
        } else if (a.shape == Body3D::Sphere && b.shape == Body3D::Capsule) {
            push(detail::sphereCapsule(i, a, j, b));
        } else if (a.shape == Body3D::Capsule && b.shape == Body3D::Sphere) {
            detail::Contact3 c = detail::sphereCapsule(j, b, i, a);
            std::swap(c.a, c.b);
            c.n = -c.n;
            push(c);
        } else if (a.shape == Body3D::Capsule && b.shape == Body3D::Capsule) {
            push(detail::capsuleCapsule(i, a, j, b));
        } else if (a.shape == Body3D::Capsule && b.shape == Body3D::Box) {
            push(detail::capsuleBox(i, a, j, b));
        } else if (a.shape == Body3D::Box && b.shape == Body3D::Capsule) {
            detail::Contact3 c = detail::capsuleBox(j, b, i, a);
            std::swap(c.a, c.b);
            c.n = -c.n;
            push(c);
        }
    }

    // Effective mass along direction d for a contact with lever arms rA, rB.
    float effMass(int ia, int ib, const math::vec3& rA, const math::vec3& rB,
                  const math::vec3& d) const {
        const Body3D& a = bodies[static_cast<size_t>(ia)];
        const Body3D& b = bodies[static_cast<size_t>(ib)];
        const math::vec3 raxd = glm::cross(rA, d);
        const math::vec3 rbxd = glm::cross(rB, d);
        return a.invMass + b.invMass + glm::dot(raxd, m_invIw[static_cast<size_t>(ia)] * raxd) +
               glm::dot(rbxd, m_invIw[static_cast<size_t>(ib)] * rbxd);
    }

    detail::Constraint3 buildConstraint(const detail::Contact3& c) const {
        const Body3D& a = bodies[static_cast<size_t>(c.a)];
        const Body3D& b = bodies[static_cast<size_t>(c.b)];
        detail::Constraint3 k;
        k.a = c.a;
        k.b = c.b;
        k.n = c.n;
        detail::makeBasis3(c.n, k.t1, k.t2);
        k.rA = c.point - a.pos;
        k.rB = c.point - b.pos;
        k.point = c.point;
        k.pen = c.pen;
        const float mN = effMass(c.a, c.b, k.rA, k.rB, k.n);
        const float mT1 = effMass(c.a, c.b, k.rA, k.rB, k.t1);
        const float mT2 = effMass(c.a, c.b, k.rA, k.rB, k.t2);
        k.massN = mN > 0.0f ? 1.0f / mN : 0.0f;
        k.massT1 = mT1 > 0.0f ? 1.0f / mT1 : 0.0f;
        k.massT2 = mT2 > 0.0f ? 1.0f / mT2 : 0.0f;
        k.mu = std::sqrt(a.friction * b.friction);
        const math::vec3 rv =
            (b.vel + glm::cross(b.angularVel, k.rB)) - (a.vel + glm::cross(a.angularVel, k.rA));
        const float vn = glm::dot(rv, k.n);
        const float e = (-vn > restitutionThreshold) ? std::min(a.restitution, b.restitution) : 0.0f;
        k.restitutionBias = -e * vn; // target closing speed to reverse on bounce
        k.key = (static_cast<uint64_t>(static_cast<uint32_t>(c.a)) << 32) |
                static_cast<uint32_t>(c.b);
        return k;
    }

    void applyImpulse(detail::Constraint3& c, const math::vec3& P) {
        Body3D& a = bodies[static_cast<size_t>(c.a)];
        Body3D& b = bodies[static_cast<size_t>(c.b)];
        a.vel -= P * a.invMass;
        a.angularVel -= m_invIw[static_cast<size_t>(c.a)] * glm::cross(c.rA, P);
        b.vel += P * b.invMass;
        b.angularVel += m_invIw[static_cast<size_t>(c.b)] * glm::cross(c.rB, P);
    }

    // Seed each constraint's accumulators from the matching contact last frame (same pair + nearest
    // point) and apply the inherited impulse, so the solver starts near the converged answer.
    void warmStart(std::vector<detail::Constraint3>& cons) {
        std::unordered_multimap<uint64_t, size_t> index;
        index.reserve(m_prev.size() * 2);
        for (size_t p = 0; p < m_prev.size(); ++p) {
            index.emplace(m_prev[p].key, p);
        }
        for (detail::Constraint3& c : cons) {
            auto range = index.equal_range(c.key);
            float bestD2 = 0.04f * 0.04f; // match tolerance (~4cm) squared
            const detail::Constraint3* best = nullptr;
            for (auto it = range.first; it != range.second; ++it) {
                const detail::Constraint3& pc = m_prev[it->second];
                const math::vec3 d = pc.point - c.point;
                const float d2 = glm::dot(d, d);
                if (d2 < bestD2) {
                    bestD2 = d2;
                    best = &pc;
                }
            }
            if (best != nullptr) {
                c.an = best->an;
                c.jt1 = best->jt1;
                c.jt2 = best->jt2;
                applyImpulse(c, c.n * c.an + c.t1 * c.jt1 + c.t2 * c.jt2);
            }
        }
    }

    void solveConstraint(detail::Constraint3& c) {
        const Body3D& a = bodies[static_cast<size_t>(c.a)];
        const Body3D& b = bodies[static_cast<size_t>(c.b)];
        auto relVel = [&]() {
            return (b.vel + glm::cross(b.angularVel, c.rB)) - (a.vel + glm::cross(a.angularVel, c.rA));
        };
        // Normal impulse (accumulated, clamped >= 0), targeting the restitution bias velocity.
        {
            const float vn = glm::dot(relVel(), c.n);
            const float dl = c.massN * (c.restitutionBias - vn);
            const float newAn = std::max(c.an + dl, 0.0f);
            const float applied = newAn - c.an;
            c.an = newAn;
            applyImpulse(c, c.n * applied);
        }
        // Friction impulse in the tangent plane, clamped inside the Coulomb cone (|jt| <= mu*an).
        {
            const math::vec3 rv = relVel();
            const float dvt1 = -c.massT1 * glm::dot(rv, c.t1);
            const float dvt2 = -c.massT2 * glm::dot(rv, c.t2);
            float n1 = c.jt1 + dvt1;
            float n2 = c.jt2 + dvt2;
            const float maxF = c.mu * c.an;
            const float m2 = n1 * n1 + n2 * n2;
            if (m2 > maxF * maxF && m2 > 1e-12f) {
                const float s = maxF / std::sqrt(m2);
                n1 *= s;
                n2 *= s;
            }
            const math::vec3 P = c.t1 * (n1 - c.jt1) + c.t2 * (n2 - c.jt2);
            c.jt1 = n1;
            c.jt2 = n2;
            applyImpulse(c, P);
        }
    }

    void correctPosition(const detail::Constraint3& c) {
        Body3D& a = bodies[static_cast<size_t>(c.a)];
        Body3D& b = bodies[static_cast<size_t>(c.b)];
        const float invSum = a.invMass + b.invMass;
        if (invSum <= 0.0f) {
            return;
        }
        const float mag = std::max(c.pen - slop, 0.0f) * correctionPercent / invSum;
        const math::vec3 correction = c.n * mag;
        a.pos -= correction * a.invMass;
        b.pos += correction * b.invMass;
    }

    std::vector<detail::Constraint3> m_prev; // last frame's constraints, for warm starting
};

} // namespace maz::game
