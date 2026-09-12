#pragma once

#include "maz/net/BitStream.hpp"

#include <cstdint>
#include <functional>
#include <vector>

// maz::net multiplayer spawner — Godot's MultiplayerSpawner: the authority creates and destroys
// networked nodes, and those spawn/despawn events are replicated so every peer instantiates and frees
// matching nodes. This models that as data: the authority assigns a network id to each spawn (with a
// scene-type tag + spawn args), queues spawn/despawn events, and serializes them to a BitStream;
// remotes apply the stream via onSpawn/onDespawn callbacks. It also keeps the live set so a late joiner
// can be sent a full snapshot (writeFull) and rebuild the world in one shot. Pure, built on
// net::BitStream, unit-tested — the transport (who to send bytes to) is the socket layer on top. Pairs
// with net::Synchronizer (M218), which keeps each spawned node's properties in sync thereafter.
namespace maz::net {

struct SpawnRecord {
    uint32_t netId = 0;
    uint32_t sceneType = 0;         // which spawnable scene/prefab to instance
    std::vector<uint32_t> args;     // spawn arguments (quantize floats first, like Snapshot)
};

class MultiplayerSpawner {
  public:
    // --- authority side ---------------------------------------------------------------------------
    // Spawn a node: assigns the next network id, records it live, and queues a spawn event. Returns
    // the new netId.
    uint32_t spawn(uint32_t sceneType, const std::vector<uint32_t>& args = {}) {
        const uint32_t id = m_nextId++;
        SpawnRecord r{id, sceneType, args};
        m_alive.push_back(r);
        m_events.push_back(Event{true, r});
        return id;
    }
    // Despawn a live node. Returns false if the id is not currently alive.
    bool despawn(uint32_t netId) {
        for (std::size_t i = 0; i < m_alive.size(); ++i) {
            if (m_alive[i].netId == netId) {
                m_alive.erase(m_alive.begin() + static_cast<std::ptrdiff_t>(i));
                SpawnRecord r;
                r.netId = netId;
                m_events.push_back(Event{false, r});
                return true;
            }
        }
        return false;
    }

    bool hasPendingEvents() const { return !m_events.empty(); }

    // Serialize the queued spawn/despawn events and clear the queue (call once per network tick).
    void writeEvents(BitWriter& bs) {
        bs.writeUint(static_cast<uint32_t>(m_events.size()), 16);
        for (const Event& e : m_events) {
            bs.writeBool(e.spawn);
            if (e.spawn) {
                writeRecord(bs, e.record);
            } else {
                bs.writeUint(e.record.netId, 32);
            }
        }
        m_events.clear();
    }

    // Serialize the full live set as spawn records — the keyframe a late joiner needs.
    void writeFull(BitWriter& bs) const {
        bs.writeUint(static_cast<uint32_t>(m_alive.size()), 16);
        for (const SpawnRecord& r : m_alive) {
            writeRecord(bs, r);
        }
    }

    // --- remote side ------------------------------------------------------------------------------
    std::function<void(const SpawnRecord&)> onSpawn;
    std::function<void(uint32_t netId)> onDespawn;

    // Apply an event stream: updates the live set and fires callbacks. Returns false on a malformed
    // (truncated) stream.
    bool readEvents(BitReader& bs) {
        const uint32_t count = bs.readUint(16);
        for (uint32_t i = 0; i < count; ++i) {
            const bool spawn = bs.readBool();
            if (!bs.ok()) {
                return false;
            }
            if (spawn) {
                SpawnRecord r;
                if (!readRecord(bs, r)) {
                    return false;
                }
                applySpawn(r);
            } else {
                const uint32_t id = bs.readUint(32);
                if (!bs.ok()) {
                    return false;
                }
                applyDespawn(id);
            }
        }
        return bs.ok();
    }

    // Apply a full snapshot: clears the live set and re-spawns everything in it.
    bool readFull(BitReader& bs) {
        for (const SpawnRecord& r : m_alive) {
            if (onDespawn) {
                onDespawn(r.netId);
            }
        }
        m_alive.clear();
        const uint32_t count = bs.readUint(16);
        for (uint32_t i = 0; i < count; ++i) {
            SpawnRecord r;
            if (!readRecord(bs, r)) {
                return false;
            }
            applySpawn(r);
        }
        return bs.ok();
    }

    // --- queries ----------------------------------------------------------------------------------
    bool isAlive(uint32_t netId) const {
        for (const SpawnRecord& r : m_alive) {
            if (r.netId == netId) {
                return true;
            }
        }
        return false;
    }
    std::size_t aliveCount() const { return m_alive.size(); }
    const std::vector<SpawnRecord>& alive() const { return m_alive; }
    uint32_t peekNextId() const { return m_nextId; }

    // Config: cap args per spawn (guards a malformed stream from allocating unboundedly).
    uint32_t maxArgs = 64;

  private:
    struct Event {
        bool spawn;
        SpawnRecord record;
    };

    void writeRecord(BitWriter& bs, const SpawnRecord& r) const {
        bs.writeUint(r.netId, 32);
        bs.writeUint(r.sceneType, 16);
        bs.writeUint(static_cast<uint32_t>(r.args.size()), 8);
        for (uint32_t a : r.args) {
            bs.writeUint(a, 32);
        }
    }
    bool readRecord(BitReader& bs, SpawnRecord& r) const {
        r.netId = bs.readUint(32);
        r.sceneType = bs.readUint(16);
        const uint32_t n = bs.readUint(8);
        if (!bs.ok() || n > maxArgs) {
            return false;
        }
        r.args.resize(n);
        for (uint32_t i = 0; i < n; ++i) {
            r.args[i] = bs.readUint(32);
        }
        return bs.ok();
    }

    void applySpawn(const SpawnRecord& r) {
        if (!isAlive(r.netId)) {
            m_alive.push_back(r);
            if (onSpawn) {
                onSpawn(r);
            }
        }
    }
    void applyDespawn(uint32_t id) {
        for (std::size_t i = 0; i < m_alive.size(); ++i) {
            if (m_alive[i].netId == id) {
                m_alive.erase(m_alive.begin() + static_cast<std::ptrdiff_t>(i));
                if (onDespawn) {
                    onDespawn(id);
                }
                return;
            }
        }
    }

    uint32_t m_nextId = 1;
    std::vector<SpawnRecord> m_alive;
    std::vector<Event> m_events;
};

} // namespace maz::net
