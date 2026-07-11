#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace maz::render {

class VulkanContext;

// Swapchain + an offscreen scene target + a composite pass. The scene (sky/meshes/sprites) renders
// into an MSAA color+depth pass that resolves to a single-sample, *sampled* sceneColor image; a
// separate composite pass then samples sceneColor and writes the swapchain image (where a
// post-process — tonemap/bloom — runs). Rebuilt on resize.
class VulkanSwapchain {
public:
    bool create(VulkanContext& ctx, uint32_t width, uint32_t height, bool vsync);
    void destroy(VulkanContext& ctx);

    VkSwapchainKHR handle() const { return m_swapchain; }
    // The scene render pass (what SpriteRenderer/MeshRenderer pipelines target).
    VkRenderPass renderPass() const { return m_renderPass; }
    VkFramebuffer sceneFramebuffer() const { return m_sceneFramebuffer; }
    // The composite pass writing the swapchain image, and its per-image framebuffers.
    VkRenderPass compositePass() const { return m_compositePass; }
    VkFramebuffer compositeFramebuffer(uint32_t i) const { return m_compositeFramebuffers[i]; }
    // The resolved scene color, sampled by the composite/post-process.
    VkImageView sceneColorView() const { return m_sceneView; }
    VkSampler sceneSampler() const { return m_sceneSampler; }

    VkExtent2D extent() const { return m_extent; }
    VkFormat format() const { return m_format; }
    VkFormat sceneFormat() const { return m_sceneFormat; } // HDR float format of the scene target
    VkSampleCountFlagBits samples() const { return m_samples; }
    uint32_t imageCount() const { return static_cast<uint32_t>(m_images.size()); }

private:
    void chooseSampleCount(VulkanContext& ctx);
    bool createColorTarget(VulkanContext& ctx);
    bool createDepthResources(VulkanContext& ctx);
    bool createSceneColor(VulkanContext& ctx);
    bool createRenderPass(VulkanContext& ctx);
    bool createCompositePass(VulkanContext& ctx);
    bool createFramebuffers(VulkanContext& ctx);

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;    // scene pass
    VkRenderPass m_compositePass = VK_NULL_HANDLE; // scene -> swapchain
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    // The offscreen scene renders to an HDR float target so emissive/bloom can exceed 1.0; the
    // composite pass tonemaps it down to the LDR swapchain (m_format).
    VkFormat m_sceneFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkFormat m_depthFormat = VK_FORMAT_D32_SFLOAT;
    VkSampleCountFlagBits m_samples = VK_SAMPLE_COUNT_1_BIT;
    VkExtent2D m_extent{};
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_views;
    std::vector<VkFramebuffer> m_compositeFramebuffers; // one per swapchain image
    VkFramebuffer m_sceneFramebuffer = VK_NULL_HANDLE;  // single (scene target)

    // Multisampled color + depth for the scene pass.
    VkImage m_colorImage = VK_NULL_HANDLE;
    VkDeviceMemory m_colorMemory = VK_NULL_HANDLE;
    VkImageView m_colorView = VK_NULL_HANDLE;
    VkImage m_depthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_depthMemory = VK_NULL_HANDLE;
    VkImageView m_depthView = VK_NULL_HANDLE;

    // Single-sample resolved scene color, sampled by the composite pass.
    VkImage m_sceneImage = VK_NULL_HANDLE;
    VkDeviceMemory m_sceneMemory = VK_NULL_HANDLE;
    VkImageView m_sceneView = VK_NULL_HANDLE;
    VkSampler m_sceneSampler = VK_NULL_HANDLE;
};

} // namespace maz::render
