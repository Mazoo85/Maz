// Unit tests for maz::core::Trie — a byte-oriented prefix tree. Exercises insert +
// idempotency, the contains (exact-word membership) vs startsWith (any-word-with-prefix)
// distinction, collect(prefix) returning ALL matching words SORTED lexicographically
// (incl. collect("") == every word), the prefix-is-also-a-word case, empty/clear
// bookkeeping, the empty-string-word edge, and branching over shared prefixes. Pure C++,
// no GPU/display.

#include "maz/core/Trie.hpp"

#include <cstdio>
#include <string>
#include <vector>

using namespace maz::core;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

// Exact ordered compare — collect is sorted so the order is deterministic.
bool vecEq(const std::vector<std::string>& a, std::initializer_list<const char*> b) {
    if (a.size() != b.size()) {
        return false;
    }
    std::size_t i = 0;
    for (const char* s : b) {
        if (a[i] != s) {
            return false;
        }
        ++i;
    }
    return true;
}

} // namespace

int main() {
    // --- insert + contains vs startsWith -------------------------------------
    {
        Trie t;
        t.insert("cat");
        t.insert("car");
        t.insert("card");
        t.insert("dog");
        check(t.size() == 4, "size==4 after 4 inserts");

        check(t.contains("cat") == true, "contains(cat)");
        check(t.contains("car") == true, "contains(car)");
        check(t.contains("card") == true, "contains(card)");
        check(t.contains("dog") == true, "contains(dog)");
        check(t.contains("ca") == false, "contains(ca) prefix not a word");
        check(t.contains("care") == false, "contains(care) not inserted");
        check(t.contains("do") == false, "contains(do) prefix not a word");

        check(t.startsWith("ca") == true, "startsWith(ca)");
        check(t.startsWith("car") == true, "startsWith(car)");
        check(t.startsWith("do") == true, "startsWith(do)");
        check(t.startsWith("xyz") == false, "startsWith(xyz) no path");
        check(t.startsWith("care") == false, "startsWith(care) path incomplete");
        check(t.startsWith("") == true, "startsWith('') empty prefix");
    }

    // --- collect prefix, sorted ----------------------------------------------
    {
        Trie t;
        t.insert("cat");
        t.insert("car");
        t.insert("card");
        t.insert("dog");
        check(vecEq(t.collect("car"), {"car", "card"}), "collect(car) == {car,card}");
        check(vecEq(t.collect("ca"), {"car", "card", "cat"}), "collect(ca) sorted {car,card,cat}");
        check(vecEq(t.collect("do"), {"dog"}), "collect(do) == {dog}");
        check(vecEq(t.collect(""), {"car", "card", "cat", "dog"}), "collect('') == ALL sorted");
        check(vecEq(t.collect("z"), {}), "collect(z) no match -> {}");
        check(vecEq(t.collect("care"), {}), "collect(care) prefix path incomplete -> {}");
    }

    // --- idempotent insert ---------------------------------------------------
    {
        Trie t;
        t.insert("cat");
        t.insert("car");
        t.insert("card");
        t.insert("dog");
        t.insert("cat");  // re-insert
        check(t.size() == 4, "size stays 4 after re-insert");
        check(t.contains("cat") == true, "contains(cat) still true");
    }

    // --- empty / clear -------------------------------------------------------
    {
        Trie t;
        check(t.empty() == true, "fresh trie empty");
        check(t.size() == 0, "fresh trie size==0");
        check(t.contains("x") == false, "fresh contains(x) false");
        check(t.startsWith("x") == false, "fresh startsWith(x) false");
        check(t.startsWith("") == true, "fresh startsWith('') true");
        check(vecEq(t.collect(""), {}), "fresh collect('') == {}");

        t.insert("cat");
        t.insert("car");
        t.clear();
        check(t.empty() == true, "empty after clear");
        check(t.size() == 0, "size==0 after clear");
        check(t.contains("cat") == false, "contains(cat) false after clear");
        check(vecEq(t.collect(""), {}), "collect('') == {} after clear");
    }

    // --- prefix that is also a word ------------------------------------------
    {
        Trie t;
        t.insert("car");
        t.insert("card");
        check(t.contains("car") == true, "contains(car) prefix-word");
        check(t.startsWith("car") == true, "startsWith(car) prefix-word");
        // The prefix node is itself a word, so it appears in its own collection.
        check(vecEq(t.collect("car"), {"car", "card"}), "collect(car) includes car itself");
    }

    // --- empty-string word edge ----------------------------------------------
    {
        Trie t2;
        t2.insert("");
        check(t2.size() == 1, "size==1 after insert('')");
        check(t2.contains("") == true, "contains('')");
        check(t2.startsWith("") == true, "startsWith('')");
        check(vecEq(t2.collect(""), {""}), "collect('') == {''}");

        t2.insert("a");
        check(vecEq(t2.collect(""), {"", "a"}), "collect('') == {'',a} empty sorts first");
        check(t2.size() == 2, "size==2 after insert(a)");
    }

    // --- branching / shared prefixes -----------------------------------------
    {
        Trie t;
        t.insert("test");
        t.insert("testing");
        t.insert("tester");
        t.insert("team");
        check(vecEq(t.collect("test"), {"test", "tester", "testing"}), "collect(test) sorted");
        check(vecEq(t.collect("tea"), {"team"}), "collect(tea) == {team}");
        check(t.contains("test") == true, "contains(test)");
        check(t.contains("tes") == false, "contains(tes) prefix not a word");
        check(t.size() == 4, "size==4 branching");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
