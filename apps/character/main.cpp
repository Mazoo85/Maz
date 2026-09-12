// Maz Engine — "SENTINEL VIEWER" — an interactive, animated 3D character showcase. The android
// (character.hpp) is authored entirely from the engine's procedural mesh primitives, rigged with
// shoulder + elbow joints and a head look, and drawn on the Vulkan PBR path. You can orbit and zoom,
// toggle a turntable, cycle the animation (breathing idle / wave / power-up / A-pose / action), and
// swap the colour theme (cyan / crimson / verdant / gold / obsidian / ice). Every animation frame has
// identical vertex/index counts, so the three material groups are dynamic meshes rebuilt from the
// procedural rig and streamed each frame via updateMesh — no reallocation. --headless / --frames N
// runs with no window for CI.
//
// Controls:  drag/arrows = orbit   wheel / W,S = zoom   Space = turntable
//            P = next animation   C = next theme   R = reset view   Esc = quit

#include "maz/Engine.hpp"

#include "character.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {
std::vector<uint8_t> makeWhite() { return std::vector<uint8_t>(4 * 4 * 4, 255); }

render::MeshHandle uploadDynamic(render::Renderer& r, const render::shapes::MeshData& m) {
    return r.createDynamicMesh(m.vertices.data(), static_cast<uint32_t>(m.vertices.size()),
                               m.indices.data(), static_cast<uint32_t>(m.indices.size()));
}
void stream(render::Renderer& r, render::MeshHandle h, const render::shapes::MeshData& m) {
    r.updateMesh(h, m.vertices.data(), static_cast<uint32_t>(m.vertices.size()));
}

const char* animName(character::Anim a) {
    switch (a) {
        case character::Anim::Wave: return "wave";
        case character::Anim::PowerUp: return "power-up";
        case character::Anim::APose: return "A-pose";
        case character::Anim::Action: return "action";
        default: return "idle";
    }
}
const char* themeName(character::Theme t) {
    switch (t) {
        case character::Theme::Crimson: return "crimson";
        case character::Theme::Verdant: return "verdant";
        case character::Theme::Gold: return "gold";
        case character::Theme::Obsidian: return "obsidian";
        case character::Theme::Ice: return "ice";
        default: return "cyan";
    }
}
} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("SENTINEL VIEWER (animated procedural character) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — SENTINEL Viewer";
    wc.width = cfg.width;
    wc.height = cfg.height;
    wc.headless = cfg.headless;
    if (!window.init(wc)) {
        return 1;
    }

    render::RendererConfig rc;
    rc.vsync = cfg.vsync;
    rc.allowHeadless = cfg.headless;
    auto renderer = render::createVulkanRenderer();
    if (!renderer->init(window, rc)) {
        return 1;
    }

    platform::Input input;
    core::Clock clock(1.0 / 60.0);

    // Viewer state.
    character::Anim anim = character::Anim::Idle;
    character::Theme theme = character::Theme::Cyan;
    float animTime = 0.0f;
    character::CharacterModel model = character::buildCharacter(character::animate(anim, 0.0f), theme);

    const render::MeshHandle bodyMesh = uploadDynamic(*renderer, model.body);
    const render::MeshHandle darkMesh = uploadDynamic(*renderer, model.dark);
    const render::MeshHandle glowMesh = uploadDynamic(*renderer, model.glow);
    const render::shapes::MeshData planeMesh =
        render::shapes::makePlane(6.0f, render::Color{0.10f, 0.11f, 0.14f, 1.0f});
    const render::MeshHandle groundMesh =
        renderer->createMesh(planeMesh.vertices.data(), static_cast<uint32_t>(planeMesh.vertices.size()),
                             planeMesh.indices.data(), static_cast<uint32_t>(planeMesh.indices.size()));
    const render::TextureHandle white = renderer->createTexture(4, 4, makeWhite().data());

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 36.0f);
    }

    render::Renderer::Material bodyMat;
    bodyMat.albedo = white;
    bodyMat.specular = 1.0f;
    bodyMat.metallic = 0.9f;
    bodyMat.roughness = 0.30f;

    render::Renderer::Material darkMat;
    darkMat.albedo = white;
    darkMat.specular = 0.5f;
    darkMat.metallic = 0.1f;
    darkMat.roughness = 0.75f;

    render::Renderer::Material glowMat;
    glowMat.albedo = white;

    render::Renderer::Material groundMat;
    groundMat.albedo = white;
    groundMat.specular = 0.2f;
    groundMat.roughness = 0.9f;

    render::SceneLighting light;
    light.ambient[0] = 0.10f;
    light.ambient[1] = 0.12f;
    light.ambient[2] = 0.16f;
    light.sunDir[0] = -0.45f;
    light.sunDir[1] = 0.78f;
    light.sunDir[2] = 0.55f;
    light.sunColor[0] = 1.00f;
    light.sunColor[1] = 0.94f;
    light.sunColor[2] = 0.84f;
    light.pointCount = 1;
    light.points[0].pos[0] = -4.0f;
    light.points[0].pos[1] = 2.5f;
    light.points[0].pos[2] = 3.5f;
    light.points[0].range = 20.0f;
    light.points[0].color[0] = 0.45f;
    light.points[0].color[1] = 0.62f;
    light.points[0].color[2] = 1.00f;
    light.points[0].intensity = 2.2f;
    light.skyZenith[0] = 0.10f;
    light.skyZenith[1] = 0.16f;
    light.skyZenith[2] = 0.34f;
    light.skyHorizon[0] = 0.56f;
    light.skyHorizon[1] = 0.60f;
    light.skyHorizon[2] = 0.70f;
    light.skyGround[0] = 0.05f;
    light.skyGround[1] = 0.05f;
    light.skyGround[2] = 0.07f;
    renderer->setLighting(light);

    // Orbit camera state.
    float yaw = 0.5f, pitch = 0.12f, dist = 4.9f;
    bool spin = true;
    const glm::vec3 target(0.0f, 1.05f, 0.0f);

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        const float dt = static_cast<float>(clock.frameDelta());
        animTime += dt;

        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }
        if (input.keyPressed(SDL_SCANCODE_P)) {
            anim = static_cast<character::Anim>((static_cast<int>(anim) + 1) % character::kAnimCount);
            animTime = 0.0f;
        }
        if (input.keyPressed(SDL_SCANCODE_C)) {
            theme = static_cast<character::Theme>((static_cast<int>(theme) + 1) % character::kThemeCount);
        }
        if (input.keyPressed(SDL_SCANCODE_SPACE)) {
            spin = !spin;
        }
        if (input.keyPressed(SDL_SCANCODE_R)) {
            yaw = 0.5f;
            pitch = 0.12f;
            dist = 4.9f;
        }

        const bool dragging = input.mouseDown(0);
        if (dragging) {
            yaw -= input.mouseDX() * 0.01f;
            pitch += input.mouseDY() * 0.01f;
        }
        if (input.keyDown(SDL_SCANCODE_LEFT)) yaw -= dt * 1.5f;
        if (input.keyDown(SDL_SCANCODE_RIGHT)) yaw += dt * 1.5f;
        if (input.keyDown(SDL_SCANCODE_UP)) pitch += dt * 1.2f;
        if (input.keyDown(SDL_SCANCODE_DOWN)) pitch -= dt * 1.2f;
        dist -= input.wheel() * 0.4f;
        if (input.keyDown(SDL_SCANCODE_W)) dist -= dt * 3.0f;
        if (input.keyDown(SDL_SCANCODE_S)) dist += dt * 3.0f;
        if (spin && !dragging) yaw += dt * 0.5f;
        pitch = std::fmax(-1.30f, std::fmin(1.30f, pitch));
        dist = std::fmax(2.4f, std::fmin(9.0f, dist));

        // Rebuild the animated character for this frame and update the glow's emissive (pulsing on
        // power-up), then stream the fresh geometry into the dynamic meshes.
        model = character::buildCharacter(character::animate(anim, animTime), theme);
        const character::Palette pal = character::paletteFor(theme);
        const float pulse =
            anim == character::Anim::PowerUp ? 1.0f + 0.35f * std::sin(animTime * 4.0f) : 1.0f;
        glowMat.emissive[0] = pal.emissive[0] * pulse;
        glowMat.emissive[1] = pal.emissive[1] * pulse;
        glowMat.emissive[2] = pal.emissive[2] * pulse;

        uint32_t bw = 0, bh = 0;
        window.drawableSize(bw, bh);
        const float aspect = bh > 0 ? static_cast<float>(bw) / static_cast<float>(bh) : 16.0f / 9.0f;

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        const glm::vec3 eye = target + dist * glm::vec3(std::cos(pitch) * std::sin(yaw),
                                                        std::sin(pitch),
                                                        std::cos(pitch) * std::cos(yaw));
        const glm::mat4 proj = math::perspective(glm::radians(32.0f), aspect, 0.1f, 100.0f);
        const glm::mat4 view = glm::lookAt(eye, target, glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;
        const glm::mat4 identity(1.0f);

        renderer->setClearColor(render::Color{0.05f, 0.06f, 0.09f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye));

            stream(*renderer, bodyMesh, model.body);
            stream(*renderer, darkMesh, model.dark);
            stream(*renderer, glowMesh, model.glow);

            renderer->drawMeshMaterial(groundMesh, glm::value_ptr(identity), groundMat);
            renderer->drawMeshMaterial(bodyMesh, glm::value_ptr(identity), bodyMat);
            renderer->drawMeshMaterial(darkMesh, glm::value_ptr(identity), darkMat);
            renderer->drawMeshMaterial(glowMesh, glm::value_ptr(identity), glowMat);

            render::Camera2D uicam;
            uicam.usePixelSpace = true;
            renderer->setCamera2D(uicam);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  SENTINEL VIEWER",
                          render::Color{1, 1, 1, 1}, 0.55f);
            const std::string status =
                std::string("anim: ") + animName(anim) + "    theme: " + themeName(theme);
            font.drawText(*renderer, 16.0f, 44.0f, status.c_str(),
                          render::Color{0.72f, 0.82f, 0.95f, 1}, 0.36f);
            font.drawText(*renderer, 16.0f, 70.0f,
                          "drag/arrows: orbit   wheel/W,S: zoom   Space: spin   P: animation   C: theme   R: reset",
                          render::Color{0.60f, 0.66f, 0.78f, 1}, 0.28f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("SENTINEL VIEWER shutting down (renderer %s)",
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
