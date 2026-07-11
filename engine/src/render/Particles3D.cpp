#include "render/Particles3D.hpp"

#include "maz/core/Log.hpp"
#include "render/VulkanContext.hpp"

#include <SDL3/SDL_filesystem.h>

#include <cstddef>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace maz::render {

namespace {

constexpr uint32_t kMaxParticles = 4096;
constexpr uint32_t kVertsPerParticle = 6;

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
        MAZ_LOG_ERROR("particle vkCreateShaderModule failed");
    }
    return module;
}

std::string assetBase() {
    const char* base = SDL_GetBasePath();
    return base ? std::string(base) : std::string();
}

struct PushData {
    float viewProj[16];
    float right[4];
    float up[4];
};

} // namespace

bool Particles3D::init(VulkanContext& ctx, VkRenderPass renderPass, uint32_t framesInFlight,
                       VkSampleCountFlagBits samples) {
    m_samples = samples;
    m_maxVertices = kMaxParticles * kVertsPerParticle;
    m_additive.reserve(m_maxVertices);

    // Shared pipeline layout (push constant only): viewProj + camera basis.
    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push.offset = 0;
    push.size = sizeof(PushData);
    VkPipelineLayoutCreateInfo pl{};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &push;
    if (vkCreatePipelineLayout(ctx.device(), &pl, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        MAZ_LOG_ERROR("particle pipeline layout failed");
        return false;
    }

    return createPipeline(ctx, renderPass, true, m_pipelineAdd) &&
           createPipeline(ctx, renderPass, false, m_pipelineAlpha) &&
           createVertexBuffers(ctx, framesInFlight);
}

bool Particles3D::createPipeline(VulkanContext& ctx, VkRenderPass renderPass, bool additive,
                                 VkPipeline& outPipeline) {
    const std::string base = assetBase();
    VkShaderModule vert =
        createShaderModule(ctx.device(), readFile(base + "shaders/particle.vert.spv"));
    VkShaderModule frag =
        createShaderModule(ctx.device(), readFile(base + "shaders/particle.frag.spv"));
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

    VkVertexInputBindingDescription vb{};
    vb.binding = 0;
    vb.stride = sizeof(Vertex);
    vb.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attrs[4]{};
    attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = offsetof(Vertex, center);
    attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[1].offset = offsetof(Vertex, corner);
    attrs[2].location = 2;
    attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[2].offset = offsetof(Vertex, uv);
    attrs[3].location = 3;
    attrs[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrs[3].offset = offsetof(Vertex, color);
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
    ms.rasterizationSamples = m_samples;

    // additive: overlapping particles build up a glow (embers). alpha: standard over-blend (smoke).
    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = additive ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = additive ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &blend;

    // Depth-test against the scene (so geometry occludes particles) but don't write depth.
    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_FALSE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

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
    gp.pDepthStencilState = &ds;
    gp.pColorBlendState = &cb;
    gp.pDynamicState = &dyn;
    gp.layout = m_pipelineLayout;
    gp.renderPass = renderPass;
    const VkResult r =
        vkCreateGraphicsPipelines(ctx.device(), VK_NULL_HANDLE, 1, &gp, nullptr, &outPipeline);
    vkDestroyShaderModule(ctx.device(), vert, nullptr);
    vkDestroyShaderModule(ctx.device(), frag, nullptr);
    if (r != VK_SUCCESS) {
        MAZ_LOG_ERROR("particle vkCreateGraphicsPipelines failed (VkResult %d)", (int)r);
        return false;
    }
    return true;
}

bool Particles3D::createVertexBuffers(VulkanContext& ctx, uint32_t framesInFlight) {
    const VkDeviceSize bytes = static_cast<VkDeviceSize>(m_maxVertices) * sizeof(Vertex);
    m_vbo.resize(framesInFlight);
    m_vboMapped.resize(framesInFlight, nullptr);
    for (uint32_t i = 0; i < framesInFlight; ++i) {
        if (!m_vbo[i].create(ctx, bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            return false;
        }
        m_vboMapped[i] = m_vbo[i].map(ctx);
        if (!m_vboMapped[i]) {
            return false;
        }
    }
    return true;
}

void Particles3D::begin() {
    m_additive.clear();
    m_alpha.clear();
}

void Particles3D::draw(const float pos[3], float size, const float color[4], bool additive) {
    std::vector<Vertex>& list = additive ? m_additive : m_alpha;
    if (m_additive.size() + m_alpha.size() + kVertsPerParticle > m_maxVertices) {
        if (!m_capacityWarned) {
            MAZ_LOG_WARN("particle buffer full (%u/frame); dropping extras", kMaxParticles);
            m_capacityWarned = true;
        }
        return;
    }
    const float h = size * 0.5f;
    // Unit quad corners (two triangles) and their UVs.
    const float cx[6] = {-h, h, h, -h, h, -h};
    const float cy[6] = {-h, -h, h, -h, h, h};
    const float ux[6] = {0, 1, 1, 0, 1, 0};
    const float uy[6] = {0, 0, 1, 0, 1, 1};
    for (int i = 0; i < 6; ++i) {
        Vertex v;
        v.center[0] = pos[0];
        v.center[1] = pos[1];
        v.center[2] = pos[2];
        v.corner[0] = cx[i];
        v.corner[1] = cy[i];
        v.uv[0] = ux[i];
        v.uv[1] = uy[i];
        v.color[0] = color[0];
        v.color[1] = color[1];
        v.color[2] = color[2];
        v.color[3] = color[3];
        list.push_back(v);
    }
}

void Particles3D::flush(VkCommandBuffer cmd, uint32_t frameIndex, const float viewProj16[16],
                        const float right3[3], const float up3[3]) {
    const uint32_t addCount = static_cast<uint32_t>(m_additive.size());
    const uint32_t alphaCount = static_cast<uint32_t>(m_alpha.size());
    if ((addCount == 0 && alphaCount == 0) || m_pipelineAdd == VK_NULL_HANDLE) {
        return;
    }
    // Pack both lists contiguously: [additive | alpha].
    auto* dst = static_cast<Vertex*>(m_vboMapped[frameIndex]);
    if (addCount) {
        std::memcpy(dst, m_additive.data(), addCount * sizeof(Vertex));
    }
    if (alphaCount) {
        std::memcpy(dst + addCount, m_alpha.data(), alphaCount * sizeof(Vertex));
    }

    VkViewport viewport{};
    viewport.width = static_cast<float>(m_viewportW);
    viewport.height = static_cast<float>(m_viewportH);
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{};
    scissor.extent = {m_viewportW, m_viewportH};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    PushData pc{};
    std::memcpy(pc.viewProj, viewProj16, sizeof(pc.viewProj));
    pc.right[0] = right3[0];
    pc.right[1] = right3[1];
    pc.right[2] = right3[2];
    pc.up[0] = up3[0];
    pc.up[1] = up3[1];
    pc.up[2] = up3[2];
    vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

    VkDeviceSize offset = 0;
    VkBuffer buf = m_vbo[frameIndex].handle();
    vkCmdBindVertexBuffers(cmd, 0, 1, &buf, &offset);

    // Additive first (embers), then alpha (smoke) over them.
    if (addCount) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineAdd);
        vkCmdDraw(cmd, addCount, 1, 0, 0);
    }
    if (alphaCount) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineAlpha);
        vkCmdDraw(cmd, alphaCount, 1, addCount, 0);
    }
}

void Particles3D::shutdown(VulkanContext& ctx) {
    for (auto& b : m_vbo) {
        b.destroy(ctx);
    }
    m_vbo.clear();
    m_vboMapped.clear();
    if (m_pipelineAdd) {
        vkDestroyPipeline(ctx.device(), m_pipelineAdd, nullptr);
        m_pipelineAdd = VK_NULL_HANDLE;
    }
    if (m_pipelineAlpha) {
        vkDestroyPipeline(ctx.device(), m_pipelineAlpha, nullptr);
        m_pipelineAlpha = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout) {
        vkDestroyPipelineLayout(ctx.device(), m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
}

} // namespace maz::render
