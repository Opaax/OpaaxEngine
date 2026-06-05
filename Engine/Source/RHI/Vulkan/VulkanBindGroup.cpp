#include "VulkanBindGroup.h"

#if OPAAX_HAS_VULKAN

#include "VulkanFrameContext.h"
#include "VulkanDevice.h"
#include "VulkanDescriptorLayout.h"
#include "VulkanUniformBuffer.h"
#include "VulkanTexture2D.h"
#include "RHI/Vulkan/VulkanSwapchain.h"   // OPAAX_FRAMES_IN_FLIGHT
#include "Core/Log/OpaaxLog.h"

namespace Opaax
{
    // NOTE: the IBindGroup::Create factory dispatch lives in RHI/BackendFactory.cpp.

    VulkanBindGroup::VulkanBindGroup(const BindGroupLayout& InLayout)
        : m_TextureCount(InLayout.TextureSlotCount)
    {
        VulkanDevice* lDevice = VulkanFrameContext::Device();
        OPAAX_CORE_ASSERT(lDevice)
        m_Device = lDevice->GetDevice();

        m_Textures.assign(m_TextureCount, nullptr);

        m_SetLayout = BuildSpriteDescriptorSetLayout(m_Device);

        const Uint32 lSetCount = OPAAX_VULKAN_FRAME_RING * OPAAX_FRAMES_IN_FLIGHT;

        // ---- Pool: enough for every ring set across both frames-in-flight ----
        VkDescriptorPoolSize lSizes[2]{};
        lSizes[0].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        lSizes[0].descriptorCount = m_TextureCount * lSetCount;
        lSizes[1].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        lSizes[1].descriptorCount = lSetCount;

        VkDescriptorPoolCreateInfo lPoolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        lPoolInfo.maxSets       = lSetCount;
        lPoolInfo.poolSizeCount = 2;
        lPoolInfo.pPoolSizes    = lSizes;
        if (vkCreateDescriptorPool(m_Device, &lPoolInfo, nullptr, &m_Pool) != VK_SUCCESS)
        {
            OPAAX_CORE_ERROR("VulkanBindGroup: descriptor pool creation failed.");
            return;
        }

        // ---- Pre-allocate all ring sets (same layout repeated) ----
        TDynArray<VkDescriptorSetLayout> lLayouts(lSetCount, m_SetLayout);
        VkDescriptorSetAllocateInfo lAlloc{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        lAlloc.descriptorPool     = m_Pool;
        lAlloc.descriptorSetCount = lSetCount;
        lAlloc.pSetLayouts        = lLayouts.data();

        m_Sets.resize(lSetCount);
        if (vkAllocateDescriptorSets(m_Device, &lAlloc, m_Sets.data()) != VK_SUCCESS)
        {
            OPAAX_CORE_ERROR("VulkanBindGroup: descriptor set allocation failed.");
            m_Sets.clear();
        }
    }

    VulkanBindGroup::~VulkanBindGroup()
    {
        // Destroying the pool frees its sets.
        if (m_Pool)      { vkDestroyDescriptorPool(m_Device, m_Pool, nullptr); }
        if (m_SetLayout) { vkDestroyDescriptorSetLayout(m_Device, m_SetLayout, nullptr); }
    }

    void VulkanBindGroup::SetUniformBuffer(IUniformBuffer& InUniformBuffer)
    {
        m_UBO = static_cast<VulkanUniformBuffer*>(&InUniformBuffer);
    }

    void VulkanBindGroup::SetTexture(Uint32 InSlot, ITexture2D& InTexture)
    {
        if (InSlot < m_Textures.size())
        {
            m_Textures[InSlot] = static_cast<VulkanTexture2D*>(&InTexture);
        }
    }

    void VulkanBindGroup::BindInto(VkCommandBuffer InCmd, VkPipelineLayout InPipelineLayout)
    {
        if (m_Sets.empty() || !m_UBO) { return; }

        const Uint64 lGen = VulkanFrameContext::Generation();
        if (lGen != m_FrameGen)
        {
            m_FrameGen   = lGen;
            m_RingCursor = 0;
        }
        if (m_RingCursor >= OPAAX_VULKAN_FRAME_RING)
        {
            OPAAX_CORE_ERROR("VulkanBindGroup: exceeded {} flushes this frame — wrapping.",
                             OPAAX_VULKAN_FRAME_RING);
            m_RingCursor = 0;
        }

        const Uint32 lFrameSlot = VulkanFrameContext::FrameSlot();
        const Uint32 lSetIndex  = lFrameSlot * OPAAX_VULKAN_FRAME_RING + m_RingCursor;
        VkDescriptorSet lSet     = m_Sets[lSetIndex];

        // ---- Image infos (binding 0). Every slot references a live view (Renderer2D fills
        //      inactive slots with the white texture) — no dangling descriptor. ----
        TDynArray<VkDescriptorImageInfo> lImageInfos(m_TextureCount);
        for (Uint32 i = 0; i < m_TextureCount; ++i)
        {
            VulkanTexture2D* lTex = m_Textures[i];
            lImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            lImageInfos[i].imageView   = lTex ? lTex->GetImageView() : VK_NULL_HANDLE;
            lImageInfos[i].sampler     = lTex ? lTex->GetSampler()   : VK_NULL_HANDLE;
        }

        // ---- Buffer info (binding 1) — this pass's view-projection slot. ----
        VkDescriptorBufferInfo lBufInfo{};
        lBufInfo.buffer = m_UBO->GetBuffer(lFrameSlot);
        lBufInfo.offset = m_UBO->GetCurrentByteOffset();
        lBufInfo.range  = m_UBO->GetBlockSize();

        VkWriteDescriptorSet lWrites[2]{};
        lWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lWrites[0].dstSet          = lSet;
        lWrites[0].dstBinding      = 0;
        lWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        lWrites[0].descriptorCount = m_TextureCount;
        lWrites[0].pImageInfo      = lImageInfos.data();

        lWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lWrites[1].dstSet          = lSet;
        lWrites[1].dstBinding      = 1;
        lWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        lWrites[1].descriptorCount = 1;
        lWrites[1].pBufferInfo     = &lBufInfo;

        vkUpdateDescriptorSets(m_Device, 2, lWrites, 0, nullptr);

        vkCmdBindDescriptorSets(InCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, InPipelineLayout,
                                0, 1, &lSet, 0, nullptr);

        ++m_RingCursor;
    }

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
