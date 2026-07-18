#pragma once

#include "maz/assets/Model.hpp"
#include "maz/math/Math.hpp"
#include "maz/render/Renderer.hpp" // for maz::render::Color

#include <cstdint>
#include <memory>
#include <vector>

// Renders a scene to an off-screen image and reads the pixels back to the CPU — no window, no
// swapchain, no display. Its purpose is automated verification: with a software Vulkan driver
// (lavapipe) it lets CI actually render the mesh pipeline and check the resulting pixels, so the
// renderer is verified to *draw correctly*, not merely to compile. Vulkan is hidden behind a PImpl.

namespace maz::platform {
class Window;
}

namespace maz::render {

class OffscreenRenderer {
public:
    OffscreenRenderer();
    ~OffscreenRenderer();

    OffscreenRenderer(const OffscreenRenderer&) = delete;
    OffscreenRenderer& operator=(const OffscreenRenderer&) = delete;

    // Create a headless Vulkan device and a `width`x`height` render target. Returns false if no
    // Vulkan device is available (no ICD/GPU), in which case valid() stays false.
    bool init(platform::Window& window, uint32_t width, uint32_t height);

    // Upload a model; returns a handle (>= 0) for Item::handle, or -1 on failure.
    int uploadModel(const assets::Model& model);

    // Upload an RGBA8 texture; returns a handle (>= 0) for SpriteItem::texture, or -1 on failure.
    int uploadTexture(const uint8_t* rgba, uint32_t width, uint32_t height);

    struct Item {
        int handle = -1;
        math::mat4 mvp{1.0f};
        math::mat4 model{1.0f};
    };

    struct SpriteItem {
        int texture = -1;
        Sprite sprite;
    };

    // Render one frame (clear + draw the items) and read the color image back as tightly packed
    // RGBA8 rows (width*height*4 bytes). Returns false on failure.
    bool renderToPixels(const Color& clear, const std::vector<Item>& items,
                        std::vector<uint8_t>& outRGBA);

    // Render one frame of 2D sprites (clear + draw the sprites) and read the color image back,
    // same packing as renderToPixels(). Proves the sprite pipeline draws correct pixels. Returns
    // false on failure (including when the sprite pipeline is unavailable).
    bool renderSpritesToPixels(const Color& clear, const std::vector<SpriteItem>& sprites,
                               std::vector<uint8_t>& outRGBA);

    void shutdown();
    bool valid() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace maz::render
