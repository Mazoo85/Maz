#pragma once

#include "maz/scene/Prefab.hpp" // scene::PropBag, scene::PropValue + typed getters/setters

#include <string>

// ASSET DEFINITION — the data that turns a composed object (a tree of primitive parts, see
// editor::Composite / scene::Prefab) into a game *character* or *item* with typed stats.
//
// Rather than invent a new file format, an AssetDef serializes into a scene::PropBag — the same typed
// key/value bag a prefab node already carries — so it lives on the ROOT node of the asset's prefab
// (editor::sceneToPrefab takes the root's props). One file, one parser: io::savePrefabText writes the
// kind + stats right next to the part tree, and loadPrefabText reads them back. Pure data, GPU-free,
// unit-tested headlessly.
namespace maz::game {

// What the composed asset represents. Item is the default so an asset with no kind set reads as a
// plain item (the common case); Character marks a playable/NPC actor.
enum class AssetKind { Item, Character };

inline const char* assetKindName(AssetKind k) {
    return k == AssetKind::Character ? "character" : "item";
}
inline AssetKind assetKindFromName(const std::string& s) {
    return s == "character" ? AssetKind::Character : AssetKind::Item;
}

// A character/item definition: a display name, a kind, and a bag of typed stats (hp, damage, heal,
// weight, …) reusing scene::PropValue so stats can be Float/Int/Bool/Vec2/Color/Text.
struct AssetDef {
    std::string name = "Untitled";
    AssetKind kind = AssetKind::Item;
    scene::PropBag stats;

    // Typed stat accessors (thin wrappers over the scene:: helpers, defaulting when absent).
    float statF(const std::string& key, float def = 0.0f) const {
        return scene::getFloat(stats, key, def);
    }
    int statI(const std::string& key, int def = 0) const { return scene::getInt(stats, key, def); }
    bool statB(const std::string& key, bool def = false) const {
        return scene::getBool(stats, key, def);
    }
    void setStatF(const std::string& key, float v) {
        scene::setProp(stats, key, scene::PropValue::makeFloat(v));
    }
    void setStatI(const std::string& key, int v) {
        scene::setProp(stats, key, scene::PropValue::makeInt(v));
    }
    void setStatB(const std::string& key, bool v) {
        scene::setProp(stats, key, scene::PropValue::makeBool(v));
    }
};

// The property key the kind is stored under on the prefab root. Double-underscored so it cannot clash
// with a user stat name; treated as reserved by the round-trip below.
inline const char* kKindKey() { return "__kind__"; }

// Flatten a definition into a prefab-root property bag: every stat verbatim, plus the reserved kind
// key. (The asset's display name is carried by the prefab root NODE's name, set by the caller, so it
// is not duplicated here.)
inline scene::PropBag assetDefToProps(const AssetDef& def) {
    scene::PropBag bag = def.stats;
    scene::setProp(bag, kKindKey(), scene::PropValue::makeText(assetKindName(def.kind)));
    return bag;
}

// Inverse: read the kind from the reserved key and treat every other property as a stat. `name` is
// filled by the caller from the prefab root node's name (it is not in the bag).
inline AssetDef assetDefFromProps(const scene::PropBag& props) {
    AssetDef def;
    for (const auto& kv : props) {
        if (kv.first == kKindKey()) {
            def.kind =
                assetKindFromName(kv.second.type == scene::PropValue::Type::Text ? kv.second.text
                                                                                 : std::string());
        } else {
            scene::setProp(def.stats, kv.first, kv.second);
        }
    }
    return def;
}

} // namespace maz::game
