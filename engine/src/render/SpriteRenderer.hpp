#pragma once

#include "maz/render/Renderer.hpp" // TextureHandle, SpriteDesc, Camera2D
#include "render/VulkanBuffer.hpp"
#include "render/VulkanTexture.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace maz::render {

class VulkanContext;

// Batched 2D sprite renderer. Sprites are transformed to quads on the CPU, streamed into a
// per-frame dynamic vertex buffer, and drawn in runs grouped by texture. Draws are recorded into
// the caller's command buffer inside an already-active render pass.
class SpriteRenderer {
public:
    bool init(VulkanContext& ctx, VkRenderPass renderPass, uint32_t framesInFlight);
    void shutdown(VulkanContext& ctx);

    TextureHandle createTexture(VulkanContext& ctx, uint32_t w, uint32_t h, const void* rgba);
    TextureHandle loadTexture(VulkanContext& ctx, const char* path);

    // May be called multiple times per frame; subsequent draws form new batches under this
    // camera (e.g. world-space follow camera, then a pixel-space HUD pass).
    void setCamera(const Camera2D& cam) {
        m_camera = cam;
        m_cameraChanged = true;
    }
    void setViewport(uint32_t w, uint32_t h) {
        m_viewportW = w;
        m_viewportH = h;
    }

    void begin();                                 // clear the frame's accumulated sprites
    void draw(TextureHandle tex, const SpriteDesc& s);
    void flush(VkCommandBuffer cmd, uint32_t frameIndex); // record the batched draws

private:
    struct Vertex {
        float pos[2];
        float uv[2];
        float color[4];
    };
    struct Batch {
        TextureHandle tex;
        uint32_t first;
        uint32_t count;
        Camera2D cam; // camera active when this batch was recorded
    };
    struct Entry {
        VulkanTexture texture;
        VkDescriptorSet set;
    };

    bool createDescriptorInfra(VulkanContext& ctx);
    bool createPipeline(VulkanContext& ctx, VkRenderPass renderPass);
    bool createVertexBuffers(VulkanContext& ctx, uint32_t framesInFlight);
    TextureHandle registerTexture(VulkanContext& ctx, VulkanTexture&& tex);

    VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;

    std::vector<Entry> m_textures;      // index 0 is a reserved invalid slot
    std::vector<VulkanBuffer> m_vbo;    // one per frame-in-flight
    std::vector<void*> m_vboMapped;
    uint32_t m_maxVertices = 0;
    uint32_t m_maxTextures = 0;

    std::vector<Vertex> m_vertices;
    std::vector<Batch> m_batches;
    bool m_capacityWarned = false;

    Camera2D m_camera{};
    bool m_cameraChanged = true;
    uint32_t m_viewportW = 0;
    uint32_t m_viewportH = 0;
};

} // namespace maz::render
