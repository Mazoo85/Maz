#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace maz::render {

class VulkanContext;

// Swapchain + a single-color render pass + per-image framebuffers. Rebuilt on resize.
class VulkanSwapchain {
public:
    bool create(VulkanContext& ctx, uint32_t width, uint32_t height, bool vsync);
    void destroy(VulkanContext& ctx);

    VkSwapchainKHR handle() const { return m_swapchain; }
    VkRenderPass renderPass() const { return m_renderPass; }
    VkExtent2D extent() const { return m_extent; }
    VkFormat format() const { return m_format; }
    uint32_t imageCount() const { return static_cast<uint32_t>(m_images.size()); }
    VkFramebuffer framebuffer(uint32_t i) const { return m_framebuffers[i]; }

private:
    bool createRenderPass(VulkanContext& ctx);
    bool createFramebuffers(VulkanContext& ctx);

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent{};
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_views;
    std::vector<VkFramebuffer> m_framebuffers;
};

} // namespace maz::render
