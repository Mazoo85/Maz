#pragma once

#include "maz/game/Collision.hpp"
#include "maz/io/Json.hpp"
#include "maz/math/Math.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

// A minimal editable scene model for the in-engine editor — the data an inspector edits and a scene
// tree lists, kept renderer-agnostic so it is pure logic and unit-testable without a GPU. Each Node
// owns a transform (position / Euler degrees / scale), a local-space AABB (for click picking), a
// mesh id (an index the app maps to a real MeshHandle), and material parameters. The editor app
// turns these into draw calls; the engine keeps only the model.
namespace maz::editor {

struct Node {
    std::string name;
    math::vec3 position{0.0f, 0.0f, 0.0f};
    math::vec3 euler{0.0f, 0.0f, 0.0f}; // degrees, applied Y then X then Z
    math::vec3 scale{1.0f, 1.0f, 1.0f};
    math::vec3 localMin{-0.5f, -0.5f, -0.5f}; // local-space AABB used for picking
    math::vec3 localMax{0.5f, 0.5f, 0.5f};
    uint32_t meshId = 0; // app-side handle index (0 = first mesh)
    int colorIndex = 0;  // app-side swatch index
    float roughness = 0.6f;
    float metallic = 0.0f;
    float specular = 0.0f;
    math::vec3 emissive{0.0f, 0.0f, 0.0f};
    bool visible = true;

    // Local->world transform: translate * rotY * rotX * rotZ * scale.
    math::mat4 modelMatrix() const {
        math::mat4 m = glm::translate(math::mat4(1.0f), position);
        m = glm::rotate(m, glm::radians(euler.y), math::vec3(0, 1, 0));
        m = glm::rotate(m, glm::radians(euler.x), math::vec3(1, 0, 0));
        m = glm::rotate(m, glm::radians(euler.z), math::vec3(0, 0, 1));
        m = glm::scale(m, scale);
        return m;
    }

    // Value equality over every editable field — used by the undo history to detect real changes.
    bool operator==(const Node& o) const {
        return name == o.name && position == o.position && euler == o.euler && scale == o.scale &&
               localMin == o.localMin && localMax == o.localMax && meshId == o.meshId &&
               colorIndex == o.colorIndex && roughness == o.roughness && metallic == o.metallic &&
               specular == o.specular && emissive == o.emissive && visible == o.visible;
    }
    bool operator!=(const Node& o) const { return !(*this == o); }

    // World-space AABB enclosing the transformed local box (transform all 8 corners, take extremes).
    void worldAabb(math::vec3& outMin, math::vec3& outMax) const {
        const math::mat4 m = modelMatrix();
        bool first = true;
        for (int i = 0; i < 8; ++i) {
            const math::vec3 corner((i & 1) ? localMax.x : localMin.x,
                                    (i & 2) ? localMax.y : localMin.y,
                                    (i & 4) ? localMax.z : localMin.z);
            const math::vec4 w = m * math::vec4(corner, 1.0f);
            const math::vec3 p(w.x, w.y, w.z);
            if (first) {
                outMin = outMax = p;
                first = false;
            } else {
                outMin = glm::min(outMin, p);
                outMax = glm::max(outMax, p);
            }
        }
    }
};

struct Scene {
    std::vector<Node> nodes;
    int selected = -1; // index of the selected node, or -1

    Node* selectedNode() {
        return (selected >= 0 && selected < static_cast<int>(nodes.size())) ? &nodes[static_cast<size_t>(selected)]
                                                                            : nullptr;
    }
};

// Snapshot-based undo/redo for the editor. A "gesture" (a drag, a keyboard nudge, a slider grab)
// brackets a set of edits: begin() captures the node list before the gesture, end() compares it to
// the result and, if anything actually changed, pushes the before-state onto the undo stack. undo()
// swaps the current state with the top of the undo stack (moving it to redo), and redo() reverses
// that. Storing whole-scene snapshots keeps it simple and correct at editor scene sizes.
struct History {
    std::vector<std::vector<Node>> undoStack;
    std::vector<std::vector<Node>> redoStack;
    std::vector<Node> pending; // state captured at gesture start
    bool inGesture = false;
    size_t capacity = 64;

    void begin(const std::vector<Node>& state) {
        if (!inGesture) {
            pending = state;
            inGesture = true;
        }
    }
    // Close a gesture; commit an undo entry only if the state actually changed.
    void end(const std::vector<Node>& state) {
        if (!inGesture) {
            return;
        }
        inGesture = false;
        if (state != pending) {
            undoStack.push_back(pending);
            if (undoStack.size() > capacity) {
                undoStack.erase(undoStack.begin());
            }
            redoStack.clear();
        }
    }
    // Record a discrete edit (add / delete / duplicate) that happens in one step rather than over a
    // held gesture: pushes `before` onto the undo stack when it differs from `after`.
    void commit(const std::vector<Node>& before, const std::vector<Node>& after) {
        if (before != after) {
            undoStack.push_back(before);
            if (undoStack.size() > capacity) {
                undoStack.erase(undoStack.begin());
            }
            redoStack.clear();
        }
    }
    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }
    bool undo(std::vector<Node>& current) {
        if (undoStack.empty()) {
            return false;
        }
        redoStack.push_back(current);
        current = undoStack.back();
        undoStack.pop_back();
        return true;
    }
    bool redo(std::vector<Node>& current) {
        if (redoStack.empty()) {
            return false;
        }
        undoStack.push_back(current);
        current = redoStack.back();
        redoStack.pop_back();
        return true;
    }
};

// Return the index of the nearest visible node whose world AABB the ray hits, or -1 for a miss.
// origin/dir are world-space (dir need not be normalized).
inline int pickNode(const Scene& scene, const math::vec3& origin, const math::vec3& dir) {
    const math::vec3 d = glm::normalize(dir);
    int best = -1;
    float bestT = 1e30f;
    for (size_t i = 0; i < scene.nodes.size(); ++i) {
        const Node& n = scene.nodes[i];
        if (!n.visible) {
            continue;
        }
        game::Aabb box;
        n.worldAabb(box.min, box.max);
        const game::RayHit hit = game::raycastAabb(origin, d, box);
        if (hit.hit && hit.t < bestT) {
            bestT = hit.t;
            best = static_cast<int>(i);
        }
    }
    return best;
}

// Snap a scalar / vector to the nearest multiple of `step` (0 or less disables, returning the input).
// Used by the editor's grid snapping so dragged objects land on tidy coordinates.
inline float snap1(float v, float step) {
    return step > 0.0f ? std::round(v / step) * step : v;
}
inline math::vec3 snapToGrid(const math::vec3& v, float step) {
    return step > 0.0f ? math::vec3(snap1(v.x, step), snap1(v.y, step), snap1(v.z, step)) : v;
}

// Intersect a ray with the horizontal plane y = planeY. Returns false if the ray is parallel to the
// plane or the hit is behind the origin; otherwise writes the world-space hit point. Used by the
// editor's translate gizmo to drag the selected node across the ground.
inline bool rayPlaneY(const math::vec3& origin, const math::vec3& dir, float planeY,
                      math::vec3& outHit) {
    if (std::abs(dir.y) < 1e-6f) {
        return false;
    }
    const float t = (planeY - origin.y) / dir.y;
    if (t < 0.0f) {
        return false;
    }
    outHit = origin + dir * t;
    return true;
}

// Build a world-space ray from a pixel on screen through the camera, using the inverse of the
// camera's view-projection (the same reconstruction the sky/SSAO use). `px,py` are pixels with the
// origin at the top-left; `w,h` the viewport size. Returns the ray origin (near plane) and a
// normalized direction.
inline void screenRay(const math::mat4& invViewProj, float px, float py, float w, float h,
                      math::vec3& outOrigin, math::vec3& outDir) {
    const float ndcX = (px / w) * 2.0f - 1.0f;
    const float ndcY = (py / h) * 2.0f - 1.0f; // Vulkan clip: y down, matching top-left pixels
    const math::vec4 nearP = invViewProj * math::vec4(ndcX, ndcY, 0.0f, 1.0f);
    const math::vec4 farP = invViewProj * math::vec4(ndcX, ndcY, 1.0f, 1.0f);
    const math::vec3 n(nearP / nearP.w);
    const math::vec3 f(farP / farP.w);
    outOrigin = n;
    outDir = glm::normalize(f - n);
}

// ---- Serialization: a scene <-> JSON round-trip (human-readable, diff-friendly) ----

inline io::JsonValue toJson(const Scene& s) {
    auto v3 = [](const math::vec3& v) {
        io::JsonValue a = io::JsonValue::array();
        a.push_back(v.x);
        a.push_back(v.y);
        a.push_back(v.z);
        return a;
    };
    io::JsonValue nodes = io::JsonValue::array();
    for (const Node& n : s.nodes) {
        io::JsonValue o = io::JsonValue::object();
        o.fields()["name"] = n.name;
        o.fields()["mesh"] = static_cast<int>(n.meshId);
        o.fields()["color"] = n.colorIndex;
        o.fields()["pos"] = v3(n.position);
        o.fields()["rot"] = v3(n.euler);
        o.fields()["scale"] = v3(n.scale);
        o.fields()["lmin"] = v3(n.localMin);
        o.fields()["lmax"] = v3(n.localMax);
        o.fields()["rough"] = n.roughness;
        o.fields()["metal"] = n.metallic;
        o.fields()["spec"] = n.specular;
        o.fields()["emissive"] = v3(n.emissive);
        o.fields()["visible"] = n.visible;
        nodes.push_back(std::move(o));
    }
    io::JsonValue root = io::JsonValue::object();
    root.fields()["nodes"] = std::move(nodes);
    root.fields()["selected"] = s.selected;
    return root;
}

inline bool fromJson(const io::JsonValue& root, Scene& out) {
    if (!root.isObject()) {
        return false;
    }
    const io::JsonValue* nodesV = root.fields().find("nodes");
    if (!nodesV || !nodesV->isArray()) {
        return false;
    }
    auto v3 = [](const io::JsonValue* a) -> math::vec3 {
        if (!a || !a->isArray() || a->items().size() < 3) {
            return math::vec3(0.0f);
        }
        return math::vec3(a->items()[0].asFloat(), a->items()[1].asFloat(),
                          a->items()[2].asFloat());
    };
    Scene s;
    for (const io::JsonValue& o : nodesV->items()) {
        if (!o.isObject()) {
            continue;
        }
        const io::JsonValue::Object& f = o.fields();
        Node n;
        if (const io::JsonValue* p = f.find("name")) n.name = p->asString();
        n.meshId = static_cast<uint32_t>(f.find("mesh") ? f.find("mesh")->asInt() : 0);
        n.colorIndex = f.find("color") ? f.find("color")->asInt() : 0;
        n.position = v3(f.find("pos"));
        n.euler = v3(f.find("rot"));
        n.scale = v3(f.find("scale"));
        n.localMin = v3(f.find("lmin"));
        n.localMax = v3(f.find("lmax"));
        n.roughness = f.find("rough") ? f.find("rough")->asFloat() : 1.0f;
        n.metallic = f.find("metal") ? f.find("metal")->asFloat() : 0.0f;
        n.specular = f.find("spec") ? f.find("spec")->asFloat() : 0.0f;
        n.emissive = v3(f.find("emissive"));
        n.visible = f.find("visible") ? f.find("visible")->asBool() : true;
        s.nodes.push_back(std::move(n));
    }
    if (const io::JsonValue* sel = root.fields().find("selected")) {
        s.selected = sel->asInt(-1);
    }
    out = std::move(s);
    return true;
}

} // namespace maz::editor
