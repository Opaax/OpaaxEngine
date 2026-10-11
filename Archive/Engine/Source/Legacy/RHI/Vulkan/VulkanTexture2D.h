#pragma once

#include "RHI/Texture.h"

#if OPAAX_HAS_VULKAN

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

namespace Opaax
{
    // =============================================================================
    // VulkanTexture2D
    // =============================================================================
    /**
     * @class VulkanTexture2D
     *
     * ITexture2D for Vulkan: a sampled VkImage (VMA-backed) + view + sampler. Uploaded once via a
     * staging buffer + a synchronous one-shot copy (textures load outside the frame loop). Channel
     * handling mirrors OpenGLTexture2D::Upload so sprites AND text render identically:
     *   - 4ch -> R8G8B8A8_UNORM, identity swizzle
     *   - 1ch (font atlas R8) -> R8_UNORM with a view swizzle {ONE,ONE,ONE,R}, so coverage reads as
     *     (1,1,1,a) — the Vulkan equivalent of GL's GL_TEXTURE_SWIZZLE_RGBA path. LINEAR + CLAMP.
     *   - 3ch -> expanded to RGBA on the CPU (Vulkan rarely supports sampled RGB8)
     */
    class VulkanTexture2D final : public ITexture2D
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        explicit VulkanTexture2D(const char* InPath);
        VulkanTexture2D(Uint32 InWidth, Uint32 InHeight);                                   // 1x1 white
        VulkanTexture2D(const unsigned char* InData, Uint32 InWidth, Uint32 InHeight, Int32 InChannels);
        ~VulkanTexture2D() override;

        // =============================================================================
        // Overrides
        // =============================================================================

        //~Begin ITexture2D interface
    public:
        void Bind(Uint32 /*InSlot*/ = 0) const override {}   // descriptor-driven; nothing to bind here
        void Unbind()                    const override {}

        Uint32 GetWidth()      const noexcept override { return m_Width; }
        Uint32 GetHeight()     const noexcept override { return m_Height; }
        Uint32 GetRendererID() const noexcept override { return 0; }   // editor uses GetTextureID seam (image view + sampler)
        bool   IsLoaded()      const noexcept override { return m_Loaded; }
        //~End ITexture2D interface

        // =============================================================================
        // Get — consumed by VulkanBindGroup
        // =============================================================================
    public:
        VkImageView GetImageView() const noexcept { return m_ImageView; }
        VkSampler   GetSampler()   const noexcept { return m_Sampler; }

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        // InChannels: 4 = RGBA8, 3 = RGB8 (expanded), 1 = R8 coverage (alpha-swizzled).
        void Upload(const unsigned char* InData, Uint32 InWidth, Uint32 InHeight, Int32 InChannels);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        VmaAllocator  m_Allocator = nullptr;
        VkDevice      m_Device    = VK_NULL_HANDLE;

        VkImage       m_Image     = VK_NULL_HANDLE;
        VmaAllocation m_Alloc     = nullptr;
        VkImageView   m_ImageView = VK_NULL_HANDLE;
        VkSampler     m_Sampler   = VK_NULL_HANDLE;

        Uint32 m_Width  = 1;
        Uint32 m_Height = 1;
        bool   m_Loaded = false;
    };

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
