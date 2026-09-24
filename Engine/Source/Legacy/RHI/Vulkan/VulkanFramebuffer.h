#pragma once

#include "RHI/Framebuffer.h"

#if OPAAX_HAS_VULKAN

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

namespace Opaax
{
    // =============================================================================
    // VulkanFramebuffer
    // =============================================================================
    /**
     * @class VulkanFramebuffer
     *
     * IFramebuffer for Vulkan: one color VkImage usable as BOTH a color attachment (the editor's
     * world/overlay passes render into it) AND a sampled texture (ImGui shows it in the
     * ViewportPanel). VMA-backed, in the swapchain color format so the same Renderer2D pipeline
     * (baked against that format) draws into it without a format mismatch.
     *
     * Vulkan 1.3 dynamic rendering => no VkFramebuffer object; the command buffer renders to the
     * image view directly. This class owns the image/view/sampler and tracks the current image
     * layout so VulkanCommandBuffer can bracket each offscreen pass COLOR <-> SHADER_READ_ONLY.
     *
     * One image (not per-frame-in-flight): the begin-of-pass barrier's first scope covers the
     * prior frame's ImGui sampling (FRAGMENT_SHADER / SHADER_READ) on the same queue, so a single
     * image is correctly synchronized across frames in flight without N copies.
     *
     * Bind/Unbind are no-ops (Vulkan has no bound-FBO state). GetColorAttachmentID returns 0 — the
     * editor display path goes through IEditorUIBackend::GetViewportTextureID, not a raw handle.
     */
    class VulkanFramebuffer final : public IFramebuffer
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        explicit VulkanFramebuffer(const FramebufferSpec& InSpec);
        ~VulkanFramebuffer() override;

        VulkanFramebuffer(const VulkanFramebuffer&)            = delete;
        VulkanFramebuffer& operator=(const VulkanFramebuffer&) = delete;

        // =============================================================================
        // Overrides
        // =============================================================================

        //~Begin IFramebuffer interface
    public:
        void Bind()   override {}
        void Unbind() override {}
        void Resize(Uint32 InWidth, Uint32 InHeight) override;

        Uint32 GetColorAttachmentID() const noexcept override { return 0; }
        Uint32 GetWidth()             const noexcept override { return m_Width; }
        Uint32 GetHeight()            const noexcept override { return m_Height; }
        //~End IFramebuffer interface

        // Apply a deferred resize at a frame-safe point — called by VulkanCommandBuffer at the top
        // of the offscreen pass (before any draw records into the image). Resize() only stores the
        // pending size: destroying the image during the editor's Draw would orphan the world-pass
        // draws this frame's command buffer already recorded into it. WaitIdle-guards the destroy.
        void ApplyPendingResize();

        // =============================================================================
        // Get — VulkanCommandBuffer (render into) + VulkanEditorUIBackend (sample)
        // =============================================================================
    public:
        VkImage       GetImage()          const noexcept { return m_Image; }
        VkImageView   GetColorImageView() const noexcept { return m_ImageView; }
        VkSampler     GetSampler()        const noexcept { return m_Sampler; }
        VkExtent2D    GetExtent()         const noexcept { return { m_Width, m_Height }; }

        // Layout the command buffer brackets each offscreen pass against (mutable tracking).
        VkImageLayout GetCurrentLayout() const noexcept            { return m_Layout; }
        void          SetCurrentLayout(VkImageLayout InLayout) noexcept { m_Layout = InLayout; }

        // Bumped on every (re)create — the UI backend rebuilds its cached descriptor set on a mismatch.
        Uint64        GetGeneration()   const noexcept { return m_Generation; }

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        void Invalidate();   // (re)create the image + view at the current size (sampler once)
        void Release();      // destroy the image + view (sampler persists across resizes)

        // =============================================================================
        // Members
        // =============================================================================
    private:
        VmaAllocator m_Allocator = nullptr;
        VkDevice     m_Device    = VK_NULL_HANDLE;
        VkFormat     m_Format    = VK_FORMAT_UNDEFINED;

        VkImage       m_Image     = VK_NULL_HANDLE;
        VmaAllocation m_Alloc     = nullptr;
        VkImageView   m_ImageView = VK_NULL_HANDLE;
        VkSampler     m_Sampler   = VK_NULL_HANDLE;

        VkImageLayout m_Layout     = VK_IMAGE_LAYOUT_UNDEFINED;
        Uint64        m_Generation = 0;

        Uint32 m_Width  = 1;
        Uint32 m_Height = 1;

        // Deferred resize (applied at the next offscreen pass start, not during Draw).
        bool   m_PendingResize = false;
        Uint32 m_PendingWidth  = 1;
        Uint32 m_PendingHeight = 1;
    };

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
