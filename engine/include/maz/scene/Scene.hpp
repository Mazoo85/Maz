#pragma once

#include "maz/math/Math.hpp"

#include <string>
#include <vector>

// The scene graph the editor edits and the engine renders: a flat list of named entities, each
// with a transform and an optional model to draw. Deliberately simple and serializable — this is
// the data the visual editor's Hierarchy and Inspector panels operate on.

namespace maz::scene {

// Position / rotation / scale. Rotation is stored as Euler angles in degrees (artist-friendly and
// round-trips cleanly through the scene file); matrix() composes them as T * R * S.
struct Transform {
    math::vec3 position{0.0f, 0.0f, 0.0f};
    math::vec3 rotationEuler{0.0f, 0.0f, 0.0f}; // degrees, applied Y then X then Z
    math::vec3 scale{1.0f, 1.0f, 1.0f};

    math::mat4 matrix() const;
};

// One thing in the scene. `modelPath` is a glTF/GLB path relative to the working directory, or
// empty for an entity with no visual (e.g. a future light or spawn point).
struct Entity {
    std::string name = "Entity";
    Transform transform;
    std::string modelPath;
};

struct Scene {
    std::string name = "Untitled Scene";
    std::vector<Entity> entities;
};

// Save/load a scene to a small human-readable text format (.mazscene). Return false and set
// `error` (if non-null) on failure. See docs/EDITOR.md for the format.
bool saveScene(const std::string& path, const Scene& scene, std::string* error = nullptr);
bool loadScene(const std::string& path, Scene& scene, std::string* error = nullptr);

} // namespace maz::scene
