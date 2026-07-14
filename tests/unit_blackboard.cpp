// Unit tests for maz::ai::Blackboard — the typed StringId-keyed shared-memory
// store the AI systems (FSM / behavior tree / steering) read and write. Exercises
// set/get roundtrip across multiple value types, type-checked has<T>, the always-
// safe getOr<T> fallback (absent OR wrong type), last-write-wins overwrite incl.
// changing a key's stored type, erase/clear/empty/size bookkeeping, and a fresh
// empty blackboard. Pure C++, no GPU/display.

#include "maz/ai/Blackboard.hpp"
#include "maz/math/Math.hpp"  // vec3

#include <cstdio>

using namespace maz::ai;
using maz::core::StringId;
using maz::core::operator""_sid;
using maz::math::vec3;

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
    // --- SET/GET ROUNDTRIP, MULTIPLE TYPES -----------------------------------
    {
        Blackboard bb;
        bb.set("hp"_sid, 100);         // int
        bb.set("speed"_sid, 2.5f);     // float
        bb.set("alive"_sid, true);     // bool
        bb.set("name"_sid, "hero"_sid);// StringId
        bb.set("pos"_sid, vec3(1, 2, 3));
        check(bb.get<int>("hp"_sid) == 100, "get<int>(hp) == 100");
        check(bb.get<float>("speed"_sid) == 2.5f, "get<float>(speed) == 2.5");
        check(bb.get<bool>("alive"_sid) == true, "get<bool>(alive) == true");
        check(bb.get<StringId>("name"_sid) == "hero"_sid, "get<StringId>(name) == hero");
        const vec3 p = bb.get<vec3>("pos"_sid);
        check(p.x == 1 && p.y == 2 && p.z == 3, "get<vec3>(pos) == (1,2,3)");
        check(bb.size() == 5, "size() == 5");
    }

    // --- has<T> TYPE-CHECKED -------------------------------------------------
    {
        Blackboard bb;
        bb.set("hp"_sid, 100);
        check(bb.has<int>("hp"_sid) == true, "has<int>(hp) true");
        check(bb.has<float>("hp"_sid) == false, "has<float>(hp) false (present, wrong type)");
        check(bb.has<int>("missing"_sid) == false, "has<int>(missing) false (absent)");
        check(bb.contains("hp"_sid) == true, "contains(hp) true");
        check(bb.contains("missing"_sid) == false, "contains(missing) false");
    }

    // --- getOr FALLBACK ------------------------------------------------------
    {
        Blackboard bb;
        bb.set("hp"_sid, 100);
        bb.set("speed"_sid, 2.5f);
        check(bb.getOr<int>("hp"_sid, -1) == 100, "getOr<int>(hp) == 100 (present, right type)");
        check(bb.getOr<int>("missing"_sid, -1) == -1, "getOr<int>(missing) == -1 (absent -> fallback)");
        check(bb.getOr<int>("speed"_sid, -1) == -1, "getOr<int>(speed) == -1 (wrong type -> fallback)");
        check(bb.getOr<float>("speed"_sid, 0.0f) == 2.5f, "getOr<float>(speed) == 2.5");
    }

    // --- OVERWRITE LAST-WRITE-WINS INCL. TYPE CHANGE -------------------------
    {
        Blackboard bb;
        bb.set("x"_sid, 1);
        check(bb.get<int>("x"_sid) == 1, "get<int>(x) == 1 after first set");
        bb.set("x"_sid, 9);
        check(bb.get<int>("x"_sid) == 9, "get<int>(x) == 9 after overwrite (last write wins)");
        bb.set("x"_sid, 3.14f);  // type change
        check(bb.has<int>("x"_sid) == false, "has<int>(x) false after type change");
        check(bb.has<float>("x"_sid) == true, "has<float>(x) true after type change");
        check(bb.get<float>("x"_sid) == 3.14f, "get<float>(x) == 3.14 after type change");
        check(bb.size() == 1, "size() == 1 (overwrite counts x once)");
    }

    // --- ERASE / CLEAR / EMPTY / SIZE ----------------------------------------
    {
        Blackboard bb;
        bb.set("hp"_sid, 100);
        bb.set("mp"_sid, 50);
        check(bb.erase("hp"_sid) == true, "erase(hp) returns true");
        check(bb.contains("hp"_sid) == false, "contains(hp) false after erase");
        check(bb.size() == 1, "size() == 1 after erase");
        check(bb.erase("missing"_sid) == false, "erase(missing) returns false");
        bb.clear();
        check(bb.empty() == true, "empty() true after clear");
        check(bb.size() == 0, "size() == 0 after clear");
    }

    // --- EMPTY BLACKBOARD ----------------------------------------------------
    {
        Blackboard bb;
        check(bb.empty() == true, "fresh blackboard empty() true");
        check(bb.size() == 0, "fresh blackboard size() == 0");
        check(bb.contains("k"_sid) == false, "fresh blackboard contains(any) false");
        check(bb.getOr<int>("k"_sid, 7) == 7, "fresh blackboard getOr<int>(k, 7) == 7");
    }

    // --- get<T> ON AN ABSENT/MISMATCHED KEY is a MAZ_ASSERT programmer-error
    //     guard (compiled out in release), not exercised here.

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
