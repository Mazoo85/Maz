// Unit tests for the scene model + .mazscene serialization. Pure data, no GPU.

#include "maz/scene/Scene.hpp"

#include <cmath>
#include <cstdio>

using namespace maz;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

bool nearly(float a, float b) { return std::fabs(a - b) < 1e-4f; }

} // namespace

int main() {
    const char* tmp = "unit_scene_tmp.mazscene";

    scene::Scene s;
    s.name = "Demo \"quoted\" Scene";
    scene::Entity ground;
    ground.name = "Ground";
    ground.modelPath = "assets/models/cube.gltf";
    ground.transform.position = {1.5f, -2.0f, 3.25f};
    ground.transform.rotationEuler = {0.0f, 45.0f, 0.0f};
    ground.transform.scale = {3.0f, 0.2f, 3.0f};
    scene::Entity floating;
    floating.name = "Floating Cube";
    floating.modelPath = "assets/models/cube.gltf";
    floating.transform.position = {0.0f, 2.0f, 0.0f};
    s.entities = {ground, floating};

    std::string err;
    check(scene::saveScene(tmp, s, &err), "save scene");

    scene::Scene r;
    check(scene::loadScene(tmp, r, &err), err.empty() ? "load scene" : err.c_str());
    check(r.name == s.name, "scene name round-trips (quotes + spaces)");
    check(r.entities.size() == 2, "entity count round-trips");
    if (r.entities.size() == 2) {
        check(r.entities[0].name == "Ground", "entity 0 name");
        check(r.entities[0].modelPath == "assets/models/cube.gltf", "entity 0 model path");
        check(nearly(r.entities[0].transform.position.x, 1.5f) &&
                  nearly(r.entities[0].transform.position.z, 3.25f),
              "entity 0 position");
        check(nearly(r.entities[0].transform.rotationEuler.y, 45.0f), "entity 0 rotation");
        check(nearly(r.entities[0].transform.scale.y, 0.2f), "entity 0 scale");
        check(r.entities[1].name == "Floating Cube", "entity 1 name (with space)");
    }

    scene::Transform identity;
    const math::vec4 o = identity.matrix() * math::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    check(nearly(o.x, 0.0f) && nearly(o.y, 0.0f) && nearly(o.z, 0.0f),
          "identity transform keeps the origin");

    scene::Transform moved;
    moved.position = {5.0f, 0.0f, 0.0f};
    const math::vec4 p = moved.matrix() * math::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    check(nearly(p.x, 5.0f), "translation moves the origin to +5x");

    scene::Scene bad;
    std::string e2;
    std::FILE* f = std::fopen("unit_scene_bad.mazscene", "w");
    if (f) {
        std::fputs("this is not a scene\n", f);
        std::fclose(f);
    }
    check(!scene::loadScene("unit_scene_bad.mazscene", bad, &e2), "rejects a non-scene file");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
