#include "render/BloomChain.hpp"

#include "maz/core/Log.hpp"
#include "render/VulkanBuffer.hpp"
#include "render/VulkanContext.hpp"

#include <SDL3/SDL_filesystem.h>

#include <fstream>
#include <string>
#include <vector>

namespace maz::render {

namespace {

std::vector<char> readFile(const std::string& path) {
    std::ifstream f(path, std::ios::ate | std::ios::binary);
    if (!f) {
        MAZ_LOG_ERROR("could not open shader '%s'", path.c_str());
        return {};
    }
    const auto size = static_cast<size_t>(f.tellg());
    std::vector<char> buffer(size);
    f.seekg(0);
    f.read(buffer.data(), static_cast<std::streamsize>(size));
    return buffer;
}

VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
    if (code.empty()) {
        return VK_NULL_HANDLE;
    }
    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &ci, nullptr, &module) != VK_SUCCESS) {
        MAZ_LOG_ERROR("bloom vkCreateShaderModule failed");
    }
    return module;
}

std::string assetBase() {
    const char* base = SDL_GetBasePath();
    return base ? std::string(base) : std::string();
}

} // namespace

bool BloomChain::init(VulkanContext& ctx, VkFormat hdrFormat, VkExtent2D sceneExtent,
                      VkImageView sceneView, VkSampler sceneSampler) {
    m_format = hdrFormat;
    if (!createRenderPass(ctx) || !createSampler(ctx) || !createDescriptors(ctx) ||
        !createTargets(ctx, sceneExtent) || !createPipelines(ctx)) {
        return false;
    }
    writeSet(ctx, m_setScene, sceneView);
    (void)sceneSampler; // the chain's own linear-clamp sampler is used for all inputs
    writeSet(ctx, m_setA, m_a.view);
    writeSet(ctx, m_setB, m_b.view);
    return true;
}

bool BloomChain::resize(VulkanContext& ctx, VkExtent2D sceneExtent, VkImageView sceneView,
                        VkSampler sceneSampler) {
    destroyTargets(ctx);
    if (!createTargets(ctx, sceneExtent)) {
        return false;
    }
    writeSet(ctx, m_setScene, sceneView);
    (void)sceneSampler;
    writeSet(ctx, m_setA, m_a.view);
    writeSet(ctx, m_setB, m_b.view);
    return true;
}

bool BloomChain::createRenderPass(VulkanContext& ctx) {
    VkAttachmentDescription color{};
    color.format = m_format;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // every pixel is written
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference ref{};
    ref.attachment = 0;
    ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &ref;

    // Wait for the prior pass's writes to the input image to be readable, and make our writes
    // available to the next pass that samples this target.
    VkSubpassDependency deps[2]{};
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;
    deps[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp.attachmentCount = 1;
    rp.pAttachments = &color;
    rp.subpassCount = 1;
    rp.pSubpasses = &subpass;
    rp.dependencyCount = 2;
    rp.pDependencies = deps;
    if (vkCreateRenderPass(ctx.device(), &rp, nullptr, &m_pass) != VK_SUCCESS) {
        MAZ_LOG_ERROR("bloom vkCreateRenderPass failed");
        return false;
    }
    return true;
}

bool BloomChain::createSampler(VulkanContext& ctx) {
    VkSamplerCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter = VK_FILTER_LINEAR;
    si.minFilter = VK_FILTER_LINEAR;
    si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    if (vkCreateSampler(ctx.device(), &si, nullptr, &m_sampler) != VK_SUCCESS) {
        MAZ_LOG_ERROR("bloom sampler failed");
        return false;
    }
    return true;
}

bool BloomChain::createTargets(VulkanContext& ctx, VkExtent2D sceneExtent) {
    m_halfExtent = {sceneExtent.width / 2 > 0 ? sceneExtent.width / 2 : 1,
                    sceneExtent.height / 2 > 0 ? sceneExtent.height / 2 : 1};
    Target* targets[2] = {&m_a, &m_b};
    for (Target* tp : targets) {
        Target& t = *tp;
        VkImageCreateInfo ii{};
        ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ii.imageType = VK_IMAGE_TYPE_2D;
        ii.format = m_format;
        ii.extent = {m_halfExtent.width, m_halfExtent.height, 1};
        ii.mipLevels = 1;
        ii.arrayLayers = 1;
        ii.samples = VK_SAMPLE_COUNT_1_BIT;
        ii.tiling = VK_IMAGE_TILING_OPTIMAL;
        ii.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vkCreateImage(ctx.device(), &ii, nullptr, &t.image) != VK_SUCCESS) {
            MAZ_LOG_ERROR("bloom target image failed");
            return false;
        }
        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(ctx.device(), t.image, &req);
        VkMemoryAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex =
            findMemoryType(ctx, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (ai.memoryTypeIndex == UINT32_MAX ||
            vkAllocateMemory(ctx.device(), &ai, nullptr, &t.memory) != VK_SUCCESS) {
            MAZ_LOG_ERROR("bloom target memory failed");
            return false;
        }
        vkBindImageMemory(ctx.device(), t.image, t.memory, 0);

        VkImageViewCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image = t.image;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = m_format;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        if (vkCreateImageView(ctx.device(), &vi, nullptr, &t.view) != VK_SUCCESS) {
            MAZ_LOG_ERROR("bloom target view failed");
            return false;
        }
        VkFramebufferCreateInfo fi{};
        fi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fi.renderPass = m_pass;
        fi.attachmentCount = 1;
        fi.pAttachments = &t.view;
        fi.width = m_halfExtent.width;
        fi.height = m_halfExtent.height;
        fi.layers = 1;
        if (vkCreateFramebuffer(ctx.device(), &fi, nullptr, &t.fbo) != VK_SUCCESS) {
            MAZ_LOG_ERROR("bloom target framebuffer failed");
            return false;
        }
    }
    m_viewA = m_a.view;
    m_viewB = m_b.view;
    return true;
}

bool BloomChain::createDescriptors(VulkanContext& ctx) {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo li{};
    li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    li.bindingCount = 1;
    li.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(ctx.device(), &li, nullptr, &m_setLayout) != VK_SUCCESS) {
        MAZ_LOG_ERROR("bloom set layout failed");
        return false;
    }
    VkDescriptorPoolSize ps{};
    ps.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ps.descriptorCount = 3;
    VkDescriptorPoolCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pi.maxSets = 3;
    pi.poolSizeCount = 1;
    pi.pPoolSizes = &ps;
    if (vkCreateDescriptorPool(ctx.device(), &pi, nullptr, &m_pool) != VK_SUCCESS) {
        MAZ_LOG_ERROR("bloom pool failed");
        return false;
    }
    VkDescriptorSet* sets[3] = {&m_setScene, &m_setA, &m_setB};
    for (VkDescriptorSet* s : sets) {
        VkDescriptorSetAllocateInfo dai{};
        dai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dai.descriptorPool = m_pool;
        dai.descriptorSetCount = 1;
        dai.pSetLayouts = &m_setLayout;
        if (vkAllocateDescriptorSets(ctx.device(), &dai, s) != VK_SUCCESS) {
            MAZ_LOG_ERROR("bloom set alloc failed");
            return false;
        }
    }
    return true;
}

void BloomChain::writeSet(VulkanContext& ctx, VkDescriptorSet set, VkImageView view) {
    VkDescriptorImageInfo img{};
    img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    img.imageView = view;
    img.sampler = m_sampler;
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &img;
    vkUpdateDescriptorSets(ctx.device(), 1, &write, 0, nullptr);
}

bool BloomChain::createPipelines(VulkanContext& ctx) {
    const std::string base = assetBase();
    VkShaderModule vert =
        createShaderModule(ctx.device(), readFile(base + "shaders/composite.vert.spv"));
    VkShaderModule down =
        createShaderModule(ctx.device(), readFile(base + "shaders/bloom_down.frag.spv"));
    VkShaderModule blur =
        createShaderModule(ctx.device(), readFile(base + "shaders/bloom_blur.frag.spv"));
    if (!vert || !down || !blur) {
        return false;
    }

    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    push.offset = 0;
    push.size = sizeof(float) * 4;
    VkPipelineLayoutCreateInfo pl{};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &m_setLayout;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &push;
    if (vkCreatePipelineLayout(ctx.device(), &pl, nullptr, &m_layout) != VK_SUCCESS) {
        MAZ_LOG_ERROR("bloom pipeline layout failed");
        return false;
    }

    auto makePipeline = [&](VkShaderModule frag, VkPipeline& out) -> bool {
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vert;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = frag;
        stages[1].pName = "main";

        VkPipelineVertexInputStateCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VkPipelineInputAssemblyStateCreateInfo ia{};
        ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo vp{};
        vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vp.viewportCount = 1;
        vp.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo rs{};
        rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.cullMode = VK_CULL_MODE_NONE;
        rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rs.lineWidth = 1.0f;
        VkPipelineMultisampleStateCreateInfo ms{};
        ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState blendAtt{};
        blendAtt.blendEnable = VK_FALSE;
        blendAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo cb{};
        cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.attachmentCount = 1;
        cb.pAttachments = &blendAtt;
        VkDynamicState dynamics[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dyn{};
        dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dyn.dynamicStateCount = 2;
        dyn.pDynamicStates = dynamics;

        VkGraphicsPipelineCreateInfo gp{};
        gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gp.stageCount = 2;
        gp.pStages = stages;
        gp.pVertexInputState = &vi;
        gp.pInputAssemblyState = &ia;
        gp.pViewportState = &vp;
        gp.pRasterizationState = &rs;
        gp.pMultisampleState = &ms;
        gp.pColorBlendState = &cb;
        gp.pDynamicState = &dyn;
        gp.layout = m_layout;
        gp.renderPass = m_pass;
        return vkCreateGraphicsPipelines(ctx.device(), VK_NULL_HANDLE, 1, &gp, nullptr, &out) ==
               VK_SUCCESS;
    };

    const bool ok = makePipeline(down, m_down) && makePipeline(blur, m_blur);
    vkDestroyShaderModule(ctx.device(), vert, nullptr);
    vkDestroyShaderModule(ctx.device(), down, nullptr);
    vkDestroyShaderModule(ctx.device(), blur, nullptr);
    if (!ok) {
        MAZ_LOG_ERROR("bloom vkCreateGraphicsPipelines failed");
        return false;
    }
    return true;
}

void BloomChain::doPass(VkCommandBuffer cmd, VkFramebuffer fbo, VkPipeline pipeline,
                        VkDescriptorSet src, const float push[4]) {
    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = m_pass;
    rp.framebuffer = fbo;
    rp.renderArea.extent = m_halfExtent;
    rp.clearValueCount = 0;
    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.width = static_cast<float>(m_halfExtent.width);
    viewport.height = static_cast<float>(m_halfExtent.height);
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{};
    scissor.extent = m_halfExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 0, 1, &src, 0, nullptr);
    vkCmdPushConstants(cmd, m_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float) * 4, push);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
}

void BloomChain::record(VkCommandBuffer cmd) {
    if (m_down == VK_NULL_HANDLE) {
        return;
    }
    const float invW = 1.0f / static_cast<float>(m_halfExtent.width);
    const float invH = 1.0f / static_cast<float>(m_halfExtent.height);
    // 1) bright-pass + downsample: scene -> B
    const float pDown[4] = {m_threshold, 0.0f, 0.0f, 0.0f};
    doPass(cmd, m_b.fbo, m_down, m_setScene, pDown);
    // 2) horizontal blur: B -> A
    const float pH[4] = {invW, 0.0f, 0.0f, 0.0f};
    doPass(cmd, m_a.fbo, m_blur, m_setB, pH);
    // 3) vertical blur: A -> B (final blurred bloom in B = bloomView())
    const float pV[4] = {0.0f, invH, 0.0f, 0.0f};
    doPass(cmd, m_b.fbo, m_blur, m_setA, pV);
}

void BloomChain::primeSkip(VkCommandBuffer cmd) {
    if (m_pass == VK_NULL_HANDLE || m_b.fbo == VK_NULL_HANDLE) {
        return;
    }
    // Begin + immediately end the bloom render pass on the output target (m_b), with no draws. The pass's
    // finalLayout (SHADER_READ_ONLY_OPTIMAL) transition still happens, so bloomView() is validly laid out
    // for the composite's sampler — but none of the bright-pass/blur fragment work runs. The mobile
    // composite multiplies bloom by 0, so the untouched (DONT_CARE) contents are never used.
    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = m_pass;
    rp.framebuffer = m_b.fbo;
    rp.renderArea.extent = m_halfExtent;
    rp.clearValueCount = 0;
    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(cmd);
}

void BloomChain::destroyTargets(VulkanContext& ctx) {
    VkDevice d = ctx.device();
    Target* targets[2] = {&m_a, &m_b};
    for (Target* tp : targets) {
        Target& t = *tp;
        if (t.fbo) vkDestroyFramebuffer(d, t.fbo, nullptr);
        if (t.view) vkDestroyImageView(d, t.view, nullptr);
        if (t.image) vkDestroyImage(d, t.image, nullptr);
        if (t.memory) vkFreeMemory(d, t.memory, nullptr);
        t = Target{};
    }
}

void BloomChain::shutdown(VulkanContext& ctx) {
    VkDevice d = ctx.device();
    destroyTargets(ctx);
    if (m_down) vkDestroyPipeline(d, m_down, nullptr);
    if (m_blur) vkDestroyPipeline(d, m_blur, nullptr);
    if (m_layout) vkDestroyPipelineLayout(d, m_layout, nullptr);
    if (m_pool) vkDestroyDescriptorPool(d, m_pool, nullptr);
    if (m_setLayout) vkDestroyDescriptorSetLayout(d, m_setLayout, nullptr);
    if (m_sampler) vkDestroySampler(d, m_sampler, nullptr);
    if (m_pass) vkDestroyRenderPass(d, m_pass, nullptr);
    m_down = m_blur = VK_NULL_HANDLE;
    m_layout = VK_NULL_HANDLE;
    m_pool = VK_NULL_HANDLE;
    m_setLayout = VK_NULL_HANDLE;
    m_sampler = VK_NULL_HANDLE;
    m_pass = VK_NULL_HANDLE;
}

} // namespace maz::render
