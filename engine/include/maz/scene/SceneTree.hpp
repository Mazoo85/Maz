#pragma once

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "maz/script/ScriptSystem.hpp"

// maz::scene::SceneTree — the unified node hierarchy, Maz's answer to Godot's SceneTree + Node2D.
// It ties together the three things a real game needs in one model:
//   1. A tree of named nodes with parent/child relationships.
//   2. 2D transform hierarchy — a child's world transform composes with its parent's (position,
//      rotation, and scale all propagate), so moving a parent moves its whole subtree.
//   3. Attached scripts — a node can carry a maz::script class whose _ready / _process(dt) /
//      _physics_process(dt) hooks the tree drives in deterministic depth-first order, with the
//      node's LOCAL transform exposed to the script as `self.node` (SC6 binding).
// Plus groups (broadcast to tagged nodes) and path lookup ("Player/Weapon"). This is the substrate
// a whole game — the ZOMBOID port included — is described and simulated on.
namespace maz::scene {

class SceneTree; // forward

// A node in the scene tree. Owns its children. Its transform (position/rotation/scale/name/visible)
// lives in a shared script::Node2D so an attached script can read and write it directly.
class SceneNode {
public:
    explicit SceneNode(std::string name) { m_xform->name = std::move(name); }

    const std::string& name() const { return m_xform->name; }
    void setName(const std::string& n) { m_xform->name = n; }

    // ---- local transform (relative to parent) ----
    script::Node2D& local() { return *m_xform; }
    const script::Node2D& local() const { return *m_xform; }
    std::shared_ptr<script::Node2D> transformPtr() const { return m_xform; }

    double x() const { return m_xform->x; }
    double y() const { return m_xform->y; }
    double rotation() const { return m_xform->rotation; }
    void setPosition(double x, double y) { m_xform->x = x; m_xform->y = y; }
    void setRotation(double r) { m_xform->rotation = r; }

    SceneNode* parent() const { return m_parent; }
    const std::vector<std::unique_ptr<SceneNode>>& children() const { return m_children; }

    // ---- world transform (composed through the hierarchy) ----
    double worldRotation() const {
        return (m_parent ? m_parent->worldRotation() : 0.0) + m_xform->rotation;
    }
    double worldScaleX() const {
        return (m_parent ? m_parent->worldScaleX() : 1.0) * m_xform->scaleX;
    }
    double worldScaleY() const {
        return (m_parent ? m_parent->worldScaleY() : 1.0) * m_xform->scaleY;
    }
    void worldPosition(double& ox, double& oy) const {
        if (!m_parent) {
            ox = m_xform->x;
            oy = m_xform->y;
            return;
        }
        double px = 0, py = 0;
        m_parent->worldPosition(px, py);
        const double pr = m_parent->worldRotation();
        const double lx = m_xform->x * m_parent->worldScaleX();
        const double ly = m_xform->y * m_parent->worldScaleY();
        const double c = std::cos(pr), s = std::sin(pr);
        ox = px + (lx * c - ly * s);
        oy = py + (lx * s + ly * c);
    }
    double worldX() const { double x, y; worldPosition(x, y); return x; }
    double worldY() const { double x, y; worldPosition(x, y); return y; }

    bool visibleInTree() const {
        if (!m_xform->visible) return false;
        return m_parent ? m_parent->visibleInTree() : true;
    }

    // ---- groups (tags for broadcast / queries) ----
    void addToGroup(const std::string& g) {
        for (const auto& x : m_groups) {
            if (x == g) return;
        }
        m_groups.push_back(g);
    }
    bool inGroup(const std::string& g) const {
        for (const auto& x : m_groups) {
            if (x == g) return true;
        }
        return false;
    }
    const std::vector<std::string>& groups() const { return m_groups; }

    // The attached script instance (Type::Object), or Nil if none.
    const script::Value& script() const { return m_script; }
    const std::string& scriptClass() const { return m_scriptClass; }

private:
    friend class SceneTree;
    std::shared_ptr<script::Node2D> m_xform = std::make_shared<script::Node2D>();
    SceneNode* m_parent = nullptr;
    std::vector<std::unique_ptr<SceneNode>> m_children;
    std::vector<std::string> m_groups;
    script::Value m_script; // attached script instance
    std::string m_scriptClass;
};

class SceneTree {
public:
    SceneTree() : m_root(std::make_unique<SceneNode>("root")) {}

    SceneNode& root() { return *m_root; }
    script::ScriptSystem& scripts() { return m_scripts; }

    // Load the script program (all node scripts live in one program so they can share code).
    bool loadScripts(const std::string& source) { return m_scripts.loadSource(source); }

    // Create a child node under `parent` and return a pointer to it (owned by the tree).
    SceneNode* createChild(SceneNode& parent, const std::string& name) {
        auto child = std::make_unique<SceneNode>(name);
        child->m_parent = &parent;
        SceneNode* raw = child.get();
        parent.m_children.push_back(std::move(child));
        return raw;
    }

    // Attach a script class to a node: binds the node's LOCAL transform to the script and runs _ready.
    bool attachScript(SceneNode& node, const std::string& className) {
        script::Value self = m_scripts.attach(className, node.m_xform);
        if (self.type != script::Value::Type::Object) {
            return false;
        }
        node.m_script = self;
        node.m_scriptClass = className;
        return true;
    }

    // Depth-first _process(dt) over the tree (parents before children — Godot's order).
    void process(double dt) { dispatch(*m_root, "_process", dt); }
    void physicsProcess(double dt) { dispatch(*m_root, "_physics_process", dt); }

    // Find a node by slash path from the root, e.g. "Player/Weapon" ("" or "root" -> the root).
    SceneNode* findNode(const std::string& path) {
        if (path.empty() || path == "root") return m_root.get();
        SceneNode* cur = m_root.get();
        size_t start = 0;
        while (start <= path.size()) {
            size_t slash = path.find('/', start);
            std::string part =
                path.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
            SceneNode* next = nullptr;
            for (const auto& c : cur->m_children) {
                if (c->name() == part) { next = c.get(); break; }
            }
            if (!next) return nullptr;
            cur = next;
            if (slash == std::string::npos) break;
            start = slash + 1;
        }
        return cur;
    }

    // Collect every node tagged with `group`.
    std::vector<SceneNode*> nodesInGroup(const std::string& group) {
        std::vector<SceneNode*> out;
        collectGroup(*m_root, group, out);
        return out;
    }

    // Call a named method on every scripted node in `group` (e.g. broadcast "on_wave_start").
    void callGroup(const std::string& group, const std::string& method,
                   std::vector<script::Value> args = {}) {
        for (SceneNode* n : nodesInGroup(group)) {
            if (n->m_script.type == script::Value::Type::Object &&
                m_scripts.vm().objectHasMethod(n->m_script, method)) {
                std::vector<script::Value> a = args;
                m_scripts.vm().callOn(n->m_script, method, a);
            }
        }
    }

    // Total node count (including the root).
    size_t nodeCount() const { return count(*m_root); }

private:
    void dispatch(SceneNode& node, const std::string& method, double dt) {
        if (node.m_script.type == script::Value::Type::Object &&
            m_scripts.vm().objectHasMethod(node.m_script, method)) {
            std::vector<script::Value> args{script::Value::fromNum(dt)};
            m_scripts.vm().callOn(node.m_script, method, args);
        }
        for (const auto& c : node.m_children) {
            dispatch(*c, method, dt);
        }
    }
    static void collectGroup(SceneNode& node, const std::string& group, std::vector<SceneNode*>& out) {
        if (node.inGroup(group)) out.push_back(&node);
        for (const auto& c : node.children()) {
            collectGroup(*c, group, out);
        }
    }
    static size_t count(const SceneNode& node) {
        size_t n = 1;
        for (const auto& c : node.children()) {
            n += count(*c);
        }
        return n;
    }

    std::unique_ptr<SceneNode> m_root;
    script::ScriptSystem m_scripts;
};

} // namespace maz::scene
