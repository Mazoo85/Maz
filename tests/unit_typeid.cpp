// Unit tests for maz::core TypeId — compile-time type identifiers + minimal
// reflection (type_name/type_hash/type_id, TypeInfo). Stability, distinctness,
// name substrings, StringId cross-checks, and constexpr usability. No exact
// hash-value asserts (hashes are compiler-dependent). Pure C++, no GPU.

#include "maz/core/TypeId.hpp"

#include <cstdint>
#include <cstdio>
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

bool contains(std::string_view hay, std::string_view needle) {
    return hay.find(needle) != std::string_view::npos;
}

struct Alpha {};
struct Beta { int x; double y; };
namespace sub { struct Gamma {}; }

} // namespace

int main() {
    // --- STABILITY -----------------------------------------------------------
    {
        check(type_id<int>() == type_id<int>(), "type_id<int> stable");
        check(type_hash<int>() == type_hash<int>(), "type_hash<int> stable");
        check(type_id<Alpha>() == type_id<Alpha>(), "type_id<Alpha> stable");
    }

    // --- DISTINCTNESS --------------------------------------------------------
    {
        check(type_id<int>() != type_id<float>(), "int != float");
        check(type_id<int>() != type_id<double>(), "int != double");
        check(type_id<Alpha>() != type_id<Beta>(), "Alpha != Beta");
        check(type_id<Alpha>() != type_id<int>(), "Alpha != int");
        check(type_id<Alpha>() != type_id<sub::Gamma>(), "Alpha != sub::Gamma");
    }

    // --- NAME substring ------------------------------------------------------
    {
        check(contains(type_name<int>(), "int"), "name<int> contains int");
        check(contains(type_name<double>(), "double"), "name<double> contains double");
        check(contains(type_name<Alpha>(), "Alpha"), "name<Alpha> contains Alpha");
        check(contains(type_name<Beta>(), "Beta"), "name<Beta> contains Beta");
        check(contains(type_name<sub::Gamma>(), "Gamma"), "name<sub::Gamma> contains Gamma");
        // Negative asserts lock the slice down: a broken slice that leaked the
        // whole __PRETTY_FUNCTION__ signature would still contain "int"/"Beta"
        // above, so also prove the decoration was stripped off both ends.
        check(!contains(type_name<int>(), "wrappedTypeName"), "name<int> stripped the function-name prefix");
        check(!contains(type_name<int>(), "]"), "name<int> stripped the trailing bracket/suffix");
        check(!contains(type_name<Beta>(), "string_view"), "name<Beta> stripped the trailing template-arg suffix");
    }

    // --- type_id from hash ---------------------------------------------------
    {
        check(type_id<int>().hash() == type_hash<int>(), "id.hash() == type_hash");
        check(type_id<int>().value == type_hash<int>(), "id.value == type_hash");
        check(type_id<int>().valid(), "type_id<int> valid");
    }

    // --- TypeInfo ------------------------------------------------------------
    {
        check(type_info<int>().size == sizeof(int) && type_info<int>().alignment == alignof(int),
              "type_info<int> size/alignment");
        check(type_info<Beta>().size == sizeof(Beta) && type_info<Beta>().alignment == alignof(Beta),
              "type_info<Beta> size/alignment");
        check(type_info<Beta>().id == type_id<Beta>(), "type_info<Beta>.id == type_id<Beta>");
        check(contains(type_info<Beta>().name, "Beta"), "type_info<Beta>.name contains Beta");
    }

    // --- constexpr usability -------------------------------------------------
    {
        static_assert(type_id<int>() != type_id<float>(), "compile-time distinct");
        constexpr std::uint64_t h = type_hash<int>();
        check(h != 0, "type_hash usable as constexpr");
        static_assert(type_info<int>().size == sizeof(int), "constexpr type_info");
    }

    // --- cv/ptr qualifiers → distinct ids ------------------------------------
    // Qualified/pointer/reference spellings differ, so their ids differ too;
    // this is documented behavior, not a defect.
    {
        check(type_id<int>() != type_id<const int>(), "int != const int");
        check(type_id<int>() != type_id<int*>(), "int != int*");
        check(type_id<int>() != type_id<int&>(), "int != int&");
        check(type_id<const int>() == type_id<const int>(), "const int stable");
    }

    // --- cross-check to StringId (iter7) -------------------------------------
    {
        check(type_id<Alpha>() == StringId(type_hash<Alpha>()), "type_id == StringId(hash)");
        check(type_id<Alpha>() == StringId::fromBytes(type_name<Alpha>().data(), type_name<Alpha>().size()),
              "type_id == StringId::fromBytes(name)");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
