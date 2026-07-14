#pragma once

#include "maz/game/CollisionLayers.hpp"
#include "maz/math/Math.hpp"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
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
    // half-extents `half`, rotated by `orientation`. A Plane is an infinite static half-space (Godot
    // WorldBoundaryShape3D): `normal` is the unit outward normal (toward the free space where bodies
    // live) and `planeD` the plane offset, so the solid region is { p : dot(p, normal) < planeD };
    // build one with makeGroundPlane().
    enum Shape { Sphere, Box, Plane };

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

} // namespace detail

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

    int add(const Body3D& b) {
        bodies.push_back(b);
        return static_cast<int>(bodies.size()) - 1;
    }

    void step(float dt, int iterations = 8) {
        const int n = static_cast<int>(bodies.size());

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

        // Detect contacts (brute-force pairs in D2; broadphase arrives in a later milestone).
        std::vector<detail::Contact3> contacts;
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
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
        }

        // Velocity resolution: several sequential-impulse passes over all contacts.
        for (int it = 0; it < iterations; ++it) {
            for (const detail::Contact3& c : contacts) {
                if (c.hit) {
                    resolveVelocity(c);
                }
            }
        }
        // Positional correction (split from velocity so it never injects energy).
        for (const detail::Contact3& c : contacts) {
            if (c.hit) {
                correctPosition(c);
            }
        }

        // Integrate position + orientation.
        for (Body3D& b : bodies) {
            if (b.invMass > 0.0f) {
                b.pos += b.vel * dt;
                const math::quat wq(0.0f, b.angularVel.x, b.angularVel.y, b.angularVel.z);
                b.orientation = glm::normalize(b.orientation + 0.5f * dt * (wq * b.orientation));
            }
        }
    }

private:
    std::vector<math::mat3> m_invIw; // world-space inverse inertia per body, refreshed each step

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
        }
    }

    void resolveVelocity(const detail::Contact3& c) {
        Body3D& a = bodies[static_cast<size_t>(c.a)];
        Body3D& b = bodies[static_cast<size_t>(c.b)];
        const float invSum = a.invMass + b.invMass;
        if (invSum <= 0.0f) {
            return;
        }
        const math::mat3& invIA = m_invIw[static_cast<size_t>(c.a)];
        const math::mat3& invIB = m_invIw[static_cast<size_t>(c.b)];
        const math::vec3 rA = c.point - a.pos;
        const math::vec3 rB = c.point - b.pos;

        auto relVel = [&]() {
            return (b.vel + glm::cross(b.angularVel, rB)) - (a.vel + glm::cross(a.angularVel, rA));
        };

        // Effective mass along a direction d at the contact point.
        auto effMass = [&](const math::vec3& d) {
            const math::vec3 raxd = glm::cross(rA, d);
            const math::vec3 rbxd = glm::cross(rB, d);
            return invSum + glm::dot(raxd, invIA * raxd) + glm::dot(rbxd, invIB * rbxd);
        };

        const math::vec3 rv = relVel();
        const float vn = glm::dot(rv, c.n);
        if (vn > 0.0f) {
            return; // separating already
        }
        const float kn = effMass(c.n);
        if (kn <= 0.0f) {
            return;
        }
        const float e = (-vn > restitutionThreshold) ? std::min(a.restitution, b.restitution) : 0.0f;
        const float jn = -(1.0f + e) * vn / kn;
        const math::vec3 P = c.n * jn;
        a.vel -= P * a.invMass;
        a.angularVel -= invIA * glm::cross(rA, P);
        b.vel += P * b.invMass;
        b.angularVel += invIB * glm::cross(rB, P);

        // Coulomb friction along the tangent of the (post-normal) relative velocity.
        const math::vec3 rv2 = relVel();
        math::vec3 t = rv2 - c.n * glm::dot(rv2, c.n);
        const float tl = std::sqrt(glm::dot(t, t));
        if (tl > 1e-6f) {
            t /= tl;
            const float kt = effMass(t);
            if (kt > 0.0f) {
                const float jt = -glm::dot(rv2, t) / kt;
                const float mu = std::sqrt(a.friction * b.friction);
                const float jtc = std::clamp(jt, -jn * mu, jn * mu);
                const math::vec3 Pt = t * jtc;
                a.vel -= Pt * a.invMass;
                a.angularVel -= invIA * glm::cross(rA, Pt);
                b.vel += Pt * b.invMass;
                b.angularVel += invIB * glm::cross(rB, Pt);
            }
        }
    }

    void correctPosition(const detail::Contact3& c) {
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
};

} // namespace maz::game
