#include "maz/render/Model.hpp"

#include "maz/core/Log.hpp"

// cgltf and stb_image are single-header libraries whose C source trips our strict warning set, so
// silence those diagnostics just around their includes. cgltf's implementation is defined here;
// stb_image's implementation already lives in VulkanTexture.cpp, so we include declarations only.
#define CGLTF_IMPLEMENTATION
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <cgltf.h>
#include <stb_image.h>
#pragma GCC diagnostic pop

#include <glm/gtc/type_ptr.hpp>
#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace maz::render {

namespace {

// Find the accessor for a given attribute set on a primitive (e.g. TEXCOORD_0 -> set 0). Returns
// nullptr when the primitive has no such attribute.
const cgltf_accessor* findAttribute(const cgltf_primitive& prim, cgltf_attribute_type type,
                                    int set) {
    for (cgltf_size a = 0; a < prim.attributes_count; ++a) {
        const cgltf_attribute& attr = prim.attributes[a];
        if (attr.type == type && attr.index == set) {
            return attr.data;
        }
    }
    return nullptr;
}

// Append one triangle primitive's geometry to `out`, transforming positions by `xform` (and normals
// by its inverse-transpose). Missing normals are derived flat from the faces; missing UVs/colors
// default to (0,0) and white. Non-triangle primitives are skipped.
void appendPrimitive(const cgltf_primitive& prim, const glm::mat4& xform, shapes::MeshData& out) {
    if (prim.type != cgltf_primitive_type_triangles) {
        return;
    }
    const cgltf_accessor* pos = findAttribute(prim, cgltf_attribute_type_position, 0);
    if (!pos) {
        return; // positions are mandatory
    }
    const cgltf_accessor* nrm = findAttribute(prim, cgltf_attribute_type_normal, 0);
    const cgltf_accessor* uv = findAttribute(prim, cgltf_attribute_type_texcoord, 0);
    const cgltf_accessor* col = findAttribute(prim, cgltf_attribute_type_color, 0);

    const glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(xform)));
    const auto vertBase = static_cast<uint32_t>(out.vertices.size());
    const cgltf_size vcount = pos->count;
    const bool haveNormals = nrm != nullptr;

    for (cgltf_size i = 0; i < vcount; ++i) {
        glm::vec3 pv(0.0f);
        cgltf_accessor_read_float(pos, i, glm::value_ptr(pv), 3);
        pv = glm::vec3(xform * glm::vec4(pv, 1.0f));

        glm::vec3 nv(0.0f, 1.0f, 0.0f);
        if (haveNormals) {
            cgltf_accessor_read_float(nrm, i, glm::value_ptr(nv), 3);
            nv = glm::normalize(normalMat * nv);
        }

        float uvw[2] = {0.0f, 0.0f};
        if (uv) {
            cgltf_accessor_read_float(uv, i, uvw, 2);
        }

        float rgba[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        if (col) {
            cgltf_accessor_read_float(col, i, rgba, 4);
        }

        out.vertices.push_back(MeshVertex{pv.x, pv.y, pv.z, nv.x, nv.y, nv.z, rgba[0], rgba[1],
                                          rgba[2], uvw[0], uvw[1]});
    }

    const cgltf_size icount = prim.indices ? prim.indices->count : vcount;
    const auto idxBase = static_cast<uint32_t>(out.indices.size());
    for (cgltf_size i = 0; i < icount; ++i) {
        const cgltf_size idx = prim.indices ? cgltf_accessor_read_index(prim.indices, i) : i;
        out.indices.push_back(vertBase + static_cast<uint32_t>(idx));
    }

    // Derive flat normals for a primitive that shipped without them.
    if (!haveNormals) {
        for (cgltf_size t = idxBase; t + 2 < out.indices.size(); t += 3) {
            MeshVertex& a = out.vertices[out.indices[t + 0]];
            MeshVertex& b = out.vertices[out.indices[t + 1]];
            MeshVertex& c = out.vertices[out.indices[t + 2]];
            const glm::vec3 pa(a.px, a.py, a.pz);
            const glm::vec3 pb(b.px, b.py, b.pz);
            const glm::vec3 pc(c.px, c.py, c.pz);
            const glm::vec3 fn = glm::normalize(glm::cross(pb - pa, pc - pa));
            for (MeshVertex* v : {&a, &b, &c}) {
                v->nx = fn.x;
                v->ny = fn.y;
                v->nz = fn.z;
            }
        }
    }
}

// Decode a material's base-color texture into RGBA8. Handles textures embedded via a bufferView
// (the bytes live in an already-loaded buffer) and external image files referenced by a relative
// URI. Returns false when there is no usable base-color texture.
bool decodeBaseColor(const cgltf_material* mat, const char* gltfPath, std::vector<uint8_t>& outPx,
                     uint32_t& outW, uint32_t& outH) {
    if (!mat || !mat->has_pbr_metallic_roughness ||
        !mat->pbr_metallic_roughness.base_color_texture.texture ||
        !mat->pbr_metallic_roughness.base_color_texture.texture->image) {
        return false;
    }
    const cgltf_image* image = mat->pbr_metallic_roughness.base_color_texture.texture->image;

    int w = 0, h = 0, comp = 0;
    stbi_uc* pixels = nullptr;
    if (image->buffer_view) {
        const cgltf_buffer_view* bv = image->buffer_view;
        const auto* bytes = static_cast<const stbi_uc*>(bv->buffer->data) + bv->offset;
        pixels = stbi_load_from_memory(bytes, static_cast<int>(bv->size), &w, &h, &comp, 4);
    } else if (image->uri && std::strncmp(image->uri, "data:", 5) != 0) {
        std::string dir(gltfPath);
        const size_t slash = dir.find_last_of("/\\");
        dir = (slash == std::string::npos) ? std::string() : dir.substr(0, slash + 1);
        pixels = stbi_load((dir + image->uri).c_str(), &w, &h, &comp, 4);
    }
    if (!pixels) {
        MAZ_LOG_WARN("glTF base-color texture could not be decoded");
        return false;
    }
    outW = static_cast<uint32_t>(w);
    outH = static_cast<uint32_t>(h);
    outPx.assign(pixels, pixels + static_cast<size_t>(w) * h * 4);
    stbi_image_free(pixels);
    return true;
}

// First material referenced by any of a node's mesh primitives (nullptr if none).
const cgltf_material* firstMaterial(const cgltf_mesh* mesh) {
    for (cgltf_size p = 0; p < mesh->primitives_count; ++p) {
        if (mesh->primitives[p].material) {
            return mesh->primitives[p].material;
        }
    }
    return nullptr;
}

// Parse + load buffers, returning the cgltf_data (or nullptr with a logged error).
cgltf_data* openGltf(const char* path) {
    cgltf_options options{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&options, path, &data) != cgltf_result_success) {
        MAZ_LOG_ERROR("glTF parse failed: %s", path);
        return nullptr;
    }
    if (cgltf_load_buffers(&options, data, path) != cgltf_result_success) {
        MAZ_LOG_ERROR("glTF buffer load failed: %s", path);
        cgltf_free(data);
        return nullptr;
    }
    return data;
}

} // namespace

bool loadGltf(const char* path, ModelData& out) {
    out = ModelData{};
    cgltf_data* data = openGltf(path);
    if (!data) {
        return false;
    }

    // Merge every mesh, baking each node's world transform into positions/normals.
    const cgltf_material* texMat = nullptr;
    for (cgltf_size n = 0; n < data->nodes_count; ++n) {
        const cgltf_node& node = data->nodes[n];
        if (!node.mesh) {
            continue;
        }
        cgltf_float world[16];
        cgltf_node_transform_world(&node, world);
        const glm::mat4 model = glm::make_mat4(world);
        for (cgltf_size p = 0; p < node.mesh->primitives_count; ++p) {
            appendPrimitive(node.mesh->primitives[p], model, out.mesh);
        }
        if (!texMat) {
            texMat = firstMaterial(node.mesh);
        }
    }

    if (texMat) {
        decodeBaseColor(texMat, path, out.texturePixels, out.textureWidth, out.textureHeight);
    }
    cgltf_free(data);

    if (out.mesh.vertices.empty()) {
        MAZ_LOG_ERROR("glTF has no triangle geometry: %s", path);
        return false;
    }
    MAZ_LOG_INFO("glTF loaded: %s (%zu verts, %zu indices, texture %ux%u)", path,
                 out.mesh.vertices.size(), out.mesh.indices.size(), out.textureWidth,
                 out.textureHeight);
    return true;
}

bool loadGltfScene(const char* path, SceneData& out) {
    out = SceneData{};
    cgltf_data* data = openGltf(path);
    if (!data) {
        return false;
    }

    // One SceneNode per node-with-mesh: geometry kept in local space, transform stored separately.
    for (cgltf_size n = 0; n < data->nodes_count; ++n) {
        const cgltf_node& node = data->nodes[n];
        if (!node.mesh) {
            continue;
        }
        SceneNode sn{};
        for (cgltf_size p = 0; p < node.mesh->primitives_count; ++p) {
            appendPrimitive(node.mesh->primitives[p], glm::mat4(1.0f), sn.mesh);
        }
        if (sn.mesh.vertices.empty()) {
            continue;
        }
        cgltf_node_transform_world(&node, sn.model);
        decodeBaseColor(firstMaterial(node.mesh), path, sn.texturePixels, sn.textureWidth,
                        sn.textureHeight);
        out.nodes.push_back(std::move(sn));
    }
    cgltf_free(data);

    if (out.nodes.empty()) {
        MAZ_LOG_ERROR("glTF scene has no drawable nodes: %s", path);
        return false;
    }
    MAZ_LOG_INFO("glTF scene loaded: %s (%zu nodes)", path, out.nodes.size());
    return true;
}

} // namespace maz::render
