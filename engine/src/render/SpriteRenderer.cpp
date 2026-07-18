#include "render/SpriteRenderer.hpp"

#include "maz/core/Log.hpp"
#include "render/VulkanContext.hpp"

#include <SDL3/SDL_filesystem.h>

#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace maz::render {

namespace {

// Upper bound on distinct textures (one descriptor set each). Plenty for a 2D game's atlases and
// tiles; createTexture() logs and fails past this rather than corrupting the pool.
constexpr uint32_t kMaxTextures = 256;

// Read a whole file into bytes. Returns false if it can't be opened or is empty.
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

} // namespace

bool SpriteRenderer::init(VulkanContext& ctx, VkRenderPass renderPass, uint32_t framesInFlight) {
    VkDevice device = ctx.device();
    if (framesInFlight == 0) {
        framesInFlight = 1;
    }
    m_frames.assign(framesInFlight, FrameBuffers{});

    // Combined image sampler at binding 0, visible to the fragment stage.
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo sl{};
    sl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    sl.bindingCount = 1;
    sl.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(device, &sl, nullptr, &m_setLayout) != VK_SUCCESS) {
        MAZ_LOG_ERROR("sprite: vkCreateDescriptorSetLayout failed");
        return false;
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = kMaxTextures;
    VkDescriptorPoolCreateInfo dp{};
    dp.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dp.maxSets = kMaxTextures;
    dp.poolSizeCount = 1;
    dp.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(device, &dp, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        MAZ_LOG_ERROR("sprite: vkCreateDescriptorPool failed");
        return false;
    }

    VkSamplerCreateInfo sampler{};
    sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.magFilter = VK_FILTER_NEAREST; // crisp pixel art — the SEGA / neon look
    sampler.minFilter = VK_FILTER_NEAREST;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    if (vkCreateSampler(device, &sampler, nullptr, &m_sampler) != VK_SUCCESS) {
        MAZ_LOG_ERROR("sprite: vkCreateSampler failed");
        return false;
    }

    // Transient pool + fence for one-time texture-upload command buffers.
    VkCommandPoolCreateInfo cp{};
    cp.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cp.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cp.queueFamilyIndex = ctx.graphicsFamily();
    if (vkCreateCommandPool(device, &cp, nullptr, &m_uploadPool) != VK_SUCCESS) {
        MAZ_LOG_ERROR("sprite: upload command pool creation failed");
        return false;
    }
    VkFenceCreateInfo fc{};
    fc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (vkCreateFence(device, &fc, nullptr, &m_uploadFence) != VK_SUCCESS) {
        MAZ_LOG_ERROR("sprite: upload fence creation failed");
        return false;
    }

    if (!createPipeline(ctx, renderPass)) {
        return false;
    }

    MAZ_LOG_INFO("sprite pipeline created (%u frame buffer set(s))", framesInFlight);
    return true;
}

bool SpriteRenderer::createPipeline(VulkanContext& ctx, VkRenderPass renderPass) {
    VkDevice device = ctx.device();

    VkShaderModule vert = loadShader(device, "sprite.vert.spv");
    VkShaderModule frag = loadShader(device, "sprite.frag.spv");
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

    VkVertexInputBindingDescription bind{};
    bind.binding = 0;
    bind.stride = static_cast<uint32_t>(sizeof(Vertex));
    bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[3]{};
    attrs[0].location = 0;
    attrs[0].binding = 0;
    attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[0].offset = static_cast<uint32_t>(offsetof(Vertex, pos));
    attrs[1].location = 1;
    attrs[1].binding = 0;
    attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[1].offset = static_cast<uint32_t>(offsetof(Vertex, uv));
    attrs[2].location = 2;
    attrs[2].binding = 0;
    attrs[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrs[2].offset = static_cast<uint32_t>(offsetof(Vertex, color));

    VkPipelineVertexInputStateCreateInfo vin{};
    vin.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vin.vertexBindingDescriptionCount = 1;
    vin.pVertexBindingDescriptions = &bind;
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
    rs.cullMode = VK_CULL_MODE_NONE; // 2D quads: winding is irrelevant
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Sprites are painter-ordered overlays: no depth test, no depth write. The render pass may
    // still carry a depth attachment (the swapchain's) — the pipeline simply ignores it.
    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;
    ds.depthCompareOp = VK_COMPARE_OP_ALWAYS;

    // Standard straight-alpha blending.
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

    VkDynamicState dynamics[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynamics;

    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push.offset = 0;
    push.size = static_cast<uint32_t>(sizeof(math::mat4));

    VkPipelineLayoutCreateInfo layout{};
    layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout.setLayoutCount = 1;
    layout.pSetLayouts = &m_setLayout;
    layout.pushConstantRangeCount = 1;
    layout.pPushConstantRanges = &push;
    if (vkCreatePipelineLayout(device, &layout, nullptr, &m_layout) != VK_SUCCESS) {
        MAZ_LOG_ERROR("sprite: vkCreatePipelineLayout failed");
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
        MAZ_LOG_ERROR("sprite: vkCreateGraphicsPipelines failed (VkResult %d)", static_cast<int>(r));
        return false;
    }
    return true;
}

int SpriteRenderer::createTexture(VulkanContext& ctx, const uint8_t* rgba, uint32_t width,
                                  uint32_t height) {
    if (m_pipeline == VK_NULL_HANDLE || rgba == nullptr || width == 0 || height == 0) {
        return -1;
    }
    if (m_textures.size() >= kMaxTextures) {
        MAZ_LOG_ERROR("sprite: texture limit (%u) reached", kMaxTextures);
        return -1;
    }
    VkDevice device = ctx.device();
    const VkDeviceSize bytes = static_cast<VkDeviceSize>(width) * height * 4;

    // --- staging buffer (host visible) ---
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    {
        VkBufferCreateInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size = bytes;
        bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device, &bi, nullptr, &staging) != VK_SUCCESS) {
            MAZ_LOG_ERROR("sprite: staging buffer creation failed");
            return -1;
        }
        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device, staging, &req);
        const uint32_t type = ctx.findMemoryType(
            req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (type == UINT32_MAX) {
            vkDestroyBuffer(device, staging, nullptr);
            return -1;
        }
        VkMemoryAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = type;
        if (vkAllocateMemory(device, &ai, nullptr, &stagingMem) != VK_SUCCESS) {
            vkDestroyBuffer(device, staging, nullptr);
            return -1;
        }
        vkBindBufferMemory(device, staging, stagingMem, 0);
        void* mapped = nullptr;
        vkMapMemory(device, stagingMem, 0, bytes, 0, &mapped);
        std::memcpy(mapped, rgba, static_cast<size_t>(bytes));
        vkUnmapMemory(device, stagingMem);
    }

    // --- device-local image ---
    Texture tex;
    {
        VkImageCreateInfo img{};
        img.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        img.imageType = VK_IMAGE_TYPE_2D;
        img.format = VK_FORMAT_R8G8B8A8_UNORM; // sampled value == source byte (no gamma)
        img.extent = {width, height, 1};
        img.mipLevels = 1;
        img.arrayLayers = 1;
        img.samples = VK_SAMPLE_COUNT_1_BIT;
        img.tiling = VK_IMAGE_TILING_OPTIMAL;
        img.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        img.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vkCreateImage(device, &img, nullptr, &tex.image) != VK_SUCCESS) {
            MAZ_LOG_ERROR("sprite: vkCreateImage failed");
            vkDestroyBuffer(device, staging, nullptr);
            vkFreeMemory(device, stagingMem, nullptr);
            return -1;
        }
        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(device, tex.image, &req);
        const uint32_t type =
            ctx.findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VkMemoryAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = type;
        if (type == UINT32_MAX ||
            vkAllocateMemory(device, &ai, nullptr, &tex.memory) != VK_SUCCESS) {
            MAZ_LOG_ERROR("sprite: image memory allocation failed");
            vkDestroyImage(device, tex.image, nullptr);
            vkDestroyBuffer(device, staging, nullptr);
            vkFreeMemory(device, stagingMem, nullptr);
            return -1;
        }
        vkBindImageMemory(device, tex.image, tex.memory, 0);
    }

    // --- one-time upload: transition, copy, transition to shader-read ---
    VkCommandBufferAllocateInfo cba{};
    cba.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cba.commandPool = m_uploadPool;
    cba.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cba.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device, &cba, &cmd);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);

    VkImageMemoryBarrier toDst{};
    toDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toDst.image = tex.image;
    toDst.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toDst.subresourceRange.levelCount = 1;
    toDst.subresourceRange.layerCount = 1;
    toDst.srcAccessMask = 0;
    toDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &toDst);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(cmd, staging, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &region);

    VkImageMemoryBarrier toRead = toDst;
    toRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &toRead);

    vkEndCommandBuffer(cmd);

    vkResetFences(device, 1, &m_uploadFence);
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(ctx.graphicsQueue(), 1, &submit, m_uploadFence);
    vkWaitForFences(device, 1, &m_uploadFence, VK_TRUE, UINT64_MAX);
    vkFreeCommandBuffers(device, m_uploadPool, 1, &cmd);
    vkDestroyBuffer(device, staging, nullptr);
    vkFreeMemory(device, stagingMem, nullptr);

    // --- view + descriptor set ---
    VkImageViewCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = tex.image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = VK_FORMAT_R8G8B8A8_UNORM;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    if (vkCreateImageView(device, &vi, nullptr, &tex.view) != VK_SUCCESS) {
        MAZ_LOG_ERROR("sprite: vkCreateImageView failed");
        vkDestroyImage(device, tex.image, nullptr);
        vkFreeMemory(device, tex.memory, nullptr);
        return -1;
    }

    VkDescriptorSetAllocateInfo dsa{};
    dsa.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsa.descriptorPool = m_descriptorPool;
    dsa.descriptorSetCount = 1;
    dsa.pSetLayouts = &m_setLayout;
    if (vkAllocateDescriptorSets(device, &dsa, &tex.set) != VK_SUCCESS) {
        MAZ_LOG_ERROR("sprite: vkAllocateDescriptorSets failed");
        vkDestroyImageView(device, tex.view, nullptr);
        vkDestroyImage(device, tex.image, nullptr);
        vkFreeMemory(device, tex.memory, nullptr);
        return -1;
    }

    VkDescriptorImageInfo info{};
    info.sampler = m_sampler;
    info.imageView = tex.view;
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = tex.set;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &info;
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    m_textures.push_back(tex);
    const int handle = static_cast<int>(m_textures.size()) - 1;
    MAZ_LOG_INFO("sprite renderer uploaded texture handle %d (%ux%u)", handle, width, height);
    return handle;
}

void SpriteRenderer::begin(uint32_t frameIndex, VkExtent2D screen) {
    m_frameIndex = m_frames.empty() ? 0 : (frameIndex % static_cast<uint32_t>(m_frames.size()));
    m_screen = screen;
    m_vertices.clear();
    m_indices.clear();
    m_batches.clear();
}

void SpriteRenderer::draw(int texture, const Sprite& s) {
    if (texture < 0 || static_cast<size_t>(texture) >= m_textures.size()) {
        return;
    }

    // Four corners in screen space, rotated about the sprite centre.
    const float cx = s.x + s.w * 0.5f;
    const float cy = s.y + s.h * 0.5f;
    const float co = std::cos(s.rotation);
    const float si = std::sin(s.rotation);
    auto corner = [&](float ox, float oy) -> std::array<float, 2> {
        const float rx = ox * co - oy * si;
        const float ry = ox * si + oy * co;
        return {cx + rx, cy + ry};
    };
    const float hw = s.w * 0.5f;
    const float hh = s.h * 0.5f;
    const std::array<float, 2> p0 = corner(-hw, -hh); // top-left
    const std::array<float, 2> p1 = corner(hw, -hh);  // top-right
    const std::array<float, 2> p2 = corner(hw, hh);   // bottom-right
    const std::array<float, 2> p3 = corner(-hw, hh);  // bottom-left

    const float col[4] = {s.tint.r, s.tint.g, s.tint.b, s.tint.a};
    const auto base = static_cast<uint32_t>(m_vertices.size());
    m_vertices.push_back({{p0[0], p0[1]}, {s.u0, s.v0}, {col[0], col[1], col[2], col[3]}});
    m_vertices.push_back({{p1[0], p1[1]}, {s.u1, s.v0}, {col[0], col[1], col[2], col[3]}});
    m_vertices.push_back({{p2[0], p2[1]}, {s.u1, s.v1}, {col[0], col[1], col[2], col[3]}});
    m_vertices.push_back({{p3[0], p3[1]}, {s.u0, s.v1}, {col[0], col[1], col[2], col[3]}});

    const auto firstIndex = static_cast<uint32_t>(m_indices.size());
    m_indices.push_back(base + 0);
    m_indices.push_back(base + 1);
    m_indices.push_back(base + 2);
    m_indices.push_back(base + 0);
    m_indices.push_back(base + 2);
    m_indices.push_back(base + 3);

    // Extend the current run if it uses the same texture, else start a new batch.
    if (!m_batches.empty() && m_batches.back().texture == texture) {
        m_batches.back().indexCount += 6;
    } else {
        m_batches.push_back({texture, firstIndex, 6});
    }
}

bool SpriteRenderer::ensureCapacity(VulkanContext& ctx, FrameBuffers& fb, VkDeviceSize vbytes,
                                    VkDeviceSize ibytes) {
    VkDevice device = ctx.device();
    auto make = [&](VkBufferUsageFlags usage, VkDeviceSize size, VkBuffer& buf,
                    VkDeviceMemory& mem) -> bool {
        VkBufferCreateInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size = size;
        bi.usage = usage;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device, &bi, nullptr, &buf) != VK_SUCCESS) {
            return false;
        }
        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device, buf, &req);
        const uint32_t type = ctx.findMemoryType(
            req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (type == UINT32_MAX) {
            return false;
        }
        VkMemoryAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = type;
        if (vkAllocateMemory(device, &ai, nullptr, &mem) != VK_SUCCESS) {
            return false;
        }
        vkBindBufferMemory(device, buf, mem, 0);
        return true;
    };

    if (fb.vertexCapacity < vbytes) {
        if (fb.vertexBuffer) vkDestroyBuffer(device, fb.vertexBuffer, nullptr);
        if (fb.vertexMemory) vkFreeMemory(device, fb.vertexMemory, nullptr);
        fb.vertexBuffer = VK_NULL_HANDLE;
        fb.vertexMemory = VK_NULL_HANDLE;
        const VkDeviceSize grow = vbytes + vbytes / 2 + 256; // headroom to avoid churn
        if (!make(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, grow, fb.vertexBuffer, fb.vertexMemory)) {
            MAZ_LOG_ERROR("sprite: vertex buffer (re)allocation failed");
            return false;
        }
        fb.vertexCapacity = grow;
    }
    if (fb.indexCapacity < ibytes) {
        if (fb.indexBuffer) vkDestroyBuffer(device, fb.indexBuffer, nullptr);
        if (fb.indexMemory) vkFreeMemory(device, fb.indexMemory, nullptr);
        fb.indexBuffer = VK_NULL_HANDLE;
        fb.indexMemory = VK_NULL_HANDLE;
        const VkDeviceSize grow = ibytes + ibytes / 2 + 256;
        if (!make(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, grow, fb.indexBuffer, fb.indexMemory)) {
            MAZ_LOG_ERROR("sprite: index buffer (re)allocation failed");
            return false;
        }
        fb.indexCapacity = grow;
    }
    return true;
}

void SpriteRenderer::flush(VulkanContext& ctx, VkCommandBuffer cmd) {
    if (m_pipeline == VK_NULL_HANDLE || m_indices.empty() || m_frames.empty()) {
        return;
    }
    FrameBuffers& fb = m_frames[m_frameIndex];
    const VkDeviceSize vbytes = sizeof(Vertex) * m_vertices.size();
    const VkDeviceSize ibytes = sizeof(uint32_t) * m_indices.size();
    if (!ensureCapacity(ctx, fb, vbytes, ibytes)) {
        return;
    }

    VkDevice device = ctx.device();
    void* mapped = nullptr;
    vkMapMemory(device, fb.vertexMemory, 0, vbytes, 0, &mapped);
    std::memcpy(mapped, m_vertices.data(), static_cast<size_t>(vbytes));
    vkUnmapMemory(device, fb.vertexMemory);
    vkMapMemory(device, fb.indexMemory, 0, ibytes, 0, &mapped);
    std::memcpy(mapped, m_indices.data(), static_cast<size_t>(ibytes));
    vkUnmapMemory(device, fb.indexMemory);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_screen.width);
    viewport.height = static_cast<float>(m_screen.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_screen;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    const math::mat4 proj =
        math::ortho2D(static_cast<float>(m_screen.width), static_cast<float>(m_screen.height));
    vkCmdPushConstants(cmd, m_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(math::mat4), &proj);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &fb.vertexBuffer, &offset);
    vkCmdBindIndexBuffer(cmd, fb.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    for (const Batch& b : m_batches) {
        if (b.texture < 0 || static_cast<size_t>(b.texture) >= m_textures.size()) {
            continue;
        }
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 0, 1,
                                &m_textures[static_cast<size_t>(b.texture)].set, 0, nullptr);
        vkCmdDrawIndexed(cmd, b.indexCount, 1, b.firstIndex, 0, 0);
    }
}

void SpriteRenderer::destroyFrame(VkDevice device, FrameBuffers& fb) {
    if (fb.vertexBuffer) vkDestroyBuffer(device, fb.vertexBuffer, nullptr);
    if (fb.vertexMemory) vkFreeMemory(device, fb.vertexMemory, nullptr);
    if (fb.indexBuffer) vkDestroyBuffer(device, fb.indexBuffer, nullptr);
    if (fb.indexMemory) vkFreeMemory(device, fb.indexMemory, nullptr);
    fb = FrameBuffers{};
}

void SpriteRenderer::destroy(VulkanContext& ctx) {
    VkDevice device = ctx.device();
    for (Texture& t : m_textures) {
        if (t.view) vkDestroyImageView(device, t.view, nullptr);
        if (t.image) vkDestroyImage(device, t.image, nullptr);
        if (t.memory) vkFreeMemory(device, t.memory, nullptr);
        // Descriptor sets are freed with the pool below.
    }
    m_textures.clear();
    for (FrameBuffers& fb : m_frames) {
        destroyFrame(device, fb);
    }
    m_frames.clear();

    if (m_uploadFence) {
        vkDestroyFence(device, m_uploadFence, nullptr);
        m_uploadFence = VK_NULL_HANDLE;
    }
    if (m_uploadPool) {
        vkDestroyCommandPool(device, m_uploadPool, nullptr);
        m_uploadPool = VK_NULL_HANDLE;
    }
    if (m_sampler) {
        vkDestroySampler(device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    if (m_descriptorPool) {
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    if (m_setLayout) {
        vkDestroyDescriptorSetLayout(device, m_setLayout, nullptr);
        m_setLayout = VK_NULL_HANDLE;
    }
    if (m_pipeline) {
        vkDestroyPipeline(device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_layout) {
        vkDestroyPipelineLayout(device, m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;
    }
}

} // namespace maz::render
