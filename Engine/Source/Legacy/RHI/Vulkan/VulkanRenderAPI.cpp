#include "VulkanRenderAPI.h"

#if OPAAX_HAS_VULKAN

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanFrameContext.h"
#include "Core/Log/OpaaxLog.h"

namespace Opaax
{
    VulkanRenderAPI::~VulkanRenderAPI()
    {
        VulkanFrameContext::Clear();

        if (m_Device && m_Device->GetDevice())
        {
            vkDeviceWaitIdle(m_Device->GetDevice());
            if (m_CommandPool)
            {
                // Destroying the pool frees its command buffers too.
                vkDestroyCommandPool(m_Device->GetDevice(), m_CommandPool, nullptr);
            }
        }
    }

    void VulkanRenderAPI::Init(IGraphicsContext& InContext)
    {
        // The window already created + Init'd the Vulkan context; borrow its device + swapchain.
        auto& lContext = static_cast<VulkanContext&>(InContext);
        m_Device    = &lContext.GetDevice();
        m_Swapchain = &lContext.GetSwapchain();

        m_CmdBuffer.Setup(m_Device, m_Swapchain);

        // Resources (built by the factory, written from neutral Renderer2D) reach the device +
        // current frame slot through this static — there is one device + swapchain.
        VulkanFrameContext::SetDevice(m_Device);
        VulkanFrameContext::SetColorFormat(m_Swapchain->GetImageFormat());   // pipeline color attachment

        // ---- Command pool + per-frame primary command buffers ----
        VkCommandPoolCreateInfo lPoolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        lPoolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        lPoolInfo.queueFamilyIndex = m_Device->GetGraphicsQueueFamily();
        if (vkCreateCommandPool(m_Device->GetDevice(), &lPoolInfo, nullptr, &m_CommandPool) != VK_SUCCESS)
        {
            OPAAX_CORE_ERROR("VulkanRenderAPI: failed to create command pool.");
            return;
        }

        VkCommandBufferAllocateInfo lAllocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        lAllocInfo.commandPool        = m_CommandPool;
        lAllocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        lAllocInfo.commandBufferCount = OPAAX_FRAMES_IN_FLIGHT;
        if (vkAllocateCommandBuffers(m_Device->GetDevice(), &lAllocInfo, m_CommandBuffers) != VK_SUCCESS)
        {
            OPAAX_CORE_ERROR("VulkanRenderAPI: failed to allocate command buffers.");
            return;
        }

        OPAAX_CORE_INFO("VulkanRenderAPI: initialized ({} frames in flight).", OPAAX_FRAMES_IN_FLIGHT);
    }

    void VulkanRenderAPI::BeginFrame()
    {
        m_FrameActive = m_Swapchain->AcquireNextImage();
        if (!m_FrameActive)
        {
            // Frame skipped (swapchain recreated). The neutral passes still run Begin/EndRenderPass
            // between BeginFrame and EndFrame, so null the recorder to make that recording a no-op.
            m_CmdBuffer.SetCurrent(VK_NULL_HANDLE);
            return;
        }

        m_FrameSlot = m_Swapchain->GetCurrentFrameIndex();

        // New frame: publish the slot + bump the generation so per-frame ring cursors reset.
        // Only on a real acquire — a skipped frame records nothing, so cursors must stay put.
        VulkanFrameContext::BeginFrame(m_FrameSlot);

        VkCommandBuffer lCmd = m_CommandBuffers[m_FrameSlot];

        vkResetCommandBuffer(lCmd, 0);

        VkCommandBufferBeginInfo lBeginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        lBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(lCmd, &lBeginInfo);

        m_CmdBuffer.SetCurrent(lCmd);
    }

    void VulkanRenderAPI::EndFrame()
    {
        if (!m_FrameActive) { return; }

        VkCommandBuffer lCmd = m_CommandBuffers[m_FrameSlot];

        // Transition the image to PRESENT_SRC (once, after the last pass) then close recording.
        m_CmdBuffer.FinishFrame();
        vkEndCommandBuffer(lCmd);

        VkSemaphore          lWait      = m_Swapchain->GetImageAvailableSemaphore();
        VkSemaphore          lSignal    = m_Swapchain->GetRenderFinishedSemaphore();
        VkPipelineStageFlags lWaitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo lSubmit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        lSubmit.waitSemaphoreCount   = 1;
        lSubmit.pWaitSemaphores      = &lWait;
        lSubmit.pWaitDstStageMask    = &lWaitStage;
        lSubmit.commandBufferCount   = 1;
        lSubmit.pCommandBuffers      = &lCmd;
        lSubmit.signalSemaphoreCount = 1;
        lSubmit.pSignalSemaphores    = &lSignal;

        if (vkQueueSubmit(m_Device->GetGraphicsQueue(), 1, &lSubmit, m_Swapchain->GetInFlightFence()) != VK_SUCCESS)
        {
            OPAAX_CORE_ERROR("VulkanRenderAPI: vkQueueSubmit failed.");
        }

        m_FrameActive = false;
    }

    ICommandBuffer& VulkanRenderAPI::GetCommandBuffer()
    {
        return m_CmdBuffer;
    }

    void VulkanRenderAPI::SetViewport(Uint32 /*X*/, Uint32 /*Y*/, Uint32 Width, Uint32 Height)
    {
        // No live command buffer outside a frame — a window resize means recreate the swapchain.
        if (m_Swapchain) { m_Swapchain->MarkForRecreate(Width, Height); }
    }

    void VulkanRenderAPI::WaitIdle()
    {
        // Block until the GPU drains — callers destroy GPU resources (Renderer2D pipeline/buffers)
        // right after, which is illegal while a frame referencing them is still in flight.
        if (m_Device && m_Device->GetDevice()) { vkDeviceWaitIdle(m_Device->GetDevice()); }
    }

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
