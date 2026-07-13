// Unit tests for maz::scene::SceneGraph (node transform hierarchy with lazy,
// dirty-flag world-transform propagation). Pure math, no GPU required. Exercises
// parent*local compose order, dirty propagation on setLocal, multi-level chains,
// reparenting, world caching stability, and rotation compose order.

#include "maz/scene/SceneGraph.hpp"

#include <cstdio>
#include <cmath>

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

math::Transform translate(float x, float y, float z) {
    return math::Transform{ math::mat3(1.0f), math::vec3(x, y, z) };
}

} // namespace

int main() {
    // --- root identity -------------------------------------------------------
    {
        scene::SceneGraph sg;
        check(approxVec(sg.getWorld(sg.root()).xform(math::vec3(1.0f, 2.0f, 3.0f)), math::vec3(1.0f, 2.0f, 3.0f)),
              "root world is identity");

        // --- child under translated parent ----------------------------------
        auto parent = sg.createNode();
        sg.setLocal(parent, translate(10.0f, 0.0f, 0.0f));
        auto child = sg.createNode(parent);
        sg.setLocal(child, translate(0.0f, 5.0f, 0.0f));
        check(approxVec(sg.getWorld(child).xform(math::vec3(0.0f, 0.0f, 0.0f)), math::vec3(10.0f, 5.0f, 0.0f)),
              "child world = parent*local (point)");
        check(approxVec(sg.getWorld(child).origin, math::vec3(10.0f, 5.0f, 0.0f)),
              "child world origin");

        // --- dirty propagation ----------------------------------------------
        sg.setLocal(parent, translate(100.0f, 0.0f, 0.0f));
        check(approxVec(sg.getWorld(child).xform(math::vec3(0.0f, 0.0f, 0.0f)), math::vec3(100.0f, 5.0f, 0.0f)),
              "parent move propagates to child");
    }

    // --- multi-level chain ---------------------------------------------------
    {
        scene::SceneGraph sg2;
        auto a = sg2.createNode();
        sg2.setLocal(a, translate(1.0f, 0.0f, 0.0f));
        auto b = sg2.createNode(a);
        sg2.setLocal(b, translate(0.0f, 2.0f, 0.0f));
        auto c = sg2.createNode(b);
        sg2.setLocal(c, translate(0.0f, 0.0f, 3.0f));
        check(approxVec(sg2.getWorld(c).origin, math::vec3(1.0f, 2.0f, 3.0f)),
              "3-level chain composes");
    }

    // --- reparent ------------------------------------------------------------
    {
        scene::SceneGraph sg3;
        auto pa = sg3.createNode();
        sg3.setLocal(pa, translate(10.0f, 0.0f, 0.0f));
        auto pb = sg3.createNode();
        sg3.setLocal(pb, translate(-3.0f, 0.0f, 0.0f));
        auto n = sg3.createNode(pa);
        sg3.setLocal(n, translate(1.0f, 0.0f, 0.0f));
        check(approxVec(sg3.getWorld(n).origin, math::vec3(11.0f, 0.0f, 0.0f)), "under pa");
        sg3.setParent(n, pb);
        check(approxVec(sg3.getWorld(n).origin, math::vec3(-2.0f, 0.0f, 0.0f)), "reparented under pb");
    }

    // --- caching stability ---------------------------------------------------
    {
        scene::SceneGraph sg4;
        auto p = sg4.createNode();
        sg4.setLocal(p, translate(4.0f, 0.0f, 0.0f));
        auto ch = sg4.createNode(p);
        sg4.setLocal(ch, translate(0.0f, 1.0f, 0.0f));
        auto w1 = sg4.getWorld(ch).origin;
        auto w2 = sg4.getWorld(ch).origin;
        check(approxVec(w1, w2) && approxVec(w1, math::vec3(4.0f, 1.0f, 0.0f)), "getWorld cached stable");
        sg4.setLocal(ch, translate(0.0f, 7.0f, 0.0f));
        check(approxVec(sg4.getWorld(ch).origin, math::vec3(4.0f, 7.0f, 0.0f)), "recompute after setLocal");
    }

    // --- rotation compose (strongest order gate) -----------------------------
    {
        scene::SceneGraph sg5;
        auto rp = sg5.createNode();
        sg5.setLocal(rp, math::Transform::fromTRS(math::vec3(0.0f, 0.0f, 0.0f),
                                                  glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
                                                  math::vec3(1.0f, 1.0f, 1.0f)));
        auto rc = sg5.createNode(rp);
        sg5.setLocal(rc, translate(1.0f, 0.0f, 0.0f));
        // parent is under root so parent world == parent local; a 90deg Z-rotation maps (1,0,0)->(0,1,0).
        check(approxVec(sg5.getWorld(rc).xform(math::vec3(0.0f, 0.0f, 0.0f)), math::vec3(0.0f, 1.0f, 0.0f), 1e-4f),
              "rotation compose order (parent*local)");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
