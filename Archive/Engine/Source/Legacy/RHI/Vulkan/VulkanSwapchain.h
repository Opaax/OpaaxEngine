#pragma once

#include "Core/OpaaxTypes.h"

#if OPAAX_HAS_VULKAN

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace Opaax
{
    class VulkanDevice;

    // Double-buffered: at most this many frames recorded/submitted before the CPU waits.
    inline constexpr Uint32 OPAAX_FRAMES_IN_FLIGHT = 2;

    // =============================================================================
    // VulkanSwapchain
    // =============================================================================
    /**
     * @class VulkanSwapchain
     *
     * Owns the VkSwapchainKHR + its images/views and the frame synchronization that straddles
     * the render API (acquire + submit) and the context (present):
     *   - per frame-in-flight : an image-available semaphore + an in-flight fence
     *   - per swapchain image : a render-finished semaphore (waited by present)
     *
     * Frame flow: AcquireNextImage (waits the fence, acquires an image, signals image-available)
     * -> the render API submits (waits image-available, signals render-finished + the fence) ->
     * Present (waits render-finished, presents, advances the frame). Recreated on resize /
     * VK_ERROR_OUT_OF_DATE_KHR.
     */
    class VulkanSwapchain
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        VulkanSwapchain(VulkanDevice& InDevice, GLFWwindow* InWindow);
        ~VulkanSwapchain();

        VulkanSwapchain(const VulkanSwapchain&)            = delete;
        VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

        // =============================================================================
        // Frame lifecycle
        // =============================================================================
    public:
        bool IsValid() const noexcept { return m_Swapchain != VK_NULL_HANDLE; }

        // Wait the current frame's fence, (re)create if flagged, acquire the next image.
        // Returns false when the frame should be skipped (swapchain was recreated / not ready).
        bool AcquireNextImage();

        // Present the acquired image (waits the render-finished semaphore the submit signaled),
        // then advance to the next frame-in-flight slot. No-op if the frame was skipped.
        void Present();

        // True between a successful AcquireNextImage and its Present — the render API gates its
        // submit on this, and Present gates on it, so a skipped (recreated) frame never submits
        // or presents against an unsignaled semaphore.
        bool IsFrameAcquired() const noexcept { return m_FrameAcquired; }

        // Defer a recreate to the next AcquireNextImage (resize path).
        void MarkForRecreate(Uint32 InWidth, Uint32 InHeight);

        // =============================================================================
        // Get — sync handles the render API needs to build its submit
        // =============================================================================
    public:
        VkSemaphore GetImageAvailableSemaphore() const noexcept { return m_ImageAvailable[m_CurrentFrame]; }
        VkSemaphore GetRenderFinishedSemaphore() const noexcept { return m_RenderFinished[m_ImageIndex]; }
        VkFence     GetInFlightFence()           const noexcept { return m_InFlightFence[m_CurrentFrame]; }

        VkImage     GetCurrentImage()     const noexcept { return m_Images[m_ImageIndex]; }
        VkImageView GetCurrentImageView() const noexcept { return m_ImageViews[m_ImageIndex]; }
        VkExtent2D  GetExtent()           const noexcept { return m_Extent; }
        VkFormat    GetImageFormat()      const noexcept { return m_ImageFormat; }

        // Frame-in-flight slot (0..OPAAX_FRAMES_IN_FLIGHT-1) — the render API indexes its
        // per-frame command buffer by this so the in-flight fence gates the right buffer.
        Uint32      GetCurrentFrameIndex() const noexcept { return m_CurrentFrame; }

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        // InLogInfo: log the result at INFO (initial build, part of the backend banner) vs TRACE
        // (recreate — fires every frame during a resize, so it must not spam at INFO).
        bool Build(Uint32 InWidth, Uint32 InHeight, bool InLogInfo);  // build swapchain + views + per-image semaphores
        void Recreate();
        void CreatePerFrameSync();                     // image-available semaphores + fences (once)
        void DestroyPerFrameSync();
        void Cleanup();                                // views + per-image semaphores + swapchain

        // =============================================================================
        // Members
        // =============================================================================
    private:
        VulkanDevice& m_Device;
        GLFWwindow*   m_Window = nullptr;

        VkSwapchainKHR        m_Swapchain   = VK_NULL_HANDLE;
        VkFormat              m_ImageFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D            m_Extent      = { 0, 0 };
        TDynArray<VkImage>     m_Images;
        TDynArray<VkImageView> m_ImageViews;
        TDynArray<VkSemaphore> m_RenderFinished;     // one per swapchain image

        VkSemaphore m_ImageAvailable[OPAAX_FRAMES_IN_FLIGHT] = {};
        VkFence     m_InFlightFence [OPAAX_FRAMES_IN_FLIGHT] = {};

        Uint32 m_CurrentFrame  = 0;
        Uint32 m_ImageIndex    = 0;
        bool   m_FrameAcquired = false;
        bool   m_NeedsRecreate = false;
        Uint32 m_PendingWidth  = 0;
        Uint32 m_PendingHeight = 0;
    };

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
