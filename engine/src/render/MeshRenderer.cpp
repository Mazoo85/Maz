#include "render/MeshRenderer.hpp"

#include "maz/core/Log.hpp"
#include "render/TextureStore.hpp"
#include "render/VulkanBuffer.hpp"
#include "render/VulkanContext.hpp"

#include <SDL3/SDL_filesystem.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
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
    float color[4];    // rgb = color * intensity, w = cos(inner) for spots (< -1.5 = omni)
    float spot[4];     // xyz spot axis, w = cos(outer)
};
struct GpuLights {
    // Per-frame camera/light matrices (written by flush each frame), then the lighting state
    // (written by setLighting). The split lets each writer touch only its own region.
    float viewProj[16];
    float lightVP[16];
    float camPos[4];   // xyz world-space camera position
    float ambient[4];  // rgb ambient, w = active point-light count
    float sunDir[4];   // xyz direction toward the sun
    float sunColor[4]; // rgb directional color
    float fog[4];      // rgb fog color, w = density (0 disables)
    // Sky gradient colors, mirrored from the sky pass so PBR materials can do analytic image-based
    // lighting (ambient irradiance + environment reflections) from the same sky the skybox draws.
    float skyZenith[4];
    float skyHorizon[4];
    float skyGround[4];
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

// Six world-space frustum planes (inward normals) extracted from a view-projection matrix
// (Gribb-Hartmann; near uses row2 for Vulkan's [0,1] clip depth).
struct Frustum {
    glm::vec4 planes[6];
};
Frustum makeFrustum(const glm::mat4& m) {
    const glm::vec4 r0(m[0][0], m[1][0], m[2][0], m[3][0]);
    const glm::vec4 r1(m[0][1], m[1][1], m[2][1], m[3][1]);
    const glm::vec4 r2(m[0][2], m[1][2], m[2][2], m[3][2]);
    const glm::vec4 r3(m[0][3], m[1][3], m[2][3], m[3][3]);
    Frustum f;
    f.planes[0] = r3 + r0; // left
    f.planes[1] = r3 - r0; // right
    f.planes[2] = r3 + r1; // bottom
    f.planes[3] = r3 - r1; // top
    f.planes[4] = r2;      // near
    f.planes[5] = r3 - r2; // far
    return f;
}
// True if the world-space AABB [mn,mx] is at least partially inside the frustum.
bool aabbInFrustum(const Frustum& f, const glm::vec3& mn, const glm::vec3& mx) {
    for (const glm::vec4& p : f.planes) {
        const glm::vec3 pv(p.x > 0 ? mx.x : mn.x, p.y > 0 ? mx.y : mn.y, p.z > 0 ? mx.z : mn.z);
        if (p.x * pv.x + p.y * pv.y + p.z * pv.z + p.w < 0.0f) {
            return false; // fully outside this plane
        }
    }
    return true;
}
// Local-space AABB (min/max over vertex positions) used for frustum culling.
void computeLocalAabb(const MeshVertex* vertices, uint32_t count, float bmin[3], float bmax[3]) {
    if (count == 0) {
        bmin[0] = bmin[1] = bmin[2] = bmax[0] = bmax[1] = bmax[2] = 0.0f;
        return;
    }
    bmin[0] = bmax[0] = vertices[0].px;
    bmin[1] = bmax[1] = vertices[0].py;
    bmin[2] = bmax[2] = vertices[0].pz;
    for (uint32_t i = 1; i < count; ++i) {
        const float p[3] = {vertices[i].px, vertices[i].py, vertices[i].pz};
        for (int k = 0; k < 3; ++k) {
            bmin[k] = p[k] < bmin[k] ? p[k] : bmin[k];
            bmax[k] = p[k] > bmax[k] ? p[k] : bmax[k];
        }
    }
}
// World-space AABB of a local AABB transformed by `model` (8 corners).
void worldAabb(const glm::mat4& model, const float bmin[3], const float bmax[3], glm::vec3& outMin,
               glm::vec3& outMax) {
    outMin = glm::vec3(1e30f);
    outMax = glm::vec3(-1e30f);
    for (int i = 0; i < 8; ++i) {
        const glm::vec3 c((i & 1) ? bmax[0] : bmin[0], (i & 2) ? bmax[1] : bmin[1],
                          (i & 4) ? bmax[2] : bmin[2]);
        const glm::vec3 w = glm::vec3(model * glm::vec4(c, 1.0f));
        outMin = glm::min(outMin, w);
        outMax = glm::max(outMax, w);
    }
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
                        VkSampleCountFlagBits samples, uint32_t framesInFlight) {
    m_store = &store;
    m_samples = samples;
    m_framesInFlight = framesInFlight > 0 ? framesInFlight : 1;
    m_meshes.emplace_back(); // reserve index 0 == kInvalidMesh

    // Directional light space: an orthographic volume over the scene, looking along the light.
    const glm::vec3 L = glm::normalize(glm::vec3(0.4f, 0.8f, 0.6f));
    const glm::mat4 lightView =
        glm::lookAt(L * 60.0f, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 lightProj = glm::ortho(-40.0f, 40.0f, -40.0f, 40.0f, 1.0f, 140.0f);
    std::memcpy(m_lightVP, glm::value_ptr(lightProj * lightView), sizeof(m_lightVP));

    // A flat tangent-space normal (0,0,1) -> RGBA (128,128,255,255); the default when a mesh has no
    // normal map, so those meshes are lit by their geometric normal (unchanged).
    const uint8_t flatNormal[4] = {128, 128, 255, 255};
    m_defaultNormal = store.createFromPixels(ctx, 1, 1, flatNormal);

    if (!createShadowResources(ctx) || !createShadowPipeline(ctx) ||
        !createSkyPipeline(ctx, renderPass) || !createLightResources(ctx) ||
        !createPipeline(ctx, renderPass, VK_POLYGON_MODE_FILL, m_pipeline) ||
        !createPipeline(ctx, renderPass, VK_POLYGON_MODE_FILL, m_instancedPipeline, true) ||
        !createPipeline(ctx, renderPass, VK_POLYGON_MODE_FILL, m_transparentPipeline, false, true) ||
        !createInstanceBuffers(ctx)) {
        return false;
    }
    // Optional wireframe pipeline (needs the fillModeNonSolid feature); best-effort.
    if (ctx.wireframeSupported()) {
        createPipeline(ctx, renderPass, VK_POLYGON_MODE_LINE, m_wireframePipeline);
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
    // 32 floats = 128 bytes, the guaranteed mobile/MoltenVK maxPushConstantsSize floor. sunColor.rgb is
    // packed into the w channels of zenith/horizon/ground (see sky.frag) rather than a 5th vec4, which would
    // make the block 144 bytes and fail pipeline creation on GPUs that cap push constants at 128.
    push.size = sizeof(float) * 32; // invViewProj(16) + zenith/horizon/ground(w=sunColor)/sunDir
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

    // Push: invViewProj (16) + zenith/horizon/ground (rgb, w=sunColor.rgb) + sunDir (xyz) = 32 floats =
    // 128 bytes. Packing sunColor into the three w lanes keeps the block at the mobile push-constant floor
    // (see sky.frag + the push-range comment above); a separate sunColor vec4 would overflow to 144 bytes.
    float push[32] = {0};
    const glm::mat4 invVP = glm::inverse(glm::make_mat4(m_viewProj));
    std::memcpy(push, glm::value_ptr(invVP), sizeof(float) * 16);
    auto putColorW = [&](int base, const float* rgb, float w) {
        push[base] = rgb[0];
        push[base + 1] = rgb[1];
        push[base + 2] = rgb[2];
        push[base + 3] = w;
    };
    putColorW(16, m_skyZenith, m_sunColor[0]);
    putColorW(20, m_skyHorizon, m_sunColor[1]);
    putColorW(24, m_skyGround, m_sunColor[2]);
    push[28] = m_sunDir[0];
    push[29] = m_sunDir[1];
    push[30] = m_sunDir[2];
    push[31] = 0.0f;
    vkCmdPushConstants(cmd, m_skyLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), push);
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
    // The vertex shader now reads viewProj/lightVP from this UBO too, not just the fragment shader.
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
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
        // Cache the sky/sun params for the sky pass (which uses push constants, not the UBO).
        m_sunDir[i] = lighting.sunDir[i];
        m_sunColor[i] = lighting.sunColor[i];
        m_skyZenith[i] = lighting.skyZenith[i];
        m_skyHorizon[i] = lighting.skyHorizon[i];
        m_skyGround[i] = lighting.skyGround[i];
        g.skyZenith[i] = lighting.skyZenith[i];
        g.skyHorizon[i] = lighting.skyHorizon[i];
        g.skyGround[i] = lighting.skyGround[i];
    }
    g.ambient[3] = static_cast<float>(count);
    g.fog[0] = lighting.fogColor[0];
    g.fog[1] = lighting.fogColor[1];
    g.fog[2] = lighting.fogColor[2];
    g.fog[3] = lighting.fogDensity;
    for (uint32_t i = 0; i < count; ++i) {
        const SceneLighting::Point& p = lighting.points[i];
        g.points[i].posRange[0] = p.pos[0];
        g.points[i].posRange[1] = p.pos[1];
        g.points[i].posRange[2] = p.pos[2];
        g.points[i].posRange[3] = p.range;
        g.points[i].color[0] = p.color[0] * p.intensity;
        g.points[i].color[1] = p.color[1] * p.intensity;
        g.points[i].color[2] = p.color[2] * p.intensity;
        if (p.spotOuterDeg > 0.0f) {
            // Spotlight: pack cone cosines (inner angle <= outer angle => cosInner >= cosOuter).
            const float inner = p.spotInnerDeg < p.spotOuterDeg ? p.spotInnerDeg : p.spotOuterDeg;
            const glm::vec3 axis = glm::normalize(
                glm::vec3(p.spotDir[0], p.spotDir[1], p.spotDir[2]));
            g.points[i].color[3] = std::cos(glm::radians(inner));
            g.points[i].spot[0] = axis.x;
            g.points[i].spot[1] = axis.y;
            g.points[i].spot[2] = axis.z;
            g.points[i].spot[3] = std::cos(glm::radians(p.spotOuterDeg));
        } else {
            g.points[i].color[3] = -2.0f; // omnidirectional marker
        }
    }
    // Write only the lighting region; the frame matrices (viewProj/lightVP/camPos) are owned by
    // flush() and must not be clobbered here.
    const size_t off = offsetof(GpuLights, ambient);
    std::memcpy(static_cast<char*>(m_lightMapped) + off,
                reinterpret_cast<const char*>(&g) + off, sizeof(g) - off);
}

bool MeshRenderer::createPipeline(VulkanContext& ctx, VkRenderPass renderPass, VkPolygonMode mode,
                                  VkPipeline& outPipeline, bool instanced, bool transparent) {
    const std::string base = assetBase();
    VkShaderModule vert = createShaderModule(
        ctx.device(),
        readFile(base + (instanced ? "shaders/mesh_instanced.vert.spv" : "shaders/mesh.vert.spv")));
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

    // Binding 0 = per-vertex mesh data; binding 1 (instanced only) = per-instance model matrix.
    VkVertexInputBindingDescription bindings[2]{};
    bindings[0] = meshBinding();
    bindings[1].binding = 1;
    bindings[1].stride = sizeof(float) * 16; // one mat4 per instance
    bindings[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    VkVertexInputAttributeDescription attrs[8]{};
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
    // Instance model matrix as four vec4 rows (locations 4..7) from binding 1.
    for (uint32_t i = 0; i < 4; ++i) {
        attrs[4 + i].location = 4 + i;
        attrs[4 + i].binding = 1;
        attrs[4 + i].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attrs[4 + i].offset = i * sizeof(float) * 4;
    }
    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = instanced ? 2u : 1u;
    vi.pVertexBindingDescriptions = bindings;
    vi.vertexAttributeDescriptionCount = instanced ? 8u : 4u;
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
    rs.polygonMode = mode;
    rs.cullMode = (transparent || mode == VK_POLYGON_MODE_LINE) ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    // Transparent surfaces test against opaque depth but must not write it, so overlapping panes
    // don't occlude each other and the back-to-front blend stays correct.
    ds.depthWriteEnable = transparent ? VK_FALSE : VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;

    // Opaque pipelines overwrite; the transparent pipeline does standard src-alpha over blending.
    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = transparent ? VK_TRUE : VK_FALSE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;
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
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push.offset = 0;
    push.size = sizeof(float) * 24; // model (mat4) + material0 (vec4) + material1 (vec4) = 96 bytes

    // set0 = albedo, set1 = shadow map, set2 = lights UBO, set3 = normal map (albedo layout reused).
    const VkDescriptorSetLayout setLayouts[] = {m_store->layout(), m_shadowSetLayout,
                                                m_lightSetLayout, m_store->layout()};
    VkPipelineLayoutCreateInfo pl{};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount = 4;
    pl.pSetLayouts = setLayouts;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &push;
    // The layout is shared by the fill and wireframe pipelines; create it once.
    if (m_layout == VK_NULL_HANDLE &&
        vkCreatePipelineLayout(ctx.device(), &pl, nullptr, &m_layout) != VK_SUCCESS) {
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
                                           &outPipeline);
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
    mesh.vertexCount = vertexCount;
    computeLocalAabb(vertices, vertexCount, mesh.bmin, mesh.bmax);
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

MeshHandle MeshRenderer::createDynamicMesh(VulkanContext& ctx, const MeshVertex* vertices,
                                           uint32_t vertexCount, const uint32_t* indices,
                                           uint32_t indexCount) {
    Mesh mesh;
    mesh.dynamic = true;
    mesh.indexCount = indexCount;
    mesh.vertexCount = vertexCount;
    computeLocalAabb(vertices, vertexCount, mesh.bmin, mesh.bmax);
    const VkDeviceSize vbytes = static_cast<VkDeviceSize>(vertexCount) * sizeof(MeshVertex);
    const VkDeviceSize ibytes = static_cast<VkDeviceSize>(indexCount) * sizeof(uint32_t);
    const VkMemoryPropertyFlags hostVisible =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    // Indices are static -> one buffer; vertices restream each frame -> one buffer per frame so a
    // write never races a prior frame still reading.
    if (!mesh.ibo.create(ctx, ibytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, hostVisible)) {
        return kInvalidMesh;
    }
    if (void* ip = mesh.ibo.map(ctx)) {
        std::memcpy(ip, indices, static_cast<size_t>(ibytes));
    }
    mesh.dynVbo.resize(m_framesInFlight);
    mesh.dynMapped.resize(m_framesInFlight, nullptr);
    for (uint32_t i = 0; i < m_framesInFlight; ++i) {
        if (!mesh.dynVbo[i].create(ctx, vbytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, hostVisible)) {
            for (auto& b : mesh.dynVbo) b.destroy(ctx);
            mesh.ibo.destroy(ctx);
            return kInvalidMesh;
        }
        mesh.dynMapped[i] = mesh.dynVbo[i].map(ctx);
        if (mesh.dynMapped[i]) {
            std::memcpy(mesh.dynMapped[i], vertices, static_cast<size_t>(vbytes));
        }
    }
    m_meshes.push_back(std::move(mesh));
    return static_cast<MeshHandle>(m_meshes.size() - 1);
}

void MeshRenderer::updateMesh(VulkanContext&, MeshHandle handle, const MeshVertex* vertices,
                              uint32_t vertexCount) {
    if (handle == kInvalidMesh || handle >= m_meshes.size()) {
        return;
    }
    Mesh& mesh = m_meshes[handle];
    if (!mesh.dynamic || vertexCount > mesh.vertexCount) {
        return; // static mesh, or more vertices than the buffer holds
    }
    void* dst = mesh.dynMapped[m_frameIndex];
    if (dst) {
        std::memcpy(dst, vertices,
                    static_cast<size_t>(vertexCount) * sizeof(MeshVertex));
    }
    computeLocalAabb(vertices, vertexCount, mesh.bmin, mesh.bmax); // keep culling AABB in sync
}

void MeshRenderer::setViewProjection(const float* viewProj16) {
    std::memcpy(m_viewProj, viewProj16, sizeof(m_viewProj));
}

void MeshRenderer::begin() {
    m_cmds.clear();
    m_transCmds.clear();
    m_instCmds.clear();
    m_instStaging.clear();
}

bool MeshRenderer::createInstanceBuffers(VulkanContext& ctx) {
    m_maxInstances = 8192;
    const VkDeviceSize bytes = static_cast<VkDeviceSize>(m_maxInstances) * sizeof(float) * 16;
    m_instVbo.resize(m_framesInFlight);
    m_instMapped.resize(m_framesInFlight, nullptr);
    for (uint32_t i = 0; i < m_framesInFlight; ++i) {
        if (!m_instVbo[i].create(ctx, bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            return false;
        }
        m_instMapped[i] = m_instVbo[i].map(ctx);
        if (!m_instMapped[i]) {
            return false;
        }
    }
    return true;
}

void MeshRenderer::drawInstanced(MeshHandle mesh, const float* models16, uint32_t count,
                                 TextureHandle texture, TextureHandle normal,
                                 const float emissive3[3], float roughness, float specular,
                                 float metallic) {
    if (mesh == kInvalidMesh || mesh >= m_meshes.size() || count == 0) {
        return;
    }
    const uint32_t already = static_cast<uint32_t>(m_instStaging.size() / 16);
    if (already + count > m_maxInstances) {
        count = m_maxInstances - already; // clamp to the per-frame instance budget
        if (count == 0) {
            return;
        }
    }
    InstCmd c;
    c.mesh = mesh;
    c.texture = texture;
    c.normal = normal;
    c.first = already;
    c.count = count;
    c.emissive[0] = emissive3 ? emissive3[0] : 0.0f;
    c.emissive[1] = emissive3 ? emissive3[1] : 0.0f;
    c.emissive[2] = emissive3 ? emissive3[2] : 0.0f;
    c.roughness = roughness;
    c.specular = specular;
    c.metallic = metallic;
    m_instCmds.push_back(c);
    m_instStaging.insert(m_instStaging.end(), models16, models16 + static_cast<size_t>(count) * 16);
}

void MeshRenderer::draw(MeshHandle mesh, const float* model16, TextureHandle texture,
                        TextureHandle normal, const float emissive3[3], float roughness,
                        float specular, float metallic) {
    if (mesh == kInvalidMesh || mesh >= m_meshes.size()) {
        return;
    }
    DrawCmd cmd;
    cmd.mesh = mesh;
    cmd.texture = texture;
    cmd.normal = normal;
    std::memcpy(cmd.model, model16, sizeof(cmd.model));
    if (emissive3) {
        cmd.emissive[0] = emissive3[0];
        cmd.emissive[1] = emissive3[1];
        cmd.emissive[2] = emissive3[2];
    }
    cmd.roughness = roughness;
    cmd.specular = specular;
    cmd.metallic = metallic;
    m_cmds.push_back(cmd);
}

void MeshRenderer::drawTransparent(MeshHandle mesh, const float* model16, TextureHandle texture,
                                   TextureHandle normal, const float emissive3[3], float roughness,
                                   float specular, float opacity, float metallic) {
    if (mesh == kInvalidMesh || mesh >= m_meshes.size()) {
        return;
    }
    DrawCmd cmd;
    cmd.mesh = mesh;
    cmd.texture = texture;
    cmd.normal = normal;
    std::memcpy(cmd.model, model16, sizeof(cmd.model));
    if (emissive3) {
        cmd.emissive[0] = emissive3[0];
        cmd.emissive[1] = emissive3[1];
        cmd.emissive[2] = emissive3[2];
    }
    cmd.roughness = roughness;
    cmd.specular = specular;
    cmd.metallic = metallic;
    cmd.alpha = opacity < 0.0f ? 0.0f : (opacity > 1.0f ? 1.0f : opacity);
    m_transCmds.push_back(cmd);
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
        VkBuffer vbuf = mesh.vboFor(m_frameIndex);
        vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &offset);
        vkCmdBindIndexBuffer(cmd, mesh.ibo.handle(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
    }
    vkCmdEndRenderPass(cmd);
}

void MeshRenderer::destroyPrepassResources(VulkanContext& ctx) {
    if (m_prepassFbo) vkDestroyFramebuffer(ctx.device(), m_prepassFbo, nullptr);
    if (m_prepassSampler) vkDestroySampler(ctx.device(), m_prepassSampler, nullptr);
    if (m_prepassView) vkDestroyImageView(ctx.device(), m_prepassView, nullptr);
    if (m_prepassImage) vkDestroyImage(ctx.device(), m_prepassImage, nullptr);
    if (m_prepassMemory) vkFreeMemory(ctx.device(), m_prepassMemory, nullptr);
    m_prepassFbo = VK_NULL_HANDLE;
    m_prepassSampler = VK_NULL_HANDLE;
    m_prepassView = VK_NULL_HANDLE;
    m_prepassImage = VK_NULL_HANDLE;
    m_prepassMemory = VK_NULL_HANDLE;
    m_prepassW = m_prepassH = 0;
}

bool MeshRenderer::ensurePrepassResources(VulkanContext& ctx, uint32_t w, uint32_t h) {
    if (w == 0 || h == 0) {
        return false;
    }
    if (m_prepassImage != VK_NULL_HANDLE && m_prepassW == w && m_prepassH == h) {
        return true; // still the right size
    }
    destroyPrepassResources(ctx);

    VkImageCreateInfo ii{};
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = VK_FORMAT_D32_SFLOAT;
    ii.extent = {w, h, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(ctx.device(), &ii, nullptr, &m_prepassImage) != VK_SUCCESS) {
        return false;
    }
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(ctx.device(), m_prepassImage, &req);
    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex =
        findMemoryType(ctx, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (ai.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(ctx.device(), &ai, nullptr, &m_prepassMemory) != VK_SUCCESS) {
        return false;
    }
    vkBindImageMemory(ctx.device(), m_prepassImage, m_prepassMemory, 0);

    VkImageViewCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = m_prepassImage;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = VK_FORMAT_D32_SFLOAT;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    if (vkCreateImageView(ctx.device(), &vi, nullptr, &m_prepassView) != VK_SUCCESS) {
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
    if (vkCreateSampler(ctx.device(), &si, nullptr, &m_prepassSampler) != VK_SUCCESS) {
        return false;
    }
    // Reuse the shadow depth pass (depth-only D32, ends SHADER_READ_ONLY) for the full-res fbo.
    VkFramebufferCreateInfo fb{};
    fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb.renderPass = m_shadowPass;
    fb.attachmentCount = 1;
    fb.pAttachments = &m_prepassView;
    fb.width = w;
    fb.height = h;
    fb.layers = 1;
    if (vkCreateFramebuffer(ctx.device(), &fb, nullptr, &m_prepassFbo) != VK_SUCCESS) {
        return false;
    }
    m_prepassW = w;
    m_prepassH = h;
    return true;
}

void MeshRenderer::renderDepthPrepass(VulkanContext& ctx, VkCommandBuffer cmd) {
    if (!m_prepassEnabled || m_cmds.empty() || m_shadowPipeline == VK_NULL_HANDLE) {
        return;
    }
    if (!ensurePrepassResources(ctx, m_viewportW, m_viewportH)) {
        return;
    }
    VkClearValue clear{};
    clear.depthStencil = {1.0f, 0};
    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = m_shadowPass;
    rp.framebuffer = m_prepassFbo;
    rp.renderArea.extent = {m_prepassW, m_prepassH};
    rp.clearValueCount = 1;
    rp.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.width = static_cast<float>(m_prepassW);
    viewport.height = static_cast<float>(m_prepassH);
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{};
    scissor.extent = {m_prepassW, m_prepassH};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline);
    const glm::mat4 camVP = glm::make_mat4(m_viewProj);
    for (const DrawCmd& dc : m_cmds) {
        const Mesh& mesh = m_meshes[dc.mesh];
        const glm::mat4 mvp = camVP * glm::make_mat4(dc.model);
        vkCmdPushConstants(cmd, m_shadowLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 16,
                           glm::value_ptr(mvp));
        VkDeviceSize offset = 0;
        VkBuffer vbuf = mesh.vboFor(m_frameIndex);
        vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &offset);
        vkCmdBindIndexBuffer(cmd, mesh.ibo.handle(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
    }
    vkCmdEndRenderPass(cmd);
}

void MeshRenderer::flush(VkCommandBuffer cmd) {
    if ((m_cmds.empty() && m_instCmds.empty() && m_transCmds.empty()) ||
        m_pipeline == VK_NULL_HANDLE) {
        return;
    }
    VkPipeline pipeline = (m_wireframe && m_wireframePipeline) ? m_wireframePipeline : m_pipeline;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

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
    const Frustum frustum = makeFrustum(vp);

    // Per-frame camera/light matrices go into the scene UBO (set 2) once, not into every push.
    if (m_lightMapped) {
        GpuLights* u = static_cast<GpuLights*>(m_lightMapped);
        std::memcpy(u->viewProj, glm::value_ptr(vp), sizeof(u->viewProj));
        std::memcpy(u->lightVP, glm::value_ptr(lightVP), sizeof(u->lightVP));
        u->camPos[0] = m_camPos[0];
        u->camPos[1] = m_camPos[1];
        u->camPos[2] = m_camPos[2];
        u->camPos[3] = 0.0f;
    }
    m_drawnLastFrame = 0;
    m_culledLastFrame = 0;
    for (const DrawCmd& dc : m_cmds) {
        const Mesh& mesh = m_meshes[dc.mesh];
        const glm::mat4 model = glm::make_mat4(dc.model);

        // Frustum culling: skip meshes whose world AABB is entirely outside the camera frustum.
        glm::vec3 wmin, wmax;
        worldAabb(model, mesh.bmin, mesh.bmax, wmin, wmax);
        if (!aabbInFrustum(frustum, wmin, wmax)) {
            ++m_culledLastFrame;
            continue;
        }
        ++m_drawnLastFrame;

        // Per-draw push: model matrix + material (96 bytes, safely under the 128-byte limit).
        float push[24];
        std::memcpy(push, glm::value_ptr(model), sizeof(float) * 16);
        push[16] = dc.emissive[0]; // material0: rgb = emissive (feeds bloom)
        push[17] = dc.emissive[1];
        push[18] = dc.emissive[2];
        push[19] = dc.roughness;   // material0.w = roughness
        push[20] = dc.specular;    // material1.x = specular strength (0 => matte)
        push[21] = 1.0f;           // material1.y = opacity (opaque)
        push[22] = dc.metallic;    // material1.z = metallic (0 => dielectric)
        push[23] = 0.0f;
        vkCmdPushConstants(cmd, m_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(push), push);

        VkDescriptorSet albedo = m_store->descriptorSet(dc.texture);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 0, 1, &albedo, 0,
                                nullptr);
        const TextureHandle nrm = m_store->valid(dc.normal) ? dc.normal : m_defaultNormal;
        VkDescriptorSet normalSet = m_store->descriptorSet(nrm);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 3, 1, &normalSet, 0,
                                nullptr);

        VkDeviceSize offset = 0;
        VkBuffer vbuf = mesh.vboFor(m_frameIndex);
        vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &offset);
        vkCmdBindIndexBuffer(cmd, mesh.ibo.handle(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
    }

    // Instanced draws: upload this frame's instance matrices, then one drawIndexed per InstCmd with
    // instanceCount copies. Uses the instanced pipeline (model matrix from binding 1).
    m_instancesLastFrame = 0;
    if (!m_instCmds.empty() && m_instancedPipeline != VK_NULL_HANDLE && m_instMapped[m_frameIndex]) {
        std::memcpy(m_instMapped[m_frameIndex], m_instStaging.data(),
                    m_instStaging.size() * sizeof(float));
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_instancedPipeline);
        VkBuffer instBuf = m_instVbo[m_frameIndex].handle();
        float idPush[24] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; // identity model (unused)
        for (const InstCmd& ic : m_instCmds) {
            const Mesh& mesh = m_meshes[ic.mesh];
            VkDescriptorSet albedo = m_store->descriptorSet(ic.texture);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 0, 1, &albedo, 0,
                                    nullptr);
            const TextureHandle nrm = m_store->valid(ic.normal) ? ic.normal : m_defaultNormal;
            VkDescriptorSet normalSet = m_store->descriptorSet(nrm);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 3, 1, &normalSet,
                                    0, nullptr);
            idPush[16] = ic.emissive[0];
            idPush[17] = ic.emissive[1];
            idPush[18] = ic.emissive[2];
            idPush[19] = ic.roughness;
            idPush[20] = ic.specular;
            idPush[21] = 1.0f; // opacity (opaque)
            idPush[22] = ic.metallic; // material1.z = metallic
            vkCmdPushConstants(cmd, m_layout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(idPush), idPush);
            VkDeviceSize offsets[2] = {0, 0};
            VkBuffer bufs[2] = {mesh.vboFor(m_frameIndex), instBuf};
            vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offsets);
            vkCmdBindIndexBuffer(cmd, mesh.ibo.handle(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, mesh.indexCount, ic.count, 0, 0, ic.first);
            m_instancesLastFrame += ic.count;
        }
    }

    // Transparent pass: sort back-to-front by camera distance, then draw with alpha blending and
    // no depth-write so overlapping translucent surfaces composite correctly over the opaque scene.
    if (!m_transCmds.empty() && m_transparentPipeline != VK_NULL_HANDLE) {
        const glm::vec3 camPos(m_camPos[0], m_camPos[1], m_camPos[2]);
        std::vector<uint32_t> order(m_transCmds.size());
        for (uint32_t i = 0; i < order.size(); ++i) {
            order[i] = i;
        }
        auto dist2 = [&](uint32_t i) {
            const DrawCmd& dc = m_transCmds[i];
            const glm::vec3 c(dc.model[12], dc.model[13], dc.model[14]); // model translation
            return glm::dot(c - camPos, c - camPos);
        };
        std::sort(order.begin(), order.end(),
                  [&](uint32_t a, uint32_t b) { return dist2(a) > dist2(b); }); // farthest first

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_transparentPipeline);
        for (uint32_t oi : order) {
            const DrawCmd& dc = m_transCmds[oi];
            const Mesh& mesh = m_meshes[dc.mesh];
            const glm::mat4 model = glm::make_mat4(dc.model);

            glm::vec3 wmin, wmax;
            worldAabb(model, mesh.bmin, mesh.bmax, wmin, wmax);
            if (!aabbInFrustum(frustum, wmin, wmax)) {
                continue;
            }

            float push[24];
            std::memcpy(push, glm::value_ptr(model), sizeof(float) * 16);
            push[16] = dc.emissive[0];
            push[17] = dc.emissive[1];
            push[18] = dc.emissive[2];
            push[19] = dc.roughness;
            push[20] = dc.specular;
            push[21] = dc.alpha; // material1.y = opacity
            push[22] = dc.metallic; // material1.z = metallic
            push[23] = 0.0f;
            vkCmdPushConstants(cmd, m_layout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(push), push);

            VkDescriptorSet albedo = m_store->descriptorSet(dc.texture);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 0, 1, &albedo, 0,
                                    nullptr);
            const TextureHandle nrm = m_store->valid(dc.normal) ? dc.normal : m_defaultNormal;
            VkDescriptorSet normalSet = m_store->descriptorSet(nrm);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 3, 1, &normalSet,
                                    0, nullptr);

            VkDeviceSize offset = 0;
            VkBuffer vbuf = mesh.vboFor(m_frameIndex);
            vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &offset);
            vkCmdBindIndexBuffer(cmd, mesh.ibo.handle(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
        }
    }
}

void MeshRenderer::shutdown(VulkanContext& ctx) {
    VkDevice d = ctx.device();
    destroyPrepassResources(ctx);
    for (Mesh& mesh : m_meshes) {
        mesh.vbo.destroy(ctx);
        mesh.ibo.destroy(ctx);
        for (VulkanBuffer& b : mesh.dynVbo) {
            b.destroy(ctx);
        }
    }
    m_meshes.clear();
    for (VulkanBuffer& b : m_instVbo) {
        b.destroy(ctx);
    }
    m_instVbo.clear();
    m_instMapped.clear();

    if (m_pipeline) vkDestroyPipeline(d, m_pipeline, nullptr);
    if (m_instancedPipeline) vkDestroyPipeline(d, m_instancedPipeline, nullptr);
    if (m_transparentPipeline) vkDestroyPipeline(d, m_transparentPipeline, nullptr);
    if (m_wireframePipeline) vkDestroyPipeline(d, m_wireframePipeline, nullptr);
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
    m_instancedPipeline = VK_NULL_HANDLE;
    m_transparentPipeline = VK_NULL_HANDLE;
    m_wireframePipeline = VK_NULL_HANDLE;
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
