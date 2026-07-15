#pragma once

#include <memory>
#include <string>
#include <vector>

#include "maz/script/Script.hpp"

// maz::script::ScriptSystem — the bridge that makes maz::script actually *drive the engine*. It
// attaches a script class (see SC5) to a game node and runs that instance's lifecycle hooks
// (_ready / _process(dt) / _physics_process(dt), see SC6) each frame, exposing the node's transform
// to the script through a bound "Node2D" host type (SC6 binding). This is the capstone that turns
// the scripting language from "a language" into "how you write Maz gameplay" — the same
// script-attached-to-node model as Godot, but sandboxed and deterministic.
//
//   ScriptSystem sys;
//   sys.registerScript("Spinner", "func _process(dt) { self.node.rotation = self.node.rotation + dt; }");
//   auto node = sys.spawn("Spinner");   // returns a Node2D the host can read/move
//   sys.process(0.016);                 // drives every attached _process(dt)
//   float r = node->rotation;           // the script moved it
namespace maz::script {

// The transform state of a scripted node. Plain fields (no GPU/glm coupling) so the system stays
// header-only and unit-testable; a host is free to copy these into its own ecs::Transform each frame.
struct Node2D {
    double x = 0.0;
    double y = 0.0;
    double rotation = 0.0; // radians
    double scaleX = 1.0;
    double scaleY = 1.0;
    bool visible = true;
    std::string name;
};

// A live script instance bound to a node. Owned by the ScriptSystem.
struct ScriptInstance {
    std::shared_ptr<Node2D> node;
    Value self;         // the script Object (SC5 instance)
    std::string script; // the script/class name it was spawned from
};

class ScriptSystem {
public:
    ScriptSystem() {
        // Bind the Node2D host type once: script code reads/writes self.node.x, .rotation, etc.
        // (SC6 property binding over the raw Node2D*).
        m_vm.bindClass("Node2D")
            .property(
                "x", [](void* n) { return Value::fromNum(static_cast<Node2D*>(n)->x); },
                [](void* n, const Value& v) { static_cast<Node2D*>(n)->x = v.number; })
            .property(
                "y", [](void* n) { return Value::fromNum(static_cast<Node2D*>(n)->y); },
                [](void* n, const Value& v) { static_cast<Node2D*>(n)->y = v.number; })
            .property(
                "rotation", [](void* n) { return Value::fromNum(static_cast<Node2D*>(n)->rotation); },
                [](void* n, const Value& v) { static_cast<Node2D*>(n)->rotation = v.number; })
            .property(
                "scale_x", [](void* n) { return Value::fromNum(static_cast<Node2D*>(n)->scaleX); },
                [](void* n, const Value& v) { static_cast<Node2D*>(n)->scaleX = v.number; })
            .property(
                "scale_y", [](void* n) { return Value::fromNum(static_cast<Node2D*>(n)->scaleY); },
                [](void* n, const Value& v) { static_cast<Node2D*>(n)->scaleY = v.number; })
            .property(
                "visible", [](void* n) { return Value::fromBool(static_cast<Node2D*>(n)->visible); },
                [](void* n, const Value& v) { static_cast<Node2D*>(n)->visible = v.isTruthy(); })
            .property(
                "name", [](void* n) { return Value::fromStr(static_cast<Node2D*>(n)->name); },
                [](void* n, const Value& v) { static_cast<Node2D*>(n)->name = v.toString(); })
            .method("translate", [](void* n, std::vector<Value>& a) {
                auto* node = static_cast<Node2D*>(n);
                node->x += a.size() > 0 ? a[0].number : 0.0;
                node->y += a.size() > 1 ? a[1].number : 0.0;
                return Value::nil();
            });
    }

    // The interpreter, for the host to register natives, tune budgets, set debug hooks, etc.
    Vm& vm() { return m_vm; }

    // Load the script source. All scripted classes (and shared functions) live in one program so
    // they can call each other and share globals. Returns false + sets error on parse failure.
    bool loadSource(const std::string& source) { return m_vm.run(source); }

    // Convenience: define (or extend) the program with a single named script class inline. Accumulates
    // across calls so you can register several scripts, then spawn any of them.
    bool registerScript(const std::string& className, const std::string& body) {
        m_program += "class " + className + " {\n" + body + "\n}\n";
        return m_vm.run(m_program);
    }

    // Spawn an instance of a script class, bind a fresh Node2D to its `node` field, and run _ready.
    // Returns the node so the host/renderer can read (and the physics system can write) its transform.
    std::shared_ptr<Node2D> spawn(const std::string& className, std::vector<Value> initArgs = {}) {
        auto node = std::make_shared<Node2D>();
        if (attach(className, node, std::move(initArgs)).type != Value::Type::Object) {
            return nullptr;
        }
        return node;
    }

    // Attach a script class to an EXISTING node (used by the SceneTree, where the node's transform
    // already lives in the hierarchy). Binds self.node, runs _ready, and returns the script instance
    // (Type::Object), or nil on failure.
    Value attach(const std::string& className, std::shared_ptr<Node2D> node,
                 std::vector<Value> initArgs = {}) {
        Value self = m_vm.instantiate(className, std::move(initArgs));
        if (self.type != Value::Type::Object) {
            m_lastError = "ScriptSystem: cannot spawn unknown or invalid script class '" + className + "'";
            return Value::nil();
        }
        Value nodeVal = m_vm.makeNativeObject("Node2D", node);
        setSelfField(self, "node", nodeVal);
        m_instances.push_back(ScriptInstance{node, self, className});
        if (m_vm.objectHasMethod(self, "_ready")) {
            m_vm.callOn(self, "_ready", {});
        }
        return self;
    }

    // Drive every live instance's _process(dt) hook (frame update).
    void process(double dt) { dispatch("_process", dt); }
    // Drive every live instance's _physics_process(dt) hook (fixed-step update).
    void physicsProcess(double dt) { dispatch("_physics_process", dt); }

    // Call a named method on every instance that defines it (e.g. a broadcast "on_pause").
    void broadcast(const std::string& method, std::vector<Value> args = {}) {
        for (auto& inst : m_instances) {
            if (m_vm.objectHasMethod(inst.self, method)) {
                std::vector<Value> a = args;
                m_vm.callOn(inst.self, method, a);
            }
        }
    }

    size_t instanceCount() const { return m_instances.size(); }
    const std::vector<ScriptInstance>& instances() const { return m_instances; }
    const std::string& error() const { return m_lastError.empty() ? m_vm.error() : m_lastError; }

    // Remove all live instances (e.g. on scene change). The script program stays loaded.
    void clearInstances() { m_instances.clear(); }

private:
    void dispatch(const std::string& method, double dt) {
        for (auto& inst : m_instances) {
            if (m_vm.objectHasMethod(inst.self, method)) {
                std::vector<Value> args{Value::fromNum(dt)};
                m_vm.callOn(inst.self, method, args);
            }
        }
    }
    // Set a field on a script instance directly (used to inject `node`).
    static void setSelfField(Value& self, const std::string& name, const Value& v) {
        if (self.type != Value::Type::Object || !self.instance) {
            return;
        }
        if (Value* f = self.instance->findField(name)) {
            *f = v;
        } else {
            self.instance->fields.emplace_back(name, v);
        }
    }

    Vm m_vm;
    std::string m_program;                    // accumulated script source (for registerScript)
    std::vector<ScriptInstance> m_instances;  // live script-attached nodes
    std::string m_lastError;
};

} // namespace maz::script
