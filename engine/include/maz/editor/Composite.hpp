#pragma once

#include "maz/editor/Scene.hpp"          // editor::Scene, editor::Node
#include "maz/math/Math.hpp"             // math::vec3, math::mat4
#include "maz/render/MeshMerge.hpp"      // render::mergeMeshes
#include "maz/render/MeshTransform.hpp"  // render::applyTransform
#include "maz/render/Shapes.hpp"         // render::shapes::MeshData
#include "maz/scene/Prefab.hpp"          // scene::Prefab, scene::PrefabNode, scene::PropBag

#include <cstddef>
#include <string>
#include <vector>

// COMPOSITE AUTHORING — turn the editor's placed nodes into a single saved, reusable object.
//
// The editor (apps/editor) lets you place instances of a fixed palette of primitive meshes, each with
// its own transform and material. This header adds the two operations that make that a *creator* for
// characters and items rather than just a scene arranger, both pure CPU / GPU-free so they unit-test
// headlessly:
//
//   bakeComposite()  — flatten every visible node into ONE mesh (via render::applyTransform to bake
//                      each node's transform, then render::mergeMeshes to concatenate), so a composed
//                      character/item can be drawn in a single draw call or exported as one object.
//   sceneToPrefab()  — serialize the node list as a scene::Prefab tree (root = the asset, one child
//   prefabToScene()    per part), so it round-trips through io::savePrefabText / loadPrefabText (the
//                      engine's .tscn-style text resource) and can carry asset metadata on the root.
//
// Nothing here is new geometry or a new format: it reuses render::shapes, render::applyTransform,
// render::mergeMeshes, and scene::Prefab. A Node's meshId is an index into the caller-supplied palette
// (the same palette the editor uploads), exactly as editor::toJson already persists it.
namespace maz::editor {

// --- Bake -------------------------------------------------------------------------------------------

// Flatten the scene's visible nodes into one mesh. For each visible node whose meshId indexes `palette`,
// the palette mesh is transformed by the node's model matrix (positions + inverse-transpose normals)
// and merged into the result. When `tintColors` is non-null and a node's colorIndex addresses it, that
// node's vertices are tinted by the swatch colour (so a single merged mesh keeps its per-part colours in
// vertex colour, since the merged object no longer has per-part textures). Nodes with an out-of-range
// meshId are skipped. The result is empty when nothing qualifies.
inline render::shapes::MeshData bakeComposite(const Scene& scene,
                                              const std::vector<render::shapes::MeshData>& palette,
                                              const std::vector<math::vec3>* tintColors = nullptr) {
    std::vector<render::shapes::MeshData> parts;
    parts.reserve(scene.nodes.size());
    for (const Node& n : scene.nodes) {
        if (!n.visible) {
            continue;
        }
        const std::size_t mesh = static_cast<std::size_t>(n.meshId);
        if (mesh >= palette.size()) {
            continue;
        }
        render::shapes::MeshData part = render::applyTransform(palette[mesh], n.modelMatrix());
        if (tintColors && n.colorIndex >= 0 &&
            static_cast<std::size_t>(n.colorIndex) < tintColors->size()) {
            const math::vec3& c = (*tintColors)[static_cast<std::size_t>(n.colorIndex)];
            for (render::MeshVertex& v : part.vertices) {
                v.r = c.x;
                v.g = c.y;
                v.b = c.z;
            }
        }
        parts.push_back(std::move(part));
    }
    return render::mergeMeshes(parts);
}

// --- Prefab round-trip ------------------------------------------------------------------------------

namespace detail {

inline void setVec3(scene::PropBag& bag, const std::string& key, const math::vec3& v) {
    scene::setProp(bag, key + "x", scene::PropValue::makeFloat(v.x));
    scene::setProp(bag, key + "y", scene::PropValue::makeFloat(v.y));
    scene::setProp(bag, key + "z", scene::PropValue::makeFloat(v.z));
}

inline math::vec3 getVec3(const scene::PropBag& bag, const std::string& key,
                          const math::vec3& def = math::vec3(0.0f)) {
    return math::vec3(scene::getFloat(bag, key + "x", def.x), scene::getFloat(bag, key + "y", def.y),
                      scene::getFloat(bag, key + "z", def.z));
}

// Serialize one editor Node into a prefab child node's property bag (every editable field).
inline scene::PrefabNode nodeToPrefab(const Node& n) {
    scene::PrefabNode pn;
    pn.name = n.name;
    scene::setProp(pn.props, "mesh", scene::PropValue::makeInt(static_cast<int>(n.meshId)));
    scene::setProp(pn.props, "color", scene::PropValue::makeInt(n.colorIndex));
    scene::setProp(pn.props, "visible", scene::PropValue::makeBool(n.visible));
    setVec3(pn.props, "pos", n.position);
    setVec3(pn.props, "rot", n.euler);
    setVec3(pn.props, "scale", n.scale);
    setVec3(pn.props, "lmin", n.localMin);
    setVec3(pn.props, "lmax", n.localMax);
    setVec3(pn.props, "em", n.emissive);
    scene::setProp(pn.props, "rough", scene::PropValue::makeFloat(n.roughness));
    scene::setProp(pn.props, "metal", scene::PropValue::makeFloat(n.metallic));
    scene::setProp(pn.props, "spec", scene::PropValue::makeFloat(n.specular));
    return pn;
}

// Rebuild an editor Node from a prefab child node (defaults match the Node struct for missing keys).
inline Node prefabToNode(const scene::PrefabNode& pn) {
    Node n;
    n.name = pn.name;
    n.meshId = static_cast<uint32_t>(scene::getInt(pn.props, "mesh", 0));
    n.colorIndex = scene::getInt(pn.props, "color", 0);
    n.visible = scene::getBool(pn.props, "visible", true);
    n.position = getVec3(pn.props, "pos");
    n.euler = getVec3(pn.props, "rot");
    n.scale = getVec3(pn.props, "scale", math::vec3(1.0f));
    n.localMin = getVec3(pn.props, "lmin", math::vec3(-0.5f));
    n.localMax = getVec3(pn.props, "lmax", math::vec3(0.5f));
    n.emissive = getVec3(pn.props, "em");
    n.roughness = scene::getFloat(pn.props, "rough", 0.6f);
    n.metallic = scene::getFloat(pn.props, "metal", 0.0f);
    n.specular = scene::getFloat(pn.props, "spec", 0.0f);
    return n;
}

} // namespace detail

// Build a prefab whose root is the asset (named `name`, carrying any `rootProps` the caller supplies —
// e.g. a game::AssetDef's kind + stats) and whose children are the scene's nodes, one per part. The
// resulting Prefab serializes with io::savePrefabText and round-trips with prefabToScene below.
inline scene::Prefab sceneToPrefab(const Scene& scene, const std::string& name,
                                   const scene::PropBag& rootProps = {}) {
    scene::Prefab prefab;
    prefab.root.name = name;
    prefab.root.props = rootProps;
    prefab.root.children.reserve(scene.nodes.size());
    for (const Node& n : scene.nodes) {
        prefab.root.children.push_back(detail::nodeToPrefab(n));
    }
    return prefab;
}

// Inverse of sceneToPrefab: replace `out`'s nodes with the prefab root's children. The root's own
// property bag (asset metadata) is left to the caller to read. Selection is reset.
inline void prefabToScene(const scene::Prefab& prefab, Scene& out) {
    out.nodes.clear();
    out.nodes.reserve(prefab.root.children.size());
    for (const scene::PrefabNode& child : prefab.root.children) {
        out.nodes.push_back(detail::prefabToNode(child));
    }
    out.clearSelection();
}

} // namespace maz::editor
