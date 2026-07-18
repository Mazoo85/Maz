#pragma once

#include "maz/math/Math.hpp"
#include "maz/render/Renderer.hpp" // Color, Sprite

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <vector>

namespace maz::render {

class VulkanContext;

// Batched 2D sprite renderer. Backend-internal, like MeshRenderer: the public Renderer interface
// exposes uploadTexture()/drawSprite(); this owns the textured-quad pipeline, one combined
// image-sampler descriptor set per texture, and a per-frame dynamic vertex/index buffer.
//
// Coordinates are screen pixels with a top-left origin (see maz::math::ortho2D). Quads are
// accumulated on the CPU between begin() and flush() and coalesced into one draw per run of
// consecutive same-texture sprites, so a batch of sprites sharing a texture costs a single
// vkCmdDrawIndexed. Depth test/write are off and alpha blending is on, so sprites composite in
// submission order — the natural model for HUDs, tilemaps and 2D scenes.
//
// This is also the engine's first descriptor-set + staging-upload path (roadmap Phase 3/5):
// textures are uploaded to device-local images via a staging buffer and a one-time layout
// transition, then bound through a descriptor set.
class SpriteRenderer {
public:
    // One interleaved sprite vertex: screen-space position, texture UV, and an RGBA tint.
    struct Vertex {
        float pos[2];
        float uv[2];
        float color[4];
    };

    // Build the pipeline against `renderPass` (must have a color attachment; a depth attachment is
    // tolerated but unused). `framesInFlight` sizes the ring of per-frame dynamic buffers — pass 1
    // for a single-shot offscreen target, kMaxFramesInFlight for the swapchain. Loads
    // shaders/sprite.vert.spv + sprite.frag.spv from beside the executable. Returns false (logs) on
    // failure; the caller may continue without 2D.
    bool init(VulkanContext& ctx, VkRenderPass renderPass, uint32_t framesInFlight);

    // Upload tightly packed RGBA8 pixels (width*height*4 bytes, top row first) as a texture.
    // Returns a handle (>= 0) for drawSprite()/draw(), or -1 on failure. Sampled with nearest
    // filtering (crisp pixel art) and clamp-to-edge addressing.
    int createTexture(VulkanContext& ctx, const uint8_t* rgba, uint32_t width, uint32_t height);

    // Begin accumulating a batch for frame `frameIndex` (< framesInFlight). `screen` is the target
    // size in pixels and drives the orthographic projection. Clears any quads from a prior frame.
    void begin(uint32_t frameIndex, VkExtent2D screen);

    // Queue one sprite. `x,y` is the top-left of the destination rect in pixels, `w,h` its size.
    // The source UV rect (u0,v0)-(u1,v1) selects a sub-region of the texture (full texture by
    // default; a smaller rect is an atlas cell). `rotation` (radians) spins the quad about its
    // centre. No-op if the texture handle is invalid.
    void draw(int texture, const Sprite& sprite);

    // Record every queued quad for the current frame into `cmd` (must be inside a render pass
    // compatible with the one from init()). May grow the frame's dynamic buffers via `ctx`.
    void flush(VulkanContext& ctx, VkCommandBuffer cmd);

    void destroy(VulkanContext& ctx);

    bool ready() const { return m_pipeline != VK_NULL_HANDLE; }
    int textureCount() const { return static_cast<int>(m_textures.size()); }

private:
    struct Texture {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkDescriptorSet set = VK_NULL_HANDLE;
    };

    // A run of consecutive quads that share a texture — collapses to one draw call.
    struct Batch {
        int texture = -1;
        uint32_t firstIndex = 0;
        uint32_t indexCount = 0;
    };

    // Host-visible, per-frame geometry. Sized on demand; a frame's buffer is only rewritten after
    // its in-flight fence has been waited on by the caller, so growing it in place is safe.
    struct FrameBuffers {
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
        VkDeviceSize vertexCapacity = 0;
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indexMemory = VK_NULL_HANDLE;
        VkDeviceSize indexCapacity = 0;
    };

    bool createPipeline(VulkanContext& ctx, VkRenderPass renderPass);
    bool ensureCapacity(VulkanContext& ctx, FrameBuffers& fb, VkDeviceSize vbytes,
                        VkDeviceSize ibytes);
    static void destroyFrame(VkDevice device, FrameBuffers& fb);

    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;

    // Transient command pool + fence reused for one-time texture uploads.
    VkCommandPool m_uploadPool = VK_NULL_HANDLE;
    VkFence m_uploadFence = VK_NULL_HANDLE;

    std::vector<Texture> m_textures;
    std::vector<FrameBuffers> m_frames;

    // Current-frame CPU accumulation.
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::vector<Batch> m_batches;
    uint32_t m_frameIndex = 0;
    VkExtent2D m_screen{0, 0};
};

} // namespace maz::render
