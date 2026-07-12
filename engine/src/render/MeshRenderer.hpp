#pragma once

#include "maz/assets/Model.hpp"
#include "maz/math/Math.hpp"

#include <vulkan/vulkan.h>

#include <vector>

namespace maz::render {

class VulkanContext;

// Draws maz::assets::Model meshes with a simple lit pipeline. Backend-internal: the public
// Renderer interface exposes uploadModel()/drawModel(); this owns the Vulkan pipeline and the
// per-mesh vertex/index buffers. Compatible with any render pass that has a color + depth
// attachment (e.g. the swapchain's), and uses dynamic viewport/scissor so resizes need no rebuild.
class MeshRenderer {
public:
    // Build the pipeline against `renderPass`. Loads shaders/mesh.vert.spv + mesh.frag.spv from
    // beside the executable. Returns false (and logs) on failure.
    bool init(VulkanContext& ctx, VkRenderPass renderPass);

    // Upload a model's geometry to GPU buffers, replacing any previously uploaded model.
    bool uploadModel(VulkanContext& ctx, const assets::Model& model);

    // Record draw commands into `cmd` (must be inside a compatible render pass). `mvp` is the full
    // model-view-projection; `model` is the model matrix alone (for transforming normals).
    void draw(VkCommandBuffer cmd, const math::mat4& mvp, const math::mat4& model,
              VkExtent2D extent) const;

    void destroy(VulkanContext& ctx);

    bool ready() const { return m_pipeline != VK_NULL_HANDLE; }
    bool hasMesh() const { return !m_meshes.empty(); }

private:
    struct GpuMesh {
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indexMemory = VK_NULL_HANDLE;
        uint32_t indexCount = 0;
    };

    void destroyMeshes(VulkanContext& ctx);

    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    std::vector<GpuMesh> m_meshes;
};

} // namespace maz::render
