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
              VkSampler sceneSampler, VkImageView bloomView, VkSampler bloomSampler);
    void shutdown(VulkanContext& ctx);

    // Re-point the sampled scene color + bloom after a swapchain rebuild (resize).
    void updateSource(VulkanContext& ctx, VkImageView sceneView, VkSampler sceneSampler,
                      VkImageView bloomView, VkSampler bloomSampler);

    void setBloom(float strength, float threshold) {
        m_strength = strength;
        m_threshold = threshold;
    }
    // Enable ACES tonemapping of the HDR scene with the given exposure. Off by default so apps that
    // don't opt in are a faithful passthrough (values already in [0,1] are written unchanged).
    void setTonemap(float exposure, bool enabled) {
        m_exposure = exposure;
        m_tonemap = enabled;
    }
    // Enable a color grade: vignette strength (0..1 corner darkening), saturation (1 = neutral),
    // contrast (1 = neutral). Off by default so apps that don't opt in are unchanged.
    void setColorGrade(float vignette, float saturation, float contrast, bool enabled) {
        m_vignette = vignette;
        m_saturation = saturation;
        m_contrast = contrast;
        m_colorGrade = enabled;
    }
    // Radial chromatic-aberration strength (UV split fraction at the screen edge). 0 (default) = off.
    void setChromatic(float strength) { m_chromatic = strength; }

    // Begin the composite pass into `framebuffer`, draw the full-screen composite, end the pass.
    void record(VkCommandBuffer cmd, VkFramebuffer framebuffer, VkExtent2D extent);

private:
    bool createPipeline(VulkanContext& ctx, VkRenderPass compositePass);
    void writeDescriptor(VulkanContext& ctx, VkImageView sceneView, VkSampler sceneSampler,
                         VkImageView bloomView, VkSampler bloomSampler);

    VkRenderPass m_compositePass = VK_NULL_HANDLE; // not owned
    VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    VkDescriptorSet m_set = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;

    float m_strength = 0.0f;   // 0 => passthrough
    float m_threshold = 0.75f; // brightness above which pixels bloom
    float m_exposure = 1.0f;   // HDR exposure multiplier before tonemapping
    bool m_tonemap = false;    // false => no tonemap (passthrough of [0,1] values)
    float m_vignette = 0.0f;   // corner darkening amount
    float m_saturation = 1.0f; // 1 = neutral
    float m_contrast = 1.0f;   // 1 = neutral
    bool m_colorGrade = false; // false => no grade (passthrough)
    float m_chromatic = 0.0f;  // radial RGB split; 0 => off
};

} // namespace maz::render
