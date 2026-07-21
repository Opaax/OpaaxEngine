#include "VulkanFramebuffer.h"

#if OPAAX_HAS_VULKAN

#include "VulkanDevice.h"
#include "VulkanFrameContext.h"
#include "RHI/RenderCommand.h"
#include "Core/Log/OpaaxLog.h"

namespace Opaax
{
    // NOTE: the IFramebuffer::Create factory dispatch lives in RHI/BackendFactory.cpp.

    VulkanFramebuffer::VulkanFramebuffer(const FramebufferSpec& InSpec)
        : m_Allocator(VulkanFrameContext::Allocator())
        , m_Width(InSpec.Width)
        , m_Height(InSpec.Height)
    {
        VulkanDevice* lDevice = VulkanFrameContext::Device();
        OPAAX_CORE_ASSERT(lDevice)
        m_Device = lDevice->GetDevice();

        // Match the swapchain color format — the Renderer2D pipeline is baked against it (dynamic
        // rendering requires the attachment format to match the pipeline's color format).
        m_Format = VulkanFrameContext::ColorFormat();
        if (m_Format == VK_FORMAT_UNDEFINED)
        {
            OPAAX_CORE_ERROR("VulkanFramebuffer: swapchain color format unset — render API not initialized?");
            m_Format = VK_FORMAT_B8G8R8A8_UNORM;   // best-effort fallback
        }

        Invalidate();
    }

    VulkanFramebuffer::~VulkanFramebuffer()
    {
        Release();
        if (m_Sampler) { vkDestroySampler(m_Device, m_Sampler, nullptr); }
    }

    void VulkanFramebuffer::Release()
    {
        if (m_ImageView) { vkDestroyImageView(m_Device, m_ImageView, nullptr); m_ImageView = VK_NULL_HANDLE; }
        if (m_Image)     { vmaDestroyImage(m_Allocator, m_Image, m_Alloc);     m_Image = VK_NULL_HANDLE; m_Alloc = nullptr; }
    }

    void VulkanFramebuffer::Invalidate()
    {
        // ---- Color image: usable as a render target AND a sampled texture ----
        {
            VkImageCreateInfo lImgInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
            lImgInfo.imageType     = VK_IMAGE_TYPE_2D;
            lImgInfo.format        = m_Format;
            lImgInfo.extent        = { m_Width, m_Height, 1 };
            lImgInfo.mipLevels     = 1;
            lImgInfo.arrayLayers   = 1;
            lImgInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
            lImgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
            lImgInfo.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            lImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            VmaAllocationCreateInfo lAllocCI{};
            lAllocCI.usage = VMA_MEMORY_USAGE_AUTO;

            if (vmaCreateImage(m_Allocator, &lImgInfo, &lAllocCI, &m_Image, &m_Alloc, nullptr) != VK_SUCCESS)
            {
                OPAAX_CORE_ERROR("VulkanFramebuffer: vmaCreateImage failed ({}x{}).", m_Width, m_Height);
                return;
            }
        }

        // ---- View ----
        {
            VkImageViewCreateInfo lViewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            lViewInfo.image            = m_Image;
            lViewInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
            lViewInfo.format           = m_Format;
            lViewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            vkCreateImageView(m_Device, &lViewInfo, nullptr, &m_ImageView);
        }

        // ---- Sampler (once; persists across resizes). LINEAR/CLAMP — the viewport blit. ----
        if (!m_Sampler)
        {
            VkSamplerCreateInfo lSamplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
            lSamplerInfo.minFilter    = VK_FILTER_LINEAR;
            lSamplerInfo.magFilter    = VK_FILTER_LINEAR;
            lSamplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            lSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            lSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            lSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            lSamplerInfo.maxLod       = 1.0f;
            vkCreateSampler(m_Device, &lSamplerInfo, nullptr, &m_Sampler);
        }

        // Fresh image starts UNDEFINED; the first offscreen pass transitions it to COLOR.
        m_Layout = VK_IMAGE_LAYOUT_UNDEFINED;
        ++m_Generation;
    }

    void VulkanFramebuffer::Resize(Uint32 InWidth, Uint32 InHeight)
    {
        if (InWidth == 0 || InHeight == 0)              { return; }
        if (InWidth == m_Width && InHeight == m_Height) { m_PendingResize = false; return; }

        // Store only — the actual destroy/recreate happens at the next offscreen pass start
        // (ApplyPendingResize), after the prior frame's draws into the old image have drained.
        m_PendingWidth  = InWidth;
        m_PendingHeight = InHeight;
        m_PendingResize = true;
    }

    void VulkanFramebuffer::ApplyPendingResize()
    {
        if (!m_PendingResize) { return; }
        m_PendingResize = false;

        // Editor resize is interactive but infrequent — block until the GPU is done with the old
        // image (a prior frame in flight may still sample it / the UI descriptor set references it)
        // before destroying it. Simpler and correct vs. deferred-free bookkeeping.
        RenderCommand::WaitIdle();

        Release();
        m_Width  = m_PendingWidth;
        m_Height = m_PendingHeight;
        Invalidate();
    }

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
