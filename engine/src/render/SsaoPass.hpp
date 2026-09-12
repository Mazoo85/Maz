#pragma once

#include "render/VulkanBuffer.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>

namespace maz::render {

class VulkanContext;

// Screen-space ambient occlusion. Given the camera depth prepass, it runs two full-resolution
// fullscreen passes into single-channel (R8) targets: (1) the SSAO pass reconstructs world position
// and a face normal from depth, samples a hemisphere, and writes a raw occlusion factor; (2) a 4x4
// box blur smooths the hashed sampling noise. The blurred result (aoView()) is multiplied into the
// scene by the composite pass. Disabled by default => the composite binds a white AO (no effect).
class SsaoPass {
public:
    bool init(VulkanContext& ctx, VkExtent2D extent, VkImageView depthView, VkSampler depthSampler);
    // Rebuild the full-res targets after a swapchain resize and re-point the depth input.
    bool resize(VulkanContext& ctx, VkExtent2D extent, VkImageView depthView, VkSampler depthSampler);
    void shutdown(VulkanContext& ctx);

    void setParams(float radius, float strength, float bias, float power) {
        m_radius = radius;
        m_strength = strength;
        m_bias = bias;
        m_power = power;
    }
    void setEnabled(bool e) { m_enabled = e; }
    bool enabled() const { return m_enabled; }

    // Record the SSAO + blur passes. Call after the depth prepass, before the scene pass. The 16-float
    // matrices are the camera view-projection and its inverse; camPos3 is the world-space camera.
    void record(VulkanContext& ctx, VkCommandBuffer cmd, const float viewProj16[16],
                const float invViewProj16[16], const float camPos3[3]);

    // The blurred AO result and a sampler for it, for the composite descriptor.
    VkImageView aoView() const { return m_blur.view; }
    VkSampler sampler() const { return m_sampler; }

private:
    struct Target {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkFramebuffer fbo = VK_NULL_HANDLE;
    };
    struct CamUbo {
        float viewProj[16];
        float invViewProj[16];
        float camPos[4];
        float params[4]; // radius, strength, bias, power
    };

    bool createRenderPass(VulkanContext& ctx);
    bool createSampler(VulkanContext& ctx);
    bool createTargets(VulkanContext& ctx, VkExtent2D extent);
    bool createDescriptors(VulkanContext& ctx);
    bool createPipelines(VulkanContext& ctx);
    void writeSsaoSet(VulkanContext& ctx, VkImageView depthView);
    void writeBlurSet(VulkanContext& ctx);
    void destroyTargets(VulkanContext& ctx);
    void doPass(VkCommandBuffer cmd, VkFramebuffer fbo, VkPipeline pipeline, VkPipelineLayout layout,
                VkDescriptorSet set);

    VkFormat m_format = VK_FORMAT_R8_UNORM;
    VkExtent2D m_extent{};
    bool m_enabled = false;
    float m_radius = 0.5f;
    float m_strength = 1.0f;
    float m_bias = 0.025f;
    float m_power = 1.5f;

    Target m_ssao; // raw AO
    Target m_blur; // blurred AO (the result)

    VkRenderPass m_pass = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;

    // SSAO pass: samples depth (binding 0) + a camera UBO (binding 1).
    VkDescriptorSetLayout m_ssaoSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_ssaoSet = VK_NULL_HANDLE;
    VkPipelineLayout m_ssaoLayout = VK_NULL_HANDLE;
    VkPipeline m_ssaoPipeline = VK_NULL_HANDLE;
    VulkanBuffer m_camUbo; // host-visible, updated each frame
    void* m_camMapped = nullptr;

    // Blur pass: samples the raw AO (binding 0).
    VkDescriptorSetLayout m_blurSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_blurSet = VK_NULL_HANDLE;
    VkPipelineLayout m_blurLayout = VK_NULL_HANDLE;
    VkPipeline m_blurPipeline = VK_NULL_HANDLE;

    VkDescriptorPool m_pool = VK_NULL_HANDLE;
};

} // namespace maz::render
