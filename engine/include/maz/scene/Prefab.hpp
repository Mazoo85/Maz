#pragma once

#include "maz/math/Math.hpp"

#include <string>
#include <utility>
#include <vector>

namespace maz::scene {

// Prefabs / instancing — Godot's PackedScene, the single most defining thing about Godot's workflow.
// A Prefab is a reusable TEMPLATE: a tree of named nodes, each carrying a bag of exported properties
// (position, colour, hp, …). You author it once and then INSTANTIATE it many times, each instance
// applying per-node OVERRIDES so every copy differs (a different spawn position, tint, or stat) without
// duplicating the template. Instancing produces a fresh, independent node tree — mutating one instance
// never touches the template or its siblings. Pure data (no GPU); header-only; unit-tests headlessly.

// One exported property value — a small tagged union covering the types a 2D game exports.
struct PropValue {
    enum class Type { Float, Int, Bool, Vec2, Color, Text };
    Type type = Type::Float;
    float f = 0.0f;
    int i = 0;
    bool b = false;
    math::vec2 v2{0.0f, 0.0f};
    math::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    std::string text;

    static PropValue makeFloat(float v) {
        PropValue p;
        p.type = Type::Float;
        p.f = v;
        return p;
    }
    static PropValue makeInt(int v) {
        PropValue p;
        p.type = Type::Int;
        p.i = v;
        return p;
    }
    static PropValue makeBool(bool v) {
        PropValue p;
        p.type = Type::Bool;
        p.b = v;
        return p;
    }
    static PropValue makeVec2(math::vec2 v) {
        PropValue p;
        p.type = Type::Vec2;
        p.v2 = v;
        return p;
    }
    static PropValue makeColor(math::vec4 v) {
        PropValue p;
        p.type = Type::Color;
        p.color = v;
        return p;
    }
    static PropValue makeText(std::string v) {
        PropValue p;
        p.type = Type::Text;
        p.text = std::move(v);
        return p;
    }
};

// An ordered key→value bag (small, so linear scan; ordered so serialization is stable).
using PropBag = std::vector<std::pair<std::string, PropValue>>;

inline const PropValue* findProp(const PropBag& bag, const std::string& key) {
    for (const auto& kv : bag) {
        if (kv.first == key) {
            return &kv.second;
        }
    }
    return nullptr;
}

// Set or replace a key in a bag (override semantics).
inline void setProp(PropBag& bag, const std::string& key, const PropValue& value) {
    for (auto& kv : bag) {
        if (kv.first == key) {
            kv.second = value;
            return;
        }
    }
    bag.emplace_back(key, value);
}

// Typed getters with a default when the key is missing or the wrong type.
inline float getFloat(const PropBag& bag, const std::string& key, float def = 0.0f) {
    const PropValue* p = findProp(bag, key);
    return (p && p->type == PropValue::Type::Float) ? p->f : def;
}
inline int getInt(const PropBag& bag, const std::string& key, int def = 0) {
    const PropValue* p = findProp(bag, key);
    return (p && p->type == PropValue::Type::Int) ? p->i : def;
}
inline bool getBool(const PropBag& bag, const std::string& key, bool def = false) {
    const PropValue* p = findProp(bag, key);
    return (p && p->type == PropValue::Type::Bool) ? p->b : def;
}
inline math::vec2 getVec2(const PropBag& bag, const std::string& key, math::vec2 def = math::vec2(0.0f)) {
    const PropValue* p = findProp(bag, key);
    return (p && p->type == PropValue::Type::Vec2) ? p->v2 : def;
}
inline math::vec4 getColor(const PropBag& bag, const std::string& key,
                           math::vec4 def = math::vec4(1.0f)) {
    const PropValue* p = findProp(bag, key);
    return (p && p->type == PropValue::Type::Color) ? p->color : def;
}

// A node in a prefab tree: a name, its exported properties, and child nodes.
struct PrefabNode {
    std::string name;
    PropBag props;
    std::vector<PrefabNode> children;
};

// A prefab is just its root node (the template).
struct Prefab {
    PrefabNode root;
};

// An override entry: a node PATH relative to the root ("" = root, "Turret", "Body/Gun") + the property
// keys to set/replace on that node.
using OverrideMap = std::vector<std::pair<std::string, PropBag>>;

// Find a node by "/"-separated path from `root` ("" returns the root). Returns nullptr if not found.
inline PrefabNode* findNode(PrefabNode& root, const std::string& path) {
    if (path.empty()) {
        return &root;
    }
    PrefabNode* cur = &root;
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t slash = path.find('/', start);
        const std::string name =
            path.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        PrefabNode* next = nullptr;
        for (PrefabNode& c : cur->children) {
            if (c.name == name) {
                next = &c;
                break;
            }
        }
        if (!next) {
            return nullptr;
        }
        cur = next;
        if (slash == std::string::npos) {
            break;
        }
        start = slash + 1;
    }
    return cur;
}

// Instantiate the prefab: deep-copy the template tree, then apply each override's property bag to the
// node at its path (setProp = replace-or-add). The returned tree is fully independent of the template.
inline PrefabNode instantiate(const Prefab& prefab, const OverrideMap& overrides = {}) {
    PrefabNode inst = prefab.root; // deep value copy (vectors + strings copy)
    for (const auto& entry : overrides) {
        PrefabNode* node = findNode(inst, entry.first);
        if (!node) {
            continue; // unknown path — ignore (Godot logs a warning; we just skip)
        }
        for (const auto& kv : entry.second) {
            setProp(node->props, kv.first, kv.second);
        }
    }
    return inst;
}

// Count nodes in a resolved/template tree (root + all descendants).
inline int nodeCount(const PrefabNode& node) {
    int n = 1;
    for (const PrefabNode& c : node.children) {
        n += nodeCount(c);
    }
    return n;
}

} // namespace maz::scene
