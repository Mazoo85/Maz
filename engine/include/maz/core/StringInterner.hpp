#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <cstddef>

#include "maz/core/StringId.hpp"
#include "maz/core/Assert.hpp"

// A string interning table. intern(s) hashes a string into a StringId (via the
// iter7 StringId::fromBytes) AND stores the original text, so resolve(id) can
// recover the source string (an empty view if the id was never interned).
// Interning the same string twice returns the same id and stores it once. This
// is the "original-string storage" companion to StringId (which is hash-only),
// mirroring Godot's StringName; use it for debug output, serialization
// round-tripping, or editor display. resolve returns a view INTO the interner's
// storage (valid until clear()/destruction/entry removal). A genuine FNV-1a 64
// hash collision (two different strings, same id) is caught by an assert in
// debug. NOT thread-safe. Ref-counting / entry removal and a global shared
// interner are future refinements.

namespace maz::core {

class StringInterner {
  public:
    // Interns s: hashes it to a StringId and stores the original string (once).
    // Interning the same string twice returns the same id without re-storing.
    // Returns the StringId, identical to StringId::fromBytes(s.data(), s.size()).
    maz::core::StringId intern(std::string_view s) {
        maz::core::StringId id = maz::core::StringId::fromBytes(s.data(), s.size());
        auto it = m_strings.find(id);
        if (it != m_strings.end()) {
            MAZ_ASSERT(it->second == s, "StringInterner::intern: hash collision (two different strings share a StringId)");
            return id;
        }
        m_strings.emplace(id, std::string(s));
        return id;
    }

    // Recovers the interned original string for id, or an EMPTY view if id was
    // never interned. The returned view borrows the interner's storage — it is
    // valid until that entry is removed / clear() / the interner is destroyed.
    std::string_view resolve(maz::core::StringId id) const {
        auto it = m_strings.find(id);
        return it == m_strings.end() ? std::string_view{} : std::string_view(it->second);
    }

    // Was this id interned?
    bool contains(maz::core::StringId id) const { return m_strings.find(id) != m_strings.end(); }

    // Was this string interned?
    bool contains(std::string_view s) const { return contains(maz::core::StringId::fromBytes(s.data(), s.size())); }

    std::size_t size() const { return m_strings.size(); }
    bool empty() const { return m_strings.empty(); }
    void clear() { m_strings.clear(); }

  private:
    struct StringIdHash {
        std::size_t operator()(maz::core::StringId s) const { return static_cast<std::size_t>(s.hash()); }
    };

    std::unordered_map<maz::core::StringId, std::string, StringIdHash> m_strings;
};

} // namespace maz::core
