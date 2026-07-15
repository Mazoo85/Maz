#pragma once

#include "maz/game/Collision.hpp"
#include "maz/math/Math.hpp"

#include <glm/gtc/matrix_transform.hpp>

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

} // namespace maz::editor
