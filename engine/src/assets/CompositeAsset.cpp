#include "maz/assets/CompositeAsset.hpp"

#include "maz/core/Log.hpp"
#include "maz/io/TextFormat.hpp"
#include "maz/math/Math.hpp"

#include <cstdlib>
#include <fstream>
#include <iomanip>

namespace maz::assets {

namespace {

const char* primToStr(PrimitiveKind k) {
    switch (k) {
    case PrimitiveKind::Box:
        return "box";
    case PrimitiveKind::Sphere:
        return "sphere";
    case PrimitiveKind::Cylinder:
        return "cylinder";
    case PrimitiveKind::Plane:
        return "plane";
    }
    return "box";
}

bool strToPrim(const std::string& s, PrimitiveKind& out) {
    if (s == "box") {
        out = PrimitiveKind::Box;
    } else if (s == "sphere") {
        out = PrimitiveKind::Sphere;
    } else if (s == "cylinder") {
        out = PrimitiveKind::Cylinder;
    } else if (s == "plane") {
        out = PrimitiveKind::Plane;
    } else {
        return false;
    }
    return true;
}

const char* kindToStr(AssetKind k) { return k == AssetKind::Character ? "character" : "item"; }

AssetKind strToKind(const std::string& s) {
    return s == "character" ? AssetKind::Character : AssetKind::Item;
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

} // namespace

Model buildModel(const AssetDoc& doc) {
    Model model;
    model.sourcePath = doc.name;

    Mesh merged;
    merged.name = doc.name;
    bool boundsSeeded = false;

    for (const Part& part : doc.parts) {
        const Mesh src = makePrimitive(part.kind, part.params);
        const math::mat4 xf = part.local.matrix();
        const math::mat3 nm = math::mat3(xf); // uniform scale (Phase 1) -> normalize is sufficient

        const auto base = static_cast<uint32_t>(merged.vertices.size());
        for (const Vertex& v : src.vertices) {
            const math::vec4 p = xf * math::vec4(v.position[0], v.position[1], v.position[2], 1.0f);
            const math::vec3 n =
                math::normalize(nm * math::vec3(v.normal[0], v.normal[1], v.normal[2]));
            Vertex out;
            out.position[0] = p.x;
            out.position[1] = p.y;
            out.position[2] = p.z;
            out.normal[0] = n.x;
            out.normal[1] = n.y;
            out.normal[2] = n.z;
            out.uv[0] = v.uv[0];
            out.uv[1] = v.uv[1];
            merged.vertices.push_back(out);
            expandBounds(model.bounds, out.position, boundsSeeded);
        }
        for (uint32_t idx : src.indices) {
            merged.indices.push_back(base + idx);
        }
    }

    model.meshes.push_back(std::move(merged));
    return model;
}

bool saveAsset(const std::string& path, const AssetDoc& doc, std::string* error) {
    std::ofstream file(path);
    if (!file.is_open()) {
        if (error) {
            *error = "could not open '" + path + "' for writing";
        }
        return false;
    }
    file << std::setprecision(9);
    file << "maz-asset 1\n";
    file << "name " << io::quote(doc.name) << '\n';
    file << "kind " << kindToStr(doc.kind) << '\n';
    for (const Part& part : doc.parts) {
        file << "part\n";
        file << "  name " << io::quote(part.name) << '\n';
        file << "  prim " << primToStr(part.kind) << '\n';
        file << "  size " << part.params.size[0] << ' ' << part.params.size[1] << ' '
             << part.params.size[2] << '\n';
        file << "  radius " << part.params.radius << '\n';
        file << "  height " << part.params.height << '\n';
        file << "  segments " << part.params.segments << '\n';
        file << "  rings " << part.params.rings << '\n';
        io::writeVec3(file, "pos", part.local.position);
        io::writeVec3(file, "rot", part.local.rotationEuler);
        io::writeVec3(file, "scale", part.local.scale);
    }
    return true;
}

bool loadAsset(const std::string& path, AssetDoc& doc, std::string* error) {
    std::ifstream file(path);
    if (!file.is_open()) {
        if (error) {
            *error = "could not open '" + path + "'";
        }
        return false;
    }

    doc = AssetDoc{};
    doc.parts.clear();
    bool sawHeader = false;
    Part* current = nullptr;

    std::string line;
    while (std::getline(file, line)) {
        std::vector<std::string> t = io::tokenize(line);
        if (t.empty() || t[0].empty() || t[0][0] == '#') {
            continue;
        }
        const std::string& key = t[0];

        if (!sawHeader) {
            if (key != "maz-asset") {
                if (error) {
                    *error = "'" + path + "' is not a maz-asset file";
                }
                return false;
            }
            sawHeader = true;
            continue;
        }

        if (key == "part") {
            doc.parts.push_back(Part{});
            current = &doc.parts.back();
        } else if (key == "name") {
            const std::string value = t.size() > 1 ? t[1] : "";
            if (current) {
                current->name = value;
            } else {
                doc.name = value;
            }
        } else if (key == "kind" && !current) {
            doc.kind = strToKind(t.size() > 1 ? t[1] : "item");
        } else if (key == "prim" && current) {
            PrimitiveKind pk = PrimitiveKind::Box;
            if (t.size() > 1 && strToPrim(t[1], pk)) {
                current->kind = pk;
            }
        } else if (key == "size" && current) {
            math::vec3 v(1.0f);
            if (io::readVec3(t, v)) {
                current->params.size[0] = v.x;
                current->params.size[1] = v.y;
                current->params.size[2] = v.z;
            }
        } else if (key == "radius" && current && t.size() > 1) {
            current->params.radius = std::strtof(t[1].c_str(), nullptr);
        } else if (key == "height" && current && t.size() > 1) {
            current->params.height = std::strtof(t[1].c_str(), nullptr);
        } else if (key == "segments" && current && t.size() > 1) {
            current->params.segments = static_cast<int>(std::strtol(t[1].c_str(), nullptr, 10));
        } else if (key == "rings" && current && t.size() > 1) {
            current->params.rings = static_cast<int>(std::strtol(t[1].c_str(), nullptr, 10));
        } else if (key == "pos" && current) {
            io::readVec3(t, current->local.position);
        } else if (key == "rot" && current) {
            io::readVec3(t, current->local.rotationEuler);
        } else if (key == "scale" && current) {
            io::readVec3(t, current->local.scale);
        } else {
            MAZ_LOG_WARN("loadAsset: ignoring unrecognized line '%s'", line.c_str());
        }
    }

    if (!sawHeader) {
        if (error) {
            *error = "'" + path + "' is empty or not a maz-asset file";
        }
        return false;
    }
    return true;
}

} // namespace maz::assets
