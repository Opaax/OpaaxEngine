#include "VulkanSwapchain.h"

#if OPAAX_HAS_VULKAN

#include "VulkanDevice.h"
#include "Core/Log/OpaaxLog.h"

#include <VkBootstrap.h>
#include <GLFW/glfw3.h>

namespace Opaax
{
    VulkanSwapchain::VulkanSwapchain(VulkanDevice& InDevice, GLFWwindow* InWindow)
        : m_Device(InDevice), m_Window(InWindow)
    {
        int lW = 0, lH = 0;
        glfwGetFramebufferSize(m_Window, &lW, &lH);

        CreatePerFrameSync();
        Build(static_cast<Uint32>(lW), static_cast<Uint32>(lH));
    }

    VulkanSwapchain::~VulkanSwapchain()
    {
        if (m_Device.GetDevice()) { vkDeviceWaitIdle(m_Device.GetDevice()); }
        Cleanup();
        DestroyPerFrameSync();
    }

    // =============================================================================
    // Build / recreate
    // =============================================================================
    bool VulkanSwapchain::Build(Uint32 InWidth, Uint32 InHeight)
    {
        if (InWidth == 0 || InHeight == 0)
        {
            // Minimized — defer until the window has a non-zero size again.
            m_NeedsRecreate = true;
            m_PendingWidth  = InWidth;
            m_PendingHeight = InHeight;
            return false;
        }

        vkb::SwapchainBuilder lBuilder{ m_Device.GetPhysicalDevice(), m_Device.GetDevice(),
                                        m_Device.GetSurface(),
                                        m_Device.GetGraphicsQueueFamily(),
                                        m_Device.GetGraphicsQueueFamily() };

        auto lRet = lBuilder.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)   // vsync (always available)
                            .set_desired_extent(InWidth, InHeight)
                            .build();
        if (!lRet)
        {
            OPAAX_CORE_ERROR("VulkanSwapchain: build failed: {}", lRet.error().message());
            return false;
        }

        vkb::Swapchain lVkb = lRet.value();
        m_Swapchain   = lVkb.swapchain;
        m_ImageFormat = lVkb.image_format;
        m_Extent      = lVkb.extent;

        auto lImages = lVkb.get_images();
        auto lViews  = lVkb.get_image_views();
        if (!lImages || !lViews)
        {
            OPAAX_CORE_ERROR("VulkanSwapchain: failed to retrieve swapchain images/views.");
            return false;
        }
        m_Images     = lImages.value();
        m_ImageViews = lViews.value();

        // One render-finished semaphore per swapchain image (present waits on it).
        m_RenderFinished.resize(m_Images.size());
        VkSemaphoreCreateInfo lSemInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        for (auto& lSem : m_RenderFinished)
        {
            vkCreateSemaphore(m_Device.GetDevice(), &lSemInfo, nullptr, &lSem);
        }

        OPAAX_CORE_INFO("VulkanSwapchain: {}x{}, {} images.", m_Extent.width, m_Extent.height, m_Images.size());
        return true;
    }

    void VulkanSwapchain::Recreate()
    {
        Uint32 lW = m_PendingWidth, lH = m_PendingHeight;
        if (lW == 0 || lH == 0)
        {
            int lqW = 0, lqH = 0;
            glfwGetFramebufferSize(m_Window, &lqW, &lqH);
            lW = static_cast<Uint32>(lqW);
            lH = static_cast<Uint32>(lqH);
        }

        if (lW == 0 || lH == 0) { return; }   // still minimized — stay flagged, try again next frame

        vkDeviceWaitIdle(m_Device.GetDevice());
        Cleanup();
        m_NeedsRecreate = !Build(lW, lH);
    }

    // =============================================================================
    // Frame lifecycle
    // =============================================================================
    bool VulkanSwapchain::AcquireNextImage()
    {
        const VkDevice lDev = m_Device.GetDevice();
        m_FrameAcquired = false;

        vkWaitForFences(lDev, 1, &m_InFlightFence[m_CurrentFrame], VK_TRUE, UINT64_MAX);

        if (m_NeedsRecreate)
        {
            m_NeedsRecreate = false;
            Recreate();
            return false;   // skip this frame; next frame draws into the fresh swapchain
        }

        const VkResult lRes = vkAcquireNextImageKHR(lDev, m_Swapchain, UINT64_MAX,
                                                    m_ImageAvailable[m_CurrentFrame], VK_NULL_HANDLE,
                                                    &m_ImageIndex);
        if (lRes == VK_ERROR_OUT_OF_DATE_KHR)
        {
            m_NeedsRecreate = true;
            return false;
        }
        if (lRes != VK_SUCCESS && lRes != VK_SUBOPTIMAL_KHR)
        {
            OPAAX_CORE_ERROR("VulkanSwapchain: vkAcquireNextImageKHR failed ({}).", static_cast<int>(lRes));
            return false;
        }

        // Only reset the fence once we know we will submit (avoids a deadlock on a skipped frame).
        vkResetFences(lDev, 1, &m_InFlightFence[m_CurrentFrame]);
        m_FrameAcquired = true;
        return true;
    }

    void VulkanSwapchain::Present()
    {
        if (!m_FrameAcquired) { return; }   // frame was skipped (recreate) — nothing submitted

        VkSemaphore lWait = m_RenderFinished[m_ImageIndex];

        VkPresentInfoKHR lInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        lInfo.waitSemaphoreCount = 1;
        lInfo.pWaitSemaphores    = &lWait;
        lInfo.swapchainCount     = 1;
        lInfo.pSwapchains        = &m_Swapchain;
        lInfo.pImageIndices      = &m_ImageIndex;

        const VkResult lRes = vkQueuePresentKHR(m_Device.GetPresentQueue(), &lInfo);
        if (lRes == VK_ERROR_OUT_OF_DATE_KHR || lRes == VK_SUBOPTIMAL_KHR)
        {
            m_NeedsRecreate = true;
        }
        else if (lRes != VK_SUCCESS)
        {
            OPAAX_CORE_ERROR("VulkanSwapchain: vkQueuePresentKHR failed ({}).", static_cast<int>(lRes));
        }

        m_CurrentFrame  = (m_CurrentFrame + 1) % OPAAX_FRAMES_IN_FLIGHT;
        m_FrameAcquired = false;
    }

    void VulkanSwapchain::MarkForRecreate(Uint32 InWidth, Uint32 InHeight)
    {
        m_NeedsRecreate = true;
        m_PendingWidth  = InWidth;
        m_PendingHeight = InHeight;
    }

    // =============================================================================
    // Sync objects + cleanup
    // =============================================================================
    void VulkanSwapchain::CreatePerFrameSync()
    {
        const VkDevice lDev = m_Device.GetDevice();

        VkSemaphoreCreateInfo lSemInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VkFenceCreateInfo     lFenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        lFenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;   // first AcquireNextImage must not block

        for (Uint32 i = 0; i < OPAAX_FRAMES_IN_FLIGHT; ++i)
        {
            vkCreateSemaphore(lDev, &lSemInfo, nullptr, &m_ImageAvailable[i]);
            vkCreateFence(lDev, &lFenceInfo, nullptr, &m_InFlightFence[i]);
        }
    }

    void VulkanSwapchain::DestroyPerFrameSync()
    {
        const VkDevice lDev = m_Device.GetDevice();
        for (Uint32 i = 0; i < OPAAX_FRAMES_IN_FLIGHT; ++i)
        {
            if (m_ImageAvailable[i]) { vkDestroySemaphore(lDev, m_ImageAvailable[i], nullptr); }
            if (m_InFlightFence[i])  { vkDestroyFence(lDev, m_InFlightFence[i], nullptr); }
        }
    }

    void VulkanSwapchain::Cleanup()
    {
        const VkDevice lDev = m_Device.GetDevice();

        for (VkSemaphore lSem : m_RenderFinished) { if (lSem) { vkDestroySemaphore(lDev, lSem, nullptr); } }
        m_RenderFinished.clear();

        for (VkImageView lView : m_ImageViews) { if (lView) { vkDestroyImageView(lDev, lView, nullptr); } }
        m_ImageViews.clear();
        m_Images.clear();

        if (m_Swapchain) { vkDestroySwapchainKHR(lDev, m_Swapchain, nullptr); m_Swapchain = VK_NULL_HANDLE; }
    }

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
