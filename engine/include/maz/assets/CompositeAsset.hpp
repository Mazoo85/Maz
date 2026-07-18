#pragma once

#include "maz/assets/Model.hpp"
#include "maz/assets/Primitives.hpp"
#include "maz/scene/Scene.hpp" // for scene::Transform

#include <string>
#include <vector>

// A composite asset: the document the character/item creator edits. It is a list of parametric
// primitive "parts", each positioned by its own transform, plus a kind tag (character vs item).
// buildModel() bakes the parts into a single renderable assets::Model, so a baked asset draws
// through the exact same uploadModel/drawModel path as a glTF file. The document itself round-trips
// to a small human-readable .mazasset text file (mirrors .mazscene; see docs/EDITOR.md).

namespace maz::assets {

// What the asset represents. Serialized by name, so append new kinds rather than inserting.
enum class AssetKind { Item, Character };

// One primitive placed in the asset's local space by `local`.
struct Part {
    std::string name = "Part";
    PrimitiveKind kind = PrimitiveKind::Box;
    PrimitiveParams params;
    scene::Transform local; // Phase 1: keep scale uniform so baked normals stay correct.
};

// The editable document: a named, kinded collection of parts.
struct AssetDoc {
    std::string name = "Untitled";
    AssetKind kind = AssetKind::Item;
    std::vector<Part> parts;
};

// Bake the document into one Model: every part's primitive is generated, transformed into asset
// space by its `local` transform, and merged into a single mesh, with overall bounds recomputed.
Model buildModel(const AssetDoc& doc);

// Save/load the document as .mazasset. Return false and set `error` (if non-null) on failure.
bool saveAsset(const std::string& path, const AssetDoc& doc, std::string* error = nullptr);
bool loadAsset(const std::string& path, AssetDoc& doc, std::string* error = nullptr);

} // namespace maz::assets
