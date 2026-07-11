#pragma once

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace maz::game::bt {

// Behavior trees — a scalable, reactive alternative to the finite state machine for AI decisions.
// A tree is ticked every frame; each node returns Success, Failure, or Running. Composites route
// the tick: a Sequence runs children until one is not Success (AND), a Selector until one is not
// Failure (fallback / priority OR). These composites are REACTIVE (memoryless): every tick re-
// evaluates from the first child, so a higher-priority branch (e.g. "flee") can pre-empt a running
// lower-priority one (e.g. "patrol") the instant its condition flips — the behaviour you want for
// reactive agents. Leaves wrap gameplay via std::function. Header-only, no GPU, so it unit-tests
// headless.

enum class Status { Success, Failure, Running };

class Node {
public:
    virtual ~Node() = default;
    virtual Status tick() = 0;
    virtual void reset() {}
};
using NodePtr = std::unique_ptr<Node>;

// Leaf: runs a gameplay action, returning its status (Running for an ongoing behaviour).
class Action : public Node {
public:
    explicit Action(std::function<Status()> fn) : m_fn(std::move(fn)) {}
    Status tick() override { return m_fn(); }

private:
    std::function<Status()> m_fn;
};

// Leaf: a predicate -> Success (true) / Failure (false). Gates branches.
class Condition : public Node {
public:
    explicit Condition(std::function<bool()> fn) : m_fn(std::move(fn)) {}
    Status tick() override { return m_fn() ? Status::Success : Status::Failure; }

private:
    std::function<bool()> m_fn;
};

// Composite: tick children in order; stop at the first that is NOT Success (propagating Failure or
// Running). Returns Success only if all children succeed. (Logical AND / ordered steps.)
class Sequence : public Node {
public:
    explicit Sequence(std::vector<NodePtr> children) : m_children(std::move(children)) {}
    Status tick() override {
        for (NodePtr& c : m_children) {
            const Status s = c->tick();
            if (s != Status::Success) {
                return s;
            }
        }
        return Status::Success;
    }
    void reset() override {
        for (NodePtr& c : m_children) {
            c->reset();
        }
    }

private:
    std::vector<NodePtr> m_children;
};

// Composite: tick children in order; stop at the first that is NOT Failure (propagating Success or
// Running). Returns Failure only if all children fail. (Priority fallback / logical OR.)
class Selector : public Node {
public:
    explicit Selector(std::vector<NodePtr> children) : m_children(std::move(children)) {}
    Status tick() override {
        for (NodePtr& c : m_children) {
            const Status s = c->tick();
            if (s != Status::Failure) {
                return s;
            }
        }
        return Status::Failure;
    }
    void reset() override {
        for (NodePtr& c : m_children) {
            c->reset();
        }
    }

private:
    std::vector<NodePtr> m_children;
};

// Decorator: flips Success<->Failure, passes Running through.
class Inverter : public Node {
public:
    explicit Inverter(NodePtr child) : m_child(std::move(child)) {}
    Status tick() override {
        const Status s = m_child->tick();
        if (s == Status::Success) return Status::Failure;
        if (s == Status::Failure) return Status::Success;
        return Status::Running;
    }
    void reset() override { m_child->reset(); }

private:
    NodePtr m_child;
};

// --- Builder helpers (concise tree construction) ---
inline NodePtr action(std::function<Status()> fn) {
    return std::make_unique<Action>(std::move(fn));
}
inline NodePtr condition(std::function<bool()> fn) {
    return std::make_unique<Condition>(std::move(fn));
}
inline NodePtr inverter(NodePtr child) {
    return std::make_unique<Inverter>(std::move(child));
}
template <typename... Ns>
NodePtr sequence(Ns&&... nodes) {
    std::vector<NodePtr> v;
    v.reserve(sizeof...(nodes));
    (v.push_back(std::forward<Ns>(nodes)), ...);
    return std::make_unique<Sequence>(std::move(v));
}
template <typename... Ns>
NodePtr selector(Ns&&... nodes) {
    std::vector<NodePtr> v;
    v.reserve(sizeof...(nodes));
    (v.push_back(std::forward<Ns>(nodes)), ...);
    return std::make_unique<Selector>(std::move(v));
}

// The tree itself: owns a root node and ticks it.
class BehaviorTree {
public:
    BehaviorTree() = default;
    explicit BehaviorTree(NodePtr root) : m_root(std::move(root)) {}
    void setRoot(NodePtr root) { m_root = std::move(root); }
    Status tick() { return m_root ? m_root->tick() : Status::Failure; }
    void reset() {
        if (m_root) {
            m_root->reset();
        }
    }

private:
    NodePtr m_root;
};

} // namespace maz::game::bt
