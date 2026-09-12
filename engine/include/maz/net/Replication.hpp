#pragma once

#include "maz/net/Snapshot.hpp"

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

// maz::net scene replication — the high-level "just keep these object fields in sync" layer, Godot's
// MultiplayerSynchronizer + SceneReplicationConfig. You declare which properties of an object travel
// together (each a get/set pair plus a wire width); a ReplicatedObject can then capture() them into
// a value vector and apply() one back. A Synchronizer holds the per-peer BASELINE (the last values
// that peer has) and turns capture/apply into wire packets: writeFull for the first packet or a
// keyframe, writeDelta for the common case (only changed fields, via the M214 snapshot delta). As
// long as a peer starts from a full and applies deltas in order, sender and receiver baselines stay
// coherent. Pure, built on net::Snapshot/BitStream, unit-tested. Values are uint32 (quantize floats
// first — same convention as Snapshot).
namespace maz::net {

// One synchronized property on an object: how to read it, how to write it, and its wire width.
struct ReplicatedProperty {
    std::function<uint32_t()> get;
    std::function<void(uint32_t)> set;
    uint8_t bits = 32;
};

// A group of properties that replicate together (one networked object/node).
class ReplicatedObject {
  public:
    void add(std::function<uint32_t()> get, std::function<void(uint32_t)> set, uint8_t bits) {
        m_props.push_back({std::move(get), std::move(set), bits});
        m_schema.push_back({bits});
    }

    const std::vector<FieldSpec>& schema() const { return m_schema; }
    std::size_t propertyCount() const { return m_props.size(); }

    // Read every property's current value into a vector (sender side).
    std::vector<uint32_t> capture() const {
        std::vector<uint32_t> v;
        v.reserve(m_props.size());
        for (const ReplicatedProperty& p : m_props) {
            v.push_back(p.get());
        }
        return v;
    }

    // Write a vector of values back into the object's properties (receiver side).
    void apply(const std::vector<uint32_t>& v) {
        const std::size_t n = v.size() < m_props.size() ? v.size() : m_props.size();
        for (std::size_t i = 0; i < n; ++i) {
            m_props[i].set(v[i]);
        }
    }

  private:
    std::vector<ReplicatedProperty> m_props;
    std::vector<FieldSpec> m_schema;
};

// Holds the baseline for one replication stream (one object toward one peer) and (de)serializes it.
class Synchronizer {
  public:
    // --- Sender side ---

    // Write a full snapshot (baseline-free keyframe) and adopt it as the new baseline.
    void writeFull(BitWriter& w, const ReplicatedObject& obj) {
        m_baseline = obj.capture();
        writeSnapshotFull(w, obj.schema(), m_baseline);
    }

    // Write a delta of the object's current values against the baseline, then advance the baseline.
    // Returns how many fields changed (0 => the delta is just the changed-field mask bits).
    std::size_t writeDelta(BitWriter& w, const ReplicatedObject& obj) {
        std::vector<uint32_t> cur = obj.capture();
        ensureBaseline(cur.size());
        const std::size_t changed = countChanged(m_baseline, cur);
        writeSnapshotDelta(w, obj.schema(), m_baseline, cur);
        m_baseline = std::move(cur);
        return changed;
    }

    // --- Receiver side ---

    // Read a full snapshot, apply it to the object, and adopt it as the baseline.
    void readFull(BitReader& r, ReplicatedObject& obj) {
        m_baseline = readSnapshotFull(r, obj.schema());
        obj.apply(m_baseline);
    }

    // Read a delta against the current baseline, apply the reconstructed values, and advance.
    void readDelta(BitReader& r, ReplicatedObject& obj) {
        ensureBaseline(obj.propertyCount());
        m_baseline = readSnapshotDelta(r, obj.schema(), m_baseline);
        obj.apply(m_baseline);
    }

    const std::vector<uint32_t>& baseline() const { return m_baseline; }
    void resetBaseline() { m_baseline.clear(); }

  private:
    void ensureBaseline(std::size_t n) {
        if (m_baseline.size() != n) {
            m_baseline.assign(n, 0u);
        }
    }

    std::vector<uint32_t> m_baseline;
};

} // namespace maz::net
