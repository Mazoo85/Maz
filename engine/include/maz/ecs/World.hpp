#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <memory>
#include <utility>  // std::move

#include "maz/core/SparseSet.hpp"
#include "maz/core/Bitset.hpp"
#include "maz/core/Assert.hpp"

// A minimal ECS World — the entity/component runtime (a Godot node/component
// analog). Entities are generational opaque handles; components are stored in
// type-erased per-type SparseSets keyed by entity index (the Pool/SparseSet
// density carried up a layer); per-entity Bitset<kMaxComponents> signatures
// track which components an entity has. FIRST SLICE: single-component each<T>
// iteration only — a multi-component view<Ts...> is a future refinement
// (mentioned, not built here). NOT thread-safe.

namespace maz::ecs {

// Opaque handle; generation 0 is null/dead; a recycled index at a new
// generation is a DIFFERENT entity.
struct Entity {
    std::uint32_t index = 0;
    std::uint32_t generation = 0;  // 0 == null/never-issued; live generations >= 1
    constexpr bool operator==(const Entity& o) const { return index == o.index && generation == o.generation; }
    constexpr bool operator!=(const Entity& o) const { return !(*this == o); }
    constexpr bool isNull() const { return generation == 0; }
    static constexpr Entity null() { return Entity{}; }
};

namespace detail {
inline std::size_t nextComponentTypeId() { static std::size_t c = 0; return c++; }
template <class T> std::size_t componentTypeId() { static const std::size_t id = nextComponentTypeId(); return id; }
} // namespace detail
// Dense component-type index [0,inf) assigned first-come per distinct T;
// PROCESS-GLOBAL (shared across World instances, never reset between tests) —
// used as a Bitset bit index + m_stores index. NOT type_id's hash (which is a
// 64-bit hash, unusable as a dense index).

struct IComponentStore {
    virtual ~IComponentStore() = default;
    virtual void remove(std::uint32_t entityIndex) = 0;
};
template <class T>
struct ComponentStore : IComponentStore {
    maz::core::SparseSet<T> set;
    void remove(std::uint32_t e) override { set.remove(e); }  // safe no-op if absent
};

class World {
  public:
    static constexpr std::size_t kMaxComponents = 64;  // compile-time cap on distinct component TYPES (== Bitset width)

    Entity create() {
        if (!m_freeIndices.empty()) {
            std::uint32_t i = m_freeIndices.back();
            m_freeIndices.pop_back();
            // m_generations[i] already holds the live (post-destroy bumped) generation; do NOT touch it.
            m_signatures[i].reset();  // defensive; destroy already reset it
            ++m_entityCount;
            return Entity{i, m_generations[i]};
        }
        std::uint32_t i = static_cast<std::uint32_t>(m_generations.size());
        m_generations.push_back(1u);          // fresh generation starts at 1, never 0
        m_signatures.push_back({});
        ++m_entityCount;
        return Entity{i, 1u};
    }

    bool alive(Entity e) const {
        return e.generation != 0u
            && e.index < static_cast<std::uint32_t>(m_generations.size())
            && m_generations[e.index] == e.generation;
    }

    void destroy(Entity e) {
        MAZ_ASSERT(alive(e), "World::destroy on a dead/stale entity");
        const std::uint32_t i = e.index;
        for (auto& store : m_stores) { if (store) { store->remove(i); } }  // remove from ALL stores (SparseSet::remove is a safe no-op if absent) — the linchpin: a recycled index must inherit NO stale component
        m_signatures[i].reset();
        ++m_generations[i];
        if (m_generations[i] == 0u) { m_generations[i] = 1u; }  // skip 0 on wrap
        m_freeIndices.push_back(i);
        --m_entityCount;
    }

    std::size_t entityCount() const { return m_entityCount; }

    template <class T> T& add(Entity e, const T& value) {
        MAZ_ASSERT(alive(e), "World::add on a dead entity");
        ComponentStore<T>& s = storeFor<T>();
        s.set.insert(e.index, value);  // overwrite-on-re-add is fine
        m_signatures[e.index].set(detail::componentTypeId<T>());
        return *s.set.get(e.index);
    }
    template <class T> T& add(Entity e, T&& value) {
        MAZ_ASSERT(alive(e), "World::add on a dead entity");
        ComponentStore<T>& s = storeFor<T>();
        s.set.insert(e.index, std::move(value));
        m_signatures[e.index].set(detail::componentTypeId<T>());
        return *s.set.get(e.index);
    }

    template <class T> T* get(Entity e) {
        if (!alive(e)) { return nullptr; }
        const std::size_t id = detail::componentTypeId<T>();
        if (id >= m_stores.size() || !m_stores[id]) { return nullptr; }
        return static_cast<ComponentStore<T>*>(m_stores[id].get())->set.get(e.index);
    }
    template <class T> const T* get(Entity e) const {
        if (!alive(e)) { return nullptr; }
        const std::size_t id = detail::componentTypeId<T>();
        if (id >= m_stores.size() || !m_stores[id]) { return nullptr; }
        return static_cast<const ComponentStore<T>*>(m_stores[id].get())->set.get(e.index);
    }

    template <class T> bool has(Entity e) const {
        return alive(e)
            && detail::componentTypeId<T>() < kMaxComponents
            && m_signatures[e.index].test(detail::componentTypeId<T>());
    }

    template <class T> void remove(Entity e) {
        MAZ_ASSERT(alive(e), "World::remove on a dead entity");
        const std::size_t id = detail::componentTypeId<T>();
        if (id < m_stores.size() && m_stores[id]) {
            static_cast<ComponentStore<T>*>(m_stores[id].get())->set.remove(e.index);
        }
        if (id < kMaxComponents) { m_signatures[e.index].clear(id); }  // clear bit even if store absent
    }

    template <class T> std::size_t componentCount() const {
        const std::size_t id = detail::componentTypeId<T>();
        if (id >= m_stores.size() || !m_stores[id]) { return 0; }
        return static_cast<const ComponentStore<T>*>(m_stores[id].get())->set.size();
    }

    template <class T, class F> void each(F&& fn) {
        const std::size_t id = detail::componentTypeId<T>();
        if (id >= m_stores.size() || !m_stores[id]) { return; }
        auto& s = static_cast<ComponentStore<T>&>(*m_stores[id]).set;
        const std::vector<std::uint32_t>& keys = s.keys();
        std::vector<T>& vals = s.values();
        for (std::size_t k = 0; k < keys.size(); ++k) {
            const std::uint32_t idx = keys[k];
            fn(Entity{idx, m_generations[idx]}, vals[k]);
        }
    }
    // fn receives (Entity, T&). Iteration order is SparseSet dense order
    // (unspecified; swap-and-pop reorders on remove). Do NOT add/remove
    // components of type T during each<T> (iterator invalidation, like mutating
    // a std::vector mid-loop). Only non-const each ships this slice;
    // view<Ts...> is deferred.

  private:
    template <class T> ComponentStore<T>& storeFor() {
        const std::size_t id = detail::componentTypeId<T>();
        MAZ_ASSERT(id < kMaxComponents, "World: too many distinct component types (kMaxComponents cap)");
        if (id >= m_stores.size()) { m_stores.resize(id + 1); }
        if (!m_stores[id]) { m_stores[id] = std::make_unique<ComponentStore<T>>(); }
        return static_cast<ComponentStore<T>&>(*m_stores[id]);
    }

    std::vector<std::uint32_t> m_generations;                    // per index: current occupant's generation
    std::vector<maz::core::Bitset<kMaxComponents>> m_signatures; // per index: component bits
    std::vector<std::uint32_t> m_freeIndices;                    // recyclable index free-list
    std::vector<std::unique_ptr<IComponentStore>> m_stores;      // indexed by componentTypeId<T>()
    std::size_t m_entityCount = 0;
};

} // namespace maz::ecs
