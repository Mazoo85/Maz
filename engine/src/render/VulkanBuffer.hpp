#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace maz::render {

class VulkanContext;

// Find a memory type index satisfying `typeFilter` and `properties`, or UINT32_MAX if none.
uint32_t findMemoryType(VulkanContext& ctx, uint32_t typeFilter, VkMemoryPropertyFlags properties);

// Thin RAII-ish wrapper over a VkBuffer + its backing VkDeviceMemory.
// Host-visible buffers can be persistently mapped for cheap per-frame streaming (sprite verts).
class VulkanBuffer {
public:
    bool create(VulkanContext& ctx, VkDeviceSize size, VkBufferUsageFlags usage,
                VkMemoryPropertyFlags memProps);
    void destroy(VulkanContext& ctx);

    // Map/keep-mapped host-visible memory. Returns nullptr on failure or for device-local memory.
    void* map(VulkanContext& ctx);
    void unmap(VulkanContext& ctx);

    VkBuffer handle() const { return m_buffer; }
    VkDeviceSize size() const { return m_size; }
    bool valid() const { return m_buffer != VK_NULL_HANDLE; }

private:
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VkDeviceSize m_size = 0;
    void* m_mapped = nullptr;
};

} // namespace maz::render
