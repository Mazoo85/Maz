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

    chooseSampleCount(ctx);
    return createColorTarget(ctx) && createDepthResources(ctx) && createSceneColor(ctx) &&
           createRenderPass(ctx) && createCompositePass(ctx) && createFramebuffers(ctx);
}

// Single-sample scene color the MSAA pass resolves into; sampled by the composite pass.
bool VulkanSwapchain::createSceneColor(VulkanContext& ctx) {
    VkImageCreateInfo ii{};
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = m_sceneFormat;
    ii.extent = {m_extent.width, m_extent.height, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(ctx.device(), &ii, nullptr, &m_sceneImage) != VK_SUCCESS) {
        MAZ_LOG_ERROR("scene color vkCreateImage failed");
        return false;
    }
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(ctx.device(), m_sceneImage, &req);
    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex =
        findMemoryType(ctx, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (ai.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(ctx.device(), &ai, nullptr, &m_sceneMemory) != VK_SUCCESS) {
        MAZ_LOG_ERROR("scene color memory allocation failed");
        return false;
    }
    vkBindImageMemory(ctx.device(), m_sceneImage, m_sceneMemory, 0);

    VkImageViewCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = m_sceneImage;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = m_sceneFormat;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    if (vkCreateImageView(ctx.device(), &vi, nullptr, &m_sceneView) != VK_SUCCESS) {
        MAZ_LOG_ERROR("scene color vkCreateImageView failed");
        return false;
    }

    VkSamplerCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter = VK_FILTER_LINEAR;
    si.minFilter = VK_FILTER_LINEAR;
    si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.maxLod = 0.0f;
    if (vkCreateSampler(ctx.device(), &si, nullptr, &m_sceneSampler) != VK_SUCCESS) {
        MAZ_LOG_ERROR("scene color vkCreateSampler failed");
        return false;
    }
    return true;
}

// Composite pass: a single-sample color pass over the swapchain image, fed by a fullscreen draw
// that samples the scene color.
bool VulkanSwapchain::createCompositePass(VulkanContext& ctx) {
    VkAttachmentDescription color{};
    color.format = m_format;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // fully overwritten by the composite
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    // Wait for the scene pass to finish writing/resolving sceneColor before we sample it, and for
    // the swapchain image to be available before we write it.
    VkSubpassDependency deps[2]{};
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;
    deps[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[1].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[1].dstSubpass = 0;
    deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp.attachmentCount = 1;
    rp.pAttachments = &color;
    rp.subpassCount = 1;
    rp.pSubpasses = &subpass;
    rp.dependencyCount = 2;
    rp.pDependencies = deps;
    if (vkCreateRenderPass(ctx.device(), &rp, nullptr, &m_compositePass) != VK_SUCCESS) {
        MAZ_LOG_ERROR("composite vkCreateRenderPass failed");
        return false;
    }
    return true;
}

void VulkanSwapchain::chooseSampleCount(VulkanContext& ctx) {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(ctx.physicalDevice(), &props);
    const VkSampleCountFlags counts =
        props.limits.framebufferColorSampleCounts & props.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_4_BIT) {
        m_samples = VK_SAMPLE_COUNT_4_BIT;
    } else if (counts & VK_SAMPLE_COUNT_2_BIT) {
        m_samples = VK_SAMPLE_COUNT_2_BIT;
    } else {
        m_samples = VK_SAMPLE_COUNT_1_BIT;
    }
    MAZ_LOG_INFO("MSAA sample count: %dx", static_cast<int>(m_samples));
}

bool VulkanSwapchain::createColorTarget(VulkanContext& ctx) {
    if (m_samples == VK_SAMPLE_COUNT_1_BIT) {
        return true; // no separate MSAA target needed
    }
    VkImageCreateInfo ii{};
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = m_format;
    ii.extent = {m_extent.width, m_extent.height, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = m_samples;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.format = m_sceneFormat; // HDR scene target (see createSceneColor)
    ii.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(ctx.device(), &ii, nullptr, &m_colorImage) != VK_SUCCESS) {
        MAZ_LOG_ERROR("msaa color vkCreateImage failed");
        return false;
    }
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(ctx.device(), m_colorImage, &req);
    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex =
        findMemoryType(ctx, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (ai.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(ctx.device(), &ai, nullptr, &m_colorMemory) != VK_SUCCESS) {
        MAZ_LOG_ERROR("msaa color memory allocation failed");
        return false;
    }
    vkBindImageMemory(ctx.device(), m_colorImage, m_colorMemory, 0);

    VkImageViewCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = m_colorImage;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = m_sceneFormat;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    if (vkCreateImageView(ctx.device(), &vi, nullptr, &m_colorView) != VK_SUCCESS) {
        MAZ_LOG_ERROR("msaa color vkCreateImageView failed");
        return false;
    }
    return true;
}

bool VulkanSwapchain::createDepthResources(VulkanContext& ctx) {
    VkImageCreateInfo ii{};
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = m_depthFormat;
    ii.extent = {m_extent.width, m_extent.height, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = m_samples;
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
    const bool msaa = m_samples != VK_SAMPLE_COUNT_1_BIT;

    // Attachment 0: color the subpass draws into. Multisampled when MSAA is on (its result is
    // resolved into sceneColor and discarded), otherwise it IS sceneColor. Either way the scene
    // ends up in the single-sample sceneColor image, left in SHADER_READ_ONLY for the composite.
    VkAttachmentDescription color{};
    color.format = m_sceneFormat;
    color.samples = m_samples;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = msaa ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = msaa ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                             : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Attachment 1: depth, matching the color sample count.
    VkAttachmentDescription depth{};
    depth.format = m_depthFormat;
    depth.samples = m_samples;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Attachment 2 (MSAA only): the single-sample sceneColor the color attachment resolves into,
    // left ready to be sampled by the composite pass.
    VkAttachmentDescription resolve{};
    resolve.format = m_sceneFormat;
    resolve.samples = VK_SAMPLE_COUNT_1_BIT;
    resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    resolve.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference resolveRef{};
    resolveRef.attachment = 2;
    resolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;
    if (msaa) {
        subpass.pResolveAttachments = &resolveRef;
    }

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    const VkAttachmentDescription attachments[] = {color, depth, resolve};
    VkRenderPassCreateInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp.attachmentCount = msaa ? 3u : 2u;
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
    const bool msaa = m_samples != VK_SAMPLE_COUNT_1_BIT;

    // One scene framebuffer targeting sceneColor: [msaa-color, depth, resolve=sceneColor] or
    // [sceneColor, depth] single-sample.
    VkImageView msaaAttachments[] = {m_colorView, m_depthView, m_sceneView};
    VkImageView singleAttachments[] = {m_sceneView, m_depthView};
    VkFramebufferCreateInfo sfb{};
    sfb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    sfb.renderPass = m_renderPass;
    sfb.attachmentCount = msaa ? 3u : 2u;
    sfb.pAttachments = msaa ? msaaAttachments : singleAttachments;
    sfb.width = m_extent.width;
    sfb.height = m_extent.height;
    sfb.layers = 1;
    if (vkCreateFramebuffer(ctx.device(), &sfb, nullptr, &m_sceneFramebuffer) != VK_SUCCESS) {
        MAZ_LOG_ERROR("scene vkCreateFramebuffer failed");
        return false;
    }

    // One composite framebuffer per swapchain image.
    m_compositeFramebuffers.resize(m_views.size());
    for (size_t i = 0; i < m_views.size(); ++i) {
        VkFramebufferCreateInfo fb{};
        fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb.renderPass = m_compositePass;
        fb.attachmentCount = 1;
        fb.pAttachments = &m_views[i];
        fb.width = m_extent.width;
        fb.height = m_extent.height;
        fb.layers = 1;
        if (vkCreateFramebuffer(ctx.device(), &fb, nullptr, &m_compositeFramebuffers[i]) !=
            VK_SUCCESS) {
            MAZ_LOG_ERROR("composite vkCreateFramebuffer failed");
            return false;
        }
    }
    return true;
}

void VulkanSwapchain::destroy(VulkanContext& ctx) {
    VkDevice device = ctx.device();
    if (m_colorView) {
        vkDestroyImageView(device, m_colorView, nullptr);
        m_colorView = VK_NULL_HANDLE;
    }
    if (m_colorImage) {
        vkDestroyImage(device, m_colorImage, nullptr);
        m_colorImage = VK_NULL_HANDLE;
    }
    if (m_colorMemory) {
        vkFreeMemory(device, m_colorMemory, nullptr);
        m_colorMemory = VK_NULL_HANDLE;
    }
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
    if (m_sceneSampler) {
        vkDestroySampler(device, m_sceneSampler, nullptr);
        m_sceneSampler = VK_NULL_HANDLE;
    }
    if (m_sceneView) {
        vkDestroyImageView(device, m_sceneView, nullptr);
        m_sceneView = VK_NULL_HANDLE;
    }
    if (m_sceneImage) {
        vkDestroyImage(device, m_sceneImage, nullptr);
        m_sceneImage = VK_NULL_HANDLE;
    }
    if (m_sceneMemory) {
        vkFreeMemory(device, m_sceneMemory, nullptr);
        m_sceneMemory = VK_NULL_HANDLE;
    }
    if (m_sceneFramebuffer) {
        vkDestroyFramebuffer(device, m_sceneFramebuffer, nullptr);
        m_sceneFramebuffer = VK_NULL_HANDLE;
    }
    for (auto fb : m_compositeFramebuffers) {
        vkDestroyFramebuffer(device, fb, nullptr);
    }
    m_compositeFramebuffers.clear();
    if (m_compositePass) {
        vkDestroyRenderPass(device, m_compositePass, nullptr);
        m_compositePass = VK_NULL_HANDLE;
    }
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
