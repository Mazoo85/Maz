// Unit tests for maz::math::Transform2D (2D affine basis+origin transform). Pure
// math, no GPU required. Exercises identity/translation/rotation(CCW)/scale
// factories, xform vs xformBasis, compose order + non-commutativity, and inverse
// round-trips. The 90-degree rotation cases give clean 0/+-1/+-2 targets within
// tolerance; comparisons use an approx tolerance since rotations involve cos/sin.

#include "maz/math/Transform2D.hpp"

#include <cstdio>
#include <cmath>

using namespace maz::math;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

bool approx(float a, float b) { return std::fabs(a - b) < 1e-5f; }

bool vapprox2(vec2 a, vec2 b) {
    return approx(a.x, b.x) && approx(a.y, b.y);
}

} // namespace

int main() {
    const float HALF_PI = 1.57079632679f;

    // --- 1. IDENTITY ---------------------------------------------------------
    {
        Transform2D t;
        check(vapprox2(t.xform(vec2(3.0f, 5.0f)), vec2(3.0f, 5.0f)), "identity xform leaves point unchanged");
        check(vapprox2(t.xformBasis(vec2(3.0f, 5.0f)), vec2(3.0f, 5.0f)), "identity xformBasis leaves vector unchanged");
    }

    // --- 2. TRANSLATION ------------------------------------------------------
    {
        auto t = Transform2D::translation(vec2(2.0f, 3.0f));
        check(vapprox2(t.xform(vec2(1.0f, 1.0f)), vec2(3.0f, 4.0f)), "translation xform adds origin");
        check(vapprox2(t.xformBasis(vec2(1.0f, 1.0f)), vec2(1.0f, 1.0f)), "translation xformBasis ignores translation");
    }

    // --- 3. ROTATION 90 CCW (+X->+Y, +Y->-X) ---------------------------------
    {
        auto r = Transform2D::rotation(HALF_PI);
        check(vapprox2(r.xform(vec2(1.0f, 0.0f)), vec2(0.0f, 1.0f)), "rotation 90 CCW: +X -> +Y");
        check(vapprox2(r.xform(vec2(0.0f, 1.0f)), vec2(-1.0f, 0.0f)), "rotation 90 CCW: +Y -> -X");
        check(vapprox2(r.xform(vec2(1.0f, 1.0f)), vec2(-1.0f, 1.0f)), "rotation 90 CCW: (1,1) -> (-1,1)");
    }

    // --- 4. SCALE ------------------------------------------------------------
    {
        auto s = Transform2D::scale(vec2(2.0f, 3.0f));
        check(vapprox2(s.xform(vec2(1.0f, 1.0f)), vec2(2.0f, 3.0f)), "scale (2,3) on (1,1)");
        check(vapprox2(s.xform(vec2(-1.0f, 2.0f)), vec2(-2.0f, 6.0f)), "scale (2,3) on (-1,2)");
    }

    // --- 5. COMPOSE ORDER (a*b).xform(p) == a.xform(b.xform(p)) + non-commutative ---
    {
        auto a = Transform2D::translation(vec2(1.0f, 0.0f));
        auto b = Transform2D::rotation(HALF_PI);
        check(vapprox2((a * b).xform(vec2(1.0f, 0.0f)), a.xform(b.xform(vec2(1.0f, 0.0f)))),
              "compose equals sequential xform");
        check(vapprox2((a * b).xform(vec2(1.0f, 0.0f)), vec2(1.0f, 1.0f)),
              "a*b: translate((1,0)) after rotate((1,0)=(0,1)) -> (1,1)");
        // Non-commutativity: (b*a).xform((1,0)) = rotate(translate((1,0))=(2,0)) = (0,2) != (1,1)
        check(vapprox2((b * a).xform(vec2(1.0f, 0.0f)), vec2(0.0f, 2.0f)),
              "b*a: rotate after translate -> (0,2)");
        check(!vapprox2((a * b).xform(vec2(1.0f, 0.0f)), (b * a).xform(vec2(1.0f, 0.0f))),
              "compose is non-commutative: a*b != b*a");
    }

    // --- 6. INVERSE ROUND-TRIP -----------------------------------------------
    {
        Transform2D t = Transform2D::translation(vec2(5.0f, -2.0f))
                        * Transform2D::rotation(HALF_PI)
                        * Transform2D::scale(vec2(2.0f, 2.0f));
        const vec2 pts[] = {vec2(1.0f, 0.0f), vec2(0.0f, 1.0f), vec2(3.0f, -4.0f), vec2(-2.0f, 5.0f)};
        for (vec2 p : pts) {
            check(vapprox2(t.inverse().xform(t.xform(p)), p), "inverse undoes xform");
            check(vapprox2(t.xform(t.inverse().xform(p)), p), "xform undoes inverse");
        }
    }

    // --- 7. IDENTITY ELEMENT (t*id == id*t == t) -----------------------------
    {
        Transform2D id;
        auto t = Transform2D::translation(vec2(3.0f, 4.0f));
        check(vapprox2((t * id).xform(vec2(1.0f, 1.0f)), t.xform(vec2(1.0f, 1.0f))), "t * identity == t");
        check(vapprox2((id * t).xform(vec2(1.0f, 1.0f)), t.xform(vec2(1.0f, 1.0f))), "identity * t == t");
    }

    // --- 8. SCALE THEN ROTATE COMPOSE ----------------------------------------
    {
        auto rs = Transform2D::rotation(HALF_PI) * Transform2D::scale(vec2(2.0f, 2.0f));
        // rotate(scale((1,0))=(2,0)) = (0,2)
        check(vapprox2(rs.xform(vec2(1.0f, 0.0f)), vec2(0.0f, 2.0f)), "rotation*scale: (1,0) -> (0,2)");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
