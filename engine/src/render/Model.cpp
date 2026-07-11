#include "maz/render/Model.hpp"

#include "maz/core/Log.hpp"

// cgltf is a single-header library; define its implementation exactly once here. Its C source
// trips our strict warning set, so silence those diagnostics just around the include.
#define CGLTF_IMPLEMENTATION
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <cgltf.h>
#pragma GCC diagnostic pop

#include <glm/gtc/type_ptr.hpp>
#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
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

} // namespace

bool loadGltf(const char* path, shapes::MeshData& out) {
    out.vertices.clear();
    out.indices.clear();

    cgltf_options options{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&options, path, &data) != cgltf_result_success) {
        MAZ_LOG_ERROR("glTF parse failed: %s", path);
        return false;
    }
    if (cgltf_load_buffers(&options, data, path) != cgltf_result_success) {
        MAZ_LOG_ERROR("glTF buffer load failed: %s", path);
        cgltf_free(data);
        return false;
    }

    // Merge every mesh in the file, baking each node's world transform into positions/normals so a
    // multi-part model loads as one mesh.
    for (cgltf_size n = 0; n < data->nodes_count; ++n) {
        const cgltf_node& node = data->nodes[n];
        if (!node.mesh) {
            continue;
        }
        cgltf_float world[16];
        cgltf_node_transform_world(&node, world);
        const glm::mat4 model = glm::make_mat4(world);
        const glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(model)));

        for (cgltf_size p = 0; p < node.mesh->primitives_count; ++p) {
            const cgltf_primitive& prim = node.mesh->primitives[p];
            if (prim.type != cgltf_primitive_type_triangles) {
                continue; // this loader only rasterizes triangle meshes
            }
            const cgltf_accessor* pos =
                findAttribute(prim, cgltf_attribute_type_position, 0);
            if (!pos) {
                continue; // positions are mandatory
            }
            const cgltf_accessor* nrm = findAttribute(prim, cgltf_attribute_type_normal, 0);
            const cgltf_accessor* uv = findAttribute(prim, cgltf_attribute_type_texcoord, 0);
            const cgltf_accessor* col = findAttribute(prim, cgltf_attribute_type_color, 0);

            const auto vertBase = static_cast<uint32_t>(out.vertices.size());
            const cgltf_size vcount = pos->count;
            const bool haveNormals = nrm != nullptr;

            for (cgltf_size i = 0; i < vcount; ++i) {
                glm::vec3 pv(0.0f);
                cgltf_accessor_read_float(pos, i, glm::value_ptr(pv), 3);
                pv = glm::vec3(model * glm::vec4(pv, 1.0f));

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

                out.vertices.push_back(MeshVertex{pv.x, pv.y, pv.z, nv.x, nv.y, nv.z, rgba[0],
                                                  rgba[1], rgba[2], uvw[0], uvw[1]});
            }

            // Indices: honor the primitive's index buffer, or synthesize a sequential one.
            const cgltf_size icount = prim.indices ? prim.indices->count : vcount;
            const auto idxBase = static_cast<uint32_t>(out.indices.size());
            for (cgltf_size i = 0; i < icount; ++i) {
                const cgltf_size idx =
                    prim.indices ? cgltf_accessor_read_index(prim.indices, i) : i;
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
    }

    cgltf_free(data);

    if (out.vertices.empty()) {
        MAZ_LOG_ERROR("glTF has no triangle geometry: %s", path);
        return false;
    }
    MAZ_LOG_INFO("glTF loaded: %s (%zu verts, %zu indices)", path, out.vertices.size(),
                 out.indices.size());
    return true;
}

} // namespace maz::render
