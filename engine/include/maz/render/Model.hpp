#pragma once

#include "maz/render/Shapes.hpp"

namespace maz::render {

// Load a glTF 2.0 model (.gltf or .glb) into CPU geometry ready for Renderer::createMesh.
//
// All meshes in the file's scene are merged into one MeshData, with each node's world transform
// baked into positions and normals, so a multi-part model (walls + roof + door) loads as a single
// draw. Vertex attributes map to MeshVertex: POSITION (required), NORMAL (optional -> derived per
// face, else +Y), TEXCOORD_0 (optional -> 0,0), COLOR_0 (optional -> white). 16- and 32-bit index
// buffers are both accepted. Returns false (and leaves `out` empty) on a missing/invalid file.
bool loadGltf(const char* path, shapes::MeshData& out);

} // namespace maz::render
