#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace maz::core {

// Interned strings — Godot's StringName. A game refers to the same names constantly (node names,
// signal names, animation tracks, input actions, entity tags), and comparing/hashing those as raw
// std::strings is slow and allocation-heavy. INTERNING each unique string once, into a table, turns
// every later reference into a small integer HANDLE: comparison is an int compare, hashing is trivial,
// and the original text is one reverse lookup away. Maz had no such facility — every subsystem
// hand-hashed or string-compared. This adds a `StringTable` (own the pool) + a lightweight `StringId`
// handle, plus the FNV-1a hash the table uses internally. std-only, header-only, no engine deps.

// A stable 32-bit FNV-1a hash of a byte string. Deterministic across runs/platforms — usable as a
// content hash, a hash-map key, or a quick "probably-equal" pre-check.
inline uint32_t fnv1a32(std::string_view s) {
    uint32_t h = 0x811C9DC5u; // offset basis
    for (char c : s) {
        h ^= static_cast<uint32_t>(static_cast<unsigned char>(c));
        h *= 0x01000193u; // FNV prime
    }
    return h;
}

inline constexpr uint32_t kInvalidStringId = 0xFFFFFFFFu;

// A handle into a StringTable. Two ids from the SAME table compare equal iff they name the same string
// (interning dedups), so name equality becomes an integer compare. Trivially copyable/hashable.
struct StringId {
    uint32_t value = kInvalidStringId;

    bool valid() const { return value != kInvalidStringId; }
    bool operator==(StringId o) const { return value == o.value; }
    bool operator!=(StringId o) const { return value != o.value; }
    bool operator<(StringId o) const { return value < o.value; } // for use as an ordered-map key
};

// Owns a pool of unique strings. `intern` adds-or-finds (returning a stable id), `find` looks up
// without inserting, `str` reverses an id back to its text. Ids are dense insertion indices, stable
// for the table's lifetime. Not thread-safe (wrap externally if shared) — matching the rest of core.
class StringTable {
public:
    // Add `s` if new, else return its existing id. Same text → same id, always.
    StringId intern(std::string_view s) {
        auto it = lookup_.find(std::string(s));
        if (it != lookup_.end()) {
            return StringId{it->second};
        }
        const uint32_t id = static_cast<uint32_t>(strings_.size());
        strings_.emplace_back(s);
        hashes_.push_back(fnv1a32(s));
        lookup_.emplace(strings_.back(), id);
        return StringId{id};
    }

    // Look up without inserting. Returns an invalid id if `s` was never interned.
    StringId find(std::string_view s) const {
        auto it = lookup_.find(std::string(s));
        return it == lookup_.end() ? StringId{kInvalidStringId} : StringId{it->second};
    }

    bool contains(std::string_view s) const { return lookup_.find(std::string(s)) != lookup_.end(); }

    // Reverse an id to its text. Returns the empty string for an invalid/out-of-range id.
    const std::string& str(StringId id) const {
        static const std::string kEmpty;
        return id.value < strings_.size() ? strings_[id.value] : kEmpty;
    }

    // The FNV-1a hash of an interned id's text (0 for an invalid id).
    uint32_t hash(StringId id) const {
        return id.value < hashes_.size() ? hashes_[id.value] : 0u;
    }

    std::size_t size() const { return strings_.size(); }
    bool empty() const { return strings_.empty(); }
    void clear() {
        strings_.clear();
        hashes_.clear();
        lookup_.clear();
    }

private:
    std::vector<std::string> strings_;              // id -> text
    std::vector<uint32_t> hashes_;                  // id -> FNV hash
    std::unordered_map<std::string, uint32_t> lookup_; // text -> id
};

} // namespace maz::core

// Hash specialization so StringId can be a key in unordered containers.
namespace std {
template <>
struct hash<maz::core::StringId> {
    std::size_t operator()(maz::core::StringId s) const noexcept {
        return std::hash<uint32_t>()(s.value);
    }
};
} // namespace std
