#pragma once

#include "RHI/IRenderAPI.h"

#if OPAAX_HAS_VULKAN

#include "Core/OpaaxTypes.h"
#include "RHI/Vulkan/VulkanCommandBuffer.h"
#include "RHI/Vulkan/VulkanSwapchain.h"   // OPAAX_FRAMES_IN_FLIGHT

#include <vulkan/vulkan.h>

namespace Opaax
{
    class VulkanDevice;
    class VulkanContext;

    // =============================================================================
    // VulkanRenderAPI
    // =============================================================================
    /**
     * @class VulkanRenderAPI
     *
     * IRenderAPI for Vulkan. Borrows the VulkanDevice + VulkanSwapchain the context owns
     * (Init downcasts the IGraphicsContext). Owns the command pool + one primary command
     * buffer per frame-in-flight. BeginFrame acquires the next image + opens recording;
     * EndFrame ends + submits (wait image-available, signal render-finished + the in-flight
     * fence); present stays in the context (SwapBuffers).
     */
    class VulkanRenderAPI final : public IRenderAPI
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        VulkanRenderAPI() = default;
        ~VulkanRenderAPI() override;

        // =============================================================================
        // Overrides
        // =============================================================================

        //~Begin IRenderAPI interface
    public:
        void            Init(IGraphicsContext& InContext)                            override;
        void            BeginFrame()                                                 override;
        void            EndFrame()                                                   override;
        ICommandBuffer& GetCommandBuffer()                                           override;
        void            SetViewport(Uint32 X, Uint32 Y, Uint32 Width, Uint32 Height) override;
        void            WaitIdle()                                                   override;
        //~End IRenderAPI interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        VulkanDevice*    m_Device    = nullptr;   // borrowed (owned by the context)
        VulkanSwapchain* m_Swapchain = nullptr;   // borrowed

        VkCommandPool   m_CommandPool = VK_NULL_HANDLE;
        VkCommandBuffer m_CommandBuffers[OPAAX_FRAMES_IN_FLIGHT] = {};

        VulkanCommandBuffer m_CmdBuffer;          // ICommandBuffer wrapper, retargeted each frame
        bool                m_FrameActive = false;
        Uint32              m_FrameSlot   = 0;     // command buffer index for the active frame
    };

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
