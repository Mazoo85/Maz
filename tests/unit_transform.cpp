// Unit tests for maz::math::Transform (affine basis+origin transform). Pure math,
// no GPU required. Exercises fromTRS (non-uniform scale, R*S column ordering),
// compose, inverse, and matrix round-trips.

#include "maz/math/Transform.hpp"

#include <cstdio>

using namespace maz;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

bool approx(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) < eps;
}

bool approxVec(math::vec3 a, math::vec3 b, float eps = 1e-5f) {
    return approx(a.x, b.x, eps) && approx(a.y, b.y, eps) && approx(a.z, b.z, eps);
}

} // namespace

int main() {
    using math::mat3;
    using math::quat;
    using math::Transform;
    using math::vec3;
    using math::vec4;

    // --- fixtures ------------------------------------------------------------
    vec3 s(2.0f, 3.0f, 4.0f);
    quat r = glm::angleAxis(glm::radians(30.0f), math::normalize(vec3(0.0f, 1.0f, 0.0f)));
    vec3 t(5.0f, -2.0f, 7.0f);
    vec3 p(1.0f, 2.0f, 3.0f);

    // --- identity ------------------------------------------------------------
    Transform I0 = Transform::identity();
    check(approxVec(I0.xform(p), p, 1e-4f), "identity xform leaves point unchanged");
    check(approxVec(I0.xformBasis(p), p, 1e-4f), "identity xformBasis leaves vector unchanged");

    // --- fromTRS vs manual (R*S column ordering guard) -----------------------
    Transform T = Transform::fromTRS(t, r, s);
    mat3 R = glm::mat3_cast(r);
    vec3 expected = t + R * (s * p); // s*p is componentwise = S*p
    check(approxVec(T.xform(p), expected, 1e-4f), "fromTRS matches t + R*(S*p)");

    // --- xformBasis has no translation ---------------------------------------
    check(approxVec(T.xformBasis(p), T.xform(p) - t, 1e-4f), "xformBasis omits translation");
    check(approxVec(T.xform(vec3(0.0f)), t, 1e-4f), "xform(0) yields translation");
    check(approxVec(T.xformBasis(vec3(0.0f)), vec3(0.0f), 1e-4f), "xformBasis(0) yields zero");

    // --- compose -------------------------------------------------------------
    Transform U = Transform::fromTRS(
        vec3(1.0f, 1.0f, 1.0f),
        glm::angleAxis(glm::radians(-50.0f), math::normalize(vec3(1.0f, 0.0f, 0.0f))),
        vec3(0.5f, 1.5f, 2.0f));
    check(approxVec((T * U).xform(p), T.xform(U.xform(p)), 1e-4f), "compose equals sequential xform");

    // --- inverse round-trip --------------------------------------------------
    check(approxVec(T.inverse().xform(T.xform(p)), p, 1e-4f), "inverse undoes xform");

    Transform Iprod = T * T.inverse();
    check(approxVec(Iprod.xform(p), p, 1e-4f), "T * T.inverse() is identity");

    // --- matrix consistency / round-trip -------------------------------------
    check(approxVec(vec3(T.toMatrix() * vec4(p, 1.0f)), T.xform(p), 1e-4f),
          "toMatrix matches xform");

    Transform back = Transform::fromMatrix(T.toMatrix());
    check(approxVec(back.xform(p), T.xform(p), 1e-4f), "fromMatrix(toMatrix) round-trips");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
