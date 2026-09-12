#pragma once

#include <cstdint>
#include <functional>
#include <memory>  // std::unique_ptr, std::make_unique
#include <vector>
#include <utility>  // std::move, std::forward

#include "maz/core/Assert.hpp"

// A classic game-AI behavior tree (the Godot gameplay behavior-tree analog, a
// sibling of the iter32 StateMachine). A tree of polymorphic nodes owned via
// unique_ptr and ticked top-down each frame; leaf Actions return
// Status{Success,Failure,Running}. Composites combine children: Sequence (AND —
// all children must succeed, fails fast on the first Failure) and Selector (OR —
// the first success wins, fails only if all children fail). Decorators wrap one
// child: Inverter (swaps Success/Failure), Succeeder (Failure->Success), Repeater
// (repeat N times). Composites remember a Running child and resume there on the
// next tick; call reset() to restart the tree from the top. Build trees with the
// action()/sequence()/selector()/inverter()/succeeder()/repeater() helpers.
// Composes iter7-era std::function, which holds a capturing lambda, a free
// function, or an iter19 maz::core::Delegate. NOT thread-safe. A blackboard, a
// parallel node, and repeat-until-fail/retry decorators are future refinements
// (not built here).

namespace maz::ai {

enum class Status : std::uint8_t { Success, Failure, Running };

// Base of every node in the tree. Ticked top-down; owns nothing by itself.
class Node {
  public:
    virtual ~Node() = default;
    virtual Status tick() = 0;
    // reset() clears any resumption/iteration state in this node and its children
    // so the next tick starts the node fresh. Default: no-op; composites and
    // decorators override and recurse into their children.
    virtual void reset() {}
};
using NodePtr = std::unique_ptr<Node>;

// A leaf that computes and returns a Status. Its function may return Running to
// indicate "still working".
class Action : public Node {
  public:
    explicit Action(std::function<Status()> fn) : m_fn(std::move(fn)) {
        MAZ_ASSERT(m_fn, "BehaviorTree Action: null function");
    }
    Status tick() override { return m_fn(); }

  private:
    std::function<Status()> m_fn;
};

// Base for nodes with an ordered list of children (Sequence, Selector).
class Composite : public Node {
  public:
    // Appends a child. c must not be null.
    void addChild(NodePtr c) {
        MAZ_ASSERT(c != nullptr, "BehaviorTree Composite::addChild: null child");
        m_children.push_back(std::move(c));
    }

    // Clears the Running-resume bookmark and recurses into every child.
    void reset() override {
        m_current = 0;
        for (const NodePtr& c : m_children) { c->reset(); }
    }

  protected:
    std::vector<NodePtr> m_children;
    std::size_t m_current = 0;  // child to resume at on the next tick
};

// AND — ticks children in order; returns Success only if ALL children succeed,
// short-circuits on the first Failure. A Running child suspends the sequence
// there and it resumes at that child on the next tick.
class Sequence : public Composite {
  public:
    Status tick() override {
        for (std::size_t i = m_current; i < m_children.size(); ++i) {
            const Status s = m_children[i]->tick();
            if (s == Status::Failure) {
                m_current = 0;
                return Status::Failure;
            }
            if (s == Status::Running) {
                m_current = i;
                return Status::Running;
            }
            // Success: advance to the next child.
            m_current = i + 1;
        }
        m_current = 0;
        return Status::Success;
    }
};

// OR (a.k.a. fallback) — ticks children in order; returns Success on the FIRST
// child that succeeds, returns Failure only if ALL children fail. A Running child
// suspends the selector there and it resumes at that child on the next tick.
class Selector : public Composite {
  public:
    Status tick() override {
        for (std::size_t i = m_current; i < m_children.size(); ++i) {
            const Status s = m_children[i]->tick();
            if (s == Status::Success) {
                m_current = 0;
                return Status::Success;
            }
            if (s == Status::Running) {
                m_current = i;
                return Status::Running;
            }
            // Failure: advance to the next child.
            m_current = i + 1;
        }
        m_current = 0;
        return Status::Failure;
    }
};

// Base for nodes wrapping exactly one child (Inverter, Succeeder, Repeater).
class Decorator : public Node {
  public:
    explicit Decorator(NodePtr child) : m_child(std::move(child)) {
        MAZ_ASSERT(m_child != nullptr, "BehaviorTree Decorator: null child");
    }

    void reset() override { m_child->reset(); }

  protected:
    NodePtr m_child;
};

// Swaps the child's Success/Failure; Running passes through unchanged.
class Inverter : public Decorator {
  public:
    using Decorator::Decorator;
    Status tick() override {
        const Status s = m_child->tick();
        if (s == Status::Success) { return Status::Failure; }
        if (s == Status::Failure) { return Status::Success; }
        return Status::Running;
    }
};

// Ticks the child; if the child is Running returns Running, otherwise always
// returns Success (both Success and Failure of the child become Success).
class Succeeder : public Decorator {
  public:
    using Decorator::Decorator;
    Status tick() override {
        const Status s = m_child->tick();
        if (s == Status::Running) { return Status::Running; }
        return Status::Success;
    }
};

// Repeats the child up to count times. Ticks the child once per tick, resetting
// the child between completed repetitions, and reports Success after count
// completions. A Failure of the child still counts as one completed repetition —
// this is a simple "repeat N times regardless" repeater; a repeat-until-fail
// variant is a future refinement.
class Repeater : public Decorator {
  public:
    Repeater(NodePtr child, int count) : Decorator(std::move(child)), m_count(count) {
        MAZ_ASSERT(count > 0, "BehaviorTree Repeater: count must be > 0");
    }

    Status tick() override {
        const Status s = m_child->tick();
        if (s == Status::Running) {
            // Child still working; don't advance the counter.
            return Status::Running;
        }
        // Success or Failure: one repetition completed.
        m_child->reset();
        ++m_done;
        if (m_done >= m_count) {
            m_done = 0;
            return Status::Success;
        }
        return Status::Running;
    }

    void reset() override {
        m_done = 0;
        Decorator::reset();
    }

  private:
    int m_count = 0;
    int m_done = 0;
};

// Owns the root of a behavior tree and ticks it top-down each frame. Move-only
// (unique_ptr member). Requires a root — not default-constructible.
class BehaviorTree {
  public:
    explicit BehaviorTree(NodePtr root) : m_root(std::move(root)) {
        MAZ_ASSERT(m_root != nullptr, "BehaviorTree: null root");
    }

    // Ticks the whole tree once and returns the root's Status.
    Status tick() { return m_root->tick(); }

    // Restarts the tree from the top (clears all Running-resume state).
    void reset() { m_root->reset(); }

  private:
    NodePtr m_root;
};

// --- Ergonomic construction helpers -----------------------------------------

inline NodePtr action(std::function<Status()> fn) { return std::make_unique<Action>(std::move(fn)); }
inline NodePtr inverter(NodePtr c) { return std::make_unique<Inverter>(std::move(c)); }
inline NodePtr succeeder(NodePtr c) { return std::make_unique<Succeeder>(std::move(c)); }
inline NodePtr repeater(NodePtr c, int n) { return std::make_unique<Repeater>(std::move(c), n); }

template <typename... Cs>
inline NodePtr sequence(Cs&&... cs) {
    auto n = std::make_unique<Sequence>();
    (n->addChild(std::forward<Cs>(cs)), ...);
    return n;
}

template <typename... Cs>
inline NodePtr selector(Cs&&... cs) {
    auto n = std::make_unique<Selector>();
    (n->addChild(std::forward<Cs>(cs)), ...);
    return n;
}

} // namespace maz::ai
