#include "maz/scene/Scene.hpp"

#include "maz/core/Log.hpp"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

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

namespace {

// Split a line into whitespace-separated tokens, treating a "double quoted" run as one token
// (quotes stripped). Simple and sufficient for the scene format.
std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::size_t i = 0;
    while (i < line.size()) {
        if (std::isspace(static_cast<unsigned char>(line[i]))) {
            ++i;
            continue;
        }
        std::string tok;
        if (line[i] == '"') {
            ++i;
            while (i < line.size() && line[i] != '"') {
                if (line[i] == '\\' && i + 1 < line.size()) {
                    ++i; // take the escaped character literally
                }
                tok += line[i++];
            }
            if (i < line.size()) {
                ++i; // closing quote
            }
        } else {
            while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i]))) {
                tok += line[i++];
            }
        }
        tokens.push_back(tok);
    }
    return tokens;
}

// Escape a string for the quoted scene-file form (backslash + quote only).
std::string quote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out += '\\';
        }
        out += c;
    }
    out += '"';
    return out;
}

void writeVec3(std::ostream& os, const char* key, const math::vec3& v) {
    os << "  " << key << ' ' << v.x << ' ' << v.y << ' ' << v.z << '\n';
}

bool readVec3(const std::vector<std::string>& t, math::vec3& out) {
    if (t.size() < 4) {
        return false;
    }
    out.x = std::strtof(t[1].c_str(), nullptr);
    out.y = std::strtof(t[2].c_str(), nullptr);
    out.z = std::strtof(t[3].c_str(), nullptr);
    return true;
}

} // namespace

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
