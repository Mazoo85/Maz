#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <functional>
#include <algorithm>
#include <unordered_map>

#include "maz/ecs/World.hpp"
#include "maz/core/Serialization.hpp"
#include "maz/core/StringId.hpp"
#include "maz/core/Assert.hpp"

// A registration-based ECS scene (de)serializer — the Godot scene save/load
// analog (a PackedScene write/read over a maz::ecs::World). Component TYPES opt
// in via registerComponent<T>(tag, writeFn, readFn); save() walks the union of
// the registered types' each<T> (an entity with NO registered component is not
// saved), assigns each surviving entity a deterministic ORDINAL (entities sorted
// by index, deduped), and emits, per entity, the tag + a length-prefixed body
// for every registered component it has. load() re-creates the entities in
// ordinal order and replays the bodies. It composes iter16
// maz::core::ByteWriter/ByteReader (fixed little-endian, fail-safe reads).
//
// Entity-REFERENCE fields (e.g. a Parent link) round-trip through EntityRemap:
// the field hooks translate an Entity to its ordinal on save and back on load,
// so a Parent survives the index/generation reassignment that create() hands out
// in the destination World (a parent not itself serialized remaps to null).
// Component bodies are LENGTH-PREFIXED (writeBytes emits a u64 length), so a
// loader that does not know a tag SKIPS exactly that body and keeps going —
// forward-compat with newer/optional component types. load() is FAIL-SAFE: a
// corrupt or truncated buffer (bad magic/version, a bogus count, a body length
// past the end) returns false without throwing or reading out of bounds — load
// into a scratch World and discard it on false. This type is COMPONENT-AGNOSTIC
// (it never names a concrete component) and is NOT thread-safe. Versioned
// migration, prefabs/blueprints, and reflection-based auto-registration are
// future refinements.

namespace maz::scene {

// Ordinal<->entity table threaded through the field hooks so entity-reference
// fields survive (de)serialization. On save it maps a live Entity to the dense
// ordinal that entity was assigned; on load it maps an ordinal back to the
// freshly-created Entity. A reference to an entity that was not serialized (or a
// stale/generation-mismatched handle) maps to kNull / Entity::null().
class EntityRemap {
public:
    static constexpr std::uint32_t kNull = 0xFFFFFFFFu;

    // --- SAVE side ---
    void addSave(maz::ecs::Entity e, std::uint32_t ordinal) {
        m_toOrdinal[e.index] = { ordinal, e.generation };
    }
    std::uint32_t toOrdinal(maz::ecs::Entity e) const {
        if (e.isNull()) { return kNull; }
        auto it = m_toOrdinal.find(e.index);
        if (it == m_toOrdinal.end() || it->second.second != e.generation) { return kNull; }
        return it->second.first;
    }

    // --- LOAD side ---
    void addLoad(maz::ecs::Entity e) { m_toEntity.push_back(e); }
    maz::ecs::Entity toEntity(std::uint32_t ordinal) const {
        if (ordinal == kNull || ordinal >= m_toEntity.size()) { return maz::ecs::Entity::null(); }
        return m_toEntity[ordinal];
    }

private:
    std::unordered_map<std::uint32_t, std::pair<std::uint32_t, std::uint32_t>> m_toOrdinal;  // entity index -> (ordinal, generation)
    std::vector<maz::ecs::Entity> m_toEntity;                                                // by ordinal
};

class SceneSerializer {
public:
    // Field hooks a component TYPE supplies at registration. WriteFn serializes a
    // T's fields into a per-component body ByteWriter (entity-reference fields use
    // the EntityRemap to emit ordinals); ReadFn reverses it into a default-
    // constructed T (translating ordinals back to entities via the EntityRemap).
    template <class T>
    using WriteFn = std::function<void(maz::core::ByteWriter&, const T&, const EntityRemap&)>;
    template <class T>
    using ReadFn = std::function<void(maz::core::ByteReader&, T&, const EntityRemap&)>;

    // Register a component type under a unique tag. The tag identifies the type in
    // the byte stream (so an unknown tag can be skipped on load). Duplicate tags
    // and invalid tags are programmer errors (MAZ_ASSERT).
    template <class T>
    void registerComponent(maz::core::StringId tag, WriteFn<T> writeFn, ReadFn<T> readFn) {
        MAZ_ASSERT(tag.valid(), "SceneSerializer::registerComponent: invalid tag");
        MAZ_ASSERT(!hasTag(tag), "SceneSerializer::registerComponent: duplicate tag");
        Registered reg;
        reg.tag = tag;
        reg.collect = [](maz::ecs::World& w, std::vector<maz::ecs::Entity>& out) {
            w.each<T>([&](maz::ecs::Entity e, T&) { out.push_back(e); });
        };
        reg.has = [](maz::ecs::World& w, maz::ecs::Entity e) { return w.has<T>(e); };
        reg.write = [writeFn](maz::core::ByteWriter& bw, maz::ecs::World& w, maz::ecs::Entity e, const EntityRemap& rm) {
            T* c = w.get<T>(e);  // precondition: caller only writes when has()==true, so c != null
            maz::core::ByteWriter body;
            writeFn(body, *c, rm);
            bw.writeBytes(body.data(), body.size());  // length-prefixed body
        };
        reg.read = [readFn](maz::core::ByteReader& br, maz::ecs::World& w, maz::ecs::Entity e, const EntityRemap& rm) {
            T c{};
            readFn(br, c, rm);
            w.add<T>(e, std::move(c));
        };
        m_types.push_back(std::move(reg));
    }

    // True if a component is already registered under tag.
    bool hasTag(maz::core::StringId tag) const {
        for (const auto& t : m_types) {
            if (t.tag == tag) { return true; }
        }
        return false;
    }

    std::size_t typeCount() const { return m_types.size(); }

    // Serialize every entity that owns at least one registered component into a
    // self-contained byte buffer. Deterministic: entities are ordered by index,
    // components by registration order.
    std::vector<std::uint8_t> save(maz::ecs::World& world) const {
        // 1. Collect the union of registered-type entities, then dedup by index.
        std::vector<maz::ecs::Entity> ents;
        for (const auto& t : m_types) { t.collect(world, ents); }
        std::sort(ents.begin(), ents.end(),
                  [](maz::ecs::Entity a, maz::ecs::Entity b) { return a.index < b.index; });
        ents.erase(std::unique(ents.begin(), ents.end(),
                               [](maz::ecs::Entity a, maz::ecs::Entity b) { return a.index == b.index; }),
                   ents.end());

        // 2. Build the save remap: ordinal i <-> ents[i].
        EntityRemap remap;
        for (std::size_t i = 0; i < ents.size(); ++i) {
            remap.addSave(ents[i], static_cast<std::uint32_t>(i));
        }

        // 3. Header.
        maz::core::ByteWriter w;
        w.writeU32(kMagic);
        w.writeU16(kVersion);
        w.writeU32(static_cast<std::uint32_t>(ents.size()));

        // 4. Per entity: component count, then (tag + length-prefixed body) each.
        for (maz::ecs::Entity e : ents) {
            std::uint32_t count = 0;
            for (const auto& t : m_types) {
                if (t.has(world, e)) { ++count; }
            }
            w.writeU32(count);
            for (const auto& t : m_types) {
                if (t.has(world, e)) {
                    w.writeStringId(t.tag);
                    t.write(w, world, e, remap);
                }
            }
        }

        // 5. Done.
        return w.buffer();
    }

    // Reconstruct entities+components from bytes into world. Returns false on any
    // corruption/truncation without throwing or reading out of bounds; on false,
    // world may hold a partial result (load into a scratch World and discard it).
    bool load(const std::vector<std::uint8_t>& bytes, maz::ecs::World& world) const {
        maz::core::ByteReader r(bytes);
        std::uint32_t magic = 0;
        std::uint16_t ver = 0;
        std::uint32_t n = 0;
        if (!r.readU32(magic) || !r.readU16(ver) || !r.readU32(n)) { return false; }
        if (magic != kMagic || ver != kVersion) { return false; }

        // Sanity-cap n against remaining bytes BEFORE creating entities: each entity
        // body carries at least a u32 count (4 bytes), so n can never exceed the
        // bytes left — this rejects a corrupt huge n before a huge-alloc DoS.
        if (n > r.remaining()) { return false; }

        EntityRemap remap;
        for (std::uint32_t i = 0; i < n; ++i) { remap.addLoad(world.create()); }

        for (std::uint32_t i = 0; i < n; ++i) {
            std::uint32_t count = 0;
            if (!r.readU32(count)) { return false; }
            for (std::uint32_t c = 0; c < count; ++c) {
                maz::core::StringId tag;
                if (!r.readStringId(tag)) { return false; }
                std::uint64_t len = 0;
                if (!r.readU64(len)) { return false; }
                if (len > r.remaining()) { return false; }
                // Sub-reader bounded to exactly len bytes so a component read can't
                // over-read into the next entity. Take the pointer + position BEFORE
                // skipping past the body.
                maz::core::ByteReader body(bytes.data() + r.position(), static_cast<std::size_t>(len));
                r.skip(static_cast<std::size_t>(len));
                const Registered* found = nullptr;
                for (const auto& t : m_types) {
                    if (t.tag == tag) { found = &t; break; }
                }
                if (found != nullptr) {
                    found->read(body, world, remap.toEntity(i), remap);
                }
                // Unknown tag: the skip already advanced r past the body (forward-compat).
            }
        }
        return r.ok();
    }

private:
    struct Registered {
        maz::core::StringId tag;
        std::function<void(maz::ecs::World&, std::vector<maz::ecs::Entity>&)> collect;
        std::function<bool(maz::ecs::World&, maz::ecs::Entity)> has;
        std::function<void(maz::core::ByteWriter&, maz::ecs::World&, maz::ecs::Entity, const EntityRemap&)> write;
        std::function<void(maz::core::ByteReader&, maz::ecs::World&, maz::ecs::Entity, const EntityRemap&)> read;
    };
    std::vector<Registered> m_types;
    static constexpr std::uint32_t kMagic = 0x4D5A5343u;  // 'MZSC'
    static constexpr std::uint16_t kVersion = 1;
};

} // namespace maz::scene
