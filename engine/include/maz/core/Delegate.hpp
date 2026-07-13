#pragma once

#include "maz/core/Assert.hpp"

namespace maz::core {

// A single-target, ZERO-HEAP "fast delegate" — Godot's Callable analog, and the
// single-target sibling of EventBus (the multicast, heap-based signal bus in
// EventBus.hpp). EventBus stores many std::function callbacks (heap) keyed by
// StringId and fans a signal out to every subscriber; a Delegate points at
// exactly ONE target and stores just TWO raw pointers (a void* m_instance plus a
// stub function pointer) — it allocates nothing. Free functions and member
// functions are bound at COMPILE TIME via C++20 `auto` non-type template
// parameters, so there is no std::function indirection and no allocation.
//
// NON-OWNING: the bound instance (and any bound lambda) must OUTLIVE the
// delegate — a Delegate never keeps its target alive. NOT thread-safe.
//
// Args are taken BY VALUE: the stub is a fixed function-pointer signature, so
// perfect-forwarding through it isn't possible; by-value is the conventional
// fast-delegate choice.
template <class Signature>
class Delegate;  // primary left undefined: only Delegate<R(Args...)> compiles
                 // (e.g. Delegate<int> yields a clear incomplete-type error)

template <class R, class... Args>
class Delegate<R(Args...)> {
  public:
    using Stub = R (*)(void*, Args...);  // trampoline; first param is the type-erased instance

    Delegate() = default;  // unbound

    // Bind a free (or static) function. Fn is the function pointer (auto NTTP).
    // The captureless lambda ignores the instance slot and decays to a Stub.
    template <auto Fn>
    static Delegate fromFunction() {
        Delegate d;
        d.m_instance = nullptr;
        d.m_stub = [](void*, Args... args) -> R { return Fn(args...); };
        return d;
    }

    // Bind a NON-const member on a live instance. C is deduced from obj; the
    // C* -> void* conversion is standard and warning-clean.
    template <auto Method, class C>
    static Delegate fromMethod(C* obj) {
        Delegate d;
        d.m_instance = obj;
        d.m_stub = [](void* inst, Args... args) -> R { return (static_cast<C*>(inst)->*Method)(args...); };
        return d;
    }

    // Bind a CONST member on a live (possibly const) instance. The const C* is
    // stored in the void* member by const_cast'ing away const AT BIND, then
    // re-adding const in the stub. Sound: the bound method is const, so the
    // pointer is never written through — no function-pointer casts are involved
    // (the const_cast is on the DATA pointer only).
    template <auto Method, class C>
    static Delegate fromConstMethod(const C* obj) {
        Delegate d;
        d.m_instance = const_cast<void*>(static_cast<const void*>(obj));
        d.m_stub = [](void* inst, Args... args) -> R { return (static_cast<const C*>(inst)->*Method)(args...); };
        return d;
    }

    // NOTE: a lambda/functor can be bound via fromMethod<&L::operator(), L>(&lam)
    // (mutable operator()) or fromConstMethod<&L::operator(), L>(&lam) (const
    // operator()) — but the lambda must OUTLIVE the delegate (non-owning). No
    // owning wrapper is provided.

    R operator()(Args... args) const {
        MAZ_ASSERT(m_stub != nullptr, "calling an unbound Delegate");
        return m_stub(m_instance, args...);
    }

    bool isBound() const { return m_stub != nullptr; }
    explicit operator bool() const { return isBound(); }

    void reset() { m_instance = nullptr; m_stub = nullptr; }

    // Equal iff same instance via same stub. Distinct fromFunction<Fn> /
    // fromMethod<Method,C> instantiations are distinct captureless-lambda types
    // => distinct stub pointers; two calls to the SAME instantiation yield the
    // SAME stub (compare equal) — intended.
    bool operator==(const Delegate& o) const { return m_instance == o.m_instance && m_stub == o.m_stub; }
    bool operator!=(const Delegate& o) const { return !(*this == o); }

  private:
    void* m_instance = nullptr;  // bound object (nullptr for a free function)
    Stub  m_stub     = nullptr;  // trampoline; nullptr == unbound
    // sizeof == 2 pointers, zero heap. NOT static_asserted here (fn-ptr vs
    // data-ptr widths may differ on exotic ABIs); size is checked (<=) in the
    // unit test.
};

} // namespace maz::core
