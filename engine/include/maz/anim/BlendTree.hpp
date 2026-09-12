#pragma once

#include "maz/anim/AnimClip.hpp"
#include "maz/anim/BlendSpace.hpp"
#include "maz/math/Math.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace maz::anim {

// Animation blend TREE — Godot's AnimationNodeBlendTree. Where a BlendSpace (BlendSpace.hpp) mixes a
// flat set of animations by one parameter and a state machine (AnimStateMachine.hpp) cross-fades
// whole states, a blend tree is the graph that NESTS them: a node reads named blend parameters and
// combines the poses of its child nodes, so you can build "a walk/run blend space, cross-faded into a
// jump by an air parameter, with an additive upper-body wave layered on top" as one evaluable tree.
//
// A `Pose` is a vector of per-joint local transforms (anim::JointPose). Leaf `Input` nodes pull a
// pose out of an external table you pass to evaluate() (in a real rig those are sampled clips); the
// interior nodes blend them. Everything is pure pose math — no GPU, no clip sampling here — so the
// tree unit-tests headlessly and stays deterministic under the fixed timestep.

using Pose = std::vector<JointPose>;

class BlendTree {
public:
    enum Type {
        Input,       // leaf: outputs the external pose whose index is stored in `a`
        Blend2,      // cross-fade child `a` -> child `b` by param in [0,1] (AnimationNodeBlend2)
        Add2,        // base child `a` + param * additive-delta child `b` (AnimationNodeAdd2)
        BlendSpace1, // 1-D blend space over child nodes, selected by param (AnimationNodeBlendSpace1D)
    };

    // ---- build API (each returns the new node's handle) --------------------------------------

    // A leaf that outputs external pose #poseId (the pose at that index in evaluate()'s `inputs`).
    int addInput(int poseId) {
        m_nodes.push_back(Node{Input, poseId, -1, {}, {}, {}});
        return static_cast<int>(m_nodes.size()) - 1;
    }

    // Cross-fade node `a` (param 0) to node `b` (param 1) by the named parameter, clamped to [0,1].
    int addBlend2(int a, int b, std::string param) {
        m_nodes.push_back(Node{Blend2, a, b, std::move(param), {}, {}});
        return static_cast<int>(m_nodes.size()) - 1;
    }

    // Additive layer: node `a` is the base, node `b` an additive delta scaled by the named parameter.
    int addAdd2(int base, int additive, std::string param) {
        m_nodes.push_back(Node{Add2, base, additive, std::move(param), {}, {}});
        return static_cast<int>(m_nodes.size()) - 1;
    }

    // A 1-D blend space over child NODES, selected by the named parameter. Add children with
    // addBlendPoint(); their poses are mixed by blendPosesWeighted using the space's weights.
    int addBlendSpace1(std::string param) {
        m_nodes.push_back(Node{BlendSpace1, -1, -1, std::move(param), {}, {}});
        return static_cast<int>(m_nodes.size()) - 1;
    }

    // Add a child node at parameter position `pos` on a BlendSpace1 node.
    void addBlendPoint(int spaceNode, float pos, int childNode) {
        if (spaceNode < 0 || spaceNode >= static_cast<int>(m_nodes.size())) {
            return;
        }
        Node& n = m_nodes[static_cast<size_t>(spaceNode)];
        const int slot = static_cast<int>(n.children.size());
        n.children.push_back(childNode);
        n.space.addPoint(pos, slot);
    }

    void setRoot(int node) { m_root = node; }
    int root() const { return m_root; }
    size_t nodeCount() const { return m_nodes.size(); }

    // ---- parameters --------------------------------------------------------------------------

    void setParam(const std::string& name, float value) { m_params[name] = value; }
    float param(const std::string& name) const {
        auto it = m_params.find(name);
        return it == m_params.end() ? 0.0f : it->second;
    }

    // ---- evaluation --------------------------------------------------------------------------

    // Evaluate the tree into `out`, given the external pose table (Input node poseId -> Pose*).
    // Empty result if there is no root or the root is out of range.
    void evaluate(const std::vector<const Pose*>& inputs, Pose& out) const {
        if (m_root < 0 || m_root >= static_cast<int>(m_nodes.size())) {
            out.clear();
            return;
        }
        eval(m_root, inputs, out);
    }

private:
    struct Node {
        int type;
        int a;             // child node id, or (for Input) the external pose index
        int b;             // second child node id
        std::string param; // blend parameter name
        BlendSpace1D space;
        std::vector<int> children; // BlendSpace1: point-slot -> child node id
    };

    void eval(int node, const std::vector<const Pose*>& inputs, Pose& out) const {
        if (node < 0 || node >= static_cast<int>(m_nodes.size())) {
            out.clear();
            return;
        }
        const Node& n = m_nodes[static_cast<size_t>(node)];
        switch (n.type) {
        case Input: {
            if (n.a >= 0 && n.a < static_cast<int>(inputs.size()) && inputs[static_cast<size_t>(n.a)]) {
                out = *inputs[static_cast<size_t>(n.a)];
            } else {
                out.clear();
            }
            return;
        }
        case Blend2: {
            Pose pa, pb;
            eval(n.a, inputs, pa);
            eval(n.b, inputs, pb);
            if (pa.empty()) {
                out = pb;
                return;
            }
            if (pb.empty()) {
                out = pa;
                return;
            }
            blendPoses(pa, pb, param(n.param), out);
            return;
        }
        case Add2: {
            Pose base, add;
            eval(n.a, inputs, base);
            eval(n.b, inputs, add);
            out = base;
            const float w = param(n.param);
            const size_t cnt = base.size() < add.size() ? base.size() : add.size();
            for (size_t i = 0; i < cnt; ++i) {
                // Treat `add` as a delta layered on the base: translation/scale scaled by weight,
                // rotation as a partial rotation slerped in from identity (Godot Add2 semantics).
                out[i].translation = base[i].translation + add[i].translation * w;
                out[i].scale = base[i].scale * glm::mix(math::vec3(1.0f), add[i].scale, w);
                const math::quat delta =
                    glm::slerp(math::quat(1.0f, 0.0f, 0.0f, 0.0f), add[i].rotation, w);
                out[i].rotation = glm::normalize(base[i].rotation * delta);
            }
            return;
        }
        case BlendSpace1: {
            const std::vector<BlendSpace1D::Weight> ws = n.space.weights(param(n.param));
            if (ws.empty()) {
                out.clear();
                return;
            }
            // Evaluate each contributing child, then blend by the space's weights.
            std::vector<Pose> childPoses(ws.size());
            std::vector<const Pose*> ptrs(ws.size());
            std::vector<float> weights(ws.size());
            for (size_t i = 0; i < ws.size(); ++i) {
                const int slot = ws[i].id;
                const int child = (slot >= 0 && slot < static_cast<int>(n.children.size()))
                                      ? n.children[static_cast<size_t>(slot)]
                                      : -1;
                eval(child, inputs, childPoses[i]);
                ptrs[i] = &childPoses[i];
                weights[i] = ws[i].weight;
            }
            blendPosesWeighted(ptrs, weights, out);
            return;
        }
        default:
            out.clear();
            return;
        }
    }

    std::vector<Node> m_nodes;
    int m_root = -1;
    std::unordered_map<std::string, float> m_params;
};

} // namespace maz::anim
