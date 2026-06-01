#pragma once

#include "RHI/ICommandBuffer.h"

#if OPAAX_HAS_VULKAN

#include <vulkan/vulkan.h>

namespace Opaax
{
    class VulkanDevice;
    class VulkanSwapchain;

    // =============================================================================
    // VulkanCommandBuffer
    // =============================================================================
    /**
     * @class VulkanCommandBuffer
     *
     * ICommandBuffer over a live VkCommandBuffer (owned + retargeted each frame by
     * VulkanRenderAPI). Uses Vulkan 1.3 dynamic rendering — BeginRenderPass transitions the
     * swapchain image to COLOR_ATTACHMENT on the first pass of the frame and vkCmdBeginRendering
     * (clear or load); EndRenderPass ends it. FinishFrame (called by the render API before the
     * command buffer ends) transitions the image to PRESENT_SRC exactly once.
     *
     * Phase 2: BindPipeline / BindBindGroup / BindVertexArray / DrawIndexed are no-ops — the
     * Vulkan resource impls land in Phase 3, so only the clear is functional.
     */
    class VulkanCommandBuffer final : public ICommandBuffer
    {
        // =============================================================================
        // Setup (called by VulkanRenderAPI)
        // =============================================================================
    public:
        void Setup(VulkanDevice* InDevice, VulkanSwapchain* InSwapchain) noexcept
        {
            m_Device    = InDevice;
            m_Swapchain = InSwapchain;
        }

        // Retarget to this frame's command buffer (resets per-frame layout tracking).
        void SetCurrent(VkCommandBuffer InCmd) noexcept
        {
            m_Cmd          = InCmd;
            m_ColorAcquired = false;
        }

        // Transition the image to PRESENT_SRC if any pass ran. Called before vkEndCommandBuffer.
        void FinishFrame();

        // =============================================================================
        // Overrides
        // =============================================================================

        //~Begin ICommandBuffer interface
    public:
        void BeginRenderPass(IRenderTarget& InTarget, ELoadOp InLoadOp, const Vector4F& InClearColor) override;
        void EndRenderPass() override;

        void SetViewport(Uint32 X, Uint32 Y, Uint32 Width, Uint32 Height) override;

        void BindPipeline(IPipeline& /*InPipeline*/)          override {}   // Phase 3
        void BindBindGroup(IBindGroup& /*InBindGroup*/)       override {}   // Phase 3
        void BindVertexArray(IVertexArray& /*InVertexArray*/) override {}   // Phase 3
        void DrawIndexed(Uint32 /*InIndexCount*/)             override {}   // Phase 3
        //~End ICommandBuffer interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        VulkanDevice*    m_Device    = nullptr;
        VulkanSwapchain* m_Swapchain = nullptr;
        VkCommandBuffer  m_Cmd       = VK_NULL_HANDLE;
        bool             m_ColorAcquired = false;   // image transitioned to COLOR this frame
    };

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
