#pragma once

#include "maz/assets/Model.hpp"
#include "maz/math/Math.hpp"

#include <cstdint>
#include <memory>

namespace maz::platform {
class Window;
}

namespace maz::render {

struct RendererConfig {
    bool vsync = true;
    bool enableValidation = false;   // Vulkan validation layers (debug builds)
    // When true the renderer may run without a presentable surface / GPU and simply no-ops.
    // Used for headless CI so the rest of the engine can still be exercised.
    bool allowHeadless = false;
};

struct Color {
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
};

// Rendering interface. Gameplay talks to this, never to Vulkan directly, so a future backend
// (a 3D path, a null/software path, WebGPU) can be swapped in without touching game code.
class Renderer {
public:
    virtual ~Renderer() = default;

    virtual bool init(platform::Window& window, const RendererConfig& cfg) = 0;
    virtual void shutdown() = 0;

    // Rebuild swapchain-sized resources after a window resize.
    virtual void onResize(uint32_t width, uint32_t height) = 0;

    // Acquire the next frame. Returns false if there's nothing to draw into this frame
    // (minimized, swapchain out-of-date, or headless/no-GPU) — callers should skip endFrame.
    virtual bool beginFrame() = 0;
    virtual void setClearColor(const Color& color) = 0;
    virtual void endFrame() = 0;

    // Upload a model for drawing. Returns a handle (>= 0) for drawModel(), or -1 if the renderer
    // is inactive (headless/no-GPU) or the upload failed. Multiple models may be uploaded.
    virtual int uploadModel(const assets::Model& model) = 0;

    // Draw a previously uploaded model. Call between beginFrame() and endFrame(). `mvp` is the full
    // model-view-projection; `model` is the model matrix alone (used for normals). No-op if the
    // handle is invalid or the renderer is inactive.
    virtual void drawModel(int handle, const math::mat4& mvp, const math::mat4& model) = 0;

    // --- Editor UI (Dear ImGui) overlay -------------------------------------------
    // Optional: bring up an ImGui overlay drawn on top of the scene each frame. Base class is a
    // no-op so the sandbox and headless runs ignore it. Returns false if unavailable (headless).
    virtual bool initGui(platform::Window& window) { return false; }
    // Begin an ImGui frame. Call once per rendered frame before issuing ImGui:: UI calls; the
    // overlay is recorded automatically inside the render pass on endFrame().
    virtual void guiNewFrame() {}

    // True when a real GPU + presentable surface are backing this renderer.
    virtual bool isActive() const = 0;
};

// Factory for the Vulkan backend.
std::unique_ptr<Renderer> createVulkanRenderer();

} // namespace maz::render
