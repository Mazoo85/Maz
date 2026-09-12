#pragma once

#include "render/VulkanBuffer.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace maz::render {

class VulkanContext;

// World-space debug-line renderer: arbitrary colored lines, boxes, and gizmos for visualizing
// colliders, light ranges, paths, and frustums. LINE_LIST topology, unlit vertex color, depth-
// tested against the scene (so lines are occluded by geometry) but not depth-writing. Drawn inside
// the scene pass after meshes. One host-visible vertex buffer per frame-in-flight, like the
// particle renderer.
class DebugDraw {
public:
    bool init(VulkanContext& ctx, VkRenderPass renderPass, uint32_t framesInFlight,
              VkSampleCountFlagBits samples);
    void shutdown(VulkanContext& ctx);

    void begin(); // clear this frame's lines
    void line(const float a[3], const float b[3], const float color[4]);
    void aabb(const float min[3], const float max[3], const float color[4]); // 12 edges
    void flush(VkCommandBuffer cmd, uint32_t frameIndex, const float viewProj16[16]);

    void setViewport(uint32_t w, uint32_t h) {
        m_viewportW = w;
        m_viewportH = h;
    }
    bool hasLines() const { return !m_verts.empty(); }

private:
    struct Vertex {
        float pos[3];
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

    std::vector<Vertex> m_verts;
    bool m_capacityWarned = false;
    uint32_t m_viewportW = 0;
    uint32_t m_viewportH = 0;
};

} // namespace maz::render
