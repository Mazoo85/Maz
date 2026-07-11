#pragma once

#include "maz/render/Renderer.hpp"
#include "render/VulkanBuffer.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace maz::render {

class VulkanContext;

// Minimal indexed 3D mesh renderer: one pipeline (position/normal/color vertices, depth-tested,
// back-face culled, directional lighting in the shader). Meshes are uploaded once; draws are
// collected each frame and recorded with an MVP + model push constant.
class MeshRenderer {
public:
    bool init(VulkanContext& ctx, VkRenderPass renderPass);
    void shutdown(VulkanContext& ctx);

    MeshHandle createMesh(VulkanContext& ctx, const MeshVertex* vertices, uint32_t vertexCount,
                          const uint32_t* indices, uint32_t indexCount);

    void setViewProjection(const float* viewProj16);
    void setViewport(uint32_t w, uint32_t h) {
        m_viewportW = w;
        m_viewportH = h;
    }

    void begin();
    void draw(MeshHandle mesh, const float* model16);
    void flush(VkCommandBuffer cmd);

private:
    struct Mesh {
        VulkanBuffer vbo;
        VulkanBuffer ibo;
        uint32_t indexCount = 0;
    };
    struct DrawCmd {
        MeshHandle mesh;
        float model[16];
    };

    bool createPipeline(VulkanContext& ctx, VkRenderPass renderPass);

    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    std::vector<Mesh> m_meshes; // index 0 reserved (kInvalidMesh)
    std::vector<DrawCmd> m_cmds;
    float m_viewProj[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    uint32_t m_viewportW = 0;
    uint32_t m_viewportH = 0;
};

} // namespace maz::render
