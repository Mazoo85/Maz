#include "render/VulkanTexture.hpp"

#include "maz/core/Log.hpp"
#include "render/VulkanBuffer.hpp"
#include "render/VulkanContext.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
// stb_image is third-party and doesn't compile clean under our strict warning set; silence
// warnings for its include only — our own code below keeps the full checks.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <stb_image.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <cstring>

namespace maz::render {

bool VulkanTexture::createFromFile(VulkanContext& ctx, const char* path) {
    int w = 0, h = 0, channels = 0;
    stbi_uc* pixels = stbi_load(path, &w, &h, &channels, STBI_rgb_alpha);
    if (!pixels) {
        MAZ_LOG_ERROR("stbi_load failed for '%s': %s", path, stbi_failure_reason());
        return false;
    }
    bool ok = create(ctx, static_cast<uint32_t>(w), static_cast<uint32_t>(h), pixels);
    stbi_image_free(pixels);
    return ok;
}


bool VulkanTexture::create(VulkanContext& ctx, uint32_t width, uint32_t height,
                           const void* rgba) {
    if (!ctx.valid() || width == 0 || height == 0 || rgba == nullptr) {
        return false;
    }
    m_width = width;
    m_height = height;
    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 4;

    // 1. Staging buffer (host-visible), fill with pixels.
    VulkanBuffer staging;
    if (!staging.create(ctx, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        return false;
    }
    if (void* dst = staging.map(ctx)) {
        std::memcpy(dst, rgba, static_cast<size_t>(imageSize));
    } else {
        staging.destroy(ctx);
        return false;
    }

    // 2. Device-local image.
    VkImageCreateInfo ii{};
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = VK_FORMAT_R8G8B8A8_SRGB;
    ii.extent = {width, height, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(ctx.device(), &ii, nullptr, &m_image) != VK_SUCCESS) {
        MAZ_LOG_ERROR("vkCreateImage failed");
        staging.destroy(ctx);
        return false;
    }

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(ctx.device(), m_image, &req);
    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = findMemoryType(ctx, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (ai.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(ctx.device(), &ai, nullptr, &m_memory) != VK_SUCCESS) {
        MAZ_LOG_ERROR("image memory allocation failed");
        staging.destroy(ctx);
        return false;
    }
    vkBindImageMemory(ctx.device(), m_image, m_memory, 0);

    // 3. Transition UNDEFINED -> TRANSFER_DST, copy, transition -> SHADER_READ_ONLY.
    VkCommandBuffer cmd = ctx.beginSingleTimeCommands();

    VkImageMemoryBarrier toTransfer{};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = m_image;
    toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.levelCount = 1;
    toTransfer.subresourceRange.layerCount = 1;
    toTransfer.srcAccessMask = 0;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &toTransfer);

    VkBufferImageCopy copy{};
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(cmd, staging.handle(), m_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    VkImageMemoryBarrier toShader = toTransfer;
    toShader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toShader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toShader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &toShader);

    ctx.endSingleTimeCommands(cmd);
    staging.destroy(ctx);

    // 4. View + sampler.
    VkImageViewCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = m_image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = VK_FORMAT_R8G8B8A8_SRGB;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    if (vkCreateImageView(ctx.device(), &vi, nullptr, &m_view) != VK_SUCCESS) {
        MAZ_LOG_ERROR("texture vkCreateImageView failed");
        return false;
    }

    VkSamplerCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter = VK_FILTER_NEAREST; // crisp pixels; suits 2D/pixel-art
    si.minFilter = VK_FILTER_NEAREST;
    si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    si.maxLod = 0.0f;
    if (vkCreateSampler(ctx.device(), &si, nullptr, &m_sampler) != VK_SUCCESS) {
        MAZ_LOG_ERROR("vkCreateSampler failed");
        return false;
    }
    return true;
}

void VulkanTexture::destroy(VulkanContext& ctx) {
    if (m_sampler) {
        vkDestroySampler(ctx.device(), m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    if (m_view) {
        vkDestroyImageView(ctx.device(), m_view, nullptr);
        m_view = VK_NULL_HANDLE;
    }
    if (m_image) {
        vkDestroyImage(ctx.device(), m_image, nullptr);
        m_image = VK_NULL_HANDLE;
    }
    if (m_memory) {
        vkFreeMemory(ctx.device(), m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
}

} // namespace maz::render
