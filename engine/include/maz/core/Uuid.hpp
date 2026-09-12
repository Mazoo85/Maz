#pragma once

#include <cstdint>
#include <string>

// maz::core UUID (v4) — generate RFC 4122 version-4 (random) universally-unique identifiers for entity IDs, save
// files, network sessions, asset GUIDs, and analytics events. `makeUuidV4` fills 128 bits from any engine RNG that
// exposes `uint32_t next()` (e.g. `core::Pcg32`) and stamps the version + variant bits; `Uuid::toString` renders
// the canonical lowercase `xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx` form; `isValidUuid` validates that layout. Because
// the randomness comes from the caller's seeded RNG, generation is fully deterministic and unit-testable. Godot
// has no UUID type, so this is a beyond-Godot utility. Header-only, std-only.
namespace maz::core {

// A 128-bit UUID as 16 bytes, big-endian in canonical text order.
struct Uuid {
    std::uint8_t bytes[16] = {};

    // Canonical lowercase text form "xxxxxxxx-xxxx-Vxxx-yxxx-xxxxxxxxxxxx".
    std::string toString() const {
        static const char* const hex = "0123456789abcdef";
        std::string s;
        s.reserve(36);
        for (int i = 0; i < 16; ++i) {
            if (i == 4 || i == 6 || i == 8 || i == 10) s.push_back('-');
            s.push_back(hex[bytes[i] >> 4]);
            s.push_back(hex[bytes[i] & 0x0F]);
        }
        return s;
    }

    bool operator==(const Uuid& o) const {
        for (int i = 0; i < 16; ++i)
            if (bytes[i] != o.bytes[i]) return false;
        return true;
    }
    bool operator!=(const Uuid& o) const { return !(*this == o); }
};

// Generate a version-4 UUID from `rng` (any type with `uint32_t next()`). Four 32-bit draws fill the 16 bytes;
// the version nibble is set to 4 and the variant bits to 10xx per RFC 4122.
template <typename Rng>
inline Uuid makeUuidV4(Rng& rng) {
    Uuid u;
    for (int i = 0; i < 16; i += 4) {
        const std::uint32_t r = rng.next();
        u.bytes[i + 0] = static_cast<std::uint8_t>(r >> 24);
        u.bytes[i + 1] = static_cast<std::uint8_t>(r >> 16);
        u.bytes[i + 2] = static_cast<std::uint8_t>(r >> 8);
        u.bytes[i + 3] = static_cast<std::uint8_t>(r);
    }
    u.bytes[6] = static_cast<std::uint8_t>((u.bytes[6] & 0x0F) | 0x40); // version 4
    u.bytes[8] = static_cast<std::uint8_t>((u.bytes[8] & 0x3F) | 0x80); // variant 10xx
    return u;
}

// Convenience: generate a v4 UUID and return it already formatted.
template <typename Rng>
inline std::string uuidV4String(Rng& rng) { return makeUuidV4(rng).toString(); }

// Validate the canonical 36-char UUID layout: hex digits with hyphens at positions 8/13/18/23 (case-insensitive).
// Checks structure only, not the version/variant nibbles.
inline bool isValidUuid(const std::string& s) {
    if (s.size() != 36) return false;
    for (std::size_t i = 0; i < 36; ++i) {
        const char c = s[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-') return false;
        } else {
            const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
            if (!hex) return false;
        }
    }
    return true;
}

} // namespace maz::core
