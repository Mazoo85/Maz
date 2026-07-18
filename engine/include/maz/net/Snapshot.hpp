#pragma once

#include "maz/net/BitStream.hpp"

#include <cstdint>
#include <vector>

// maz::net snapshot / delta replication — how game state crosses the wire efficiently. A snapshot
// is the current value of a fixed SCHEMA of fields (each with a known bit width — a health that's
// 0..100 is 7 bits, a tile id 0..1023 is 10). Sending a full snapshot every tick is wasteful, so
// DELTA replication sends, against a baseline the peer already has, only a changed-field bitmask
// plus the values that actually changed. Most fields don't change most ticks, so a delta is a few
// bits instead of a full struct — the core bandwidth win of networked games (Godot's
// MultiplayerSynchronizer does the same idea; the per-field bit widths make Maz's tighter).
// Pure, built on net::BitStream, unit-tested. Values are carried as uint32 (quantize floats before,
// e.g. via a fixed-point scale) so the transport stays integer-exact.
namespace maz::net {

struct FieldSpec {
    uint8_t bits = 32; // wire width of this field (1..32)
};

// Write every field's value at its schema width (the full, baseline-free snapshot — used for the
// first packet to a peer, or a keyframe).
inline void writeSnapshotFull(BitWriter& w, const std::vector<FieldSpec>& schema,
                              const std::vector<uint32_t>& values) {
    const size_t n = schema.size() < values.size() ? schema.size() : values.size();
    for (size_t i = 0; i < n; ++i) {
        w.writeBits(values[i], schema[i].bits);
    }
}

inline std::vector<uint32_t> readSnapshotFull(BitReader& r, const std::vector<FieldSpec>& schema) {
    std::vector<uint32_t> out;
    out.reserve(schema.size());
    for (const FieldSpec& f : schema) {
        out.push_back(r.readBits(f.bits));
    }
    return out;
}

// Number of fields that differ between base and cur (both must match the schema length).
inline size_t countChanged(const std::vector<uint32_t>& base, const std::vector<uint32_t>& cur) {
    size_t c = 0;
    const size_t n = base.size() < cur.size() ? base.size() : cur.size();
    for (size_t i = 0; i < n; ++i) {
        if (base[i] != cur[i]) {
            ++c;
        }
    }
    return c;
}

// Write a delta of `cur` against `base`: one changed-bit per field, then each changed field's value
// at its width. `base`, `cur`, and `schema` must be the same length.
inline void writeSnapshotDelta(BitWriter& w, const std::vector<FieldSpec>& schema,
                               const std::vector<uint32_t>& base,
                               const std::vector<uint32_t>& cur) {
    const size_t n = schema.size();
    for (size_t i = 0; i < n; ++i) {
        w.writeBool(base[i] != cur[i]);
    }
    for (size_t i = 0; i < n; ++i) {
        if (base[i] != cur[i]) {
            w.writeBits(cur[i], schema[i].bits);
        }
    }
}

// Reconstruct `cur` from `base` + the delta the writer produced. Unchanged fields copy `base`.
inline std::vector<uint32_t> readSnapshotDelta(BitReader& r, const std::vector<FieldSpec>& schema,
                                               const std::vector<uint32_t>& base) {
    const size_t n = schema.size();
    std::vector<bool> changed(n, false);
    for (size_t i = 0; i < n; ++i) {
        changed[i] = r.readBool();
    }
    std::vector<uint32_t> out(base.begin(), base.begin() + static_cast<long>(n <= base.size() ? n : base.size()));
    out.resize(n, 0);
    for (size_t i = 0; i < n; ++i) {
        if (changed[i]) {
            out[i] = r.readBits(schema[i].bits);
        }
    }
    return out;
}

} // namespace maz::net
