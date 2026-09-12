#pragma once

#include "maz/math/Transform2D.hpp" // math::Transform2D, math::vec2

// maz::scene RemoteTransform2D — Godot's RemoteTransform2D node: a node that pushes its own transform
// onto some *other* node each frame (handy for driving a decoupled follower, a camera rig, or a
// hierarchy-crossing attachment). It mirrors Godot's toggles — copy position, rotation, and/or scale
// independently — plus the global-vs-local coordinate choice. This is the pure transform-merge maths:
// given the remote's transform and the target's current transform, it returns the target's new
// transform with only the enabled channels overwritten. When all three channels are on it copies the
// whole transform verbatim (so skew survives); otherwise it recomposes from the selected components.
// Header-only, deterministic, unit-tested; the SceneTree integrator supplies the transforms per frame.
namespace maz::scene {

class RemoteTransform2D {
  public:
    RemoteTransform2D() = default;

    void setUpdatePosition(bool v) { m_position = v; }
    bool updatePosition() const { return m_position; }
    void setUpdateRotation(bool v) { m_rotation = v; }
    bool updateRotation() const { return m_rotation; }
    void setUpdateScale(bool v) { m_scale = v; }
    bool updateScale() const { return m_scale; }

    // Whether the integrator feeds/expects GLOBAL transforms (true, Godot default) or LOCAL ones. This
    // flag records intent for the SceneTree layer; the merge maths itself is space-agnostic. Use
    // globalToLocal() below to turn a desired global transform into a local one under a parent.
    void setUseGlobalCoordinates(bool v) { m_global = v; }
    bool useGlobalCoordinates() const { return m_global; }

    // Merge the enabled channels of `source` into `target`, returning the target's new transform.
    math::Transform2D apply(const math::Transform2D& source, const math::Transform2D& target) const {
        if (m_position && m_rotation && m_scale) {
            return source; // full copy — preserves skew exactly, matching Godot
        }
        const float rot = m_rotation ? source.getRotation() : target.getRotation();
        const math::vec2 scale = m_scale ? source.getScale() : target.getScale();
        const math::vec2 pos = m_position ? source.origin : target.origin;
        return math::Transform2D::compose(rot, scale, pos);
    }

    // Convert a desired GLOBAL transform into the LOCAL transform under `parentGlobal` (for
    // use_global_coordinates mode when the target sits inside a parent): local = parent^-1 * global.
    static math::Transform2D globalToLocal(const math::Transform2D& desiredGlobal,
                                           const math::Transform2D& parentGlobal) {
        return parentGlobal.affineInverse() * desiredGlobal;
    }

  private:
    bool m_position = true;
    bool m_rotation = true;
    bool m_scale = true;
    bool m_global = true;
};

} // namespace maz::scene
