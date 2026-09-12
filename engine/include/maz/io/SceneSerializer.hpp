#pragma once

#include "maz/ecs/World.hpp"
#include "maz/io/Json.hpp"

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

namespace maz::io {

// Reflection-lite ECS scene serialization: save and load a live ecs::World as JSON. The ECS stores
// arbitrary component types in type-erased pools, so — without a full reflection system — the app
// tells the serializer, once, how each component maps to/from JSON:
//
//   io::SceneSerializer s;
//   s.component<Transform>("Transform",
//       [](const Transform& t){ io::JsonValue j; j.set("x", t.x); j.set("y", t.y); return j; },
//       [](const io::JsonValue& j){ return Transform{ j["x"].asFloat(), j["y"].asFloat() }; });
//
// saveWorld then emits { "entities": [ { "id": N, "components": { "Transform": {..}, .. } }, .. ] }
// with entities in ascending-id order (deterministic), and loadWorld rebuilds the world from that.
// This is the standard content-pipeline backbone for save games, prefabs, and an editor. It lives in
// the io layer so ecs stays dependency-free. Header-only.

class SceneSerializer {
public:
    // Register a component type with a display name and its to/from JSON converters.
    template <class T, class ToFn, class FromFn>
    void component(const std::string& name, ToFn toJson, FromFn fromJson) {
        CompOps ops;
        ops.name = name;
        ops.has = [](ecs::World& w, ecs::Entity e) { return w.has<T>(e); };
        ops.save = [toJson](ecs::World& w, ecs::Entity e) -> JsonValue {
            return toJson(*w.get<T>(e));
        };
        ops.load = [fromJson](ecs::World& w, ecs::Entity e, const JsonValue& j) {
            w.add<T>(e, fromJson(j));
        };
        ops.collect = [](ecs::World& w, std::unordered_set<ecs::Entity>& ids) {
            w.each<T>([&](ecs::Entity e, T&) { ids.insert(e); });
        };
        m_ops.push_back(std::move(ops));
    }

    // Serialize every entity that has at least one registered component. Entities are emitted in
    // ascending-id order and each entity's components in registration order, so the output is stable.
    JsonValue saveWorld(ecs::World& world) const {
        std::unordered_set<ecs::Entity> idset;
        for (const CompOps& op : m_ops) op.collect(world, idset);
        std::vector<ecs::Entity> ids(idset.begin(), idset.end());
        std::sort(ids.begin(), ids.end());

        JsonValue entities = JsonValue::array();
        for (ecs::Entity e : ids) {
            JsonValue comps = JsonValue::object();
            for (const CompOps& op : m_ops) {
                if (op.has(world, e)) comps.set(op.name, op.save(world, e));
            }
            JsonValue ent = JsonValue::object();
            ent.set("id", static_cast<int>(e));
            ent.set("components", std::move(comps));
            entities.push_back(std::move(ent));
        }
        JsonValue root = JsonValue::object();
        root.set("entities", std::move(entities));
        return root;
    }

    // Rebuild a world from a serialized document. Each entity gets a fresh id (ids are not preserved,
    // since the world assigns them); components with no registered type are skipped. Returns the number
    // of entities created.
    int loadWorld(const JsonValue& root, ecs::World& world) const {
        int created = 0;
        for (const JsonValue& ent : root["entities"].items()) {
            const ecs::Entity e = world.create();
            ++created;
            const JsonValue& comps = ent["components"];
            for (const CompOps& op : m_ops) {
                const JsonValue& cj = comps[op.name];
                if (!cj.isNull()) op.load(world, e, cj);
            }
        }
        return created;
    }

    // Convenience: save/load straight to/from a JSON file on disk.
    bool saveWorldFile(ecs::World& world, const std::string& path) const {
        return writeJsonFile(path, saveWorld(world), 2);
    }
    int loadWorldFile(const std::string& path, ecs::World& world) const {
        auto parsed = parseJsonFile(path);
        if (!parsed.ok) return 0;
        return loadWorld(parsed.value, world);
    }

private:
    struct CompOps {
        std::string name;
        std::function<bool(ecs::World&, ecs::Entity)> has;
        std::function<JsonValue(ecs::World&, ecs::Entity)> save;
        std::function<void(ecs::World&, ecs::Entity, const JsonValue&)> load;
        std::function<void(ecs::World&, std::unordered_set<ecs::Entity>&)> collect;
    };

    std::vector<CompOps> m_ops;
};

} // namespace maz::io
