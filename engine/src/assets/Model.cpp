#include "maz/assets/Model.hpp"

#include "maz/core/Log.hpp"

// cgltf is a single-header C99 glTF 2.0 parser (vendored under third_party). Its
// implementation is compiled separately in cgltf_impl.c (with relaxed warnings, since
// the engine builds first-party code with -Werror); here we only need the declarations.
#include "cgltf.h"

#include <cfloat>
#include <cstring>

namespace maz::assets {

std::size_t Model::vertexCount() const {
    std::size_t n = 0;
    for (const Mesh& m : meshes) {
        n += m.vertices.size();
    }
    return n;
}

std::size_t Model::indexCount() const {
    std::size_t n = 0;
    for (const Mesh& m : meshes) {
        n += m.indices.size();
    }
    return n;
}

std::size_t Model::triangleCount() const { return indexCount() / 3; }

namespace {

// Read `count` vec-N attributes out of an accessor into a flat float buffer.
// Returns false if the accessor is missing or the read fails.
bool readAttribute(const cgltf_accessor* accessor, std::vector<float>& out, cgltf_size components) {
    if (accessor == nullptr) {
        return false;
    }
    out.assign(accessor->count * components, 0.0f);
    for (cgltf_size i = 0; i < accessor->count; ++i) {
        if (!cgltf_accessor_read_float(accessor, i, &out[i * components], components)) {
            return false;
        }
    }
    return true;
}

const cgltf_accessor* findAttribute(const cgltf_primitive* prim, cgltf_attribute_type type) {
    for (cgltf_size i = 0; i < prim->attributes_count; ++i) {
        if (prim->attributes[i].type == type) {
            return prim->attributes[i].data;
        }
    }
    return nullptr;
}

void expandBounds(Bounds& b, const float p[3], bool& seeded) {
    if (!seeded) {
        for (int k = 0; k < 3; ++k) {
            b.min[k] = b.max[k] = p[k];
        }
        seeded = true;
        return;
    }
    for (int k = 0; k < 3; ++k) {
        if (p[k] < b.min[k]) b.min[k] = p[k];
        if (p[k] > b.max[k]) b.max[k] = p[k];
    }
}

void fail(std::string* error, const std::string& msg) {
    if (error != nullptr) {
        *error = msg;
    }
    MAZ_LOG_ERROR("loadModel: %s", msg.c_str());
}

} // namespace

bool loadModel(const std::string& path, Model& out, std::string* error) {
    out = Model{};
    out.sourcePath = path;

    cgltf_options options{};
    cgltf_data* data = nullptr;

    cgltf_result res = cgltf_parse_file(&options, path.c_str(), &data);
    if (res != cgltf_result_success) {
        fail(error, "could not parse '" + path + "' (is it valid glTF/GLB?)");
        return false;
    }

    // Loads .bin buffers and decodes embedded base64 data URIs.
    res = cgltf_load_buffers(&options, data, path.c_str());
    if (res != cgltf_result_success) {
        cgltf_free(data);
        fail(error, "could not load buffers for '" + path + "'");
        return false;
    }

    if (cgltf_validate(data) != cgltf_result_success) {
        cgltf_free(data);
        fail(error, "glTF failed validation: '" + path + "'");
        return false;
    }

    bool boundsSeeded = false;

    for (cgltf_size mi = 0; mi < data->meshes_count; ++mi) {
        const cgltf_mesh& gmesh = data->meshes[mi];
        for (cgltf_size pi = 0; pi < gmesh.primitives_count; ++pi) {
            const cgltf_primitive& prim = gmesh.primitives[pi];
            if (prim.type != cgltf_primitive_type_triangles) {
                MAZ_LOG_WARN("loadModel: skipping non-triangle primitive in mesh '%s'",
                             gmesh.name ? gmesh.name : "<unnamed>");
                continue;
            }

            const cgltf_accessor* posAcc = findAttribute(&prim, cgltf_attribute_type_position);
            if (posAcc == nullptr) {
                MAZ_LOG_WARN("loadModel: primitive without POSITION; skipping");
                continue;
            }
            const cgltf_accessor* nrmAcc = findAttribute(&prim, cgltf_attribute_type_normal);
            const cgltf_accessor* uvAcc = findAttribute(&prim, cgltf_attribute_type_texcoord);

            std::vector<float> pos, nrm, uv;
            if (!readAttribute(posAcc, pos, 3)) {
                cgltf_free(data);
                fail(error, "failed reading POSITION accessor in '" + path + "'");
                return false;
            }
            readAttribute(nrmAcc, nrm, 3); // optional
            readAttribute(uvAcc, uv, 2);   // optional

            Mesh mesh;
            mesh.name = gmesh.name ? gmesh.name : "";
            const cgltf_size vcount = posAcc->count;
            mesh.vertices.resize(vcount);
            for (cgltf_size v = 0; v < vcount; ++v) {
                Vertex& vert = mesh.vertices[v];
                vert.position[0] = pos[v * 3 + 0];
                vert.position[1] = pos[v * 3 + 1];
                vert.position[2] = pos[v * 3 + 2];
                if (!nrm.empty()) {
                    vert.normal[0] = nrm[v * 3 + 0];
                    vert.normal[1] = nrm[v * 3 + 1];
                    vert.normal[2] = nrm[v * 3 + 2];
                } else {
                    vert.normal[0] = vert.normal[1] = vert.normal[2] = 0.0f;
                }
                if (!uv.empty()) {
                    vert.uv[0] = uv[v * 2 + 0];
                    vert.uv[1] = uv[v * 2 + 1];
                } else {
                    vert.uv[0] = vert.uv[1] = 0.0f;
                }
                expandBounds(out.bounds, vert.position, boundsSeeded);
            }

            // Indices. glTF primitives may omit them (implicit 0,1,2,...).
            if (prim.indices != nullptr) {
                const cgltf_size icount = prim.indices->count;
                mesh.indices.resize(icount);
                for (cgltf_size i = 0; i < icount; ++i) {
                    mesh.indices[i] = static_cast<uint32_t>(cgltf_accessor_read_index(prim.indices, i));
                }
            } else {
                mesh.indices.resize(vcount);
                for (cgltf_size i = 0; i < vcount; ++i) {
                    mesh.indices[i] = static_cast<uint32_t>(i);
                }
            }

            out.meshes.push_back(std::move(mesh));
        }
    }

    cgltf_free(data);

    if (out.meshes.empty()) {
        fail(error, "no triangle meshes found in '" + path + "'");
        return false;
    }

    MAZ_LOG_INFO("loadModel: '%s' -> %zu mesh(es), %zu verts, %zu tris",
                 path.c_str(), out.meshes.size(), out.vertexCount(), out.triangleCount());
    return true;
}

} // namespace maz::assets
