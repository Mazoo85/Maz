// Unit tests for maz::ecs::propagateTransforms — the ECS transform-propagation
// system (LocalTransform + Parent -> WorldTransform, up the parent chain with
// per-pass memoization). Exercises root passthrough, parent->child and multi-level
// chains, iteration-order independence (child stored before parent), idempotency,
// transform-less/dead parents treated as roots, the parentWorld*local compose order,
// end-to-end via SystemScheduler, and cycle termination. Pure C++, no GPU/display.

#include "maz/ecs/TransformSystem.hpp"
#include "maz/ecs/SystemScheduler.hpp"

#include <cstdio>

using namespace maz::ecs;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

} // namespace

int main() {
    // Read a world origin (assert the component exists first).
    auto worldOrigin = [](World& w, Entity e){ return w.get<WorldTransform>(e)->value.origin; };

    // --- ROOT ----------------------------------------------------------------
    {
        World w;
        Entity e = w.create();
        w.add<LocalTransform>(e, LocalTransform{ maz::math::Transform{ maz::math::mat3(1.0f), maz::math::vec3(5, 0, 0) } });
        propagateTransforms(w);
        check(w.has<WorldTransform>(e), "root got a WorldTransform");
        maz::math::vec3 o = worldOrigin(w, e);
        check(o.x == 5.0f && o.y == 0.0f && o.z == 0.0f, "root world == own local (5,0,0)");
    }

    // --- PARENT -> CHILD -----------------------------------------------------
    {
        World w;
        Entity parent = w.create();
        w.add<LocalTransform>(parent, LocalTransform{ maz::math::Transform{ maz::math::mat3(1.0f), maz::math::vec3(10, 0, 0) } });
        Entity child = w.create();
        w.add<LocalTransform>(child, LocalTransform{ maz::math::Transform{ maz::math::mat3(1.0f), maz::math::vec3(1, 0, 0) } });
        setParent(w, child, parent);
        propagateTransforms(w);
        maz::math::vec3 co = worldOrigin(w, child);
        check(co.x == 11.0f && co.y == 0.0f && co.z == 0.0f, "child world == parent+local (11,0,0)");
        maz::math::vec3 po = worldOrigin(w, parent);
        check(po.x == 10.0f && po.y == 0.0f && po.z == 0.0f, "parent world == own local (10,0,0)");
    }

    // --- THREE-LEVEL CHAIN ---------------------------------------------------
    {
        World w;
        Entity gp = w.create();
        w.add<LocalTransform>(gp, LocalTransform{ maz::math::Transform{ maz::math::mat3(1.0f), maz::math::vec3(100, 0, 0) } });
        Entity p = w.create();
        w.add<LocalTransform>(p, LocalTransform{ maz::math::Transform{ maz::math::mat3(1.0f), maz::math::vec3(10, 0, 0) } });
        Entity c = w.create();
        w.add<LocalTransform>(c, LocalTransform{ maz::math::Transform{ maz::math::mat3(1.0f), maz::math::vec3(1, 0, 0) } });
        setParent(w, p, gp);
        setParent(w, c, p);
        propagateTransforms(w);
        maz::math::vec3 co = worldOrigin(w, c);
        check(co.x == 111.0f && co.y == 0.0f && co.z == 0.0f, "grandchild world == gp+p+c (111,0,0)");
    }

    // --- ORDER INDEPENDENCE --------------------------------------------------
    {
        // Create the CHILD entity first, then the parent, so each<LocalTransform>
        // visits the child before the parent in dense (SparseSet) order. A naive
        // single-pass that computes in iteration order would resolve the child
        // against an un-computed parent and FAIL this; the memoized recursion does not.
        World w;
        Entity child = w.create();
        w.add<LocalTransform>(child, LocalTransform{ maz::math::Transform{ maz::math::mat3(1.0f), maz::math::vec3(1, 0, 0) } });
        Entity parent = w.create();
        w.add<LocalTransform>(parent, LocalTransform{ maz::math::Transform{ maz::math::mat3(1.0f), maz::math::vec3(10, 0, 0) } });
        setParent(w, child, parent);
        propagateTransforms(w);
        maz::math::vec3 co = worldOrigin(w, child);
        check(co.x == 11.0f && co.y == 0.0f && co.z == 0.0f, "child-before-parent order still yields (11,0,0)");
    }

    // --- IDEMPOTENT ----------------------------------------------------------
    {
        World w;
        Entity parent = w.create();
        w.add<LocalTransform>(parent, LocalTransform{ maz::math::Transform{ maz::math::mat3(1.0f), maz::math::vec3(10, 0, 0) } });
        Entity child = w.create();
        w.add<LocalTransform>(child, LocalTransform{ maz::math::Transform{ maz::math::mat3(1.0f), maz::math::vec3(1, 0, 0) } });
        setParent(w, child, parent);
        propagateTransforms(w);
        propagateTransforms(w);  // twice
        maz::math::vec3 co = worldOrigin(w, child);
        check(co.x == 11.0f && co.y == 0.0f && co.z == 0.0f, "idempotent: child still (11,0,0) after two passes");
    }

    // --- TRANSFORM-LESS / DEAD PARENT = ROOT ---------------------------------
    {
        World w;
        // (a) Parent entity exists but has NO LocalTransform -> child treated as root.
        Entity noXform = w.create();  // deliberately no LocalTransform
        Entity child = w.create();
        w.add<LocalTransform>(child, LocalTransform{ maz::math::Transform{ maz::math::mat3(1.0f), maz::math::vec3(1, 0, 0) } });
        setParent(w, child, noXform);
        // (b) Parent entity destroyed after setParent -> stale handle, child treated as root.
        Entity deadParent = w.create();
        w.add<LocalTransform>(deadParent, LocalTransform{ maz::math::Transform{ maz::math::mat3(1.0f), maz::math::vec3(10, 0, 0) } });
        Entity child2 = w.create();
        w.add<LocalTransform>(child2, LocalTransform{ maz::math::Transform{ maz::math::mat3(1.0f), maz::math::vec3(2, 0, 0) } });
        setParent(w, child2, deadParent);
        w.destroy(deadParent);
        propagateTransforms(w);
        maz::math::vec3 co = worldOrigin(w, child);
        check(co.x == 1.0f && co.y == 0.0f && co.z == 0.0f, "transform-less parent -> child world == own local (1,0,0)");
        maz::math::vec3 c2o = worldOrigin(w, child2);
        check(c2o.x == 2.0f && c2o.y == 0.0f && c2o.z == 0.0f, "dead parent -> child world == own local (2,0,0)");
    }

    // --- COMPOSE ORDER -------------------------------------------------------
    {
        // Parent basis = uniform scale 2, origin 0. Child local origin (1,0,0).
        // parentWorld * local => origin = parent.basis * child.origin = 2 * 1 = 2.0.
        // A swapped local * parentWorld would give 1.0 -> this pins the compose order.
        World w;
        Entity parent = w.create();
        w.add<LocalTransform>(parent, LocalTransform{ maz::math::Transform{ maz::math::mat3(2.0f), maz::math::vec3(0, 0, 0) } });
        Entity child = w.create();
        w.add<LocalTransform>(child, LocalTransform{ maz::math::Transform{ maz::math::mat3(1.0f), maz::math::vec3(1, 0, 0) } });
        setParent(w, child, parent);
        propagateTransforms(w);
        maz::math::vec3 co = worldOrigin(w, child);
        check(co.x == 2.0f, "compose order: child world origin.x == parent.basis * child.origin == 2.0");
    }

    // --- END-TO-END VIA SYSTEMSCHEDULER --------------------------------------
    {
        World w;
        SystemScheduler s;
        s.add("transform_propagation", propagateTransforms);
        Entity parent = w.create();
        w.add<LocalTransform>(parent, LocalTransform{ maz::math::Transform{ maz::math::mat3(1.0f), maz::math::vec3(10, 0, 0) } });
        Entity child = w.create();
        w.add<LocalTransform>(child, LocalTransform{ maz::math::Transform{ maz::math::mat3(1.0f), maz::math::vec3(1, 0, 0) } });
        setParent(w, child, parent);
        s.run(w);
        maz::math::vec3 co = worldOrigin(w, child);
        check(co.x == 11.0f && co.y == 0.0f && co.z == 0.0f, "scheduler-driven pass yields child (11,0,0)");
    }

    // --- CYCLE TERMINATES ----------------------------------------------------
    {
        // a's parent is b and b's parent is a (a constructed cycle). The pass must
        // COMPLETE (no hang); values are defined-but-arbitrary, so we assert only
        // that both entities received a WorldTransform.
        World w;
        Entity a = w.create();
        w.add<LocalTransform>(a, LocalTransform{ maz::math::Transform{ maz::math::mat3(1.0f), maz::math::vec3(1, 0, 0) } });
        Entity b = w.create();
        w.add<LocalTransform>(b, LocalTransform{ maz::math::Transform{ maz::math::mat3(1.0f), maz::math::vec3(2, 0, 0) } });
        setParent(w, a, b);
        setParent(w, b, a);
        propagateTransforms(w);
        check(w.has<WorldTransform>(a) && w.has<WorldTransform>(b), "cycle terminates: both got a WorldTransform");
    }

    // --- SELF-PARENT CYCLE ---------------------------------------------------
    {
        // Self-parent cycle: setParent(a, a). The inProgress guard checks the parent
        // is on the stack BEFORE inserting the current node, so a self-parent terminates
        // (treated as a root on the recursive step) instead of infinite-recursing.
        World w;
        Entity a = w.create();
        w.add<LocalTransform>(a, LocalTransform{ maz::math::Transform{ maz::math::mat3(1.0f), maz::math::vec3(3, 0, 0) } });
        setParent(w, a, a);          // a is its own parent
        propagateTransforms(w);      // must COMPLETE (no hang / stack overflow)
        check(w.has<WorldTransform>(a), "self-parent: entity still gets a WorldTransform (pass terminated)");
        // value is defined-but-arbitrary under a broken cycle; do not assert exact origin.
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
