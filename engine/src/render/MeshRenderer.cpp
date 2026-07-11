#include "render/MeshRenderer.hpp"

#include "maz/core/Log.hpp"
#include "render/TextureStore.hpp"
#include "render/VulkanContext.hpp"

#include <SDL3/SDL_filesystem.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstddef>
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
        MAZ_LOG_ERROR("mesh vkCreateShaderModule failed");
    }
    return module;
}

std::string assetBase() {
    const char* base = SDL_GetBasePath();
    return base ? std::string(base) : std::string();
}

} // namespace

bool MeshRenderer::init(VulkanContext& ctx, TextureStore& store, VkRenderPass renderPass) {
    m_store = &store;
    m_meshes.emplace_back(); // reserve index 0 == kInvalidMesh
    return createPipeline(ctx, renderPass);
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

    VkVertexInputBindingDescription vb{};
    vb.binding = 0;
    vb.stride = sizeof(MeshVertex);
    vb.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

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
    rs.cullMode = VK_CULL_MODE_NONE; // depth test gives a correct solid; avoids winding pitfalls
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
    push.size = sizeof(float) * 32; // mvp (16) + model (16)

    VkDescriptorSetLayout setLayout = m_store->layout();
    VkPipelineLayoutCreateInfo pl{};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &setLayout;
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
    gp.subpass = 0;

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

    const glm::mat4 vp = glm::make_mat4(m_viewProj);
    for (const DrawCmd& dc : m_cmds) {
        const Mesh& mesh = m_meshes[dc.mesh];
        const glm::mat4 model = glm::make_mat4(dc.model);
        const glm::mat4 mvp = vp * model;

        float push[32];
        std::memcpy(push, glm::value_ptr(mvp), sizeof(float) * 16);
        std::memcpy(push + 16, glm::value_ptr(model), sizeof(float) * 16);
        vkCmdPushConstants(cmd, m_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), push);

        VkDescriptorSet set = m_store->descriptorSet(dc.texture);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 0, 1, &set, 0,
                                nullptr);

        VkDeviceSize offset = 0;
        VkBuffer vbuf = mesh.vbo.handle();
        vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &offset);
        vkCmdBindIndexBuffer(cmd, mesh.ibo.handle(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
    }
}

void MeshRenderer::shutdown(VulkanContext& ctx) {
    for (Mesh& mesh : m_meshes) {
        mesh.vbo.destroy(ctx);
        mesh.ibo.destroy(ctx);
    }
    m_meshes.clear();
    if (m_pipeline) {
        vkDestroyPipeline(ctx.device(), m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_layout) {
        vkDestroyPipelineLayout(ctx.device(), m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;
    }
}

} // namespace maz::render
