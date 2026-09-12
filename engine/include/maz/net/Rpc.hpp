#pragma once

#include "maz/net/BitStream.hpp"

#include <cstdint>
#include <functional>
#include <unordered_map>

// maz::net RPC layer — remote procedure calls, the high-level way gameplay code talks across the
// wire without hand-rolling packet formats. A peer registers named handlers; to "call" a method on
// another peer it writes a compact header (a 16-bit method id + a 2-bit transfer mode) followed by
// the argument bytes, and the receiver dispatches to the matching handler with the reader parked at
// the args. Method NAMES hash to ids (FNV-1a, deterministic across peers/platforms) so strings
// never ride the wire. This is Godot's @rpc / rpc()/rpc_id() model — reliable/unreliable/ordered
// transfer modes included — kept pure and unit-tested; the transport just carries the bytes.
namespace maz::net {

// How the transport should deliver a call. Mirrors Godot's MultiplayerPeer transfer modes; the
// dispatcher records it so the transport layer can route reliable vs unreliable appropriately.
enum class RpcMode : uint8_t {
    Reliable = 0,          // guaranteed, ordered (state changes, chat)
    Unreliable = 1,        // fire-and-forget (frequent, self-correcting updates)
    UnreliableOrdered = 2, // unreliable but drops out-of-order stragglers
};

using RpcMethodId = uint16_t;

// Deterministic name -> id hash (FNV-1a 32-bit folded to 16). Both peers compute the same id for
// the same method name, so only the number crosses the wire.
inline RpcMethodId rpcHash(const char* name) {
    uint32_t h = 2166136261u;
    for (const char* p = name; *p != '\0'; ++p) {
        h ^= static_cast<uint8_t>(*p);
        h *= 16777619u;
    }
    return static_cast<RpcMethodId>((h ^ (h >> 16)) & 0xFFFFu);
}

class RpcDispatcher {
  public:
    // A handler receives a BitReader positioned at the call's arguments and decodes them itself.
    using Handler = std::function<void(BitReader&)>;

    void bind(RpcMethodId id, Handler h) { m_handlers[id] = std::move(h); }
    void bindNamed(const char* name, Handler h) { bind(rpcHash(name), std::move(h)); }
    bool bound(RpcMethodId id) const { return m_handlers.count(id) != 0; }

    // Write a call header. The caller then writes the arguments with the same BitWriter, in the
    // encoding the matching handler expects.
    static void writeHeader(BitWriter& w, RpcMethodId id, RpcMode mode = RpcMode::Reliable) {
        w.writeBits(id, 16);
        w.writeBits(static_cast<uint32_t>(mode), 2);
    }
    static void writeHeaderNamed(BitWriter& w, const char* name, RpcMode mode = RpcMode::Reliable) {
        writeHeader(w, rpcHash(name), mode);
    }

    // Read one call from `r`: pull the header, look up the handler, and invoke it with `r` parked
    // at the arguments. Returns false (and counts it) if the header is truncated or the method is
    // unregistered — the caller can log or drop. On success, lastMethod()/lastMode() reflect it.
    bool dispatch(BitReader& r) {
        const RpcMethodId id = static_cast<RpcMethodId>(r.readBits(16));
        const uint32_t modeBits = r.readBits(2);
        if (!r.ok()) {
            ++m_malformed;
            return false;
        }
        auto it = m_handlers.find(id);
        if (it == m_handlers.end()) {
            ++m_unknown;
            return false;
        }
        m_lastMethod = id;
        m_lastMode = static_cast<RpcMode>(modeBits);
        it->second(r);
        return true;
    }

    std::size_t handlerCount() const { return m_handlers.size(); }
    uint32_t unknownCalls() const { return m_unknown; }
    uint32_t malformedCalls() const { return m_malformed; }
    RpcMethodId lastMethod() const { return m_lastMethod; }
    RpcMode lastMode() const { return m_lastMode; }

  private:
    std::unordered_map<RpcMethodId, Handler> m_handlers;
    uint32_t m_unknown = 0;
    uint32_t m_malformed = 0;
    RpcMethodId m_lastMethod = 0;
    RpcMode m_lastMode = RpcMode::Reliable;
};

} // namespace maz::net
