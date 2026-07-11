#pragma once

#include <cstdint>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace maz::core {

// A type-safe publish/subscribe event bus — the decoupling glue between systems. A gameplay system
// emits an event value (any type) and every subscriber registered for THAT type is invoked, without
// emitter and listener knowing about each other (damage -> audio + particles + score + UI, all
// independent). Subscription returns a token you can later unsubscribe. Dispatch snapshots the
// listener list, so a handler may safely subscribe/unsubscribe or emit further events during a call.
// Header-only; no GPU. Not thread-safe — intended for the single game thread.
class EventBus {
public:
    using Token = uint64_t;
    static constexpr Token kInvalidToken = 0;

    // Register `fn` to receive every event of type T. Returns a token for unsubscribe().
    template <typename T>
    Token subscribe(std::function<void(const T&)> fn) {
        const Token token = ++m_counter;
        auto& list = m_handlers[std::type_index(typeid(T))];
        list.push_back(Handler{token, [fn = std::move(fn)](const void* e) {
                                    fn(*static_cast<const T*>(e));
                                }});
        m_tokenType.emplace(token, std::type_index(typeid(T)));
        return token;
    }

    // Deliver `event` to every current subscriber of type T (in subscription order).
    template <typename T>
    void emit(const T& event) const {
        const auto it = m_handlers.find(std::type_index(typeid(T)));
        if (it == m_handlers.end()) {
            return;
        }
        // Snapshot so a handler can (un)subscribe or emit re-entrantly without invalidating us.
        const std::vector<Handler> snapshot = it->second;
        for (const Handler& h : snapshot) {
            h.fn(&event);
        }
    }

    // Remove a previously-registered subscription. Safe to call with a stale/invalid token.
    void unsubscribe(Token token) {
        const auto tt = m_tokenType.find(token);
        if (tt == m_tokenType.end()) {
            return;
        }
        auto hit = m_handlers.find(tt->second);
        if (hit != m_handlers.end()) {
            auto& list = hit->second;
            for (size_t i = 0; i < list.size(); ++i) {
                if (list[i].token == token) {
                    list.erase(list.begin() + static_cast<std::ptrdiff_t>(i));
                    break;
                }
            }
        }
        m_tokenType.erase(tt);
    }

    // Number of live subscribers for event type T (for tests/telemetry).
    template <typename T>
    size_t subscriberCount() const {
        const auto it = m_handlers.find(std::type_index(typeid(T)));
        return it == m_handlers.end() ? 0 : it->second.size();
    }

    void clear() {
        m_handlers.clear();
        m_tokenType.clear();
    }

private:
    struct Handler {
        Token token;
        std::function<void(const void*)> fn;
    };
    std::unordered_map<std::type_index, std::vector<Handler>> m_handlers;
    std::unordered_map<Token, std::type_index> m_tokenType;
    Token m_counter = 0;
};

} // namespace maz::core
