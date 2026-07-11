#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace maz::platform {
class Window;
}

namespace maz::render {

// Owns the core Vulkan objects: instance, (optional) surface, physical + logical device, queues.
// init() degrades gracefully: if no suitable GPU/ICD exists it logs and returns false without
// aborting, so headless CI can proceed with an inactive renderer.
class VulkanContext {
public:
    bool init(platform::Window& window, bool wantSurface, bool enableValidation);
    void shutdown();

    bool valid() const { return m_device != VK_NULL_HANDLE; }
    bool hasSurface() const { return m_surface != VK_NULL_HANDLE; }

    VkInstance instance() const { return m_instance; }
    VkPhysicalDevice physicalDevice() const { return m_physical; }
    VkDevice device() const { return m_device; }
    VkSurfaceKHR surface() const { return m_surface; }
    VkQueue graphicsQueue() const { return m_graphicsQueue; }
    VkQueue presentQueue() const { return m_presentQueue; }
    uint32_t graphicsFamily() const { return m_graphicsFamily; }
    uint32_t presentFamily() const { return m_presentFamily; }

    // One-time command submission on the graphics queue (staging uploads, layout transitions).
    // Simple and synchronous: allocates, records, submits, waits idle, frees. Fine for uploads
    // done at load time. Returns VK_NULL_HANDLE if the context is invalid.
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer cmd);

private:
    bool createInstance(platform::Window& window, bool wantSurface, bool enableValidation);
    bool pickPhysicalDevice(bool wantSurface);
    bool createLogicalDevice(bool wantSurface);
    bool createTransientPool();

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    VkCommandPool m_transientPool = VK_NULL_HANDLE;
    uint32_t m_graphicsFamily = 0;
    uint32_t m_presentFamily = 0;
    bool m_validation = false;
};

} // namespace maz::render
