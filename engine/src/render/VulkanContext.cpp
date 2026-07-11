#include "render/VulkanContext.hpp"

#include "maz/core/Log.hpp"
#include "maz/platform/Window.hpp"

#include <SDL3/SDL_vulkan.h>

#include <cstring>
#include <vector>

namespace maz::render {

using core::LogLevel;

namespace {

constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                             VkDebugUtilsMessageTypeFlagsEXT,
                                             const VkDebugUtilsMessengerCallbackDataEXT* data,
                                             void*) {
    LogLevel level = LogLevel::Trace;
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        level = LogLevel::Error;
    } else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        level = LogLevel::Warn;
    }
    core::logMessage(level, "vulkan", 0, "%s", data->pMessage);
    return VK_FALSE;
}

bool hasLayer(const char* name) {
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    for (const auto& l : layers) {
        if (std::strcmp(l.layerName, name) == 0) {
            return true;
        }
    }
    return false;
}

bool deviceHasSwapchainExt(VkPhysicalDevice dev) {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> exts(count);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, exts.data());
    for (const auto& e : exts) {
        if (std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

bool VulkanContext::init(platform::Window& window, bool wantSurface, bool enableValidation) {
    if (!createInstance(window, wantSurface, enableValidation)) {
        return false;
    }

    if (wantSurface) {
        if (!SDL_Vulkan_CreateSurface(window.sdl(), m_instance, nullptr, &m_surface)) {
            MAZ_LOG_WARN("SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
            m_surface = VK_NULL_HANDLE;
            wantSurface = false;
        }
    }

    if (!pickPhysicalDevice(wantSurface)) {
        return false;
    }
    if (!createLogicalDevice(wantSurface)) {
        return false;
    }
    return true;
}

bool VulkanContext::createInstance(platform::Window& window, bool wantSurface,
                                   bool enableValidation) {
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "Maz Sandbox";
    app.pEngineName = "Maz Engine";
    app.apiVersion = VK_API_VERSION_1_1;

    std::vector<const char*> extensions;
    if (wantSurface) {
        uint32_t sdlCount = 0;
        const char* const* sdlExt = SDL_Vulkan_GetInstanceExtensions(&sdlCount);
        if (!sdlExt) {
            MAZ_LOG_WARN("SDL_Vulkan_GetInstanceExtensions failed: %s", SDL_GetError());
            return false;
        }
        extensions.assign(sdlExt, sdlExt + sdlCount);
    }

    m_validation = enableValidation && hasLayer(kValidationLayer);
    if (enableValidation && !m_validation) {
        MAZ_LOG_WARN("validation requested but %s not available", kValidationLayer);
    }
    if (m_validation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app;
    ci.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();
    const char* layers[] = {kValidationLayer};
    if (m_validation) {
        ci.enabledLayerCount = 1;
        ci.ppEnabledLayerNames = layers;
    }

    VkResult r = vkCreateInstance(&ci, nullptr, &m_instance);
    if (r != VK_SUCCESS) {
        MAZ_LOG_WARN("vkCreateInstance failed (VkResult %d) — no Vulkan available", (int)r);
        return false;
    }

    if (m_validation) {
        auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
        if (create) {
            VkDebugUtilsMessengerCreateInfoEXT dbg{};
            dbg.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            dbg.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            dbg.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            dbg.pfnUserCallback = debugCallback;
            create(m_instance, &dbg, nullptr, &m_debugMessenger);
        }
    }
    (void)window;
    return true;
}

bool VulkanContext::pickPhysicalDevice(bool wantSurface) {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    if (count == 0) {
        MAZ_LOG_WARN("no Vulkan physical devices found (running without a GPU)");
        return false;
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

    VkPhysicalDevice best = VK_NULL_HANDLE;
    uint32_t bestGraphics = 0, bestPresent = 0;
    int bestScore = -1;

    for (VkPhysicalDevice dev : devices) {
        if (wantSurface && !deviceHasSwapchainExt(dev)) {
            continue;
        }
        uint32_t famCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &famCount, nullptr);
        std::vector<VkQueueFamilyProperties> fams(famCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &famCount, fams.data());

        bool foundGfx = false, foundPresent = false;
        uint32_t gfx = 0, present = 0;
        for (uint32_t i = 0; i < famCount; ++i) {
            if (fams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                if (!foundGfx) {
                    gfx = i;
                    foundGfx = true;
                }
            }
            if (wantSurface) {
                VkBool32 sup = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, m_surface, &sup);
                if (sup && !foundPresent) {
                    present = i;
                    foundPresent = true;
                }
            }
        }
        if (!foundGfx || (wantSurface && !foundPresent)) {
            continue;
        }

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(dev, &props);
        int score = (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? 1000 : 10;
        if (score > bestScore) {
            bestScore = score;
            best = dev;
            bestGraphics = gfx;
            bestPresent = wantSurface ? present : gfx;
        }
    }

    if (best == VK_NULL_HANDLE) {
        MAZ_LOG_WARN("no Vulkan device satisfies the required queues/extensions");
        return false;
    }

    m_physical = best;
    m_graphicsFamily = bestGraphics;
    m_presentFamily = bestPresent;

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(best, &props);
    MAZ_LOG_INFO("selected GPU: %s", props.deviceName);
    return true;
}

bool VulkanContext::createLogicalDevice(bool wantSurface) {
    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;

    VkDeviceQueueCreateInfo gfx{};
    gfx.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    gfx.queueFamilyIndex = m_graphicsFamily;
    gfx.queueCount = 1;
    gfx.pQueuePriorities = &priority;
    queueInfos.push_back(gfx);

    if (wantSurface && m_presentFamily != m_graphicsFamily) {
        VkDeviceQueueCreateInfo present = gfx;
        present.queueFamilyIndex = m_presentFamily;
        queueInfos.push_back(present);
    }

    std::vector<const char*> deviceExts;
    if (wantSurface) {
        deviceExts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    // Enable optional features we use where supported (wireframe needs fillModeNonSolid).
    VkPhysicalDeviceFeatures supported{};
    vkGetPhysicalDeviceFeatures(m_physical, &supported);
    VkPhysicalDeviceFeatures enabled{};
    enabled.fillModeNonSolid = supported.fillModeNonSolid;
    m_fillModeNonSolid = supported.fillModeNonSolid == VK_TRUE;

    VkDeviceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    ci.pQueueCreateInfos = queueInfos.data();
    ci.enabledExtensionCount = static_cast<uint32_t>(deviceExts.size());
    ci.ppEnabledExtensionNames = deviceExts.empty() ? nullptr : deviceExts.data();
    ci.pEnabledFeatures = &enabled;

    VkResult r = vkCreateDevice(m_physical, &ci, nullptr, &m_device);
    if (r != VK_SUCCESS) {
        MAZ_LOG_WARN("vkCreateDevice failed (VkResult %d)", (int)r);
        m_device = VK_NULL_HANDLE;
        return false;
    }

    vkGetDeviceQueue(m_device, m_graphicsFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentFamily, 0, &m_presentQueue);
    return createTransientPool();
}

bool VulkanContext::createTransientPool() {
    VkCommandPoolCreateInfo pool{};
    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pool.queueFamilyIndex = m_graphicsFamily;
    if (vkCreateCommandPool(m_device, &pool, nullptr, &m_transientPool) != VK_SUCCESS) {
        MAZ_LOG_ERROR("vkCreateCommandPool (transient) failed");
        return false;
    }
    return true;
}

VkCommandBuffer VulkanContext::beginSingleTimeCommands() {
    if (m_device == VK_NULL_HANDLE || m_transientPool == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }
    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = m_transientPool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(m_device, &alloc, &cmd);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);
    return cmd;
}

void VulkanContext::endSingleTimeCommands(VkCommandBuffer cmd) {
    if (cmd == VK_NULL_HANDLE) {
        return;
    }
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(m_graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphicsQueue);
    vkFreeCommandBuffers(m_device, m_transientPool, 1, &cmd);
}

void VulkanContext::shutdown() {
    if (m_transientPool) {
        vkDestroyCommandPool(m_device, m_transientPool, nullptr);
        m_transientPool = VK_NULL_HANDLE;
    }
    if (m_device) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    if (m_surface) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    if (m_debugMessenger) {
        auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroy) {
            destroy(m_instance, m_debugMessenger, nullptr);
        }
        m_debugMessenger = VK_NULL_HANDLE;
    }
    if (m_instance) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

} // namespace maz::render
