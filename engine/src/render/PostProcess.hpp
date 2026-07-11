#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace maz::render {

class VulkanContext;

// Full-screen composite/post-process: samples the resolved scene color and writes the swapchain
// image through the composite render pass, applying tonemap + a threshold bloom. With bloom
// strength 0 (default) it is a faithful passthrough, so 2D/3D apps look identical.
class PostProcess {
public:
    bool init(VulkanContext& ctx, VkRenderPass compositePass, VkImageView sceneView,
              VkSampler sceneSampler);
    void shutdown(VulkanContext& ctx);

    // Re-point the sampled scene color after a swapchain rebuild (resize).
    void updateSource(VulkanContext& ctx, VkImageView sceneView, VkSampler sceneSampler);

    void setBloom(float strength, float threshold) {
        m_strength = strength;
        m_threshold = threshold;
    }

    // Begin the composite pass into `framebuffer`, draw the full-screen composite, end the pass.
    void record(VkCommandBuffer cmd, VkFramebuffer framebuffer, VkExtent2D extent);

private:
    bool createPipeline(VulkanContext& ctx, VkRenderPass compositePass);
    void writeDescriptor(VulkanContext& ctx, VkImageView view, VkSampler sampler);

    VkRenderPass m_compositePass = VK_NULL_HANDLE; // not owned
    VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    VkDescriptorSet m_set = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;

    float m_strength = 0.0f;   // 0 => passthrough
    float m_threshold = 0.75f; // brightness above which pixels bloom
};

} // namespace maz::render
