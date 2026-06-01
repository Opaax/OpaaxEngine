#pragma once

#include "RHI/Texture.h"

#if OPAAX_HAS_VULKAN

namespace Opaax
{
    // =============================================================================
    // VulkanTexture2D  (Phase 2 stub)
    // =============================================================================
    // Tracks size only; no image/sampler yet. IsLoaded() returns true so Renderer2D's draw
    // asserts pass. Phase 3 backs it with a VkImage + sampler + VMA allocation.
    class VulkanTexture2D final : public ITexture2D
    {
    public:
        explicit VulkanTexture2D(const char* /*InPath*/) {}
        VulkanTexture2D(Uint32 InWidth, Uint32 InHeight) : m_Width(InWidth), m_Height(InHeight) {}
        VulkanTexture2D(const unsigned char* /*InData*/, Uint32 InWidth, Uint32 InHeight, Int32 /*InChannels*/)
            : m_Width(InWidth), m_Height(InHeight) {}

        void Bind(Uint32 /*InSlot*/ = 0) const override {}
        void Unbind()                    const override {}

        Uint32 GetWidth()      const noexcept override { return m_Width; }
        Uint32 GetHeight()     const noexcept override { return m_Height; }
        Uint32 GetRendererID() const noexcept override { return 0; }
        bool   IsLoaded()      const noexcept override { return true; }

    private:
        Uint32 m_Width  = 1;
        Uint32 m_Height = 1;
    };

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
