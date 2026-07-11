#pragma once

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

// Opaque texture handle. 0 is the invalid/"no texture" sentinel.
using TextureHandle = uint32_t;
constexpr TextureHandle kInvalidTexture = 0;

// One sprite draw. Position/size are in the active 2D camera's units (pixels by default),
// origin at the sprite's top-left. `rotation` is radians about the sprite center. `uvMin`/`uvMax`
// select a sub-rectangle of the texture (0..1); defaults cover the whole texture. `color` tints.
struct SpriteDesc {
    float x = 0.0f, y = 0.0f;
    float width = 0.0f, height = 0.0f;
    float rotation = 0.0f;
    float uvMinX = 0.0f, uvMinY = 0.0f;
    float uvMaxX = 1.0f, uvMaxY = 1.0f;
    Color color{1.0f, 1.0f, 1.0f, 1.0f};
};

// 2D camera. Defaults to pixel-space matching the framebuffer (origin top-left, y-down).
struct Camera2D {
    float centerX = 0.0f, centerY = 0.0f; // world point at the viewport center (0 = use default)
    float zoom = 1.0f;
    bool usePixelSpace = true;            // when true, ignore center/zoom and map 1 unit = 1 pixel
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

    // --- 2D sprites (Phase 3) ---
    // Load an RGBA image from disk. Returns kInvalidTexture on failure or when inactive.
    virtual TextureHandle loadTexture(const char* path) = 0;
    // Create a texture from tightly-packed RGBA8 pixels (rowlen = width*4).
    virtual TextureHandle createTexture(uint32_t width, uint32_t height, const void* rgbaPixels) = 0;
    // Set the 2D camera used for subsequent drawSprite calls this frame.
    virtual void setCamera2D(const Camera2D& camera) = 0;
    // Queue a sprite for drawing between beginFrame/endFrame. No-op when inactive.
    virtual void drawSprite(TextureHandle texture, const SpriteDesc& sprite) = 0;

    // True when a real GPU + presentable surface are backing this renderer.
    virtual bool isActive() const = 0;
};

// Factory for the Vulkan backend.
std::unique_ptr<Renderer> createVulkanRenderer();

} // namespace maz::render
