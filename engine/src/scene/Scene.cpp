#include "maz/scene/Scene.hpp"

#include "maz/core/Log.hpp"
#include "maz/io/TextFormat.hpp"

#include <fstream>
#include <iomanip>

namespace maz::scene {

math::mat4 Transform::matrix() const {
    const math::mat4 identity(1.0f);
    const math::mat4 t = glm::translate(identity, position);
    const math::mat4 r =
        glm::rotate(identity, glm::radians(rotationEuler.y), math::vec3(0.0f, 1.0f, 0.0f)) *
        glm::rotate(identity, glm::radians(rotationEuler.x), math::vec3(1.0f, 0.0f, 0.0f)) *
        glm::rotate(identity, glm::radians(rotationEuler.z), math::vec3(0.0f, 0.0f, 1.0f));
    const math::mat4 s = glm::scale(identity, scale);
    return t * r * s;
}

// The .mazscene format shares its tokenizer and vec3 helpers with the rest of the engine's
// text formats; see maz/io/TextFormat.hpp.
using io::quote;
using io::readVec3;
using io::tokenize;
using io::writeVec3;

bool saveScene(const std::string& path, const Scene& scene, std::string* error) {
    std::ofstream file(path);
    if (!file.is_open()) {
        if (error) {
            *error = "could not open '" + path + "' for writing";
        }
        return false;
    }
    file << std::setprecision(9);
    file << "maz-scene 1\n";
    file << "name " << quote(scene.name) << '\n';
    for (const Entity& e : scene.entities) {
        file << "entity\n";
        file << "  name " << quote(e.name) << '\n';
        if (!e.modelPath.empty()) {
            file << "  model " << quote(e.modelPath) << '\n';
        }
        writeVec3(file, "pos", e.transform.position);
        writeVec3(file, "rot", e.transform.rotationEuler);
        writeVec3(file, "scale", e.transform.scale);
    }
    return true;
}

bool loadScene(const std::string& path, Scene& scene, std::string* error) {
    std::ifstream file(path);
    if (!file.is_open()) {
        if (error) {
            *error = "could not open '" + path + "'";
        }
        return false;
    }

    scene = Scene{};
    scene.entities.clear();
    bool sawHeader = false;
    Entity* current = nullptr;

    std::string line;
    while (std::getline(file, line)) {
        std::vector<std::string> t = tokenize(line);
        if (t.empty() || t[0].empty() || t[0][0] == '#') {
            continue;
        }
        const std::string& key = t[0];

        if (!sawHeader) {
            if (key != "maz-scene") {
                if (error) {
                    *error = "'" + path + "' is not a maz-scene file";
                }
                return false;
            }
            sawHeader = true;
            continue;
        }

        if (key == "entity") {
            scene.entities.push_back(Entity{});
            current = &scene.entities.back();
        } else if (key == "name") {
            const std::string value = t.size() > 1 ? t[1] : "";
            if (current) {
                current->name = value;
            } else {
                scene.name = value;
            }
        } else if (key == "model" && current) {
            current->modelPath = t.size() > 1 ? t[1] : "";
        } else if (key == "pos" && current) {
            readVec3(t, current->transform.position);
        } else if (key == "rot" && current) {
            readVec3(t, current->transform.rotationEuler);
        } else if (key == "scale" && current) {
            readVec3(t, current->transform.scale);
        } else {
            MAZ_LOG_WARN("loadScene: ignoring unrecognized line '%s'", line.c_str());
        }
    }

    if (!sawHeader) {
        if (error) {
            *error = "'" + path + "' is empty or not a maz-scene file";
        }
        return false;
    }
    return true;
}

} // namespace maz::scene
