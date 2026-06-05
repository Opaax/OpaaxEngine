#include "VulkanCommandBuffer.h"

#if OPAAX_HAS_VULKAN

#include "VulkanSwapchain.h"
#include "VulkanFrameContext.h"
#include "VulkanFramebuffer.h"
#include "VulkanPipeline.h"
#include "VulkanBindGroup.h"
#include "VulkanBuffer.h"
#include "RHI/Buffer.h"   // IVertexArray
#include "Renderer/RenderTarget.hpp"

namespace Opaax
{
    namespace
    {
        // Record a color-aspect image layout transition (single mip/layer swapchain image).
        void TransitionImage(VkCommandBuffer InCmd, VkImage InImage,
                             VkImageLayout InOld, VkImageLayout InNew,
                             VkPipelineStageFlags InSrcStage, VkPipelineStageFlags InDstStage,
                             VkAccessFlags InSrcAccess, VkAccessFlags InDstAccess)
        {
            VkImageMemoryBarrier lBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            lBarrier.oldLayout           = InOld;
            lBarrier.newLayout           = InNew;
            lBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            lBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            lBarrier.image               = InImage;
            lBarrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            lBarrier.srcAccessMask       = InSrcAccess;
            lBarrier.dstAccessMask       = InDstAccess;

            vkCmdPipelineBarrier(InCmd, InSrcStage, InDstStage, 0, 0, nullptr, 0, nullptr, 1, &lBarrier);
        }
    }

    void VulkanCommandBuffer::BeginRenderPass(IRenderTarget& InTarget, ELoadOp InLoadOp,
                                              const Vector4F& InClearColor)
    {
        if (m_Cmd == VK_NULL_HANDLE) { return; }   // frame skipped — record nothing

        // Offscreen target (editor ViewportPanel) — render into its image, then EndRenderPass hands
        // it to the sampler. Swapchain target (null framebuffer) falls through to the present path.
        if (IFramebuffer* lFBBase = InTarget.GetFramebuffer())
        {
            auto& lFB = static_cast<VulkanFramebuffer&>(*lFBBase);
            m_CurrentFramebuffer = &lFB;

            // Frame-safe point to apply a pending resize: the prior frame's draws into the old image
            // have drained and nothing has recorded into the (possibly new) image yet this frame.
            lFB.ApplyPendingResize();

            // Transition to COLOR. A pipeline barrier's first scope covers earlier submissions on
            // this queue, so a fresh image (UNDEFINED) needs no wait, while a reused one waits the
            // prior frame's ImGui sampling (FRAGMENT_SHADER / SHADER_READ) — correct cross-frame.
            const VkImageLayout        lOld       = lFB.GetCurrentLayout();
            const bool                 lFresh     = (lOld == VK_IMAGE_LAYOUT_UNDEFINED);
            const VkPipelineStageFlags lSrcStage  = lFresh ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                                                           : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            const VkAccessFlags        lSrcAccess = lFresh ? 0 : VK_ACCESS_SHADER_READ_BIT;

            TransitionImage(m_Cmd, lFB.GetImage(),
                            lOld, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            lSrcStage, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                            lSrcAccess, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
            lFB.SetCurrentLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            const VkExtent2D lFbExtent = lFB.GetExtent();

            VkRenderingAttachmentInfo lFbColor{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
            lFbColor.imageView   = lFB.GetColorImageView();
            lFbColor.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            lFbColor.loadOp      = (InLoadOp == ELoadOp::Clear) ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                                               : VK_ATTACHMENT_LOAD_OP_LOAD;
            lFbColor.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
            lFbColor.clearValue.color = { { InClearColor.x, InClearColor.y, InClearColor.z, InClearColor.w } };

            VkRenderingInfo lFbInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
            lFbInfo.renderArea           = { { 0, 0 }, lFbExtent };
            lFbInfo.layerCount           = 1;
            lFbInfo.colorAttachmentCount = 1;
            lFbInfo.pColorAttachments    = &lFbColor;

            vkCmdBeginRendering(m_Cmd, &lFbInfo);
            SetViewport(0, 0, lFbExtent.width, lFbExtent.height);
            return;
        }

        m_CurrentFramebuffer = nullptr;

        const VkImage     lImage = m_Swapchain->GetCurrentImage();
        const VkImageView lView  = m_Swapchain->GetCurrentImageView();
        const VkExtent2D  lExtent = m_Swapchain->GetExtent();

        // First pass of the frame: bring the image from its (undefined) state into COLOR. Later
        // passes on the same image (overlay, ELoadOp::Load) keep the contents — no re-transition.
        if (!m_ColorAcquired)
        {
            TransitionImage(m_Cmd, lImage,
                            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                            0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
            m_ColorAcquired = true;
        }

        VkRenderingAttachmentInfo lColor{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        lColor.imageView   = lView;
        lColor.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        lColor.loadOp      = (InLoadOp == ELoadOp::Clear) ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                                          : VK_ATTACHMENT_LOAD_OP_LOAD;
        lColor.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
        lColor.clearValue.color = { { InClearColor.x, InClearColor.y, InClearColor.z, InClearColor.w } };

        VkRenderingInfo lInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
        lInfo.renderArea           = { { 0, 0 }, lExtent };
        lInfo.layerCount           = 1;
        lInfo.colorAttachmentCount = 1;
        lInfo.pColorAttachments    = &lColor;

        vkCmdBeginRendering(m_Cmd, &lInfo);

        SetViewport(0, 0, lExtent.width, lExtent.height);
    }

    void VulkanCommandBuffer::EndRenderPass()
    {
        if (m_Cmd == VK_NULL_HANDLE) { return; }
        vkCmdEndRendering(m_Cmd);

        if (m_CurrentFramebuffer)
        {
            // Offscreen: hand the image to the fragment shader so ImGui can sample it this frame
            // (and so the next frame's begin-of-pass barrier has a defined source layout).
            TransitionImage(m_Cmd, m_CurrentFramebuffer->GetImage(),
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
            m_CurrentFramebuffer->SetCurrentLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            m_CurrentFramebuffer = nullptr;
        }
    }

    void VulkanCommandBuffer::SetViewport(Uint32 X, Uint32 Y, Uint32 Width, Uint32 Height)
    {
        if (m_Cmd == VK_NULL_HANDLE) { return; }

        // Negative-height viewport flips clip-space Y so the OpenGL-style (Y-up) projection renders
        // upright on Vulkan (Y-down NDC) — applied to ALL passes so winding stays consistent (cull
        // is NONE, but a positive-height offscreen pass was observed to drop the sprite entirely).
        // For the editor's offscreen image the resulting orientation is corrected by the ImGui
        // sampling UVs in ViewportPanel (Vulkan samples top-down, see ViewportPanel::Draw).
        VkViewport lViewport{};
        lViewport.x        = static_cast<float>(X);
        lViewport.y        = static_cast<float>(Y + Height);
        lViewport.width    = static_cast<float>(Width);
        lViewport.height   = -static_cast<float>(Height);
        lViewport.minDepth = 0.0f;
        lViewport.maxDepth = 1.0f;
        vkCmdSetViewport(m_Cmd, 0, 1, &lViewport);

        VkRect2D lScissor{ { static_cast<int32_t>(X), static_cast<int32_t>(Y) }, { Width, Height } };
        vkCmdSetScissor(m_Cmd, 0, 1, &lScissor);
    }

    void VulkanCommandBuffer::BindPipeline(IPipeline& InPipeline)
    {
        if (m_Cmd == VK_NULL_HANDLE) { return; }

        // Backend invariant: the active backend's factory only produces VulkanPipeline here.
        auto& lPipeline = static_cast<VulkanPipeline&>(InPipeline);
        m_CurrentPipelineLayout = lPipeline.GetPipelineLayout();
        vkCmdBindPipeline(m_Cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, lPipeline.GetPipeline());
    }

    void VulkanCommandBuffer::BindBindGroup(IBindGroup& InBindGroup)
    {
        if (m_Cmd == VK_NULL_HANDLE) { return; }
        static_cast<VulkanBindGroup&>(InBindGroup).BindInto(m_Cmd, m_CurrentPipelineLayout);
    }

    void VulkanCommandBuffer::BindVertexArray(IVertexArray& InVertexArray)
    {
        if (m_Cmd == VK_NULL_HANDLE) { return; }

        const Uint32 lFrameSlot = VulkanFrameContext::FrameSlot();

        // One interleaved vertex buffer (the batch VBO) at the offset its SetData just wrote.
        const auto& lVertexBuffers = InVertexArray.GetVertexBuffers();
        if (!lVertexBuffers.empty())
        {
            auto* lVB = static_cast<VulkanVertexBuffer*>(lVertexBuffers[0].get());
            VkBuffer     lBuffer = lVB->GetBuffer(lFrameSlot);
            VkDeviceSize lOffset = lVB->GetLastBindOffset();
            vkCmdBindVertexBuffers(m_Cmd, 0, 1, &lBuffer, &lOffset);
        }

        if (const IIndexBuffer* lIBO = InVertexArray.GetIndexBuffer())
        {
            auto* lIB = static_cast<const VulkanIndexBuffer*>(lIBO);
            vkCmdBindIndexBuffer(m_Cmd, lIB->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
        }
    }

    void VulkanCommandBuffer::DrawIndexed(Uint32 InIndexCount)
    {
        if (m_Cmd == VK_NULL_HANDLE) { return; }
        vkCmdDrawIndexed(m_Cmd, InIndexCount, 1, 0, 0, 0);
    }

    void VulkanCommandBuffer::FinishFrame()
    {
        if (m_Cmd == VK_NULL_HANDLE) { return; }
        if (!m_ColorAcquired) { return; }   // no pass ran this frame — nothing to present

        TransitionImage(m_Cmd, m_Swapchain->GetCurrentImage(),
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0);
    }

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
