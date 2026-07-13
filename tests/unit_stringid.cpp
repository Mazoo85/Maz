// Unit tests for maz::core::StringId and the FNV-1a 64-bit hash. Exact-equality
// checks on the canonical hash vectors, plus determinism, distinctness, operator,
// validity, UDL, and length-limited byte-hashing coverage. Pure C++, no GPU.

#include "maz/core/StringId.hpp"

#include <cstdint>
#include <cstdio>

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
    // --- CANONICAL VECTORS (KEY gate) ----------------------------------------
    // Reference FNV-1a 64-bit outputs. If these fail, the hash is wrong.
    check(fnv1a64("") == 0xcbf29ce484222325ULL, "canonical hash empty");
    check(fnv1a64("a") == 0xaf63dc4c8601ec8cULL, "canonical hash a");
    check(fnv1a64("foobar") == 0x85944171f73967e8ULL, "canonical hash foobar");
    check(StringId("foobar").hash() == 0x85944171f73967e8ULL, "ctor hash foobar");

    // --- DETERMINISM ---------------------------------------------------------
    check(StringId("hello") == StringId("hello"), "same string hashes identically");

    // --- DISTINCT ------------------------------------------------------------
    check(StringId("foo") != StringId("bar"), "distinct strings differ (foo/bar)");
    check(StringId("foo") != StringId("foobar"), "distinct strings differ (foo/foobar)");

    // --- OPERATORS -----------------------------------------------------------
    {
        StringId foo("foo");
        StringId bar("bar");
        // Build a consistent a<b ordering from whichever hashes smaller.
        const StringId& lo = (foo < bar) ? foo : bar;
        const StringId& hi = (foo < bar) ? bar : foo;
        check(lo < hi, "operator< yields a strict ordering");
        check(!(hi < lo), "operator< is asymmetric");
        check(lo == lo, "operator== reflexive");
        check(lo != hi, "operator!= for distinct ids");
        check(!(lo != lo), "operator!= false for equal ids");
    }

    // --- VALID ---------------------------------------------------------------
    check(!StringId().valid(), "default-constructed id is invalid");
    check(StringId("x").valid(), "non-empty string id is valid");

    // --- UDL -----------------------------------------------------------------
    check(("foobar"_sid) == StringId("foobar"), "UDL matches ctor");

    // --- fromBytes (length-limited) ------------------------------------------
    check(StringId::fromBytes("foobar!", 6) == StringId::fromBytes("foobar", 6),
          "fromBytes honours length (ignores trailing byte)");
    check(StringId::fromBytes("foobar!", 6).hash() == fnv1a64("foobar"),
          "fromBytes(6) equals fnv1a64 of prefix");
    check(StringId::fromBytes("foo", 3) != StringId::fromBytes("food", 4),
          "fromBytes distinguishes differing lengths");

    // --- COMPILE-TIME --------------------------------------------------------
    static_assert(StringId("x").valid(), "constexpr in test TU");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
