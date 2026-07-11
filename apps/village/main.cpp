// Maz Engine — "VILLAGE QUEST" (a playable game on a glTF scene)
// The M19 village scene, turned into a game: the whole level (ground, houses, trees) loads from
// one glTF file via maz::render::loadGltfScene; the houses become solid collision, and golden coins
// are scattered in the open spaces between them. Walk the village (WASD + mouse-look), collect
// every coin against the clock, and your best time is saved across runs. Composes scene loading +
// collision + audio + save + a win state — the engine's systems working together on a real level.
// --demo autopilots a collecting run (for capture); --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using namespace maz;

namespace {

std::string assetPath(const char* rel) {
    const char* base = SDL_GetBasePath();
    return (base ? std::string(base) : std::string()) + rel;
}

// A drawable scene object: a GPU mesh, its texture, and its world transform.
struct Placed {
    render::MeshHandle mesh;
    render::TextureHandle tex;
    render::TextureHandle normal;
    float model[16];
};

// World-space AABB of a node's local geometry under its transform (8 transformed corners).
game::Aabb worldBounds(const render::shapes::MeshData& mesh, const glm::mat4& m) {
    glm::vec3 lo(1e18f), hi(-1e18f);
    for (const render::MeshVertex& v : mesh.vertices) {
        lo = glm::min(lo, glm::vec3(v.px, v.py, v.pz));
        hi = glm::max(hi, glm::vec3(v.px, v.py, v.pz));
    }
    glm::vec3 wlo(1e18f), whi(-1e18f);
    for (int i = 0; i < 8; ++i) {
        const glm::vec3 c((i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z);
        const glm::vec3 w = glm::vec3(m * glm::vec4(c, 1.0f));
        wlo = glm::min(wlo, w);
        whi = glm::max(whi, w);
    }
    return game::Aabb{wlo, whi};
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    const bool autopilot = cfg.demo;
    MAZ_LOG_INFO("VILLAGE QUEST starting (autopilot=%d)", autopilot);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Village Quest";
    wc.width = cfg.width;
    wc.height = cfg.height;
    wc.headless = cfg.headless;
    if (!window.init(wc)) {
        return 1;
    }
    if (!cfg.headless && !autopilot) {
        window.setRelativeMouse(true); // mouse-look
    }

    render::RendererConfig rc;
    rc.vsync = cfg.vsync;
    rc.allowHeadless = cfg.headless;
    auto renderer = render::createVulkanRenderer();
    if (!renderer->init(window, rc)) {
        return 1;
    }

    const uint8_t white[4] = {255, 255, 255, 255};
    render::TextureHandle blank = renderer->createTexture(1, 1, white);

    // Load the scene: one Placed per node; textured nodes (the houses) also become solid collision.
    std::vector<Placed> scene;
    std::vector<game::Aabb> solids;
    std::vector<glm::vec3> houseCenters;
    render::SceneData sceneData;
    if (render::loadGltfScene(assetPath("assets/models/village.gltf").c_str(), sceneData)) {
        for (const render::SceneNode& node : sceneData.nodes) {
            Placed pl;
            pl.mesh = renderer->createMesh(
                node.mesh.vertices.data(), static_cast<uint32_t>(node.mesh.vertices.size()),
                node.mesh.indices.data(), static_cast<uint32_t>(node.mesh.indices.size()));
            pl.tex = node.hasTexture()
                         ? renderer->createTexture(node.textureWidth, node.textureHeight,
                                                   node.texturePixels.data())
                         : blank;
            pl.normal = node.hasNormal()
                            ? renderer->createTexture(node.normalWidth, node.normalHeight,
                                                      node.normalPixels.data())
                            : render::kInvalidTexture;
            std::memcpy(pl.model, node.model, sizeof(pl.model));
            scene.push_back(pl);
            if (node.hasTexture()) { // houses are the textured nodes
                solids.push_back(worldBounds(node.mesh, glm::make_mat4(node.model)));
                houseCenters.emplace_back(node.model[12], node.model[13], node.model[14]);
            }
        }
    } else {
        MAZ_LOG_WARN("no village scene loaded; rendering empty");
    }

    // A full day/night cycle: the sun arcs over ~48s while the sky, ambient, fog, and the warm
    // house lamps all respond (lamps brighten at night, dim by day) — showcasing dynamic lighting,
    // the dynamic sky, point lights, and fog together.
    auto mix3 = [](const glm::vec3& a, const glm::vec3& b, float t) { return glm::mix(a, b, t); };
    auto dayNightLighting = [&](double elapsed) {
        const double period = 48.0;
        const double a = std::fmod(elapsed, period) / period * 2.0 * 3.14159265;
        const float sh = static_cast<float>(std::sin(a)); // sun height, -1..1
        const float day = glm::clamp(sh * 1.6f, 0.0f, 1.0f);
        const float dusk = glm::clamp(1.0f - std::fabs(sh) * 2.5f, 0.0f, 1.0f); // near the horizon

        render::SceneLighting L;
        const glm::vec3 sunD = glm::normalize(
            glm::vec3(std::cos(a) * 0.55f, glm::max(sh, -0.15f), 0.45f));
        L.sunDir[0] = sunD.x; L.sunDir[1] = sunD.y; L.sunDir[2] = sunD.z;

        const glm::vec3 sunCol = mix3(glm::vec3(0.02f), mix3(glm::vec3(1.0f, 0.55f, 0.25f),
                                                            glm::vec3(1.0f, 0.96f, 0.85f), day),
                                      glm::clamp(day + dusk, 0.0f, 1.0f)) * (0.25f + 0.75f * day);
        L.sunColor[0] = sunCol.r; L.sunColor[1] = sunCol.g; L.sunColor[2] = sunCol.b;

        const glm::vec3 amb = mix3(glm::vec3(0.08f, 0.09f, 0.14f), glm::vec3(0.34f, 0.34f, 0.36f), day);
        L.ambient[0] = amb.r; L.ambient[1] = amb.g; L.ambient[2] = amb.b;

        glm::vec3 zen = mix3(glm::vec3(0.02f, 0.03f, 0.09f), glm::vec3(0.24f, 0.44f, 0.82f), day);
        glm::vec3 hor = mix3(glm::vec3(0.05f, 0.06f, 0.13f), glm::vec3(0.72f, 0.82f, 0.95f), day);
        hor = mix3(hor, glm::vec3(0.95f, 0.5f, 0.25f), dusk * 0.7f); // sunset/sunrise warmth
        const glm::vec3 grd = mix3(glm::vec3(0.04f, 0.04f, 0.07f), glm::vec3(0.42f, 0.45f, 0.50f), day);
        L.skyZenith[0] = zen.r; L.skyZenith[1] = zen.g; L.skyZenith[2] = zen.b;
        L.skyHorizon[0] = hor.r; L.skyHorizon[1] = hor.g; L.skyHorizon[2] = hor.b;
        L.skyGround[0] = grd.r; L.skyGround[1] = grd.g; L.skyGround[2] = grd.b;
        L.fogColor[0] = hor.r; L.fogColor[1] = hor.g; L.fogColor[2] = hor.b;
        L.fogDensity = 0.03f;

        // Lamps: bright at night, nearly off at noon.
        const float lamp = mix3(glm::vec3(3.8f), glm::vec3(0.4f), day).x;
        for (const glm::vec3& c : houseCenters) {
            if (L.pointCount >= render::SceneLighting::kMaxPointLights - 1) break; // leave 1 for spot
            render::SceneLighting::Point& p = L.points[L.pointCount++];
            p.pos[0] = c.x; p.pos[1] = 1.2f; p.pos[2] = c.z;
            p.range = 8.5f;
            p.color[0] = 1.0f; p.color[1] = 0.60f; p.color[2] = 0.26f;
            p.intensity = lamp;
        }
        // A cool-white searchlight sweeping the square — a spotlight cone, lit only at night.
        if (L.pointCount < render::SceneLighting::kMaxPointLights) {
            render::SceneLighting::Point& s = L.points[L.pointCount++];
            s.pos[0] = 0.0f; s.pos[1] = 7.5f; s.pos[2] = 0.0f;
            s.range = 26.0f;
            const float sweep = static_cast<float>(elapsed) * 0.7f;
            s.spotDir[0] = std::cos(sweep) * 0.8f;
            s.spotDir[1] = -1.0f;
            s.spotDir[2] = std::sin(sweep) * 0.8f;
            s.spotInnerDeg = 9.0f;
            s.spotOuterDeg = 15.0f;
            s.color[0] = 0.85f; s.color[1] = 0.92f; s.color[2] = 1.0f;
            s.intensity = mix3(glm::vec3(6.0f), glm::vec3(0.0f), day).x; // night only
        }
        return L;
    };

    // A golden coin mesh, scattered in the open spaces between the houses.
    const render::shapes::MeshData coinData =
        render::shapes::makeSphere(0.45f, 12, 18, render::Color{1.0f, 0.86f, 0.28f, 1});
    render::MeshHandle coinMesh =
        renderer->createMesh(coinData.vertices.data(), static_cast<uint32_t>(coinData.vertices.size()),
                             coinData.indices.data(), static_cast<uint32_t>(coinData.indices.size()));
    const int kCoins = 8;
    std::vector<glm::vec3> coins;
    std::vector<bool> collected(static_cast<size_t>(kCoins), false);
    std::mt19937 rng(7u);
    auto frand = [&](float lo, float hi) {
        return std::uniform_real_distribution<float>(lo, hi)(rng);
    };
    for (int i = 0; i < kCoins; ++i) {
        glm::vec3 chosen(0.0f, 1.0f, 0.0f);
        for (int tries = 0; tries < 80; ++tries) {
            const glm::vec3 p(frand(-14.0f, 14.0f), 1.0f, frand(-14.0f, 14.0f));
            const game::Aabb probe = game::Aabb::fromCenterSize(p, glm::vec3(2.4f, 4.0f, 2.4f));
            bool clear = true;
            for (const game::Aabb& s : solids) {
                if (probe.overlaps(s)) {
                    clear = false;
                    break;
                }
            }
            if (clear) {
                chosen = p;
                break;
            }
        }
        coins.push_back(chosen);
    }
    int collectedCount = 0;
    const glm::vec3 playerHalf(0.5f, 1.1f, 0.5f);
    const float eyeHeight = 1.7f;

    // A bonfire of glowing embers rising in the square (3D billboard particles; M28).
    struct Ember {
        glm::vec3 pos, vel;
        float life, maxLife;
    };
    const glm::vec3 fire(0.0f, 0.2f, 4.0f);
    std::vector<Ember> embers(110);
    auto respawnEmber = [&](Ember& e) {
        e.pos = fire + glm::vec3(frand(-0.35f, 0.35f), 0.0f, frand(-0.35f, 0.35f));
        e.vel = glm::vec3(frand(-0.4f, 0.4f), frand(1.3f, 2.6f), frand(-0.4f, 0.4f));
        e.maxLife = frand(1.2f, 2.6f);
        e.life = e.maxLife;
    };
    for (Ember& e : embers) {
        respawnEmber(e);
        e.life = frand(0.0f, e.maxLife); // stagger so the fire starts full
    }

    // A smoke column rising above the embers (alpha-blended particles that grow and fade).
    struct Smoke {
        glm::vec3 pos, vel;
        float life, maxLife;
    };
    std::vector<Smoke> smoke(70);
    auto respawnSmoke = [&](Smoke& s) {
        s.pos = fire + glm::vec3(frand(-0.2f, 0.2f), 0.5f, frand(-0.2f, 0.2f));
        s.vel = glm::vec3(frand(-0.15f, 0.15f), frand(0.8f, 1.5f), frand(-0.15f, 0.15f));
        s.maxLife = frand(2.2f, 3.8f);
        s.life = s.maxLife;
    };
    for (Smoke& s : smoke) {
        respawnSmoke(s);
        s.life = frand(0.0f, s.maxLife);
    }

    // Short-lived sparkle bursts spawned when a coin is collected.
    struct Spark {
        glm::vec3 pos, vel;
        float life, maxLife;
    };
    std::vector<Spark> sparks;
    sparks.reserve(512);
    auto burst = [&](const glm::vec3& at) {
        for (int s = 0; s < 24; ++s) {
            const float a = frand(0.0f, 6.2831853f);
            const float el = frand(-0.6f, 1.0f);
            const float speed = frand(1.5f, 4.0f);
            const float horiz = std::sqrt(std::max(0.0f, 1.0f - el * el));
            Spark sp;
            sp.pos = at;
            sp.vel = glm::vec3(std::cos(a) * horiz, el, std::sin(a) * horiz) * speed;
            sp.vel.y += 1.5f;
            sp.maxLife = frand(0.4f, 0.9f);
            sp.life = sp.maxLife;
            sparks.push_back(sp);
        }
    };

    audio::Audio audio;
    audio.init(); // no-op without a device
    const audio::SoundDesc sfxCoin{audio::Wave::Square, 720.0f, 1180.0f, 0.09f, 0.28f};
    const audio::SoundDesc sfxWin{audio::Wave::Triangle, 523.0f, 1047.0f, 0.55f, 0.32f};

    core::KeyValueStore store;
    store.load(platform::prefPath("MazEngine", "VillageQuest", "save.ini"));
    float bestTime = store.getFloat("best_time", 0.0f);

    ui::Font font;
    font.load(*renderer, assetPath("assets/fonts/DejaVuSans.ttf").c_str(), 34.0f);

    platform::Input input;
    core::Clock clock(1.0 / 60.0);

    game::FlyCamera camera;
    camera.setPosition(glm::vec3(0.0f, eyeHeight, 18.0f));
    camera.setYawPitch(-1.5708f, -0.05f);
    float avoidYaw = 0.0f;    // autopilot: steer around houses it bumps into
    double runTime = 0.0;     // seconds since start until the level is cleared
    bool won = false;

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        uint32_t bw = 0, bh = 0;
        window.drawableSize(bw, bh);
        const float aspect =
            bh > 0 ? static_cast<float>(bw) / static_cast<float>(bh) : 16.0f / 9.0f;

        // Facing: autopilot turns toward the nearest uncollected coin; else mouse-look.
        if (autopilot && !won) {
            int target = -1;
            float bestDist = 1e18f;
            for (size_t i = 0; i < coins.size(); ++i) {
                if (collected[i]) {
                    continue;
                }
                const float dx = coins[i].x - camera.position().x;
                const float dz = coins[i].z - camera.position().z;
                const float d = dx * dx + dz * dz;
                if (d < bestDist) {
                    bestDist = d;
                    target = static_cast<int>(i);
                }
            }
            if (target >= 0) {
                const glm::vec3 dir = coins[static_cast<size_t>(target)] - camera.position();
                camera.setYawPitch(std::atan2(dir.z, dir.x) + avoidYaw, -0.05f);
            }
        } else if (!autopilot) {
            const float sens = 0.0025f;
            camera.look(input.mouseDX() * sens, -input.mouseDY() * sens);
            // Right stick looks around (gamepad).
            const float padLook = 0.05f;
            camera.look(input.gamepadAxis(platform::pad::RightX) * padLook,
                        -input.gamepadAxis(platform::pad::RightY) * padLook);
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            const float dt = static_cast<float>(clock.fixedDelta());
            if (!won) {
                runTime += dt;
            }

            // Integrate the bonfire embers: rise, arc, fade, respawn.
            for (Ember& e : embers) {
                e.pos += e.vel * dt;
                e.vel.y -= 0.5f * dt;
                e.vel.x *= 0.985f;
                e.vel.z *= 0.985f;
                e.life -= dt;
                if (e.life <= 0.0f) {
                    respawnEmber(e);
                }
            }
            // Smoke rises, drifts, slows, respawns.
            for (Smoke& s : smoke) {
                s.pos += s.vel * dt;
                s.vel.x *= 0.99f;
                s.vel.z *= 0.99f;
                s.life -= dt;
                if (s.life <= 0.0f) {
                    respawnSmoke(s);
                }
            }
            // Integrate + retire pickup sparks.
            for (Spark& sp : sparks) {
                sp.pos += sp.vel * dt;
                sp.vel.y -= 4.0f * dt;
                sp.life -= dt;
            }
            sparks.erase(std::remove_if(sparks.begin(), sparks.end(),
                                        [](const Spark& s) { return s.life <= 0.0f; }),
                         sparks.end());

            float fwd = 0.0f, strafe = 0.0f, speed = 8.0f;
            if (autopilot && !won) {
                fwd = 1.0f;
                speed = 6.0f;
            } else if (!autopilot) {
                if (input.keyDown(SDL_SCANCODE_W)) fwd += 1.0f;
                if (input.keyDown(SDL_SCANCODE_S)) fwd -= 1.0f;
                if (input.keyDown(SDL_SCANCODE_D)) strafe += 1.0f;
                if (input.keyDown(SDL_SCANCODE_A)) strafe -= 1.0f;
                // Left stick moves (gamepad); up on the stick is forward.
                fwd -= input.gamepadAxis(platform::pad::LeftY);
                strafe += input.gamepadAxis(platform::pad::LeftX);
            }

            // Move on the ground plane (no vertical fly), sliding against the solid houses.
            glm::vec3 delta = camera.moveDelta(fwd, strafe, 0.0f, dt, speed);
            delta.y = 0.0f;
            const glm::vec3 before = camera.position();
            glm::vec3 after = game::slideMove(before, delta, playerHalf, solids);
            after.y = eyeHeight; // stay at eye height
            camera.setPosition(after);

            if (autopilot && !won) {
                const float intended = glm::length(glm::vec3(delta.x, 0.0f, delta.z));
                const float moved = glm::length(after - before);
                if (intended > 1e-4f && moved < 0.4f * intended) {
                    avoidYaw += 0.14f;
                } else {
                    avoidYaw *= 0.85f;
                }
            }

            for (size_t i = 0; i < coins.size(); ++i) {
                if (collected[i]) {
                    continue;
                }
                const glm::vec3 d = coins[i] - camera.position();
                if (d.x * d.x + d.z * d.z < 1.6f * 1.6f) {
                    collected[i] = true;
                    ++collectedCount;
                    audio.play(sfxCoin);
                    burst(coins[i]);
                }
            }

            if (!won && collectedCount >= kCoins) {
                won = true;
                audio.play(sfxWin);
                const float t = static_cast<float>(runTime);
                if (bestTime <= 0.0f || t < bestTime) {
                    bestTime = t;
                    store.set("best_time", bestTime);
                    store.save();
                }
            }
        }

        const glm::mat4 proj = math::perspective(glm::radians(60.0f), aspect, 0.1f, 200.0f);
        const glm::mat4 viewProj = proj * camera.view();

        renderer->setLighting(dayNightLighting(clock.elapsed()));
        // Bloom scales with the cycle: strong glow at night (lamps/coins), subtle by day.
        {
            const double a = std::fmod(clock.elapsed(), 48.0) / 48.0 * 2.0 * 3.14159265;
            const float day = glm::clamp(static_cast<float>(std::sin(a)) * 1.6f, 0.0f, 1.0f);
            renderer->setBloom(glm::mix(0.85f, 0.15f, day), 0.75f);
            // Cinematic grade: a soft vignette plus a touch of extra saturation/contrast, a little
            // stronger at night for mood.
            renderer->setColorGrade(glm::mix(0.45f, 0.30f, day), 1.12f, 1.06f, true);
            renderer->setChromaticAberration(0.004f); // subtle lens fringing at the screen edges
        }
        renderer->setClearColor(render::Color{0.10f, 0.12f, 0.16f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(camera.position()));
            for (const Placed& pl : scene) {
                renderer->drawMesh(pl.mesh, pl.model, pl.tex, pl.normal);
            }

            // Bonfire embers as camera-facing billboards (warm, additive, fading with life).
            const glm::vec3 fwd = camera.forward();
            const glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
            const glm::vec3 up = glm::normalize(glm::cross(right, fwd));
            renderer->setCameraBasis(glm::value_ptr(right), glm::value_ptr(up));
            // Smoke first (alpha, grows + fades as it rises).
            for (const Smoke& s : smoke) {
                const float t = s.life / s.maxLife;      // 1 fresh -> 0 dead
                const float age = 1.0f - t;
                const float size = 0.35f + age * 1.2f;   // billows outward as it rises
                const float g = 0.22f + 0.12f * t;       // dark gray, lighter when fresh
                const float col[4] = {g, g, g, t * 0.30f};
                renderer->drawParticle3D(glm::value_ptr(s.pos), size, col, false);
            }
            for (const Ember& e : embers) {
                const float t = e.life / e.maxLife; // 1 fresh -> 0 dead
                const float size = 0.05f + 0.16f * t;
                const float col[4] = {1.0f, 0.35f + 0.4f * t, 0.12f * t, t};
                renderer->drawParticle3D(glm::value_ptr(e.pos), size, col);
            }
            // Golden pickup sparks.
            for (const Spark& sp : sparks) {
                const float t = sp.life / sp.maxLife;
                const float size = 0.04f + 0.11f * t;
                const float col[4] = {1.0f, 0.92f, 0.45f, t};
                renderer->drawParticle3D(glm::value_ptr(sp.pos), size, col);
            }
            const float bob = 0.15f * std::sin(static_cast<float>(clock.elapsed()) * 2.5f);
            const float spin = static_cast<float>(clock.elapsed()) * 2.0f;
            for (size_t i = 0; i < coins.size(); ++i) {
                if (collected[i]) {
                    continue;
                }
                glm::vec3 p = coins[i];
                p.y += bob;
                glm::mat4 m = glm::translate(glm::mat4(1.0f), p);
                m = glm::rotate(m, spin, glm::vec3(0, 1, 0));
                renderer->drawMesh(coinMesh, glm::value_ptr(m), blank);
            }

            render::Camera2D uiCam;
            uiCam.usePixelSpace = true;
            renderer->setCamera2D(uiCam);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  VILLAGE QUEST",
                          render::Color{1, 1, 1, 1}, 0.72f);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "COINS  %d / %d          TIME  %.1f", collectedCount,
                          kCoins, runTime);
            font.drawText(*renderer, 16.0f, 46.0f, buf, render::Color{1, 0.95f, 0.5f, 1}, 0.55f);
            if (bestTime > 0.0f) {
                std::snprintf(buf, sizeof(buf), "BEST  %.1f", bestTime);
                font.drawText(*renderer, 16.0f, 74.0f, buf, render::Color{0.7f, 0.85f, 1.0f, 1}, 0.45f);
            }
            if (won) {
                std::snprintf(buf, sizeof(buf), "VILLAGE CLEARED!  %.1fs", runTime);
                font.drawTextCentered(*renderer, static_cast<float>(bw) * 0.5f,
                                      static_cast<float>(bh) * 0.5f - 20.0f, buf,
                                      render::Color{0.4f, 1.0f, 0.5f, 1}, 0.9f);
            } else if (!autopilot) {
                font.drawText(*renderer, 16.0f, static_cast<float>(bh) - 40.0f,
                              "WASD move   mouse look   collect every coin",
                              render::Color{0.7f, 0.75f, 0.85f, 1}, 0.42f);
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("VILLAGE QUEST shutting down (coins %d/%d, best %.1f, renderer %s, audio %s)",
                 collectedCount, kCoins, bestTime, renderer->isActive() ? "active" : "inactive",
                 audio.active() ? "active" : "inactive");
    audio.shutdown();
    renderer->shutdown();
    window.shutdown();
    return 0;
}
