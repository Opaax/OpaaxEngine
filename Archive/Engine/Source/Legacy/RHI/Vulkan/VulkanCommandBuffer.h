#pragma once

#include "RHI/ICommandBuffer.h"

#if OPAAX_HAS_VULKAN

#include <vulkan/vulkan.h>

namespace Opaax
{
    class VulkanDevice;
    class VulkanSwapchain;
    class VulkanFramebuffer;

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
     * BindPipeline / BindBindGroup / BindVertexArray / DrawIndexed record into the live
     * VkCommandBuffer (Phase 3). BindPipeline caches the bound pipeline's layout so the bind group
     * has it for vkCmdBindDescriptorSets.
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
            m_Cmd                   = InCmd;
            m_ColorAcquired         = false;
            m_CurrentPipelineLayout = VK_NULL_HANDLE;
        }

        // Transition the image to PRESENT_SRC if any pass ran. Called before vkEndCommandBuffer.
        void FinishFrame();

        // The live VkCommandBuffer of the current frame (VK_NULL_HANDLE on a skipped frame).
        // The Vulkan editor UI backend records ImGui draws into it during RenderDrawData.
        VkCommandBuffer GetVkCommandBuffer() const noexcept { return m_Cmd; }

        // =============================================================================
        // Overrides
        // =============================================================================

        //~Begin ICommandBuffer interface
    public:
        void BeginRenderPass(IRenderTarget& InTarget, ELoadOp InLoadOp, const Vector4F& InClearColor) override;
        void EndRenderPass() override;

        void SetViewport(Uint32 X, Uint32 Y, Uint32 Width, Uint32 Height) override;

        void BindPipeline(IPipeline& InPipeline)          override;
        void BindBindGroup(IBindGroup& InBindGroup)       override;
        void BindVertexArray(IVertexArray& InVertexArray) override;
        void DrawIndexed(Uint32 InIndexCount)             override;
        //~End ICommandBuffer interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        VulkanDevice*    m_Device    = nullptr;
        VulkanSwapchain* m_Swapchain = nullptr;
        VkCommandBuffer  m_Cmd       = VK_NULL_HANDLE;
        bool             m_ColorAcquired = false;   // SWAPCHAIN image transitioned to COLOR this frame

        // The offscreen framebuffer of the currently-open pass (editor viewport), or null when the
        // open pass targets the swapchain. EndRenderPass uses it to hand the image to the sampler.
        VulkanFramebuffer* m_CurrentFramebuffer = nullptr;

        VkPipelineLayout m_CurrentPipelineLayout = VK_NULL_HANDLE;   // last bound, for descriptor binding
    };

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
