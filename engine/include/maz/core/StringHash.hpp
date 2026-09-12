#pragma once

#include "maz/core/Utf8.hpp"

#include <cstdint>
#include <string>

// maz::core String hashing — Godot's String.hash() / hash64(): the classic djb2 hash (hash*33 + c,
// seeded at 5381). Godot's String is code-point based, so it hashes over Unicode CODE POINTS, not raw
// UTF-8 bytes — meaning "é" hashes as one value (U+00E9), not as its two encoded bytes. These reuse
// core::utf8Decode so a UTF-8 std::string hashes identically to the same text in Godot. Deterministic,
// header-only; used for fast string keying, dictionary bucketing, and content fingerprints.
namespace maz::core {

// 32-bit djb2 hash over code points — Godot's String.hash().
inline std::uint32_t stringHash32(const std::string& s) {
    std::uint32_t h = 5381u;
    for (char32_t cp : utf8Decode(s)) {
        h = ((h << 5) + h) + static_cast<std::uint32_t>(cp); // h * 33 + cp
    }
    return h;
}

// 64-bit djb2 hash over code points — Godot's String.hash64().
inline std::uint64_t stringHash64(const std::string& s) {
    std::uint64_t h = 5381u;
    for (char32_t cp : utf8Decode(s)) {
        h = ((h << 5) + h) + static_cast<std::uint64_t>(cp);
    }
    return h;
}

} // namespace maz::core
