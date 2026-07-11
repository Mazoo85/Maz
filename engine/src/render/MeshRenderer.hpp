#pragma once

#include "maz/render/Renderer.hpp"
#include "render/VulkanBuffer.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace maz::render {

class VulkanContext;
class TextureStore;

// Indexed 3D mesh renderer with shadow mapping. Each frame it first renders queued meshes into a
// depth-only shadow map from a directional light (renderShadow), then the main pass samples that
// map. Meshes are position/normal/color/uv, depth-tested, back-face left on (cull none), lit by a
// directional term modulated by the shadow.
class MeshRenderer {
public:
    bool init(VulkanContext& ctx, TextureStore& store, VkRenderPass renderPass,
              VkSampleCountFlagBits samples);
    void shutdown(VulkanContext& ctx);

    MeshHandle createMesh(VulkanContext& ctx, const MeshVertex* vertices, uint32_t vertexCount,
                          const uint32_t* indices, uint32_t indexCount);

    void setViewProjection(const float* viewProj16);
    void setCameraPosition(const float* pos3) {
        m_camPos[0] = pos3[0];
        m_camPos[1] = pos3[1];
        m_camPos[2] = pos3[2];
    }
    void setLighting(VulkanContext& ctx, const SceneLighting& lighting); // updates the lights UBO
    void setViewport(uint32_t w, uint32_t h) {
        m_viewportW = w;
        m_viewportH = h;
    }

    void begin();
    void draw(MeshHandle mesh, const float* model16, TextureHandle texture);

    bool hasDraws() const { return !m_cmds.empty(); }
    void renderShadow(VkCommandBuffer cmd); // depth-only pass into the shadow map (own render pass)
    void renderSky(VkCommandBuffer cmd);    // gradient sky background (call at main-pass start)
    void flush(VkCommandBuffer cmd);        // main color pass (call inside the main render pass)

private:
    static constexpr uint32_t kShadowSize = 2048;

    struct Mesh {
        VulkanBuffer vbo;
        VulkanBuffer ibo;
        uint32_t indexCount = 0;
    };
    struct DrawCmd {
        MeshHandle mesh;
        TextureHandle texture;
        float model[16];
    };

    bool createShadowResources(VulkanContext& ctx);
    bool createShadowPipeline(VulkanContext& ctx);
    bool createSkyPipeline(VulkanContext& ctx, VkRenderPass renderPass);
    bool createLightResources(VulkanContext& ctx);
    bool createPipeline(VulkanContext& ctx, VkRenderPass renderPass);

    TextureStore* m_store = nullptr; // shared texture registry (not owned)
    VkSampleCountFlagBits m_samples = VK_SAMPLE_COUNT_1_BIT; // main/sky pass sample count

    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_skyLayout = VK_NULL_HANDLE;
    VkPipeline m_skyPipeline = VK_NULL_HANDLE;

    // Shadow map + its depth-only pass/pipeline, and a set-1 descriptor exposing it to the main
    // fragment shader.
    VkImage m_shadowImage = VK_NULL_HANDLE;
    VkDeviceMemory m_shadowMemory = VK_NULL_HANDLE;
    VkImageView m_shadowView = VK_NULL_HANDLE;
    VkSampler m_shadowSampler = VK_NULL_HANDLE;
    VkRenderPass m_shadowPass = VK_NULL_HANDLE;
    VkFramebuffer m_shadowFbo = VK_NULL_HANDLE;
    VkPipelineLayout m_shadowLayout = VK_NULL_HANDLE;
    VkPipeline m_shadowPipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_shadowSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_shadowPool = VK_NULL_HANDLE;
    VkDescriptorSet m_shadowSet = VK_NULL_HANDLE;

    // set = 2 : scene lighting (ambient + sun + point lights), a host-visible UBO.
    VulkanBuffer m_lightUbo;
    VkDescriptorSetLayout m_lightSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_lightPool = VK_NULL_HANDLE;
    VkDescriptorSet m_lightSet = VK_NULL_HANDLE;
    void* m_lightMapped = nullptr;

    std::vector<Mesh> m_meshes; // index 0 reserved (kInvalidMesh)
    std::vector<DrawCmd> m_cmds;
    float m_viewProj[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    float m_lightVP[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    float m_camPos[3] = {0, 0, 0};
    // Sky/sun params (from setLighting) fed to the sky pass so it matches the scene lighting.
    float m_sunDir[3] = {0.4f, 0.8f, 0.6f};
    float m_sunColor[3] = {1.0f, 0.95f, 0.8f};
    float m_skyZenith[3] = {0.24f, 0.44f, 0.82f};
    float m_skyHorizon[3] = {0.72f, 0.82f, 0.95f};
    float m_skyGround[3] = {0.42f, 0.45f, 0.50f};
    uint32_t m_viewportW = 0;
    uint32_t m_viewportH = 0;
};

} // namespace maz::render
