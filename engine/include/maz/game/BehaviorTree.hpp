#pragma once

#include <any>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
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

// Blackboard: the tree's shared working memory. Leaves read/write named, typed values (perceptions,
// targets, cooldowns, flags) instead of talking to each other, so a subtree stays reusable and the
// tree's decisions are driven by data — the standard companion to a behavior tree. Header-only
// (std::any), typed accessors; getOr() is the safe read (missing key or wrong type -> fallback).
class Blackboard {
public:
    template <typename T>
    void set(const std::string& key, T value) {
        m_data[key] = std::move(value);
    }
    bool has(const std::string& key) const { return m_data.find(key) != m_data.end(); }
    void erase(const std::string& key) { m_data.erase(key); }
    void clear() { m_data.clear(); }

    // Throws std::bad_any_cast if absent/mistyped — use when the key is guaranteed present.
    template <typename T>
    T get(const std::string& key) const {
        return std::any_cast<T>(m_data.at(key));
    }
    // Safe read: returns `fallback` if the key is missing or holds a different type.
    template <typename T>
    T getOr(const std::string& key, T fallback) const {
        const auto it = m_data.find(key);
        if (it == m_data.end()) {
            return fallback;
        }
        const T* p = std::any_cast<T>(&it->second);
        return p ? *p : fallback;
    }

private:
    std::unordered_map<std::string, std::any> m_data;
};

// Composite: tick ALL children every tick (they run "in parallel"). Success/failure is decided by a
// policy — RequireOne succeeds as soon as any child succeeds (fails only if all fail); RequireAll
// succeeds only when every child succeeds (fails as soon as any fails). Otherwise Running. The classic
// use is "do X while monitoring Y". (Godot behavior-tree Parallel.)
enum class ParallelPolicy { RequireOne, RequireAll };
class Parallel : public Node {
public:
    Parallel(ParallelPolicy success, std::vector<NodePtr> children)
        : m_success(success), m_children(std::move(children)) {}
    Status tick() override {
        int succ = 0, fail = 0;
        for (NodePtr& c : m_children) {
            const Status s = c->tick(); // every child is ticked (true parallel semantics)
            if (s == Status::Success) {
                ++succ;
            } else if (s == Status::Failure) {
                ++fail;
            }
        }
        const int n = static_cast<int>(m_children.size());
        if (m_success == ParallelPolicy::RequireOne) {
            if (succ >= 1) return Status::Success;
            if (fail == n) return Status::Failure;
        } else {
            if (succ == n) return Status::Success;
            if (fail >= 1) return Status::Failure;
        }
        return Status::Running;
    }
    void reset() override {
        for (NodePtr& c : m_children) {
            c->reset();
        }
    }

private:
    ParallelPolicy m_success;
    std::vector<NodePtr> m_children;
};

// Decorator: re-run the child until it has SUCCEEDED `count` times (count <= 0 => forever). Returns
// Running while repeating, Success once the target count is reached; a child Failure aborts -> Failure.
class Repeater : public Node {
public:
    Repeater(int count, NodePtr child) : m_count(count), m_child(std::move(child)) {}
    Status tick() override {
        const Status s = m_child->tick();
        if (s == Status::Failure) {
            return Status::Failure;
        }
        if (s == Status::Success) {
            ++m_done;
            m_child->reset(); // ready for the next repetition
            if (m_count > 0 && m_done >= m_count) {
                return Status::Success;
            }
        }
        return Status::Running;
    }
    void reset() override {
        m_done = 0;
        m_child->reset();
    }

private:
    int m_count;
    int m_done = 0;
    NodePtr m_child;
};

// Decorator: force the result — AlwaysSucceed turns Failure into Success, AlwaysFail turns Success into
// Failure; Running always passes through. Handy to keep a Sequence going past an optional step.
class AlwaysSucceed : public Node {
public:
    explicit AlwaysSucceed(NodePtr child) : m_child(std::move(child)) {}
    Status tick() override {
        const Status s = m_child->tick();
        return s == Status::Running ? Status::Running : Status::Success;
    }
    void reset() override { m_child->reset(); }

private:
    NodePtr m_child;
};
class AlwaysFail : public Node {
public:
    explicit AlwaysFail(NodePtr child) : m_child(std::move(child)) {}
    Status tick() override {
        const Status s = m_child->tick();
        return s == Status::Running ? Status::Running : Status::Failure;
    }
    void reset() override { m_child->reset(); }

private:
    NodePtr m_child;
};

// Decorator: pass the child's status through unchanged but record it (as an int: 0 Success / 1 Failure /
// 2 Running) into `*out`. A transparent probe for debugging and for visualizing which branch ran.
class Tap : public Node {
public:
    Tap(int* out, NodePtr child) : m_out(out), m_child(std::move(child)) {}
    Status tick() override {
        const Status s = m_child->tick();
        if (m_out) {
            *m_out = static_cast<int>(s);
        }
        return s;
    }
    void reset() override { m_child->reset(); }

private:
    int* m_out;
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
template <typename... Ns>
NodePtr parallel(ParallelPolicy policy, Ns&&... nodes) {
    std::vector<NodePtr> v;
    v.reserve(sizeof...(nodes));
    (v.push_back(std::forward<Ns>(nodes)), ...);
    return std::make_unique<Parallel>(policy, std::move(v));
}
inline NodePtr repeater(int count, NodePtr child) {
    return std::make_unique<Repeater>(count, std::move(child));
}
inline NodePtr alwaysSucceed(NodePtr child) {
    return std::make_unique<AlwaysSucceed>(std::move(child));
}
inline NodePtr alwaysFail(NodePtr child) {
    return std::make_unique<AlwaysFail>(std::move(child));
}
inline NodePtr tap(int* out, NodePtr child) {
    return std::make_unique<Tap>(out, std::move(child));
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
