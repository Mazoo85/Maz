#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <tuple>
#include <utility>
#include <vector>

namespace maz::core {

// Per-object named signals — Godot's `signal` / `connect` / `emit`. The engine already has a global,
// by-TYPE publish/subscribe bus (`core::EventBus`), but Godot's signals are a different, more granular
// pattern: each OBJECT owns its own named channels ("this button's `pressed`", "this health's `changed`")
// that carry typed arguments, and other objects connect callbacks to a SPECIFIC emitter's signal. On top
// of plain connect/emit it adds the two flavours Godot leans on constantly — ONE-SHOT connections (fire
// once, then auto-disconnect) and DEFERRED connections (the call is queued and run later at a flush point
// instead of re-entrantly mid-emit). Header-only, type-safe, no allocation beyond the connection list;
// unit-tests headlessly.

using ConnectionId = std::uint64_t;
inline constexpr ConnectionId kInvalidConnection = 0;

template <typename... Args>
class Signal {
public:
    using Handler = std::function<void(Args...)>;

    // Connect a handler. Returns an id you can later disconnect() / isConnected().
    ConnectionId connect(Handler fn) { return add(std::move(fn), false, false); }
    // Fires exactly once, then auto-disconnects.
    ConnectionId connectOnce(Handler fn) { return add(std::move(fn), true, false); }
    // The call is queued on emit and runs at the next flushDeferred() instead of immediately.
    ConnectionId connectDeferred(Handler fn) { return add(std::move(fn), false, true); }
    // Deferred AND one-shot.
    ConnectionId connectDeferredOnce(Handler fn) { return add(std::move(fn), true, true); }

    // Disconnect a specific connection. Returns true if it was connected.
    bool disconnect(ConnectionId id) {
        for (Slot& s : m_slots) {
            if (s.id == id && s.alive) {
                s.alive = false;
                return true;
            }
        }
        return false;
    }
    void disconnectAll() { m_slots.clear(); }

    bool isConnected(ConnectionId id) const {
        for (const Slot& s : m_slots) {
            if (s.id == id && s.alive) {
                return true;
            }
        }
        return false;
    }
    std::size_t connectionCount() const {
        std::size_t n = 0;
        for (const Slot& s : m_slots) {
            n += s.alive ? 1 : 0;
        }
        return n;
    }

    // Emit the signal: immediate handlers run now (in connection order); deferred handlers are queued with
    // a copy of the arguments to run at flushDeferred(); one-shot handlers auto-disconnect. A snapshot is
    // taken first, so a handler may safely connect/disconnect (including itself) during dispatch.
    void emit(Args... args) {
        struct Fire {
            ConnectionId id;
            Handler fn;
            bool oneShot;
            bool deferred;
        };
        std::vector<Fire> snap;
        snap.reserve(m_slots.size());
        for (const Slot& s : m_slots) {
            if (s.alive) {
                snap.push_back(Fire{s.id, s.fn, s.oneShot, s.deferred});
            }
        }
        for (Fire& f : snap) {
            if (f.deferred) {
                Handler fn = f.fn;
                auto boundArgs = std::make_tuple(args...);
                m_deferred.push_back([fn, boundArgs]() { std::apply(fn, boundArgs); });
            } else {
                f.fn(args...);
            }
            if (f.oneShot) {
                disconnect(f.id);
            }
        }
        compact();
    }

    // Run every queued deferred invocation (in emit order) and clear the queue.
    void flushDeferred() {
        std::vector<std::function<void()>> q;
        q.swap(m_deferred);
        for (auto& f : q) {
            f();
        }
    }
    std::size_t pendingDeferred() const { return m_deferred.size(); }

private:
    struct Slot {
        ConnectionId id;
        Handler fn;
        bool oneShot;
        bool deferred;
        bool alive;
    };

    ConnectionId add(Handler fn, bool once, bool deferred) {
        const ConnectionId id = ++m_next;
        m_slots.push_back(Slot{id, std::move(fn), once, deferred, true});
        return id;
    }
    void compact() {
        m_slots.erase(std::remove_if(m_slots.begin(), m_slots.end(),
                                     [](const Slot& s) { return !s.alive; }),
                      m_slots.end());
    }

    std::vector<Slot> m_slots;
    std::vector<std::function<void()>> m_deferred;
    ConnectionId m_next = kInvalidConnection;
};

} // namespace maz::core
