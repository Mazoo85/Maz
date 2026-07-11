#include "render/VulkanSwapchain.hpp"

#include "maz/core/Log.hpp"
#include "render/VulkanBuffer.hpp"
#include "render/VulkanContext.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace maz::render {

namespace {

VkSurfaceFormatKHR chooseFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return f;
        }
    }
    return formats.front();
}

VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes, bool vsync) {
    if (!vsync) {
        for (auto m : modes) {
            if (m == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                return m;
            }
        }
    } else {
        for (auto m : modes) {
            if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
                return m; // low-latency vsync
            }
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR; // always supported
}

} // namespace

bool VulkanSwapchain::create(VulkanContext& ctx, uint32_t width, uint32_t height, bool vsync) {
    VkPhysicalDevice phys = ctx.physicalDevice();
    VkSurfaceKHR surface = ctx.surface();

    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &caps);

    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &fmtCount, formats.data());

    uint32_t pmCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &pmCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(pmCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &pmCount, presentModes.data());

    if (formats.empty() || presentModes.empty()) {
        MAZ_LOG_ERROR("surface reports no formats/present modes");
        return false;
    }

    VkSurfaceFormatKHR surfaceFormat = chooseFormat(formats);
    VkPresentModeKHR presentMode = choosePresentMode(presentModes, vsync);
    m_format = surfaceFormat.format;

    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        m_extent = caps.currentExtent;
    } else {
        m_extent.width = std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width);
        m_extent.height =
            std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR ci{};
    ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface = surface;
    ci.minImageCount = imageCount;
    ci.imageFormat = surfaceFormat.format;
    ci.imageColorSpace = surfaceFormat.colorSpace;
    ci.imageExtent = m_extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = presentMode;
    ci.clipped = VK_TRUE;

    uint32_t families[] = {ctx.graphicsFamily(), ctx.presentFamily()};
    if (ctx.graphicsFamily() != ctx.presentFamily()) {
        ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices = families;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    if (vkCreateSwapchainKHR(ctx.device(), &ci, nullptr, &m_swapchain) != VK_SUCCESS) {
        MAZ_LOG_ERROR("vkCreateSwapchainKHR failed");
        return false;
    }

    uint32_t count = 0;
    vkGetSwapchainImagesKHR(ctx.device(), m_swapchain, &count, nullptr);
    m_images.resize(count);
    vkGetSwapchainImagesKHR(ctx.device(), m_swapchain, &count, m_images.data());

    m_views.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        VkImageViewCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image = m_images[i];
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = m_format;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        if (vkCreateImageView(ctx.device(), &vi, nullptr, &m_views[i]) != VK_SUCCESS) {
            MAZ_LOG_ERROR("vkCreateImageView failed");
            return false;
        }
    }

    return createDepthResources(ctx) && createRenderPass(ctx) && createFramebuffers(ctx);
}

bool VulkanSwapchain::createDepthResources(VulkanContext& ctx) {
    VkImageCreateInfo ii{};
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = m_depthFormat;
    ii.extent = {m_extent.width, m_extent.height, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(ctx.device(), &ii, nullptr, &m_depthImage) != VK_SUCCESS) {
        MAZ_LOG_ERROR("depth vkCreateImage failed");
        return false;
    }

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(ctx.device(), m_depthImage, &req);
    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex =
        findMemoryType(ctx, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (ai.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(ctx.device(), &ai, nullptr, &m_depthMemory) != VK_SUCCESS) {
        MAZ_LOG_ERROR("depth memory allocation failed");
        return false;
    }
    vkBindImageMemory(ctx.device(), m_depthImage, m_depthMemory, 0);

    VkImageViewCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = m_depthImage;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = m_depthFormat;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    if (vkCreateImageView(ctx.device(), &vi, nullptr, &m_depthView) != VK_SUCCESS) {
        MAZ_LOG_ERROR("depth vkCreateImageView failed");
        return false;
    }
    return true;
}

bool VulkanSwapchain::createRenderPass(VulkanContext& ctx) {
    VkAttachmentDescription color{};
    color.format = m_format;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depth{};
    depth.format = m_depthFormat;
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    const VkAttachmentDescription attachments[] = {color, depth};
    VkRenderPassCreateInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp.attachmentCount = 2;
    rp.pAttachments = attachments;
    rp.subpassCount = 1;
    rp.pSubpasses = &subpass;
    rp.dependencyCount = 1;
    rp.pDependencies = &dep;

    if (vkCreateRenderPass(ctx.device(), &rp, nullptr, &m_renderPass) != VK_SUCCESS) {
        MAZ_LOG_ERROR("vkCreateRenderPass failed");
        return false;
    }
    return true;
}

bool VulkanSwapchain::createFramebuffers(VulkanContext& ctx) {
    m_framebuffers.resize(m_views.size());
    for (size_t i = 0; i < m_views.size(); ++i) {
        VkImageView attachments[] = {m_views[i], m_depthView};
        VkFramebufferCreateInfo fb{};
        fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb.renderPass = m_renderPass;
        fb.attachmentCount = 2;
        fb.pAttachments = attachments;
        fb.width = m_extent.width;
        fb.height = m_extent.height;
        fb.layers = 1;
        if (vkCreateFramebuffer(ctx.device(), &fb, nullptr, &m_framebuffers[i]) != VK_SUCCESS) {
            MAZ_LOG_ERROR("vkCreateFramebuffer failed");
            return false;
        }
    }
    return true;
}

void VulkanSwapchain::destroy(VulkanContext& ctx) {
    VkDevice device = ctx.device();
    if (m_depthView) {
        vkDestroyImageView(device, m_depthView, nullptr);
        m_depthView = VK_NULL_HANDLE;
    }
    if (m_depthImage) {
        vkDestroyImage(device, m_depthImage, nullptr);
        m_depthImage = VK_NULL_HANDLE;
    }
    if (m_depthMemory) {
        vkFreeMemory(device, m_depthMemory, nullptr);
        m_depthMemory = VK_NULL_HANDLE;
    }
    for (auto fb : m_framebuffers) {
        vkDestroyFramebuffer(device, fb, nullptr);
    }
    m_framebuffers.clear();
    if (m_renderPass) {
        vkDestroyRenderPass(device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
    for (auto v : m_views) {
        vkDestroyImageView(device, v, nullptr);
    }
    m_views.clear();
    m_images.clear();
    if (m_swapchain) {
        vkDestroySwapchainKHR(device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

} // namespace maz::render
