#pragma once

#include <cstddef>
#include <functional>
#include <vector>
#include <utility>  // std::move

#include "maz/ecs/World.hpp"
#include "maz/core/StringId.hpp"
#include "maz/core/Assert.hpp"

// An ordered ECS system scheduler — the ECS's per-tick "update schedule" (the
// Godot _process/_physics_process analog). Register named systems (callables
// over World&); run() invokes every ENABLED system in REGISTRATION ORDER. A
// system is std::function<void(World&)>, which holds a capturing lambda, a free
// function, a member function, or a maz::core::Delegate<void(World&)> (Delegate
// has operator(), so it converts in — non-capturing systems can use Delegate to
// avoid the heap; capturing systems pay the std::function allocation). Systems
// may freely mutate the World; do NOT add/remove systems from inside a running
// system (vector iterator invalidation during run(), same caveat as
// World::each/view). NOT thread-safe. Parallel/staged execution + system
// dependencies (before/after) are future refinements (not built here).

namespace maz::ecs {

using SystemFn = std::function<void(World&)>;

class SystemScheduler {
  public:
    void add(maz::core::StringId name, SystemFn fn) {
        // Fail fast on an empty callable here rather than std::bad_function_call at run().
        MAZ_ASSERT(static_cast<bool>(fn), "SystemScheduler::add: empty system function");
        MAZ_ASSERT(!has(name), "SystemScheduler::add: duplicate system name");
        m_systems.push_back(Entry{ name, std::move(fn), true });
    }

    // Runs every ENABLED system in registration order. Do NOT add/remove or
    // enable/disable systems from inside a running system — the vector is being
    // iterated (iterator invalidation, same caveat as World::each/view).
    void run(World& world) {
        for (Entry& e : m_systems) { if (e.enabled) { e.fn(world); } }
    }

    bool has(maz::core::StringId name) const {
        for (const Entry& e : m_systems) { if (e.name == name) { return true; } }
        return false;
    }

    // erase preserves the relative registration order of the remaining systems.
    bool remove(maz::core::StringId name) {
        for (std::size_t i = 0; i < m_systems.size(); ++i) {
            if (m_systems[i].name == name) { m_systems.erase(m_systems.begin() + static_cast<std::ptrdiff_t>(i)); return true; }
        }
        return false;
    }

    bool setEnabled(maz::core::StringId name, bool enabled) {
        for (Entry& e : m_systems) { if (e.name == name) { e.enabled = enabled; return true; } }
        return false;  // not found
    }

    bool isEnabled(maz::core::StringId name) const {
        for (const Entry& e : m_systems) { if (e.name == name) { return e.enabled; } }
        return false;  // absent -> not enabled
    }

    std::size_t size() const { return m_systems.size(); }
    void clear() { m_systems.clear(); }

  private:
    struct Entry {
        maz::core::StringId name;
        SystemFn fn;
        bool enabled;
    };

    // Name lookup is an O(n) linear scan (fine for a first slice — a
    // StringId-keyed map is a future optimization).
    std::vector<Entry> m_systems;  // registration order == run order
};

} // namespace maz::ecs
