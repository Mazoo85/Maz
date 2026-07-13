#pragma once

#include <cstdint>
#include <cstddef>

namespace maz::core {

// Compile-time FNV-1a 64-bit hash, used to turn short string literals into
// cheap, comparable integer identifiers (see StringId below).
inline constexpr uint64_t kFnvOffset = 14695981039346656037ULL;
inline constexpr uint64_t kFnvPrime  = 1099511628211ULL;

// Hash an explicit byte range (may contain embedded NULs).
constexpr uint64_t fnv1a64(const char* data, size_t len) {
    uint64_t h = kFnvOffset;
    for (size_t i = 0; i < len; ++i) {
        // Double cast is mandatory: unsigned char makes bytes >= 0x80 well-defined,
        // and the widening to uint64_t keeps -Wconversion quiet.
        h ^= static_cast<uint64_t>(static_cast<unsigned char>(data[i]));
        h *= kFnvPrime;
    }
    return h;
}

// Hash a NUL-terminated C string (terminator excluded).
constexpr uint64_t fnv1a64(const char* str) {
    uint64_t h = kFnvOffset;
    for (; *str != '\0'; ++str) {
        h ^= static_cast<uint64_t>(static_cast<unsigned char>(*str));
        h *= kFnvPrime;
    }
    return h;
}

// A hashed string identifier. Cheap to copy, compare, and store; the original
// text is not retained.
struct StringId {
    uint64_t value = 0;  // 0 = invalid/empty sentinel (a real string could hash to 0
                         // with prob 2^-64 — acceptable for a hash-only id)

    constexpr StringId() = default;
    constexpr StringId(const char* str) : value(fnv1a64(str)) {}  // implicit, ergonomic
    explicit constexpr StringId(uint64_t hashValue) : value(hashValue) {}

    static constexpr StringId fromBytes(const char* data, size_t len) {
        return StringId(fnv1a64(data, len));  // paren form hits the explicit ctor
    }

    constexpr bool valid() const { return value != 0; }
    constexpr uint64_t hash() const { return value; }

    constexpr bool operator==(const StringId& o) const { return value == o.value; }
    constexpr bool operator!=(const StringId& o) const { return value != o.value; }
    constexpr bool operator<(const StringId& o) const { return value < o.value; }
};

// User-defined literal: "foo"_sid -> StringId. The leading underscore is what
// makes `_sid` a valid user suffix (suffixes without one are reserved); the space
// in `operator"" _sid` is the conventional pre-C++23 spelling.
constexpr StringId operator"" _sid(const char* str, size_t len) {
    return StringId::fromBytes(str, len);
}

// Canonical FNV-1a 64-bit test vectors, verified at compile time.
static_assert(fnv1a64("") == 0xcbf29ce484222325ULL, "FNV-1a64 empty");
static_assert(fnv1a64("a") == 0xaf63dc4c8601ec8cULL, "FNV-1a64 a");
static_assert(fnv1a64("foobar") == 0x85944171f73967e8ULL, "FNV-1a64 foobar");
static_assert(StringId("foobar").value == 0x85944171f73967e8ULL, "ctor hashes");
static_assert("foobar"_sid == StringId("foobar"), "UDL == ctor");
static_assert(!StringId().valid(), "default invalid");

} // namespace maz::core
