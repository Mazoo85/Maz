#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>

#include "maz/core/StringId.hpp"

namespace maz::core {

// Compile-time type identity + minimal reflection — the Godot ClassDB/Variant-type
// analog (ROADMAP Phase-2 "Minimal reflection (type ids)"). Type ids ARE
// maz::core::StringId (composed from StringId, iter7): a type's id is fnv1a64 of
// its compiler-spelled name. Names are sliced from __PRETTY_FUNCTION__ (GCC/Clang)
// / __FUNCSIG__ (MSVC). Hashes are RUN-STABLE but COMPILER-DEPENDENT — never
// hard-code exact hash values; compare ids for equality, don't assume a specific
// number. Returned std::string_view points into compiler-static storage (static
// storage duration) — valid for the whole program, safe to store. NOT
// thread-safety-relevant (pure compile-time). MSVC path is written but CI-pending
// (only GCC/Clang run here).

namespace detail {

// Capture the compiler's decorated signature for T (embeds T's spelling).
template <class T>
constexpr std::string_view wrappedTypeName() {
#if defined(__clang__) || defined(__GNUC__)
    return __PRETTY_FUNCTION__;
#elif defined(_MSC_VER)
    return __FUNCSIG__;
#else
#error "maz::core::TypeId: unsupported compiler (no __PRETTY_FUNCTION__/__FUNCSIG__)"
#endif
}

// Self-calibrate prefix/suffix from a probe type spelled identically everywhere
// ("double"). The prefix/suffix decoration is byte-identical across all T (only
// the T=<spelling> token varies), so calibrating once from double is valid for
// every T. std::string_view(const char*) runs strlen at compile time (C++20
// constexpr) — fine. MSVC prints class/struct prefixes on T — acceptable, hash
// still stable per-compiler (CI-pending).
inline constexpr std::string_view kProbeName = "double";
inline constexpr std::size_t kPrefixLen = wrappedTypeName<double>().find(kProbeName);
inline constexpr std::size_t kSuffixLen = wrappedTypeName<double>().size() - kPrefixLen - kProbeName.size();

} // namespace detail

// The type's compiler-spelled name. Returned view points into the compiler-static
// signature array (program-lifetime, safe to store); the slice is NOT
// NUL-terminated at its end (see type_hash).
template <class T>
constexpr std::string_view type_name() {
    const std::string_view wrapped = detail::wrappedTypeName<T>();
    return wrapped.substr(detail::kPrefixLen, wrapped.size() - detail::kPrefixLen - detail::kSuffixLen);
}

// FNV-1a 64-bit hash of the type name. n.data() points mid-array with no '\0' at
// n.data()+n.size(), so the (const char*) fnv1a64 overload would read past the
// name — use the (data,len) overload EXCLUSIVELY.
template <class T>
constexpr std::uint64_t type_hash() {
    const std::string_view n = type_name<T>();
    return fnv1a64(n.data(), n.size());
}

// The type's id as a StringId. Paren form hits the explicit uint64 ctor.
template <class T>
constexpr StringId type_id() {
    return StringId(type_hash<T>());
}

struct TypeInfo {
    StringId id;
    std::string_view name;
    std::size_t size;
    std::size_t alignment;
};

// Full reflection record for T. Requires a COMPLETE, non-void T (needs
// sizeof/alignof). type_name/type_hash/type_id<void>() remain well-formed; only
// type_info<void>() is ill-formed. No void specialization (documented constraint).
template <class T>
constexpr TypeInfo type_info() {
    return TypeInfo{ type_id<T>(), type_name<T>(), sizeof(T), alignof(T) };
}

// Canonical compile-time properties (no hard-coded compiler-specific hashes).
static_assert(type_id<int>() == type_id<int>(), "type_id stable for int");
static_assert(type_id<int>() != type_id<float>(), "type_id distinguishes int/float");
static_assert(type_id<int>() != type_id<double>(), "type_id distinguishes int/double");
static_assert(type_hash<int>() != 0, "type_hash<int> nonzero");
static_assert(type_id<int>().value == type_hash<int>(), "type_id wraps type_hash");
static_assert(type_info<int>().size == sizeof(int), "type_info size for int");

} // namespace maz::core
