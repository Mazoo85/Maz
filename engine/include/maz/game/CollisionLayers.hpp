#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <unordered_map>

namespace maz::game {

// Collision layers & masks — Godot's collision_layer / collision_mask. Overlap geometry (Area2D,
// Physics2D) answers "do these two shapes touch?"; layers answer the OTHER half every game needs:
// "should these two even be considered?". Each object lives in some LAYERS ("what I am": player,
// enemy, pickup, wall) and scans some MASK ("what I react to"). A pickup magnet scans only the pickup
// layer; an enemy hurtbox scans only the player layer; the player's bullets scan only enemies. Without
// this, every zone reacts to everything. It's pure bitmask logic — no allocation on the hot path — so
// it's deterministic and unit-tests headlessly. Godot 2D exposes 32 layers; LayerMask is a 32-bit set.

using LayerMask = uint32_t;

// Bit for layer `index` (0-based; index 0 is the editor's "Layer 1"). Out-of-range -> empty.
inline constexpr LayerMask layerBit(int index) {
    return (index >= 0 && index < 32) ? (static_cast<LayerMask>(1) << index) : 0u;
}

// Combine several layer indices into one mask.
inline LayerMask layerMask(std::initializer_list<int> indices) {
    LayerMask m = 0;
    for (int i : indices) {
        m |= layerBit(i);
    }
    return m;
}

// Directional test: does an observer scanning `observerMask` detect a body that lives in `targetLayer`?
// This is how a Godot Area2D / RayCast2D picks what it sees (observer.collision_mask & body.collision_layer).
inline bool detects(LayerMask observerMask, LayerMask targetLayer) {
    return (observerMask & targetLayer) != 0u;
}

// Symmetric test: do two objects interact — does EITHER scan the other's layer? This is how a physics
// broadphase decides to pair two bodies (Godot pairs when a.mask&b.layer OR b.mask&a.layer is nonzero).
inline bool interact(LayerMask aLayer, LayerMask aMask, LayerMask bLayer, LayerMask bMask) {
    return (aMask & bLayer) != 0u || (bMask & aLayer) != 0u;
}

// A body's / area's membership: `layer` = the layers it lives in ("what I am"); `mask` = the layers it
// scans ("what I react to"). Mirrors Godot's collision_layer / collision_mask on CollisionObject2D.
struct CollisionObject2D {
    LayerMask layer = layerBit(0);
    LayerMask mask = layerBit(0);

    void setLayerBit(int i, bool on) { layer = on ? (layer | layerBit(i)) : (layer & ~layerBit(i)); }
    void setMaskBit(int i, bool on) { mask = on ? (mask | layerBit(i)) : (mask & ~layerBit(i)); }
    bool layerHas(int i) const { return (layer & layerBit(i)) != 0u; }
    bool maskHas(int i) const { return (mask & layerBit(i)) != 0u; }

    // Does THIS object (an observer, using its mask) detect `other` (in its layer)?
    bool detects(const CollisionObject2D& other) const { return game::detects(mask, other.layer); }
    // Do this and `other` interact (either one scans the other's layer)?
    bool interactsWith(const CollisionObject2D& other) const {
        return game::interact(layer, mask, other.layer, other.mask);
    }
};

// Ergonomics: name the layers (Godot lets you name its 32 layers), so gameplay code reads
// `reg.bit("enemy")` instead of a magic bit. Names are assigned bit indices 0..31 in insertion order.
class LayerRegistry {
public:
    // Register `name` -> next free bit index (0..31), or return its existing index. -1 if full (>32).
    int add(const std::string& name) {
        auto it = m_index.find(name);
        if (it != m_index.end()) {
            return it->second;
        }
        const int idx = m_next < 32 ? m_next++ : -1;
        if (idx >= 0) {
            m_index[name] = idx;
        }
        return idx;
    }
    int index(const std::string& name) const {
        auto it = m_index.find(name);
        return it == m_index.end() ? -1 : it->second;
    }
    LayerMask bit(const std::string& name) const { return layerBit(index(name)); }
    LayerMask mask(std::initializer_list<const char*> names) const {
        LayerMask m = 0;
        for (const char* n : names) {
            m |= bit(n);
        }
        return m;
    }
    std::size_t size() const { return m_index.size(); }

private:
    std::unordered_map<std::string, int> m_index;
    int m_next = 0;
};

} // namespace maz::game
