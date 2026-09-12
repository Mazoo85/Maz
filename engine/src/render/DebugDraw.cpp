#include "render/DebugDraw.hpp"

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

constexpr uint32_t kMaxLines = 16384;
constexpr uint32_t kVertsPerLine = 2;

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
        MAZ_LOG_ERROR("debug-line vkCreateShaderModule failed");
    }
    return module;
}

std::string assetBase() {
    const char* base = SDL_GetBasePath();
    return base ? std::string(base) : std::string();
}

} // namespace

bool DebugDraw::init(VulkanContext& ctx, VkRenderPass renderPass, uint32_t framesInFlight,
                     VkSampleCountFlagBits samples) {
    m_samples = samples;
    m_maxVertices = kMaxLines * kVertsPerLine;
    m_verts.reserve(256);

    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push.offset = 0;
    push.size = sizeof(float) * 16; // viewProj
    VkPipelineLayoutCreateInfo pl{};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &push;
    if (vkCreatePipelineLayout(ctx.device(), &pl, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        MAZ_LOG_ERROR("debug-line pipeline layout failed");
        return false;
    }

    return createPipeline(ctx, renderPass) && createVertexBuffers(ctx, framesInFlight);
}

bool DebugDraw::createPipeline(VulkanContext& ctx, VkRenderPass renderPass) {
    const std::string base = assetBase();
    VkShaderModule vert =
        createShaderModule(ctx.device(), readFile(base + "shaders/debugline.vert.spv"));
    VkShaderModule frag =
        createShaderModule(ctx.device(), readFile(base + "shaders/debugline.frag.spv"));
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
    VkVertexInputAttributeDescription attrs[2]{};
    attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = offsetof(Vertex, pos);
    attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrs[1].offset = offsetof(Vertex, color);
    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &vb;
    vi.vertexAttributeDescriptionCount = 2;
    vi.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

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

    // Alpha-blended so translucent debug overlays read cleanly over the scene.
    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_TRUE;
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

    // Depth-test against the scene (geometry occludes lines) but don't write depth.
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
        vkCreateGraphicsPipelines(ctx.device(), VK_NULL_HANDLE, 1, &gp, nullptr, &m_pipeline);
    vkDestroyShaderModule(ctx.device(), vert, nullptr);
    vkDestroyShaderModule(ctx.device(), frag, nullptr);
    if (r != VK_SUCCESS) {
        MAZ_LOG_ERROR("debug-line vkCreateGraphicsPipelines failed (VkResult %d)", (int)r);
        return false;
    }
    return true;
}

bool DebugDraw::createVertexBuffers(VulkanContext& ctx, uint32_t framesInFlight) {
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

void DebugDraw::begin() { m_verts.clear(); }

void DebugDraw::line(const float a[3], const float b[3], const float color[4]) {
    if (m_verts.size() + kVertsPerLine > m_maxVertices) {
        if (!m_capacityWarned) {
            MAZ_LOG_WARN("debug-line buffer full (%u lines/frame); dropping extras", kMaxLines);
            m_capacityWarned = true;
        }
        return;
    }
    Vertex va;
    va.pos[0] = a[0];
    va.pos[1] = a[1];
    va.pos[2] = a[2];
    std::memcpy(va.color, color, sizeof(va.color));
    Vertex vb = va;
    vb.pos[0] = b[0];
    vb.pos[1] = b[1];
    vb.pos[2] = b[2];
    m_verts.push_back(va);
    m_verts.push_back(vb);
}

void DebugDraw::aabb(const float min[3], const float max[3], const float color[4]) {
    // Eight corners of the box.
    const float xs[2] = {min[0], max[0]};
    const float ys[2] = {min[1], max[1]};
    const float zs[2] = {min[2], max[2]};
    auto corner = [&](int i, float out[3]) {
        out[0] = xs[i & 1];
        out[1] = ys[(i >> 1) & 1];
        out[2] = zs[(i >> 2) & 1];
    };
    // 12 edges as pairs of corner indices (bit 0=x, 1=y, 2=z).
    static const int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7},  // x-parallel
                                     {0, 2}, {1, 3}, {4, 6}, {5, 7},  // y-parallel
                                     {0, 4}, {1, 5}, {2, 6}, {3, 7}}; // z-parallel
    for (const auto& e : edges) {
        float a[3];
        float b[3];
        corner(e[0], a);
        corner(e[1], b);
        line(a, b, color);
    }
}

void DebugDraw::flush(VkCommandBuffer cmd, uint32_t frameIndex, const float viewProj16[16]) {
    const uint32_t count = static_cast<uint32_t>(m_verts.size());
    if (count == 0 || m_pipeline == VK_NULL_HANDLE) {
        return;
    }
    std::memcpy(m_vboMapped[frameIndex], m_verts.data(), count * sizeof(Vertex));

    VkViewport viewport{};
    viewport.width = static_cast<float>(m_viewportW);
    viewport.height = static_cast<float>(m_viewportH);
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{};
    scissor.extent = {m_viewportW, m_viewportH};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 16,
                       viewProj16);
    VkDeviceSize offset = 0;
    VkBuffer buf = m_vbo[frameIndex].handle();
    vkCmdBindVertexBuffers(cmd, 0, 1, &buf, &offset);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    vkCmdDraw(cmd, count, 1, 0, 0);
}

void DebugDraw::shutdown(VulkanContext& ctx) {
    for (auto& b : m_vbo) {
        b.destroy(ctx);
    }
    m_vbo.clear();
    m_vboMapped.clear();
    if (m_pipeline) {
        vkDestroyPipeline(ctx.device(), m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout) {
        vkDestroyPipelineLayout(ctx.device(), m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
}

} // namespace maz::render
