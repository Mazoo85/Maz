#pragma once

#include "maz/math/Math.hpp"

#include <glm/gtc/matrix_inverse.hpp>

#include <vector>

namespace maz::anim {

// Skeletal-animation core: a joint hierarchy plus the matrix math that turns an animated pose into
// the per-joint "skinning matrices" a mesh is deformed by. This is the engine-agnostic heart of
// character animation; a renderer can apply the result on the GPU (a joint-matrix UBO + skinned
// vertex shader) or, as the demo does, skin vertices on the CPU and stream them through the dynamic-
// mesh path — so it needs no new vertex format and can't regress the existing mesh pipeline.
//
// Convention: joints are stored parents-before-children (topological order), each with a parent
// index (-1 for a root) and a rest-pose LOCAL transform. From those, global bind transforms and
// their inverses are precomputed. Header-only, pure math — unit-tests without a GPU.
struct Joint {
    int parent = -1;             // index of the parent joint, or -1 for a root
    math::mat4 localBind{1.0f};  // rest-pose transform relative to the parent
};

class Skeleton {
public:
    Skeleton() = default;
    explicit Skeleton(std::vector<Joint> joints) { setJoints(std::move(joints)); }

    // Set the joints (parents before children) and precompute global bind + inverse-bind matrices.
    void setJoints(std::vector<Joint> joints) {
        m_joints = std::move(joints);
        const size_t n = m_joints.size();
        m_globalBind.assign(n, math::mat4(1.0f));
        m_inverseBind.assign(n, math::mat4(1.0f));
        for (size_t i = 0; i < n; ++i) {
            const int p = m_joints[i].parent;
            m_globalBind[i] = (p >= 0) ? m_globalBind[static_cast<size_t>(p)] * m_joints[i].localBind
                                       : m_joints[i].localBind;
            m_inverseBind[i] = glm::inverse(m_globalBind[i]);
        }
    }

    size_t jointCount() const { return m_joints.size(); }
    int parent(size_t i) const { return m_joints[i].parent; }
    const math::mat4& localBind(size_t i) const { return m_joints[i].localBind; }
    const std::vector<math::mat4>& globalBind() const { return m_globalBind; }
    const std::vector<math::mat4>& inverseBind() const { return m_inverseBind; }

    // Chain per-joint LOCAL transforms into global (model-space) transforms: global[i] =
    // global[parent] * local[i]. `local` must have jointCount() entries.
    void computeGlobals(const std::vector<math::mat4>& local,
                        std::vector<math::mat4>& outGlobal) const {
        const size_t n = m_joints.size();
        outGlobal.assign(n, math::mat4(1.0f));
        for (size_t i = 0; i < n; ++i) {
            const int p = m_joints[i].parent;
            outGlobal[i] =
                (p >= 0) ? outGlobal[static_cast<size_t>(p)] * local[i] : local[i];
        }
    }

    // Produce skinning matrices from an animated pose (per-joint local transforms):
    //   skin[i] = globalPose[i] * inverseBind[i]
    // Applied to a bind-pose vertex, skin[i] moves it by however joint i moved from its rest pose,
    // so at rest (local == localBind) every skin matrix is the identity. `local` has jointCount()
    // entries; `outSkin` is filled with the same count.
    void computeSkinning(const std::vector<math::mat4>& local,
                         std::vector<math::mat4>& outSkin) const {
        std::vector<math::mat4> global;
        computeGlobals(local, global);
        const size_t n = m_joints.size();
        outSkin.assign(n, math::mat4(1.0f));
        for (size_t i = 0; i < n; ++i) {
            outSkin[i] = global[i] * m_inverseBind[i];
        }
    }

    // Convenience: the rest-pose local transforms (a copy), a handy base to animate from.
    std::vector<math::mat4> restLocals() const {
        std::vector<math::mat4> locals;
        locals.reserve(m_joints.size());
        for (const Joint& j : m_joints) {
            locals.push_back(j.localBind);
        }
        return locals;
    }

private:
    std::vector<Joint> m_joints;
    std::vector<math::mat4> m_globalBind;
    std::vector<math::mat4> m_inverseBind;
};

} // namespace maz::anim
