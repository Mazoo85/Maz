#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// glTF model loading — the engine side of the Blender pipeline.
//
// Blender exports a mesh as glTF 2.0 (.gltf/.glb); loadModel() reads it into an
// engine-friendly CPU representation: interleaved vertices + a triangle index list,
// ready to be uploaded to a GPU buffer once the mesh renderer lands. See
// docs/BLENDER_PIPELINE.md for the full workflow.

namespace maz::assets {

// One interleaved vertex. POD and tightly packed on purpose so a mesh's vertex
// vector can be memcpy'd straight into a Vulkan vertex buffer later.
struct Vertex {
    float position[3];
    float normal[3];
    float uv[2];
};

// A single mesh primitive: a triangle list indexing into `vertices`.
struct Mesh {
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

// Axis-aligned bounds over every mesh, in model space.
struct Bounds {
    float min[3]{0.0f, 0.0f, 0.0f};
    float max[3]{0.0f, 0.0f, 0.0f};
};

// A model loaded from a glTF/GLB file: its meshes plus overall bounds.
struct Model {
    std::string sourcePath;
    std::vector<Mesh> meshes;
    Bounds bounds;

    std::size_t vertexCount() const;
    std::size_t indexCount() const;
    std::size_t triangleCount() const;
};

// Load a glTF (.gltf) or binary glTF (.glb) file into `out`.
//
// Returns true on success. On failure returns false and, if `error` is non-null,
// fills it with a human-readable reason. Only triangle primitives are imported;
// positions are always present, while missing normals/UVs default to zero.
bool loadModel(const std::string& path, Model& out, std::string* error = nullptr);

} // namespace maz::assets
