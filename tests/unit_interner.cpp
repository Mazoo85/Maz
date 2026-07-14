// Unit tests for maz::core::StringInterner — the original-string storage
// companion to the hash-only StringId. Exercises intern+resolve roundtrip,
// same-string de-duplication, that the interned id equals the plain StringId
// hash (fromBytes and the _sid UDL), resolve/contains for unknown ids, both
// contains overloads (id and string), multiple distinct strings, the empty
// string edge case, and clear(). Pure C++, no GPU/display.

#include "maz/core/StringInterner.hpp"

#include <cstdio>
#include <string>
#include <string_view>

using namespace maz::core;

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
    StringInterner si;

    // --- intern + resolve roundtrip ------------------------------------------
    StringId id = si.intern("hello");
    check(id.valid(), "intern returns a valid id");
    check(si.resolve(id) == "hello", "resolve recovers the interned string");
    check(si.size() == 1, "size is 1 after one intern");

    // --- intern same string dedupes ------------------------------------------
    {
        StringId a = si.intern("world");
        StringId b = si.intern("world");
        check(a == b, "interning the same string returns the same id");
        check(si.size() == 2, "size is 2 (hello + world), dedup stores once");
        check(si.resolve(a) == "world", "resolve recovers the deduped string");
    }

    // --- id matches StringId hash --------------------------------------------
    {
        check(si.intern("hello") == StringId::fromBytes("hello", 5),
              "interned id == plain StringId::fromBytes (interning doesn't change the id)");
        check(si.intern("hello") == "hello"_sid, "interned id == _sid UDL");
    }

    // --- resolve unknown id -> empty -----------------------------------------
    {
        StringId unknown = StringId::fromBytes("never-interned", 14);
        check(si.resolve(unknown).empty(), "resolve of an un-interned id is an empty view");
        check(!si.contains(unknown), "contains(id) false for an un-interned id");
    }

    // --- contains (id and string overloads) ----------------------------------
    {
        check(si.contains(id), "contains(id) true for interned hello");
        check(si.contains(std::string_view("hello")), "contains(string) true for interned hello");
        check(!si.contains(std::string_view("nope")), "contains(string) false for un-interned string");
    }

    // --- multiple strings distinct -------------------------------------------
    {
        StringInterner mi;
        StringId ia = mi.intern("a");
        StringId ib = mi.intern("b");
        StringId ic = mi.intern("c");
        StringId il = mi.intern("longer string here");
        check(mi.size() == 4, "size reflects four distinct strings");
        check(mi.resolve(ia) == "a", "resolve a");
        check(mi.resolve(ib) == "b", "resolve b");
        check(mi.resolve(ic) == "c", "resolve c");
        check(mi.resolve(il) == "longer string here", "resolve longer string here");
        check(ia != ib && ib != ic && ia != ic && il != ia, "ids are distinct");
    }

    // --- empty string --------------------------------------------------------
    {
        StringId e = si.intern("");
        check(e.valid(), "empty-string id is valid (kFnvOffset != 0)");
        check(si.resolve(e).empty(), "resolve of the empty-string id is an empty view");
        check(si.contains(e), "contains(id) true for the interned empty string");
    }

    // --- clear / empty -------------------------------------------------------
    {
        si.clear();
        check(si.empty(), "empty() after clear");
        check(si.size() == 0, "size 0 after clear");
        check(si.resolve(id).empty(), "cleared id no longer resolves");
        check(!si.contains(id), "cleared id no longer contained");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
