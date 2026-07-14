// Unit tests for maz::scene::SceneSerializer — the registration-based ECS scene
// (de)serializer. Exercises data-component round-trips (Name/Tag/LocalTransform),
// entity-reference remapping so a Parent hierarchy survives index/generation
// reassignment, unknown-tag forward-compat skipping, fail-safe load on
// corrupt/truncated/bad-magic/bad-version buffers, empty-world round-trip, and
// byte-for-byte determinism. Pure C++, no GPU/display.

#include "maz/scene/SceneSerializer.hpp"
#include "maz/ecs/Components.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

using namespace maz::scene;
using namespace maz::ecs;
using maz::core::operator""_sid;  // the UDL lives in maz::core; using-namespace does not pull UDLs

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

bool contains(const std::vector<Entity>& v, Entity e) {
    for (Entity x : v) {
        if (x == e) { return true; }
    }
    return false;
}

// Registers the iter-26 core components with the serializer.
void registerCoreComponents(SceneSerializer& s) {
    s.registerComponent<Name>(
        "Name"_sid,
        [](maz::core::ByteWriter& w, const Name& c, const EntityRemap&) { w.writeStringId(c.id); },
        [](maz::core::ByteReader& r, Name& c, const EntityRemap&) {
            maz::core::StringId id; r.readStringId(id); c.id = id;
        });
    s.registerComponent<Tag>(
        "Tag"_sid,
        [](maz::core::ByteWriter& w, const Tag& c, const EntityRemap&) { w.writeStringId(c.id); },
        [](maz::core::ByteReader& r, Tag& c, const EntityRemap&) {
            maz::core::StringId id; r.readStringId(id); c.id = id;
        });
    s.registerComponent<LocalTransform>(
        "LocalTransform"_sid,
        [](maz::core::ByteWriter& w, const LocalTransform& c, const EntityRemap&) {
            for (int col = 0; col < 3; ++col) {
                for (int row = 0; row < 3; ++row) { w.writeFloat(c.value.basis[col][row]); }
            }
            w.writeFloat(c.value.origin.x);
            w.writeFloat(c.value.origin.y);
            w.writeFloat(c.value.origin.z);
        },
        [](maz::core::ByteReader& r, LocalTransform& c, const EntityRemap&) {
            for (int col = 0; col < 3; ++col) {
                for (int row = 0; row < 3; ++row) { r.readFloat(c.value.basis[col][row]); }
            }
            r.readFloat(c.value.origin.x);
            r.readFloat(c.value.origin.y);
            r.readFloat(c.value.origin.z);
        });
    s.registerComponent<Parent>(
        "Parent"_sid,
        [](maz::core::ByteWriter& w, const Parent& c, const EntityRemap& rm) { w.writeU32(rm.toOrdinal(c.value)); },
        [](maz::core::ByteReader& r, Parent& c, const EntityRemap& rm) {
            std::uint32_t o = EntityRemap::kNull; r.readU32(o); c.value = rm.toEntity(o);
        });
}

} // namespace

int main() {
    // --- ROUND-TRIP DATA COMPONENTS ------------------------------------------
    {
        World w;
        const char* names[3] = { "e0", "e1", "e2" };
        const char* tags[3]  = { "ta", "tb", "tc" };
        for (int i = 0; i < 3; ++i) {
            Entity e = w.create();
            w.add<Name>(e, Name{ maz::core::StringId(names[i]) });
            w.add<Tag>(e, Tag{ maz::core::StringId(tags[i]) });
            LocalTransform lt;  // identity basis
            lt.value.origin = maz::math::vec3(static_cast<float>(i),
                                              static_cast<float>(i + 1),
                                              static_cast<float>(i + 2));
            w.add<LocalTransform>(e, lt);
        }
        SceneSerializer s;
        registerCoreComponents(s);
        std::vector<std::uint8_t> bytes = s.save(w);

        World w2;
        bool ok = s.load(bytes, w2);
        check(ok, "load of data-component scene succeeds");
        check(w2.componentCount<Name>() == 3, "3 Name components restored");
        for (int i = 0; i < 3; ++i) {
            Entity e = findByName(w2, maz::core::StringId(names[i]));
            check(!e.isNull(), "entity found by name after load");
            check(w2.get<Tag>(e)->id == maz::core::StringId(tags[i]), "Tag round-tripped");
            check(w2.get<LocalTransform>(e)->value.origin.x == static_cast<float>(i), "origin.x round-tripped");
            check(w2.get<LocalTransform>(e)->value.origin.y == static_cast<float>(i + 1), "origin.y round-tripped");
            check(w2.get<LocalTransform>(e)->value.origin.z == static_cast<float>(i + 2), "origin.z round-tripped");
            check(w2.get<LocalTransform>(e)->value.basis[0][0] == 1.0f, "identity basis round-tripped");
        }
    }

    // --- PARENT HIERARCHY ROUND-TRIPS ----------------------------------------
    {
        World w;
        // Make the SAVE world non-trivial: create+destroy throwaways so the saved
        // entities' indices/generations are NOT a clean 0,1,2 @ generation 1. Destroy
        // pushes an index onto the free-list and bumps its generation; the subsequent
        // create() pops the most-recent free index, so P/A land on recycled (gapped)
        // indices at generation 2.
        Entity g0 = w.create();
        Entity g1 = w.create();
        w.destroy(g0);
        w.destroy(g1);  // free-list now [0,1] at bumped generations
        Entity p = w.create(); w.add<Name>(p, Name{ "parent"_sid });   // recycles a gapped index
        Entity a = w.create(); w.add<Name>(a, Name{ "childA"_sid }); w.add<Parent>(a, Parent{ p });
        Entity b = w.create(); w.add<Name>(b, Name{ "childB"_sid }); w.add<Parent>(b, Parent{ p });

        SceneSerializer s;
        registerCoreComponents(s);
        std::vector<std::uint8_t> bytes = s.save(w);

        World w2;
        // Pre-seed w2 so its create() hands back DIFFERENT indices/generations than the
        // save world. Without this, both worlds create entities in the same order and
        // the loaded handles COINCIDE with the saved ones — EntityRemap is then only
        // ever exercised as an IDENTITY map, and a broken impl that serialized raw
        // Entity bits with NO remap would still pass. The create-and-destroy cycles bump
        // the free-list/generations; the pad consumes a recycled index so load() begins
        // from a diverged allocator state.
        for (int i = 0; i < 3; ++i) { Entity d = w2.create(); w2.destroy(d); }
        Entity pad = w2.create();  // consumes the recycled index
        (void)pad;

        bool ok = s.load(bytes, w2);
        check(ok, "load of parent hierarchy succeeds");
        Entity p2 = findByName(w2, "parent"_sid);
        Entity a2 = findByName(w2, "childA"_sid);
        Entity b2 = findByName(w2, "childB"_sid);
        check(!p2.isNull() && !a2.isNull() && !b2.isNull(), "parent + children found after load");
        check(parentOf(w2, a2) == p2, "childA parent remaps to parent");
        check(parentOf(w2, b2) == p2, "childB parent remaps to parent");
        std::vector<Entity> kids = childrenOf(w2, p2);
        check(kids.size() == 2, "parent has 2 children after load");
        check(contains(kids, a2) && contains(kids, b2), "both children point at parent");
        // Divergence guard — the assertion that makes this test meaningful. The loaded
        // parent handle MUST differ from the saved one, proving the ordinal remap (not
        // raw Entity bits) is what round-trips the Parent reference. This fails if
        // someone regresses to serializing raw Entity index/generation with no remap.
        check(p2.index != p.index || p2.generation != p.generation,
              "loaded parent has a DIFFERENT handle than saved -> remap (not raw bits) is what round-trips");
    }

    // --- UNKNOWN-TAG FORWARD-COMPAT SKIP -------------------------------------
    {
        World w;
        Entity e = w.create();
        w.add<Name>(e, Name{ "e0"_sid });
        w.add<Tag>(e, Tag{ "ta"_sid });
        LocalTransform lt; lt.value.origin = maz::math::vec3(7.0f, 8.0f, 9.0f);
        w.add<LocalTransform>(e, lt);
        // two more so the count matters
        Entity e1 = w.create(); w.add<Name>(e1, Name{ "e1"_sid }); w.add<Tag>(e1, Tag{ "tb"_sid });
        Entity e2 = w.create(); w.add<Name>(e2, Name{ "e2"_sid }); w.add<Tag>(e2, Tag{ "tc"_sid });

        SceneSerializer full;
        registerCoreComponents(full);
        std::vector<std::uint8_t> bytes = full.save(w);

        // A loader that only knows Name; Tag/LocalTransform bodies must be skipped.
        SceneSerializer nameOnly;
        nameOnly.registerComponent<Name>(
            "Name"_sid,
            [](maz::core::ByteWriter& bw, const Name& c, const EntityRemap&) { bw.writeStringId(c.id); },
            [](maz::core::ByteReader& br, Name& c, const EntityRemap&) {
                maz::core::StringId id; br.readStringId(id); c.id = id;
            });

        World w3;
        bool ok = nameOnly.load(bytes, w3);
        check(ok, "load with unknown tags skipped succeeds");
        check(w3.componentCount<Name>() == 3, "3 entities restored despite skipped tags");
        Entity r0 = findByName(w3, "e0"_sid);
        check(!r0.isNull(), "e0 name restored under partial loader");
        check(w3.get<Tag>(r0) == nullptr, "Tag body was skipped (not present)");
        check(w3.get<LocalTransform>(r0) == nullptr, "LocalTransform body was skipped (not present)");
    }

    // --- CORRUPT / TRUNCATED BUFFER ------------------------------------------
    {
        World w;
        Entity e = w.create();
        w.add<Name>(e, Name{ "e0"_sid });
        w.add<Tag>(e, Tag{ "ta"_sid });
        LocalTransform lt; lt.value.origin = maz::math::vec3(1.0f, 2.0f, 3.0f);
        w.add<LocalTransform>(e, lt);
        SceneSerializer s;
        registerCoreComponents(s);
        std::vector<std::uint8_t> good = s.save(w);

        // Truncate the tail (mid-body).
        std::vector<std::uint8_t> truncTail(good.begin(), good.end() - 5);
        World w4;
        check(s.load(truncTail, w4) == false, "tail-truncated buffer fails to load");

        // Truncate to just past the header (into the first entity's body).
        std::vector<std::uint8_t> truncHead(good.begin(), good.begin() + 12);
        World w4b;
        check(s.load(truncHead, w4b) == false, "header-only truncation fails to load");

        // Corrupt the magic.
        std::vector<std::uint8_t> badMagic = good;
        badMagic[0] = static_cast<std::uint8_t>(badMagic[0] ^ 0xFFu);
        World w5;
        check(s.load(badMagic, w5) == false, "bad magic fails to load");

        // Empty buffer.
        std::vector<std::uint8_t> empty;
        World w5b;
        check(s.load(empty, w5b) == false, "empty buffer fails to load");

        // Corrupt the entity-count u32 to 0xFFFFFFFF. Header layout is magic u32
        // (@0..3), version u16 (@4..5), count u32 (@6..9) little-endian, so the count
        // starts at byte 6. The load guard `if (n > r.remaining()) return false;` must
        // reject this absurd count BEFORE it tries to create n entities (huge-alloc DoS
        // guard) — without the fix this case would never exercise that branch.
        std::vector<std::uint8_t> bad = good;
        bad[6] = bad[7] = bad[8] = bad[9] = 0xFFu;
        World wbad;
        check(s.load(bad, wbad) == false, "corrupt huge entity count rejected (n > remaining guard)");
    }

    // --- EMPTY WORLD ----------------------------------------------------------
    {
        World empty;
        SceneSerializer s;
        registerCoreComponents(s);
        std::vector<std::uint8_t> bytes = s.save(empty);
        World w6;
        check(s.load(bytes, w6) == true, "empty-world round-trip succeeds");
        check(w6.componentCount<Name>() == 0, "empty world restores no components");
    }

    // --- DETERMINISM ----------------------------------------------------------
    {
        World w;
        for (int i = 0; i < 3; ++i) {
            Entity e = w.create();
            w.add<Name>(e, Name{ maz::core::StringId(i == 0 ? "a" : (i == 1 ? "b" : "c")) });
            w.add<Tag>(e, Tag{ "t"_sid });
        }
        SceneSerializer s;
        registerCoreComponents(s);
        std::vector<std::uint8_t> b1 = s.save(w);
        std::vector<std::uint8_t> b2 = s.save(w);
        check(b1 == b2, "save is byte-for-byte deterministic");
    }

    // --- VERSION / MAGIC MISMATCH --------------------------------------------
    {
        World w;
        Entity e = w.create(); w.add<Name>(e, Name{ "e0"_sid });
        SceneSerializer s;
        registerCoreComponents(s);
        std::vector<std::uint8_t> good = s.save(w);
        // Overwrite the version u16 at offset 4..5 with 0xFFFF.
        std::vector<std::uint8_t> badVer = good;
        badVer[4] = 0xFFu;
        badVer[5] = 0xFFu;
        World w7;
        check(s.load(badVer, w7) == false, "version mismatch fails to load");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
