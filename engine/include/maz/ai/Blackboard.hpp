#pragma once

#include <any>
#include <cstddef>
#include <unordered_map>
#include <utility>  // std::move

#include "maz/core/StringId.hpp"
#include "maz/core/Assert.hpp"

// A typed key-value blackboard — the shared memory AI behaviors read and write.
// Store any copyable type under a StringId key (std::any-backed); retrieve with
// type-checked get<T> (programmer-error to mismatch — asserts) or the
// always-safe getOr<T>(key, fallback) (returns the fallback if the key is absent
// or the stored type differs). set overwrites last-write-wins, including changing
// a key's type. The glue between iter32 StateMachine / iter33 BehaviorTree /
// iter34 Steering. Composes iter7 maz::core::StringId. Values are copied in and
// out (store small values or handles/pointers). NOT thread-safe. A typed-view /
// change-notification / nested blackboards are future refinements (not built here).

namespace maz::ai {

class Blackboard {
  public:
    // Stores value under key (inserts or overwrites). Overwriting with a DIFFERENT
    // type is allowed — last write wins, the stored type included. key must be valid.
    template <typename T>
    void set(maz::core::StringId key, T value) {
        MAZ_ASSERT(key.valid(), "Blackboard::set: invalid key");
        m_values[key] = std::any(std::move(value));
    }

    // True if key is present under ANY stored type.
    bool contains(maz::core::StringId key) const {
        return m_values.find(key) != m_values.end();
    }

    // True if key is present AND stored as exactly T.
    template <typename T>
    bool has(maz::core::StringId key) const {
        auto it = m_values.find(key);
        return it != m_values.end() && it->second.type() == typeid(T);
    }

    // Returns a copy of the value stored under key as T. Programmer-error to get an
    // absent or wrong-typed key (guarded by assert; std::any_cast would throw
    // std::bad_any_cast on a type mismatch in release — the documented behavior).
    template <typename T>
    T get(maz::core::StringId key) const {
        auto it = m_values.find(key);
        MAZ_ASSERT(it != m_values.end(), "Blackboard::get: key not present");
        MAZ_ASSERT(it->second.type() == typeid(T), "Blackboard::get: type mismatch");
        return std::any_cast<T>(it->second);
    }

    // Safe accessor: returns the value stored under key as T, or fallback if the key
    // is absent OR stored as a different type. Never asserts/throws.
    template <typename T>
    T getOr(maz::core::StringId key, T fallback) const {
        auto it = m_values.find(key);
        if (it != m_values.end() && it->second.type() == typeid(T)) {
            return std::any_cast<T>(it->second);
        }
        return fallback;
    }

    // Removes key. Returns true if something was removed, false if key was absent.
    bool erase(maz::core::StringId key) {
        return m_values.erase(key) != 0;
    }

    void clear() { m_values.clear(); }

    std::size_t size() const { return m_values.size(); }

    bool empty() const { return m_values.empty(); }

  private:
    struct StringIdHash {
        std::size_t operator()(maz::core::StringId s) const { return static_cast<std::size_t>(s.hash()); }
    };

    std::unordered_map<maz::core::StringId, std::any, StringIdHash> m_values;
};

} // namespace maz::ai
