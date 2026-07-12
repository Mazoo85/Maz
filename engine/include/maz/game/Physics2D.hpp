#pragma once

#include "maz/math/Math.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

namespace maz::game {

// Impulse-based 2D rigid-body dynamics — the layer above collision *detection* (this module resolves
// collisions, not just reports them). Bodies are circles or axis-aligned boxes carrying velocity, an
// inverse mass (0 = immovable/infinite mass), restitution (bounciness), and friction; the world
// integrates gravity, then resolves overlaps with a normal impulse + Coulomb friction impulse + a
// positional correction (so stacks don't sink), and bounces bodies off a static box. Deterministic
// under a fixed timestep and free of GPU/RNG, so it unit-tests headlessly. Coordinates are whatever
// the caller uses (the demos use screen pixels with +y pointing down).

struct Body2D {
    enum Shape { Circle, Box };

    math::vec2 pos{0.0f, 0.0f};
    math::vec2 vel{0.0f, 0.0f};
    int shape = Circle;
    float radius = 0.5f;        // used when shape == Circle
    math::vec2 half{0.5f, 0.5f}; // half-extents when shape == Box
    float invMass = 1.0f;       // 0 => static (infinite mass, never moves)
    float restitution = 0.4f;   // 0 = inelastic, 1 = perfectly bouncy
    float friction = 0.0f;      // Coulomb coefficient (0 = frictionless; default keeps circle demos as-is)

    // --- Rotation (opt-in) -------------------------------------------------------------------------
    // A body carries an orientation and spin, but rotation is *locked* by default: invInertia = 0 means
    // an infinite moment of inertia, so contact impulses produce no torque and the body behaves exactly
    // like the older translation-only rigid body. Call enableRotation() to give it a finite inertia
    // (derived from its shape + mass) and let it tumble. This keeps every existing demo bit-identical.
    float angle = 0.0f;            // orientation, radians (CCW)
    float angularVel = 0.0f;       // spin, radians/sec
    float invInertia = 0.0f;       // 1 / moment-of-inertia; 0 => rotation locked
    float linearDamping = 0.0f;    // per-second velocity decay (0 => none), like Godot's linear_damp
    float angularDamping = 0.0f;   // per-second spin decay (0 => none), like Godot's angular_damp

    // Derive the inverse moment of inertia from the shape + mass so the body can rotate. A solid box of
    // mass m and size w×h has I = m(w²+h²)/12; a disc has I = ½mr². Static bodies (invMass 0) stay locked.
    void enableRotation() {
        if (invMass <= 0.0f) {
            invInertia = 0.0f;
            return;
        }
        const float m = 1.0f / invMass;
        float inertia;
        if (shape == Box) {
            const float w = half.x * 2.0f;
            const float h = half.y * 2.0f;
            inertia = m * (w * w + h * h) / 12.0f;
        } else {
            inertia = 0.5f * m * radius * radius;
        }
        invInertia = inertia > 0.0f ? 1.0f / inertia : 0.0f;
    }
};

struct Bounds2D {
    float minX = 0.0f, minY = 0.0f, maxX = 0.0f, maxY = 0.0f;
};

// A constraint tying two bodies together (or one body to a fixed world point) — Godot's PinJoint2D /
// DampedSpringJoint2D / GrooveJoint2D. A Pin forces the two anchor points to coincide (a hinge / rope
// link); a Spring pulls them toward `restLength` with a stiffness + damping (a soft, bouncy link); a
// Groove constrains body `b`'s anchor to a LINE (the groove) through body `a`'s anchor along `axis`,
// free to slide along it but held on the line (a rail / slider). Anchors are given in each body's local
// (rotated) frame; when `b < 0` a Pin/Spring anchors body `a` to the fixed world point `anchorB` (a
// Groove needs both bodies — make body `a` static for a world-fixed rail). Solved with sequential
// impulses inside the oriented step, so joints and contacts compose.
struct Joint2D {
    enum Type { Pin, Spring, Groove };
    int type = Pin;
    int a = -1;               // first body index (the groove body for Groove)
    int b = -1;               // second body index, or < 0 to anchor to the fixed world point `anchorB`
    math::vec2 localA{0.0f, 0.0f};   // anchor on body a, in a's local frame (a groove reference point)
    math::vec2 anchorB{0.0f, 0.0f};  // anchor on body b (local) if b >= 0, else a fixed world point
    math::vec2 axis{1.0f, 0.0f};     // Groove: slide direction in a's local frame (need not be unit)
    float restLength = 0.0f;  // Spring: target separation
    float stiffness = 0.0f;   // Spring: restoring force per unit stretch
    float damping = 0.0f;     // Spring: velocity damping along the joint axis
};

// --- Contact generation: normal (pointing from a toward b) + penetration depth --------------------
namespace detail {

inline bool contactCircleCircle(const Body2D& a, const Body2D& b, math::vec2& n, float& pen) {
    const math::vec2 d = b.pos - a.pos;
    const float dist2 = glm::dot(d, d);
    const float r = a.radius + b.radius;
    if (dist2 >= r * r) {
        return false;
    }
    const float dist = std::sqrt(dist2);
    n = dist > 1e-6f ? d / dist : math::vec2(1.0f, 0.0f);
    pen = r - dist;
    return true;
}

inline bool contactBoxBox(const Body2D& a, const Body2D& b, math::vec2& n, float& pen) {
    const float dx = b.pos.x - a.pos.x;
    const float dy = b.pos.y - a.pos.y;
    const float ox = (a.half.x + b.half.x) - std::fabs(dx);
    const float oy = (a.half.y + b.half.y) - std::fabs(dy);
    if (ox <= 0.0f || oy <= 0.0f) {
        return false;
    }
    if (ox < oy) { // separate along the axis of least penetration
        n = math::vec2(dx < 0.0f ? -1.0f : 1.0f, 0.0f);
        pen = ox;
    } else {
        n = math::vec2(0.0f, dy < 0.0f ? -1.0f : 1.0f);
        pen = oy;
    }
    return true;
}

// Circle `c` vs box `x`. Returns the normal pointing from the circle toward the box + penetration.
inline bool contactCircleBox(const Body2D& c, const Body2D& x, math::vec2& nCircleToBox, float& pen) {
    const math::vec2 bmin = x.pos - x.half;
    const math::vec2 bmax = x.pos + x.half;
    const math::vec2 closest(glm::clamp(c.pos.x, bmin.x, bmax.x),
                             glm::clamp(c.pos.y, bmin.y, bmax.y));
    const math::vec2 d = c.pos - closest; // box surface -> circle center
    const float dist2 = glm::dot(d, d);
    if (dist2 > c.radius * c.radius) {
        return false;
    }
    if (dist2 > 1e-12f) { // circle center outside the box
        const float dist = std::sqrt(dist2);
        nCircleToBox = -(d / dist); // from circle toward box
        pen = c.radius - dist;
        return true;
    }
    // Circle center is inside the box: push out along the axis of least penetration.
    const float px = x.half.x - std::fabs(c.pos.x - x.pos.x);
    const float py = x.half.y - std::fabs(c.pos.y - x.pos.y);
    if (px < py) {
        nCircleToBox = math::vec2(c.pos.x < x.pos.x ? 1.0f : -1.0f, 0.0f);
        pen = px + c.radius;
    } else {
        nCircleToBox = math::vec2(0.0f, c.pos.y < x.pos.y ? 1.0f : -1.0f);
        pen = py + c.radius;
    }
    return true;
}

} // namespace detail

// Fill (n, pen) for any shape pair; n points from a toward b. Returns false if not overlapping.
inline bool contact(const Body2D& a, const Body2D& b, math::vec2& n, float& pen) {
    if (a.shape == Body2D::Circle && b.shape == Body2D::Circle) {
        return detail::contactCircleCircle(a, b, n, pen);
    }
    if (a.shape == Body2D::Box && b.shape == Body2D::Box) {
        return detail::contactBoxBox(a, b, n, pen);
    }
    if (a.shape == Body2D::Circle) { // a circle, b box
        return detail::contactCircleBox(a, b, n, pen);
    }
    // a box, b circle: compute circle->box then flip so n points a(box)->b(circle).
    math::vec2 nCircleToBox;
    if (!detail::contactCircleBox(b, a, nCircleToBox, pen)) {
        return false;
    }
    n = -nCircleToBox;
    return true;
}

// Apply a normal impulse (+ Coulomb friction) and positional correction for a known contact.
inline void resolveContact(Body2D& a, Body2D& b, const math::vec2& n, float pen) {
    const float invSum = a.invMass + b.invMass;
    if (invSum <= 0.0f) {
        return;
    }
    const math::vec2 rv = b.vel - a.vel;
    const float vn = glm::dot(rv, n);
    if (vn < 0.0f) { // only resolve if closing
        const float e = a.restitution < b.restitution ? a.restitution : b.restitution;
        const float jn = -(1.0f + e) * vn / invSum;
        const math::vec2 impulse = n * jn;
        a.vel -= impulse * a.invMass;
        b.vel += impulse * b.invMass;

        const float mu = std::sqrt(a.friction * b.friction);
        if (mu > 0.0f) {
            const math::vec2 rv2 = b.vel - a.vel;
            const math::vec2 vt = rv2 - n * glm::dot(rv2, n); // tangential relative velocity
            const float tlen = std::sqrt(glm::dot(vt, vt));
            if (tlen > 1e-6f) {
                const math::vec2 tdir = vt / tlen;
                float jt = -glm::dot(rv2, tdir) / invSum;
                const float maxF = mu * jn; // Coulomb cone
                jt = jt < -maxF ? -maxF : (jt > maxF ? maxF : jt);
                const math::vec2 fImpulse = tdir * jt;
                a.vel -= fImpulse * a.invMass;
                b.vel += fImpulse * b.invMass;
            }
        }
    }
    const float slop = 0.01f, percent = 0.8f;
    const float corrMag = (pen - slop > 0.0f ? pen - slop : 0.0f) / invSum * percent;
    const math::vec2 corr = n * corrMag;
    a.pos -= corr * a.invMass;
    b.pos += corr * b.invMass;
}

// Resolve one pair of any shapes. Returns true if they were overlapping.
inline bool collide(Body2D& a, Body2D& b) {
    math::vec2 n;
    float pen;
    if (!contact(a, b, n, pen)) {
        return false;
    }
    resolveContact(a, b, n, pen);
    return true;
}

// Backward-compatible circle-only pair resolution (unchanged numerics).
inline bool collideCircles(Body2D& a, Body2D& b) {
    math::vec2 n;
    float pen;
    if (!detail::contactCircleCircle(a, b, n, pen)) {
        return false;
    }
    resolveContact(a, b, n, pen);
    return true;
}

// Keep a body inside a static box, bouncing its velocity by its restitution on each wall it hits.
// Uses per-axis extents so boxes and circles are both handled.
inline void collideBounds(Body2D& b, const Bounds2D& bounds) {
    if (b.invMass <= 0.0f) {
        return;
    }
    const float ex = b.shape == Body2D::Box ? b.half.x : b.radius;
    const float ey = b.shape == Body2D::Box ? b.half.y : b.radius;
    const float e = b.restitution;
    if (b.pos.x - ex < bounds.minX) {
        b.pos.x = bounds.minX + ex;
        if (b.vel.x < 0.0f) b.vel.x = -b.vel.x * e;
    }
    if (b.pos.x + ex > bounds.maxX) {
        b.pos.x = bounds.maxX - ex;
        if (b.vel.x > 0.0f) b.vel.x = -b.vel.x * e;
    }
    if (b.pos.y - ey < bounds.minY) {
        b.pos.y = bounds.minY + ey;
        if (b.vel.y < 0.0f) b.vel.y = -b.vel.y * e;
    }
    if (b.pos.y + ey > bounds.maxY) {
        b.pos.y = bounds.maxY - ey;
        if (b.vel.y > 0.0f) b.vel.y = -b.vel.y * e;
    }
}

// --- Oriented rigid-body dynamics (rotation) ------------------------------------------------------
// The functions above are translation-only. This layer adds real angular dynamics: oriented boxes,
// contact points, and rotational impulses that spin bodies about those points. It runs only when a
// body has enabled rotation (invInertia > 0); otherwise PhysicsWorld2D uses the exact old path, so
// nothing here can perturb existing scenes. Deterministic (cos/sin/sqrt), so it unit-tests headlessly.
namespace detail {

inline float cross2(math::vec2 a, math::vec2 b) { return a.x * b.y - a.y * b.x; } // z of a×b
inline math::vec2 crossSV(float s, math::vec2 v) { return math::vec2(-s * v.y, s * v.x); } // s×v

// A single contact: normal (from a toward b), penetration depth, and world-space contact point.
struct Manifold {
    math::vec2 n{0.0f, 0.0f};
    float pen = 0.0f;
    math::vec2 point{0.0f, 0.0f};
    bool hit = false;
};

// The four corners of an oriented box, CCW from the (-hx,-hy) local corner.
inline void boxCorners(const Body2D& b, math::vec2 out[4]) {
    const float c = std::cos(b.angle), s = std::sin(b.angle);
    const math::vec2 ax(c, s), ay(-s, c); // local x/y axes in world space
    out[0] = b.pos - ax * b.half.x - ay * b.half.y;
    out[1] = b.pos + ax * b.half.x - ay * b.half.y;
    out[2] = b.pos + ax * b.half.x + ay * b.half.y;
    out[3] = b.pos - ax * b.half.x + ay * b.half.y;
}

// Oriented-box vs oriented-box via SAT. Single contact point = the incident box's deepest vertex.
inline Manifold obbObb(const Body2D& A, const Body2D& B) {
    Manifold m;
    const float ca = std::cos(A.angle), sa = std::sin(A.angle);
    const float cb = std::cos(B.angle), sb = std::sin(B.angle);
    const math::vec2 axes[4] = {{ca, sa}, {-sa, ca}, {cb, sb}, {-sb, cb}};
    math::vec2 cornersA[4], cornersB[4];
    boxCorners(A, cornersA);
    boxCorners(B, cornersB);

    float minOverlap = 1e30f;
    int bestAxis = 0;
    for (int i = 0; i < 4; ++i) {
        const math::vec2 ax = axes[i];
        float minA = 1e30f, maxA = -1e30f, minB = 1e30f, maxB = -1e30f;
        for (int k = 0; k < 4; ++k) {
            const float pa = glm::dot(cornersA[k], ax);
            const float pb = glm::dot(cornersB[k], ax);
            minA = pa < minA ? pa : minA;
            maxA = pa > maxA ? pa : maxA;
            minB = pb < minB ? pb : minB;
            maxB = pb > maxB ? pb : maxB;
        }
        const float overlap = (maxA < maxB ? maxA : maxB) - (minA > minB ? minA : minB);
        if (overlap <= 0.0f) {
            return m; // separating axis found -> no collision
        }
        if (overlap < minOverlap) {
            minOverlap = overlap;
            bestAxis = i;
        }
    }
    math::vec2 n = axes[bestAxis];
    if (glm::dot(B.pos - A.pos, n) < 0.0f) {
        n = -n; // orient from A toward B
    }
    // Contact point = deepest penetrating vertex of the incident box (the box that did NOT own the
    // separating axis). If bestAxis is one of A's faces, B is incident (find B's vertex furthest along
    // -n, i.e. deepest into A); otherwise A is incident (A's vertex furthest along +n, into B).
    math::vec2 point{0.0f, 0.0f};
    if (bestAxis < 2) {
        float best = 1e30f;
        for (int k = 0; k < 4; ++k) {
            const float d = glm::dot(cornersB[k], n);
            if (d < best) { best = d; point = cornersB[k]; }
        }
    } else {
        float best = -1e30f;
        for (int k = 0; k < 4; ++k) {
            const float d = glm::dot(cornersA[k], n);
            if (d > best) { best = d; point = cornersA[k]; }
        }
    }
    m.n = n;
    m.pen = minOverlap;
    m.point = point;
    m.hit = true;
    return m;
}

// Circle `c` vs oriented box `x`; normal points from a(circle) toward b(box).
inline Manifold circleObb(const Body2D& c, const Body2D& x) {
    Manifold m;
    const float ct = std::cos(x.angle), st = std::sin(x.angle);
    const math::vec2 d = c.pos - x.pos;
    const math::vec2 local(d.x * ct + d.y * st, -d.x * st + d.y * ct); // rotate into box frame
    const math::vec2 closest(glm::clamp(local.x, -x.half.x, x.half.x),
                             glm::clamp(local.y, -x.half.y, x.half.y));
    const math::vec2 diff = local - closest;
    const float dist2 = glm::dot(diff, diff);
    if (dist2 > c.radius * c.radius) {
        return m;
    }
    const float dist = std::sqrt(dist2);
    math::vec2 nLocal = dist > 1e-6f ? diff / dist : math::vec2(1.0f, 0.0f); // box surface -> circle
    // World-space contact point + normal (rotate local back out).
    const math::vec2 pWorld(x.pos.x + closest.x * ct - closest.y * st,
                            x.pos.y + closest.x * st + closest.y * ct);
    const math::vec2 nWorld(nLocal.x * ct - nLocal.y * st, nLocal.x * st + nLocal.y * ct);
    m.n = -nWorld; // from circle toward box
    m.pen = c.radius - dist;
    m.point = pWorld;
    m.hit = true;
    return m;
}

// Any oriented shape pair; n points from a toward b, with a world contact point.
inline Manifold manifold(const Body2D& a, const Body2D& b) {
    if (a.shape == Body2D::Box && b.shape == Body2D::Box) {
        return obbObb(a, b);
    }
    if (a.shape == Body2D::Circle && b.shape == Body2D::Circle) {
        Manifold m;
        math::vec2 n;
        float pen;
        if (!contactCircleCircle(a, b, n, pen)) {
            return m;
        }
        m.n = n;
        m.pen = pen;
        m.point = a.pos + n * a.radius; // on a's surface toward b
        m.hit = true;
        return m;
    }
    if (a.shape == Body2D::Circle) { // a circle, b box
        return circleObb(a, b);
    }
    Manifold m = circleObb(b, a); // a box, b circle: compute circle->box then flip
    m.n = -m.n;
    return m;
}

// Apply a rotational normal + friction impulse at the contact point, then positional correction.
inline void resolveRot(Body2D& a, Body2D& b, const Manifold& mf) {
    const math::vec2 n = mf.n;
    const math::vec2 ra = mf.point - a.pos;
    const math::vec2 rb = mf.point - b.pos;
    const float raxn = cross2(ra, n), rbxn = cross2(rb, n);
    const float invSum =
        a.invMass + b.invMass + raxn * raxn * a.invInertia + rbxn * rbxn * b.invInertia;
    if (invSum <= 0.0f) {
        return;
    }
    const math::vec2 va = a.vel + crossSV(a.angularVel, ra);
    const math::vec2 vb = b.vel + crossSV(b.angularVel, rb);
    const math::vec2 rv = vb - va;
    const float vn = glm::dot(rv, n);
    if (vn < 0.0f) { // closing
        const float e = a.restitution < b.restitution ? a.restitution : b.restitution;
        const float jn = -(1.0f + e) * vn / invSum;
        const math::vec2 imp = n * jn;
        a.vel -= imp * a.invMass;
        a.angularVel -= a.invInertia * cross2(ra, imp);
        b.vel += imp * b.invMass;
        b.angularVel += b.invInertia * cross2(rb, imp);

        const float mu = std::sqrt(a.friction * b.friction);
        if (mu > 0.0f) {
            const math::vec2 va2 = a.vel + crossSV(a.angularVel, ra);
            const math::vec2 vb2 = b.vel + crossSV(b.angularVel, rb);
            const math::vec2 rv2 = vb2 - va2;
            math::vec2 t = rv2 - n * glm::dot(rv2, n);
            const float tl = std::sqrt(glm::dot(t, t));
            if (tl > 1e-6f) {
                t /= tl;
                const float raxt = cross2(ra, t), rbxt = cross2(rb, t);
                const float invSumT =
                    a.invMass + b.invMass + raxt * raxt * a.invInertia + rbxt * rbxt * b.invInertia;
                float jt = -glm::dot(rv2, t) / invSumT;
                const float maxF = mu * jn;
                jt = jt < -maxF ? -maxF : (jt > maxF ? maxF : jt);
                const math::vec2 fimp = t * jt;
                a.vel -= fimp * a.invMass;
                a.angularVel -= a.invInertia * cross2(ra, fimp);
                b.vel += fimp * b.invMass;
                b.angularVel += b.invInertia * cross2(rb, fimp);
            }
        }
    }
    const float slop = 0.01f, percent = 0.8f;
    const float corrMag = (mf.pen - slop > 0.0f ? mf.pen - slop : 0.0f) / invSum * percent;
    const math::vec2 corr = n * corrMag;
    a.pos -= corr * a.invMass;
    b.pos += corr * b.invMass;
}

// Rotate a local offset into world space by a body's orientation.
inline math::vec2 rotate2(math::vec2 v, float angle) {
    const float c = std::cos(angle), s = std::sin(angle);
    return math::vec2(v.x * c - v.y * s, v.x * s + v.y * c);
}

// Point-to-point (pin) constraint: drive the two world anchors together. `b` may be null (anchor `a`
// to the fixed world point `worldB`). Standard 2x2 effective-mass solve + Baumgarte position bias.
inline void solvePin(Body2D& a, Body2D* b, math::vec2 localA, math::vec2 localBOrWorld, float dt) {
    const math::vec2 rA = rotate2(localA, a.angle);
    const math::vec2 worldA = a.pos + rA;
    math::vec2 rB{0.0f, 0.0f}, worldB = localBOrWorld;
    float bInvM = 0.0f, bInvI = 0.0f;
    if (b != nullptr) {
        rB = rotate2(localBOrWorld, b->angle);
        worldB = b->pos + rB;
        bInvM = b->invMass;
        bInvI = b->invInertia;
    }
    const math::vec2 vA = a.vel + crossSV(a.angularVel, rA);
    const math::vec2 vB = b ? (b->vel + crossSV(b->angularVel, rB)) : math::vec2(0.0f, 0.0f);
    const math::vec2 cdot = vB - vA;

    const float im = a.invMass + bInvM;
    const float iA = a.invInertia, iB = bInvI;
    const float k11 = im + iA * rA.y * rA.y + iB * rB.y * rB.y;
    const float k12 = -iA * rA.x * rA.y - iB * rB.x * rB.y;
    const float k22 = im + iA * rA.x * rA.x + iB * rB.x * rB.x;
    const float det = k11 * k22 - k12 * k12;
    if (std::fabs(det) < 1e-12f) {
        return;
    }
    const float beta = 0.2f; // position drift correction
    const math::vec2 bias = (worldB - worldA) * (beta / dt);
    const math::vec2 rhs = -(cdot + bias);
    const math::vec2 p((k22 * rhs.x - k12 * rhs.y) / det, (-k12 * rhs.x + k11 * rhs.y) / det);

    a.vel -= p * a.invMass;
    a.angularVel -= a.invInertia * cross2(rA, p);
    if (b != nullptr) {
        b->vel += p * bInvM;
        b->angularVel += bInvI * cross2(rB, p);
    }
}

// Damped spring: a soft restoring force along the joint axis toward `rest`. `b` may be null.
inline void solveSpring(Body2D& a, Body2D* b, math::vec2 localA, math::vec2 localBOrWorld, float rest,
                        float stiff, float damp, float dt) {
    const math::vec2 rA = rotate2(localA, a.angle);
    const math::vec2 worldA = a.pos + rA;
    math::vec2 rB{0.0f, 0.0f}, worldB = localBOrWorld;
    float bInvM = 0.0f, bInvI = 0.0f;
    if (b != nullptr) {
        rB = rotate2(localBOrWorld, b->angle);
        worldB = b->pos + rB;
        bInvM = b->invMass;
        bInvI = b->invInertia;
    }
    math::vec2 d = worldB - worldA;
    const float len = std::sqrt(glm::dot(d, d));
    if (len < 1e-6f) {
        return;
    }
    const math::vec2 n = d / len;
    const float c = len - rest;
    const math::vec2 vA = a.vel + crossSV(a.angularVel, rA);
    const math::vec2 vB = b ? (b->vel + crossSV(b->angularVel, rB)) : math::vec2(0.0f, 0.0f);
    const float vrel = glm::dot(vB - vA, n);
    const float force = -stiff * c - damp * vrel; // along n (a -> b)
    const math::vec2 p = n * (force * dt);

    a.vel -= p * a.invMass;
    a.angularVel -= a.invInertia * cross2(rA, p);
    if (b != nullptr) {
        b->vel += p * bInvM;
        b->angularVel += bInvI * cross2(rB, p);
    }
}

// Groove (slider) constraint: hold slider `s`'s anchor on the LINE through groove-body `g`'s anchor
// along `axisLocal`, free to slide along it (Godot GrooveJoint2D). A single perpendicular-impulse
// solve + Baumgarte position bias removes only the off-line motion. Make `g` static for a world rail.
inline void solveGroove(Body2D& g, Body2D& s, math::vec2 grooveAnchorLocal, math::vec2 axisLocal,
                        math::vec2 sliderAnchorLocal, float dt) {
    const math::vec2 refA = g.pos + rotate2(grooveAnchorLocal, g.angle);
    math::vec2 axis = rotate2(axisLocal, g.angle);
    const float al = std::sqrt(glm::dot(axis, axis));
    if (al < 1e-6f) {
        return;
    }
    axis /= al;
    const math::vec2 perp(-axis.y, axis.x); // constraint acts along the groove normal
    const math::vec2 rS = rotate2(sliderAnchorLocal, s.angle);
    const math::vec2 worldS = s.pos + rS;
    // Constraint point on the groove = projection of the slider anchor onto the line (correct arms).
    const float along = glm::dot(worldS - refA, axis);
    const math::vec2 proj = refA + axis * along;
    const math::vec2 rA = proj - g.pos;
    const math::vec2 rB = worldS - s.pos;
    const float c = glm::dot(worldS - refA, perp); // signed off-line distance to drive to 0

    const math::vec2 vA = g.vel + crossSV(g.angularVel, rA);
    const math::vec2 vB = s.vel + crossSV(s.angularVel, rB);
    const float cdot = glm::dot(vB - vA, perp);

    const float raxp = cross2(rA, perp), rbxp = cross2(rB, perp);
    const float k = g.invMass + s.invMass + g.invInertia * raxp * raxp + s.invInertia * rbxp * rbxp;
    if (k < 1e-12f) {
        return;
    }
    const float beta = 0.2f;
    const float bias = (beta / dt) * c;
    const float lambda = -(cdot + bias) / k;
    const math::vec2 p = perp * lambda;

    g.vel -= p * g.invMass;
    g.angularVel -= g.invInertia * cross2(rA, p);
    s.vel += p * s.invMass;
    s.angularVel += s.invInertia * cross2(rB, p);
}

} // namespace detail

class PhysicsWorld2D {
public:
    math::vec2 gravity{0.0f, 0.0f};
    Bounds2D bounds{};
    bool hasBounds = false;
    std::vector<Body2D> bodies;
    std::vector<Joint2D> joints;

    uint32_t add(const Body2D& b) {
        bodies.push_back(b);
        return static_cast<uint32_t>(bodies.size() - 1);
    }

    uint32_t addJoint(const Joint2D& j) {
        joints.push_back(j);
        return static_cast<uint32_t>(joints.size() - 1);
    }

    // Advance the simulation by dt: integrate gravity + motion, then resolve contacts for `iterations`
    // passes (more iterations = stiffer stacks). Body-body pairs are resolved before the walls.
    // If any body has rotation enabled (invInertia > 0) the oriented rigid-body solver runs; otherwise
    // the exact translation-only path below runs, keeping every non-rotating scene bit-identical.
    void step(float dt, int iterations = 4) {
        if (!joints.empty()) {
            stepRotational(dt, iterations);
            return;
        }
        for (const Body2D& b : bodies) {
            if (b.invInertia > 0.0f) {
                stepRotational(dt, iterations);
                return;
            }
        }
        for (Body2D& b : bodies) {
            if (b.invMass > 0.0f) {
                b.vel += gravity * dt;
            }
        }
        for (Body2D& b : bodies) {
            b.pos += b.vel * dt;
        }
        for (int it = 0; it < iterations; ++it) {
            for (size_t i = 0; i < bodies.size(); ++i) {
                for (size_t j = i + 1; j < bodies.size(); ++j) {
                    collide(bodies[i], bodies[j]);
                }
            }
            if (hasBounds) {
                for (Body2D& b : bodies) {
                    collideBounds(b, bounds);
                }
            }
        }
    }

private:
    // Oriented rigid-body integration: gravity + linear/angular damping, then advance position AND
    // orientation, then resolve oriented contacts with rotational impulses. Static walls are modelled
    // as static box bodies (invMass 0), so a box corner striking a wall imparts the right spin.
    void stepRotational(float dt, int iterations) {
        for (Body2D& b : bodies) {
            if (b.invMass > 0.0f) {
                b.vel += gravity * dt;
                b.vel *= 1.0f / (1.0f + b.linearDamping * dt);
                b.angularVel *= 1.0f / (1.0f + b.angularDamping * dt);
            }
        }
        for (Body2D& b : bodies) {
            b.pos += b.vel * dt;
            b.angle += b.angularVel * dt;
        }
        for (int it = 0; it < iterations; ++it) {
            for (size_t i = 0; i < bodies.size(); ++i) {
                for (size_t j = i + 1; j < bodies.size(); ++j) {
                    detail::Manifold m = detail::manifold(bodies[i], bodies[j]);
                    if (m.hit) {
                        detail::resolveRot(bodies[i], bodies[j], m);
                    }
                }
            }
            for (const Joint2D& jt : joints) {
                if (jt.a < 0 || jt.a >= static_cast<int>(bodies.size())) {
                    continue;
                }
                Body2D& A = bodies[static_cast<size_t>(jt.a)];
                Body2D* B = (jt.b >= 0 && jt.b < static_cast<int>(bodies.size()))
                                ? &bodies[static_cast<size_t>(jt.b)]
                                : nullptr;
                if (jt.type == Joint2D::Pin) {
                    detail::solvePin(A, B, jt.localA, jt.anchorB, dt);
                } else if (jt.type == Joint2D::Groove) {
                    if (B != nullptr) {
                        detail::solveGroove(A, *B, jt.localA, jt.axis, jt.anchorB, dt);
                    }
                } else {
                    detail::solveSpring(A, B, jt.localA, jt.anchorB, jt.restLength, jt.stiffness,
                                        jt.damping, dt);
                }
            }
            if (hasBounds) {
                for (Body2D& b : bodies) {
                    collideBounds(b, bounds);
                }
            }
        }
    }
};

} // namespace maz::game
