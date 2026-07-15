#include "render/SsaoPass.hpp"

#include "maz/core/Log.hpp"
#include "render/VulkanContext.hpp"

#include <SDL3/SDL_filesystem.h>

#include <cstring>
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
        MAZ_LOG_ERROR("ssao vkCreateShaderModule failed");
    }
    return module;
}

std::string assetBase() {
    const char* base = SDL_GetBasePath();
    return base ? std::string(base) : std::string();
}

} // namespace

bool SsaoPass::init(VulkanContext& ctx, VkExtent2D extent, VkImageView depthView,
                    VkSampler depthSampler) {
    (void)depthSampler; // the pass uses its own sampler for all inputs
    if (!m_camUbo.create(ctx, sizeof(CamUbo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        MAZ_LOG_ERROR("ssao UBO create failed");
        return false;
    }
    m_camMapped = m_camUbo.map(ctx);
    if (!m_camMapped) {
        return false;
    }
    if (!createRenderPass(ctx) || !createSampler(ctx) || !createDescriptors(ctx) ||
        !createTargets(ctx, extent) || !createPipelines(ctx)) {
        return false;
    }
    writeSsaoSet(ctx, depthView);
    writeBlurSet(ctx);
    return true;
}

bool SsaoPass::resize(VulkanContext& ctx, VkExtent2D extent, VkImageView depthView,
                      VkSampler depthSampler) {
    (void)depthSampler;
    destroyTargets(ctx);
    if (!createTargets(ctx, extent)) {
        return false;
    }
    writeSsaoSet(ctx, depthView);
    writeBlurSet(ctx);
    return true;
}

bool SsaoPass::createRenderPass(VulkanContext& ctx) {
    VkAttachmentDescription color{};
    color.format = m_format;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
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
    VkSubpassDependency deps[2]{};
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;
    deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
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
        MAZ_LOG_ERROR("ssao vkCreateRenderPass failed");
        return false;
    }
    return true;
}

bool SsaoPass::createSampler(VulkanContext& ctx) {
    VkSamplerCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter = VK_FILTER_LINEAR;
    si.minFilter = VK_FILTER_LINEAR;
    si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    if (vkCreateSampler(ctx.device(), &si, nullptr, &m_sampler) != VK_SUCCESS) {
        MAZ_LOG_ERROR("ssao sampler failed");
        return false;
    }
    return true;
}

bool SsaoPass::createTargets(VulkanContext& ctx, VkExtent2D extent) {
    m_extent = {extent.width > 0 ? extent.width : 1, extent.height > 0 ? extent.height : 1};
    Target* targets[2] = {&m_ssao, &m_blur};
    for (Target* tp : targets) {
        Target& t = *tp;
        VkImageCreateInfo ii{};
        ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ii.imageType = VK_IMAGE_TYPE_2D;
        ii.format = m_format;
        ii.extent = {m_extent.width, m_extent.height, 1};
        ii.mipLevels = 1;
        ii.arrayLayers = 1;
        ii.samples = VK_SAMPLE_COUNT_1_BIT;
        ii.tiling = VK_IMAGE_TILING_OPTIMAL;
        ii.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vkCreateImage(ctx.device(), &ii, nullptr, &t.image) != VK_SUCCESS) {
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
            return false;
        }
        VkFramebufferCreateInfo fi{};
        fi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fi.renderPass = m_pass;
        fi.attachmentCount = 1;
        fi.pAttachments = &t.view;
        fi.width = m_extent.width;
        fi.height = m_extent.height;
        fi.layers = 1;
        if (vkCreateFramebuffer(ctx.device(), &fi, nullptr, &t.fbo) != VK_SUCCESS) {
            return false;
        }
    }
    // Clear both targets to white (occlusion 1.0 = fully lit) and leave them in SHADER_READ, so the
    // composite can sample the AO even when SSAO never runs this frame (the disabled path).
    VkCommandBuffer c = ctx.beginSingleTimeCommands();
    for (Target* tp : targets) {
        VkImageMemoryBarrier b{};
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.image = tp->image;
        b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.srcAccessMask = 0;
        b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr, 0, nullptr, 1, &b);
        VkClearColorValue white{};
        white.float32[0] = white.float32[1] = white.float32[2] = white.float32[3] = 1.0f;
        VkImageSubresourceRange r{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(c, tp->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &white, 1, &r);
        b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &b);
    }
    ctx.endSingleTimeCommands(c);
    return true;
}

bool SsaoPass::createDescriptors(VulkanContext& ctx) {
    // SSAO set layout: binding 0 = depth sampler, binding 1 = camera UBO.
    VkDescriptorSetLayoutBinding sb[2]{};
    sb[0].binding = 0;
    sb[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sb[0].descriptorCount = 1;
    sb[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    sb[1].binding = 1;
    sb[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sb[1].descriptorCount = 1;
    sb[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo li{};
    li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    li.bindingCount = 2;
    li.pBindings = sb;
    if (vkCreateDescriptorSetLayout(ctx.device(), &li, nullptr, &m_ssaoSetLayout) != VK_SUCCESS) {
        return false;
    }
    // Blur set layout: binding 0 = AO sampler.
    VkDescriptorSetLayoutBinding bb{};
    bb.binding = 0;
    bb.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bb.descriptorCount = 1;
    bb.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo bli{};
    bli.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    bli.bindingCount = 1;
    bli.pBindings = &bb;
    if (vkCreateDescriptorSetLayout(ctx.device(), &bli, nullptr, &m_blurSetLayout) != VK_SUCCESS) {
        return false;
    }
    VkDescriptorPoolSize ps[2]{};
    ps[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ps[0].descriptorCount = 2;
    ps[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ps[1].descriptorCount = 1;
    VkDescriptorPoolCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pi.maxSets = 2;
    pi.poolSizeCount = 2;
    pi.pPoolSizes = ps;
    if (vkCreateDescriptorPool(ctx.device(), &pi, nullptr, &m_pool) != VK_SUCCESS) {
        return false;
    }
    VkDescriptorSetAllocateInfo dai{};
    dai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dai.descriptorPool = m_pool;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &m_ssaoSetLayout;
    if (vkAllocateDescriptorSets(ctx.device(), &dai, &m_ssaoSet) != VK_SUCCESS) {
        return false;
    }
    dai.pSetLayouts = &m_blurSetLayout;
    if (vkAllocateDescriptorSets(ctx.device(), &dai, &m_blurSet) != VK_SUCCESS) {
        return false;
    }
    return true;
}

void SsaoPass::writeSsaoSet(VulkanContext& ctx, VkImageView depthView) {
    VkDescriptorImageInfo img{};
    img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    img.imageView = depthView;
    img.sampler = m_sampler;
    VkDescriptorBufferInfo buf{};
    buf.buffer = m_camUbo.handle();
    buf.offset = 0;
    buf.range = sizeof(CamUbo);
    VkWriteDescriptorSet w[2]{};
    w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w[0].dstSet = m_ssaoSet;
    w[0].dstBinding = 0;
    w[0].descriptorCount = 1;
    w[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w[0].pImageInfo = &img;
    w[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w[1].dstSet = m_ssaoSet;
    w[1].dstBinding = 1;
    w[1].descriptorCount = 1;
    w[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w[1].pBufferInfo = &buf;
    vkUpdateDescriptorSets(ctx.device(), 2, w, 0, nullptr);
}

void SsaoPass::writeBlurSet(VulkanContext& ctx) {
    VkDescriptorImageInfo img{};
    img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    img.imageView = m_ssao.view;
    img.sampler = m_sampler;
    VkWriteDescriptorSet w{};
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = m_blurSet;
    w.dstBinding = 0;
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo = &img;
    vkUpdateDescriptorSets(ctx.device(), 1, &w, 0, nullptr);
}

bool SsaoPass::createPipelines(VulkanContext& ctx) {
    const std::string base = assetBase();
    VkShaderModule vert =
        createShaderModule(ctx.device(), readFile(base + "shaders/composite.vert.spv"));
    VkShaderModule ssao = createShaderModule(ctx.device(), readFile(base + "shaders/ssao.frag.spv"));
    VkShaderModule blur =
        createShaderModule(ctx.device(), readFile(base + "shaders/ssao_blur.frag.spv"));
    if (!vert || !ssao || !blur) {
        return false;
    }
    VkPipelineLayoutCreateInfo pl{};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &m_ssaoSetLayout;
    if (vkCreatePipelineLayout(ctx.device(), &pl, nullptr, &m_ssaoLayout) != VK_SUCCESS) {
        return false;
    }
    pl.pSetLayouts = &m_blurSetLayout;
    if (vkCreatePipelineLayout(ctx.device(), &pl, nullptr, &m_blurLayout) != VK_SUCCESS) {
        return false;
    }

    auto makePipeline = [&](VkShaderModule frag, VkPipelineLayout layout, VkPipeline& out) -> bool {
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
        VkPipelineColorBlendAttachmentState ba{};
        ba.blendEnable = VK_FALSE;
        ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
        VkPipelineColorBlendStateCreateInfo cb{};
        cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.attachmentCount = 1;
        cb.pAttachments = &ba;
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
        gp.layout = layout;
        gp.renderPass = m_pass;
        return vkCreateGraphicsPipelines(ctx.device(), VK_NULL_HANDLE, 1, &gp, nullptr, &out) ==
               VK_SUCCESS;
    };
    const bool ok = makePipeline(ssao, m_ssaoLayout, m_ssaoPipeline) &&
                    makePipeline(blur, m_blurLayout, m_blurPipeline);
    vkDestroyShaderModule(ctx.device(), vert, nullptr);
    vkDestroyShaderModule(ctx.device(), ssao, nullptr);
    vkDestroyShaderModule(ctx.device(), blur, nullptr);
    if (!ok) {
        MAZ_LOG_ERROR("ssao vkCreateGraphicsPipelines failed");
        return false;
    }
    return true;
}

void SsaoPass::doPass(VkCommandBuffer cmd, VkFramebuffer fbo, VkPipeline pipeline,
                      VkPipelineLayout layout, VkDescriptorSet set) {
    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = m_pass;
    rp.framebuffer = fbo;
    rp.renderArea.extent = m_extent;
    rp.clearValueCount = 0;
    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport{};
    viewport.width = static_cast<float>(m_extent.width);
    viewport.height = static_cast<float>(m_extent.height);
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{};
    scissor.extent = m_extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &set, 0, nullptr);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
}

void SsaoPass::record(VulkanContext& ctx, VkCommandBuffer cmd, const float viewProj16[16],
                      const float invViewProj16[16], const float camPos3[3]) {
    (void)ctx;
    if (!m_enabled || m_ssaoPipeline == VK_NULL_HANDLE) {
        return;
    }
    CamUbo u{};
    std::memcpy(u.viewProj, viewProj16, sizeof(u.viewProj));
    std::memcpy(u.invViewProj, invViewProj16, sizeof(u.invViewProj));
    u.camPos[0] = camPos3[0];
    u.camPos[1] = camPos3[1];
    u.camPos[2] = camPos3[2];
    u.camPos[3] = 1.0f;
    u.params[0] = m_radius;
    u.params[1] = m_strength;
    u.params[2] = m_bias;
    u.params[3] = m_power;
    std::memcpy(m_camMapped, &u, sizeof(u));

    doPass(cmd, m_ssao.fbo, m_ssaoPipeline, m_ssaoLayout, m_ssaoSet); // depth -> raw AO
    doPass(cmd, m_blur.fbo, m_blurPipeline, m_blurLayout, m_blurSet); // raw AO -> blurred AO
}

void SsaoPass::destroyTargets(VulkanContext& ctx) {
    VkDevice d = ctx.device();
    Target* targets[2] = {&m_ssao, &m_blur};
    for (Target* tp : targets) {
        Target& t = *tp;
        if (t.fbo) vkDestroyFramebuffer(d, t.fbo, nullptr);
        if (t.view) vkDestroyImageView(d, t.view, nullptr);
        if (t.image) vkDestroyImage(d, t.image, nullptr);
        if (t.memory) vkFreeMemory(d, t.memory, nullptr);
        t = Target{};
    }
}

void SsaoPass::shutdown(VulkanContext& ctx) {
    VkDevice d = ctx.device();
    destroyTargets(ctx);
    if (m_camMapped) {
        m_camUbo.unmap(ctx);
        m_camMapped = nullptr;
    }
    m_camUbo.destroy(ctx);
    if (m_ssaoPipeline) vkDestroyPipeline(d, m_ssaoPipeline, nullptr);
    if (m_blurPipeline) vkDestroyPipeline(d, m_blurPipeline, nullptr);
    if (m_ssaoLayout) vkDestroyPipelineLayout(d, m_ssaoLayout, nullptr);
    if (m_blurLayout) vkDestroyPipelineLayout(d, m_blurLayout, nullptr);
    if (m_pool) vkDestroyDescriptorPool(d, m_pool, nullptr);
    if (m_ssaoSetLayout) vkDestroyDescriptorSetLayout(d, m_ssaoSetLayout, nullptr);
    if (m_blurSetLayout) vkDestroyDescriptorSetLayout(d, m_blurSetLayout, nullptr);
    if (m_sampler) vkDestroySampler(d, m_sampler, nullptr);
    if (m_pass) vkDestroyRenderPass(d, m_pass, nullptr);
    m_ssaoPipeline = m_blurPipeline = VK_NULL_HANDLE;
    m_ssaoLayout = m_blurLayout = VK_NULL_HANDLE;
    m_pool = VK_NULL_HANDLE;
    m_ssaoSetLayout = m_blurSetLayout = VK_NULL_HANDLE;
    m_sampler = VK_NULL_HANDLE;
    m_pass = VK_NULL_HANDLE;
}

} // namespace maz::render
