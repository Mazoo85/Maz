#include "render/TextureStore.hpp"

#include "maz/core/Log.hpp"
#include "render/VulkanContext.hpp"

#include <utility>

namespace maz::render {

bool TextureStore::init(VulkanContext& ctx, uint32_t maxTextures) {
    m_maxTextures = maxTextures;
    m_textures.push_back(Entry{}); // reserve index 0 == kInvalidTexture

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
        MAZ_LOG_ERROR("TextureStore: vkCreateDescriptorSetLayout failed");
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
        MAZ_LOG_ERROR("TextureStore: vkCreateDescriptorPool failed");
        return false;
    }
    return true;
}

TextureHandle TextureStore::registerTexture(VulkanContext& ctx, VulkanTexture&& tex) {
    if (m_textures.size() >= m_maxTextures) {
        MAZ_LOG_ERROR("TextureStore: pool exhausted (max %u)", m_maxTextures);
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
        MAZ_LOG_ERROR("TextureStore: vkAllocateDescriptorSets failed");
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

TextureHandle TextureStore::createFromPixels(VulkanContext& ctx, uint32_t w, uint32_t h,
                                             const void* rgba) {
    VulkanTexture tex;
    if (!tex.create(ctx, w, h, rgba)) {
        return kInvalidTexture;
    }
    return registerTexture(ctx, std::move(tex));
}

TextureHandle TextureStore::createFromFile(VulkanContext& ctx, const char* path) {
    VulkanTexture tex;
    if (!tex.createFromFile(ctx, path)) {
        return kInvalidTexture;
    }
    return registerTexture(ctx, std::move(tex));
}

VkDescriptorSet TextureStore::descriptorSet(TextureHandle h) const {
    return valid(h) ? m_textures[h].set : VK_NULL_HANDLE;
}

void TextureStore::shutdown(VulkanContext& ctx) {
    for (size_t i = 1; i < m_textures.size(); ++i) { // 0 is the reserved invalid slot
        m_textures[i].texture.destroy(ctx);
    }
    m_textures.clear();
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
