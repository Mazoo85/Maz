#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace maz::render {

class VulkanContext;

// A GPU texture: device-local RGBA8 image + view + sampler, uploaded via a staging buffer.
class VulkanTexture {
public:
    // Upload tightly-packed RGBA8 pixels. Returns false on failure.
    bool create(VulkanContext& ctx, uint32_t width, uint32_t height, const void* rgba);
    // Decode an image file (PNG/JPG/…) to RGBA8 via stb_image and upload it.
    bool createFromFile(VulkanContext& ctx, const char* path);
    void destroy(VulkanContext& ctx);

    VkImageView view() const { return m_view; }
    VkSampler sampler() const { return m_sampler; }
    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    bool valid() const { return m_image != VK_NULL_HANDLE; }

private:
    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VkImageView m_view = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

} // namespace maz::render
