// Unit tests for maz::ecs core components (Components.hpp) — the ECS's built-in
// component types (LocalTransform/WorldTransform/Name/Tag/Parent) and the small
// helpers over World (findByName/setParent/parentOf/childrenOf). Exercises
// add/retrieve, name lookup, transform origin defaults, parent upsert + reparent,
// children collection, and multi-component coexistence. Pure C++, no GPU/display.

#include "maz/ecs/Components.hpp"

#include <cstdio>
#include <vector>

using namespace maz::ecs;
using maz::core::StringId;
using maz::core::operator""_sid;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

bool contains(const std::vector<Entity>& v, Entity e) {
    for (const Entity& x : v) { if (x == e) { return true; } }
    return false;
}

} // namespace

int main() {
    // --- NAME / TAG ADD + RETRIEVE -------------------------------------------
    {
        World w;
        Entity e = w.create();
        w.add<Name>(e, Name{"player"_sid});
        w.add<Tag>(e, Tag{"enemy"_sid});
        check(w.has<Name>(e), "Name added");
        check(w.get<Name>(e)->id == "player"_sid, "Name id retrieved");
        check(w.has<Tag>(e), "Tag added");
        check(w.get<Tag>(e)->id == "enemy"_sid, "Tag id retrieved");
        check(w.has<Name>(e) && w.has<Tag>(e), "Name and Tag coexist (distinct types)");
    }

    // --- findByName ----------------------------------------------------------
    {
        World w;
        Entity eA = w.create(); w.add<Name>(eA, Name{"a"_sid});
        Entity eB = w.create(); w.add<Name>(eB, Name{"b"_sid});
        Entity eC = w.create(); w.add<Name>(eC, Name{"c"_sid});
        check(findByName(w, "b"_sid) == eB, "findByName returns the b-entity");
        check(findByName(w, "missing"_sid).isNull(), "findByName absent name is null");
    }
    {
        World w;
        check(findByName(w, "anything"_sid).isNull(), "findByName on empty world is null");
    }

    // --- LocalTransform / WorldTransform -------------------------------------
    {
        World w;
        Entity e = w.create();
        LocalTransform lt;
        lt.value.origin = maz::math::vec3(1.0f, 2.0f, 3.0f);
        w.add<LocalTransform>(e, lt);
        LocalTransform* got = w.get<LocalTransform>(e);
        check(got->value.origin.x == 1.0f && got->value.origin.y == 2.0f && got->value.origin.z == 3.0f,
              "LocalTransform origin retrieved (1,2,3)");
        Entity e2 = w.create();
        check(w.add<WorldTransform>(e2, WorldTransform{}).value.origin.x == 0.0f,
              "WorldTransform defaults to identity origin (0)");
    }

    // --- setParent / parentOf ------------------------------------------------
    {
        World w;
        Entity P = w.create();
        Entity P2 = w.create();
        Entity C = w.create();
        check(parentOf(w, C).isNull(), "parentOf null before setParent");
        setParent(w, C, P);
        check(w.has<Parent>(C), "Parent component added by setParent");
        check(parentOf(w, C) == P, "parentOf returns P after setParent");
        setParent(w, C, P2);
        check(parentOf(w, C) == P2, "parentOf returns P2 after reparent");
        check(w.componentCount<Parent>() == 1, "reparent leaves exactly one Parent component");
    }

    // --- childrenOf ----------------------------------------------------------
    {
        World w;
        Entity P = w.create();
        Entity A = w.create();
        Entity B = w.create();
        Entity U = w.create();  // unrelated, no parent
        setParent(w, A, P);
        setParent(w, B, P);
        std::vector<Entity> kids = childrenOf(w, P);
        check(kids.size() == 2, "childrenOf(P) has 2 children");
        check(contains(kids, A) && contains(kids, B), "childrenOf(P) contains A and B (order-independent)");
        check(childrenOf(w, U).empty(), "childrenOf entity with no children is empty");
    }

    // --- REPARENT MOVES CHILD BETWEEN PARENTS --------------------------------
    {
        World w;
        Entity P1 = w.create();
        Entity P2 = w.create();
        Entity A = w.create();
        setParent(w, A, P1);
        setParent(w, A, P2);
        std::vector<Entity> k1 = childrenOf(w, P1);
        std::vector<Entity> k2 = childrenOf(w, P2);
        check(!contains(k1, A) && k1.size() == 0, "reparent removes A from P1");
        check(contains(k2, A), "reparent adds A to P2");
    }

    // --- ALL COMPONENTS COEXIST ----------------------------------------------
    {
        World w;
        Entity parent = w.create();
        Entity e = w.create();
        LocalTransform lt;
        lt.value.origin = maz::math::vec3(4.0f, 5.0f, 6.0f);
        w.add<Name>(e, Name{"hero"_sid});
        w.add<Tag>(e, Tag{"pickup"_sid});
        w.add<LocalTransform>(e, lt);
        setParent(w, e, parent);
        check(w.has<Name>(e) && w.has<Tag>(e) && w.has<LocalTransform>(e) && w.has<Parent>(e),
              "all four component types present on one entity");
        check(w.get<Name>(e)->id == "hero"_sid, "coexist: Name value intact");
        check(w.get<Tag>(e)->id == "pickup"_sid, "coexist: Tag value intact");
        check(w.get<LocalTransform>(e)->value.origin.x == 4.0f
                  && w.get<LocalTransform>(e)->value.origin.y == 5.0f
                  && w.get<LocalTransform>(e)->value.origin.z == 6.0f,
              "coexist: LocalTransform value intact");
        check(parentOf(w, e) == parent, "coexist: Parent value intact");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
