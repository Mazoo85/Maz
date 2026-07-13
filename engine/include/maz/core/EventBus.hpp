#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <map>
#include <vector>
#include <utility>  // std::move

#include "maz/core/StringId.hpp"

namespace maz::core {

// A named-signal event bus — the Godot signal/connect/emit analog. Subscribers
// connect a std::function callback to a StringId channel; emit(channel, event)
// invokes every live callback on that channel; subscribe() returns a Connection
// token; disconnect() is generationally safe (a stale, double, or null
// disconnect is a harmless no-op, exactly like Pool's destroy).
//
// Re-entrancy: a callback may subscribe or disconnect during an emit — emit
// snapshots the slot count, rechecks liveness each iteration, and copies the
// std::function before calling it. New subscriptions made during an emit fire
// only on the NEXT emit; a slot disconnected earlier in the same emit loop is
// skipped. Channels are keyed by std::map<StringId, ...> (StringId has
// operator< but no std::hash, and std::map nodes are address-stable across an
// insert — load-bearing for holding a Channel& across a re-entrant subscribe).
//
// NOT thread-safe. Lifetime caveat: anything a callback captures by reference
// must outlive the subscription. clear() must NOT be called from inside an
// in-flight emit callback.

// An opaque, generational reference to a subscription. Cheap to copy, compare,
// and store. Non-templated (like Handle): it carries no Event, so a type-tagged
// variant (Connection<Event>) that prevents mixing tokens from buses of
// different Event types is a possible future refinement — not built here.
struct Connection {
    StringId channel{};
    uint32_t index = 0;
    uint32_t generation = 0;  // 0 == null/never-issued sentinel; live generations >= 1

    constexpr bool operator==(const Connection& o) const {
        return channel == o.channel && index == o.index && generation == o.generation;
    }
    constexpr bool operator!=(const Connection& o) const { return !(*this == o); }

    constexpr bool isNull() const { return generation == 0; }
    static constexpr Connection null() { return Connection{}; }
};

template <class Event>
class EventBus {
public:
    using Callback = std::function<void(const Event&)>;

    // Connect cb to channel and return a Connection that can later disconnect it.
    Connection subscribe(StringId channel, Callback cb) {
        Channel& ch = m_channels[channel];
        // Reuse a free slot only when NOT mid-emit: a reused index < the emit's
        // snapshot n would misfire in the in-flight emit (a fresh append past n
        // is correctly skipped this emit; a recycled low index would not be).
        if (ch.emitDepth == 0 && !ch.freeIndices.empty()) {
            uint32_t i = ch.freeIndices.back();
            ch.freeIndices.pop_back();
            // Do NOT touch generation — it already holds the post-disconnect
            // bumped value (Pool rule: subscribe never bumps the generation).
            ch.slots[i].fn = std::move(cb);
            ch.slots[i].alive = true;
            ++ch.liveCount;
            return Connection{channel, i, ch.slots[i].generation};
        }
        uint32_t i = static_cast<uint32_t>(ch.slots.size());
        ch.slots.push_back(Slot{ std::move(cb), 1u, true });  // fresh slots start at generation 1
        ++ch.liveCount;
        return Connection{channel, i, 1u};
    }

    // Tear down the subscription named by c. Returns false (a harmless no-op) for
    // a null, stale, double, or out-of-range Connection.
    bool disconnect(const Connection& c) {
        if (c.generation == 0) {
            return false;  // null token
        }
        auto it = m_channels.find(c.channel);
        if (it == m_channels.end()) {
            return false;
        }
        Channel& ch = it->second;
        if (static_cast<std::size_t>(c.index) >= ch.slots.size()) {
            return false;  // out of range (cast widens for -Wconversion)
        }
        Slot& s = ch.slots[c.index];
        if (!s.alive || s.generation != c.generation) {
            return false;  // stale / double disconnect — harmless no-op
        }
        // Tombstone the slot (never erase/renumber) so outstanding Connections
        // stay valid and this stays safe mid-emit: emit only skips !alive slots.
        s.alive = false;
        ++s.generation;
        if (s.generation == 0) {
            s.generation = 1;  // skip 0 on wraparound — it is the null sentinel
        }
        s.fn = nullptr;  // release any state the callback captured
        ch.freeIndices.push_back(c.index);
        --ch.liveCount;
        return true;
    }

    // Invoke every live callback on channel with event. NON-const: emit mutates
    // emitDepth (and a re-entrant subscribe may append slots / insert channels).
    void emit(StringId channel, const Event& event) {
        auto it = m_channels.find(channel);
        if (it == m_channels.end()) {
            return;
        }
        // std::map node is address-stable across a re-entrant subscribe(): a
        // re-entrant subscribe only push_backs into ch.slots (the vector may
        // realloc) and inserts NEW map nodes (existing nodes never move), so
        // this Channel& stays valid for the whole loop. We must not, however,
        // hold a Slot&/Callback& across fn(event).
        Channel& ch = it->second;
        ++ch.emitDepth;
        std::size_t n = ch.slots.size();  // SNAPSHOT: slots appended past n are not visited this emit
        for (std::size_t i = 0; i < n; ++i) {
            if (!ch.slots[i].alive) {
                continue;  // recheck each iteration: skip slots disconnected earlier in THIS loop
            }
            Callback fn = ch.slots[i].fn;  // COPY before calling: fn may push_back and REALLOCATE ch.slots, dangling a Slot&
            fn(event);
        }
        --ch.emitDepth;
    }

    // Live subscriber count on channel (0 for an unknown channel).
    std::size_t size(StringId channel) const {
        auto it = m_channels.find(channel);
        return it == m_channels.end() ? std::size_t{0} : it->second.liveCount;
    }

    std::size_t channelCount() const { return m_channels.size(); }

    // Drop all channels and subscriptions; outstanding Connections become no-ops.
    // Must NOT be called from inside an in-flight emit callback.
    void clear() { m_channels.clear(); }

private:
    struct Slot {
        Callback fn{};
        uint32_t generation = 1;  // fresh slots start at generation 1; 0 is reserved as null
        bool alive = false;
    };

    struct Channel {
        std::vector<Slot> slots;
        std::vector<uint32_t> freeIndices;  // free-list of recyclable slot indices
        std::size_t liveCount = 0;
        int emitDepth = 0;  // >0 while an emit on this channel is in flight
    };

    std::map<StringId, Channel> m_channels;
};

} // namespace maz::core
