#include "render/SpriteRenderer.hpp"

#include "maz/core/Log.hpp"
#include "maz/math/Math.hpp"
#include "render/VulkanContext.hpp"

#include <SDL3/SDL_filesystem.h>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace maz::render {

namespace {

constexpr uint32_t kMaxSprites = 8192;              // per frame
constexpr uint32_t kVertsPerSprite = 6;             // two triangles, no index buffer

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
        MAZ_LOG_ERROR("vkCreateShaderModule failed");
    }
    return module;
}

// Directory containing the executable, so shaders resolve regardless of CWD.
std::string assetBase() {
    const char* base = SDL_GetBasePath();
    return base ? std::string(base) : std::string();
}

} // namespace

bool SpriteRenderer::init(VulkanContext& ctx, VkRenderPass renderPass, uint32_t framesInFlight) {
    m_maxVertices = kMaxSprites * kVertsPerSprite;
    m_maxTextures = 256;
    m_textures.push_back(Entry{}); // reserve index 0 == kInvalidTexture
    m_vertices.reserve(m_maxVertices);

    return createDescriptorInfra(ctx) && createPipeline(ctx, renderPass) &&
           createVertexBuffers(ctx, framesInFlight);
}

bool SpriteRenderer::createDescriptorInfra(VulkanContext& ctx) {
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
        MAZ_LOG_ERROR("vkCreateDescriptorSetLayout failed");
        return false;
    }

    VkDescriptorPoolSize size{};
    size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    size.descriptorCount = m_maxTextures;

    VkDescriptorPoolCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pi.maxSets = m_maxTextures;
    pi.poolSizeCount = 1;
    pi.pPoolSizes = &size;
    if (vkCreateDescriptorPool(ctx.device(), &pi, nullptr, &m_pool) != VK_SUCCESS) {
        MAZ_LOG_ERROR("vkCreateDescriptorPool failed");
        return false;
    }
    return true;
}

bool SpriteRenderer::createPipeline(VulkanContext& ctx, VkRenderPass renderPass) {
    const std::string base = assetBase();
    VkShaderModule vert = createShaderModule(ctx.device(), readFile(base + "shaders/sprite.vert.spv"));
    VkShaderModule frag = createShaderModule(ctx.device(), readFile(base + "shaders/sprite.frag.spv"));
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

    VkVertexInputAttributeDescription attrs[3]{};
    attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[0].offset = offsetof(Vertex, pos);
    attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[1].offset = offsetof(Vertex, uv);
    attrs[2].location = 2;
    attrs[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrs[2].offset = offsetof(Vertex, color);

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &vb;
    vi.vertexAttributeDescriptionCount = 3;
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
    push.size = sizeof(glm::mat4);

    VkPipelineLayoutCreateInfo pl{};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &m_setLayout;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &push;
    if (vkCreatePipelineLayout(ctx.device(), &pl, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        MAZ_LOG_ERROR("vkCreatePipelineLayout failed");
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
    gp.layout = m_pipelineLayout;
    gp.renderPass = renderPass;
    gp.subpass = 0;

    VkResult r = vkCreateGraphicsPipelines(ctx.device(), VK_NULL_HANDLE, 1, &gp, nullptr,
                                           &m_pipeline);
    vkDestroyShaderModule(ctx.device(), vert, nullptr);
    vkDestroyShaderModule(ctx.device(), frag, nullptr);
    if (r != VK_SUCCESS) {
        MAZ_LOG_ERROR("vkCreateGraphicsPipelines failed (VkResult %d)", (int)r);
        return false;
    }
    return true;
}

bool SpriteRenderer::createVertexBuffers(VulkanContext& ctx, uint32_t framesInFlight) {
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

TextureHandle SpriteRenderer::registerTexture(VulkanContext& ctx, VulkanTexture&& tex) {
    if (m_textures.size() >= m_maxTextures) {
        MAZ_LOG_ERROR("texture pool exhausted (max %u)", m_maxTextures);
        tex.destroy(ctx);
        return kInvalidTexture;
    }

    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = m_pool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &m_setLayout;
    VkDescriptorSet set = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(ctx.device(), &ai, &set) != VK_SUCCESS) {
        MAZ_LOG_ERROR("vkAllocateDescriptorSets failed");
        tex.destroy(ctx);
        return kInvalidTexture;
    }

    VkDescriptorImageInfo img{};
    img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    img.imageView = tex.view();
    img.sampler = tex.sampler();
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &img;
    vkUpdateDescriptorSets(ctx.device(), 1, &write, 0, nullptr);

    m_textures.push_back(Entry{std::move(tex), set});
    return static_cast<TextureHandle>(m_textures.size() - 1);
}

TextureHandle SpriteRenderer::createTexture(VulkanContext& ctx, uint32_t w, uint32_t h,
                                            const void* rgba) {
    VulkanTexture tex;
    if (!tex.create(ctx, w, h, rgba)) {
        return kInvalidTexture;
    }
    return registerTexture(ctx, std::move(tex));
}

TextureHandle SpriteRenderer::loadTexture(VulkanContext& ctx, const char* path) {
    VulkanTexture tex;
    if (!tex.createFromFile(ctx, path)) {
        return kInvalidTexture;
    }
    return registerTexture(ctx, std::move(tex));
}

void SpriteRenderer::begin() {
    m_vertices.clear();
    m_batches.clear();
    m_cameraChanged = true; // first draw captures the current camera
}

void SpriteRenderer::draw(TextureHandle tex, const SpriteDesc& s) {
    if (tex == kInvalidTexture || tex >= m_textures.size()) {
        return;
    }
    if (m_vertices.size() + kVertsPerSprite > m_maxVertices) {
        if (!m_capacityWarned) {
            MAZ_LOG_WARN("sprite batch full (%u sprites/frame); dropping extras", kMaxSprites);
            m_capacityWarned = true;
        }
        return;
    }

    // Four corners around the sprite center, rotated, in world/pixel space.
    const float hw = s.width * 0.5f;
    const float hh = s.height * 0.5f;
    const float cx = s.x + hw;
    const float cy = s.y + hh;
    const float c = std::cos(s.rotation);
    const float sn = std::sin(s.rotation);

    auto corner = [&](float ox, float oy, float u, float v) -> Vertex {
        Vertex out{};
        out.pos[0] = cx + ox * c - oy * sn;
        out.pos[1] = cy + ox * sn + oy * c;
        out.uv[0] = u;
        out.uv[1] = v;
        out.color[0] = s.color.r;
        out.color[1] = s.color.g;
        out.color[2] = s.color.b;
        out.color[3] = s.color.a;
        return out;
    };

    const Vertex tl = corner(-hw, -hh, s.uvMinX, s.uvMinY);
    const Vertex tr = corner(hw, -hh, s.uvMaxX, s.uvMinY);
    const Vertex br = corner(hw, hh, s.uvMaxX, s.uvMaxY);
    const Vertex bl = corner(-hw, hh, s.uvMinX, s.uvMaxY);

    const auto first = static_cast<uint32_t>(m_vertices.size());
    if (m_batches.empty() || m_batches.back().tex != tex || m_cameraChanged) {
        m_batches.push_back(Batch{tex, first, 0, m_camera});
        m_cameraChanged = false;
    }
    m_vertices.push_back(tl);
    m_vertices.push_back(tr);
    m_vertices.push_back(br);
    m_vertices.push_back(tl);
    m_vertices.push_back(br);
    m_vertices.push_back(bl);
    m_batches.back().count += kVertsPerSprite;
}

void SpriteRenderer::flush(VkCommandBuffer cmd, uint32_t frameIndex) {
    if (m_vertices.empty() || m_pipeline == VK_NULL_HANDLE) {
        return;
    }
    std::memcpy(m_vboMapped[frameIndex], m_vertices.data(), m_vertices.size() * sizeof(Vertex));

    const float w = static_cast<float>(m_viewportW);
    const float h = static_cast<float>(m_viewportH);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    VkViewport viewport{};
    viewport.width = w;
    viewport.height = h;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{};
    scissor.extent = {m_viewportW, m_viewportH};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkDeviceSize offset = 0;
    VkBuffer buffer = m_vbo[frameIndex].handle();
    vkCmdBindVertexBuffers(cmd, 0, 1, &buffer, &offset);

    // Each batch carries its own camera, so a world-space pass and a pixel-space HUD pass can
    // coexist in one frame.
    for (const Batch& batch : m_batches) {
        glm::mat4 viewProj;
        if (batch.cam.usePixelSpace) {
            viewProj = math::ortho2D(w, h);
        } else {
            const float halfW = w * 0.5f / batch.cam.zoom;
            const float halfH = h * 0.5f / batch.cam.zoom;
            // y-down (top-left origin): min-y maps to the top. See math::ortho2D.
            viewProj = glm::ortho(batch.cam.centerX - halfW, batch.cam.centerX + halfW,
                                  batch.cam.centerY - halfH, batch.cam.centerY + halfH, -1.0f,
                                  1.0f);
        }
        vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4),
                           glm::value_ptr(viewProj));
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1,
                                &m_textures[batch.tex].set, 0, nullptr);
        vkCmdDraw(cmd, batch.count, 1, batch.first, 0);
    }
}

void SpriteRenderer::shutdown(VulkanContext& ctx) {
    for (auto& b : m_vbo) {
        b.destroy(ctx);
    }
    m_vbo.clear();
    m_vboMapped.clear();

    for (size_t i = 1; i < m_textures.size(); ++i) { // index 0 is the reserved invalid slot
        m_textures[i].texture.destroy(ctx);
    }
    m_textures.clear();

    if (m_pipeline) {
        vkDestroyPipeline(ctx.device(), m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout) {
        vkDestroyPipelineLayout(ctx.device(), m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
    if (m_pool) {
        vkDestroyDescriptorPool(ctx.device(), m_pool, nullptr);
        m_pool = VK_NULL_HANDLE;
    }
    if (m_setLayout) {
        vkDestroyDescriptorSetLayout(ctx.device(), m_setLayout, nullptr);
        m_setLayout = VK_NULL_HANDLE;
    }
}

} // namespace maz::render
