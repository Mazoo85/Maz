#pragma once

#include "maz/render/Renderer.hpp" // TextureHandle, SpriteDesc, Camera2D
#include "render/VulkanBuffer.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace maz::render {

class VulkanContext;
class TextureStore;

// Batched 2D sprite renderer. Sprites are transformed to quads on the CPU, streamed into a
// per-frame dynamic vertex buffer, and drawn in runs grouped by texture. Draws are recorded into
// the caller's command buffer inside an already-active render pass.
class SpriteRenderer {
public:
    bool init(VulkanContext& ctx, TextureStore& store, VkRenderPass renderPass,
              uint32_t framesInFlight, VkSampleCountFlagBits samples);
    void shutdown(VulkanContext& ctx);

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
    uint32_t spriteCount() const { return static_cast<uint32_t>(m_vertices.size() / 6); }
    void draw(TextureHandle tex, const SpriteDesc& s);
    // Fill a convex polygon (triangle fan from points[0]) with a flat color, using `whiteTex`.
    void fillPolygon(TextureHandle whiteTex, const Point2* points, uint32_t count, const Color& color,
                     BlendMode blend = BlendMode::Alpha);
    // Fill a triangle fan from verts[0] with per-vertex (interpolated) color, using `whiteTex`.
    void fillPolygonFan(TextureHandle whiteTex, const PolyVertex* verts, uint32_t count,
                        BlendMode blend = BlendMode::Alpha);
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
        Camera2D cam;                        // camera active when this batch was recorded
        BlendMode blend = BlendMode::Alpha;  // alpha (over) or additive compositing
    };

    bool createPipeline(VulkanContext& ctx, VkRenderPass renderPass);
    bool createVertexBuffers(VulkanContext& ctx, uint32_t framesInFlight);

    TextureStore* m_store = nullptr;    // shared texture registry (not owned)
    VkSampleCountFlagBits m_samples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;     // alpha (over) blend
    VkPipeline m_pipelineAdd = VK_NULL_HANDLE;  // additive blend (same layout/shaders)

    std::vector<VulkanBuffer> m_vbo;    // one per frame-in-flight
    std::vector<void*> m_vboMapped;
    uint32_t m_maxVertices = 0;

    std::vector<Vertex> m_vertices;
    std::vector<Batch> m_batches;
    bool m_capacityWarned = false;

    Camera2D m_camera{};
    bool m_cameraChanged = true;
    uint32_t m_viewportW = 0;
    uint32_t m_viewportH = 0;
};

} // namespace maz::render
