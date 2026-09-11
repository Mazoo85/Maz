#include "render/MeshRenderer.hpp"

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

// Push constants handed to the vertex shader each draw. Two mat4 = 128 bytes, the guaranteed
// minimum push-constant size, so this stays within spec on every device.
struct MeshPush {
    math::mat4 mvp;
    math::mat4 model;
};

// Read a whole file into bytes. Returns false if it can't be opened.
bool readFile(const std::string& path, std::vector<char>& out) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    const std::streamsize size = file.tellg();
    if (size <= 0) {
        return false;
    }
    out.resize(static_cast<size_t>(size));
    file.seekg(0);
    file.read(out.data(), size);
    return true;
}

// Resolve a shader path next to the executable: <base>/shaders/<name>.
std::string shaderPath(const char* name) {
    const char* base = SDL_GetBasePath(); // owned by SDL; do not free
    std::string dir = base ? base : "";
    return dir + "shaders/" + name;
}

VkShaderModule loadShader(VkDevice device, const char* name) {
    std::vector<char> code;
    const std::string path = shaderPath(name);
    if (!readFile(path, code)) {
        MAZ_LOG_ERROR("could not read shader '%s'", path.c_str());
        return VK_NULL_HANDLE;
    }
    if (code.size() % 4 != 0) {
        MAZ_LOG_ERROR("shader '%s' size not a multiple of 4 (%zu)", path.c_str(), code.size());
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &ci, nullptr, &module) != VK_SUCCESS) {
        MAZ_LOG_ERROR("vkCreateShaderModule failed for '%s'", path.c_str());
        return VK_NULL_HANDLE;
    }
    return module;
}

// Create a host-visible buffer and fill it with `bytes` bytes from `src`.
bool createFilledBuffer(VulkanContext& ctx, VkBufferUsageFlags usage, const void* src,
                        VkDeviceSize bytes, VkBuffer& outBuffer, VkDeviceMemory& outMemory) {
    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = bytes;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(ctx.device(), &bi, nullptr, &outBuffer) != VK_SUCCESS) {
        MAZ_LOG_ERROR("vkCreateBuffer failed");
        return false;
    }

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(ctx.device(), outBuffer, &req);
    const uint32_t memType = ctx.findMemoryType(
        req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memType == UINT32_MAX) {
        MAZ_LOG_ERROR("no host-visible memory type for buffer");
        return false;
    }

    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = memType;
    if (vkAllocateMemory(ctx.device(), &ai, nullptr, &outMemory) != VK_SUCCESS) {
        MAZ_LOG_ERROR("vkAllocateMemory (buffer) failed");
        return false;
    }
    vkBindBufferMemory(ctx.device(), outBuffer, outMemory, 0);

    void* mapped = nullptr;
    if (vkMapMemory(ctx.device(), outMemory, 0, bytes, 0, &mapped) != VK_SUCCESS) {
        MAZ_LOG_ERROR("vkMapMemory failed");
        return false;
    }
    std::memcpy(mapped, src, static_cast<size_t>(bytes));
    vkUnmapMemory(ctx.device(), outMemory);
    return true;
}

} // namespace

bool MeshRenderer::init(VulkanContext& ctx, VkRenderPass renderPass) {
    VkDevice device = ctx.device();

    VkShaderModule vert = loadShader(device, "mesh.vert.spv");
    VkShaderModule frag = loadShader(device, "mesh.frag.spv");
    if (vert == VK_NULL_HANDLE || frag == VK_NULL_HANDLE) {
        if (vert) vkDestroyShaderModule(device, vert, nullptr);
        if (frag) vkDestroyShaderModule(device, frag, nullptr);
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

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = static_cast<uint32_t>(sizeof(assets::Vertex));
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[3]{};
    attrs[0].location = 0;
    attrs[0].binding = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = static_cast<uint32_t>(offsetof(assets::Vertex, position));
    attrs[1].location = 1;
    attrs[1].binding = 0;
    attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset = static_cast<uint32_t>(offsetof(assets::Vertex, normal));
    attrs[2].location = 2;
    attrs[2].binding = 0;
    attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[2].offset = static_cast<uint32_t>(offsetof(assets::Vertex, uv));

    VkPipelineVertexInputStateCreateInfo vin{};
    vin.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vin.vertexBindingDescriptionCount = 1;
    vin.pVertexBindingDescriptions = &binding;
    vin.vertexAttributeDescriptionCount = 3;
    vin.pVertexAttributeDescriptions = attrs;

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
    rs.cullMode = VK_CULL_MODE_BACK_BIT; // cull back faces; geometry is CCW-outward (glTF spec winding)
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

    VkPipelineColorBlendAttachmentState blendAttach{};
    blendAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttach.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &blendAttach;

    VkDynamicState dynamics[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynamics;

    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push.offset = 0;
    push.size = static_cast<uint32_t>(sizeof(MeshPush));

    VkPipelineLayoutCreateInfo layout{};
    layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout.pushConstantRangeCount = 1;
    layout.pPushConstantRanges = &push;
    if (vkCreatePipelineLayout(device, &layout, nullptr, &m_layout) != VK_SUCCESS) {
        MAZ_LOG_ERROR("vkCreatePipelineLayout failed");
        vkDestroyShaderModule(device, vert, nullptr);
        vkDestroyShaderModule(device, frag, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipe{};
    pipe.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipe.stageCount = 2;
    pipe.pStages = stages;
    pipe.pVertexInputState = &vin;
    pipe.pInputAssemblyState = &ia;
    pipe.pViewportState = &vp;
    pipe.pRasterizationState = &rs;
    pipe.pMultisampleState = &ms;
    pipe.pDepthStencilState = &ds;
    pipe.pColorBlendState = &cb;
    pipe.pDynamicState = &dyn;
    pipe.layout = m_layout;
    pipe.renderPass = renderPass;
    pipe.subpass = 0;

    const VkResult r =
        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipe, nullptr, &m_pipeline);
    vkDestroyShaderModule(device, vert, nullptr);
    vkDestroyShaderModule(device, frag, nullptr);
    if (r != VK_SUCCESS) {
        MAZ_LOG_ERROR("vkCreateGraphicsPipelines failed (VkResult %d)", static_cast<int>(r));
        return false;
    }

    MAZ_LOG_INFO("mesh pipeline created");
    return true;
}

int MeshRenderer::uploadModel(VulkanContext& ctx, const assets::Model& model) {
    GpuModel gpuModel;
    for (const assets::Mesh& mesh : model.meshes) {
        if (mesh.vertices.empty() || mesh.indices.empty()) {
            continue;
        }
        GpuMesh gpu;
        const VkDeviceSize vbytes = sizeof(assets::Vertex) * mesh.vertices.size();
        const VkDeviceSize ibytes = sizeof(uint32_t) * mesh.indices.size();
        if (!createFilledBuffer(ctx, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mesh.vertices.data(), vbytes,
                                gpu.vertexBuffer, gpu.vertexMemory) ||
            !createFilledBuffer(ctx, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, mesh.indices.data(), ibytes,
                                gpu.indexBuffer, gpu.indexMemory)) {
            return -1;
        }
        gpu.indexCount = static_cast<uint32_t>(mesh.indices.size());
        gpuModel.push_back(gpu);
    }
    if (gpuModel.empty()) {
        return -1;
    }
    m_models.push_back(std::move(gpuModel));
    const int handle = static_cast<int>(m_models.size()) - 1;
    MAZ_LOG_INFO("mesh renderer uploaded model handle %d (%zu mesh(es))", handle,
                 m_models[static_cast<size_t>(handle)].size());
    return handle;
}

void MeshRenderer::draw(VkCommandBuffer cmd, int handle, const math::mat4& mvp,
                        const math::mat4& model, VkExtent2D extent) const {
    if (m_pipeline == VK_NULL_HANDLE || handle < 0 ||
        static_cast<size_t>(handle) >= m_models.size()) {
        return;
    }

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    MeshPush push{};
    push.mvp = mvp;
    push.model = model;
    vkCmdPushConstants(cmd, m_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPush), &push);

    for (const GpuMesh& gpu : m_models[static_cast<size_t>(handle)]) {
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &gpu.vertexBuffer, &offset);
        vkCmdBindIndexBuffer(cmd, gpu.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, gpu.indexCount, 1, 0, 0, 0);
    }
}

void MeshRenderer::destroyModels(VulkanContext& ctx) {
    VkDevice device = ctx.device();
    for (GpuModel& gpuModel : m_models) {
        for (GpuMesh& gpu : gpuModel) {
            if (gpu.vertexBuffer) vkDestroyBuffer(device, gpu.vertexBuffer, nullptr);
            if (gpu.vertexMemory) vkFreeMemory(device, gpu.vertexMemory, nullptr);
            if (gpu.indexBuffer) vkDestroyBuffer(device, gpu.indexBuffer, nullptr);
            if (gpu.indexMemory) vkFreeMemory(device, gpu.indexMemory, nullptr);
        }
    }
    m_models.clear();
}

void MeshRenderer::destroy(VulkanContext& ctx) {
    destroyModels(ctx);
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
