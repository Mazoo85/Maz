#pragma once

#include "maz/render/Shapes.hpp"

#include <cstdint>
#include <vector>

namespace maz::render {

// A glTF model loaded into CPU data: merged geometry plus its base-color texture (if any).
struct ModelData {
    shapes::MeshData mesh;
    // Decoded RGBA8 base-color texture. Empty when the model has no material texture; the caller
    // then falls back to a flat/white texture and the mesh's vertex colors show through.
    std::vector<uint8_t> texturePixels;
    uint32_t textureWidth = 0;
    uint32_t textureHeight = 0;

    bool hasTexture() const { return textureWidth > 0 && textureHeight > 0; }
};

// Load a glTF 2.0 model (.gltf or .glb) into CPU data ready for the Renderer.
//
// All meshes in the file's scene are merged into one MeshData, with each node's world transform
// baked into positions and normals, so a multi-part model (walls + roof + door) loads as a single
// draw. Vertex attributes map to MeshVertex: POSITION (required), NORMAL (optional -> derived per
// face, else +Y), TEXCOORD_0 (optional -> 0,0), COLOR_0 (optional -> white). 16- and 32-bit index
// buffers are both accepted. The first material's base-color texture (embedded via a bufferView, or
// referenced as an external image file) is decoded into `texturePixels`. Returns false (and leaves
// `out` empty) on a missing/invalid file.
bool loadGltf(const char* path, ModelData& out);

} // namespace maz::render
