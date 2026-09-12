#pragma once

#include "maz/render/Renderer.hpp" // TextureHandle
#include "render/VulkanTexture.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace maz::render {

class VulkanContext;

// Shared texture registry: owns one combined-image-sampler descriptor set layout, a descriptor
// pool, and per-texture (VulkanTexture + descriptor set). Both the 2D sprite renderer and the 3D
// mesh renderer bind textures from here using this same layout, so a texture created once works
// with either pipeline and handles are a single namespace.
class TextureStore {
public:
    bool init(VulkanContext& ctx, uint32_t maxTextures = 256);
    void shutdown(VulkanContext& ctx);

    TextureHandle createFromPixels(VulkanContext& ctx, uint32_t w, uint32_t h, const void* rgba);
    TextureHandle createFromFile(VulkanContext& ctx, const char* path);

    VkDescriptorSetLayout layout() const { return m_setLayout; }
    VkDescriptorSet descriptorSet(TextureHandle h) const;
    bool valid(TextureHandle h) const { return h != kInvalidTexture && h < m_textures.size(); }

private:
    TextureHandle registerTexture(VulkanContext& ctx, VulkanTexture&& tex);

    struct Entry {
        VulkanTexture texture;
        VkDescriptorSet set;
    };

    VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    std::vector<Entry> m_textures; // index 0 reserved (kInvalidTexture)
    uint32_t m_maxTextures = 0;
};

} // namespace maz::render
