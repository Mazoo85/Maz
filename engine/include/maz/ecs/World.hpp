#pragma once

#include <cstdint>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace maz::ecs {

// A lightweight entity-component system. Entities are ids; components live in per-type sparse
// sets. Iterate with each<T>() or view<A, B>(). Header-only so component types stay generic.
//
//   World w;
//   Entity e = w.create();
//   w.add<Transform>(e, {0, 0});
//   w.add<Velocity>(e, {10, 0});
//   w.view<Transform, Velocity>([&](Entity, Transform& t, Velocity& v) { t.x += v.vx * dt; });

using Entity = uint32_t;
constexpr Entity kNull = 0;

namespace detail {

struct IPool {
    virtual ~IPool() = default;
    virtual void remove(Entity e) = 0;
};

// Sparse set: a dense component array kept parallel to its entity array, plus an entity->index
// map. Removal swaps with the last element (O(1)), so iteration order is unspecified.
template <class T>
class Pool : public IPool {
public:
    T& add(Entity e, const T& value) {
        auto it = m_index.find(e);
        if (it != m_index.end()) {
            m_data[it->second] = value;
            return m_data[it->second];
        }
        m_index.emplace(e, m_dense.size());
        m_dense.push_back(e);
        m_data.push_back(value);
        return m_data.back();
    }

    bool has(Entity e) const { return m_index.find(e) != m_index.end(); }

    T* tryGet(Entity e) {
        auto it = m_index.find(e);
        return it == m_index.end() ? nullptr : &m_data[it->second];
    }

    void remove(Entity e) override {
        auto it = m_index.find(e);
        if (it == m_index.end()) {
            return;
        }
        const std::size_t idx = it->second;
        const std::size_t last = m_dense.size() - 1;
        m_dense[idx] = m_dense[last];
        m_data[idx] = std::move(m_data[last]);
        m_index[m_dense[idx]] = idx;
        m_dense.pop_back();
        m_data.pop_back();
        m_index.erase(it);
    }

    std::size_t size() const { return m_dense.size(); }
    Entity entityAt(std::size_t i) const { return m_dense[i]; }
    T& dataAt(std::size_t i) { return m_data[i]; }

private:
    std::vector<Entity> m_dense;
    std::vector<T> m_data;
    std::unordered_map<Entity, std::size_t> m_index;
};

} // namespace detail

class World {
public:
    Entity create() {
        Entity e;
        if (!m_free.empty()) {
            e = m_free.back();
            m_free.pop_back();
        } else {
            e = m_nextId++;
        }
        m_alive.insert(e);
        return e;
    }

    void destroy(Entity e) {
        if (m_alive.erase(e) == 0) {
            return;
        }
        for (auto& [type, pool] : m_pools) {
            pool->remove(e);
        }
        m_free.push_back(e);
    }

    bool valid(Entity e) const { return m_alive.find(e) != m_alive.end(); }
    std::size_t size() const { return m_alive.size(); }

    template <class T>
    T& add(Entity e, const T& value) {
        return pool<T>().add(e, value);
    }
    template <class T>
    bool has(Entity e) {
        return pool<T>().has(e);
    }
    template <class T>
    T* get(Entity e) {
        return pool<T>().tryGet(e);
    }
    template <class T>
    void remove(Entity e) {
        pool<T>().remove(e);
    }

    // Iterate every entity that has component T: fn(Entity, T&).
    template <class T, class Fn>
    void each(Fn&& fn) {
        auto& p = pool<T>();
        for (std::size_t i = 0; i < p.size(); ++i) {
            fn(p.entityAt(i), p.dataAt(i));
        }
    }

    // Iterate every entity that has both A and B: fn(Entity, A&, B&). Walks the A pool and
    // looks up B, so put the rarer component first for speed.
    template <class A, class B, class Fn>
    void view(Fn&& fn) {
        auto& pa = pool<A>();
        auto& pb = pool<B>();
        for (std::size_t i = 0; i < pa.size(); ++i) {
            const Entity e = pa.entityAt(i);
            if (B* b = pb.tryGet(e)) {
                fn(e, pa.dataAt(i), *b);
            }
        }
    }

private:
    template <class T>
    detail::Pool<T>& pool() {
        const std::type_index ti(typeid(T));
        auto it = m_pools.find(ti);
        if (it == m_pools.end()) {
            auto owned = std::make_unique<detail::Pool<T>>();
            detail::Pool<T>* raw = owned.get();
            m_pools.emplace(ti, std::move(owned));
            return *raw;
        }
        return *static_cast<detail::Pool<T>*>(it->second.get());
    }

    Entity m_nextId = 1; // 0 == kNull
    std::vector<Entity> m_free;
    std::unordered_set<Entity> m_alive;
    std::unordered_map<std::type_index, std::unique_ptr<detail::IPool>> m_pools;
};

} // namespace maz::ecs
