#include "render/MeshRenderer.hpp"

#include "maz/core/Log.hpp"
#include "render/TextureStore.hpp"
#include "render/VulkanBuffer.hpp"
#include "render/VulkanContext.hpp"

#include <SDL3/SDL_filesystem.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstddef>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace maz::render {

namespace {

// std140 layout matching the `Lights` UBO block in mesh.frag. Every member is vec4-aligned.
struct GpuPointLight {
    float posRange[4]; // xyz world position, w = range
    float color[4];    // rgb = color * intensity, w unused
};
struct GpuLights {
    float ambient[4];  // rgb ambient, w = active point-light count
    float sunDir[4];   // xyz direction toward the sun
    float sunColor[4]; // rgb directional color
    GpuPointLight points[SceneLighting::kMaxPointLights];
};

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
        MAZ_LOG_ERROR("mesh vkCreateShaderModule failed");
    }
    return module;
}

std::string assetBase() {
    const char* base = SDL_GetBasePath();
    return base ? std::string(base) : std::string();
}

VkVertexInputBindingDescription meshBinding() {
    VkVertexInputBindingDescription vb{};
    vb.binding = 0;
    vb.stride = sizeof(MeshVertex);
    vb.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return vb;
}

} // namespace

bool MeshRenderer::init(VulkanContext& ctx, TextureStore& store, VkRenderPass renderPass,
                        VkSampleCountFlagBits samples) {
    m_store = &store;
    m_samples = samples;
    m_meshes.emplace_back(); // reserve index 0 == kInvalidMesh

    // Directional light space: an orthographic volume over the scene, looking along the light.
    const glm::vec3 L = glm::normalize(glm::vec3(0.4f, 0.8f, 0.6f));
    const glm::mat4 lightView =
        glm::lookAt(L * 60.0f, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 lightProj = glm::ortho(-40.0f, 40.0f, -40.0f, 40.0f, 1.0f, 140.0f);
    std::memcpy(m_lightVP, glm::value_ptr(lightProj * lightView), sizeof(m_lightVP));

    if (!createShadowResources(ctx) || !createShadowPipeline(ctx) ||
        !createSkyPipeline(ctx, renderPass) || !createLightResources(ctx) ||
        !createPipeline(ctx, renderPass)) {
        return false;
    }
    setLighting(ctx, SceneLighting{}); // default daytime look until an app overrides it
    return true;
}

bool MeshRenderer::createSkyPipeline(VulkanContext& ctx, VkRenderPass renderPass) {
    const std::string base = assetBase();
    VkShaderModule vert = createShaderModule(ctx.device(), readFile(base + "shaders/sky.vert.spv"));
    VkShaderModule frag = createShaderModule(ctx.device(), readFile(base + "shaders/sky.frag.spv"));
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

    VkPipelineVertexInputStateCreateInfo vi{}; // no vertex buffer
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
    ms.rasterizationSamples = m_samples;

    VkPipelineDepthStencilStateCreateInfo ds{}; // sky writes no depth and ignores it
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;
    ds.depthCompareOp = VK_COMPARE_OP_ALWAYS;

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
    push.size = sizeof(float) * 16; // invViewProj
    VkPipelineLayoutCreateInfo pl{};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &push;
    if (vkCreatePipelineLayout(ctx.device(), &pl, nullptr, &m_skyLayout) != VK_SUCCESS) {
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
    gp.pDepthStencilState = &ds;
    gp.pColorBlendState = &cb;
    gp.pDynamicState = &dyn;
    gp.layout = m_skyLayout;
    gp.renderPass = renderPass;
    VkResult r =
        vkCreateGraphicsPipelines(ctx.device(), VK_NULL_HANDLE, 1, &gp, nullptr, &m_skyPipeline);
    vkDestroyShaderModule(ctx.device(), vert, nullptr);
    vkDestroyShaderModule(ctx.device(), frag, nullptr);
    if (r != VK_SUCCESS) {
        MAZ_LOG_ERROR("sky pipeline failed (VkResult %d)", (int)r);
        return false;
    }
    return true;
}

void MeshRenderer::renderSky(VkCommandBuffer cmd) {
    if (m_cmds.empty() || m_skyPipeline == VK_NULL_HANDLE) {
        return; // only draw a sky when there's a 3D scene
    }
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyPipeline);
    VkViewport viewport{};
    viewport.width = static_cast<float>(m_viewportW);
    viewport.height = static_cast<float>(m_viewportH);
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{};
    scissor.extent = {m_viewportW, m_viewportH};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    const glm::mat4 invVP = glm::inverse(glm::make_mat4(m_viewProj));
    vkCmdPushConstants(cmd, m_skyLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float) * 16,
                       glm::value_ptr(invVP));
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

bool MeshRenderer::createShadowResources(VulkanContext& ctx) {
    // Depth image, sampled in the main pass.
    VkImageCreateInfo ii{};
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = VK_FORMAT_D32_SFLOAT;
    ii.extent = {kShadowSize, kShadowSize, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(ctx.device(), &ii, nullptr, &m_shadowImage) != VK_SUCCESS) {
        MAZ_LOG_ERROR("shadow vkCreateImage failed");
        return false;
    }
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(ctx.device(), m_shadowImage, &req);
    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex =
        findMemoryType(ctx, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (ai.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(ctx.device(), &ai, nullptr, &m_shadowMemory) != VK_SUCCESS) {
        MAZ_LOG_ERROR("shadow memory allocation failed");
        return false;
    }
    vkBindImageMemory(ctx.device(), m_shadowImage, m_shadowMemory, 0);

    VkImageViewCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = m_shadowImage;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = VK_FORMAT_D32_SFLOAT;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    if (vkCreateImageView(ctx.device(), &vi, nullptr, &m_shadowView) != VK_SUCCESS) {
        MAZ_LOG_ERROR("shadow vkCreateImageView failed");
        return false;
    }

    VkSamplerCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter = VK_FILTER_NEAREST;
    si.minFilter = VK_FILTER_NEAREST;
    si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.maxLod = 0.0f;
    if (vkCreateSampler(ctx.device(), &si, nullptr, &m_shadowSampler) != VK_SUCCESS) {
        MAZ_LOG_ERROR("shadow vkCreateSampler failed");
        return false;
    }

    // Depth-only render pass; ends in SHADER_READ_ONLY so the main pass can sample it.
    VkAttachmentDescription depth{};
    depth.format = VK_FORMAT_D32_SFLOAT;
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentReference depthRef{};
    depthRef.attachment = 0;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pDepthStencilAttachment = &depthRef;
    VkSubpassDependency deps[2]{};
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;
    deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    VkRenderPassCreateInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp.attachmentCount = 1;
    rp.pAttachments = &depth;
    rp.subpassCount = 1;
    rp.pSubpasses = &subpass;
    rp.dependencyCount = 2;
    rp.pDependencies = deps;
    if (vkCreateRenderPass(ctx.device(), &rp, nullptr, &m_shadowPass) != VK_SUCCESS) {
        MAZ_LOG_ERROR("shadow vkCreateRenderPass failed");
        return false;
    }

    VkFramebufferCreateInfo fb{};
    fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb.renderPass = m_shadowPass;
    fb.attachmentCount = 1;
    fb.pAttachments = &m_shadowView;
    fb.width = kShadowSize;
    fb.height = kShadowSize;
    fb.layers = 1;
    if (vkCreateFramebuffer(ctx.device(), &fb, nullptr, &m_shadowFbo) != VK_SUCCESS) {
        MAZ_LOG_ERROR("shadow vkCreateFramebuffer failed");
        return false;
    }

    // set = 1 : the shadow map, sampled in the main fragment shader.
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo li{};
    li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    li.bindingCount = 1;
    li.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(ctx.device(), &li, nullptr, &m_shadowSetLayout) != VK_SUCCESS) {
        MAZ_LOG_ERROR("shadow set layout failed");
        return false;
    }
    VkDescriptorPoolSize ps{};
    ps.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ps.descriptorCount = 1;
    VkDescriptorPoolCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pi.maxSets = 1;
    pi.poolSizeCount = 1;
    pi.pPoolSizes = &ps;
    if (vkCreateDescriptorPool(ctx.device(), &pi, nullptr, &m_shadowPool) != VK_SUCCESS) {
        MAZ_LOG_ERROR("shadow pool failed");
        return false;
    }
    VkDescriptorSetAllocateInfo dai{};
    dai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dai.descriptorPool = m_shadowPool;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &m_shadowSetLayout;
    if (vkAllocateDescriptorSets(ctx.device(), &dai, &m_shadowSet) != VK_SUCCESS) {
        MAZ_LOG_ERROR("shadow set alloc failed");
        return false;
    }
    VkDescriptorImageInfo img{};
    img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    img.imageView = m_shadowView;
    img.sampler = m_shadowSampler;
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_shadowSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &img;
    vkUpdateDescriptorSets(ctx.device(), 1, &write, 0, nullptr);
    return true;
}

bool MeshRenderer::createShadowPipeline(VulkanContext& ctx) {
    const std::string base = assetBase();
    VkShaderModule vert =
        createShaderModule(ctx.device(), readFile(base + "shaders/shadow.vert.spv"));
    VkShaderModule frag =
        createShaderModule(ctx.device(), readFile(base + "shaders/shadow.frag.spv"));
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

    VkVertexInputBindingDescription vb = meshBinding();
    VkVertexInputAttributeDescription attr{};
    attr.location = 0;
    attr.format = VK_FORMAT_R32G32B32_SFLOAT;
    attr.offset = offsetof(MeshVertex, px);
    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &vb;
    vi.vertexAttributeDescriptionCount = 1;
    vi.pVertexAttributeDescriptions = &attr;

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
    rs.depthBiasEnable = VK_TRUE; // slope bias reduces shadow acne
    rs.depthBiasConstantFactor = 1.5f;
    rs.depthBiasSlopeFactor = 2.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;

    VkDynamicState dynamics[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynamics;

    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push.offset = 0;
    push.size = sizeof(float) * 16; // lightMVP
    VkPipelineLayoutCreateInfo pl{};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &push;
    if (vkCreatePipelineLayout(ctx.device(), &pl, nullptr, &m_shadowLayout) != VK_SUCCESS) {
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
    gp.pDepthStencilState = &ds;
    gp.pDynamicState = &dyn;
    gp.layout = m_shadowLayout;
    gp.renderPass = m_shadowPass;
    VkResult r = vkCreateGraphicsPipelines(ctx.device(), VK_NULL_HANDLE, 1, &gp, nullptr,
                                           &m_shadowPipeline);
    vkDestroyShaderModule(ctx.device(), vert, nullptr);
    vkDestroyShaderModule(ctx.device(), frag, nullptr);
    if (r != VK_SUCCESS) {
        MAZ_LOG_ERROR("shadow pipeline failed (VkResult %d)", (int)r);
        return false;
    }
    return true;
}

bool MeshRenderer::createLightResources(VulkanContext& ctx) {
    // A host-visible, persistently-mapped UBO for the lighting block, plus its set-2 layout/set.
    if (!m_lightUbo.create(ctx, sizeof(GpuLights), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        MAZ_LOG_ERROR("lights UBO create failed");
        return false;
    }
    m_lightMapped = m_lightUbo.map(ctx);
    if (!m_lightMapped) {
        MAZ_LOG_ERROR("lights UBO map failed");
        return false;
    }

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo li{};
    li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    li.bindingCount = 1;
    li.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(ctx.device(), &li, nullptr, &m_lightSetLayout) != VK_SUCCESS) {
        MAZ_LOG_ERROR("lights set layout failed");
        return false;
    }
    VkDescriptorPoolSize ps{};
    ps.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ps.descriptorCount = 1;
    VkDescriptorPoolCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pi.maxSets = 1;
    pi.poolSizeCount = 1;
    pi.pPoolSizes = &ps;
    if (vkCreateDescriptorPool(ctx.device(), &pi, nullptr, &m_lightPool) != VK_SUCCESS) {
        MAZ_LOG_ERROR("lights pool failed");
        return false;
    }
    VkDescriptorSetAllocateInfo dai{};
    dai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dai.descriptorPool = m_lightPool;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &m_lightSetLayout;
    if (vkAllocateDescriptorSets(ctx.device(), &dai, &m_lightSet) != VK_SUCCESS) {
        MAZ_LOG_ERROR("lights set alloc failed");
        return false;
    }
    VkDescriptorBufferInfo bufInfo{};
    bufInfo.buffer = m_lightUbo.handle();
    bufInfo.offset = 0;
    bufInfo.range = sizeof(GpuLights);
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_lightSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &bufInfo;
    vkUpdateDescriptorSets(ctx.device(), 1, &write, 0, nullptr);
    return true;
}

void MeshRenderer::setLighting(VulkanContext& ctx, const SceneLighting& lighting) {
    (void)ctx;
    if (!m_lightMapped) {
        return;
    }
    const uint32_t count = lighting.pointCount < SceneLighting::kMaxPointLights
                               ? lighting.pointCount
                               : SceneLighting::kMaxPointLights;
    GpuLights g{};
    for (int i = 0; i < 3; ++i) {
        g.ambient[i] = lighting.ambient[i];
        g.sunDir[i] = lighting.sunDir[i];
        g.sunColor[i] = lighting.sunColor[i];
    }
    g.ambient[3] = static_cast<float>(count);
    for (uint32_t i = 0; i < count; ++i) {
        const SceneLighting::Point& p = lighting.points[i];
        g.points[i].posRange[0] = p.pos[0];
        g.points[i].posRange[1] = p.pos[1];
        g.points[i].posRange[2] = p.pos[2];
        g.points[i].posRange[3] = p.range;
        g.points[i].color[0] = p.color[0] * p.intensity;
        g.points[i].color[1] = p.color[1] * p.intensity;
        g.points[i].color[2] = p.color[2] * p.intensity;
    }
    std::memcpy(m_lightMapped, &g, sizeof(g));
}

bool MeshRenderer::createPipeline(VulkanContext& ctx, VkRenderPass renderPass) {
    const std::string base = assetBase();
    VkShaderModule vert = createShaderModule(ctx.device(), readFile(base + "shaders/mesh.vert.spv"));
    VkShaderModule frag = createShaderModule(ctx.device(), readFile(base + "shaders/mesh.frag.spv"));
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

    VkVertexInputBindingDescription vb = meshBinding();
    VkVertexInputAttributeDescription attrs[4]{};
    attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = offsetof(MeshVertex, px);
    attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset = offsetof(MeshVertex, nx);
    attrs[2].location = 2;
    attrs[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[2].offset = offsetof(MeshVertex, r);
    attrs[3].location = 3;
    attrs[3].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[3].offset = offsetof(MeshVertex, u);
    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &vb;
    vi.vertexAttributeDescriptionCount = 4;
    vi.pVertexAttributeDescriptions = attrs;

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

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;

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
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push.offset = 0;
    push.size = sizeof(float) * 48; // mvp + model + lightVP

    const VkDescriptorSetLayout setLayouts[] = {m_store->layout(), m_shadowSetLayout,
                                                m_lightSetLayout};
    VkPipelineLayoutCreateInfo pl{};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount = 3;
    pl.pSetLayouts = setLayouts;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &push;
    if (vkCreatePipelineLayout(ctx.device(), &pl, nullptr, &m_layout) != VK_SUCCESS) {
        MAZ_LOG_ERROR("mesh vkCreatePipelineLayout failed");
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
    gp.pDepthStencilState = &ds;
    gp.pColorBlendState = &cb;
    gp.pDynamicState = &dyn;
    gp.layout = m_layout;
    gp.renderPass = renderPass;
    VkResult r = vkCreateGraphicsPipelines(ctx.device(), VK_NULL_HANDLE, 1, &gp, nullptr,
                                           &m_pipeline);
    vkDestroyShaderModule(ctx.device(), vert, nullptr);
    vkDestroyShaderModule(ctx.device(), frag, nullptr);
    if (r != VK_SUCCESS) {
        MAZ_LOG_ERROR("mesh vkCreateGraphicsPipelines failed (VkResult %d)", (int)r);
        return false;
    }
    return true;
}

MeshHandle MeshRenderer::createMesh(VulkanContext& ctx, const MeshVertex* vertices,
                                    uint32_t vertexCount, const uint32_t* indices,
                                    uint32_t indexCount) {
    Mesh mesh;
    mesh.indexCount = indexCount;
    const VkDeviceSize vbytes = static_cast<VkDeviceSize>(vertexCount) * sizeof(MeshVertex);
    const VkDeviceSize ibytes = static_cast<VkDeviceSize>(indexCount) * sizeof(uint32_t);
    const VkMemoryPropertyFlags hostVisible =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    if (!mesh.vbo.create(ctx, vbytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, hostVisible) ||
        !mesh.ibo.create(ctx, ibytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, hostVisible)) {
        mesh.vbo.destroy(ctx);
        mesh.ibo.destroy(ctx);
        return kInvalidMesh;
    }
    if (void* vp = mesh.vbo.map(ctx)) {
        std::memcpy(vp, vertices, static_cast<size_t>(vbytes));
    }
    if (void* ip = mesh.ibo.map(ctx)) {
        std::memcpy(ip, indices, static_cast<size_t>(ibytes));
    }
    m_meshes.push_back(std::move(mesh));
    return static_cast<MeshHandle>(m_meshes.size() - 1);
}

void MeshRenderer::setViewProjection(const float* viewProj16) {
    std::memcpy(m_viewProj, viewProj16, sizeof(m_viewProj));
}

void MeshRenderer::begin() { m_cmds.clear(); }

void MeshRenderer::draw(MeshHandle mesh, const float* model16, TextureHandle texture) {
    if (mesh == kInvalidMesh || mesh >= m_meshes.size()) {
        return;
    }
    DrawCmd cmd;
    cmd.mesh = mesh;
    cmd.texture = texture;
    std::memcpy(cmd.model, model16, sizeof(cmd.model));
    m_cmds.push_back(cmd);
}

void MeshRenderer::renderShadow(VkCommandBuffer cmd) {
    if (m_cmds.empty() || m_shadowPipeline == VK_NULL_HANDLE) {
        return;
    }
    VkClearValue clear{};
    clear.depthStencil = {1.0f, 0};
    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = m_shadowPass;
    rp.framebuffer = m_shadowFbo;
    rp.renderArea.extent = {kShadowSize, kShadowSize};
    rp.clearValueCount = 1;
    rp.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.width = static_cast<float>(kShadowSize);
    viewport.height = static_cast<float>(kShadowSize);
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{};
    scissor.extent = {kShadowSize, kShadowSize};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline);
    const glm::mat4 lightVP = glm::make_mat4(m_lightVP);
    for (const DrawCmd& dc : m_cmds) {
        const Mesh& mesh = m_meshes[dc.mesh];
        const glm::mat4 lightMVP = lightVP * glm::make_mat4(dc.model);
        vkCmdPushConstants(cmd, m_shadowLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 16,
                           glm::value_ptr(lightMVP));
        VkDeviceSize offset = 0;
        VkBuffer vbuf = mesh.vbo.handle();
        vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &offset);
        vkCmdBindIndexBuffer(cmd, mesh.ibo.handle(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
    }
    vkCmdEndRenderPass(cmd);
}

void MeshRenderer::flush(VkCommandBuffer cmd) {
    if (m_cmds.empty() || m_pipeline == VK_NULL_HANDLE) {
        return;
    }
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    VkViewport viewport{};
    viewport.width = static_cast<float>(m_viewportW);
    viewport.height = static_cast<float>(m_viewportH);
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{};
    scissor.extent = {m_viewportW, m_viewportH};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // The shadow map (set = 1) and lights UBO (set = 2) are constant across the frame.
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 1, 1, &m_shadowSet, 0,
                            nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 2, 1, &m_lightSet, 0,
                            nullptr);

    const glm::mat4 vp = glm::make_mat4(m_viewProj);
    const glm::mat4 lightVP = glm::make_mat4(m_lightVP);
    for (const DrawCmd& dc : m_cmds) {
        const Mesh& mesh = m_meshes[dc.mesh];
        const glm::mat4 model = glm::make_mat4(dc.model);
        const glm::mat4 mvp = vp * model;

        float push[48];
        std::memcpy(push, glm::value_ptr(mvp), sizeof(float) * 16);
        std::memcpy(push + 16, glm::value_ptr(model), sizeof(float) * 16);
        std::memcpy(push + 32, glm::value_ptr(lightVP), sizeof(float) * 16);
        vkCmdPushConstants(cmd, m_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), push);

        VkDescriptorSet albedo = m_store->descriptorSet(dc.texture);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 0, 1, &albedo, 0,
                                nullptr);

        VkDeviceSize offset = 0;
        VkBuffer vbuf = mesh.vbo.handle();
        vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &offset);
        vkCmdBindIndexBuffer(cmd, mesh.ibo.handle(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
    }
}

void MeshRenderer::shutdown(VulkanContext& ctx) {
    VkDevice d = ctx.device();
    for (Mesh& mesh : m_meshes) {
        mesh.vbo.destroy(ctx);
        mesh.ibo.destroy(ctx);
    }
    m_meshes.clear();

    if (m_pipeline) vkDestroyPipeline(d, m_pipeline, nullptr);
    if (m_layout) vkDestroyPipelineLayout(d, m_layout, nullptr);
    if (m_skyPipeline) vkDestroyPipeline(d, m_skyPipeline, nullptr);
    if (m_skyLayout) vkDestroyPipelineLayout(d, m_skyLayout, nullptr);
    if (m_shadowPipeline) vkDestroyPipeline(d, m_shadowPipeline, nullptr);
    if (m_shadowLayout) vkDestroyPipelineLayout(d, m_shadowLayout, nullptr);
    if (m_shadowPool) vkDestroyDescriptorPool(d, m_shadowPool, nullptr);
    if (m_shadowSetLayout) vkDestroyDescriptorSetLayout(d, m_shadowSetLayout, nullptr);
    if (m_lightPool) vkDestroyDescriptorPool(d, m_lightPool, nullptr);
    if (m_lightSetLayout) vkDestroyDescriptorSetLayout(d, m_lightSetLayout, nullptr);
    m_lightUbo.destroy(ctx);
    m_lightMapped = nullptr;
    if (m_shadowFbo) vkDestroyFramebuffer(d, m_shadowFbo, nullptr);
    if (m_shadowPass) vkDestroyRenderPass(d, m_shadowPass, nullptr);
    if (m_shadowSampler) vkDestroySampler(d, m_shadowSampler, nullptr);
    if (m_shadowView) vkDestroyImageView(d, m_shadowView, nullptr);
    if (m_shadowImage) vkDestroyImage(d, m_shadowImage, nullptr);
    if (m_shadowMemory) vkFreeMemory(d, m_shadowMemory, nullptr);
    m_pipeline = VK_NULL_HANDLE;
    m_layout = VK_NULL_HANDLE;
    m_skyPipeline = VK_NULL_HANDLE;
    m_skyLayout = VK_NULL_HANDLE;
    m_shadowPipeline = VK_NULL_HANDLE;
    m_shadowLayout = VK_NULL_HANDLE;
    m_shadowPool = VK_NULL_HANDLE;
    m_shadowSetLayout = VK_NULL_HANDLE;
    m_shadowFbo = VK_NULL_HANDLE;
    m_shadowPass = VK_NULL_HANDLE;
    m_shadowSampler = VK_NULL_HANDLE;
    m_shadowView = VK_NULL_HANDLE;
    m_shadowImage = VK_NULL_HANDLE;
    m_shadowMemory = VK_NULL_HANDLE;
}

} // namespace maz::render
