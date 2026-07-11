#include "render/PostProcess.hpp"

#include "maz/core/Log.hpp"
#include "render/VulkanContext.hpp"

#include <SDL3/SDL_filesystem.h>

#include <cstdint>
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
        MAZ_LOG_ERROR("composite vkCreateShaderModule failed");
    }
    return module;
}

std::string assetBase() {
    const char* base = SDL_GetBasePath();
    return base ? std::string(base) : std::string();
}

struct PushParams {
    float params[4]; // x = bloom strength, y = threshold, z = exposure, w = tonemap enable
    float grade[4];  // x = vignette, y = saturation, z = contrast, w = color-grade enable
    float extra[4];  // x = chromatic aberration strength
};

} // namespace

bool PostProcess::init(VulkanContext& ctx, VkRenderPass compositePass, VkImageView sceneView,
                       VkSampler sceneSampler, VkImageView bloomView, VkSampler bloomSampler) {
    // Descriptor set: combined image samplers for the scene color (0) and the bloom texture (1).
    VkDescriptorSetLayoutBinding bindings[2]{};
    for (uint32_t i = 0; i < 2; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    VkDescriptorSetLayoutCreateInfo li{};
    li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    li.bindingCount = 2;
    li.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(ctx.device(), &li, nullptr, &m_setLayout) != VK_SUCCESS) {
        MAZ_LOG_ERROR("composite set layout failed");
        return false;
    }
    VkDescriptorPoolSize ps{};
    ps.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ps.descriptorCount = 2;
    VkDescriptorPoolCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pi.maxSets = 1;
    pi.poolSizeCount = 1;
    pi.pPoolSizes = &ps;
    if (vkCreateDescriptorPool(ctx.device(), &pi, nullptr, &m_pool) != VK_SUCCESS) {
        MAZ_LOG_ERROR("composite pool failed");
        return false;
    }
    VkDescriptorSetAllocateInfo dai{};
    dai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dai.descriptorPool = m_pool;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &m_setLayout;
    if (vkAllocateDescriptorSets(ctx.device(), &dai, &m_set) != VK_SUCCESS) {
        MAZ_LOG_ERROR("composite set alloc failed");
        return false;
    }
    writeDescriptor(ctx, sceneView, sceneSampler, bloomView, bloomSampler);
    m_compositePass = compositePass;
    return createPipeline(ctx, compositePass);
}

void PostProcess::writeDescriptor(VulkanContext& ctx, VkImageView sceneView, VkSampler sceneSampler,
                                  VkImageView bloomView, VkSampler bloomSampler) {
    VkDescriptorImageInfo imgs[2]{};
    imgs[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imgs[0].imageView = sceneView;
    imgs[0].sampler = sceneSampler;
    imgs[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imgs[1].imageView = bloomView;
    imgs[1].sampler = bloomSampler;
    VkWriteDescriptorSet writes[2]{};
    for (uint32_t i = 0; i < 2; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = m_set;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].pImageInfo = &imgs[i];
    }
    vkUpdateDescriptorSets(ctx.device(), 2, writes, 0, nullptr);
}

void PostProcess::updateSource(VulkanContext& ctx, VkImageView sceneView, VkSampler sceneSampler,
                               VkImageView bloomView, VkSampler bloomSampler) {
    writeDescriptor(ctx, sceneView, sceneSampler, bloomView, bloomSampler);
}

bool PostProcess::createPipeline(VulkanContext& ctx, VkRenderPass compositePass) {
    const std::string base = assetBase();
    VkShaderModule vert =
        createShaderModule(ctx.device(), readFile(base + "shaders/composite.vert.spv"));
    VkShaderModule frag =
        createShaderModule(ctx.device(), readFile(base + "shaders/composite.frag.spv"));
    if (!vert || !frag) {
        return false;
    }

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

    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_FALSE;
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &blend;

    VkDynamicState dynamics[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynamics;

    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    push.offset = 0;
    push.size = sizeof(PushParams);

    VkPipelineLayoutCreateInfo pl{};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &m_setLayout;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &push;
    if (vkCreatePipelineLayout(ctx.device(), &pl, nullptr, &m_layout) != VK_SUCCESS) {
        MAZ_LOG_ERROR("composite pipeline layout failed");
        vkDestroyShaderModule(ctx.device(), vert, nullptr);
        vkDestroyShaderModule(ctx.device(), frag, nullptr);
        return false;
    }

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
    gp.renderPass = compositePass;
    const VkResult r =
        vkCreateGraphicsPipelines(ctx.device(), VK_NULL_HANDLE, 1, &gp, nullptr, &m_pipeline);
    vkDestroyShaderModule(ctx.device(), vert, nullptr);
    vkDestroyShaderModule(ctx.device(), frag, nullptr);
    if (r != VK_SUCCESS) {
        MAZ_LOG_ERROR("composite vkCreateGraphicsPipelines failed (VkResult %d)", (int)r);
        return false;
    }
    return true;
}

void PostProcess::record(VkCommandBuffer cmd, VkFramebuffer framebuffer, VkExtent2D extent) {
    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = m_compositePass;
    rp.framebuffer = framebuffer;
    rp.renderArea.extent = extent;
    rp.clearValueCount = 0; // loadOp is DONT_CARE; the composite writes every pixel
    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    VkViewport viewport{};
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{};
    scissor.extent = extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 0, 1, &m_set, 0, nullptr);
    PushParams pc{};
    pc.params[0] = m_strength;
    pc.params[1] = m_threshold;
    pc.params[2] = m_exposure;
    pc.params[3] = m_tonemap ? 1.0f : 0.0f;
    pc.grade[0] = m_vignette;
    pc.grade[1] = m_saturation;
    pc.grade[2] = m_contrast;
    pc.grade[3] = m_colorGrade ? 1.0f : 0.0f;
    pc.extra[0] = m_chromatic;
    vkCmdPushConstants(cmd, m_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
}

void PostProcess::shutdown(VulkanContext& ctx) {
    VkDevice d = ctx.device();
    if (m_pipeline) vkDestroyPipeline(d, m_pipeline, nullptr);
    if (m_layout) vkDestroyPipelineLayout(d, m_layout, nullptr);
    if (m_pool) vkDestroyDescriptorPool(d, m_pool, nullptr);
    if (m_setLayout) vkDestroyDescriptorSetLayout(d, m_setLayout, nullptr);
    m_pipeline = VK_NULL_HANDLE;
    m_layout = VK_NULL_HANDLE;
    m_pool = VK_NULL_HANDLE;
    m_setLayout = VK_NULL_HANDLE;
    m_set = VK_NULL_HANDLE;
}

} // namespace maz::render
