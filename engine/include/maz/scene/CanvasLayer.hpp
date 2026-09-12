#pragma once

#include "maz/math/Transform2D.hpp" // math::Transform2D, math::vec2

#include <algorithm>
#include <cstddef>
#include <vector>

// maz::scene CanvasLayer — Godot's CanvasLayer node: an independent 2D drawing layer with its own
// transform and a draw-order index, used to pin HUDs/UI to the screen (unaffected by the world camera)
// or to build parallax-free overlays. Its children draw through the layer's transform (offset / rotation
// / scale). By default the layer is screen-fixed; with follow_viewport enabled it instead tracks the
// world/viewport canvas transform, optionally scaled. A CanvasLayerStack keeps several layers ordered by
// their `layer` index (low draws first, behind). Pure transform maths on math::Transform2D, header-only,
// deterministic — the renderer consumes finalTransform()/drawOrder() to place and order the layers.
namespace maz::scene {

class CanvasLayer {
  public:
    void setLayer(int layer) { m_layer = layer; }
    int layer() const { return m_layer; }

    void setVisible(bool v) { m_visible = v; }
    bool visible() const { return m_visible; }

    void setOffset(math::vec2 o) { m_offset = o; }
    math::vec2 offset() const { return m_offset; }
    void setRotation(float r) { m_rotation = r; }
    float rotation() const { return m_rotation; }
    void setScale(math::vec2 s) { m_scale = s; }
    math::vec2 scale() const { return m_scale; }

    // follow_viewport: when off (default) the layer is screen-fixed; when on it tracks the world/
    // viewport canvas transform (scaled by follow_viewport_scale).
    void setFollowViewport(bool on) { m_follow = on; }
    bool followViewport() const { return m_follow; }
    void setFollowViewportScale(float s) { m_followScale = s; }
    float followViewportScale() const { return m_followScale; }

    // The layer's own transform (Godot Transform2D(rotation, scale, offset)).
    math::Transform2D transform() const {
        return math::Transform2D::compose(m_rotation, m_scale, m_offset);
    }

    // The transform children actually draw through, given the viewport's canvas transform (the world
    // camera). Screen-fixed when not following; otherwise the layer transform composed with the
    // viewport transform scaled by follow_viewport_scale.
    math::Transform2D finalTransform(const math::Transform2D& viewportCanvas) const {
        if (!m_follow) {
            return transform();
        }
        const math::Transform2D scaled{viewportCanvas.x * m_followScale,
                                       viewportCanvas.y * m_followScale,
                                       viewportCanvas.origin * m_followScale};
        return transform() * scaled;
    }

  private:
    int m_layer = 0;
    bool m_visible = true;
    math::vec2 m_offset{0.0f, 0.0f};
    float m_rotation = 0.0f;
    math::vec2 m_scale{1.0f, 1.0f};
    bool m_follow = false;
    float m_followScale = 1.0f;
};

// Orders several CanvasLayers by their layer index for drawing (low first / behind). Godot's default
// world sits at layer 0; overlays use higher indices, backgrounds negative.
class CanvasLayerStack {
  public:
    // Register a layer (non-owning). Returns its handle index for later reference.
    std::size_t add(CanvasLayer* layer) {
        m_layers.push_back(layer);
        return m_layers.size() - 1;
    }
    std::size_t count() const { return m_layers.size(); }
    void clear() { m_layers.clear(); }

    // Layers sorted ascending by `layer` index, stable within equal indices (insertion order kept).
    // Only visible layers are included.
    std::vector<CanvasLayer*> drawOrder() const {
        std::vector<CanvasLayer*> out;
        out.reserve(m_layers.size());
        for (CanvasLayer* l : m_layers) {
            if (l && l->visible()) {
                out.push_back(l);
            }
        }
        std::stable_sort(out.begin(), out.end(),
                         [](const CanvasLayer* a, const CanvasLayer* b) { return a->layer() < b->layer(); });
        return out;
    }

  private:
    std::vector<CanvasLayer*> m_layers;
};

} // namespace maz::scene
