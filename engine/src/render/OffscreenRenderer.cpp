#include "maz/render/OffscreenRenderer.hpp"

#include "maz/core/Log.hpp"
#include "render/MeshRenderer.hpp"
#include "render/SpriteRenderer.hpp"
#include "render/VulkanContext.hpp"

#include <array>
#include <cstring>
#include <memory>

namespace maz::render {

namespace {
constexpr VkFormat kColorFormat = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat kDepthFormat = VK_FORMAT_D32_SFLOAT;
} // namespace

struct OffscreenRenderer::Impl {
    VulkanContext ctx;
    MeshRenderer mesh;
    SpriteRenderer sprite;
    uint32_t width = 0;
    uint32_t height = 0;

    VkImage colorImage = VK_NULL_HANDLE;
    VkDeviceMemory colorMemory = VK_NULL_HANDLE;
    VkImageView colorView = VK_NULL_HANDLE;
    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory = VK_NULL_HANDLE;
    VkImageView depthView = VK_NULL_HANDLE;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;

    VkBuffer readback = VK_NULL_HANDLE;
    VkDeviceMemory readbackMemory = VK_NULL_HANDLE;
    VkDeviceSize readbackSize = 0;

    bool ready = false;

    bool createImage(VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect,
                     VkImage& image, VkDeviceMemory& memory, VkImageView& view) {
        VkImageCreateInfo img{};
        img.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        img.imageType = VK_IMAGE_TYPE_2D;
        img.format = format;
        img.extent = {width, height, 1};
        img.mipLevels = 1;
        img.arrayLayers = 1;
        img.samples = VK_SAMPLE_COUNT_1_BIT;
        img.tiling = VK_IMAGE_TILING_OPTIMAL;
        img.usage = usage;
        img.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vkCreateImage(ctx.device(), &img, nullptr, &image) != VK_SUCCESS) {
            return false;
        }

        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(ctx.device(), image, &req);
        const uint32_t type = ctx.findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (type == UINT32_MAX) {
            return false;
        }
        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = type;
        if (vkAllocateMemory(ctx.device(), &alloc, nullptr, &memory) != VK_SUCCESS) {
            return false;
        }
        vkBindImageMemory(ctx.device(), image, memory, 0);

        VkImageViewCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image = image;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = format;
        vi.subresourceRange.aspectMask = aspect;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        return vkCreateImageView(ctx.device(), &vi, nullptr, &view) == VK_SUCCESS;
    }

    bool createRenderPass() {
        VkAttachmentDescription color{};
        color.format = kColorFormat;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; // ready to copy out

        VkAttachmentDescription depth{};
        depth.format = kDepthFormat;
        depth.samples = VK_SAMPLE_COUNT_1_BIT;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        VkSubpassDependency deps[2]{};
        deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        deps[0].dstSubpass = 0;
        deps[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        deps[1].srcSubpass = 0;
        deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[1].dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        deps[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        VkAttachmentDescription attachments[] = {color, depth};
        VkRenderPassCreateInfo rp{};
        rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rp.attachmentCount = 2;
        rp.pAttachments = attachments;
        rp.subpassCount = 1;
        rp.pSubpasses = &subpass;
        rp.dependencyCount = 2;
        rp.pDependencies = deps;
        return vkCreateRenderPass(ctx.device(), &rp, nullptr, &renderPass) == VK_SUCCESS;
    }

    bool createReadbackBuffer() {
        readbackSize = static_cast<VkDeviceSize>(width) * height * 4;
        VkBufferCreateInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size = readbackSize;
        bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(ctx.device(), &bi, nullptr, &readback) != VK_SUCCESS) {
            return false;
        }
        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(ctx.device(), readback, &req);
        const uint32_t type = ctx.findMemoryType(
            req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (type == UINT32_MAX) {
            return false;
        }
        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = type;
        if (vkAllocateMemory(ctx.device(), &alloc, nullptr, &readbackMemory) != VK_SUCCESS) {
            return false;
        }
        vkBindBufferMemory(ctx.device(), readback, readbackMemory, 0);
        return true;
    }
};

OffscreenRenderer::OffscreenRenderer() : m_impl(std::make_unique<Impl>()) {}
OffscreenRenderer::~OffscreenRenderer() { shutdown(); }
bool OffscreenRenderer::valid() const { return m_impl && m_impl->ready; }

bool OffscreenRenderer::init(platform::Window& window, uint32_t width, uint32_t height) {
    Impl& d = *m_impl;
    d.width = width;
    d.height = height;

    if (!d.ctx.init(window, /*wantSurface=*/false, /*enableValidation=*/false)) {
        MAZ_LOG_WARN("offscreen: no Vulkan device available");
        return false;
    }
    if (!d.createImage(kColorFormat,
                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                       VK_IMAGE_ASPECT_COLOR_BIT, d.colorImage, d.colorMemory, d.colorView)) {
        MAZ_LOG_ERROR("offscreen: color image creation failed");
        return false;
    }
    if (!d.createImage(kDepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                       VK_IMAGE_ASPECT_DEPTH_BIT, d.depthImage, d.depthMemory, d.depthView)) {
        MAZ_LOG_ERROR("offscreen: depth image creation failed");
        return false;
    }
    if (!d.createRenderPass()) {
        MAZ_LOG_ERROR("offscreen: render pass creation failed");
        return false;
    }

    VkImageView attachments[] = {d.colorView, d.depthView};
    VkFramebufferCreateInfo fb{};
    fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb.renderPass = d.renderPass;
    fb.attachmentCount = 2;
    fb.pAttachments = attachments;
    fb.width = width;
    fb.height = height;
    fb.layers = 1;
    if (vkCreateFramebuffer(d.ctx.device(), &fb, nullptr, &d.framebuffer) != VK_SUCCESS) {
        MAZ_LOG_ERROR("offscreen: framebuffer creation failed");
        return false;
    }

    VkCommandPoolCreateInfo pool{};
    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool.queueFamilyIndex = d.ctx.graphicsFamily();
    if (vkCreateCommandPool(d.ctx.device(), &pool, nullptr, &d.commandPool) != VK_SUCCESS) {
        return false;
    }
    VkCommandBufferAllocateInfo cba{};
    cba.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cba.commandPool = d.commandPool;
    cba.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cba.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(d.ctx.device(), &cba, &d.cmd) != VK_SUCCESS) {
        return false;
    }
    VkFenceCreateInfo fc{};
    fc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (vkCreateFence(d.ctx.device(), &fc, nullptr, &d.fence) != VK_SUCCESS) {
        return false;
    }
    if (!d.createReadbackBuffer()) {
        MAZ_LOG_ERROR("offscreen: readback buffer creation failed");
        return false;
    }
    if (!d.mesh.init(d.ctx, d.renderPass)) {
        MAZ_LOG_ERROR("offscreen: mesh pipeline init failed");
        return false;
    }
    // Sprite pipeline is optional here: the mesh render probe doesn't need it, but the sprite probe
    // does. A single frame buffer set is enough for one-shot offscreen rendering.
    if (!d.sprite.init(d.ctx, d.renderPass, 1)) {
        MAZ_LOG_WARN("offscreen: sprite pipeline init failed; 2D probes disabled");
    }

    d.ready = true;
    MAZ_LOG_INFO("offscreen renderer ready (%ux%u)", width, height);
    return true;
}

int OffscreenRenderer::uploadModel(const assets::Model& model) {
    if (!valid()) {
        return -1;
    }
    return m_impl->mesh.uploadModel(m_impl->ctx, model);
}

int OffscreenRenderer::uploadTexture(const uint8_t* rgba, uint32_t width, uint32_t height) {
    if (!valid()) {
        return -1;
    }
    return m_impl->sprite.createTexture(m_impl->ctx, rgba, width, height);
}

bool OffscreenRenderer::renderToPixels(const Color& clear, const std::vector<Item>& items,
                                       std::vector<uint8_t>& outRGBA) {
    if (!valid()) {
        return false;
    }
    Impl& d = *m_impl;
    const VkExtent2D extent{d.width, d.height};

    vkResetFences(d.ctx.device(), 1, &d.fence);
    vkResetCommandBuffer(d.cmd, 0);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(d.cmd, &begin);

    std::array<VkClearValue, 2> clears{};
    clears[0].color = {{clear.r, clear.g, clear.b, clear.a}};
    clears[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = d.renderPass;
    rp.framebuffer = d.framebuffer;
    rp.renderArea.extent = extent;
    rp.clearValueCount = static_cast<uint32_t>(clears.size());
    rp.pClearValues = clears.data();
    vkCmdBeginRenderPass(d.cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    for (const Item& item : items) {
        d.mesh.draw(d.cmd, item.handle, item.mvp, item.model, extent);
    }

    vkCmdEndRenderPass(d.cmd);

    // Color image is now in TRANSFER_SRC layout (render pass finalLayout); copy it to the buffer.
    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {d.width, d.height, 1};
    vkCmdCopyImageToBuffer(d.cmd, d.colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, d.readback, 1,
                           &region);

    vkEndCommandBuffer(d.cmd);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &d.cmd;
    if (vkQueueSubmit(d.ctx.graphicsQueue(), 1, &submit, d.fence) != VK_SUCCESS) {
        MAZ_LOG_ERROR("offscreen: vkQueueSubmit failed");
        return false;
    }
    vkWaitForFences(d.ctx.device(), 1, &d.fence, VK_TRUE, UINT64_MAX);

    outRGBA.resize(static_cast<size_t>(d.readbackSize));
    void* mapped = nullptr;
    if (vkMapMemory(d.ctx.device(), d.readbackMemory, 0, d.readbackSize, 0, &mapped) != VK_SUCCESS) {
        return false;
    }
    std::memcpy(outRGBA.data(), mapped, static_cast<size_t>(d.readbackSize));
    vkUnmapMemory(d.ctx.device(), d.readbackMemory);
    return true;
}

bool OffscreenRenderer::renderSpritesToPixels(const Color& clear,
                                              const std::vector<SpriteItem>& sprites,
                                              std::vector<uint8_t>& outRGBA) {
    if (!valid() || !m_impl->sprite.ready()) {
        return false;
    }
    Impl& d = *m_impl;
    const VkExtent2D extent{d.width, d.height};

    vkResetFences(d.ctx.device(), 1, &d.fence);
    vkResetCommandBuffer(d.cmd, 0);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(d.cmd, &begin);

    std::array<VkClearValue, 2> clears{};
    clears[0].color = {{clear.r, clear.g, clear.b, clear.a}};
    clears[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = d.renderPass;
    rp.framebuffer = d.framebuffer;
    rp.renderArea.extent = extent;
    rp.clearValueCount = static_cast<uint32_t>(clears.size());
    rp.pClearValues = clears.data();
    vkCmdBeginRenderPass(d.cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    d.sprite.begin(0, extent);
    for (const SpriteItem& item : sprites) {
        d.sprite.draw(item.texture, item.sprite);
    }
    d.sprite.flush(d.ctx, d.cmd);

    vkCmdEndRenderPass(d.cmd);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {d.width, d.height, 1};
    vkCmdCopyImageToBuffer(d.cmd, d.colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, d.readback, 1,
                           &region);

    vkEndCommandBuffer(d.cmd);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &d.cmd;
    if (vkQueueSubmit(d.ctx.graphicsQueue(), 1, &submit, d.fence) != VK_SUCCESS) {
        MAZ_LOG_ERROR("offscreen: vkQueueSubmit (sprites) failed");
        return false;
    }
    vkWaitForFences(d.ctx.device(), 1, &d.fence, VK_TRUE, UINT64_MAX);

    outRGBA.resize(static_cast<size_t>(d.readbackSize));
    void* mapped = nullptr;
    if (vkMapMemory(d.ctx.device(), d.readbackMemory, 0, d.readbackSize, 0, &mapped) != VK_SUCCESS) {
        return false;
    }
    std::memcpy(outRGBA.data(), mapped, static_cast<size_t>(d.readbackSize));
    vkUnmapMemory(d.ctx.device(), d.readbackMemory);
    return true;
}

void OffscreenRenderer::shutdown() {
    if (!m_impl) {
        return;
    }
    Impl& d = *m_impl;
    if (d.ctx.valid()) {
        vkDeviceWaitIdle(d.ctx.device());
    }
    VkDevice dev = d.ctx.device();
    if (d.ready || d.ctx.valid()) {
        d.sprite.destroy(d.ctx);
        d.mesh.destroy(d.ctx);
        if (d.readback) vkDestroyBuffer(dev, d.readback, nullptr);
        if (d.readbackMemory) vkFreeMemory(dev, d.readbackMemory, nullptr);
        if (d.fence) vkDestroyFence(dev, d.fence, nullptr);
        if (d.commandPool) vkDestroyCommandPool(dev, d.commandPool, nullptr);
        if (d.framebuffer) vkDestroyFramebuffer(dev, d.framebuffer, nullptr);
        if (d.renderPass) vkDestroyRenderPass(dev, d.renderPass, nullptr);
        if (d.colorView) vkDestroyImageView(dev, d.colorView, nullptr);
        if (d.colorImage) vkDestroyImage(dev, d.colorImage, nullptr);
        if (d.colorMemory) vkFreeMemory(dev, d.colorMemory, nullptr);
        if (d.depthView) vkDestroyImageView(dev, d.depthView, nullptr);
        if (d.depthImage) vkDestroyImage(dev, d.depthImage, nullptr);
        if (d.depthMemory) vkFreeMemory(dev, d.depthMemory, nullptr);
    }
    d.readback = VK_NULL_HANDLE;
    d.readbackMemory = VK_NULL_HANDLE;
    d.fence = VK_NULL_HANDLE;
    d.commandPool = VK_NULL_HANDLE;
    d.framebuffer = VK_NULL_HANDLE;
    d.renderPass = VK_NULL_HANDLE;
    d.colorView = d.depthView = VK_NULL_HANDLE;
    d.colorImage = d.depthImage = VK_NULL_HANDLE;
    d.colorMemory = d.depthMemory = VK_NULL_HANDLE;
    d.ctx.shutdown();
    d.ready = false;
}

} // namespace maz::render
