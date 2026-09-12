#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

// maz::core::Checkpoints — game-state save slots plus a rolling rewind buffer. This is the "save
// the whole world and put it back" primitive: the game hands over an opaque byte blob of its
// serialized state (from io::Serialize, io::SceneSerializer, its script fields, whatever) and
// Checkpoints stores and returns it. Two complementary modes:
//
//   1. NAMED SLOTS — save("chapter2", bytes) / load("chapter2"). Classic manual + auto checkpoints
//      and save files. serialize()/loadFile() round-trip ALL named slots to one versioned blob you
//      write to disk, so a whole save file is one call.
//   2. REWIND RING — a fixed-capacity ring of recent snapshots keyed by frame.
//   autosave(frame,bytes)
//      pushes one and evicts the oldest past capacity; rewind(k) returns the k-th newest and
//      rewindToFrame(f) the newest at-or-before frame f. That's the backbone of rewind mechanics
//      (Braid / Prince of Persia), rollback netcode, and sandbox "undo" — something Godot has no
//      built-in equivalent for, so this is engine-ahead, not just parity.
//
// State is opaque bytes, so Checkpoints is serialization-scheme-agnostic and unit-tests without a
// game. Header-only, no GPU, no threads, no I/O (the caller owns the file). Little-endian on-disk.
namespace maz::core {

struct Snapshot {
    std::string name;          // slot name ("" for rewind-ring entries)
    uint64_t frame = 0;        // fixed-step frame the snapshot was taken on
    std::vector<uint8_t> data; // the game's serialized state
};

class Checkpoints {
  public:
    // ---- named slots ----
    // Save (or overwrite) a named slot with the game's serialized state.
    void save(const std::string& name, std::vector<uint8_t> state, uint64_t frame = 0) {
        Snapshot s;
        s.name = name;
        s.frame = frame;
        s.data = std::move(state);
        m_slots[name] = std::move(s);
    }
    // Load a named slot, or nullptr if it doesn't exist. Pointer valid until that slot is
    // overwritten/erased or the file is reloaded.
    const Snapshot* load(const std::string& name) const {
        auto it = m_slots.find(name);
        return it == m_slots.end() ? nullptr : &it->second;
    }
    bool has(const std::string& name) const { return m_slots.count(name) != 0; }
    bool erase(const std::string& name) { return m_slots.erase(name) != 0; }
    size_t slotCount() const { return m_slots.size(); }
    std::vector<std::string> names() const {
        std::vector<std::string> out;
        out.reserve(m_slots.size());
        for (const auto& [k, v] : m_slots) {
            out.push_back(k);
        }
        return out;
    }

    // ---- rolling rewind ring ----
    // Set how many recent snapshots the rewind ring keeps (default 0 = disabled). Shrinking drops
    // the oldest entries immediately.
    void setRewindCapacity(size_t n) {
        m_rewindCap = n;
        trimRewind();
    }
    size_t rewindCapacity() const { return m_rewindCap; }

    // Push a snapshot onto the rewind ring; the oldest is evicted once capacity is exceeded. No-op
    // when capacity is 0.
    void autosave(uint64_t frame, std::vector<uint8_t> state) {
        if (m_rewindCap == 0) {
            return;
        }
        Snapshot s;
        s.frame = frame;
        s.data = std::move(state);
        m_rewind.push_back(std::move(s));
        trimRewind();
    }
    size_t rewindCount() const { return m_rewind.size(); }

    // The k-th newest ring snapshot (0 = newest), or nullptr if out of range. rewind(1) is "one
    // step back", the common rewind/rollback move.
    const Snapshot* rewind(size_t stepsBack = 0) const {
        if (stepsBack >= m_rewind.size()) {
            return nullptr;
        }
        return &m_rewind[m_rewind.size() - 1 - stepsBack];
    }

    // The newest ring snapshot whose frame is <= `frame` (restore to a point in time), or nullptr
    // if every snapshot is newer than `frame` (rewound too far — it fell off the ring).
    const Snapshot* rewindToFrame(uint64_t frame) const {
        for (size_t i = m_rewind.size(); i-- > 0;) {
            if (m_rewind[i].frame <= frame) {
                return &m_rewind[i];
            }
        }
        return nullptr;
    }

    // Total bytes held by the rewind ring (for a memory budget).
    size_t rewindByteSize() const {
        size_t n = 0;
        for (const Snapshot& s : m_rewind) {
            n += s.data.size();
        }
        return n;
    }

    void clearRewind() { m_rewind.clear(); }
    void clear() {
        m_slots.clear();
        m_rewind.clear();
    }

    // ---- whole save-file serialize/load (named slots only; the rewind ring is runtime state) ----
    // Layout: "MZSV" | u32 version | u32 slotCount |
    //         slotCount x [ u32 nameLen | name | u64 frame | u32 dataLen | data ]
    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> out;
        putTag(out, 'M', 'Z', 'S', 'V');
        putU32(out, kVersion);
        putU32(out, static_cast<uint32_t>(m_slots.size()));
        for (const auto& [name, snap] : m_slots) {
            putU32(out, static_cast<uint32_t>(name.size()));
            out.insert(out.end(), name.begin(), name.end());
            putU64(out, snap.frame);
            putU32(out, static_cast<uint32_t>(snap.data.size()));
            out.insert(out.end(), snap.data.begin(), snap.data.end());
        }
        return out;
    }

    // Restore all named slots from a serialize() blob (replacing current slots). Rejects a bad
    // tag/version or truncated data; leaves slots empty on failure. The rewind ring is untouched.
    bool loadFile(const std::vector<uint8_t>& bytes) {
        std::unordered_map<std::string, Snapshot> loaded;
        size_t off = 0;
        if (bytes.size() < 12) {
            return false;
        }
        if (bytes[0] != 'M' || bytes[1] != 'Z' || bytes[2] != 'S' || bytes[3] != 'V') {
            return false;
        }
        off = 4;
        const uint32_t version = getU32(bytes, off);
        const uint32_t count = getU32(bytes, off);
        if (version != kVersion) {
            return false;
        }
        for (uint32_t i = 0; i < count; ++i) {
            if (off + 4 > bytes.size()) {
                return false;
            }
            const uint32_t nameLen = getU32(bytes, off);
            if (off + nameLen + 8 + 4 > bytes.size()) {
                return false;
            }
            std::string name(reinterpret_cast<const char*>(bytes.data() + off), nameLen);
            off += nameLen;
            const uint64_t frame = getU64(bytes, off);
            const uint32_t dataLen = getU32(bytes, off);
            if (off + dataLen > bytes.size()) {
                return false;
            }
            Snapshot s;
            s.name = name;
            s.frame = frame;
            s.data.assign(bytes.begin() + static_cast<std::ptrdiff_t>(off),
                          bytes.begin() + static_cast<std::ptrdiff_t>(off + dataLen));
            off += dataLen;
            loaded[name] = std::move(s);
        }
        m_slots = std::move(loaded);
        return true;
    }

  private:
    static constexpr uint32_t kVersion = 1;

    void trimRewind() {
        while (m_rewind.size() > m_rewindCap) {
            m_rewind.pop_front();
        }
    }

    static void putTag(std::vector<uint8_t>& o, char a, char b, char c, char d) {
        o.push_back(static_cast<uint8_t>(a));
        o.push_back(static_cast<uint8_t>(b));
        o.push_back(static_cast<uint8_t>(c));
        o.push_back(static_cast<uint8_t>(d));
    }
    static void putU32(std::vector<uint8_t>& o, uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            o.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
        }
    }
    static void putU64(std::vector<uint8_t>& o, uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            o.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
        }
    }
    static uint32_t getU32(const std::vector<uint8_t>& b, size_t& off) {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            v |= static_cast<uint32_t>(b[off + static_cast<size_t>(i)]) << (8 * i);
        }
        off += 4;
        return v;
    }
    static uint64_t getU64(const std::vector<uint8_t>& b, size_t& off) {
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
            v |= static_cast<uint64_t>(b[off + static_cast<size_t>(i)]) << (8 * i);
        }
        off += 8;
        return v;
    }

    std::unordered_map<std::string, Snapshot> m_slots;
    std::deque<Snapshot> m_rewind;
    size_t m_rewindCap = 0;
};

} // namespace maz::core
