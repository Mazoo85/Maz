#include "maz/render/Renderer.hpp"

#include "maz/core/Log.hpp"
#include "maz/platform/Window.hpp"
#include "render/VulkanContext.hpp"
#include "render/VulkanSwapchain.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace maz::render {

namespace {
constexpr uint32_t kMaxFramesInFlight = 2;
} // namespace

// Vulkan implementation of the Renderer interface. Records a render-pass clear each frame and
// presents. When no GPU/surface is available it stays inactive and beginFrame() returns false,
// so headless/CI runs still exercise the rest of the engine.
class VulkanRenderer final : public Renderer {
public:
    bool init(platform::Window& window, const RendererConfig& cfg) override;
    void shutdown() override;
    void onResize(uint32_t width, uint32_t height) override;
    bool beginFrame() override;
    void setClearColor(const Color& color) override { m_clearColor = color; }
    void endFrame() override;
    bool isActive() const override { return m_active; }

private:
    bool createCommands();
    bool createSync();
    void destroySync();
    void recreateSwapchain(uint32_t width, uint32_t height);

    VulkanContext m_ctx;
    VulkanSwapchain m_swapchain;
    RendererConfig m_cfg;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::array<VkCommandBuffer, kMaxFramesInFlight> m_commandBuffers{};
    std::array<VkSemaphore, kMaxFramesInFlight> m_imageAvailable{};
    std::array<VkSemaphore, kMaxFramesInFlight> m_renderFinished{};
    std::array<VkFence, kMaxFramesInFlight> m_inFlight{};
    std::vector<VkFence> m_imagesInFlight;

    Color m_clearColor{};
    uint32_t m_currentFrame = 0;
    uint32_t m_imageIndex = 0;
    bool m_active = false;
};

bool VulkanRenderer::init(platform::Window& window, const RendererConfig& cfg) {
    m_cfg = cfg;
    const bool wantSurface = window.supportsVulkan();

    if (!m_ctx.init(window, wantSurface, cfg.enableValidation)) {
        if (cfg.allowHeadless) {
            MAZ_LOG_WARN("renderer inactive: no Vulkan device — continuing headless");
            return true;
        }
        MAZ_LOG_ERROR("renderer init failed: no Vulkan device");
        return false;
    }

    if (!m_ctx.hasSurface()) {
        if (cfg.allowHeadless) {
            MAZ_LOG_WARN("renderer inactive: no presentable surface — continuing headless");
            return true;
        }
        MAZ_LOG_ERROR("renderer init failed: no presentable surface");
        return false;
    }

    uint32_t w = 0, h = 0;
    window.drawableSize(w, h);
    if (!m_swapchain.create(m_ctx, w, h, cfg.vsync)) {
        return false;
    }
    if (!createCommands() || !createSync()) {
        return false;
    }

    m_active = true;
    MAZ_LOG_INFO("renderer active (%ux%u, vsync %s)", m_swapchain.extent().width,
                 m_swapchain.extent().height, cfg.vsync ? "on" : "off");
    return true;
}

bool VulkanRenderer::createCommands() {
    VkCommandPoolCreateInfo pool{};
    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool.queueFamilyIndex = m_ctx.graphicsFamily();
    if (vkCreateCommandPool(m_ctx.device(), &pool, nullptr, &m_commandPool) != VK_SUCCESS) {
        MAZ_LOG_ERROR("vkCreateCommandPool failed");
        return false;
    }

    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = m_commandPool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = kMaxFramesInFlight;
    if (vkAllocateCommandBuffers(m_ctx.device(), &alloc, m_commandBuffers.data()) != VK_SUCCESS) {
        MAZ_LOG_ERROR("vkAllocateCommandBuffers failed");
        return false;
    }
    return true;
}

bool VulkanRenderer::createSync() {
    VkSemaphoreCreateInfo sem{};
    sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fence{};
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (vkCreateSemaphore(m_ctx.device(), &sem, nullptr, &m_imageAvailable[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_ctx.device(), &sem, nullptr, &m_renderFinished[i]) != VK_SUCCESS ||
            vkCreateFence(m_ctx.device(), &fence, nullptr, &m_inFlight[i]) != VK_SUCCESS) {
            MAZ_LOG_ERROR("failed to create per-frame sync objects");
            return false;
        }
    }
    m_imagesInFlight.assign(m_swapchain.imageCount(), VK_NULL_HANDLE);
    return true;
}

void VulkanRenderer::recreateSwapchain(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return; // minimized; try again on a later resize
    }
    vkDeviceWaitIdle(m_ctx.device());
    m_swapchain.destroy(m_ctx);
    if (!m_swapchain.create(m_ctx, width, height, m_cfg.vsync)) {
        MAZ_LOG_ERROR("swapchain recreation failed");
        m_active = false;
        return;
    }
    m_imagesInFlight.assign(m_swapchain.imageCount(), VK_NULL_HANDLE);
}

void VulkanRenderer::onResize(uint32_t width, uint32_t height) {
    if (m_active) {
        recreateSwapchain(width, height);
    }
}

bool VulkanRenderer::beginFrame() {
    if (!m_active) {
        return false;
    }

    vkWaitForFences(m_ctx.device(), 1, &m_inFlight[m_currentFrame], VK_TRUE, UINT64_MAX);

    VkResult acquire =
        vkAcquireNextImageKHR(m_ctx.device(), m_swapchain.handle(), UINT64_MAX,
                              m_imageAvailable[m_currentFrame], VK_NULL_HANDLE, &m_imageIndex);
    if (acquire == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain(m_swapchain.extent().width, m_swapchain.extent().height);
        return false;
    }
    if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) {
        MAZ_LOG_ERROR("vkAcquireNextImageKHR failed (VkResult %d)", (int)acquire);
        return false;
    }

    // Ensure the image we just acquired isn't still being read by a previous frame.
    if (m_imagesInFlight[m_imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(m_ctx.device(), 1, &m_imagesInFlight[m_imageIndex], VK_TRUE, UINT64_MAX);
    }
    m_imagesInFlight[m_imageIndex] = m_inFlight[m_currentFrame];
    vkResetFences(m_ctx.device(), 1, &m_inFlight[m_currentFrame]);

    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &begin);

    VkClearValue clear{};
    clear.color = {{m_clearColor.r, m_clearColor.g, m_clearColor.b, m_clearColor.a}};

    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = m_swapchain.renderPass();
    rp.framebuffer = m_swapchain.framebuffer(m_imageIndex);
    rp.renderArea.extent = m_swapchain.extent();
    rp.clearValueCount = 1;
    rp.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    // (Phase 3: bind pipelines and issue sprite/mesh draws here.)
    return true;
}

void VulkanRenderer::endFrame() {
    if (!m_active) {
        return;
    }
    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &m_imageAvailable[m_currentFrame];
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &m_renderFinished[m_currentFrame];
    vkQueueSubmit(m_ctx.graphicsQueue(), 1, &submit, m_inFlight[m_currentFrame]);

    VkSwapchainKHR swapchain = m_swapchain.handle();
    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &m_renderFinished[m_currentFrame];
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain;
    present.pImageIndices = &m_imageIndex;

    VkResult r = vkQueuePresentKHR(m_ctx.presentQueue(), &present);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain(m_swapchain.extent().width, m_swapchain.extent().height);
    } else if (r != VK_SUCCESS) {
        MAZ_LOG_ERROR("vkQueuePresentKHR failed (VkResult %d)", (int)r);
    }

    m_currentFrame = (m_currentFrame + 1) % kMaxFramesInFlight;
}

void VulkanRenderer::destroySync() {
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (m_imageAvailable[i]) {
            vkDestroySemaphore(m_ctx.device(), m_imageAvailable[i], nullptr);
        }
        if (m_renderFinished[i]) {
            vkDestroySemaphore(m_ctx.device(), m_renderFinished[i], nullptr);
        }
        if (m_inFlight[i]) {
            vkDestroyFence(m_ctx.device(), m_inFlight[i], nullptr);
        }
        m_imageAvailable[i] = VK_NULL_HANDLE;
        m_renderFinished[i] = VK_NULL_HANDLE;
        m_inFlight[i] = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::shutdown() {
    if (m_ctx.valid()) {
        vkDeviceWaitIdle(m_ctx.device());
    }
    if (m_active) {
        destroySync();
        if (m_commandPool) {
            vkDestroyCommandPool(m_ctx.device(), m_commandPool, nullptr);
            m_commandPool = VK_NULL_HANDLE;
        }
        m_swapchain.destroy(m_ctx);
        m_active = false;
    }
    m_ctx.shutdown();
}

std::unique_ptr<Renderer> createVulkanRenderer() { return std::make_unique<VulkanRenderer>(); }

} // namespace maz::render
