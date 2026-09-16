// tests/render/gpu_capture.cpp — exercises the surfaceless offscreen GPU path end to end: build a
// headless window (no display), init the Vulkan renderer in OFFSCREEN mode (RendererConfig::
// offscreenWidth/Height, no swapchain / no present), render one lit cube through the real PBR frame
// graph, and read the frame back with Renderer::captureImage(). Proves captureImage returns a
// correctly-sized image in which the centre (the cube) differs from the corner (the sky/background).
//
// This needs a real Vulkan device. In CI's Linux job software Vulkan (lavapipe) provides one; on a box
// with no ICD the renderer stays inactive, so the test SELF-SKIPS with exit(0) — exactly like the
// golden-image test. It only fails when a device IS present and the capture is wrong.
#include "maz/math/Math.hpp" // math::perspective (Vulkan-correct, Y-flipped — what the renderer expects)
#include "maz/platform/Window.hpp"
#include "maz/render/Image.hpp"
#include "maz/render/Renderer.hpp"
#include "maz/render/Shapes.hpp"

#include <glm/gtc/matrix_transform.hpp> // glm::lookAt
#include <glm/gtc/type_ptr.hpp>         // glm::value_ptr

#include <cmath>
#include <cstdio>

using namespace maz;

int main() {
    constexpr int kSize = 64;

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "gpu_capture";
    wc.headless = true; // SDL dummy video driver — no display, no presentable surface
    if (!window.init(wc)) {
        std::printf("gpu_capture: window init failed — skipped\n");
        return 0; // no windowing at all: nothing to test here
    }

    render::RendererConfig rc;
    rc.allowHeadless = true;
    rc.offscreenWidth = kSize; // ask for the surfaceless offscreen capture path
    rc.offscreenHeight = kSize;
    auto renderer = render::createVulkanRenderer();
    if (!renderer->init(window, rc)) {
        // init only returns false with a device present but offscreen setup failing — a real bug.
        std::fprintf(stderr, "gpu_capture: renderer init failed with a device present\n");
        window.shutdown();
        return 1;
    }
    if (!renderer->isActive()) {
        std::printf("gpu_capture: no Vulkan device — skipped\n");
        renderer->shutdown();
        window.shutdown();
        return 0; // self-skip, like golden_images without lavapipe
    }

    // A unit cube and a 1x1 white albedo (vertex colours carry the look).
    const render::shapes::MeshData box = render::shapes::makeBox(1.0f, render::Color{1, 1, 1, 1});
    const render::MeshHandle cube =
        renderer->createMesh(box.vertices.data(), static_cast<uint32_t>(box.vertices.size()),
                             box.indices.data(), static_cast<uint32_t>(box.indices.size()));
    const uint8_t white[4] = {255, 255, 255, 255};
    const render::TextureHandle albedo = renderer->createTexture(1, 1, white);

    // Frame the cube dead centre.
    const glm::vec3 eye(1.7f, 1.3f, 2.3f);
    const glm::mat4 proj = math::perspective(glm::radians(50.0f), 1.0f, 0.05f, 100.0f);
    const glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f), glm::vec3(0, 1, 0));
    const glm::mat4 viewProj = proj * view;
    const glm::mat4 model(1.0f);

    int failures = 0;
    render::Image shot;
    bool captured = false;
    renderer->setClearColor(render::Color{0.05f, 0.06f, 0.09f, 1.0f});
    if (renderer->beginFrame()) {
        renderer->setViewProjection3D(glm::value_ptr(viewProj));
        renderer->setCameraPosition(glm::value_ptr(eye));
        render::Renderer::Material mat;
        mat.albedo = albedo;
        mat.roughness = 0.5f;
        mat.specular = 1.0f;
        renderer->drawMeshMaterial(cube, glm::value_ptr(model), mat);
        renderer->endFrame();
        captured = renderer->captureImage(shot);
    }

    if (!captured) {
        std::fprintf(stderr, "gpu_capture: captureImage returned false on an active offscreen renderer\n");
        ++failures;
    } else {
        if (shot.width() != kSize || shot.height() != kSize) {
            std::fprintf(stderr, "gpu_capture: captured %dx%d, expected %dx%d\n", shot.width(),
                         shot.height(), kSize, kSize);
            ++failures;
        }
        // The cube covers the centre; the corner shows sky/background. They must differ.
        const render::Color centre = shot.getPixel(kSize / 2, kSize / 2);
        const render::Color corner = shot.getPixel(2, 2);
        const float d = std::fabs(centre.r - corner.r) + std::fabs(centre.g - corner.g) +
                        std::fabs(centre.b - corner.b);
        if (d < 0.05f) {
            std::fprintf(stderr,
                         "gpu_capture: centre and corner nearly identical (d=%.3f) — cube did not "
                         "render into the captured frame\n",
                         static_cast<double>(d));
            ++failures;
        }
    }

    renderer->shutdown();
    window.shutdown();
    if (failures == 0) {
        std::printf("gpu_capture: offscreen render + captureImage verified\n");
    }
    return failures ? 1 : 0;
}
