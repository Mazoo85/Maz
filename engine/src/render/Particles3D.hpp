#pragma once

#include "render/VulkanBuffer.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace maz::render {

class VulkanContext;

// World-space, camera-facing billboard particles for the 3D scene (glowing embers, sparks, smoke).
// Each particle is a quad expanded in the vertex shader along the camera's right/up axes, so it
// always faces the camera. Additive-blended, depth-tested against the scene but not depth-writing.
// Drawn inside the scene pass after meshes, before the 2D layer.
class Particles3D {
public:
    bool init(VulkanContext& ctx, VkRenderPass renderPass, uint32_t framesInFlight,
              VkSampleCountFlagBits samples);
    void shutdown(VulkanContext& ctx);

    void begin(); // clear the frame's particles
    void draw(const float pos[3], float size, const float color[4]);
    // Record the billboards. viewProj16 column-major; right/up are the camera's world-space axes.
    void flush(VkCommandBuffer cmd, uint32_t frameIndex, const float viewProj16[16],
               const float right3[3], const float up3[3]);

    void setViewport(uint32_t w, uint32_t h) {
        m_viewportW = w;
        m_viewportH = h;
    }
    uint32_t particleCount() const { return static_cast<uint32_t>(m_vertices.size() / 6); }

private:
    struct Vertex {
        float center[3];
        float corner[2]; // offset along camera right/up, already scaled by size
        float uv[2];
        float color[4];
    };

    bool createPipeline(VulkanContext& ctx, VkRenderPass renderPass);
    bool createVertexBuffers(VulkanContext& ctx, uint32_t framesInFlight);

    VkSampleCountFlagBits m_samples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;

    std::vector<VulkanBuffer> m_vbo; // one per frame-in-flight
    std::vector<void*> m_vboMapped;
    uint32_t m_maxVertices = 0;

    std::vector<Vertex> m_vertices;
    bool m_capacityWarned = false;
    uint32_t m_viewportW = 0;
    uint32_t m_viewportH = 0;
};

} // namespace maz::render
