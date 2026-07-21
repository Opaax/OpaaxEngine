#include "VulkanTexture2D.h"

#if OPAAX_HAS_VULKAN

#include "VulkanFrameContext.h"
#include "VulkanDevice.h"
#include "Core/Log/OpaaxLog.h"

// stb_image — STB_IMAGE_IMPLEMENTATION is defined once in OpenGLTexture2D.cpp (always compiled);
// here we only call into it.
#include <stb/stb_image.h>

#include <cstring>

namespace Opaax
{
    // NOTE: the ITexture2D::Create factory dispatch lives in RHI/BackendFactory.cpp.

    VulkanTexture2D::VulkanTexture2D(const char* InPath)
        : m_Allocator(VulkanFrameContext::Allocator())
    {
        // Match the GL loader: flip vertically (the UV layout assumes bottom-left origin).
        stbi_set_flip_vertically_on_load(1);

        Int32 lWidth = 0, lHeight = 0, lChannels = 0;
        unsigned char* lData = stbi_load(InPath, &lWidth, &lHeight, &lChannels, 0);
        if (!lData)
        {
            OPAAX_CORE_ERROR("VulkanTexture2D: failed to load '{}' — {}", InPath, stbi_failure_reason());
            return;
        }

        Upload(lData, static_cast<Uint32>(lWidth), static_cast<Uint32>(lHeight), lChannels);
        stbi_image_free(lData);
    }

    VulkanTexture2D::VulkanTexture2D(Uint32 InWidth, Uint32 InHeight)
        : m_Allocator(VulkanFrameContext::Allocator())
    {
        // 1x1 (or NxN) solid white — tinted by the shader. Fill RGBA white.
        const Uint32 lCount = InWidth * InHeight;
        TDynArray<Uint32> lWhite(lCount, 0xFFFFFFFFu);
        Upload(reinterpret_cast<const unsigned char*>(lWhite.data()), InWidth, InHeight, 4);
    }

    VulkanTexture2D::VulkanTexture2D(const unsigned char* InData, Uint32 InWidth, Uint32 InHeight, Int32 InChannels)
        : m_Allocator(VulkanFrameContext::Allocator())
    {
        if (!InData)
        {
            OPAAX_CORE_ERROR("VulkanTexture2D: raw upload received null data");
            return;
        }
        Upload(InData, InWidth, InHeight, InChannels);
    }

    VulkanTexture2D::~VulkanTexture2D()
    {
        if (m_Sampler)   { vkDestroySampler(m_Device, m_Sampler, nullptr); }
        if (m_ImageView) { vkDestroyImageView(m_Device, m_ImageView, nullptr); }
        if (m_Image)     { vmaDestroyImage(m_Allocator, m_Image, m_Alloc); }
    }

    namespace
    {
        void TransitionForCopy(VkCommandBuffer InCmd, VkImage InImage,
                               VkImageLayout InOld, VkImageLayout InNew,
                               VkPipelineStageFlags InSrc, VkPipelineStageFlags InDst,
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
            vkCmdPipelineBarrier(InCmd, InSrc, InDst, 0, 0, nullptr, 0, nullptr, 1, &lBarrier);
        }
    }

    void VulkanTexture2D::Upload(const unsigned char* InData, Uint32 InWidth, Uint32 InHeight, Int32 InChannels)
    {
        m_Width  = InWidth;
        m_Height = InHeight;

        VulkanDevice* lDevice = VulkanFrameContext::Device();
        OPAAX_CORE_ASSERT(lDevice)
        m_Device = lDevice->GetDevice();

        const bool lIsR8 = (InChannels == 1);

        // Resolve format + a tightly-packed pixel buffer. Vulkan rarely samples RGB8, so 3ch is
        // expanded to RGBA; R8 stays single-channel and is alpha-swizzled in the view.
        const VkFormat lFormat        = lIsR8 ? VK_FORMAT_R8_UNORM : VK_FORMAT_R8G8B8A8_UNORM;
        const Uint32   lDstChannels   = lIsR8 ? 1u : 4u;
        const VkDeviceSize lImageSize = static_cast<VkDeviceSize>(InWidth) * InHeight * lDstChannels;

        TDynArray<Uint8> lExpanded;   // only used for the 3ch -> 4ch path
        const unsigned char* lSrc = InData;
        if (InChannels == 3)
        {
            lExpanded.resize(static_cast<size_t>(InWidth) * InHeight * 4u);
            const Uint32 lPixels = InWidth * InHeight;
            for (Uint32 p = 0; p < lPixels; ++p)
            {
                lExpanded[p * 4 + 0] = InData[p * 3 + 0];
                lExpanded[p * 4 + 1] = InData[p * 3 + 1];
                lExpanded[p * 4 + 2] = InData[p * 3 + 2];
                lExpanded[p * 4 + 3] = 255;
            }
            lSrc = lExpanded.data();
        }

        // ---- Staging buffer (host-visible) ----
        VkBuffer      lStaging      = VK_NULL_HANDLE;
        VmaAllocation lStagingAlloc = nullptr;
        {
            VkBufferCreateInfo lBufInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
            lBufInfo.size  = lImageSize;
            lBufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

            VmaAllocationCreateInfo lAllocCI{};
            lAllocCI.usage = VMA_MEMORY_USAGE_AUTO;
            lAllocCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                           | VMA_ALLOCATION_CREATE_MAPPED_BIT;

            VmaAllocationInfo lOut{};
            if (vmaCreateBuffer(m_Allocator, &lBufInfo, &lAllocCI, &lStaging, &lStagingAlloc, &lOut) != VK_SUCCESS)
            {
                OPAAX_CORE_ERROR("VulkanTexture2D: staging buffer creation failed.");
                return;
            }
            std::memcpy(lOut.pMappedData, lSrc, lImageSize);
        }

        // ---- Device-local image ----
        {
            VkImageCreateInfo lImgInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
            lImgInfo.imageType     = VK_IMAGE_TYPE_2D;
            lImgInfo.format        = lFormat;
            lImgInfo.extent        = { InWidth, InHeight, 1 };
            lImgInfo.mipLevels     = 1;
            lImgInfo.arrayLayers   = 1;
            lImgInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
            lImgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
            lImgInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            lImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            VmaAllocationCreateInfo lAllocCI{};
            lAllocCI.usage = VMA_MEMORY_USAGE_AUTO;

            if (vmaCreateImage(m_Allocator, &lImgInfo, &lAllocCI, &m_Image, &m_Alloc, nullptr) != VK_SUCCESS)
            {
                OPAAX_CORE_ERROR("VulkanTexture2D: vmaCreateImage failed.");
                vmaDestroyBuffer(m_Allocator, lStaging, lStagingAlloc);
                return;
            }
        }

        // ---- Copy staging -> image (synchronous, outside the frame loop) ----
        lDevice->ImmediateSubmit([&](VkCommandBuffer InCmd)
        {
            TransitionForCopy(InCmd, m_Image,
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              0, VK_ACCESS_TRANSFER_WRITE_BIT);

            VkBufferImageCopy lCopy{};
            lCopy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            lCopy.imageExtent      = { InWidth, InHeight, 1 };
            vkCmdCopyBufferToImage(InCmd, lStaging, m_Image,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &lCopy);

            TransitionForCopy(InCmd, m_Image,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                              VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        });

        vmaDestroyBuffer(m_Allocator, lStaging, lStagingAlloc);

        // ---- View (R8 coverage swizzled into alpha; RGBA identity) ----
        {
            VkImageViewCreateInfo lViewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            lViewInfo.image            = m_Image;
            lViewInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
            lViewInfo.format           = lFormat;
            lViewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            if (lIsR8)
            {
                lViewInfo.components = { VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE,
                                        VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_R };
            }
            vkCreateImageView(m_Device, &lViewInfo, nullptr, &m_ImageView);
        }

        // ---- Sampler (mirror GL: LINEAR-min/NEAREST-mag/REPEAT; R8 -> LINEAR-mag/CLAMP) ----
        {
            VkSamplerCreateInfo lSamplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
            lSamplerInfo.minFilter    = VK_FILTER_LINEAR;
            lSamplerInfo.magFilter    = lIsR8 ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
            lSamplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            const VkSamplerAddressMode lAddr = lIsR8 ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
                                                     : VK_SAMPLER_ADDRESS_MODE_REPEAT;
            lSamplerInfo.addressModeU = lAddr;
            lSamplerInfo.addressModeV = lAddr;
            lSamplerInfo.addressModeW = lAddr;
            lSamplerInfo.maxLod       = 1.0f;
            vkCreateSampler(m_Device, &lSamplerInfo, nullptr, &m_Sampler);
        }

        m_Loaded = (m_ImageView != VK_NULL_HANDLE) && (m_Sampler != VK_NULL_HANDLE);
    }

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
