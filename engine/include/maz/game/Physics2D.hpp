#pragma once

#include "maz/game/CollisionLayers.hpp"
#include "maz/math/Math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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

    // --- Sleeping (opt-in via PhysicsWorld2D::allowSleep) ------------------------------------------
    // A body that stays below the world's sleep thresholds for long enough goes to sleep: it stops
    // integrating and solving (zero CPU) until something touches or wakes it. Godot's can_sleep /
    // sleeping. sleepTimer accumulates quiet time; sleeping is the current state. Untouched by the
    // legacy solvers, so default behaviour is unchanged.
    float sleepTimer = 0.0f;
    bool sleeping = false;

    // --- Collision filtering (Godot collision_layer / collision_mask) ------------------------------
    // `collisionLayer` = which layers this body lives in ("what I am"); `collisionMask` = which layers
    // it reacts to ("what I hit"). Two bodies are pair-tested only if either scans the other's layer
    // (game::interact). Both default to every bit set, i.e. collide-with-everything, so a body that
    // never touches these fields behaves exactly as before.
    LayerMask collisionLayer = ~0u;
    LayerMask collisionMask = ~0u;

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

// --- Two-point contact manifolds (stable stacks) --------------------------------------------------
// obbObb above returns a SINGLE contact point (the deepest incident vertex). That is enough to stop
// two boxes overlapping, but a single point carries no torque balance, so an oriented box resting on
// another slowly rotates off and the stack topples. Real engines (Box2D, Godot) generate a TWO-point
// manifold along the shared face by reference/incident-face clipping, so both far ends of the contact
// are held and the stack stays square. This is that clip. It is opt-in (PhysicsWorld2D::solveManifolds)
// so every existing rotating scene keeps the exact single-point numerics; new scenes switch it on.
struct Contact2 {
    math::vec2 n{0.0f, 0.0f}; // from a toward b
    int count = 0;
    math::vec2 point[2]{};
    float pen[2]{0.0f, 0.0f};
    bool hit = false;
};

// Pick the edge of a CCW box (corners c0..c3) whose outward normal best matches `dir`; return its two
// vertices in perimeter order. Outward normal of edge (p,q) on a CCW polygon is (e.y, -e.x).
inline void bestFace(const math::vec2 c[4], math::vec2 dir, math::vec2& v0, math::vec2& v1) {
    float best = -1e30f;
    for (int i = 0; i < 4; ++i) {
        const math::vec2 p = c[i];
        const math::vec2 q = c[(i + 1) & 3];
        const math::vec2 e = q - p;
        const math::vec2 outward(e.y, -e.x);
        const float d = glm::dot(outward, dir);
        if (d > best) {
            best = d;
            v0 = p;
            v1 = q;
        }
    }
}

// Clip segment (in[0],in[1]) to the half-plane { x : dot(nrm, x) <= offset }. Keeps inside endpoints and
// adds the crossing point when the segment straddles the plane. Returns 0..2 output points.
inline int clipSegment(math::vec2 out[2], const math::vec2 in[2], math::vec2 nrm, float offset) {
    int o = 0;
    const float d0 = glm::dot(nrm, in[0]) - offset;
    const float d1 = glm::dot(nrm, in[1]) - offset;
    if (d0 <= 0.0f && o < 2) out[o++] = in[0];
    if (d1 <= 0.0f && o < 2) out[o++] = in[1];
    if (d0 * d1 < 0.0f && o < 2) {
        const float t = d0 / (d0 - d1);
        out[o++] = in[0] + (in[1] - in[0]) * t;
    }
    return o;
}

// Oriented-box vs oriented-box, returning up to TWO contact points along the shared face (n from a→b).
inline Contact2 obbObbManifold(const Body2D& A, const Body2D& B) {
    Contact2 m;
    const float ca = std::cos(A.angle), sa = std::sin(A.angle);
    const float cb = std::cos(B.angle), sb = std::sin(B.angle);
    const math::vec2 axes[4] = {{ca, sa}, {-sa, ca}, {cb, sb}, {-sb, cb}};
    math::vec2 cA[4], cB[4];
    boxCorners(A, cA);
    boxCorners(B, cB);

    float minOverlap = 1e30f;
    int best = 0;
    for (int i = 0; i < 4; ++i) {
        const math::vec2 ax = axes[i];
        float minA = 1e30f, maxA = -1e30f, minB = 1e30f, maxB = -1e30f;
        for (int k = 0; k < 4; ++k) {
            const float pa = glm::dot(cA[k], ax);
            const float pb = glm::dot(cB[k], ax);
            minA = pa < minA ? pa : minA;
            maxA = pa > maxA ? pa : maxA;
            minB = pb < minB ? pb : minB;
            maxB = pb > maxB ? pb : maxB;
        }
        const float overlap = (maxA < maxB ? maxA : maxB) - (minA > minB ? minA : minB);
        if (overlap <= 0.0f) {
            return m; // separating axis -> no contact
        }
        if (overlap < minOverlap) {
            minOverlap = overlap;
            best = i;
        }
    }
    math::vec2 n = axes[best];
    if (glm::dot(B.pos - A.pos, n) < 0.0f) {
        n = -n; // orient a -> b
    }
    m.n = n;

    // Reference box owns the separating axis; the other is incident. refN is the reference face's
    // outward normal (pointing toward the incident box).
    const bool refIsA = best < 2;
    const math::vec2* refC = refIsA ? cA : cB;
    const math::vec2* incC = refIsA ? cB : cA;
    const math::vec2 refN = refIsA ? n : -n;

    math::vec2 rv0, rv1, iv0, iv1;
    bestFace(refC, refN, rv0, rv1);   // reference face (its outward normal ~ refN)
    bestFace(incC, -refN, iv0, iv1);  // incident face (most anti-parallel to refN)

    math::vec2 tangent = rv1 - rv0;
    const float tl = std::sqrt(glm::dot(tangent, tangent));
    if (tl < 1e-9f) {
        return m;
    }
    tangent /= tl;

    // Clip the incident segment to the reference face's two side planes.
    math::vec2 seg[2] = {iv0, iv1};
    math::vec2 tmp[2];
    if (clipSegment(tmp, seg, -tangent, -glm::dot(tangent, rv0)) < 2) {
        return m;
    }
    math::vec2 clipped[2];
    if (clipSegment(clipped, tmp, tangent, glm::dot(tangent, rv1)) < 2) {
        return m;
    }

    // Keep clipped points that lie behind the reference face; penetration = depth behind it.
    const float refOffset = glm::dot(refN, rv0);
    for (int k = 0; k < 2; ++k) {
        const float sep = glm::dot(refN, clipped[k]) - refOffset;
        if (sep <= 0.0f && m.count < 2) {
            m.point[m.count] = clipped[k];
            m.pen[m.count] = -sep;
            ++m.count;
        }
    }
    m.hit = m.count > 0;
    return m;
}

// Any oriented pair as a Contact2. Box-box uses the two-point clip; other pairs reuse the single-point
// manifold() (count 1), so the manifold solver handles mixed scenes uniformly.
inline Contact2 manifold2(const Body2D& a, const Body2D& b) {
    if (a.shape == Body2D::Box && b.shape == Body2D::Box) {
        return obbObbManifold(a, b);
    }
    Contact2 m;
    const Manifold s = manifold(a, b);
    if (s.hit) {
        m.n = s.n;
        m.count = 1;
        m.point[0] = s.point;
        m.pen[0] = s.pen;
        m.hit = true;
    }
    return m;
}

// Resolve a multi-point manifold by applying the (tested) single-point rotational solve at each contact
// point in turn. Two points sharing a face give the torque balance that keeps a stack square.
inline void resolveManifold(Body2D& a, Body2D& b, const Contact2& m) {
    for (int k = 0; k < m.count; ++k) {
        Manifold s;
        s.n = m.n;
        s.point = m.point[k];
        s.pen = m.pen[k];
        s.hit = true;
        resolveRot(a, b, s);
    }
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

// --- Warm-started sequential-impulse contact constraint (Box2D-style) ------------------------------
// The resolvers above recompute a fresh impulse from scratch every iteration and every frame, so a
// tall stack needs many iterations to stop sinking and jittering. A real engine (Box2D, and Godot's
// GodotPhysics2D) instead keeps a persistent per-contact ACCUMULATED impulse that (a) is clamped as a
// running total — the Coulomb friction cone bounds the accumulated tangent impulse against the
// accumulated normal impulse, not a single iteration's — and (b) carries across frames as a "warm
// start", so the first iteration each step already applies almost the right force. That is what lets
// a stack stay rigid at a handful of iterations. Position error is removed by a Baumgarte bias folded
// into the normal target and gated by a slop so a resting contact does not buzz. This whole path is
// opt-in (PhysicsWorld2D::warmStarting) so every existing scene keeps its exact prior numerics.
struct ContactConstraint {
    int a = -1, b = -1;
    math::vec2 n{0.0f, 0.0f}; // from a toward b
    int count = 0;
    math::vec2 rA[2]{}, rB[2]{}; // contact arms from each body's centre
    float pen[2]{0.0f, 0.0f};
    float massN[2]{0.0f, 0.0f};   // effective normal mass per point
    float massT[2]{0.0f, 0.0f};   // effective tangent mass per point
    float restBias[2]{0.0f, 0.0f}; // restitution target velocity per point
    float jN[2]{0.0f, 0.0f};      // accumulated normal impulse (warm-started)
    float jT[2]{0.0f, 0.0f};      // accumulated tangent impulse (warm-started)
    float jBias[2]{0.0f, 0.0f};   // accumulated split-impulse position impulse (NOT warm-started)
    float e = 0.0f, mu = 0.0f;
    uint64_t key = 0; // (a,b) pair id for frame-to-frame warm-start matching
};

inline uint64_t pairKey(int a, int b) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(a)) << 32) | static_cast<uint32_t>(b);
}

// Precompute a constraint from a fresh manifold: effective masses, contact arms, and the restitution
// target from the CURRENT approach velocity (so a fast impact bounces, a resting stack does not).
inline ContactConstraint buildConstraint(int ia, const Body2D& a, int ib, const Body2D& b,
                                         const Contact2& m, float restThreshold) {
    ContactConstraint c;
    c.a = ia;
    c.b = ib;
    c.n = m.n;
    c.count = m.count;
    c.key = pairKey(ia, ib);
    c.e = a.restitution < b.restitution ? a.restitution : b.restitution;
    c.mu = std::sqrt(a.friction * b.friction);
    const math::vec2 t(-m.n.y, m.n.x);
    for (int k = 0; k < m.count; ++k) {
        const math::vec2 rA = m.point[k] - a.pos;
        const math::vec2 rB = m.point[k] - b.pos;
        c.rA[k] = rA;
        c.rB[k] = rB;
        c.pen[k] = m.pen[k];
        const float rnA = cross2(rA, m.n), rnB = cross2(rB, m.n);
        const float kN = a.invMass + b.invMass + a.invInertia * rnA * rnA + b.invInertia * rnB * rnB;
        c.massN[k] = kN > 0.0f ? 1.0f / kN : 0.0f;
        const float rtA = cross2(rA, t), rtB = cross2(rB, t);
        const float kT = a.invMass + b.invMass + a.invInertia * rtA * rtA + b.invInertia * rtB * rtB;
        c.massT[k] = kT > 0.0f ? 1.0f / kT : 0.0f;
        const math::vec2 va = a.vel + crossSV(a.angularVel, rA);
        const math::vec2 vb = b.vel + crossSV(b.angularVel, rB);
        const float vn = glm::dot(vb - va, m.n);
        c.restBias[k] = vn < -restThreshold ? -c.e * vn : 0.0f;
    }
    return c;
}

// Seed the solve by re-applying last frame's accumulated impulses (the warm start).
inline void warmStart(Body2D& a, Body2D& b, const ContactConstraint& c) {
    const math::vec2 t(-c.n.y, c.n.x);
    for (int k = 0; k < c.count; ++k) {
        const math::vec2 P = c.n * c.jN[k] + t * c.jT[k];
        a.vel -= P * a.invMass;
        a.angularVel -= a.invInertia * cross2(c.rA[k], P);
        b.vel += P * b.invMass;
        b.angularVel += b.invInertia * cross2(c.rB[k], P);
    }
}

// One VELOCITY iteration: friction first (cone bounded by the accumulated normal impulse), then the
// normal impulse toward the restitution target only. Position error is handled separately by
// solveBias so it never injects energy into the real velocity (a resting stack truly comes to rest).
inline void solveVelocity(Body2D& a, Body2D& b, ContactConstraint& c) {
    const math::vec2 t(-c.n.y, c.n.x);
    for (int k = 0; k < c.count; ++k) {
        { // Friction.
            const math::vec2 va = a.vel + crossSV(a.angularVel, c.rA[k]);
            const math::vec2 vb = b.vel + crossSV(b.angularVel, c.rB[k]);
            const float vt = glm::dot(vb - va, t);
            const float maxT = c.mu * c.jN[k];
            const float old = c.jT[k];
            c.jT[k] = glm::clamp(old + c.massT[k] * (-vt), -maxT, maxT);
            const math::vec2 P = t * (c.jT[k] - old);
            a.vel -= P * a.invMass;
            a.angularVel -= a.invInertia * cross2(c.rA[k], P);
            b.vel += P * b.invMass;
            b.angularVel += b.invInertia * cross2(c.rB[k], P);
        }
        { // Normal.
            const math::vec2 va = a.vel + crossSV(a.angularVel, c.rA[k]);
            const math::vec2 vb = b.vel + crossSV(b.angularVel, c.rB[k]);
            const float vn = glm::dot(vb - va, c.n);
            const float old = c.jN[k];
            const float sum = old + c.massN[k] * (c.restBias[k] - vn);
            c.jN[k] = sum > 0.0f ? sum : 0.0f;
            const math::vec2 P = c.n * (c.jN[k] - old);
            a.vel -= P * a.invMass;
            a.angularVel -= a.invInertia * cross2(c.rA[k], P);
            b.vel += P * b.invMass;
            b.angularVel += b.invInertia * cross2(c.rB[k], P);
        }
    }
}

// One POSITION iteration (Bullet-style split impulse): solve a Baumgarte bias against a pair of
// pseudo-velocities (pv/pw) that are integrated into position but never touch the real velocity, so
// pushing bodies out of penetration adds no bounce energy. Slop leaves a small allowed overlap so a
// resting contact does not buzz.
inline void solveBias(const Body2D& a, math::vec2& pvA, float& pwA, const Body2D& b, math::vec2& pvB,
                      float& pwB, ContactConstraint& c, float baumgarte, float slop, float invDt) {
    for (int k = 0; k < c.count; ++k) {
        const float bias = baumgarte * invDt * (c.pen[k] - slop > 0.0f ? c.pen[k] - slop : 0.0f);
        const math::vec2 va = pvA + crossSV(pwA, c.rA[k]);
        const math::vec2 vb = pvB + crossSV(pwB, c.rB[k]);
        const float vn = glm::dot(vb - va, c.n);
        const float old = c.jBias[k];
        const float sum = old + c.massN[k] * (bias - vn);
        c.jBias[k] = sum > 0.0f ? sum : 0.0f;
        const math::vec2 P = c.n * (c.jBias[k] - old);
        pvA -= P * a.invMass;
        pwA -= a.invInertia * cross2(c.rA[k], P);
        pvB += P * b.invMass;
        pwB += b.invInertia * cross2(c.rB[k], P);
    }
}

} // namespace detail

class PhysicsWorld2D {
public:
    math::vec2 gravity{0.0f, 0.0f};
    Bounds2D bounds{};
    bool hasBounds = false;
    // Opt-in: resolve oriented box-box contacts with two-point clipped manifolds (stable stacks). Off by
    // default so every existing rotating scene keeps its exact single-point numerics.
    bool solveManifolds = false;
    // Opt-in: Box2D-style warm-started accumulated-impulse solver (see detail::ContactConstraint). Off
    // by default so every existing scene keeps its exact prior numerics; when on, a stack stays rigid
    // at a handful of iterations where the from-scratch resolvers need many. Implies two-point
    // manifolds internally. baumgarte/slop/restitutionThreshold tune the solve.
    bool warmStarting = false;
    float baumgarte = 0.2f;            // position-error correction gain
    float slop = 0.005f;               // penetration tolerated before correction kicks in (anti-jitter)
    float restitutionThreshold = 1.0f; // approach speed below which restitution is ignored (resting)
    // Opt-in (warm-solver path only): a uniform spatial-hash broadphase replaces the O(n²) all-pairs
    // scan, so scenes with many bodies scale. It yields the SAME contacts in the SAME order as brute
    // force (candidate pairs are sorted by index), so results are bit-identical — just faster.
    bool broadphase = false;
    float broadphaseCellSize = 0.0f; // grid cell size; 0 => default (128 world units)
    // Opt-in (warm-solver path only): bodies that stay quiet for `sleepTime` go to sleep and are
    // skipped by integration + the solver until woken, saving CPU on settled piles (Godot can_sleep).
    // Bodies connected by a contact or joint form an island that sleeps/wakes together, so a resting
    // stack sleeps as a unit and a disturbance to any member wakes the whole thing.
    bool allowSleep = false;
    float sleepLinearThreshold = 14.0f;  // |velocity| below this counts as quiet (world units/sec)
    float sleepAngularThreshold = 0.25f; // |spin| below this counts as quiet (rad/sec)
    float sleepTime = 0.5f;              // seconds a whole island must stay quiet before it sleeps
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

    // Wake a body (clear its sleep state + quiet timer). Call after teleporting a body or changing its
    // velocity externally, so the sleep system re-evaluates it — Godot wakes bodies on such changes too.
    void wake(uint32_t i) {
        if (i < bodies.size()) {
            bodies[i].sleeping = false;
            bodies[i].sleepTimer = 0.0f;
        }
    }

    // Advance the simulation by dt: integrate gravity + motion, then resolve contacts for `iterations`
    // passes (more iterations = stiffer stacks). Body-body pairs are resolved before the walls.
    // If any body has rotation enabled (invInertia > 0) the oriented rigid-body solver runs; otherwise
    // the exact translation-only path below runs, keeping every non-rotating scene bit-identical.
    void step(float dt, int iterations = 4) {
        if (warmStarting) {
            stepSolver(dt, iterations);
            return;
        }
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
    // Candidate body-pair list for the warm solver. Without broadphase this is the full O(n²) set;
    // with it, a uniform spatial hash returns only pairs whose (rotation-conservative) AABBs share a
    // cell. Either way the list is sorted ascending by (i,j) so the downstream contact resolution order
    // — and hence the result — is identical to brute force; broadphase only skips pairs that could not
    // possibly touch. Bodies far larger than a cell go in a "large" list tested against everything, so
    // a big static floor never floods the grid.
    std::vector<std::pair<int, int>> collectPairs() const {
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
        auto extent = [](const Body2D& b) {
            return b.shape == Body2D::Box ? std::sqrt(b.half.x * b.half.x + b.half.y * b.half.y)
                                          : b.radius;
        };
        const float cell = broadphaseCellSize > 0.0f ? broadphaseCellSize : 128.0f;
        const float invCell = 1.0f / cell;
        auto cellKey = [](int cx, int cy) {
            return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) |
                   static_cast<uint32_t>(cy);
        };
        std::unordered_map<uint64_t, std::vector<int>> grid;
        std::vector<int> large;
        for (int i = 0; i < n; ++i) {
            const float e = extent(bodies[i]);
            const int minX = static_cast<int>(std::floor((bodies[i].pos.x - e) * invCell));
            const int maxX = static_cast<int>(std::floor((bodies[i].pos.x + e) * invCell));
            const int minY = static_cast<int>(std::floor((bodies[i].pos.y - e) * invCell));
            const int maxY = static_cast<int>(std::floor((bodies[i].pos.y + e) * invCell));
            const long span = static_cast<long>(maxX - minX + 1) * static_cast<long>(maxY - minY + 1);
            if (span > 256) {
                large.push_back(i);
                continue;
            }
            for (int cx = minX; cx <= maxX; ++cx) {
                for (int cy = minY; cy <= maxY; ++cy) {
                    grid[cellKey(cx, cy)].push_back(i);
                }
            }
        }
        std::unordered_set<uint64_t> seen;
        auto add = [&](int a, int b) {
            if (a > b) {
                std::swap(a, b);
            }
            const uint64_t k =
                (static_cast<uint64_t>(static_cast<uint32_t>(a)) << 32) | static_cast<uint32_t>(b);
            if (seen.insert(k).second) {
                pairs.emplace_back(a, b);
            }
        };
        for (const auto& kv : grid) {
            const std::vector<int>& v = kv.second;
            for (size_t i = 0; i < v.size(); ++i) {
                for (size_t j = i + 1; j < v.size(); ++j) {
                    add(v[i], v[j]);
                }
            }
        }
        for (int li : large) {
            for (int k = 0; k < n; ++k) {
                if (k != li) {
                    add(li, k);
                }
            }
        }
        std::sort(pairs.begin(), pairs.end());
        return pairs;
    }

    // Decide which bodies are asleep this step. Dynamic bodies connected by a contact (from last
    // frame) or a joint form an island via union-find; an island sleeps only when its quietest member
    // has been quiet for `sleepTime`, so a resting stack sleeps as a unit and any disturbed member
    // keeps the whole island awake. A body that has just fallen asleep has its velocity zeroed.
    void updateSleepStates() {
        const int n = static_cast<int>(bodies.size());
        std::vector<int> parent(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            parent[static_cast<size_t>(i)] = i;
        }
        auto find = [&](int x) {
            while (parent[static_cast<size_t>(x)] != x) {
                parent[static_cast<size_t>(x)] =
                    parent[static_cast<size_t>(parent[static_cast<size_t>(x)])];
                x = parent[static_cast<size_t>(x)];
            }
            return x;
        };
        auto unite = [&](int a, int b) {
            const int ra = find(a), rb = find(b);
            if (ra != rb) {
                parent[static_cast<size_t>(ra)] = rb;
            }
        };
        auto dynamic = [&](int i) { return bodies[static_cast<size_t>(i)].invMass > 0.0f; };
        for (const detail::ContactConstraint& c : m_prev) {
            if (dynamic(c.a) && dynamic(c.b)) {
                unite(c.a, c.b);
            }
        }
        for (const Joint2D& jt : joints) {
            if (jt.a >= 0 && jt.a < n && jt.b >= 0 && jt.b < n && dynamic(jt.a) && dynamic(jt.b)) {
                unite(jt.a, jt.b);
            }
        }
        std::unordered_map<int, float> islandMin;
        for (int i = 0; i < n; ++i) {
            if (!dynamic(i)) {
                continue;
            }
            const int r = find(i);
            const float t = bodies[static_cast<size_t>(i)].sleepTimer;
            auto it = islandMin.find(r);
            if (it == islandMin.end() || t < it->second) {
                islandMin[r] = t;
            }
        }
        for (int i = 0; i < n; ++i) {
            if (!dynamic(i)) {
                continue;
            }
            Body2D& b = bodies[static_cast<size_t>(i)];
            const bool asleep = islandMin[find(i)] >= sleepTime;
            if (asleep && !b.sleeping) {
                b.vel = math::vec2(0.0f, 0.0f);
                b.angularVel = 0.0f;
            }
            b.sleeping = asleep;
        }
    }

    // After integration, grow each awake body's quiet timer while it stays below the sleep thresholds,
    // and reset it the moment it moves. Sleeping bodies keep their (already-elapsed) timer.
    void updateSleepTimers(float dt) {
        const float linT2 = sleepLinearThreshold * sleepLinearThreshold;
        for (Body2D& b : bodies) {
            if (b.invMass <= 0.0f || b.sleeping) {
                continue;
            }
            const bool quiet = glm::dot(b.vel, b.vel) <= linT2 &&
                               std::fabs(b.angularVel) <= sleepAngularThreshold;
            b.sleepTimer = quiet ? b.sleepTimer + dt : 0.0f;
        }
    }

    // Warm-started sequential-impulse step (opt-in via warmStarting). Integrate velocity, build fresh
    // two-point contact constraints, carry last frame's accumulated impulses into them (warm start),
    // run `iterations` velocity passes over contacts + joints, integrate position, then keep the
    // constraints for next frame. This is the modern Box2D/Godot solve; accumulation + warm starting
    // are what make a tall stack stay rigid at a handful of iterations.
    void stepSolver(float dt, int iterations) {
        const float invDt = dt > 0.0f ? 1.0f / dt : 0.0f;
        // Sleeping: a sleeping body is made temporarily immovable (invMass/invInertia 0) so it is
        // skipped by integration and acts as static in the solve; its real mass is restored after the
        // step. This reuses the entire solver unchanged. Disabled bodies are decided from last frame's
        // islands, so a body newly touched by a mover wakes on the following step (no tunneling: the
        // mover still collides with it as a static obstacle in between).
        std::vector<size_t> sleptIdx;
        std::vector<float> sleptM, sleptI;
        if (allowSleep) {
            updateSleepStates();
            for (size_t i = 0; i < bodies.size(); ++i) {
                if (bodies[i].sleeping) {
                    sleptIdx.push_back(i);
                    sleptM.push_back(bodies[i].invMass);
                    sleptI.push_back(bodies[i].invInertia);
                    bodies[i].invMass = 0.0f;
                    bodies[i].invInertia = 0.0f;
                }
            }
        }
        for (Body2D& b : bodies) {
            if (b.invMass > 0.0f) {
                b.vel += gravity * dt;
                b.vel *= 1.0f / (1.0f + b.linearDamping * dt);
                b.angularVel *= 1.0f / (1.0f + b.angularDamping * dt);
            }
        }
        // Broadphase + narrowphase -> fresh constraints (two-point manifolds for box pairs).
        std::vector<detail::ContactConstraint> contacts;
        for (const std::pair<int, int>& pr : collectPairs()) {
            const size_t i = static_cast<size_t>(pr.first), j = static_cast<size_t>(pr.second);
            // Layer/mask filtering: skip pairs that should never interact (Godot collision_layer/mask).
            if (!game::interact(bodies[i].collisionLayer, bodies[i].collisionMask,
                                bodies[j].collisionLayer, bodies[j].collisionMask)) {
                continue;
            }
            detail::Contact2 m = detail::manifold2(bodies[i], bodies[j]);
            if (m.hit) {
                contacts.push_back(detail::buildConstraint(pr.first, bodies[i], pr.second, bodies[j], m,
                                                           restitutionThreshold));
            }
        }
        // Warm start: inherit accumulated impulses from the matching pair last frame.
        if (!m_prev.empty()) {
            std::unordered_map<uint64_t, size_t> index;
            index.reserve(m_prev.size() * 2);
            for (size_t p = 0; p < m_prev.size(); ++p) {
                index[m_prev[p].key] = p;
            }
            for (detail::ContactConstraint& c : contacts) {
                auto it = index.find(c.key);
                if (it != index.end()) {
                    const detail::ContactConstraint& old = m_prev[it->second];
                    for (int k = 0; k < c.count && k < old.count; ++k) {
                        c.jN[k] = old.jN[k];
                        c.jT[k] = old.jT[k];
                    }
                }
            }
        }
        for (detail::ContactConstraint& c : contacts) {
            detail::warmStart(bodies[static_cast<size_t>(c.a)], bodies[static_cast<size_t>(c.b)], c);
        }
        // Velocity iterations: contacts (restitution only), then joints.
        for (int it = 0; it < iterations; ++it) {
            for (detail::ContactConstraint& c : contacts) {
                detail::solveVelocity(bodies[static_cast<size_t>(c.a)],
                                      bodies[static_cast<size_t>(c.b)], c);
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
        }
        // Position (split-impulse) iterations: push out of penetration via pseudo-velocities that add
        // no bounce energy. pv/pw start at zero and are folded into the position integration below.
        std::vector<math::vec2> pv(bodies.size(), math::vec2(0.0f, 0.0f));
        std::vector<float> pw(bodies.size(), 0.0f);
        for (int it = 0; it < iterations; ++it) {
            for (detail::ContactConstraint& c : contacts) {
                const size_t ia = static_cast<size_t>(c.a), ib = static_cast<size_t>(c.b);
                detail::solveBias(bodies[ia], pv[ia], pw[ia], bodies[ib], pv[ib], pw[ib], c, baumgarte,
                                  slop, invDt);
            }
        }
        for (size_t i = 0; i < bodies.size(); ++i) {
            bodies[i].pos += (bodies[i].vel + pv[i]) * dt;
            bodies[i].angle += (bodies[i].angularVel + pw[i]) * dt;
        }
        if (hasBounds) {
            for (Body2D& b : bodies) {
                collideBounds(b, bounds);
            }
        }
        m_prev = std::move(contacts);
        if (allowSleep) {
            for (size_t k = 0; k < sleptIdx.size(); ++k) {
                bodies[sleptIdx[k]].invMass = sleptM[k];
                bodies[sleptIdx[k]].invInertia = sleptI[k];
            }
            updateSleepTimers(dt);
        }
    }

    std::vector<detail::ContactConstraint> m_prev; // last frame's constraints, for warm starting

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
                    if (solveManifolds) {
                        detail::Contact2 m = detail::manifold2(bodies[i], bodies[j]);
                        if (m.hit) {
                            detail::resolveManifold(bodies[i], bodies[j], m);
                        }
                    } else {
                        detail::Manifold m = detail::manifold(bodies[i], bodies[j]);
                        if (m.hit) {
                            detail::resolveRot(bodies[i], bodies[j], m);
                        }
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
