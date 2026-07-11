#include "render/VulkanBuffer.hpp"

#include "maz/core/Log.hpp"
#include "render/VulkanContext.hpp"

#include <cstdint>

namespace maz::render {

uint32_t findMemoryType(VulkanContext& ctx, uint32_t typeFilter,
                        VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(ctx.physicalDevice(), &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        const bool typeOk = (typeFilter & (1u << i)) != 0;
        const bool propsOk = (memProps.memoryTypes[i].propertyFlags & properties) == properties;
        if (typeOk && propsOk) {
            return i;
        }
    }
    return UINT32_MAX;
}

bool VulkanBuffer::create(VulkanContext& ctx, VkDeviceSize size, VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags memProps) {
    m_size = size;

    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(ctx.device(), &bi, nullptr, &m_buffer) != VK_SUCCESS) {
        MAZ_LOG_ERROR("vkCreateBuffer failed (%llu bytes)", (unsigned long long)size);
        return false;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(ctx.device(), m_buffer, &req);

    uint32_t typeIndex = findMemoryType(ctx, req.memoryTypeBits, memProps);
    if (typeIndex == UINT32_MAX) {
        MAZ_LOG_ERROR("no compatible memory type for buffer");
        return false;
    }

    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = typeIndex;
    if (vkAllocateMemory(ctx.device(), &ai, nullptr, &m_memory) != VK_SUCCESS) {
        MAZ_LOG_ERROR("vkAllocateMemory failed");
        return false;
    }
    vkBindBufferMemory(ctx.device(), m_buffer, m_memory, 0);
    return true;
}

void* VulkanBuffer::map(VulkanContext& ctx) {
    if (m_mapped) {
        return m_mapped;
    }
    if (vkMapMemory(ctx.device(), m_memory, 0, m_size, 0, &m_mapped) != VK_SUCCESS) {
        MAZ_LOG_ERROR("vkMapMemory failed");
        m_mapped = nullptr;
    }
    return m_mapped;
}

void VulkanBuffer::unmap(VulkanContext& ctx) {
    if (m_mapped) {
        vkUnmapMemory(ctx.device(), m_memory);
        m_mapped = nullptr;
    }
}

void VulkanBuffer::destroy(VulkanContext& ctx) {
    unmap(ctx);
    if (m_buffer) {
        vkDestroyBuffer(ctx.device(), m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
    }
    if (m_memory) {
        vkFreeMemory(ctx.device(), m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
    m_size = 0;
}

} // namespace maz::render
