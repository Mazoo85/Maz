// Maz render probe — automated proof that the mesh renderer actually draws.
//
// Renders the test cube off-screen (no window/display) via OffscreenRenderer, reads the pixels
// back, and asserts the cube was drawn: the center of the image must be the lit clay-red material
// (not the clear color), while the corners must stay the clear color. With a software Vulkan
// driver (lavapipe) this runs in CI, turning "the renderer compiles" into "the renderer draws
// correct pixels". Exits non-zero on failure.

#include "maz/Engine.hpp"
#include "maz/assets/Model.hpp"
#include "maz/render/OffscreenRenderer.hpp"
#include "maz/scene/Camera.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

struct Pixel {
    uint8_t r, g, b, a;
};

Pixel at(const std::vector<uint8_t>& px, uint32_t w, uint32_t x, uint32_t y) {
    const size_t i = (static_cast<size_t>(y) * w + x) * 4;
    return {px[i], px[i + 1], px[i + 2], px[i + 3]};
}

} // namespace

int main(int argc, char** argv) {
    const char* modelPath = argc > 1 ? argv[1] : "assets/models/cube.gltf";
    const uint32_t size = 128;

    // A headless window only satisfies the context's signature; no surface is created.
    platform::Window window;
    platform::WindowConfig wc;
    wc.headless = true;
    wc.width = size;
    wc.height = size;
    if (!window.init(wc)) {
        std::printf("render probe: FAIL (window init)\n");
        return 1;
    }

    render::OffscreenRenderer offscreen;
    if (!offscreen.init(window, size, size)) {
        // No Vulkan driver at all — cannot verify. Treated as failure so CI (which installs
        // lavapipe) tells us if the driver is missing rather than silently passing.
        std::printf("render probe: FAIL (no Vulkan device — is a Vulkan ICD installed?)\n");
        window.shutdown();
        return 2;
    }

    assets::Model model;
    std::string err;
    if (!assets::loadModel(modelPath, model, &err)) {
        std::printf("render probe: FAIL (load model: %s)\n", err.c_str());
        return 1;
    }
    const int handle = offscreen.uploadModel(model);
    if (handle < 0) {
        std::printf("render probe: FAIL (upload model)\n");
        return 1;
    }

    scene::Camera camera;
    camera.setPosition({1.6f, 1.4f, 2.6f}); // off-axis so a lit, shaded face faces the camera
    camera.setTarget({0.0f, 0.0f, 0.0f});
    camera.setPerspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);

    render::OffscreenRenderer::Item item;
    item.handle = handle;
    item.model = math::mat4(1.0f);
    item.mvp = camera.viewProjection() * item.model;

    const render::Color clear{0.10f, 0.11f, 0.13f, 1.0f};
    std::vector<uint8_t> pixels;
    if (!offscreen.renderToPixels(clear, {item}, pixels)) {
        std::printf("render probe: FAIL (render)\n");
        return 1;
    }

    const Pixel center = at(pixels, size, size / 2, size / 2);
    const Pixel corner = at(pixels, size, 3, 3);
    std::printf("render probe: center=(%u,%u,%u) corner=(%u,%u,%u)\n",
                static_cast<unsigned>(center.r), static_cast<unsigned>(center.g),
                static_cast<unsigned>(center.b), static_cast<unsigned>(corner.r),
                static_cast<unsigned>(corner.g), static_cast<unsigned>(corner.b));

    // The clear color in 8-bit is roughly (26, 28, 33).
    auto nearClear = [](Pixel p) {
        return p.r < 60 && p.g < 60 && p.b < 70;
    };

    int failures = 0;
    auto expect = [&](bool cond, const char* what) {
        if (!cond) {
            std::printf("  MISS: %s\n", what);
            ++failures;
        }
    };

    // Center must be the drawn cube: not the background, and red-dominant (clay material lit).
    expect(!nearClear(center), "center pixel is not the clear color (cube was drawn)");
    expect(center.r > center.g && center.r > center.b, "center pixel is red-dominant (cube material)");
    expect(center.r > 70, "center pixel is bright enough to be lit");
    // A corner should still be background — proves we didn't just fill the whole image.
    expect(nearClear(corner), "corner pixel is still the clear color");

    std::printf("render probe: %s\n", failures == 0 ? "PASS" : "FAIL");
    offscreen.shutdown();
    window.shutdown();
    return failures == 0 ? 0 : 1;
}
