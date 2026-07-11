#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace maz::render {

class VulkanContext;

// Separable downsampled bloom. Given the resolved HDR scene color, it runs three half-resolution
// fullscreen passes into two ping-pong float targets: (1) bright-pass + 2x2 downsample of the scene,
// (2) horizontal Gaussian blur, (3) vertical Gaussian blur. The blurred result (bloomView()) is then
// sampled and added by the composite pass. Half-res keeps it cheap and widens the glow; separable
// blur is O(2n) taps instead of O(n^2).
class BloomChain {
public:
    bool init(VulkanContext& ctx, VkFormat hdrFormat, VkExtent2D sceneExtent, VkImageView sceneView,
              VkSampler sceneSampler);
    // Rebuild the half-res targets after a swapchain resize and re-point the scene input.
    bool resize(VulkanContext& ctx, VkExtent2D sceneExtent, VkImageView sceneView,
                VkSampler sceneSampler);
    void shutdown(VulkanContext& ctx);

    void setThreshold(float t) { m_threshold = t; }

    // Record the three bloom passes. Call after the scene pass (which left sceneColor in
    // SHADER_READ), before the composite pass.
    void record(VkCommandBuffer cmd);

    // The blurred bloom result and a sampler for it, for the composite descriptor.
    VkImageView bloomView() const { return m_viewB; }
    VkSampler sampler() const { return m_sampler; }

private:
    struct Target {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkFramebuffer fbo = VK_NULL_HANDLE;
    };

    bool createTargets(VulkanContext& ctx, VkExtent2D sceneExtent);
    bool createRenderPass(VulkanContext& ctx);
    bool createSampler(VulkanContext& ctx);
    bool createDescriptors(VulkanContext& ctx);
    bool createPipelines(VulkanContext& ctx);
    void writeSet(VulkanContext& ctx, VkDescriptorSet set, VkImageView view);
    void destroyTargets(VulkanContext& ctx);
    void doPass(VkCommandBuffer cmd, VkFramebuffer fbo, VkPipeline pipeline, VkDescriptorSet src,
                const float push[4]);

    VkFormat m_format = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkExtent2D m_halfExtent{};

    Target m_a; // ping
    Target m_b; // pong (holds the final blurred bloom)
    // Convenience aliases for the two views (m_a.view / m_b.view).
    VkImageView m_viewA = VK_NULL_HANDLE;
    VkImageView m_viewB = VK_NULL_HANDLE;

    VkRenderPass m_pass = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    VkDescriptorSet m_setScene = VK_NULL_HANDLE; // samples the full-res scene color
    VkDescriptorSet m_setA = VK_NULL_HANDLE;     // samples target A
    VkDescriptorSet m_setB = VK_NULL_HANDLE;     // samples target B

    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_down = VK_NULL_HANDLE; // bright-pass + downsample
    VkPipeline m_blur = VK_NULL_HANDLE; // separable Gaussian

    float m_threshold = 0.75f;
};

} // namespace maz::render
